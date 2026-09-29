# DX12 validation

The Windows adapter supports the shared raster API, sampled 2D/array/cube/3D textures, writable 2D/3D images, compute dispatch, and multisampled offscreen color targets. Feature and texture-format queries describe what this implementation can bind.

SSBO support is intentionally deferred to [dmsdk-ssbo-support](https://github.com/defold/defold/tree/dmsdk-ssbo-support). That branch already adds the shared buffer API, DX12 bindings, and HLSL compiler handling, in addition to the other adapters. The overlap was checked at `b0b70474b33960b67b9b3dbff3d03fbb408a402d`. Until integrated, this adapter reports storage buffers unsupported and rejects programs containing them. Integration should use the retained descriptor pages and explicit root-parameter indices on this branch.

## Automated Windows tests

After installing the normal engine dependencies into `tmp/dynamo_home`, run from a Windows development shell with CMake, Ninja, MSVC and Python available:

```powershell
./ci/test_dx12.ps1
```

The script configures a separate Debug build in `tmp/dx12-ci`, then builds and runs the shared graphics tests, shader compiler tests and DX12 WARP tests. It accepts `-SdkRoot`, `-BuildDirectory`, and `-Python` overrides. The Windows CI job runs this script and installs the Windows Graphics Tools capability if necessary.

WARP tests require the D3D12 debug layer but do not require a display or physical GPU. Warning/error messages fail the tests, except the two optimized-clear-value mismatch advisories. Coverage includes descriptor/uniform exhaustion, arrays and compressed sub-updates, stencil pixels, 3D mip/Z updates, dependent UAV dispatches, compute-to-draw sampling, depth sampling, cubemap faces and resizing, MSAA MRT clears/resolves/readback, reload failure preservation, lines, depth bias and scissor preservation.

Enable the optional CPU submission benchmark after correctness checks:

```powershell
$env:DEFOLD_DX12_BENCHMARK = '1'
./ci/test_dx12.ps1
Remove-Item Env:DEFOLD_DX12_BENCHMARK
```

This reports a cold draw/PSO creation, 1,000 small buffer uploads and 1,000 cached draws under WARP. It does not measure hardware GPU throughput or establish performance parity.

## Cross-adapter captures and presentation

With the same build configured, enable Vulkan and build the shared test app:

```powershell
cmake -S . -B tmp/dx12-ci -DWITH_VULKAN=ON
cmake --build tmp/dx12-ci --target test_app_graphics
$app = './tmp/dx12-ci/output/engine/graphics/build/x86_64-win32/src/test/test_app_graphics.exe'
$env:DEFOLD_TEST_AUTO_EXIT = '1'
foreach ($adapter in @('dx12', 'vulkan', 'opengl')) {
    $env:DEFOLD_TEST_CAPTURE = "$PWD/tmp/$adapter.bgra"
    & $app $adapter raster-parity headless resize
    if ($LASTEXITCODE -ne 0) { throw "$adapter parity failed" }
}
Get-FileHash tmp/dx12.bgra, tmp/vulkan.bgra, tmp/opengl.bgra
& $app dx12 raster-parity headless resize-mid-frame validate msaa
```

This renders an asymmetric four-quadrant image, verifies every BGRA pixel and checks orientation for 200 frames, resizes every tenth frame and changes the presentation interval. The optional capture is frame 3 at 512 x 512, raw BGRA8 in top-to-bottom row order. The DX12-specific invocation also exercises 4x backbuffer MSAA and resizing during an active frame. `headless` hides the window; these presentation tests still need a desktop and a supported GPU. An unavailable explicitly requested adapter fails instead of silently selecting another.

Regenerate the checked-in SPIR-V test assets after editing `parity.vert` or `parity.frag`:

```powershell
python engine/graphics/src/test/test_app_graphics_package_parity.py tmp/dynamo_home/ext/bin/x86_64-win32/glslang.exe
```

## Current boundaries

- Public pixel readback handles RGBA8/BGRA8 color attachments, including MSAA resolves and cubemap faces. Other attachment formats have no readback conversion.
- Cubemap targets remain single-sampled. Offscreen MSAA resolves color attachments; depth remains multisampled.
- Texture upload callbacks retain the synchronous fallback; asynchronous upload performance is not claimed.
- The shared primitive enum exposes triangles, triangle strips and lines; there is no shared point primitive to implement.
- Targeted WARP and Windows capture tests do not replace game/GUI scene coverage, a GPU-vendor matrix, presentation pacing measurements or validation of the private Xbox backend.
