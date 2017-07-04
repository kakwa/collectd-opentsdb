/**
 * collectd - src/write_opentsdb.c
 * Copyright (C) 2012       Pierre-Yves Ritschard
 * Copyright (C) 2011       Scott Sanders
 * Copyright (C) 2009       Paul Sadauskas
 * Copyright (C) 2009       Doug MacEachern
 * Copyright (C) 2007-2012  Florian octo Forster
 * Copyright (C) 2013-2014  Limelight Networks, Inc.
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; only version 2 of the License is applicable.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA
 *
 * Based on the write_graphite plugin. Authors:
 *   Florian octo Forster <octo at collectd.org>
 *   Doug MacEachern <dougm at hyperic.com>
 *   Paul Sadauskas <psadauskas at gmail.com>
 *   Scott Sanders <scott at jssjr.com>
 *   Pierre-Yves Ritschard <pyr at spootnik.org>
 * Based on the write_tsdb plugin. Authors:
 *   Brett Hawn <bhawn at llnw.com>
 *   Kevin Bowling <kbowling@llnw.com>
 *   Yves Mettier <ymettier@free.fr>
 * Based on the write_http plugin. Authors:
 *   Florian octo Forster <octo at collectd.org>
 *   Doug MacEachern <dougm@hyperic.com>
 *   Paul Sadauskas <psadauskas@gmail.com>
 * write_opentsdb plugin Authors:
 *   Pierre-Francois Carpentier <carpentier.pf@gmail.com>
 **/

/* write_opentsdb plugin configuation example
 * --------------------------------------
 *
 * <Plugin write_opentsdb>
 *   <Node>
 *     Host "http://localhost:4242"
 *     Port "4242"
 *   </Node>
 * </Plugin>
 *
 * write_opentsdb meta_data
 * --------------------
 *  - tsdb_prefix : Will prefix the OpenTSDB <metric> (also prefix tsdb_id if
 * defined)
 *  - tsdb_id     : Replace the metric with this tag
 *
 *  - tsdb_tag_plugin         : When defined, tsdb_tag_* removes the related
 *  - tsdb_tag_pluginInstance : item from metric id.
 *  - tsdb_tag_type           : If it is not empty, it will be the key of an
 *  - tsdb_tag_typeInstance   : opentsdb tag (the value is the item itself)
 *  - tsdb_tag_dsname         : If it is empty, no tag is defined.
 *
 *  - tsdb_tag_add_*          : Should contain "tagv". Il will add a tag.
 *                            : The key tagk comes from the tsdb_tag_add_*
 *                            : tag. Example : tsdb_tag_add_status adds a tag
 *                            : named 'status'.
 *                            : It will be sent as is to the TSDB server.
 *
 * write_opentsdb plugin filter rules example
 * --------------------------------------
 *
 * <Chain "PreCache">
 *   <Rule "opentsdb_cpu">
 *     <Match "regex">
 *       Plugin "^cpu$"
 *     </Match>
 *     <Target "set">
 *       MetaDataSet "tsdb_tag_pluginInstance" "cpu"
 *       MetaDataSet "tsdb_tag_type" ""
 *       MetaDataSet "tsdb_prefix" "sys."
 *     </Target>
 *   </Rule>
 *   <Rule "opentsdb_df">
 *     <Match "regex">
 *       Plugin "^df$"
 *     </Match>
 *     <Target "set">
 *       MetaDataSet "tsdb_tag_pluginInstance" "mount"
 *       MetaDataSet "tsdb_tag_type" ""
 *       MetaDataSet "tsdb_prefix" "sys."
 *     </Target>
 *   </Rule>
 *   <Rule "opentsdb_disk">
 *     <Match "regex">
 *       Plugin "^disk$"
 *     </Match>
 *     <Target "set">
 *       MetaDataSet "tsdb_tag_pluginInstance" "disk"
 *       MetaDataSet "tsdb_prefix" "sys."
 *     </Target>
 *   </Rule>
 *   <Rule "opentsdb_interface">
 *     <Match "regex">
 *       Plugin "^interface$"
 *     </Match>
 *     <Target "set">
 *       MetaDataSet "tsdb_tag_pluginInstance" "iface"
 *       MetaDataSet "tsdb_prefix" "sys."
 *     </Target>
 *   </Rule>
 *   <Rule "opentsdb_load">
 *     <Match "regex">
 *       Plugin "^loac$"
 *     </Match>
 *     <Target "set">
 *       MetaDataSet "tsdb_tag_type" ""
 *       MetaDataSet "tsdb_prefix" "sys."
 *     </Target>
 *   </Rule>
 *   <Rule "opentsdb_swap">
 *     <Match "regex">
 *       Plugin "^swap$"
 *     </Match>
 *     <Target "set">
 *       MetaDataSet "tsdb_prefix" "sys."
 *     </Target>
 *   </Rule>
 * </Chain>
 *
 * IMPORTANT WARNING
 * -----------------
 * OpenTSDB allows no more than 8 tags.
 * Collectd admins should be aware of this when defining filter rules and host
 * tags.
 *
 */


/* Plugin entry points:
 * - write ->  wt_write
 * - flush -> wt_flush
 * - complex_config -> wt_config;
 */

#include <fcntl.h>
#include <unistd.h>
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <inttypes.h>
#include <curl/curl.h>
#include <json-c/json.h>
#include <netdb.h>
#include <pwd.h>

#define COLLECTD_USERAGENT "collectd"
#define HAVE__BOOL 1
#define FP_LAYOUT_NEED_NOTHING 1

// Collectd headers
#include <collectd.h>
#include <common.h>
#include <plugin.h>
#include <utils_cache.h>

#ifndef GAUGE_FORMAT
#define GAUGE_FORMAT "%.15g"
#endif

#ifndef WT_DEFAULT_NODE
#define WT_DEFAULT_NODE "http://localhost:4242"
#endif

#ifndef WT_DEFAULT_ESCAPE
#define WT_DEFAULT_ESCAPE '.'
#endif

/* Ethernet - (IPv6 + TCP) = 1500 - (40 + 32) = 1428 */
#ifndef WT_SEND_BUF_SIZE
#define WT_SEND_BUF_SIZE 1428
#endif

/* Meta data definitions about tsdb tags */
#define TSDB_TAG_PLUGIN 0
#define TSDB_TAG_PLUGININSTANCE 1
#define TSDB_TAG_TYPE 2
#define TSDB_TAG_TYPEINSTANCE 3
#define TSDB_TAG_DSNAME 4
static const char *meta_tag_metric_id[] = {
    "tsdb_tag_plugin", "tsdb_tag_pluginInstance", "tsdb_tag_type",
    "tsdb_tag_typeInstance", "tsdb_tag_dsname"};

/*
 * Private variables
 */
struct wt_callback {

  char *node;

  // Curl Parameters
  CURL *curl;
  struct curl_slist *headers;
  char curl_errbuf[CURL_ERROR_SIZE];
  int timeout;
  char *cacert;
  char *capath;
  char *clientkey;
  char *clientcert;
  char *clientkeypass;
  long sslversion;
  _Bool verify_peer;
  _Bool verify_host;
  _Bool log_http_error;

  _Bool store_rates;
  _Bool always_append_ds;

  // set to true if host contains a json structure with tags
  _Bool json_host_tag;
  // set to true to set tag fqdn to host if host is not a parsable json structure
  // only useful if json_host_tag is set to true
  _Bool auto_fqdn_failback;
  // Maximum number of metrics in buffer
  int buffer_metric_max;
  // the Json buffer
  json_object *json_buffer;
  // number of metrics in buffer
  int buffer_metric_size;

  // mutex used for emptying/happending in the buffer
  pthread_mutex_t send_lock;

  int connect_failed_log_count;
  time_t last_error_log;
};

static void wt_callback_free(void *data);
int wt_config_curl(struct wt_callback *cb);
static int wt_write_nolock(struct wt_callback *cb);

// Discard return from libcurl
size_t writefunc(void *ptr, size_t size, size_t nmemb, void *s)
{
  return size*nmemb;
}

/* Reset the metric buffer
 */
static void wt_reset_buffer(struct wt_callback *cb) {
  json_object_put(cb->json_buffer);
  cb->json_buffer = json_object_new_array();
  cb->buffer_metric_size = 0;
}

static int wt_flush(cdtime_t timeout,
                    const char *identifier __attribute__((unused)),
                    user_data_t *user_data) {
  struct wt_callback *cb;
  int status;

  cb = user_data->data;

  pthread_mutex_lock(&cb->send_lock);
  status = wt_write_nolock(cb);
  pthread_mutex_unlock(&cb->send_lock);

  return status;
}

int wh_log_http_error(struct wt_callback *cb, int status) {
  int ret = 0;
  long http_code = 0;

  curl_easy_getinfo(cb->curl, CURLINFO_RESPONSE_CODE, &http_code);

  if ((http_code != 204 && http_code != 0) || status != CURLE_OK){
    time_t ct = time(NULL);
    ret = 1;
    cb->connect_failed_log_count++;
    if(ct - cb->last_error_log > 30){
        if(http_code != 204 && http_code != 0)
          ERROR("write_opentsdb plugin: HTTP Error code: %lu", http_code);
        if(status != CURLE_OK){
          ERROR("write_opentsdb plugin: curl_easy_perform failed with "
                "status %i: %s",
                status, cb->curl_errbuf);
        }
        ERROR("write_opentsdb plugin: %d OpenTSDB http POST errors since last log",
              cb->connect_failed_log_count);
        cb->connect_failed_log_count = 0;
        cb->last_error_log = ct;
    }
  }
  return ret;
}

/* OpenTSDB writer
 * Must be called wrapped around locks (use cb->send_lock for that)
 */
static int wt_write_nolock(struct wt_callback *cb){
  const char *data = json_object_to_json_string(cb->json_buffer);

  //for primitive debugging
  //printf("%s\n", data);

  int status = 0;
  curl_easy_setopt(cb->curl, CURLOPT_POSTFIELDS, data);
  status = curl_easy_perform(cb->curl);

  status = wh_log_http_error(cb, status);

  wt_reset_buffer(cb);
  return 0;
  //return status;
}

static int wt_format_values(char *ret, size_t ret_len, int ds_num,
                            const data_set_t *ds, const value_list_t *vl,
                            _Bool store_rates) {
  size_t offset = 0;
  int status;
  gauge_t *rates = NULL;

  assert(0 == strcmp(ds->type, vl->type));

  memset(ret, 0, ret_len);

#define BUFFER_ADD(...)                                                        \
  do {                                                                         \
    status = ssnprintf(ret + offset, ret_len - offset, __VA_ARGS__);           \
    if (status < 1) {                                                          \
      sfree(rates);                                                            \
      return -1;                                                               \
    } else if (((size_t)status) >= (ret_len - offset)) {                       \
      sfree(rates);                                                            \
      return -1;                                                               \
    } else                                                                     \
      offset += ((size_t)status);                                              \
  } while (0)

  if (ds->ds[ds_num].type == DS_TYPE_GAUGE)
    BUFFER_ADD(GAUGE_FORMAT, vl->values[ds_num].gauge);
  else if (store_rates) {
    if (rates == NULL)
      rates = uc_get_rate(ds, vl);
    if (rates == NULL) {
      WARNING("format_values: "
              "uc_get_rate failed.");
      return -1;
    }
    BUFFER_ADD(GAUGE_FORMAT, rates[ds_num]);
  } else if (ds->ds[ds_num].type == DS_TYPE_COUNTER)
    BUFFER_ADD("%llu", vl->values[ds_num].counter);
  else if (ds->ds[ds_num].type == DS_TYPE_DERIVE)
    BUFFER_ADD("%" PRIi64, vl->values[ds_num].derive);
  else if (ds->ds[ds_num].type == DS_TYPE_ABSOLUTE)
    BUFFER_ADD("%" PRIu64, vl->values[ds_num].absolute);
  else {
    ERROR("format_values plugin: Unknown data source type: %i",
          ds->ds[ds_num].type);
    sfree(rates);
    return -1;
  }

#undef BUFFER_ADD

  sfree(rates);
  return 0;
}

static int wt_add_tag(json_object *tags_array, const char *key, const char *value){
  json_object *tag_value = json_object_new_string(value);
  json_object_object_add(tags_array, key, tag_value);
  return 0;
}

static int wt_format_tags(json_object *dp, const value_list_t *vl,
                          const struct wt_callback *cb, const char *ds_name) {
  int status;
  char *temp = NULL;
  char **meta_toc;
  const char *host = vl->host;
  int i, n;
  json_object *tags_array = NULL;

  if(cb->json_host_tag){
    tags_array = json_tokener_parse(host);
    if((tags_array == NULL) && cb->auto_fqdn_failback){
      DEBUG("Failed to parse json host '%s', fallback to simple fqdn tag", host);
      tags_array = json_object_new_object();
      wt_add_tag(tags_array, "fqdn", host);
    } else if((tags_array == NULL)) {
      ERROR("Failed to parse json host '%s'", host);
      return 1;
    }
  } else {
    tags_array = json_object_new_object();
    wt_add_tag(tags_array, "fqdn", host);
  }
#define TSDB_META_TAG_ADD_PREFIX "tsdb_tag_add_"

#define TSDB_META_DATA_GET_STRING(tag)                                         \
  do {                                                                         \
    temp = NULL;                                                               \
    status = meta_data_get_string(vl->meta, tag, &temp);                       \
    if (status == -ENOENT) {                                                   \
      temp = NULL;                                                             \
      /* defaults to empty string */                                           \
    } else if (status < 0) {                                                   \
      sfree(temp);                                                             \
      return status;                                                           \
    }                                                                          \
  } while (0)


  if (vl->meta) {
    TSDB_META_DATA_GET_STRING(meta_tag_metric_id[TSDB_TAG_PLUGIN]);
    if (temp && strlen(temp) != 0) {
      wt_add_tag(tags_array, temp, vl->plugin);
      sfree(temp);
    }

    TSDB_META_DATA_GET_STRING(meta_tag_metric_id[TSDB_TAG_PLUGININSTANCE]);
    if (temp && strlen(temp) != 0) {
      wt_add_tag(tags_array, temp, vl->plugin_instance);
      sfree(temp);
    }

    TSDB_META_DATA_GET_STRING(meta_tag_metric_id[TSDB_TAG_TYPE]);
    if (temp && strlen(temp) != 0) {
      wt_add_tag(tags_array, temp, vl->type);
      sfree(temp);
    }

    TSDB_META_DATA_GET_STRING(meta_tag_metric_id[TSDB_TAG_TYPEINSTANCE]);
    if (temp && strlen(temp) != 0) {
      wt_add_tag(tags_array, temp, vl->type_instance);
      sfree(temp);
    }

    if (ds_name) {
      TSDB_META_DATA_GET_STRING(meta_tag_metric_id[TSDB_TAG_DSNAME]);
      if (temp && strlen(temp) != 0) {
        wt_add_tag(tags_array, temp, ds_name);
        sfree(temp);
      }
    }

    n = meta_data_toc(vl->meta, &meta_toc);
    for (i = 0; i < n; i++) {
      if (strncmp(meta_toc[i], TSDB_META_TAG_ADD_PREFIX,
                  sizeof(TSDB_META_TAG_ADD_PREFIX) - 1)) {
        free(meta_toc[i]);
        continue;
      }
      if ('\0' == meta_toc[i][sizeof(TSDB_META_TAG_ADD_PREFIX) - 1]) {
        ERROR("write_opentsdb plugin: meta_data tag '%s' is unknown (host=%s, "
              "plugin=%s, type=%s)",
              temp, vl->host, vl->plugin, vl->type);
        free(meta_toc[i]);
        continue;
      }

      TSDB_META_DATA_GET_STRING(meta_toc[i]);
      if (temp && temp[0]) {
        int n;
        char *key = meta_toc[i] + sizeof(TSDB_META_TAG_ADD_PREFIX) - 1;

        wt_add_tag(tags_array, key, temp);
      }
      if (temp)
        sfree(temp);
      free(meta_toc[i]);
    }
    if (meta_toc)
      free(meta_toc);

  }

#undef TSDB_META_DATA_GET_STRING
  json_object_object_add(dp, "tags", tags_array);

  return 0;
}

static int wt_format_name(char *ret, int ret_len, const value_list_t *vl,
                          const struct wt_callback *cb, const char *ds_name) {
  int status;
  int i;
  char *temp = NULL;
  char *prefix = NULL;
  const char *meta_prefix = "tsdb_prefix";
  char *tsdb_id = NULL;
  const char *meta_id = "tsdb_id";

  _Bool include_in_id[] = {
      /* plugin =          */ 1,
      /* plugin instance = */ (vl->plugin_instance[0] == '\0') ? 0 : 1,
      /* type =            */ 1,
      /* type instance =   */ (vl->type_instance[0] == '\0') ? 0 : 1,
      /* ds_name =         */ (ds_name == NULL) ? 0 : 1};

  if (vl->meta) {
    status = meta_data_get_string(vl->meta, meta_prefix, &temp);
    if (status == -ENOENT) {
      /* defaults to empty string */
    } else if (status < 0) {
      sfree(temp);
      return status;
    } else {
      prefix = temp;
    }

    status = meta_data_get_string(vl->meta, meta_id, &temp);
    if (status == -ENOENT) {
      /* defaults to empty string */
    } else if (status < 0) {
      sfree(temp);
      return status;
    } else {
      tsdb_id = temp;
    }

    for (i = 0; i < (sizeof(meta_tag_metric_id) / sizeof(*meta_tag_metric_id));
         i++) {
      if (meta_data_exists(vl->meta, meta_tag_metric_id[i]) == 0) {
        /* defaults to already initialized format */
      } else {
        include_in_id[i] = 0;
      }
    }
  }
  if (tsdb_id) {
    ssnprintf(ret, ret_len, "%s%s", prefix ? prefix : "", tsdb_id);
  } else {
#define TSDB_STRING_APPEND_STRING(string)                                      \
  do {                                                                         \
    const char *str = (string);                                                \
    size_t len = strlen(str);                                                  \
    if (len > (remaining_len - 1)) {                                           \
      ptr[0] = '\0';                                                           \
      return (-ENOSPC);                                                        \
    }                                                                          \
    if (len > 0) {                                                             \
      memcpy(ptr, str, len);                                                   \
      ptr += len;                                                              \
      remaining_len -= len;                                                    \
    }                                                                          \
  } while (0)

#define TSDB_STRING_APPEND_DOT                                                 \
  do {                                                                         \
    if (remaining_len > 2) {                                                   \
      ptr[0] = '.';                                                            \
      ptr++;                                                                   \
      remaining_len--;                                                         \
    } else {                                                                   \
      ptr[0] = '\0';                                                           \
      return (-ENOSPC);                                                        \
    }                                                                          \
  } while (0)

    char *ptr = ret;
    size_t remaining_len = ret_len;
    if (prefix) {
      TSDB_STRING_APPEND_STRING(prefix);
    }
    if (include_in_id[TSDB_TAG_PLUGIN]) {
      TSDB_STRING_APPEND_STRING(vl->plugin);
    }

    if (include_in_id[TSDB_TAG_PLUGININSTANCE]) {
      TSDB_STRING_APPEND_DOT;
      TSDB_STRING_APPEND_STRING(vl->plugin_instance);
    }
    if (include_in_id[TSDB_TAG_TYPE]) {
      TSDB_STRING_APPEND_DOT;
      TSDB_STRING_APPEND_STRING(vl->type);
    }
    if (include_in_id[TSDB_TAG_TYPEINSTANCE]) {
      TSDB_STRING_APPEND_DOT;
      TSDB_STRING_APPEND_STRING(vl->type_instance);
    }

    if (include_in_id[TSDB_TAG_DSNAME]) {
      TSDB_STRING_APPEND_DOT;
      TSDB_STRING_APPEND_STRING(ds_name);
    }
    ptr[0] = '\0';
#undef TSDB_STRING_APPEND_STRING
#undef TSDB_STRING_APPEND_DOT
  }

  sfree(tsdb_id);
  sfree(prefix);
  return 0;
}

static int wt_write_messages(const data_set_t *ds, const value_list_t *vl,
                             struct wt_callback *cb) {
  char key[10 * DATA_MAX_NAME_LEN];
  char values[512];

  int status;

  if (0 != strcmp(ds->type, vl->type)) {
    ERROR("write_opentsdb plugin: DS type does not match "
          "value list type");
    return -1;
  }

  for (size_t i = 0; i < ds->ds_num; i++) {
    const char *ds_name = NULL;
    int ret = 0;

    json_object *dp = json_object_new_object();

    if (cb->always_append_ds || (ds->ds_num > 1)){
      ds_name = ds->ds[i].name;
    }

    /* Copy the identifier to 'key' and escape it. */
    ret = wt_format_name(key, sizeof(key), vl, cb, ds_name);
    if (ret != 0) {
      ERROR("write_opentsdb plugin: error with format_name");
      status += ret;
      continue;
    }
    escape_string(key, sizeof(key));

    /* Convert the values to an ASCII representation and put that into
     * 'values'. */
    ret = wt_format_values(values, sizeof(values), i, ds, vl, cb->store_rates);
    if (ret != 0) {
      ERROR("write_opentsdb plugin: error with "
            "wt_format_values");
      status += ret;
      continue;
    }

    // Add the timestamp
    json_object *js_timestamp = json_object_new_double(CDTIME_T_TO_DOUBLE(vl->time));
    json_object_object_add(dp, "timestamp", js_timestamp);
    // Add the metric
    json_object  *js_key = json_object_new_string(key);
    json_object_object_add(dp, "metric", js_key);
    // Add the value
    json_object *js_values = json_object_new_string(values);
    json_object_object_add(dp, "value", js_values);

    // Add the tags
    status = wt_format_tags(dp, vl, cb, ds_name);
    if (status != 0) {
      ERROR("write_opentsdb plugin: error with format_tags");
      status += ret;
      json_object_put(dp);
      continue;
    }

    // We need some locks to avoid disaster
    pthread_mutex_lock(&cb->send_lock);

    /* Send the message to tsdb if buffer is full
     */

    if(cb->buffer_metric_size == cb->buffer_metric_max ){
      ret = wt_write_nolock(cb);
      status += ret;
    }

    /* Add the new metric to the buffer
     */
    json_object_array_add(cb->json_buffer, dp);
    cb->buffer_metric_size++;

    // Release lock
    pthread_mutex_unlock(&cb->send_lock);
  }

  return status;
}

/* Write callback
 */
static int wt_write(const data_set_t *ds, const value_list_t *vl,
                    user_data_t *user_data) {
  struct wt_callback *cb;
  int status;

  if (user_data == NULL)
    return EINVAL;

  cb = user_data->data;

  status = wt_write_messages(ds, vl, cb);

  return status;
}

/* Initialization of the plugin
 * create the wt_callback
 * initialize the curl object
 */
static int wt_config_tsd(oconfig_item_t *ci) {
  struct wt_callback *cb;
  char callback_name[DATA_MAX_NAME_LEN];

  cb = calloc(1, sizeof(*cb));
  if (cb == NULL) {
    ERROR("write_opentsdb plugin: calloc failed.");
    return -1;
  }
  cb->node = NULL;
  cb->store_rates = 0;
  cb->buffer_metric_max = 30;
  cb->buffer_metric_size = 0;
  cb->connect_failed_log_count = 0;
  cb->last_error_log = 0;
  cb->auto_fqdn_failback = 0;
  cb->json_host_tag = 0;

  pthread_mutex_init(&cb->send_lock, NULL);
  int status = 0;

  for (int i = 0; i < ci->children_num; i++) {
    oconfig_item_t *child = ci->children + i;

    if (strcasecmp("URL", child->key) == 0){
      char *base_url = NULL;
      cf_util_get_string(child, &base_url);
      cb->node = calloc(10 + strlen(base_url), 1);
      snprintf(cb->node, 10 + strlen(base_url), "%s/api/put", base_url);
      free(base_url);
    }
    else if (strcasecmp("Timeout", child->key) == 0)
      status = cf_util_get_int(child, &cb->timeout);
    else if (strcasecmp("BufferSize", child->key) == 0)
      status = cf_util_get_int(child, &cb->buffer_metric_max);
    else if (strcasecmp("JsonHostTag", child->key) == 0)
      status = cf_util_get_boolean(child, &cb->json_host_tag);
    else if (strcasecmp("AutoFqdnFallback", child->key) == 0)
      status = cf_util_get_boolean(child, &cb->auto_fqdn_failback);
    else if (strcasecmp("StoreRates", child->key) == 0)
      status = cf_util_get_boolean(child, &cb->store_rates);
    else if (strcasecmp("AlwaysAppendDS", child->key) == 0)
      status = cf_util_get_boolean(child, &cb->always_append_ds);
    else if (strcasecmp("VerifyPeer", child->key) == 0)
      status = cf_util_get_boolean(child, &cb->verify_peer);
    else if (strcasecmp("VerifyHost", child->key) == 0)
      status = cf_util_get_boolean(child, &cb->verify_host);
    else if (strcasecmp("CACert", child->key) == 0)
      status = cf_util_get_string(child, &cb->cacert);
    else if (strcasecmp("CAPath", child->key) == 0)
      status = cf_util_get_string(child, &cb->capath);
    else if (strcasecmp("ClientKey", child->key) == 0)
      status = cf_util_get_string(child, &cb->clientkey);
    else if (strcasecmp("ClientCert", child->key) == 0)
      status = cf_util_get_string(child, &cb->clientcert);
    else if (strcasecmp("ClientKeyPass", child->key) == 0)
      status = cf_util_get_string(child, &cb->clientkeypass);
    else if (strcasecmp("SSLVersion", child->key) == 0) {
      char *value = NULL;
      status = cf_util_get_string(child, &value);
      if (status != 0)
        break;
      if (value == NULL || strcasecmp("default", value) == 0)
        cb->sslversion = CURL_SSLVERSION_DEFAULT;
      else if (strcasecmp("SSLv2", value) == 0)
        cb->sslversion = CURL_SSLVERSION_SSLv2;
      else if (strcasecmp("SSLv3", value) == 0)
        cb->sslversion = CURL_SSLVERSION_SSLv3;
      else if (strcasecmp("TLSv1", value) == 0)
        cb->sslversion = CURL_SSLVERSION_TLSv1;
#if (LIBCURL_VERSION_MAJOR > 7) ||                                             \
    (LIBCURL_VERSION_MAJOR == 7 && LIBCURL_VERSION_MINOR >= 34)
      else if (strcasecmp("TLSv1_0", value) == 0)
        cb->sslversion = CURL_SSLVERSION_TLSv1_0;
      else if (strcasecmp("TLSv1_1", value) == 0)
        cb->sslversion = CURL_SSLVERSION_TLSv1_1;
      else if (strcasecmp("TLSv1_2", value) == 0)
        cb->sslversion = CURL_SSLVERSION_TLSv1_2;
#endif
#if (LIBCURL_VERSION_MAJOR > 7) ||                                             \
    (LIBCURL_VERSION_MAJOR == 7 && LIBCURL_VERSION_MINOR >= 52)
      else if (strcasecmp("TLSv1_3", value) == 0)
        cb->sslversion = CURL_SSLVERSION_TLSv1_3;
#endif
      else {
        ERROR("write_opentsdb plugin: Invalid SSLVersion "
              "option: %s.",
              value);
        status = EINVAL;
      }
      sfree(value);
    }
    else {
      ERROR("write_opentsdb plugin: Invalid configuration "
            "option: %s.",
            child->key);
      status = EINVAL;
    }
  }

  status = wt_config_curl(cb);

  cb->json_buffer = json_object_new_array();
  cb->buffer_metric_size = 0;

  ssnprintf(callback_name, sizeof(callback_name), "write_opentsdb/%s",
            cb->node != NULL ? cb->node : WT_DEFAULT_NODE);

  user_data_t user_data = {.data = cb, .free_func = wt_callback_free};

  plugin_register_write(callback_name, wt_write, &user_data);

  user_data.free_func = NULL;
  plugin_register_flush(callback_name, wt_flush, &user_data);

  return status;
}

/* Intialization of the curl structure
 */
int wt_config_curl(struct wt_callback *cb){

  if (cb->curl != NULL)
    return 0;

  cb->curl = curl_easy_init();
  if (cb->curl == NULL) {
    ERROR("curl plugin: curl_easy_init failed.");
    return -1;
  }

  if (cb->timeout > 0)
    curl_easy_setopt(cb->curl, CURLOPT_TIMEOUT_MS, (long)cb->timeout);

  curl_easy_setopt(cb->curl, CURLOPT_NOSIGNAL, 1L);
  curl_easy_setopt(cb->curl, CURLOPT_WRITEFUNCTION, writefunc);
  curl_easy_setopt(cb->curl, CURLOPT_USERAGENT, COLLECTD_USERAGENT);

  cb->headers = curl_slist_append(cb->headers, "Accept:  */*");
  curl_slist_append(cb->headers, "Content-Type: application/json");
  cb->headers = curl_slist_append(cb->headers, "Expect:");
  curl_easy_setopt(cb->curl, CURLOPT_HTTPHEADER, cb->headers);

  curl_easy_setopt(cb->curl, CURLOPT_ERRORBUFFER, cb->curl_errbuf);
  curl_easy_setopt(cb->curl, CURLOPT_URL, cb->node);
  curl_easy_setopt(cb->curl, CURLOPT_FOLLOWLOCATION, 1L);
  curl_easy_setopt(cb->curl, CURLOPT_MAXREDIRS, 50L);

  curl_easy_setopt(cb->curl, CURLOPT_SSL_VERIFYPEER, (long)cb->verify_peer);
  curl_easy_setopt(cb->curl, CURLOPT_SSL_VERIFYHOST, cb->verify_host ? 2L : 0L);
  curl_easy_setopt(cb->curl, CURLOPT_SSLVERSION, cb->sslversion);
  if (cb->cacert != NULL)
    curl_easy_setopt(cb->curl, CURLOPT_CAINFO, cb->cacert);
  if (cb->capath != NULL)
    curl_easy_setopt(cb->curl, CURLOPT_CAPATH, cb->capath);

  if (cb->clientkey != NULL && cb->clientcert != NULL) {
    curl_easy_setopt(cb->curl, CURLOPT_SSLKEY, cb->clientkey);
    curl_easy_setopt(cb->curl, CURLOPT_SSLCERT, cb->clientcert);

    if (cb->clientkeypass != NULL)
      curl_easy_setopt(cb->curl, CURLOPT_SSLKEYPASSWD, cb->clientkeypass);
  }
}

/* Plugin de-itialization
 */
static void wt_callback_free(void *data) {
  struct wt_callback *cb;

  if (data == NULL)
    return;

  cb = data;

  pthread_mutex_lock(&cb->send_lock);

  wt_write_nolock(cb);

  sfree(cb->node);

  if (cb->curl != NULL) {
    curl_easy_cleanup(cb->curl);
    cb->curl = NULL;
  }

  if (cb->headers != NULL) {
    curl_slist_free_all(cb->headers);
    cb->headers = NULL;
  }
  sfree(cb->cacert);
  sfree(cb->capath);
  sfree(cb->clientkey);
  sfree(cb->clientcert);
  sfree(cb->clientkeypass);

  pthread_mutex_destroy(&cb->send_lock);

  sfree(cb);
}

/* plugin initialization callback
 */
static int wt_config(oconfig_item_t *ci) {
  int status = 0;
  for (int i = 0; i < ci->children_num; i++) {
    oconfig_item_t *child = ci->children + i;

    if (strcasecmp("Node", child->key) == 0)
      status = wt_config_tsd(child);
    else {
      ERROR("write_opentsdb plugin: Invalid configuration "
            "option: %s.",
            child->key);
      status = EINVAL;
    }
  }
  return status;
}

/* Registering of the module
 */
void module_register(void) {
  plugin_register_complex_config("write_opentsdb", wt_config);
}

/* vim: set sw=4 ts=4 sts=4 tw=78 et : */
