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

