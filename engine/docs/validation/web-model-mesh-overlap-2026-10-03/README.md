# Web model/mesh overlap validation — 2026-10-03

The new opt-in `component-web-snapshots` path passed the mixed-component browser
checks. Game simulation can complete on the worker while browser main consumes
an owned frame containing models and meshes. Preparation still drains the previous
consumer before accessing component worlds; culling, batching and geometry
preparation run on the worker. This is a correctness result, not a speedup claim.

See [configuration, ownership and limits](../../WEB_COMPONENT_POC.md#overlapping-modelmesh-and-mixed-component-frames-2026-10-03).

## Browser results

Chrome 154.0.8037.95 on macOS, headless with software WebGL and COOP/COEP.
Release `wasm_pthread-web`, deterministic 60 Hz simulation timestep, 220 updates
per normal case. The default worker stack is 5 MiB; stack painting was disabled.

[Final machine-readable results](mixed/results.json), logs and PNGs are in `mixed/`.
All eight normal cases produced final PNG SHA-256
`4f47eb7ba25919ee319982a9c0a801e33ff410d51bf98a0cd3775b1315e1be00`.
Both forced context-loss cases shut down with the expected code 1. All normal
cases exited successfully and produced no WebGL errors.

| Case | Result | Complete simulation updates inside active consumption, by checkpoint |
| --- | --- | ---: |
| Owned frames, default rAF scheduler | Pass | 122 |
| Owned frames, artificial 2 ms delay per instanced draw | Pass | 165 |
| Owned frames, completion scheduler | Pass | 1 |
| Owned frames, retry scheduler | Pass | 120 |
| Broad serialized control | Pass | 0 |
| Direct control | Pass | 0 |
| Non-threaded component control | Pass | 0 |
| Legacy broad-mode flag | Pass | 0 |
| Serialized context loss | Clean stop | — |
| Owned-frame context loss | Clean stop | — |

These overlap counts are strict lower bounds: a complete simulation update must
start and finish inside an executing consumer callback. Partial overlap does not
count. The delayed case is a deterministic concurrency assertion and must never
be used as a performance measurement. Differences between scheduler counts are
not throughput rankings.

The fixture covers local/world-space meshes, shared buffer growth and shrink,
a static model, animated CPU skinning, GPU skinning and instancing. Color probes
verify seven visible geometry paths and the displaced animated pose; a WebGL
probe confirms at least two animated models share an instanced draw. It checks
animation callbacks, bone movement, model disable/enable, factory/proxy creation
and destruction, plus sprites, labels, tilemaps, GUI/text/particles, physics,
input, audio completion, window resizing and render/update pause/resume.

The two frame slots retained 186,896 bytes (about 182.5 KiB) at the final diagnostic
checkpoint in the default overlapping case. This is frame storage only, excluding
consumer context, GPU resources, worker stack and browser memory. No new memory
or throughput benchmark was performed.

The [existing sprite-only regression](sprite-regression/results.json) also passed
all 12 direct/inline/threaded/scheduler/context-loss cases with identical frozen
pixels and simulation checksums. That suite ran before the last light-capacity
fix, which only affects the new broad owned-frame path. `initial-mixed/` contains
the earlier mixed checks before that fix; use `mixed/` as the final evidence.

## Native checks

| Check | Passed |
| --- | ---: |
| Full render suite | 90 tests / 2,523 assertions |
| Web component admission | 1 test |
| Model filter | 30 tests |
| Mesh filter (includes mesh-set resources and some model tests) | 24 tests |
| Existing sprite snapshot filter | 11 tests |

Logs are saved alongside this file. Five new render tests cover copied objects,
constants/matrices/time, vertex/index upload ordering, frame admission/rejection,
resource invalidation during capture, a strict actual-consumption overlap counter,
and light values surviving producer deletion. The light test also guards against
sizing the consumer UBO from active lights instead of the configured shader array
capacity. Native tests use the null graphics adapter; the browser checks exercise
WebGL. The test log socket was denied by the sandbox in native gamesys runs; all
test assertions passed.

## Reproduction

From the repository root, using the existing configured dependency environment:

```sh
cmake --build engine/build/arm64-macos-render-poc --target test_render test_engine test_gamesys -j8

# Compile a copy of engine/engine/src/test/web_component_2d with bob.jar.
# Then select the compiled fixture for the web engine:
EMSDK="$PWD/tmp/web-poc-emsdk" cmake -S . -B engine/build/wasm-pthread-component-poc \
  -DDEFOLD_WEB_POC_CONTENT="$PWD/tmp/web-components-project/build/default"
EMSDK="$PWD/tmp/web-poc-emsdk" cmake --build engine/build/wasm-pthread-component-poc --target dmengine_release -j8
cp engine/engine/src/test/web_component_2d/index.html \
  tmp/web-component-poc-build/engine/engine/build/wasm_pthread-web/index.html
python3 scripts/web/serve_component_poc.py \
  tmp/web-component-poc-build/engine/engine/build/wasm_pthread-web --port 8766
```

In a second terminal, provide Playwright and pngjs via `NODE_PATH`, then run:

```sh
CHROME_EXECUTABLE='/Applications/Google Chrome.app/Contents/MacOS/Google Chrome' \
  node scripts/web/test_component_2d.cjs http://127.0.0.1:8766/ NEW_OUTPUT_DIRECTORY
```

[Build metadata and artifact hashes](build.json) identify the tested dirty working
tree build. A local copy of the validated bundle is retained at
`tmp/web-model-mesh-overlap-validated-bundle`. The previous benchmark-content CMake
configuration was restored after validation. Tests do not establish production
readiness, all possible material/render-script features, hardware-GPU performance,
Safari/Firefox behavior, or extension compatibility.
