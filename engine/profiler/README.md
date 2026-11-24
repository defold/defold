# Resource loading profiling

Enable per-resource load timings and logs in `game.project`:

```ini
[resource]
profile = 1
```

Each completed or failed load reports its path, total time, preload, read, create,
post-create, queue and wait times at INFO level. Pending post-create calls accumulate
into one record. Cache hits do not produce loading records.

Completed initial-load timings are also retained in each loaded resource's
descriptor. Open the resource profiler, click **Capture**, then **Show load times**
to reveal sortable **Load**, **Preload**, **Read**, and **Create** columns in
milliseconds. Showing the columns sorts by total load time, with bars showing each
total relative to the slowest visible resource. Click **Hide load times** to return
to the compact table; the timing columns are hidden by default.
An em dash means that profiling was disabled or the initial load has not completed.

Timing storage is embedded in the resource and loader structures, so it is also
allocated when `resource.profile=0`. On arm64, each resource descriptor grows from
56 to 88 bytes: 32 KiB extra at the default capacity of 1,024 resources. Each
preloader gains 56 KiB in its request array, and each pending post-create entry
grows by 96 bytes (12 KiB at the initial capacity of 128 entries). This avoids
separate allocations for timing state. Profiling clock reads and timing logs are skipped
while disabled.

Timings remain unchanged on cache hits and hot reloads, and disappear when the
resource is unloaded. Callback times include nested dependency loads, so totals
across resources must not be added together. Capturing the table does not enable
profiling retroactively; launch with `resource.profile=1` to measure startup loads.


# Profile property peak report

Enable peak counter tracking in `game.project`:

```ini
[profiler]
track_peak_counters = 1
```

This setting is disabled by default. When enabled, debug engines track the
highest value reached by every numeric and boolean `DM_PROPERTY_*` property.
Existing `DM_PROPERTY_SET_*`, `DM_PROPERTY_ADD_*`, and
`DM_PROPERTY_RESET` calls participate automatically, including properties
registered by extensions. Groups are omitted from the report.

Peaks are captured on each update, so a value that falls again before the end of
a frame is still included. Frame resets restore the current value to its declared
default without clearing its peak. Defaults are included even when a property
was never updated. Each enabled engine instance starts a reporting period from
the property defaults after loading its configuration, including after
`sys.reboot()`.

When an enabled game engine instance is deleted, it writes the property names,
peaks, and descriptions to `profile.txt`. Peak values are not printed to the
console or log. The file is overwritten on each shutdown. Multiple editor instances use
`instance_<index>_profile.txt`, matching the log filename convention.

The engine tries these locations in order:

1. `project.log_dir`, when set.
2. The platform log directory (the working directory on desktop).
3. The application's support directory, as for `log.txt`.

With `profiler.track_peak_counters=0`, peak collection and reporting are skipped;
the on-screen and remote profiler counters continue to work normally.
The report is written independently of `project.write_log_file`. Its destination
is logged on success, and file failures are logged as warnings. Release
engines and the built-in connection app do not write reports. Browser builds use
the platform's virtual filesystem and require an orderly shutdown to report.

Collection lives in `dlib/profile`, independently of the on-screen or remote
profiler backend. `ProfileGetPropertyPeaks()` copies a consistent, typed snapshot
under the existing profiler lock. Formatting and file output happen after
the lock is released. Properties do not currently carry `game.project` limit
metadata, so this report contains measured peaks without limit comparisons.

Tests: `test_profile` covers collection and backend dispatch;
`test_profile_report` covers engine shutdown, file output without peak logging, reboot, and debug
mode suppression with null graphics.
