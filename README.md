## Usage
### Environment Variables

#### General

* `SCOREP_METRIC_PLUGINS=meminfo_plugin`

* `SCOREP_METRIC_MEMINFO_PLUGIN` (required)

    Comma-separated list of values to test for.
    Can also contain regular expressions (`.*`)

* `SCOREP_METRIC_MEMINFO_PLUGIN_INTERVALL` (optional, default: 10 ms)

    Intervall of testing.
    The value is a duration string which will be parsed, e.g. `100 ms`.
