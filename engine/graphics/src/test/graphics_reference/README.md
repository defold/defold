# Graphics capture references

These are actual GPU captures, visually reviewed on 2026-09-17. Ordinary
test runs only read them. There is deliberately no reference-update switch.
References use Metal except `stencil_faces`, which uses OpenGL: Metal's
current face-specific state produces an incorrect blank image. The five
advanced stencil references and the cubemap also match independently specified expected pixels in
`test_graphics_images.py`, and OpenGL and Vulkan agree with those pixels.

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
| `stencil_depth` | Real front/behind depths against an invisible occluder. Top band orange; middle band green with an orange center from depth-failure INCR; bottom band blue from stencil-failure INVERT. Also checks stencil failure takes precedence over depth failure. |
| `stencil_faces` | Opposite triangle windings. Orange/cyan top tiles verify separate front/back operations; magenta/green bottom tiles verify separate EQUAL/NOTEQUAL comparisons. Common face state reads back the result, preventing incorrect write/read face assignments from cancelling out. |
| `cubemap` | Creates a real 16×16 RGBA cubemap and uploads six faces in +X, -X, +Y, -Y, +Z, -Z order. Direction sampling renders a cross with +Y above; -X, +Z, +X, -Z across; -Y below. Distinct colors, axis labels, an upper-left white marker and a lower-right dark marker expose missing/swapped faces, rotation and mirroring. Uses nearest filtering and explicit mip level 0, with each texel displayed as 3×3 pixels. |

Before the repeated render, stencil cases overwrite a larger region with 1.
The subsequent clear must remove that stale mask. Metal's first basic/nested
images are correct, but the repeated render currently differs and fails the
capture. The nested reference is the independently verified first image;
accepting that reference does not exempt the repeated-render failure.
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
(requires `glslangValidator`). Vulkan's shader converts the test's upward
clip-space Y to Vulkan's downward Y. Test vertices use depth in [0, 1]; the
OpenGL shader maps it to [-1, 1]. Windows compiles the local HLSL to
DXBC and its root signature using `D3DCompile` when creating the test program.
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

The ordinary and sequential runners also register these tests. Hosted CI
automatically captures only on native Linux/Vulkan (Mesa software Vulkan
under Xvfb); macOS and Windows record explicit policy skips. This does not
disable the manual target. Native macOS WebGPU requires the Apple Silicon
Dawn build support; configurations without that adapter are explicitly skipped.
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
against RGB (37, 73, 109). Rendering defects remain failures, including
the known DX12 stencil defect; there are no expected-failure exemptions.
Readback diagnostics compare the whole image and require identical pixels.

Validation on the capture host: all nine cases pass on OpenGL and Vulkan
through MoltenVK with 100% reference likeness. Metal fails the
basic/nested repeated-render checks and the separate-face image comparison
(about 59.96% likeness); its other cases pass. These remain ordinary failures,
without expected-failure exemptions or backend-specific draw workarounds.
Each case also checks a 13-pixel-wide subregion and rendering after readback.
The cubemap matches its expected pixels and Metal reference at 100% on all
three available backends. Viewport and depth/stencil continuation probes pass
on those backends. The latter caught Vulkan discarding depth/stencil when
readback ended a pass; readback now resumes with attachment loads and stores
that preserve those contents. The 21 graphics harness tests pass, including exact
reference pixel checks, representative stencil faults, and missing, swapped,
rotated or mirrored cubemap faces, failed-render diagnostics, portable saved
diagnostics, stale-image removal and final case totals.
Native Dawn support is absent from this checkout,
so WebGPU device validation is pending. DX12 passed a Windows SDK syntax
check; its actual stencil failure still needs to be captured on Windows.
Native Linux CI execution is also pending.
