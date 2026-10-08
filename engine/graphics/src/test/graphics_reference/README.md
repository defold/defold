# Graphics capture references

The original nine references are actual GPU captures, visually reviewed on 2026-09-17. Ordinary
test runs only read them. There is deliberately no reference-update switch.
References use Metal except `stencil_faces`, which uses OpenGL: the
original handwritten Metal shader omitted the production compiler's Y flip
and produced an incorrect blank image in that case. The corrected shader now
matches the unchanged OpenGL reference with Metal's original face mapping. The five
advanced stencil references and the cubemap also match independently specified expected pixels in
`test_graphics_images.py`, and OpenGL, Vulkan and WebGPU agree with those pixels.

Provenance: Apple Silicon (`arm64`), macOS 27.0, Metal adapter, Debug CMake
build, 256×256 RGBA8 offscreen target, depth/stencil, one sample. Captured
using the sources in this change, based on Defold `7f0f554f41`.

Capture commands (from the repository root):

```sh
build/graphics-root/engine/graphics/build/arm64-macos/src/test/test_app_graphics --backend metal --case clear --output build/graphics-captures
build/graphics-root/engine/graphics/build/arm64-macos/src/test/test_app_graphics --backend metal --case triangle --output build/graphics-captures
build/graphics-root/engine/graphics/build/arm64-macos/src/test/test_app_graphics --backend metal --case stencil --output build/graphics-captures
build/graphics-root/engine/graphics/build/arm64-macos/src/test/test_app_graphics --backend metal --case stencil_nested --output build/graphics-captures
build/graphics-root/engine/graphics/build/arm64-macos/src/test/test_app_graphics --backend metal --case stencil_masks --output build/graphics-captures
build/graphics-root/engine/graphics/build/arm64-macos/src/test/test_app_graphics --backend metal --case stencil_ops --output build/graphics-captures
build/graphics-root/engine/graphics/build/arm64-macos/src/test/test_app_graphics --backend metal --case stencil_depth --output build/graphics-captures
build/graphics-root/engine/graphics/build/arm64-macos/src/test/test_app_graphics --backend opengl --case stencil_faces --output build/graphics-captures
build/graphics-root/engine/graphics/build/arm64-macos/src/test/test_app_graphics --backend metal --case cubemap --output build/graphics-captures
```

The reviewed outputs were copied from `build/graphics-captures/metal/`
and, for `stencil_faces`, `build/graphics-captures/opengl/`.
Clear is uniformly RGB (37, 73, 109). The triangle has a high blue vertex,
a low red vertex on the left, and a green vertex on the right. Stencil
shows an offset orange rectangle within the larger drawn rectangle, with
clear pixels surrounding it. All images are opaque and top-down.

| Case | Coverage and expected image |
| --- | --- |
| `stencil_nested` | Three mask levels built with equality and increment. Orange outer region, green child, blue grandchild. Children extend outside their parents so incorrect acceptance remains visible. |
| `stencil_masks` | Low/high-nibble write masks preserve the other bits; a zero write mask blocks REPLACE. Full-value comparisons produce yellow/orange/green/blue regions. Magenta/cyan strips compare masked references and stored values. |
| `stencil_ops` | Nine green tiles, row-major: ZERO, REPLACE, INCR, INCR at 255, DECR, DECR at 0, INVERT, INCR_WRAP at 255, DECR_WRAP at 0. Each tile only draws after an exact-value comparison. |
| `stencil_depth` | Real front/behind depths against an invisible occluder with back-face culling enabled. Top band orange; middle band green with an orange center from depth-failure INCR; bottom band blue from stencil-failure INVERT. Also checks stencil failure takes precedence over depth failure. |
| `stencil_faces` | Opposite triangle windings. Orange/cyan top tiles verify separate front/back operations; magenta/green bottom tiles verify separate EQUAL/NOTEQUAL comparisons. Common face state reads back the result, preventing incorrect write/read face assignments from cancelling out. |
| `cubemap` | Creates a real 16×16 RGBA cubemap and uploads six faces in +X, -X, +Y, -Y, +Z, -Z order. Direction sampling renders a cross with +Y above; -X, +Z, +X, -Z across; -Y below. Distinct colors, axis labels, an upper-left white marker and a lower-right dark marker expose missing/swapped faces, rotation and mirroring. Uses nearest filtering and explicit mip level 0, with each texel displayed as 3×3 pixels. |

Before the repeated render, stencil cases overwrite a larger region with 1.
The subsequent clear must remove that stale mask. The initial test-feature
commit exposed Metal losing the basic/nested masks on the repeated render:
its clear pipeline replaced the active GPU pipeline without invalidating the
cached binding. The subsequent Metal fix restores the draw pipeline after
clear. The nested reference remains the independently verified first image.
Failures retain the second image as `<case>.png.repeated.png`; the report
shows the first and repeated images with their difference.

The triangle case also compares uninterrupted drawing with a sequence
split by readback. One probe retains a partial viewport; the other retains
an existing stencil mask and depth occluder. No state setters, rebinding or
clears occur between the readback and subsequent draw. On failure, both
probe images are retained as `<case>.png.<probe>-expected.png` and
`<case>.png.<probe>-actual.png`, for `viewport` or `depth-stencil`.

Shader sources are local to the test. CMake regenerates their embedded data
when the sources change, using the same aligned embedding helper as the font
viewer. Builds with Vulkan compile SPIR-V with the SDK's `glslang` (or
`glslangValidator` from PATH). Generated files stay in the build directory;
builds without Vulkan do not require a SPIR-V compiler.
The vertex shaders follow production compiler
conventions: MSL negates Y, SPIR-V leaves Y unchanged, and OpenGL maps the
test's [0, 1] depth to [-1, 1]. WGSL supplies the marked Y-flipped offscreen
entry point generated by Bob's shader pipeline. Metal, Vulkan and WebGPU
offscreen image rows are flipped only when exporting PNGs, preserving the winding used for culling
and front/back stencil selection. References and expected pixels are unchanged.
The cubemap variant supplies texture/sampler reflection and native resource
bindings for each backend; its vertex shader passes sampling directions in
the shared second vertex stream.

# Running and collecting captures

```sh
cmake --build <cmake-build-directory> --target generate_graphics_test_images
python3 engine/graphics/src/test/test_graphics_images.py
```

The manual CMake target renders the local backend matrix, records explicit
skips for adapters absent from the build, and produces
`engine/graphics/build/graphics-render-report/<target-platform>/index.html` and `results.json`.
Capture processes create hidden windows with focus-on-show disabled, so the
offscreen tests do not activate a window while running.
Capture PNGs, logs, reproduction commands and platform/adapter metadata
are in `engine/graphics/build/graphics-test-images/<target-platform>/`. Differences are in the
report directory; the HTML embeds all images and logs and can be copied alone.
Unsupported texture formats remain in the capture manifest but are omitted
from the report and its totals. The report header identifies the tested target
platform, independently of the machine running the Python harness. The overview shows platform and backend totals,
summaries for basic rendering, stencil, cubemaps and texture formats, then
links to each failed test. Results default to backend sections, each containing
basic rendering, stencil, cubemaps and texture formats. One report-wide switch
changes to category sections with tests/formats and their backends underneath.
Category summary links select the category view; backend links select the backend
view. Switching groups preserves open comparisons and failure links. Passing
results and the separate skipped-tests section are collapsed by default. Without
JavaScript, the backend/category hierarchy is used.
Keep any diagnostic PNGs alongside the original capture when copying a
capture directory. Capture attempts and skips remove stale diagnostic images.

The capture backends are OpenGL, Metal, Vulkan and WebGPU. The manual CMake
matrix uses Metal/OpenGL/Vulkan/WebGPU on macOS, Vulkan on Linux, and
OpenGL/Vulkan on Windows, with unavailable adapters skipped. Native WebGPU
uses Dawn on Apple Silicon macOS; configure the build with `-DWITH_WEBGPU=ON`
and the Dawn SDK dependencies. DX12 capture support is deferred.

To run only WebGPU locally with the configured capture executable:

```sh
python3 engine/graphics/src/test/run_graphics_images.py --executable build/graphics-root/engine/graphics/build/arm64-macos/src/test/test_app_graphics --capture-dir build/webgpu-captures --backend webgpu --available webgpu --output build/webgpu-render-report
```

The ordinary and sequential runners register the likeness tests. Hosted CI
automatically captures on native Linux/Vulkan (Mesa software Vulkan
under Xvfb) and `arm64_sim-ios`; native macOS and Windows record explicit policy skips. This does not
disable the manual target. The report is collected by the existing
`upload-build-reports` action alongside the font report, including failed runs.
The CI run's **Build Reports** job summary links the downloadable artifact and
provides a download command. The artifact preserves the report paths and includes
the standalone HTML and `results.json`; missing reports are recorded as diagnostic
text. Report links are published in the run summary, not in PR comments.
An explicitly requested missing backend fails:

```sh
python3 engine/graphics/src/test/run_graphics_images.py --executable <test_app_graphics> --capture-dir captures --backend vulkan --available vulkan --output report
```

## Shader regeneration

Edit `graphics_capture.vert` or `graphics_capture*.frag`, then regenerate the
checked-in GLSL, MSL and WGSL variants. `shaderc_standalone` cross-compiles the
SPIR-V produced by `glslang` to GLSL 330 and MSL 2.2 using the engine's compiler
options, including Metal's vertex Y flip and argument buffers. Tint produces
WGSL, then the generator invokes Bob's shared `WgslShader.java` helper using
the host Java runtime to add the offscreen entry point. Vulkan continues
to compile SPIR-V from these sources during the CMake build.

Build the **host** `shaderc_standalone` target first (include `shaderc` in
`DEFOLD_SELECTED_ENGINE_LIBS` if using a focused build). For example, from the
repository root in a Defold build shell on Apple Silicon:

```sh
cmake --build engine/build/arm64-macos --target shaderc_standalone
python3 engine/graphics/src/test/generate_graphics_shaders.py \
  --glslang "$DYNAMO_HOME/ext/bin/arm64-macos/glslang" \
  --shaderc engine/shaderc/build/arm64-macos/src/standalone/shaderc_standalone \
  --tint "$DYNAMO_HOME/ext/bin/arm64-macos/tint"
```

Use the corresponding host paths on other platforms or with an isolated
`DEFOLD_BUILD_HOME`. `--java` selects the host Java runtime when it is not on
`PATH`. `--output-dir` allows generating into a temporary directory for comparison;
`--check` instead fails on stale or missing variants without updating them.
Compiler failures and empty shader output leave the checked-in variants intact.
Commit regenerated variants with their source edits; ordinary
builds, including cross builds for the iOS simulator, use the checked-in variants
without requiring a runnable target-platform shader compiler. Re-run the capture
matrix after regeneration. The OpenGL vertex variant remaps the test's [0, 1]
depth to OpenGL's [-1, 1] clip range, and GLSL texture bindings use shaderc's
combined texture/sampler names.

Native builds that include both `graphics` and `shaderc` register
`run_test_graphics_shaders` with `run_tests`. This regenerates all twelve variants
using the freshly built host compiler and compares them with the checked-in files,
and tests failure handling and the shared WGSL transformation. The
`run_test_shaderc_standalone` target checks the compiler's exit status on real
compilation and file errors. Cross builds continue to use the verified fixtures.

## iOS simulator

Configure CMake with `TARGET_PLATFORM=arm64_sim-ios` on an Apple Silicon Mac
with Xcode and an installed iOS simulator runtime. Metal is selected by default.
For Vulkan, install an arm64 **simulator** MoltenVK library in
`$DYNAMO_HOME/ext/lib/arm64_sim-ios/libMoltenVK.a` and configure with
`WITH_VULKAN=ON`; see [MoltenVK packaging](../../../../../share/ext/moltenvk/README.md#arm64_sim-ios).
The SDK also needs the simulator builds of GLFW, Basis Universal and LZ4.

Run the same `generate_graphics_test_images` CMake target, or invoke the harness directly:

```sh
MTL_DEBUG_LAYER=1 python3 engine/graphics/src/test/run_graphics_images.py \
  --executable <simulator-test_app_graphics> --target-platform arm64_sim-ios \
  --matrix metal vulkan --available metal vulkan \
  --capture-dir build/graphics-test-images/arm64_sim-ios \
  --output build/graphics-render-report/arm64_sim-ios
```

Use `--available metal` for a build without Vulkan. `--simulator <name-or-UDID>`
or `IOS_SIMULATOR_ID` selects a device; otherwise the existing iOS test runner
prefers a booted simulator. The harness installs a temporary app and fixtures,
launches the real `test_app_graphics` once per backend/case through UIKit,
collects captures and logs, then uninstalls its own app. It leaves the simulator
running. A completion marker is required as well as the backend and target
platform markers, so an app crash cannot pass based on `simctl`'s exit status.
Reports record the simulator runtime and device rather than the host macOS version.

Metal ASTC cases are required on `arm64_sim-ios`: reporting those formats as
unsupported fails the test instead of skipping it. The simulator exposes Apple2,
which supports 2D ASTC. Metal's array/3D ASTC feature retains its Apple3 requirement
because it also covers compressed volume textures.

Local validation on 2026-09-26: iPhone 17 Pro simulator, iOS 26.5, Apple M5 Max,
Xcode 27 beta, `MTL_DEBUG_LAYER=1`, MoltenVK 1.4.2: 26 Metal and 28 Vulkan cases
passed, with no failures. All nine rendering/stencil/cubemap cases pass on each
backend. All 14 ASTC formats now pass on Metal. The 16 unsupported format/backend
combinations remain in the capture manifest and are excluded from the report. Native macOS regression coverage
also passes all 80 supported cases across Metal, OpenGL and Vulkan.

## Saved captures

Keep `captures.json` and backend directories together when moving captures
between machines. Rebuild comparisons without rerunning any backend:

```sh
python3 engine/graphics/src/test/run_graphics_images.py --images mac-captures --images linux-captures --images windows-captures --output combined-report
```

Every available capture is compared against its reviewed reference at ≥99%,
including images left by failed processes. A matching image cannot turn a
failed process into a passing case. Each backend/case contributes exactly one
pass, failure or skip to the totals, combining capture and comparison results.
Only clear uses the entire image; all other cases use the foreground union
against RGB (37, 73, 109). Rendering defects remain failures; there are no expected-failure exemptions.
Readback diagnostics compare the whole image and require identical pixels.

Validation on 2026-10-02 (`arm64-macos`, Debug): all nine cases pass on Metal,
OpenGL, Vulkan through MoltenVK and WebGPU through Dawn at 100% reference likeness: 36 passed,
0 failed, 0 skipped. The 50 graphics unit tests and 22 graphics harness tests
pass. Linux and Windows captures were not run locally.

The production changes retained for these tests are:

- Metal: flush a pending clear before readback, convert RGBA attachments to
  the existing BGRA `ReadPixels` contract, and invalidate the pipeline cache
  after the clear draw so the next draw restores its depth/stencil state.
  The original offscreen stencil-face mapping is preserved.
- Vulkan: read the selected target (including its resolve texture), enable
  transfer-source usage, and convert RGBA attachments to BGRA. Offscreen
  stencil state is swapped to match the existing cull-face adjustment.
- WebGPU: read color attachments through a mapped staging buffer, unpack padded
  rows and convert RGBA to BGRA. Request copy-source usage for supported surfaces
  and offscreen color attachments. Separate back-face comparison and pass
  operations use the back-face state instead of accidentally using the front.

Negative controls with the corrected shaders reproduce the two basic/nested
Metal repeated-render failures when cache invalidation is removed, and the
Vulkan separate-face failure when its swap is removed. Reintroducing the
previous proposed Metal face-mapping change also fails `stencil_faces`.
WebGPU passes eight cases before fixing its two back-face stencil assignments;
`stencil_faces` improves from 77.06% to 100% with those assignments corrected.
The depth case now runs with back-face culling enabled. Each case checks a
13-pixel-wide readback region and repeated rendering; the triangle also
checks viewport and depth/stencil preservation across readback. These are
focused render-target regressions; Parking Jam was not run.

# Compressed texture references

`texture_*.png` are CPU-decoded references for the precompressed payloads in
[`../texture_formats`](../texture_formats/README.md), plus the RGBA source baseline.
Each format has its own capture case and reference at the same 99% likeness
threshold. Unsupported formats are explicitly skipped before upload. These
references are generated offline from the blocks, independently of the graphics
backend, and are never updated by ordinary test runs. Add `--show` to a capture
command to display the selected case in a window until it is closed.
