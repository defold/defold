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

Timings remain unchanged on cache hits and hot reloads, and disappear when the
resource is unloaded. Callback times include nested dependency loads, so totals
across resources must not be added together. Capturing the table does not enable
profiling retroactively; launch with `resource.profile=1` to measure startup loads.

