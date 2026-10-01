# Sprite snapshot and render-thread proof of concept

Use `--config=render.sprite_snapshot=0` for existing rendering, `=1` for inline
snapshots, or `=2` for the macOS/Metal threaded experiment. The benchmark project
is `/Users/jhonny/dev/defold-projects/experiment-decoupled-rendering`.

## Implemented scope

Milestones 1–5 provide sprite capture, inline consumption, independent producer
recording/extraction, a bounded render worker and conservative resource barriers.
Milestones 6–7 add a counterbalanced comparison collector, memory diagnostics and
sanitizer/lifecycle validation. The render-command subset below is the final PoC scope.
Mode 0 remains the default. Mode 1 supports the existing component/render-script
pipeline. Mode 2 supports one main collection containing sprites, scripts and
preloaded factory components; other component creation is rejected.

Each sprite record is 128 bytes and each bound is 16 bytes (squared radius).
Bindings, equal constant sets and resolved animation geometry are deduplicated. Constants and dynamic
attributes are copied into contiguous frame arrays. Resource references retain
sprite descriptors, effective materials and effective texture sets. The geometry
consumer has no live component pool or animation-cache pointer in snapshot mode.
Capture resolves each unique geometry from the current resource generation, never
from a cache entry that may predate an atlas replacement after component update.
A stack-only component-shaped view adapts captured values to the existing geometry
math without duplicating the gameplay component pool.

`SpriteRendererState` owns staging vertices/indices, render objects, attribute and
geometry scratch, constant buffers and buffered GPU handles. Simulation no longer
rewinds those GPU buffers. Visibility stays in the render-list consumer's scratch,
so different frusta can reuse the same captured bounds. Sorting and batching remain
in the existing global render list, preserving ordering across component types.

Quads, slice9, trimmed geometry, multiple textures, animation frames, component
constants, dynamic attributes and render-material vertex layouts use this path.
Tests compare generated data with the legacy path and destroy/change components
after capture, including animation-cache eviction and component-pool compaction.

## Threaded ownership and admission

The single producer reserves a CPU slot before game-object update. Simulation can
run while the previous frame renders. Lua commands are recorded into producer-only
storage; supported recording APIs do not mutate consumer state. After post-update
and system messages, the producer captures sprites into its reserved slot while
the worker may still read the other slot. Only then does it wait for completion,
retire the previous slot, prepare the surface and publish. The worker owns
render-list construction, culling, sorting, batching, geometry, graphics submission
and presentation. Lua, input/event polling and window APIs stay on main. Metal's
surface dimensions/layer are prepared on main while the consumer is idle;
`MetalBeginFrame` skips window queries in threaded mode. Metal completion callbacks
synchronize their frame resources explicitly with the submitting thread. Each
worker frame has an autorelease pool, drained on that same worker after submission.

There are exactly two CPU slots: Free → Building → Ready → Reading → Free. The
producer may build N+1 while N is reading; publishing N+1 waits for N to complete.
Consequently there is at most one published, incomplete frame, no latest-frame
replacement, and no queue of older ready frames. Ordered IDs are checked.
`thread_captures_with_consumer_outstanding` counts captures completed before the
previous consumer finished, providing evidence that the boundary permits overlap. CPU-slot completion means submission/presentation was encoded;
it does not mean the GPU has finished.

Each slot has an allocation-time 32 MiB capacity ceiling, including capture maps
and metadata (64 MiB total). Admission conservatively includes the old allocation
while its replacement grows, rather than counting only the final retained size.
Arrays retain their high-water capacities. Exceeding
the budget rejects the frame and exits the experiment instead of dropping sprites.
There are 128 command entries per slot in fixed storage. View/projection matrices,
frusta and predicate tags are copied; no Lua userdata crosses the boundary.
Supported commands are draw without per-draw constants, default render target,
clear, viewport, view/projection, blend/depth/stencil/cull/color-mask and polygon
offset state. Per-sprite constants/attributes remain supported. Camera handles,
material overrides, texture bindings, custom render targets, debug draws and compute
commands reject capture. Render APIs that would touch shared state are rejected
before handle lookup/allocation, including per-draw constant cloning and render
listeners. Built-in text/line/resize requests are rejected; ordinary Lua messages
and window-resized notifications remain producer-owned. Native extension render hooks, profiler overlays, screen
recording and other component renderers are outside threaded mode's scope.

## Resource lifetime and controls

The resource factory and its reference counts remain main-thread-owned. Slot
references are released by the producer after CPU drain; the consumer never calls
the resource factory. Before creation, destruction or in-place replacement, the
factory/graphics mutation hooks request a synchronous control on the render worker.
The worker drains submitted GPU work **and completion callbacks** by acquiring all
Metal frame permits, then returns graphics ownership temporarily to main. Main
performs the mutation; the next publication hands ownership back to the worker.
This intentionally stalls resource changes and avoids making the existing resource
loaders and factory thread-safe. It is a conservative ownership transfer, not a
parallel upload/resource implementation. Texture upload jobs are made synchronous
while this mode is enabled; startup uploads finish before the worker starts.

The control lane has one fixed entry and no owned upload payload; callers block
until completion. It runs independently of frame submission, including while
rendering is paused. The private `sprite._snapshot_pause(bool)` exercises both the
GPU barrier and this idle-worker path. Shutdown drains CPU/GPU work and joins the
worker before resources/worlds are destroyed. General collection streaming,
multiple render worlds and platform/device-loss recovery remain out of scope.

The benchmark's optional same-layout resource stress repeatedly updates the texture used by
sprites, replaces its atlas DDF, and creates/releases one scratch texture. Upload
payload is capped at 64 KiB and only one scratch resource is live. Optional pauses
alternate every stress event and issue an extra control while already paused.
These runs deliberately include mutation waits and are **not** steady-state
performance comparisons. Normal runs leave stress disabled.

## Diagnostics and validation

The private Lua helper `sprite._get_snapshot_stats()` reports the active mode,
record sizes, active payload bytes, retained frame capacity, table/reference counts
and cumulative capture time/count for the caller's collection. The private
`sprite._snapshot_clock()` supplies monotonic elapsed seconds for the benchmark. Threaded mode also
reports submitted/completed IDs, slot/control bounds, producer wait, consumer work,
frame age, control time, fixed command capacity and the worker stack reservation.
Producer-owned slot statistics are copied after extraction. Renderer-owned
statistics are copied after the publication drain, so sampling never reads buffers
that the worker may be growing. Queue counters are read under its mutex. Sampling does not drain the renderer. It is an internal
PoC probe, not a supported SDK API. The benchmark samples it once per second.

Memory counters have distinct scopes and must not be summed with RSS:

- `frame_capacity_bytes`: both slots' retained arrays, lookup tables and metadata.
- `payload_used_bytes`: active captured arrays, excluding unused capacity/maps.
- `frame_growth_peak_bytes`: sum of per-slot historical conservative growth peaks,
  including old/new allocations coexisting. This bounds rather than measures a
  simultaneous allocation peak, and excludes allocator overhead.
- `renderer_cpu_capacity_bytes`: staging, render objects, geometry/attribute scratch
  and named constant buffers; `constant_buffer_capacity_bytes` is its constant subset.
- `renderer_gpu_logical_bytes`: logical vertex/index buffer sizes, excluding Metal
  alignment, transient replacements and other GPU resources.
- `retained_resource_reported_bytes`: unique directly retained sprite/material/
  texture-set descriptor sizes for the latest slot. These are factory-reported
  sizes, not exclusive ownership or a complete transitive resource graph. Resources
  shared between slots are not charged twice in this steady-state sample, taken
  after retiring the previous slot.
- `metal_device_allocated_bytes`: Metal's device-reported current resource allocation,
  available for all three modes. It includes allocations outside the sprite renderer
  but is not a complete driver-memory measurement. It overlaps process RSS on
  unified-memory hardware.
- Process RSS is sampled externally once per second; brief peaks can be missed.

Shared render-context allocations, allocator overhead, thread-local/runtime storage,
and opaque driver allocations are not attributed to sprites. Fixed command/queue
storage and the worker stack reservation are reported separately. Missing adapter
memory support is omitted rather than reported as zero.

Capacity grows geometrically within the threaded slot limit and stays allocated for reuse. Stable capacity after
warm-up differs from a leak; active sprite/table/reference counts should return to
zero after deletion and an empty capture. Capture timing uses monotonic elapsed time around extraction, so it includes
scheduling delays as well as dependency retention, table construction and copying.
It excludes later geometry generation and GPU execution.

Use the same optimized PoC binary and compiled content with snapshot modes 0, 1 and 2
for comparisons. The archived original-renderer baseline predates this refactor
and the private diagnostics. Do not compare debug/O0 runs with optimized runs.

## Building and validation

Use an isolated CMake `RelWithDebInfo` build with `DEFOLD_BUILD_HOME` and
`DEFOLD_SKIP_BOB_LIGHT=ON`, reusing the installed Bob and SDK dependencies.
Build `dmengine_release`, `test_gamesys`, `test_render` and sprite/material content.
Run gamesys tests from the generated `gamesys-test-runtime` directory. Use separate
invocations for each jc_test filter. The new tests cover command ownership/rejection,
slot ordering, paused controls, deletion before consumption and capacity rejection.
For ThreadSanitizer use a separate build with `-DWITH_TSAN=ON`; do not put sanitizer
flags in global CMake flags, as build-time Python loads an unsanitized DDF helper.
Sanitizer results are correctness checks, never performance measurements.

The original optimized baseline and quality audit are in
`results/baseline-20260930-o2`, and prior inline validation is in
`results/inline-validation-20260930`. New threaded validation preserves the engine,
source snapshot, per-run JSON, sampled RSS and test logs under
`results/threaded-validation-20260930-final`. Presentation pacing and interactive host
activity limit speedup conclusions. Frame age is measured from slot reservation
before simulation to consumer completion, not display scanout or GPU completion.

## Completion evaluation

`tools/collect_matrix.py` in the benchmark project freezes the optimized engine,
changed/new engine sources, benchmark source and compiled content. Its 16 unique
workloads cover counts, visibility, simulation, moving fraction, passes and constant
batch diversity. Each runs in all three modes with three repetitions, ten seconds
warm-up and thirty seconds measurement (144 trials). Adjacent mode order is
counterbalanced per workload, and workload order rotates between repetitions.
The common 10k control is shared across dimensions. Checkpoints support resuming
without discarding failures. Reports provide medians, ranges and within-repetition
ratios, without claiming significance from three repetitions on an interactive host.

Completion evidence lives under `results/completion-*`. Sanitizer runs are separate
from performance measurements. Longer active/paused resource-churn trials check
capacity plateaus, cleanup, queue bounds and memory trends. ASan covers existing,
inline and threaded paths; TSan exercises overlap and paused resource controls.
The narrow PoC does not implement other components/backends, asynchronous streaming,
custom render targets/cameras, device-loss recovery or input-to-display latency.

## Follow-up evaluation

Capture reuses the preceding binding only after comparing the sprite resource,
effective material and every effective texture-set pointer. This avoids repeated
descriptor construction/hashing for adjacent equal bindings; the index is local to
one capture and does not cache resource generations across frames. Geometry still
resolves against the current atlas generation. The adjacent-override regression
compares captured and legacy geometry as texture/material overrides change.
Animated churn also exposed stale simulation-cache pointers after atlas DDF
replacement. Cache entries now validate texture-set generations before use, including release
builds with reload callbacks disabled. The generation counter is widened to 32 bits.
A separate regression exercises update/render after direct same-layout recreation
without reload notifications. This preserves cache reuse between resource changes.

`--config=render.sprite_trace=/absolute/path/pipeline.csv` enables an optional
65,536-record CPU/Metal trace (168 bytes per record, 10.5 MiB). It records the start of
CPU input processing, update end, preparation end, publication, consumer start and
Flip return. The producer publishes the record through the existing queue; the
consumer writes render-stage fields, and Metal callbacks write GPU fields. Records are
never reused. Full buffers count dropped records, and analysis rejects overflow.
Shutdown joins the worker and drains GPU completion callbacks before writing/freeing the trace. Normal throughput runs
leave tracing disabled. Inline preparation includes render-list construction;
threaded preparation includes Lua command recording and sprite capture, so the
stage durations are not identical work across modes.

These timestamps are NOT physical input arrival, GPU completion, presentation or
scanout. The benchmark's optional input probe records click-dispatch times and
changes the background. A camera/sensor must measure physical input-to-display
latency separately. Diagnostic traces and sanitizer runs are excluded from
performance comparisons.

The benchmark project provides `tools/compare_capture.py` for alternating old/new
inline comparisons, `tools/collect_matrix.py --suite representative --require-ac`
for synthetic crowd/mixed/fill-heavy scenes at uncapped/60/120 Hz, and
`tools/analyze_evaluation.py` for provisional gates. Actual game validation and
physical latency are explicit pending requirements, not implied by synthetic
throughput. AC power is checked every five seconds; interactive host activity and
thermal variation remain limitations. Six repetitions counterbalance mode order
twice. Results are preserved separately from the original completion evidence.

### Detailed wait diagnostics

The optional trace separates pacing duration, deadline overshoot, producer queue
wait, snapshot finish, main-thread surface work, publication-to-worker delay,
Metal frame-slot wait, drawable acquisition, CPU encoding and command submission.
GPU execution duration is read asynchronously from Metal completion timestamps;
it is a duration on Metal's clock, not an absolute CPU timestamp or scanout time.
Only completion callbacks write GPU fields. Bounded trace records are never reused;
all modes drain GPU callbacks before exporting and freeing storage. Tracing remains
off for throughput comparisons. The Python analyzer accepts historical CPU-only
traces and validates ordering and GPU completion for the extended format.
