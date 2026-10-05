# Web component-threading PoC

[The production-candidate evaluation](WEB_PRODUCTION_CANDIDATE.md) adds longer
same-build tests, a deterministic offline gameplay replay and explicit acceptance
gates. Its desktop candidate fails the provisional memory budget and gameplay p99
gate; threading remains opt-in.

## Memory experiment

Sprite snapshots now use a 96-byte record (previously 128): a full 3D affine
transform without its invariant bottom row, optional slice-9 data in a side array,
and a material tag key captured once per shared binding. Bounds remain 16 bytes.
This layout applies to the inline, component-threaded and render-layer sprite
consumers. Both immutable frame slots and resource lifetime rules are preserved.
Bulk record/bound arrays reserve the active count before capture, rounded to 16
entries, with 12.5% headroom on subsequent growth. Capacity is reused, not shrunk
each frame. Existing slot and combined capacity admission limits still apply.

The default web worker stack remains 5 MiB. Experimental controls:

- `render.poc_web_stack_kb=2048` selects a 2 MiB simulation stack. Accepted range:
  256–16384 KiB. Extensions and projects with deeper native calls need their own
  validation; this is not a general production stack-size recommendation.
- `render.poc_web_stack_measure=1` paints unused worker stack at entry and scans
  it at shutdown. `Module.webStack` reports reserved bytes and deepest touched
  bytes. It excludes unwritten reservations and JavaScript stack usage. This
  diagnostic is off by default; initialization/finalization on main are excluded.
- `Module._dmEngineSampleWebMemory()` updates `Module.webAllocator` from
  `mallinfo`: allocated bytes (including worker stack), free allocator space, and
  arena bytes. WASM capacity, static data/main stack and GPU storage are separate.
  This allocator walk takes a lock and is intended for sparse diagnostics.
- CMake `DEFOLD_WEB_POC_MEMORY_GROWTH_LINEAR_STEP=4194304`, together with
  `DEFOLD_WEB_COMPONENT_POC=ON`, tests 4 MiB linear growth. Empty (default) retains
  Emscripten's growth policy. Smaller capacity can trade against more growth work.

`sprite._get_snapshot_stats().slots_payload_used_bytes` reports both slots' used
payload at capture, before retirement. `payload_used_bytes` retains its original
single-slot meaning. Frame capacity includes lookup tables; retained resource
sizes represent shared references, not duplicated textures. Summed slot growth
peaks are a conservative admission figure, not an observed process-memory peak.

See [the memory benchmark workflow](../../scripts/web/benchmark/README.md#memory-comparison).
The [2026-10-03 comparison report](benchmarks/web-memory-2026-10-03/REPORT.md)
contains the measured live memory, WASM capacity, throughput and p99 results,
including default-stack controls and the opt-in smaller-stack/growth experiments.

## Ownership

The original sprite-only path runs simulation, game Lua, render-script command capture,
and sprite extraction on one pthread. The browser main thread consumes immutable
sprite snapshots: culling, batching, geometry generation, and WebGL calls all
remain on main. It does not transfer the canvas to an OffscreenCanvas worker.

Initialization and finalization run on main. Input is sampled on main while the
simulation worker is idle, then handed over with the next tick. Window events use
a bounded queue. The existing two-slot component queue is pumped without blocking
main; simulation may overlap consumption of the previous snapshot. Resource
creation/upload/deletion use a synchronous main-owner lane: only the simulation
worker waits, and upload payloads are owned. This reuses graphics ownership
wrappers, but does not record draw calls into graphics packets.

## Opt-in web scheduling experiment (2026-10-03)

The overlapping sprite path now has a completion-dispatch experiment:

```ini
[render]
poc_pipeline = component
poc_threaded = 1
poc_web_schedule = 1
```

The default (`poc_web_schedule=0`) only dispatches updates from browser animation
callbacks. With `1`, worker completion also queues a nonblocking main-thread
notification. Main samples input and starts another update only if the worker is
idle and the document is visible. Snapshot consumption still runs on animation
callbacks; the existing two-slot queue supplies backpressure. This avoids waiting
for another animation callback when a worker finishes just after the previous
callback checked its state. It does not move WebGL off browser main. Render/update
pause falls back to animation-callback dispatch, and queued notifications check
for shutdown before accessing state. The experiment requires `wasm_pthread-web`
and is rejected by broad serialized mode. It is also available with the opt-in
owned component frames below; it is not a new default.

Two independently selectable follow-up experiments keep the same two-slot queue:

- `poc_web_cache_window=1` captures OPENED with the main-owned input/window
  snapshot before waking the worker. The worker's per-update window-open query
  then avoids the synchronous graphics lane. The default is `0` for comparison.
  Queued close events still run before simulation. Owner writes happen only while
  the worker is idle, and the tick publication makes the snapshot visible.
- `poc_web_schedule=2` retries dispatch after consuming a snapshot, **only if no
  update was dispatched at the beginning of that browser callback**. It catches
  a worker finishing during rendering without completion-driven run-ahead.
  This is a separate scheduling alternative to `1`, not an adaptive policy.
  Like `1`, it requires an overlapping component path; broad serialized mode remains rejected.

For example, use `poc_web_cache_window=1` with schedule `0`, `1`, or `2` to isolate
the cache and scheduling effects. All new options remain opt-in.

The [window-cache and retry evaluation](benchmarks/web-threading-improvements-2026-10-03/REPORT.md)
compares both changes independently, including dispatch-to-submit age, throughput,
p99 and memory. This metric includes the pre-simulation window wait omitted from
the original simulation-start age. It still does not measure physical input latency.

For bounded diagnostic traces, also set:

```ini
[render]
poc_web_metrics = 1
sprite_trace = /trace.csv
```

After orderly shutdown, `Module.webCpuTrace` contains the CPU CSV and
`Module.webSchedule` contains separate main/worker schedule samples. Schedule
samples are `[kind, phase, begin_ms, end_ms]`: kind 0 is an animation callback
(including dispatch and pumping), 1 is input preparation/dispatch, 2 is one worker
EngineUpdate, and 3 is a synchronous graphics-owner roundtrip on the worker
(including wait and owner execution). Filter worker kind 2 when computing update
durations: kind 3 spans are nested inside updates. `Module.webSchedule.retries`
counts successful post-consumption dispatches over the entire run, including
warm-up. Phase values are WAITING=0, TICK=1, RUNNING=2, DONE=3. On this pthread
build, timestamps share Emscripten's time-origin-adjusted clock; CSV uses
microseconds, scheduler samples milliseconds. Export happens after joining the
worker; there is no concurrent traversal of the buffers. A 65,536-record frame
trace and two 32,768-record scheduler buffers add diagnostic memory; overflow is
reported, not overwritten. Disable metrics when measuring normal memory capacity.

`scripts/web/benchmark` prepares web copies of the existing native sprite
workloads and Bunnymark, runs the four-mode comparison in visible Chrome, and
produces graphs and a report. The deterministic sprite regression now also checks
completion dispatch and context loss. See the [evaluation report](benchmarks/web-threading-2026-10-03/REPORT.md)
for measured results and remaining platform/latency/energy limitations.

## Broad built-in component compatibility (2026-10-03)

For testing projects containing built-in 2D and 3D components, use the PoC **Release** engine with:

```ini
[render]
poc_pipeline = component
poc_threaded = 1
poc_web_components = 1

[sound]
use_thread = 0
```

Bundle `wasm_pthread-web` and serve with COOP/COEP as described below. Keep your
project's normal render script, sound instance limits and component resources.
Do not set `sound.max_sound_instances=0` if the project uses sounds. The old
`mixed_preparation` and `poc_gameplay_components` flags are unnecessary here.

This admits built-in sprites, GUI (including text and GUI particles), particle
FX, labels, tilemaps, cameras, collision objects, sounds, scripts, factories,
collection factories and collection proxies. Built-in light components are also
admitted, along with mesh and model components. Unknown extension component types
remain rejected. The debug engine's profiler render hooks are still outside this PoC;
use Release. Native extension render hooks and render context listeners are
rejected; arbitrary extension code and browser/DOM calls from game Lua still
require an ownership audit. Reboot and context restoration remain unsupported.

**With `poc_web_overlap=0` (the default), this remains a serialized compatibility path.**
Add `poc_web_overlap=1` for the owned-frame path described below.
Simulation, component updates, game Lua and render Lua run on the pthread.
Component rendering and render-command execution run on browser main through
synchronous handoffs. The worker waits while main accesses live component worlds,
so resources, render scratch, cameras, text and geometry cannot be mutated
concurrently by simulation. Main never waits for the worker. Render Lua retains
its command operands until main finishes consumption, including multi-pass views,
constants and render targets; this path does not use the sprite-only command
capture subset. Audio mixing runs on the simulation worker, and Web Audio device
operations proxy to browser main. Window queries/mutations similarly use main.

The console and `sprite._get_snapshot_stats().mode` identify threaded execution as
`component-web-serialized`. Setting `poc_threaded=0` with `poc_web_components=1` uses
ordinary component rendering on main (reported as `existing`); this is a useful
compatibility control, not an immutable-snapshot inline comparison. Set
`poc_web_components=0` to restore the original overlapping sprite snapshot path and its
existing benchmark semantics. The flag defaults to zero. `poc_web_2d` remains
a compatible alias, including mesh/model support. When both are specified,
`poc_web_components` takes precedence, including an explicit zero. The previous
`component-web-2d-serialized` diagnostic is now named `component-web-serialized`.

Mesh and model ownership follows the same exclusive handoff. Rig evaluation,
animation callbacks, bone game objects and mesh buffer edits remain on the worker.
The model pose-texture upload and graphics-resource creation/deletion use the
synchronous browser-main lane; vertex uploads own their queued bytes. Every
external consumption handoff flushes pending uploads first, so local-space meshes
cannot consume queued stale buffers. Local-space mesh draws also recheck the
buffer version, covering Lua buffer resizing after the component update; this
fix applies to direct rendering as well. Model/mesh render callbacks, culling, batching,
skinning geometry, instance buffers and draw submission run on main while the
worker waits. Component worlds and resources therefore remain alive for the
whole consumption call, including collections loaded through proxies. These
components remain rejected by the overlapping sprite snapshot path and the
separate render-layer PoC. For model/mesh overlap, use the broad owned-frame path below.

The tradeoff is explicit: broader project coverage without simulation/render
CPU overlap during the handoffs. Do not attribute sprite-fast-path performance
results to this serialized mode. The owned-frame mode below is a separate opt-in
implementation and needs its own performance evaluation.

The regression fixture retains its original directory name,
`engine/engine/src/test/web_component_2d`, and exercises the
stock multi-pass render script, a camera, label/tile changes, GUI text/boxes and
particles, ordinary particles, dynamic physics, sprite factories, collection
factories, async proxy load/init/enable/disable/final/unload, input and sound
completion. It also covers runtime GUI texture creation/deletion and resizing.
The fixture now also includes local/world-space dynamic meshes, a static model,
CPU-skinned, GPU-skinned and instanced animated models. It replaces a shared mesh
buffer (growing and shrinking it), changes constants, disables/re-enables a model,
checks animated bone movement and animation completion, and destroys animated
models/meshes through both a factory and a collection proxy. Colored pixel probes
verify each geometry path is visible, including a displaced animated pose, rather
than accepting identical blank output. A browser draw probe verifies at least
two animated models share an instanced draw. WebGL errors fail the test.

The runner checks frozen pixels against direct, non-threaded and legacy-flag controls,
render/update pause/resume, clean exit and forced context loss. Network streaming,
third-party extensions and every individual component feature are not validated
by this small fixture.

To build that fixture, follow the content/build steps below but substitute
`web_component_2d` for the fixture source and use a separate content directory.
Set `DEFOLD_WEB_POC_CONTENT` to that directory's `build/default`. After configuring,
copy its `index.html` into the engine output directory (the CMake default template
is for the sprite-only fixture). Then serve the output and run:

```sh
CHROME_EXECUTABLE='/Applications/Google Chrome.app/Contents/MacOS/Google Chrome' \
  node scripts/web/test_component_2d.cjs http://127.0.0.1:8766/ tmp/web2d-check
```

As with the original test, use a new output directory and provide Playwright and
`pngjs` via Node's module path. This is a correctness test using software WebGL, not a
performance measurement. Audio completion is checked; audible output quality is
not assessed by the headless test.

Model/mesh validation: the extended fixture passes in threaded, direct,
non-threaded and legacy-flag modes, with the same final PNG SHA-256
`4f47eb7ba25919ee319982a9c0a801e33ff410d51bf98a0cd3775b1315e1be00`.
All seven geometry color probes and the displaced CPU/GPU/instanced pose probes
pass, with no WebGL errors. Animation callbacks, model disable/enable, dynamic
buffer growth/shrink, factory/proxy cleanup, input/audio, pause/resume and
context-loss shutdown pass. The original overlapping sprite fixture passes six
control/scheduling/context-loss cases. Both fixtures were checked with the opt-in
2 MiB worker stack. Native checks pass: 65 graphics tests, 17 model tests, the
24-test mesh filter (partly overlapping the model tests), and two web-admission
tests. [Saved correctness evidence](validation/web-components-2026-10-03/results.json)
and [sprite regression evidence](validation/web-components-2026-10-03/sprite-results.json)
are included. This is Chrome software-WebGL correctness evidence; no model/mesh
performance or energy benchmarks were run.

Original 2D-only validation (before adding models/meshes): all three fixture modes produced the same PNG
SHA-256 `c8734caec84ec202b82ce887da353a360be04c60c951d138e1a6a4128df7ab83`.
Threaded input and sound completion, render/update pause/resume, and context-loss
shutdown passed in headless Chrome. Local evidence is in
`tmp/web2d-smoke-complete/results.json`. The native admission regression passes
16 assertions; the existing 85 render and 64 graphics tests also pass. No new
performance claims or full benchmark runs accompany this compatibility change.
The original sprite-only regression also passes in direct, inline and threaded
modes with identical pixels and simulation checksums, including context-loss
shutdown (`tmp/web2d-sprite-regression/results.json`).

## Overlapping model/mesh and mixed component frames (2026-10-03)

Enable the new path in a Release `wasm_pthread-web` build:

```ini
[render]
poc_pipeline = component
poc_threaded = 1
poc_web_components = 1
poc_web_overlap = 1
poc_web_prepare_overlap = 1

[sound]
use_thread = 0
```

The console and `sprite._get_snapshot_stats().mode` report
`component-web-snapshots`. Setting only `poc_web_overlap=0` restores the serialized
control. Setting both `poc_web_overlap=0` and `poc_threaded=0` restores direct
main-thread execution. The existing sprite-only path is unchanged.
`poc_web_prepare_overlap` defaults to 1 within the opt-in owned-frame path.
Setting it to 0 retains a preparation barrier for same-build comparisons: simulation
still overlaps, but preparation waits for the previous consumer. This control also
uses the new owned texture uploads; it is not the historical engine binary.

The worker can now advance simulation **while browser main draws the previous
frame**, including scenes containing models and meshes. Two reusable frame slots
own finalized render objects, transforms, copied component/render-script constants,
camera matrices, texture bindings, compacted light data, pass commands, and vertex
and index upload bytes, plus owned 2D texture upload bytes. No consumer callback accesses a live component pool, rig,
Lua state, text layout, camera registry, or producer render context. Main uses a
separate render context for draw state and light-buffer scratch.

| Stage | Owner and synchronization |
| --- | --- |
| Simulation, animation, callbacks, physics, game Lua | Worker; may overlap previous frame consumption |
| Preparation: component rendering, culling, batching, CPU skinning, geometry, render Lua | Worker; may overlap previous frame consumption |
| Resource creation, replacement and destruction | Browser main via the synchronized owner lane; may require draining |
| Pass execution, owned vertex/index/pose-texture uploads, WebGL draws | Browser main; consumes the published frame |

This is a prepared component-output snapshot. **Simulation and preparation can
both overlap the previous consumer.** Culling, batching and geometry stay on the
worker; models do not use the original sprite-only extraction/consumption split.
Upload recording is thread-local. Bone-texture bytes and buffer data are copied
into the building slot and uploaded only during its consumption. Preparation uses
producer-owned buffer-size and viewport metadata and the captured pose dimensions,
plus producer-owned font-atlas dimensions, so it does not read state being changed
by the previous consumer. Per-pass upload
boundaries preserve reuse of buffers and textures across draws with different data.
Publication still waits for the previous frame to retire, bounding the queue to
one building slot and one consuming slot.

GPU/material handles are protected by drain-before-mutation barriers, rather than
by duplicating all resources. Creation, replacement and destruction during the
next simulation may therefore shorten overlap. Destructive resource changes
**between already captured passes** reject the unpublished frame; they cannot
silently replay stale handles. Ordinary full and partial 2D texture uploads are
owned and ordered with passes, including model poses and built-in font atlases.
Array, cube and 3D uploads during capture are rejected. Custom extension
drawing/graphics calls remain outside the supported scope.
Compute commands are rejected, and the earlier reboot/context-restoration and
extension restrictions still apply. Normal stock multipass rendering is covered.

Each slot is bounded to 64 draw passes, 256 pass commands, 16,384 render objects,
4,096 texture bindings and 4,096 light records across passes, 4 MiB of retained
constant storage, and 16 MiB of upload payload with at most 4,096 upload commands.
Admission failures stop the PoC with a diagnostic. Allocations grow on demand and
are reused. `component_frame_capacity_bytes` reports retained storage for both
slots; `component_frame_used_bytes` reports occupied arrays/upload bytes plus
retained constant storage. These are CPU frame storage metrics, not total browser
or GPU memory.

The main-owned OPENED window snapshot is automatically enabled for this opt-in
path; otherwise that query would synchronize before simulation and defeat overlap.
Scheduling remains `poc_web_schedule=0` unless explicitly changed.

`sprite._get_snapshot_stats().thread_simulations_during_render` counts complete
simulation-update intervals that start and finish while a consumer callback is
actively executing. `thread_simulation_overlap_us` sums those intervals. Queued
or already-retired frames do not count. This deliberately undercounts partial
overlap and is an execution diagnostic, not a speedup measurement.
`thread_preparations_during_render` and `thread_preparation_overlap_us` apply the
same strict test to the preparation span. `component_preparation_overlap` reports
the selected preparation control. Zero complete spans does not exclude partial overlap.

Current validation is in
[the preparation overlap record](validation/web-preparation-overlap-full-2026-10-03/README.md).
The earlier simulation-only milestone is preserved in
[the model/mesh overlap validation record](validation/web-model-mesh-overlap-2026-10-03/README.md).
The mixed fixture compares animated and final pixels against serialized/direct
controls, checks CPU/GPU skinning and instancing, buffer growth/shrink, destruction,
pause/resume and context loss. A separate correctness case inserts a documented
2 ms main-thread draw delay to make the overlap assertion deterministic. Neither
case is a throughput or memory benchmark. Hardware-browser performance and memory
results are reported separately in [the preparation benchmark](WEB_PREPARATION_OVERLAP_RESULTS.md).

## Sprite-only scope and controls

The first milestone supports preloaded sprite/script/factory content, basic input,
render pause/resume, sprite creation/deletion, and orderly shutdown. It requires
WebGL2 and a pthread-capable build. Mixed GUI/particle preparation, gameplay
component admission, sound instances, native render-extension hooks, and native
trace/delay options are rejected. Arbitrary resource-job producers, streaming,
general browser/DOM APIs from Lua, and reboot are outside this milestone. It is
not yet a general-purpose web export for existing games.

Hidden tabs stop receiving simulation ticks and skip draws; a timer retires
accepted work. WebGL context loss stops the run with an explicit failure rather
than attempting restoration. Browser main never waits on a worker join or a frame
condition variable. The simulation worker reserves a 5 MiB stack; this is separate
from snapshot/upload storage and browser/driver memory.

The fixture selects these modes in the same pthread binary:

| URL | Configuration | Execution |
| --- | --- | --- |
| `?mode=direct` | `render.poc_pipeline=legacy`, `render.poc_threaded=0` | Existing flow on main |
| `?mode=inline` | `render.poc_pipeline=component`, `render.poc_threaded=0` | Snapshot extraction/consumption on main |
| `?mode=threaded` | `render.poc_pipeline=component`, `render.poc_threaded=1` | Simulation pthread, consumption on main |

These controls share the instrumented PoC binary; direct is not an untouched
vanilla-engine build. Ordinary non-pthread exports remain a separate build.
Scheduling follows browser animation frames, so this harness does not measure
uncapped CPU throughput like the native benchmark runner.

## Reproduction

Run from the repository root with the normal Defold SDK dependencies installed.
Use Emscripten **4.0.6** and export `EMSDK` to its SDK directory for both configure
and build. This checkout used `tmp/web-poc-emsdk` because its original SDK symlink
was stale; the original symlink was left unchanged.

Build a disposable copy of the fixture using the full Bob jar (bob-light does not
include the built-in content):

```sh
mkdir -p tmp/web-poc-project
cp -R engine/engine/src/test/web_component_poc/. tmp/web-poc-project/
DYNAMO_HOME="$PWD/tmp/dynamo_home" java -jar tmp/dynamo_home/share/java/bob.jar \
  --root "$PWD/tmp/web-poc-project" --output build/default \
  --platform wasm-web --variant release --archive build
```

If the SDK lacks pthread versions of the external libraries, build them first:

```sh
export EMSDK="$PWD/tmp/web-poc-emsdk"
cmake -S external -B tmp/web-poc-ext -G Ninja \
  -DTARGET_PLATFORM=wasm_pthread-web -DBUILD_TESTS=OFF \
  -DDEFOLD_SDK_ROOT="$PWD/tmp/dynamo_home" \
  -DDEFOLD_BUILD_HOME="$PWD/tmp/web-poc-ext-output"
cmake --build tmp/web-poc-ext --target install -j6
```

Configure and build the engine:

```sh
cmake -S . -B engine/build/wasm-pthread-component-poc -G Ninja \
  -DTARGET_PLATFORM=wasm_pthread-web -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -DDEFOLD_SDK_ROOT="$PWD/tmp/dynamo_home" \
  -DDEFOLD_BUILD_HOME="$PWD/tmp/web-component-poc-build" \
  -DBUILD_TESTS=ON -DDEFOLD_SKIP_BOB_LIGHT=ON \
  -DDEFOLD_WEB_COMPONENT_POC=ON \
  -DDEFOLD_WEB_POC_CONTENT="$PWD/tmp/web-poc-project/build/default"
cmake --build engine/build/wasm-pthread-component-poc --target dmengine_release -j8
python3 scripts/web/serve_component_poc.py \
  tmp/web-component-poc-build/engine/engine/build/wasm_pthread-web
```

The server binds localhost and supplies COOP/COEP headers required for shared
memory. The build prewarms one pthread and disallows blocking on browser main.
Open `http://127.0.0.1:8765/?mode=threaded`, or run the headless correctness runner
with Playwright available to Node (via installation or `NODE_PATH`):

```sh
CHROME_EXECUTABLE='/Applications/Google Chrome.app/Contents/MacOS/Google Chrome' \
  node scripts/web/test_component_poc.cjs http://127.0.0.1:8765/ tmp/web-poc-check
```

Use a new output directory each time. The runner writes JSON, logs, and PNGs.
It uses software WebGL for reproducible correctness checks, not performance tests.

## Validation, 2026-10-02

The fixture runs 200 fixed simulation ticks with 256 sprites, deletes half and
recreates them, receives an input event, and captures a frozen visual checkpoint.
Direct, inline, and threaded modes produced checksum `82980080` and identical PNG
hashes. The threaded case also passed render pause/resume. Forced WebGL context
loss returned failure code 1 with clean shutdown and no JavaScript exceptions.

Native focused suites passed: 85 render tests, 64 graphics tests, and 2 condition
variable tests. New tests cover external-owner frame pumping, resource dispatch,
and waking all condition-variable waiters. The web implementation also fixes a
previously stubbed pthread condition-variable broadcast and preserves an optional
adapter swap-interval hook instead of invoking a null callback.

The browser run used headless Chrome 154 on macOS; final local artifacts are in
`tmp/web-poc-smoke-final/`. Full performance/memory/energy benchmarks were not run.
Firefox, Safari, hardware WebGL, hidden-tab lifecycle automation, and real-game
coverage remain to be validated before drawing performance or portability
conclusions.

## Opt-in model upload ownership experiment

`render.poc_web_owned_model_buffers=1` lends storage from the building component
frame to model batching, then returns it to that frame before recording the
upload. Generated world-space/CPU-skinned vertex data and model instance data are
written into reusable frame-owned arrays. The producer's corresponding scratch
arrays are left empty. Copied capture remains the default (`0`), and mesh uploads,
pose textures and other component types retain their existing paths.

Each render-list dispatch gets distinct storage, preserving multiple render
passes and uploads to a shared graphics buffer. Only a retired slot is reused;
the consumer never observes producer-writable arrays. This does not add a queue
slot, reduce the worker stack, or alter resource mutation barriers. The existing
payload/capacity limits still reject an oversized frame. Slot-owned array
metadata and retained capacities are included in component frame accounting;
`component_owned_upload_bytes` reports the last prepared frame's upload bytes
that avoided the capture copy. It does not claim all renderer copies are removed.

Native tests cover retired-slot reuse, independent frames, upload ordering,
default fallback and capacity rejection. Web tests exercise animated CPU/GPU and
instanced models, resource replacement, matching pixels, and preparation while a
consumer is deliberately stalled. See the benchmark runner's model-buffer/GC
workflow for separate allocation-attribution and timing runs.

## Opt-in sprite placement and paced completion experiments

The broad overlapping component path now has two independent experiment switches:

```ini
[render]
poc_pipeline = component
poc_threaded = 1
poc_web_components = 1
poc_web_overlap = 1
poc_web_prepare_overlap = 1
poc_web_deferred_sprites = 1
poc_web_schedule = 3
```

`poc_web_deferred_sprites` defaults to `0`. At `1`, the worker extracts compact
immutable sprite snapshots, culls, globally sorts, and selects batches together
with other component entries. Each sprite batch records its selected snapshot
indices at the original draw position. Browser main generates sprite geometry
and uploads it when consuming that pass. Other components retain their existing
preparation paths. This moves geometry work only: culling, sorting and batching
have not moved to main. Render Lua remains on the worker.

The callback reads frame-owned data and writes render-owner scratch, never live
sprite components. The reserved two-slot lifecycle protects snapshots; resource
mutation barriers protect retained resources. Sprite-world deletion drains
published work before freeing renderer scratch. Deferred descriptor/index pools
are reused and count toward sprite frame budgets; the component frame also
accounts for its deferred draw registry. Capacity failure rejects the frame.
Each render pass retains its ordering, matrices, constants and graphics uploads.

The existing sprite diagnostic renderer-scratch counters are not refreshed by
this deferred path; their zero values must not be interpreted as zero scratch
usage. Use the WASM allocator totals for memory comparisons. Deferred payload
and descriptor capacities remain budgeted and counted.

`poc_web_schedule=3` grants one admission credit per visible browser animation
callback. A busy worker can consume an unused credit through its completion
callback, avoiding a whole extra rAF wait. A fast worker must wait for a new
credit, so completion callbacks cannot run simulation ahead of the browser.
Credits do not accumulate; hidden tabs lose credit, and shutdown can wake the
worker without credit. This limits dispatch to browser opportunities, not a
fixed 60 or 120 Hz. It does not guarantee uniform update intervals or input
latency. Mode `1` retains unrestricted completion dispatch; `0` remains the
default rAF scheduler, and `2` retains its post-consumption retry behavior.

`poc_web_schedule=4` is a separate readiness-consumption experiment. It keeps
mode 1's completion-driven simulation dispatch, but gives rendering one credit
per visible browser animation callback. If that callback finds no ready frame,
the worker's completion notification may consume a frame using the unused
credit. Consumption spends the credit; credits never accumulate. Both paths
wake the next simulation update before consuming, preserving overlap. The
frame queue still has two slots. Hidden-tab retirement and shutdown may drain
accepted work without a visible presentation credit. This is opt-in and does
not change the default scheduler. After shutdown, `Module.webRenderAdmission`
reports `readyRenders`, `retired`, and `browserTicks` for checking the bound.
See the [space game replay comparison](SPACE_GAME_WEB_REPLAY_RESULTS.md) for the
vanilla control, correctness checks, memory measurements and scheduling results.

Select the switches independently when evaluating: geometry only uses
`poc_web_deferred_sprites=1, poc_web_schedule=1`; pacing only uses `0, 3`;
both use `1, 3`. The separate model-buffer ownership experiment remains off
unless explicitly enabled. A Release `wasm_pthread-web` build and cross-origin
isolation are required. Console activation includes
`WEB_COMPONENT_SPRITES deferred_geometry=1` and `WEB_POC_OPTIONS schedule=3`.

See [sprite placement and pacing results](WEB_SPRITE_PLACEMENT_RESULTS.md) for the measured comparison and recommended experimental configuration.

### Per-update diagnostics and aggregate memory accounting

`render.poc_web_diagnostics=1` adds a bounded, opt-in diagnostic record for each
admitted worker update. Browser main records dispatch and admission source; the
worker records wake-up, event handling and completion. Synchronous graphics-owner
calls contribute separate queue, owner execution and worker-resumption spans.
Records are exported as `Module.webUpdateDiagnostics` only after the worker joins.
The benchmark contract requires sequential IDs, consistent timestamps and no lost
records. No diagnostic buffer is allocated when the switch is off.

Deferred sprite scratch accounting is now sampled on the render owner at completed
consumption and read only when that frame slot is retired. It no longer reports
zero merely because the producer cannot safely inspect live render scratch.
Aggregates include every registered sprite world, including collection proxies;
world deletion removes it after existing consumption barriers. Component frame
upload and constant capacities are exposed as subsets of total frame capacity.
These are CPU-capacity counters; logical GPU sizes are reported separately.
