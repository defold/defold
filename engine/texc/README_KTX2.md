# KTX2 source textures

Bob and the editor import standalone `.ktx2` files and glTF/GLB images containing
KTX2 bytes. External image URIs, data URIs, and buffer views use the same decoder.
The output is the existing `.texturec` format. Model texture bindings can select
KTX2 images directly; atlas, tile-source, and cubemap-face inputs are unchanged.

The editor opens KTX2 as a texture overview with selectable authored mip levels
in the Outline and scene. Properties show the source format, supercompression,
dimensions, mip count, channels, color space, orientation, and alpha metadata.
The scene shows the matched texture profile. This overview retains the source
mip sizes even when the profile resizes, regenerates, or disables compiled mips;
material previews and builds still apply the profile.

Supported sources are non-array 2D textures using ETC1S/BasisLZ, UASTC (optionally
Zstd- or Zlib-compressed), BC7, or 8-bit R/RG/RGB/RGBA UNORM/sRGB. Native pixel and BC7
payloads can also use Zstd or Zlib. Color primaries must be unspecified or BT.709.
HDR, other GPU codecs, arrays, cubemaps,
volumes, and runtime KTX2 loading are not supported. Container ranges and decoded
sizes are checked before decoding; decoded mip chains are limited to 512 MiB.

## Texture profiles

Existing profile format, compressor, preset, size, alpha, and mipmap settings
apply. Profiles match the ordinary source path or the virtual glTF image path,
for example `/models/robot.glb/images/0.ktx2`.

Three optional platform settings default to false:

```protobuf
recompress: false
regenerate_mipmaps: false
keep_ktx2_format: false
```

In the texture profiles editor, select a profile and platform, then enable
**Keep KTX2 Format**. This replaces the platform's format alternatives for KTX2
sources: ETC1S stays ETC1S, UASTC stays UASTC, BC7 stays BC7, and raw sources stay
uncompressed. Other image types still use the configured format alternatives.
For example:

```protobuf
path_settings { path: "**/*.ktx2" profile: "KTX2" }
profiles {
  name: "KTX2"
  platforms {
    os: OS_ID_GENERIC
    mipmaps: true
    premultiply_alpha: false
    keep_ktx2_format: true
  }
}
```

With this setting, compatible source levels retain their encoded data. ETC1S
slices and codebooks are repacked into single-mip `.basis` payloads; UASTC blocks
are repacked the same way, and BC7 blocks are stored directly. Zstd/Zlib wrapping
is removed offline. Required pixel processing re-encodes into the same codec,
which can introduce additional loss. The project developer is responsible for
runtime codec and GPU support; this option does not add runtime decoders.

`recompress` forces encoding with the selected compressor and preset. Otherwise,
compatible UASTC levels can retain their blocks, repacked as individual `.basis`
images in `.texturec`. This requires BasisU output and no pixel transformations.
With **Keep KTX2 Format**, `recompress` instead uses the source codec's offline
encoder. Without that option, ETC1S and BC7 are decoded and encoded using the
profile; BC7-to-BasisU conversion is lossy. Disabling compression or using no
profile retains the existing uncompressed-output behavior.

Authored mip images are retained even when their encoding changes. A size limit
can select an existing mip and its descendants. Resampling rebuilds the chain;
missing descendants are generated from the last retained image.
`regenerate_mipmaps` instead generates descendants from the processed base image;
it does not force recompression of an otherwise reusable base. `mipmaps: false`
still emits only the base level.

Orientation, channel swizzles, transfer function, and premultiplication metadata
are interpreted consistently for builds and previews. R/G channels remain data
channels rather than becoming luminance/alpha. The usual Defold Y flip still
applies, so top-left KTX2 sources (including standard glTF KTX2 images) normally
require decoding and re-encoding even with **Keep KTX2 Format**. Bottom-left
sources can retain their encoded data when the other settings are compatible.
Source bytes and the effective profile participate in editor build
and preview invalidation, including unsaved profile edits.

Linear/sRGB source metadata selects BasisU's encoding options and is preserved in
repacked and recompressed Basis headers. Decoding does not apply a gamma conversion
to the source channel values. Normal maps and other data textures should use
linear KTX2 metadata; disable `premultiply_alpha` when alpha stores data. KTX2
resize and mipmap filtering operates in linear light for sRGB RGB channels, then
converts back to sRGB. Linear data and alpha are filtered directly, without gamma
conversion or normal-vector normalization. Ordinary images retain their existing
channel-value filtering. Runtime textures still use UNORM formats; color conversion
for shading remains the shader's responsibility.

The native implementation lives in `src/texc_ktx2.cpp` and is exposed through
`TexcLibraryJni.Ktx2Texture`, an owned, closeable source with lazy mip decoding.
Basis Universal 2.50's `ktx2_transcoder` reads ETC1S and UASTC, including Zstd,
and its `basisu_file` writer repacks retained ETC1S/UASTC data. A bounded container
adapter handles native BC7/raw pixels and unwraps Zlib before passing UASTC to
BasisU. The BC7 and Zstd decoders come from the same external `basis_encoder`
library, which also supplies the offline ETC1S/UASTC and scalar BC7 encoders.
The runtime links only `basis_transcoder` and has no KTX2 dependency.

See the [KTX2 specification](https://registry.khronos.org/KTX/specs/2.0/ktxspec.v2.html)
and [KHR_texture_basisu](https://github.com/KhronosGroup/glTF/tree/main/extensions/2.0/Khronos/KHR_texture_basisu).
The glTF extension specifies ETC1S/UASTC 2D textures; native BC7 sources are an
additional build-time import capability.
