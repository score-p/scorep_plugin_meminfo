# Scorep Plugin Meminfo
## Usage
### Environment Variables

#### General

* `SCOREP_METRIC_PLUGINS=meminfo_plugin`

* `LO2S_METRIC_MEMINFO_PLUGIN` (required)

    Comma-separated list of metrics.
    Metrics can also contain regular expressions (`.*`)

* `SCOREP_METRIC_MEMINFO_PLUGIN_INTERVALL` (optional, default: "10ms")

    Define the intervall for testing on memory usage.
    The value is a duration string which will be parsed, e.g. `100 ms`.
