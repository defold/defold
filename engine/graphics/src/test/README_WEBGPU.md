# WebGPU pipeline descriptor tests

`test_graphics_webgpu_pipeline` exercises the production pipeline builder without
opening a window or creating a graphics device. It checks two color attachments,
disabled blending, and separate color/alpha blend factors and equations.

Use AddressSanitizer for reproducible descriptor-lifetime failures. The test
compiles the production translation unit with an instrumented replacement for
`wgpuDeviceCreateRenderPipeline()`. This replacement reads the nested descriptors
inside the API call, so ASan checks their lifetimes even though the packaged Dawn
library is not instrumented. No production test hook is required.

From a configured Defold build environment on arm64 macOS, run:

```sh
cmake -S . -B engine/build/webgpu-pipeline-asan -G Ninja \
    -DDEFOLD_BUILD_HOME="$PWD/engine/build/webgpu-pipeline-asan" \
    -DDEFOLD_SELECTED_ENGINE_LIBS=dlib,ddf,platform,graphics \
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
