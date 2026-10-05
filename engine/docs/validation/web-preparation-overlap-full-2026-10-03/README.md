# Web preparation overlap validation

Validated on October 3–4, 2026. This extends the earlier model/mesh milestone:
simulation **and preparation** may now execute while browser main consumes the
previous owned frame. Culling, batching, CPU skinning, geometry generation and
render Lua remain on the worker. Publication and resource mutation retain their
safety barriers.

## Results

[Final machine-readable browser results](aligned/results.json) include ten normal cases and two
context-loss cases. Chrome 154.0.8037.95, Release `wasm_pthread-web`, headless
software WebGL. All ten normal cases produced the same frozen PNG SHA-256:
`4f47eb7ba25919ee319982a9c0a801e33ff410d51bf98a0cd3775b1315e1be00`.
The harness also checked displaced animated poses and seven independent geometry
color probes, ensuring equality cannot pass with all geometry missing. At least
two animated models shared an instanced draw. Both context-loss cases stopped
with the expected failure code; all normal cases exited successfully.

The coverage includes static models, CPU/GPU/instanced animation, local/world-space
meshes, shared mesh-buffer growth/shrink, model enable/disable, animation callbacks,
factory and proxy creation/destruction, sprites, labels, tilemaps, GUI/text/particles,
physics, input, sound completion, resizing and update/render pause/resume.

The slow-consumer cases insert a deliberate 2 ms delay into each instanced draw.
Full overlap must record complete preparation spans within active consumption;
the `poc_web_prepare_overlap=0` control must record zero. Both must overlap
simulation. These assertions passed. Counts are recorded in `results.json` and
individual logs under `aligned/`. This artificial delay is solely a correctness test and is absent
from all performance runs. An earlier three-case check is preserved in
[the initial validation directory](../web-preparation-overlap-2026-10-03/results.json).

| Native check | Result |
| --- | --- |
| Full render suite | 94 tests / 2,550 assertions passed |
| Full graphics suite | 65 tests / 17,328 assertions passed |
| Full gamesys suite | 563 tests / 25,422 assertions passed |
| Web component admission | 1 test / 16 assertions passed |

The new native regression tests exercise simultaneous recording and replay on
separate threads, copied texture/buffer bytes, independent texture dimensions and
viewport state, and rejection of oversized/unsupported texture uploads before
reading their source. A further regression verifies text uses producer-owned
atlas dimensions even when backend dimensions describe another frame. This avoids
reading metadata while a previous atlas reset uploads on main. Render-queue tests verify the strict preparation-overlap
counter while a consumer callback is active. Another regression verifies that an
odd-sized glyph upload cannot misalign a later floating-point pose payload for
WebGL's typed heap views. The latest native results are in
`native-render-aligned.log`, `native-gamesys-aligned.log`, `native-admission-final.log`
and `native-graphics-aligned.log`. Earlier native logs remain as intermediate evidence.

The [benchmark workload pixel check](benchmark-geometry/results.json) separately
verifies visible world/local meshes, static models, CPU/GPU-skinned models and
instancing at both 200 and 1,000 groups. It passes in direct and full-overlap modes,
with no WebGL errors. This additional workload check preceded the final payload-alignment
fix; the full 12-case fixture suite was rerun after that fix. Its software-rendered
screenshots are not timing evidence.

## Reproduction

```sh
cmake --build engine/build/arm64-macos-render-poc \
  --target test_render test_graphics test_engine test_gamesys -j8
tmp/render-poc-build/engine/render/build/arm64-macos/src/test/test_render
tmp/render-poc-build/engine/graphics/build/arm64-macos/src/test/test_graphics
# Run test_gamesys from its prepared gamesys-test-runtime directory.

# After building the web_component_2d fixture content and Release engine:
python3 scripts/web/serve_component_poc.py \
  tmp/web-preparation-overlap-aligned-bundle --port 8766
NODE_PATH=/path/to/node_modules CHROME_EXECUTABLE=/path/to/chrome \
  node scripts/web/test_component_2d.cjs http://127.0.0.1:8766/ NEW_RESULTS_DIR
```

The native and web build roots were isolated under `tmp/render-poc-build` and
`tmp/web-component-poc-build`. [Build hashes](builds.json) identify both the frozen
initial correctness bundle and the initial benchmark bundle; [final hashes](builds-aligned.json)
identify the binaries used after the font-atlas and payload-alignment fixes. The final source delta from the recorded
Git HEAD is preserved as [a compressed patch](engine-changes-aligned.patch.gz).
The frozen correctness bundle is `tmp/web-preparation-overlap-aligned-bundle`.
The final 60-run hardware benchmark bundle is
`tmp/web-preparation-benchmark-aligned-bundle`; its engine files match the
benchmark-output hashes in `builds-aligned.json`.

Performance and memory measurements are separate:
[hardware-browser report](../../WEB_PREPARATION_OVERLAP_RESULTS.md).
These checks do not establish cross-browser compatibility, race freedom for
arbitrary extensions, or native/mobile performance.
