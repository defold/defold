# Web component-threading PoC

This opt-in prototype runs simulation, game Lua, render-script command capture,
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

## Scope and controls

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
