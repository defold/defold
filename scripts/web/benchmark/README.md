# Web component scheduling benchmark

This ports the existing native sprite workloads and Bunnymark into a disposable
project. It does not modify the original projects. GUI is removed from the native
controller object. Both scenes share the native project’s 60,000 sprite/object
capacity and graphics settings; Bunnymark assets and animations are reused, but
this is not its original standalone bundle configuration. All four modes share
these settings. No claim is made about snapshot support for other components.

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

Configuration knobs in the engine:

- `render.poc_web_schedule=0`: current animation-callback dispatch (default).
- `render.poc_web_schedule=1`: opt-in completion dispatch, overlapping sprites only.
- `render.poc_web_schedule=2`: retry after consumption if this callback has not
  already dispatched; overlapping sprites only.
- `render.poc_web_cache_window=1`: snapshot window-open state at dispatch to avoid
  the worker's synchronous query; defaults to 0 for controlled comparisons.
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
