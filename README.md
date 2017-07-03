# collectd-opentsdb

## Description

This plugin is a fork of the write_tsdb plugin.

It adds a few improvement over the stock write_tsdb plugin:

* inclusion of [write_tsdb plugin: Export metadata](https://github.com/collectd/collectd/pull/1655/files) PR for tag setup.
* implementation of the [http "/api/put" API](http://opentsdb.net/docs/build/html/api_http/put.html) of OpenTSDB instead of the "telnet protocol".
* support of ssl/tls with optional client side certicate for authentication.
* support for settings tags at through json data in Hostname.

## Documentation

The plugin documentation is available here:

* [docs/collectd.conf.pod](https://github.com/kakwa/collectd-tsdb2/blob/master/docs/collectd.conf.pod)

## Dependencies

* collectd
* libcurl
* libjson-c

## Building

```bash
# change your install prefix according to your collect installation
cmake . -DCMAKE_INSTALL_PREFIX=/usr/

# compilation
make

# install
make install
```

## Configuration

Here is a configuration example for this plugin

```xml

# Hostname used to set some static tags
Hostname "{\"fqdn\": \"http.node1.example.org\", \"env\": \"prod\", \"role\": \"http\"}"

# Plugin configuration
LoadPlugin write_opentsdb

<Plugin write_opentsdb> 
        <Node> 
                Url "http://localhost:5000" 
                JsonHostTag true 
                AutoFqdnFailback false 
                StoreRates false 
                AlwaysAppendDS false 
        </Node> 
</Plugin>


# Metric rewrite/tag handling
LoadPlugin match_regex
LoadPlugin target_set


<Chain "PreCache">
  <Rule "opentsdb_cpu">
    <Match "regex">
      Plugin "^cpu$"
    </Match>
    <Target "set">
      MetaDataSet "tsdb_tag_pluginInstance" "cpu"
      MetaDataSet "tsdb_tag_type" ""
      MetaDataSet "tsdb_prefix" "sys."
    </Target>
  </Rule>
  <Rule "opentsdb_df">
    <Match "regex">
      Plugin "^df$"
    </Match>
    <Target "set">
      MetaDataSet "tsdb_tag_pluginInstance" "mount"
      MetaDataSet "tsdb_tag_type" ""
      MetaDataSet "tsdb_prefix" "sys."
    </Target>
  </Rule>
  <Rule "opentsdb_disk">
    <Match "regex">
      Plugin "^disk$"
    </Match>
    <Target "set">
      MetaDataSet "tsdb_tag_pluginInstance" "disk"
      MetaDataSet "tsdb_prefix" "sys."
    </Target>
  </Rule>
  <Rule "opentsdb_interface">
    <Match "regex">
      Plugin "^interface$"
    </Match>
    <Target "set">
      MetaDataSet "tsdb_tag_pluginInstance" "iface"
      MetaDataSet "tsdb_prefix" "sys."
    </Target>
  </Rule>
  <Rule "opentsdb_load">
    <Match "regex">
      Plugin "^loac$"
    </Match>
    <Target "set">
      MetaDataSet "tsdb_tag_type" ""
      MetaDataSet "tsdb_prefix" "sys."
    </Target>
  </Rule>
  <Rule "opentsdb_swap">
    <Match "regex">
      Plugin "^swap$"
    </Match>
    <Target "set">
      MetaDataSet "tsdb_prefix" "sys."
    </Target>
  </Rule>
</Chain>
```
