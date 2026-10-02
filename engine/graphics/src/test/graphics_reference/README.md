# Graphics capture references

These are actual GPU captures, visually reviewed on 2026-09-17. Ordinary
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

Shader sources are local to the test. Regenerate the checked-in shader
header with `python3 engine/graphics/src/test/package_capture_shaders.py`
(requires `glslangValidator`). The vertex shaders follow production compiler
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
`engine/graphics/build/graphics-render-report/index.html` and `results.json`.
Capture processes create hidden windows with focus-on-show disabled, so the
offscreen tests do not activate a window while running.
Capture PNGs, logs, reproduction commands and platform/adapter metadata
are in `engine/graphics/build/graphics-test-images/`. Differences are in the
report directory; the HTML embeds all images and logs and can be copied alone.
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
automatically captures only on native Linux/Vulkan (Mesa software Vulkan
under Xvfb); macOS and Windows record explicit policy skips. This does not
disable the manual target. The report is collected by the existing
`upload-build-reports` action, including failed runs.
An explicitly requested missing backend fails:

```sh
python3 engine/graphics/src/test/run_graphics_images.py --executable <test_app_graphics> --capture-dir captures --backend vulkan --available vulkan --output report
```

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
