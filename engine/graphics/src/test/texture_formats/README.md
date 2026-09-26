# Texture format fixtures

`testimage.png` is a deterministic 256×256 grid labelled A1–H8. The palette
and alternating rows make channel swaps, rotation, mirroring and block-layout
errors visible. The original palette uses green `#558B7B`, pale yellow
`#F6E697`, orange `#F3AD70` and red `#DA6357`, with dark `#121815` labels.
`testimage.<format>` contains a **raw GPU payload**, without a container header: one 256×256 mip, top-down, tightly packed. PVRTC uses its
native swizzled block order. The app passes these bytes to `SetTexture`; it does not
compress or transcode at runtime. Metal uploads PVRTC with zero row/image pitches
to a shared texture, then blits into the private texture. Vulkan/MoltenVK uses
`VK_EXT_host_image_copy`. Both paths keep the compressed block order unchanged.

The cases cover RGBA8, BC1/3/4/5/7, ETC1, ETC2 RGBA, EAC R/RG, PVRTC1 RGB/RGBA
4bpp, and all 14 supported ASTC block dimensions. `formats.inc` is the shared
case/format/block-size list consumed by the app and report runner. PVRTC1 2bpp
is not included: the repository's Basis encoder only produces PVRTC1 4bpp.
Depth/stencil, integer, floating-point and other uncompressed formats are
outside this compressed-texture fixture set.

Run a capture, or keep the texture visible until the window is closed:

```sh
<test_app_graphics> --backend metal --case texture_bc7 --output captures
<test_app_graphics> --backend metal --case texture_bc7 --show
```

Use `--list-cases` for the full list. Every texture case queries
`IsTextureFormatSupported` before creating or loading the texture. Unsupported
formats print `GRAPHICS_CAPTURE_SKIP` and exit with code 77; the capture manifest
records the reason. Missing/corrupt supported fixtures and rendering errors
remain failures. The capture uses nearest filtering at mip 0, with opaque
output; R/RG textures retain their native red/red-green sampled channels.

## Regeneration

Normal builds and test runs only read the checked-in assets. Regeneration
requires Pillow, the built Defold `texc` library, Basis Universal 2.50 and
astcenc from the Defold SDK. On macOS, from the repository root and a Defold
build shell:

```sh
c++ -std=c++17 -DBASISU_SUPPORT_SSE=0 \
  -I "$DYNAMO_HOME/include" -I "$DYNAMO_HOME/ext/include" \
  engine/graphics/src/test/texture_formats/encode.cpp \
  "$DYNAMO_HOME/lib/arm64-macos/libtexc.a" \
  "$DYNAMO_HOME/ext/lib/arm64-macos/libbasis_encoder.a" \
  "$DYNAMO_HOME/ext/lib/arm64-macos/libbasis_transcoder.a" \
  "$DYNAMO_HOME/ext/lib/arm64-macos/libastcenc.a" \
  "$DYNAMO_HOME/lib/arm64-macos/libdlib.a" \
  "$DYNAMO_HOME/lib/arm64-macos/libprofile_null.a" \
  -framework Foundation -framework Security -o /tmp/encode-graphics-textures
python3 engine/graphics/src/test/texture_formats/generate.py \
  --encoder /tmp/encode-graphics-textures
```

The offline tool encodes BC/ETC/PVRTC from UASTC at level 2 without RDO, and
ASTC at quality 60, using one thread. It independently CPU-decodes the resulting
blocks into `../graphics_reference/texture_<format>.png`. These are format-specific
references, so lossy compression is not mistaken for a backend rendering defect.
Review changed source images, payloads and decoded references together.

On Apple Silicon/macOS, the current captures pass RGBA, BC, ETC1/ETC2 RGBA, PVRTC4 and ASTC
on Metal and Vulkan/MoltenVK, and RGBA/BC1/BC3/BC4/BC5 on OpenGL. BC4/BC5 are
required on desktop OpenGL 3.0+; missing support fails the capture instead of
skipping it. Both render with 100% reference likeness on this host. EAC R/RG were reported
unsupported on these Metal/Vulkan devices. The PVRTC4 cases exposed uploads that
treated swizzled data as rows. Both RGB and RGBA now render with 100% reference
likeness on Metal and MoltenVK using direct texture uploads. These cases also
compare a fresh asynchronous upload against the synchronous render. MoltenVK
requires host-image-copy support to advertise PVRTC formats. The fixtures, CPU
references and 99% threshold are unchanged. WebGPU, DX12 and mobile devices have
not been validated.
