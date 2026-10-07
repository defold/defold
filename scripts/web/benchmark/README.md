# Web component scheduling benchmark

This ports the existing native sprite workloads and Bunnymark into a disposable
project. It does not modify the original projects. GUI is removed from the native
controller object. Both scenes share the native project’s 60,000 sprite/object
capacity and graphics settings; Bunnymark assets and animations are reused, but
this is not its original standalone bundle configuration. All modes share
these settings. The optional geometry workload below also evaluates broad owned
component frames with model and mesh rendering.

From the repository root, with the PoC SDK and Emscripten 4.0.6 installed:

```sh
python3 scripts/web/benchmark/prepare.py \
  /path/to/experiment-decoupled-rendering /path/to/sprite_bunnymark \
  "$PWD/tmp/web-benchmark-project"
DYNAMO_HOME="$PWD/tmp/dynamo_home" java -jar tmp/dynamo_home/share/java/bob.jar \
  --root "$PWD/tmp/web-benchmark-project" --output build/default \
  --platform wasm-web --variant release --archive build
```

Follow the pthread CMake setup in `engine/docs/WEB_COMPONENT_POC.md`, substituting
this project's `build/default` for `DEFOLD_WEB_POC_CONTENT`. Build
`dmengine_release`, then copy `index.html` from this directory into the engine's
web output directory. Serve it on localhost:8766 with
`scripts/web/serve_component_poc.py` (COOP/COEP are required).

```sh
NODE_PATH=/path/to/node_modules node scripts/web/benchmark/run.cjs tmp/new-results
python3 scripts/web/benchmark/report.py tmp/new-results \
  engine/docs/benchmarks/web-threading-2026-10-03
```

The collector requires Playwright and local Chrome. It currently validates the
Apple M1 Pro GPU used for this experiment and AC power using macOS `pmset`; adapt
these explicit platform checks when porting to another machine. It launches a
visible Chrome window, not the user's existing browser profile, and checks focus
and visibility at each sample. The report requires matplotlib and numpy.

Optional environment settings: `REPEATS`, `SECONDS`, `WARMUP`, `METRICS=0`,
`CHROME_EXECUTABLE`, and `HEADLESS=1` for correctness pilots only. An optional final
argument selects one case (`bunny30k`, for example). Both output directory and
per-run files are new; runs are never silently overwritten.

If a collection stops (for example, a failed focus check), use `RESUME=1` with
the same output directory and identical case/mode/timing options. Registered
successful runs are kept. Failed attempts move into `failed/` before retrying;
their JSON and any trace remain part of the evidence. A configuration mismatch
or an unregistered successful run requires explicit reconciliation.

For the report's memory/overhead control, run the same `bunny30k` case with
`METRICS=0`, then pass `--control CONTROL_DIRECTORY` to `report.py`. Main matrix
memory includes diagnostic buffers. CPU percentages describe utilization, not
power; submit timestamps are neither GPU completion nor display presentation.
The collection intentionally does not claim physical input latency or energy.

## Broad component preparation overlap

Prepare with `--memory-probe`, then run `add_geometry.py PROJECT_DIRECTORY` before
building content to add the model/mesh workloads. Select them explicitly through
`CASES=geometry200,geometry1000`; the original default workload list is unchanged.
Each group contains two meshes and five models (four animated). All groups are
tiled within the viewport and use shared, small triangle assets.

`MODES=direct,serialized,barrier,overlap` compares main-thread direct rendering,
exclusive worker/main handoffs, owned frames with a preparation barrier, and owned
frames with full preparation overlap. These use one Release build. The barrier
control includes the new texture-upload capture, so it is not an older binary.
Use `METRICS=0 MEMORY_PROBE=1` for the performance/memory comparison and
`preparation_report.py RESULTS ARTIFACTS` to produce charts, CSV, tables and a raw
archive. The [full report](../../../engine/docs/WEB_PREPARATION_OVERLAP_RESULTS.md)
documents configurations and interpretation.

Visible runs activate their isolated Chrome **process** through macOS AppKit;
activating the Chrome bundle by name can select a different user profile.
Focus emulation remains disabled and failed focus checks invalidate a run.

Configuration knobs in the engine:

- `render.poc_web_schedule=0`: current animation-callback dispatch (default).
- `render.poc_web_schedule=1`: opt-in completion dispatch for the overlapping sprite or owned-component paths.
- `render.poc_web_schedule=2`: retry after consumption if this callback has not
  already dispatched; available for the same overlapping paths.
- `render.poc_web_cache_window=1`: snapshot window-open state at dispatch to avoid
  the worker's synchronous query; defaults to 0 for sprite-path comparisons and
  is automatically enabled for broad owned component frames.
- `render.poc_web_metrics=1` with `render.sprite_trace=/trace.csv`: bounded CPU and
  scheduler traces, exported to `Module.webCpuTrace` / `Module.webSchedule` after
  shutdown. The runner rejects trace overflow.

For the focused cache/retry comparison, select a new output directory:

```sh
MODES=direct,threaded,scheduled,cached_raf,cached_completion,cached_retry \
CASES=bunny10k,bunny30k,balanced,render50k,fill \
  NODE_PATH=/path/to/node_modules node scripts/web/benchmark/run.cjs tmp/new-improvements
python3 scripts/web/benchmark/improvements_report.py tmp/new-improvements \
  engine/docs/benchmarks/web-threading-improvements-2026-10-03
```

`MODES` and `CASES` are comma-separated selections. `cached_*` modes enable the
window cache; `cached_retry` selects schedule 2. A `METRICS=0` 30k collection can
be supplied to the report with `--control`. The shared `analysis.py` filters
worker update records (kind 2) separately from nested graphics roundtrips (kind 3).
The runner now disables Playwright focus emulation after navigation. Visibility
is still sampled, not an independent OS-level occlusion measurement.

Correctness: `test_component_poc.cjs` covers all schedulers with identical pixels,
input, pause/resume and context-loss shutdown. Optional `HIDDEN_CHECKS=1` injects
`document.hidden` and pauses animation callbacks for the context-loss case; this
tests hidden-service control flow, not real browser tab visibility. Use
`POC_TEST_MODES=cached-raf,cached-completion,cached-retry,cached-retry-context-loss`
for those focused checks.

## Memory comparison

Pass `--memory-probe` to `prepare.py` to add a bounded update-interval histogram
and snapshot samples every two seconds. Run with `MEMORY_PROBE=1 METRICS=0` to
collect these and allocator `mallinfo` samples without the large CPU trace buffers.
The interval p99 is an upper bound rounded to 0.05 ms; it is a simulation-update
interval, not a render-submission or presentation interval.

`STACK_KB` selects the web simulation stack (default 5120); `STACK_MEASURE=1`
enables an opt-in unused-stack watermark. Its shutdown result reports deepest
observed stack writes, not unwritten stack reservations or JavaScript stack use.
Use watermark runs for correctness/stack sizing and leave it off for performance.
The smaller stack is experimental, especially for projects with native extensions.

For an interleaved build comparison, `VARIANTS_FILE` accepts a JSON object:

```json
{
  "before": {"mode":"cached_completion", "url":"http://127.0.0.1:8767/before/", "stackKb":5120},
  "after": {"mode":"cached_completion", "url":"http://127.0.0.1:8767/after/", "stackKb":2048}
}
```

Serve the parent directory containing the preserved bundles. Variant order rotates
between repeats. Preserve artifact hashes in the variants object (extra fields
are retained in the manifest), and use identical `.data` archives for all builds.
Both the stack setting and variants are checked when resuming a collection.
Each variant can also specify an absolute `bundleDir`. When any does, every
variant must supply one; the runner hashes each bundle before collection and
checks it after every run. This supports comparisons against a separate vanilla
engine while keeping one identical project archive. `AUDIO_ACTIVE=1` permits
WebAudio autoplay and mutes browser output; it does not disable audio processing.

The opt-in `overlap_ready` mode selects `render.poc_web_schedule=4`: completion
dispatch plus at most one visible frame consumption per browser tick, with a
completion callback able to fill an unused render credit. It keeps two slots.
The runner checks activation and the exported consumption bound. The
[anonymous space game comparison](../../../engine/docs/SPACE_GAME_WEB_REPLAY_RESULTS.md)
used a private adapter with common replay/allocator probes for vanilla and PoC.

External project adapters and raw results can contain private source, paths,
dependencies, gameplay state and screenshots. Keep them under Git-ignored
`tmp/private-external-benchmarks/`. Publish only reviewed anonymous metrics and
plots; do not copy raw archives or force-add private evidence. Generic report
exporters may archive their inputs and must not be used to publish private game
collections without a separate sanitization review.

```sh
MEMORY_PROBE=1 METRICS=0 CASES=bunny30k,render50k \
VARIANTS_FILE=tmp/web-memory-variants.json \
  NODE_PATH=/path/to/node_modules node scripts/web/benchmark/run.cjs tmp/memory-results
python3 scripts/web/benchmark/memory_report.py tmp/memory-results \
  engine/docs/benchmarks/web-memory-results
```

The memory report separates peak **sampled live allocator bytes**, allocator free
space, WASM capacity, snapshot capacity, and logical GPU buffers. Snapshot bytes
and the worker stack are already part of allocator totals: do not add them again.
Retained resource descriptors are references to shared resources, not copies.
The samples do not prove the instantaneous allocation peak during startup. Sparse
allocator scans also have a cost, so these are measured comparisons with identical
instrumentation, not zero-overhead timing runs.


## Production-candidate evaluation

`candidate.json` freezes the proposed broad-component configuration and provisional
acceptance limits. `overlap_completion` selects broad owned frames with preparation
overlap and completion dispatch; `barrier_completion` keeps that scheduler while
serializing preparation. The existing `cached_raf` / `cached_completion` modes
remain the specialized sprite snapshot implementation. No engine default changes.

Use `CASES_FILE=cases.json` for project-specific cases. A replay case contains
`name`, `scene: "replay"`, `expected` (id/ticks/measure_ticks/events), optional
`url`, `timeout_seconds`, and the supported project `config` values. The runner
preserves project sound capacity, checks cleanup and exact gameplay signatures,
and rejects mismatches across modes/repeats. It requires the existing replay SDK
and a project-specific deterministic adapter; it cannot infer gameplay inputs.

Prepare a disposable copy of the previously instrumented offline game:

```sh
python3 scripts/web/benchmark/prepare_replay.py REPLAY_PROJECT NEW_PROJECT \
  --manifest REPLAY_PROJECT/replay/game.manifest.json --long-melon
```

`--long-melon` is specific to the reviewed offline Underwatermelon adapter. It uses
15,000 fixed 60 Hz simulation ticks, 600 warmup ticks, twelve drops spread over
250 simulated seconds, periodic movement, and 600-tick checkpoints. Wall duration
is measured, not assumed from the simulated duration. Physics, merging, GUI and
sound remain active; the optional streaming loader remains excluded. A sparse
long scenario is a compatibility/latency test, not a CPU-saturated game.
Without this flag, the adapter's original scenario is preserved. Build with Bob,
link/preload the new archive, and copy this directory's `index.html` into the bundle.

Freeze separate bundles before collecting, then use:

```sh
CASES=bunny30k,geometry1000 MODES=direct,overlap_completion \
  REPEATS=3 SECONDS=120 WARMUP=10 METRICS=0 MEMORY_PROBE=1 \
  BUNDLE_DIR=FROZEN_BUNDLE GATES_FILE=scripts/web/benchmark/candidate.json \
  NODE_PATH=/path/to/node_modules \
  python3 scripts/web/benchmark/awake_run.py --settings NEW_SETTINGS_FILE -- \
  node scripts/web/benchmark/run.cjs NEW_RESULTS
```

`awake_run.py` is macOS-specific and restores the original screen-saver timeout
on normal exit or collection failure. Its settings file is retained for recovery
if the process is forcibly killed. `run.cjs` currently uses macOS AC-power and
foreground activation checks. `GPU_MATCH` optionally narrows the hardware renderer;
software renderers are rejected. `DEVICE_ID` labels the physical test machine.
Headless runs are validation only and cannot enter the candidate report. The
runner hashes frozen bundle files and rejects changes or mismatched resume gates.
A different physical device needs its own collection; CPU throttling is not a
substitute for lower-powered hardware evidence.

For replay collection use `CASES_FILE=NEW_PROJECT/web-cases.json` instead of
`CASES`, setting the case URL if served on another port. `SECONDS` does not change
a fixed-tick replay. The report verifies actual measurement duration.

```sh
python3 scripts/web/benchmark/candidate_report.py NEW_REPORT_DIR \
  SYNTHETIC_RESULTS GAMEPLAY_RESULTS
python3 -m unittest discover -s scripts/web/benchmark -p 'test_candidate_report.py'
python3 -m unittest discover -s scripts/web/benchmark -p 'test_awake_run.py'
node --test scripts/web/benchmark/test_launch_config.cjs \
  scripts/web/benchmark/test_replay_contract.cjs
```

The report checks completeness, focus, display dimensions, replay equivalence,
repeat count, duration, throughput, p99 and live memory. It generates graphs, CSV,
raw archives and a machine-readable assessment. Production remains NOT_READY
while the external evidence listed in `candidate.json` is missing; local timing
passes do not manufacture vanilla, device, energy or physical-latency evidence.
Gameplay population growth is explicitly unproven as a steady-state leak test.

### Model-buffer ownership and GC attribution

`render.poc_web_owned_model_buffers=1` enables frame-owned generated model vertex
and instance buffers within broad overlapping component frames. It defaults to
zero. The benchmark page accepts `owned_models=1`; a runner variant selects it
with `"ownedModels": true`. Direct and copied controls use the same binary:

```json
{
  "direct": {"mode": "direct"},
  "copied": {"mode": "overlap_completion"},
  "owned": {"mode": "overlap_completion", "ownedModels": true}
}
```

Prepare a fresh geometry benchmark project with the current `memory_probe.lua`
and `geometry_benchmark.script`, build its content and engine, then freeze the
bundle. For the diagnostic, use `CASES=geometry1000 MEMORY_PROBE=1 METRICS=0
MEMORY_DIAGNOSTIC=1` and `VARIANTS_FILE` pointing at that JSON. The geometry script
collects twice on the Lua owner after warmup (before starting its measurement
clock) and after stopping its clock. `WEB_MEMORY_GC` records allocator and Lua
bytes before/after each collection. These are **diagnostic timings only** and
`candidate_report.py` rejects them. The private web `sprite._snapshot_memory()`
returns two numbers without allocating a snapshot table.

Repeat with `MEMORY_SAMPLES=0` to disable Lua snapshot tables/JSON logging while
retaining external allocator sampling and the fixed timing histogram. This
separates probe garbage from garbage generated by ordinary engine/script calls.
Then run normal repeats with `MEMORY_DIAGNOSTIC=0 MEMORY_SAMPLES=1` for performance.
Keep the stack size, scheduler, content and binary unchanged across modes.

```sh
python3 scripts/web/benchmark/owned_memory_report.py OUTPUT_DIRECTORY \
  PERFORMANCE_DIRECTORY GC_DIAGNOSTIC_DIRECTORY GC_WITHOUT_SERIALIZATION_DIRECTORY
```

This report expects the three variants above and one geometry case. It keeps GC
boundaries separate from performance, plots throughput/allocation/frame capacity,
and archives manifests, runner sources and raw measurements. Allocator bytes
include the worker stack and uncollected Lua garbage; a full collection need not
shrink WASM capacity. Short stable post-GC baselines are not a general leak-free
certification.

### Sprite placement and paced completion

Variant files can independently select the new experiments:

```json
{
  "direct": {"mode": "direct"},
  "current": {"mode": "overlap_completion"},
  "deferred": {"mode": "overlap_completion", "deferredSprites": true},
  "paced": {"mode": "overlap_paced"},
  "combined": {"mode": "overlap_paced", "deferredSprites": true}
}
```

The deferred variant keeps worker culling/sorting/batch selection but generates
sprite geometry on browser main at the original pass positions. The paced
variant uses scheduler 3 (one admission credit per visible rAF, available to
completion callbacks). Both are off by default; model upload ownership remains
an independent option. The runner validates the activation lines.

Use short `METRICS=1` runs only for CPU attribution, then `METRICS=0` repeated
foreground runs with `MEMORY_PROBE=1` for comparisons. The report generator takes
complete collections, checks Release/foreground/timing/replay evidence, preserves
raw files and produces throughput, p99, allocation and WASM-capacity graphs:

```sh
python3 scripts/web/benchmark/placement_report.py OUTPUT BUNNY_RUNS GEOMETRY_RUNS GAMEPLAY_RUNS --pilot DIAGNOSTIC_RUNS
```

The optional `--pilot` input adds a separate diagnostic wall-time-span graph and
internal input-to-submit timings. It never enters the performance means. Do not
mix instrumented or forced-GC diagnostics into those comparisons.

### Per-update scheduling diagnostics and smaller-stack validation

Use a newly prepared replay copy (the adapter now marks diagnostic output), then
`METRICS=1 DIAGNOSTICS=1 MEMORY_PROBE=1 STACK_MEASURE=1 REPEATS=1` for attribution.
`DIAGNOSTICS=1` enables `render.poc_web_diagnostics`; it is off by default. The
runner saves `Module.webUpdateDiagnostics` with sequential update IDs, admission
source, browser tick, dispatch/wake/event/update timestamps, and per-update sums
of synchronous graphics-owner queue, execution, and return time. It rejects
missing, overflowing, nonfinite, reordered, or impossible timing records.

All timestamps use the same Emscripten monotonic clock. `update_diagnostics.py`
matches engine trace frames to their containing update and partitions successive
worker wake intervals into previous worker duration, admission gap, dispatch,
and next wake-up delay. Graphics calls and publication waits are subsets of the
worker span. Render consumption overlaps it. These values are wall-time spans,
not additive CPU utilization or physical input/display latency. Other synchronous
JavaScript calls, such as extension or sound calls bypassing the graphics owner,
remain unattributed within the worker duration.

Use `METRICS=0 DIAGNOSTICS=0 STACK_MEASURE=0` for acceptance measurements. The
replay and report contracts reject instrumented diagnostic records as performance
evidence. The diagnostic buffer holds 16,384 updates, enough for the current
15,000-tick replay plus cleanup; longer scenarios need a larger buffer or bounded
sampling before running, not silently dropped records.

A variant may specify `stackKb: 2048` to compare with the unchanged 5120 KiB
default. Run the broad component fixture (including model/mesh, collection unload,
pause/resume and context loss) with `STACK_KB=2048 STACK_MEASURE=1` first. Stack
watermarks measure deepest writes in the exercised paths; they do not prove the
maximum possible stack requirement for arbitrary Lua/native extension code.

Snapshot diagnostics now expose `all_sprite_worlds`, aggregate sprite frame and
renderer scratch capacity across collections, and component upload/constant
capacity. Deferred sprite scratch is sampled by its render owner and read from a
retired slot, so it can lag up to two frames. `all_sprite_constant_capacity_bytes`
is a subset of sprite scratch; `component_upload_capacity_bytes` and
`component_constant_capacity_bytes` are subsets of component-frame capacity.
Never add those subsets twice. GPU logical buffers and retained resource sizes
are separate from CPU allocation; WASM capacity includes unused allocator space.

### Android Chrome over USB

Set `ADB_SERIAL` explicitly to select a connected, unlocked Android device. The
runner uses installed Chrome through a forwarded DevTools socket (default host
port 9223), starts a fresh Chrome process per run, and checks real page visibility
and focus. Existing Chrome data is retained; Chrome is force-stopped between runs.
Serve the bundles locally with COOP/COEP headers and use `adb reverse` to make the
server port reachable as device localhost. Keep the device awake on external
power, with Battery Saver disabled and orientation fixed; preserve and restore
its original settings after collection.

```sh
adb -s DEVICE_SERIAL reverse tcp:8771 tcp:8771
ADB=/path/to/adb ADB_SERIAL=DEVICE_SERIAL \
CASES_FILE=/path/to/private-cases.json VARIANTS_FILE=/path/to/private-variants.json \
METRICS=0 MEMORY_PROBE=1 REPEATS=3 \
  node scripts/web/benchmark/run.cjs tmp/private-external-benchmarks/android-results
```

During cooldown, the runner temporarily dims the screen and restores the original
brightness and automatic-brightness setting before launch, including on errors or
interrupts. Before each run, the runner waits for Android thermal status 0/1 (none/light) and at least 90% aggregate CPU idle while Chrome
is stopped. This is a sustained external-power protocol without a fixed Celsius
cutoff; charging can prevent the device from reaching a cold-start temperature. Moderate or higher thermal status waits; OS thermal
protections are never overridden. This catches background update jobs even on a cool device. It records current thermal readings approximately
every ten seconds during measurement, plus start/end power state. Runs retain
any later throttling observations; they are not silently filtered for speed.
Allocator/WASM measurements are available; process RSS is `null`, not zero.
`AUDIO_ACTIVE=1` verifies running WebAudio during Android gameplay replays. Desktop
autoplay/mute launch flags are not applied to the installed Android browser.
Raw external-project evidence must remain private; publish only reviewed anonymous
numeric measurements and plots, following the privacy guidance above.

Geometry stress cases can exceed the timing histogram's 1,000 ms range. Normal
collection rejects any overflow. An explicitly separate geometry-only collection
may set `ALLOW_TIMING_OVERFLOW=1` to retain throughput and memory measurements
while recording the overflow count. This does not enlarge the histogram or invent
a percentile: an out-of-range p99 remains absent in raw evidence and exports as
`null` with its lower bound. The Android report marks it as censored and omits
affected p99 means and percentage comparisons. Preserve rejected attempts and
restart the geometry comparison under the explicit policy; do not mix policies
or silently change the validation of a running collection.

### Quiet frame-pacing evaluation

Worker stdout synchronously proxies to browser main in this Emscripten build.
The interactive Bunnymark status line and periodic `WEB_MEMORY` output can
therefore introduce long update intervals while main is rendering. To isolate
engine pacing, rebuild content with the current `prepare.py` and
`memory_probe.lua`, then use `STATUS_REPORTS=0 MEMORY_SAMPLES=0 METRICS=0
DIAGNOSTICS=0 STACK_MEASURE=0 MEMORY_PROBE=1`. The runner verifies that Bunnymark
actually suppressed its status messages. Final snapshot accounting is emitted
after measurement as `WEB_MEMORY_FINAL`; browser allocator polling remains
enabled. Preserve a separate logging-on diagnostic control instead of treating
old logging-on p99 values as directly comparable.

An `overlap_ready` variant can specify `"readyBudgetMs":32` to evaluate the
opt-in readiness estimate; zero retains the original policy. Use identical
content and Release binaries across direct, completion, ready, and budgeted
variants. Freeze bundles, rotate three repetitions, and keep heavy geometry
overflow reporting in its own collection. `pacing_report.py OUTPUT COLLECTION...`
exports anonymous per-run numbers, throughput/p99 and memory graphs, capacity
counters and explicit target calculations. Optional repeated `--diagnostic DIR`
arguments include aggregate trace attribution separately from acceptance data.
Keep raw game evidence private as described above.

The broad component fixture accepts `ready_budget_ms` in its URL. Set
`READY_BUDGET_MS=32` when running `test_component_2d.cjs` with ready modes to
verify the selected policy. A budget below 2 ms with a `-slow` ready mode
requires a nonzero budget-rejection count. Compare `readyRenders` with an
unbudgeted control to verify changed consumption: rejection counts also include
callbacks that would have lacked render credit.
