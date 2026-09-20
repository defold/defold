# Basis Universal

Defold builds Basis Universal **2.50**, pinned to upstream tag
[`v2_50`](https://github.com/BinomialLLC/basis_universal/releases/tag/v2_50),
commit `9bebe16726b3a61c8c213eeee3b7cffb462ef34e`.

`package/basis_universal-2.50` contains the unmodified upstream `encoder`,
`transcoder`, `zstd`, and `LICENSES` directories, plus `LICENSE`, `NOTICE`, and
the upstream `CMakeLists.txt` for reference. Defold's build is in the parent
`CMakeLists.txt`; it applies `defold.patch` to a build copy of the runtime
transcoder to fix missing guards when the new codecs are disabled and remove
an unused header dependency that breaks PS4 builds.
`defold-encoder.patch` fixes the offline UASTC LDR encoder so its `.basis` output
honors the selected linear/sRGB transfer function.
`scripts/cmake/functions_basisu.cmake` imports the installed archives and their
compiler settings into the separate engine/tools build.

Build and install with the regular Defold toolchain:

```sh
./scripts/build.py --platform=arm64-macos build_ext
```

Run after `install_ext` and before building the engine. Headers are installed
under `tmp/dynamo_home/ext/include/basis`; archives are installed under
`tmp/dynamo_home/ext/lib/<platform>`.

## Runtime and offline libraries

| Library | Platforms | Features |
| --- | --- | --- |
| `basis_transcoder` | All | Runtime `.basis` transcoding: UASTC LDR 4x4 and the existing restricted ETC1S support. No XUASTC, XUBC7, HDR, KTX2 parser, Zstd, or offline encoders. |
| `basis_encoder` | Desktop | All upstream Basis Universal encoders and transcoders, its KTX2/DDS support, image readers, and Zstd compression/decompression. |

The engine links `basis_transcoder`. The texture compiler, `texconvert`, and
editor JNI library link `basis_encoder`. External dependencies are uninstrumented,
so tools and JNI use the same archive and no separate `basis_encoder_noasan`
is built. Link one Basis library per executable/shared library;
the two variants contain overlapping transcoder symbols.

The runtime matches Defold's previous codec selection through compiler
definitions. The ETC1S-to-GPU conversion restrictions do not disable UASTC's
own GPU conversion paths. The full offline library enables all of these ETC1S
conversions and the new codecs, including their Zstd support.

Non-Debug web builds compile the runtime transcoder with `-Oz` to reduce its
code size. The engine's optimization and link settings are unchanged.

Basis requires C++17, which the CMake imports propagate to its consumers.
Installation removes obsolete Basis archives from the SDK's `lib/<platform>`
and retires the old external `basis_encoder_noasan` archive. OpenCL and the optional
external astcenc backend are off; the full library's built-in codecs remain
enabled. The existing SSE encoder setting is retained on x86-64 Windows and macOS.

This updates the libraries; Defold's texture profiles and serialized compression
types still select the existing UASTC path. Exposing new codecs or importing
arbitrary KTX2 textures requires separate pipeline changes.

A minimal arm64 macOS `.basis`-to-RGBA decoder, compiled with `-O2`, linked with
dead stripping, and stripped, measured:

| Configuration | Bytes | KiB |
| --- | ---: | ---: |
| Previous 1.16.4 runtime | 133,560 | 130.4 |
| 2.50 with XUASTC, XUBC7, HDR, and Zstd | 631,792 | 617.0 |
| Current 2.50 runtime, matching the previous codec selection | 133,696 | 130.6 |

The current runtime adds 136 bytes (0.10%) over the previous version in this
test. These are decoder comparisons, not measurements of the final engine binary.

The runtime tests use pre-encoded UASTC and ETC1S fixtures so no offline encoder
can mask a missing runtime codec. They cover alpha and non-block-aligned mips
down to 1x1 through Defold's runtime, and check that the new codecs are disabled.
The texture compiler tests separately exercise encoding and KTX2
transcoding, including the new codecs, through `basis_encoder`.

## Changes in the previous Defold copy

The previous vendored sources were compared against upstream tag `1.16.4`,
commit `900e40fb5d2502927360fe2f31762bdbb624455f`. Only three files differed:

* `transcoder/basisu_transcoder.cpp`: the `DM_BASIS_ENCODER`,
  `DM_BASIS_TRANSCODER_UASTC`, and `DM_BASIS_TRANSCODER_ETC1S` patch selected
  transcoder features. The runtime build disabled the legacy ETC1S conversion
  tables to reduce size while retaining UASTC conversion. The same restrictions
  are now set in Defold's CMake build instead of the upstream source.
* `transcoder/basisu_containers.h`: used `__is_trivially_copyable` instead of
  `__has_trivial_copy` in the old GCC/Clang compatibility branch. Upstream 2.50
  has replaced this implementation; the patch is no longer needed.
* `transcoder/basisu_containers_impl.h`: replaced two `sprintf` calls with
  bounded `snprintf` calls in allocation-failure messages. Upstream 2.50 uses
  bounded `vsnprintf` in its replacement error handler.

The old build also disabled runtime KTX2 support and all Zstd support, compiled
the encoder only for desktop platforms, and built a separate no-ASan encoder.
The source update script and its partial patch have been retired with the old
`engine/dlib/src/basis` directory.

## Patch for 2.50

`defold.patch` guards the XUBC7 decoder includes, arithmetic coding tables, and
XUASTC block helpers with `BASISD_SUPPORT_XUASTC`. Upstream 2.50 otherwise fails
to compile with XUASTC/XUBC7 disabled. The runtime also disables
`BASISD_SUPPORT_UASTC_HDR` and `BASISD_SUPPORT_KTX2_ZSTD`.

The patch also makes the shared ASTC helpers and table initialization available
when `BASISD_SUPPORT_XUASTC=1` and `BASISD_SUPPORT_UASTC_HDR=0`. This preserves
the option to add XUASTC without HDR. With Zstd disabled, only XUASTC's
arithmetic-coded profile is supported; XUBC7 decoding still requires Zstd.
The default runtime continues to disable XUASTC. Any experimental build must
set matching feature macros in both the library and its consumers.

The patch also removes the unused `<fenv.h>` include from
`transcoder/basisu_astc_helpers.h`, where it prevents builds with PS4 toolchains
that lack this header.

`build_ext` uses Git to apply the patch to a build copy of the runtime source
and headers, and installs those patched headers for engine consumers;
the full offline library builds the original source. All 115 vendored files
still match upstream byte-for-byte. Keep future source edits in this patch,
following `external/README.md`, with paths relative to the upstream source root
and `a/` and `b/` prefixes. It can also be reapplied with `patch -p1` when
updating versions. Preserve the leading space on blank context lines so Git
can also apply the patch after a Windows checkout converts it to CRLF.
