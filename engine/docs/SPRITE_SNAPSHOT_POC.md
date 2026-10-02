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
preloaded factory components. The opt-in mixed milestone described below also
admits GUI and particle FX with `render.mixed_preparation=1`. Other component
creation is rejected unless explicitly admitted by the gameplay experiment below.

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
The narrow PoC does not implement all component types/backends, asynchronous streaming,
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


## Mixed-component milestone (October 2026)

`render.mixed_preparation=1` extends the experiment to standard GUI boxes/text and
standalone particle FX in the main collection. The benchmark enables this flag.
Mode 0 keeps the original renderer. Mode 1 performs sprite snapshots plus the same
mixed geometry preparation, constant copying and upload copying on the main
thread. Mode 2 moves consumption and upload replay to the worker. This inline
control measures restructuring cost separately from the additional overlap.
Without the flag, the original sprite-only modes retain their behavior.

GUI and particle simulation remain on main. After the previous consumer completes,
main generates their geometry through the existing component renderers and
materializes owned draw packets. Each packet copies the render object, named
constants, ordering metadata and an immutable culling sphere where applicable.
GUI stencil parameters travel in the copied render object. Text becomes geometry;
all queued TextLayout references are released on main before publication. The
worker never reads a GUI node, text layout, emitter or particle simulation array.

Vertex buffer allocation/sub-data requests during preparation are recorded in an
owned byte buffer and replayed on the consuming thread before drawing. Ordinary
per-frame uploads therefore do not force GPU drain. The single viewport is only
applied on main when it changes; blindly reapplying it would invoke the conservative
mutation barrier every frame. Resource creation, destruction, glyph-cache texture
changes and other mutations still use that barrier. Stable warmed-up runs should
report zero controls unless an actual resource mutation occurs.

Sprite extraction can overlap the previous consumer, as before. Mixed geometry
preparation currently happens **after** CPU drain, so it is a serial part of the
frame. There is one reusable mixed packet/upload set, protected by that drain and
the existing publication mutex, alongside the two sprite slots. This is a measured
migration step, not fully asynchronous GUI/particle geometry generation. Geometry
is generated before consumer culling. Text nodes and particle emitters are
materialized individually, which can produce more draws than legacy batching;
GUI box batching still occurs in the component renderer. These costs appear in
both mixed snapshot modes and are candidates for the next optimization.

The command subset additionally requires one explicit view, projection and viewport
before drawing, with the same values throughout a frame. The benchmark disables
GUI automatic adjustment so GUI and world geometry use the same fixed projection.
Different per-pass projections, material overrides, render targets, cameras,
collection streaming, custom GUI extensions and other component families are not
validated by this milestone. Particle and sprite materials share the `tile` tag in
the mixed benchmark, exercising global world ordering in one pass; GUI follows
with stencil clipping enabled.

### Mixed memory and admission

There are at most 4,096 prepared draw packets and 4 MiB of retained copied constant
storage, including conservative replacement admission. Vertex upload capture
allows 4,096 commands and 16 MiB of active bytes, with a 32 MiB ceiling for
conservative old/new allocation coexistence. Overflow rejects the frame before
publication; no live-pointer or direct-upload fallback is used. These limits are
additional to the existing sprite slot budgets. Component geometry scratch and
font/glyph/GPU allocations still follow their existing configured limits.

The private sprite probe reports producer-owned `mixed_packet_count`,
`mixed_packet_capacity_bytes`, `mixed_prepare_count`, `mixed_prepare_us_total`,
`mixed_upload_used_bytes`, `mixed_upload_capacity_bytes` and
`mixed_upload_growth_peak_bytes` in both snapshot modes. Preparation time covers
GUI/particle extraction, draw copying and upload recording, excluding queue wait
and actual GPU upload. `captured_command_capacity_bytes` reports fixed command
storage. Packet and upload capacities retain their high-water allocations; active
packet count and upload bytes must return to zero after cleanup.

The older `renderer_cpu_capacity_bytes` and `renderer_gpu_logical_bytes` counters
still describe the sprite renderer. They do not suddenly include GUI, text or
particle staging. Whole-process RSS and Metal allocation measurements cover wider,
overlapping scopes and must not be added to the component counters.

### Mixed validation and collection

Render tests cover draw/constant/bounds ownership, stencil/order retention,
TextLayout release before consumption, deferred upload order and byte ownership,
allocation/command rejection, and preservation of sprite dispatches by the inline
control. The particle regression compares generated positions with legacy
rendering and consumes them after another simulation update, including culling.
The legacy fixture contains unspecified vertex fields and spare allocation bytes,
which are excluded from that geometry comparison.

`tools/verify_mixed_pixels.py` records Metal framebuffer readbacks at the same
update number in all three modes and requires exact equality for a deterministic
sprite/GUI/text scene with node deletion/recreation, alpha and clipping. It uses
`render.poc_capture_path` and `render.poc_capture_frame`; readback stalls the GPU
and these runs are excluded from timing evidence. Particle randomness is not
synchronized across separate processes; particle geometry is checked in the
native regression instead of claiming pixel equality for that simulation.

`tools/collect_mixed.py --phase throughput` runs mixed 20k-sprite crowd and 5k-sprite
balanced workloads. `--phase pacing` runs the mixed fill-heavy scene at 60 and
120 Hz. Every case adds 64 boxes, 64 changing text nodes and eight emitters with a
combined capacity of 2,048 particles. Existing collector freezing, mode rotation,
AC checks, visible/no-focus windows, RSS sampling and cleanup validation apply.
Use untraced optimized runs for comparisons; sanitizer, readback and churn runs
are separate. The first exploratory collection used an incomplete inline control
and is preserved separately; do not pool it with the final comparison.

### Completed mixed evaluation

The authoritative October 1 timing/memory collection is `mixed-*-20261001-v3`
in the benchmark project's `results` directory. Start with
`mixed-analysis-20261001-v3/findings.md`; it links the raw data, CSVs, diagnostics
and correctness evidence. The unchanged optimized engine has SHA-256
`0bb7a54aa488aabe8ba1c8502fb99934cb9e9921f0cd55a4465f601abff8300b`.

Across three paired throughput repetitions, threading improved the mixed 20k
crowd by 23.9% and the 5k balanced scene by 25.2% relative to the original renderer.
The equivalent inline path was approximately neutral and +1.9%, respectively.
Six untraced repetitions at each cap showed median paired p99 regressions of
10.2% at 60 Hz and 14.7% at 120 Hz: the provisional pacing gate still fails.
Short separate traces identify preparation tails and pacing/dispatch delays as
avenues, not a proven single cause. GUI/particle preparation remains serial.

Mixed packet/upload capacity retained 1.06 MiB in both snapshot modes. Whole-
process sampled peak RSS increased by 13.63 MiB for the threaded 20k crowd and
3.33 MiB for the 5k scene (medians of per-run peaks). In separate 60-second churn
runs, owned capacities stayed constant and all active payload/reference counts
cleared after deletion. RSS does not establish the absence of leaks.

The v2 timing collection is excluded because power history confirmed display
off/on transitions. Controlled macOS runs now verify a temporary display/system-
awake assertion before launching the engine and release it on every exit path.
AC checks alone were insufficient. The corrected collection's power audit has
no display transitions. Windows remain visible without taking focus. The v2
ASan/TSan and exact-pixel runs still validate the same binary's correctness;
neither sanitizer nor readback measurements are timing evidence.

The result supports continuing the experiment, not production rollout. Address
capped preparation/scheduling tails next, then validate a real project, another
physical target/backend and physical input-to-display latency.

### Pacing experiments

Two Apple-only options keep the default engine behavior unchanged:

- `render.poc_deadline_wait=0`: existing `dmTime::Sleep` pacing.
- `render.poc_deadline_wait=1`: blocking `mach_wait_until` using the existing
  monotonic frame deadlines.
- `render.poc_deadline_wait=2`: staged blocking waits. Sleep for approximately
  half of the remaining interval, recheck, and finish with a full
  deadline wait once at most 250 microseconds remain. At most eight early waits
  are attempted. Every wait blocks in the OS; there is no busy-wait finish.
- `render.poc_qos=0/1/2/3`: no override, interactive QoS for main, for the render
  worker, or for both. Overrides are scoped to engine/worker lifetime and do not
  change the host thread's requested QoS. Main's override starts after worker
  creation so the main-only treatment does not affect inherited worker settings.

The timer option changes the wait implementation only: deadline advancement,
missed-deadline handling, timestep accounting, and uncapped execution retain their
existing behavior. Conversion uses duration-sized integer arithmetic, rounds up
fractional ticks, and chunks long waits to avoid overflow. Interrupted waits
recheck the monotonic deadline. A Mach API error falls back to ordinary sleep and
prints an error that invalidates the benchmark. Non-Apple engines reject these
experimental settings. No QoS override is needed to use either timer treatment.

The benchmark adds `text_update_period` (30 by default, zero freezes text) and
`text_update_stagger` (zero by default). Staggering keeps equal per-node update
counts over a full period while spreading the work across frames. The isolation
matrix keeps the sprite/fill workload fixed and varies GUI, particles, combined
components, frozen text and staggered text. All are available in
`tools/collect_pacing.py` in the benchmark project.

On macOS the collector reads aggregate process user+system CPU time and resident
bytes directly through `proc_pid_rusage`, avoiding a `ps` subprocess per sample.
CPU tick conversion uses the host's Mach timebase and is checked against Python's
independent process CPU clock. `--process-sampler ps` retains the earlier sampler
for an explicit interference comparison; other platforms keep the `ps` fallback.
CPU utilization is the cumulative CPU delta divided by elapsed time between the
first and last measurement-phase samples; 100% means one fully occupied core.
The native sampler also records physical footprint and OS-reported interrupt and
package-idle wakeups. These counters exclude phase edges and are not a wattage or
energy measurement. Controlled
runs continue to require AC and verified awake assertions. The final policy
comparison counterbalances timer order independently from cap order and rotates
the original/inline/threaded rendering modes. Diagnostic traces and untraced
performance trials remain separate.

These options are experimental tools, not a proposed cross-platform pacing API.
Extra wakeups and scheduling changes need energy testing on a physical device
before production adoption, even if sampled process CPU cost is small.

### Completed pacing evaluation

The final collection is `pacing-confirmation-20261001-v1` in the benchmark
project's `results` directory. Read `pacing-analysis-20261001-v1/findings.md`
for the paired comparisons, raw evidence and reproduction command. It contains
72 untraced trials: six repetitions of three renderer modes, two frame caps and
two timer policies, with five seconds warm-up and twenty seconds measurement.
All collection checks passed, with no display or sleep transitions recorded.

At 60 Hz, the threaded renderer's median p99 update interval changed from
17.60 ms with the default wait to 16.80 ms with staged waiting; the median paired
improvement was 4.7%. At 120 Hz the result was effectively neutral: 9.150 versus
9.175 ms. Both timer policies passed the provisional median threading/original
p99 gate at both caps. Since the default timer also passed, this does not
establish that staged waiting repaired the earlier mixed milestone's regression.
The six-pair median gate does not imply that every individual pair passed.
Diagnostic traces and older collections remain separate observations.

The staged policy changed threaded process CPU utilization by -2.12 percentage
points at 60 Hz and -0.30 points at 120 Hz (median paired differences, with 100%
representing one core). At 60 Hz, OS-reported process interrupt wakeups increased
from approximately 203 to 504 per second. Sampled peak RSS was essentially
unchanged, all tracked retained capacities stayed constant within every trial,
and active payloads cleared at teardown. These are not energy or leak proofs.

Focused engine tests, 30 Python tests, Lua tests and ASan/TSan checks passed;
the report records their scope and sanitizer settings. Uncapped throughput was
not remeasured in this milestone. Keep the timer experiments opt-in while
validating a representative game, actual energy use, another physical target
and physical input-to-display latency.

### External-project replay evaluation

The benchmark project's `REPLAY.md` documents a reusable runner and small Lua
adapter SDK. The runner freezes arbitrary prebuilt project content and the
engine, rotates renderer conditions, and rejects divergent gameplay checkpoints.
Threading, mixed preparation and timer treatments remain explicit opt-ins.

`render.poc_gameplay_components=1` admits collision objects and sound components
to the threaded/mixed experiment only when they have no render callback. Their
simulation and component state remain producer-owned; this does not move physics
or sound onto the render worker. Physics debug drawing is rejected at startup
and runtime debug toggling is disabled in threaded mode. Collection proxies,
labels and other unsupported renderers remain excluded.

`engine.poc_replay_hz=N` requires `replay.enabled=1` and N from 1 to 1000; zero is
the default and retains normal simulation. It runs exactly one 1/N-second engine
simulation step per update, regardless of the measured wall-clock interval.
Pacing still uses real time. This makes physics, timers and animations repeatable
for the selected same-target replay; it is not a normal gameplay timestep policy
or a guarantee of cross-platform physics determinism. Uncapped replay intentionally
advances game time faster than wall time. The runner requires activation evidence.

The first integration is an isolated offline copy of Underwatermelon. It retains
fruit gameplay, physics, GUI, particles and sound, with an explicit recorded
input sequence. Its optional streamed-music loader/status label are excluded.
The project adaptation, source provenance, renderer restrictions and measurement
limits are documented in the benchmark project. The original game is unchanged.

Energy is measured independently of CPU/wakeup counters. The runner can collect
estimated macOS CPU/GPU rail power through noninteractive `powermetrics`, or
import measured time-stamped intervals. Missing permission/data is reported as
unavailable, never zero. System rail estimates include other applications and
are not process-only power. This integration does not establish production
readiness, energy savings or physical input-to-display latency improvements.
