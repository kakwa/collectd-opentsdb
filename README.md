# collectd-opentsdb

[![Build Status](https://travis-ci.org/kakwa/collectd-opentsdb.svg?branch=master)](https://travis-ci.org/kakwa/collectd-opentsdb)

## Description

This plugin is a fork of the write_tsdb plugin.

It adds a few improvement over the stock write_tsdb plugin:

* inclusion of [write_tsdb plugin: Export metadata](https://github.com/collectd/collectd/pull/1655/files) PR for tag setup.
* implementation of the [http "/api/put" API](http://opentsdb.net/docs/build/html/api_http/put.html) of OpenTSDB instead of the "telnet protocol".
* support of ssl/tls with optional client side certificates for authentication.
* support for settings tags through json data in Hostname.

## Documentation

The full plugin documentation is available here:

* [docs/collectd-opentsdb.pod](https://github.com/kakwa/collectd-tsdb2/blob/master/docs/collectd-opentsdb.pod)

Alternatively, once installed:

```bash
man collectd-opentsdb
```

## Dependencies

* [collectd](https://collectd.org/)
* [libcurl](https://curl.haxx.se/)
* [libjson-c](https://github.com/json-c/json-c)

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
                URL "http://localhost:5000"
                JsonHostTag true
                AutoFqdnFallback false
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
      MetaData "tsdb_tag_pluginInstance" "cpu"
      MetaData "tsdb_tag_type" ""
      MetaData "tsdb_prefix" "sys."
    </Target>
  </Rule>
  <Rule "opentsdb_df">
    <Match "regex">
      Plugin "^df$"
    </Match>
    <Target "set">
      MetaData "tsdb_tag_pluginInstance" "mount"
      MetaData "tsdb_tag_type" ""
      MetaData "tsdb_prefix" "sys."
    </Target>
  </Rule>
  <Rule "opentsdb_disk">
    <Match "regex">
      Plugin "^disk$"
    </Match>
    <Target "set">
      MetaData "tsdb_tag_pluginInstance" "disk"
      MetaData "tsdb_prefix" "sys."
    </Target>
  </Rule>
  <Rule "opentsdb_interface">
    <Match "regex">
      Plugin "^interface$"
    </Match>
    <Target "set">
      MetaData "tsdb_tag_pluginInstance" "iface"
      MetaData "tsdb_prefix" "sys."
    </Target>
  </Rule>
  <Rule "opentsdb_load">
    <Match "regex">
      Plugin "^loac$"
    </Match>
    <Target "set">
      MetaData "tsdb_tag_type" ""
      MetaData "tsdb_prefix" "sys."
    </Target>
  </Rule>
  <Rule "opentsdb_swap">
    <Match "regex">
      Plugin "^swap$"
    </Match>
    <Target "set">
      MetaData "tsdb_prefix" "sys."
    </Target>
  </Rule>
</Chain>
```

## Changelogs

### 0.0.3

* fix typo in AutoFqdnFallback option name (previously: AutoFqdnFailback)
* better man page

### 0.0.2

* capping error logs to two every 30 seconds if http POST of metrics fails (avoid spamming syslog with error logs)
* fix error handling in metric treatment
* add man page installation
* clean man page

### 0.0.1

* first version
