# WebGPU binding and pipeline tests

`test_graphics_webgpu_pipeline` exercises the production pipeline builder without
opening a window or creating a graphics device. It checks two color attachments,
disabled blending, and separate color/alpha blend factors and equations.

Use AddressSanitizer for reproducible descriptor-lifetime failures. The test
compiles the production translation unit with an instrumented replacement for
`wgpuDeviceCreateRenderPipeline()`. This replacement reads the nested descriptors
inside the API call, so ASan checks their lifetimes even though the packaged Dawn
library is not instrumented. No production test hook is required.

The existing `wasm-web` nightly ASan job runs this test through `run_tests`.
It uses the normal test registration and Bun/Node runner; no workflow changes,
window, or graphics device are required.

From a configured Defold build environment with Emscripten 4.0.6, run:

```sh
cmake -S . -B engine/build/webgpu-pipeline-wasm-asan -G Ninja \
    -DDEFOLD_BUILD_HOME="$PWD/engine/build/webgpu-pipeline-wasm-asan" \
    -DDEFOLD_SELECTED_ENGINE_LIBS=testmain,dlib,ddf,platform,graphics \
    -DTARGET_PLATFORM=wasm-web -DWITH_ASAN=ON
cmake --build engine/build/webgpu-pipeline-wasm-asan \
    --target run_test_graphics_webgpu_pipeline
```

The same regression remains available on arm64 macOS with Dawn:

```sh
cmake -S . -B engine/build/webgpu-pipeline-asan -G Ninja \
    -DDEFOLD_BUILD_HOME="$PWD/engine/build/webgpu-pipeline-asan" \
    -DDEFOLD_SELECTED_ENGINE_LIBS=testmain,dlib,ddf,platform,graphics \
    -DTARGET_PLATFORM=arm64-macos -DWITH_WEBGPU=ON -DWITH_ASAN=ON
cmake --build engine/build/webgpu-pipeline-asan \
    --target run_test_graphics_webgpu_pipeline
```

Before the lifetime fix, ASan reports `stack-use-after-scope` when the API reads
`targets_desc`. Moving only the color-target array into the enclosing scope
exposes the same error for `blend_state`. With both descriptors in the enclosing
scope, both tests pass. In ordinary builds the tests still check descriptor
values, but reproducing the expired-stack access requires `WITH_ASAN=ON`.

The window-backed pixel test remains available separately:

```sh
cmake --build engine/build/webgpu-pipeline-asan --target test_app_graphics
DEFOLD_TEST_AUTO_EXIT=1 engine/build/webgpu-pipeline-asan/engine/graphics/build/arm64-macos/src/test/test_app_graphics webgpu webgpu-blending
```

## Float32 texture bindings

`test_graphics_webgpu_textures` checks that sampled R32F, RG32F and RGBA32F textures
use unfilterable layouts and non-filtering samplers when `float32-filterable` is
unavailable. It includes mixed filtered textures, samplers in another bind group,
texture-unit remapping, uniform preservation, layout/cache reuse and cleanup.
Integer, storage-image and existing compute layouts retain their binding types.

On native macOS, a real WebGPU device is requested without the optional float32
filtering feature. The production layout, bind-group and pipeline builders render
three draws with vertex texture sampling and a linearly filtered RGBA8 color
texture. Pixel readback checks both sampling paths while switching the vertex
texture from RGBA8 to RGBA32F and back; the last draw must reuse the first pipeline.
This case is explicitly skipped if no native adapter is available. The remaining
tests run without a graphics device, including in the existing Wasm ASan suite.

Use either of the configured build directories above:

```sh
cmake --build engine/build/webgpu-pipeline-asan --target run_test_graphics_webgpu_textures
cmake --build engine/build/webgpu-pipeline-wasm-asan --target run_test_graphics_webgpu_textures
```
