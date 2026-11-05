# Compression validation

Measured on 2026-09-23, Apple M5 Max, arm64 macOS, Xcode 27 beta Clang and
CMake 4.4.0. Release builds use the repository's `-O2` flags and dead stripping.
The comparison baseline is commit `386641ec54`, built with the same SDK and flags.

## Checks performed

- Native modelc: **54 tests, 825 assertions passed**, including the pre-existing
  tests and the compression tests.
- Same suite passed with AddressSanitizer and UndefinedBehaviorSanitizer;
  codec archives were instrumented as well. LeakSanitizer is unavailable on this
  macOS runtime, so `ASAN_OPTIONS=detect_leaks=0` was required.
- CMake `build_ext` completed for ARM64 and x86-64 macOS. The x86-64 invocation
  built/installed ARM64 host dependencies before the target dependencies.
- ARM64 `model`, `modelc_shared`, Java bindings, and tests built successfully.
  The x86-64 macOS `model` static library also built, including the adapter.
- Java JNI smoke imports passed for external Draco Box, external EXT Meshopt Box,
  KHR Meshopt GLB Box, compressed animation/skinning/morph data, shared Draco
  accessors, and KHR Meshopt CesiumMan. Invalid decoded indices raised the existing
  `ModelException`. `-Xcheck:jni` also reported existing unchecked-exception
  warnings in the JNI conversion code; these are outside the compression adapter.
- Follow-up Bob regression: `ModelUtil.loadScene` rejected intentionally empty
  Meshopt fallbacks after JNI had successfully decoded the model. Removing this
  redundant buffer check fixes the supplied BrainStem Meshopt file (34,084
  vertices, one skin, one animation); BrainStem Draco also passes. All 25
  `ModelUtilTest` and `ModelUtilMetadataTest` tests pass, including EXT/KHR omitted
  fallbacks and a missing compressed source. The positive regression reproduced
  the original failure before the Java fix. Gradle `install runSingleTest`
  also passed for both classes, and the rebuilt Bob jar loaded both supplied
  BrainStem variants.
- Bob's automated Draco regression reuses `draco/shared.gltf` and the existing
  scene-loading helper. It verifies shared accessor isolation, triangle lists
  and strips, and normalized colors through JNI. All 18 `ModelUtilTest` tests pass.
- Review found that metadata dependency discovery still required unused Meshopt
  fallback URIs. It now omits those while retaining compressed sources and mixed
  buffers. The inline EXT/KHR regression failed before the fix and passes after it.
- Review reduced test data from 42 files to 13, reusing the native Box fixture in
  Bob and embedding the small mode/filter vectors in one generated table. Draco
  extraction now uses flat error paths and one allocation per decoded accessor.
- Removed 309 unused Draco upstream files. A fresh ARM64 CMake build of both
  codecs passed with the retained decoder sources and header dependencies.
- The shared library's exported symbols are unchanged from the baseline.
- Both fixture generators reproduced the committed files byte for byte.
- Every retained Meshoptimizer/Draco upstream file was compared byte for byte
  with its pinned release archive. Waf files were not changed.

Linux ARM64/x86-64 and Windows x86-64 are configured in the desktop CMake target
set but were not built on this macOS host. Full x86-64 modelc linking/testing
requires the corresponding engine SDK libraries; this workspace has ARM64 engine
libraries. Those remaining platform checks require their normal build hosts/SDKs.
The bundled cgltf writer has pre-existing compile incompatibilities with the
bundled parser header (including missing sparse-extras fields); its Draco
attribute-writing branch was updated to the new ID representation, but a full
writer roundtrip test could not be built without unrelated changes.

## Release footprint

`modelc_shared.dylib`, comparing copies processed identically with `strip -S`
then `strip -x`:

| Measurement | Baseline | With decompression | Growth |
| --- | ---: | ---: | ---: |
| File bytes | 254,488 | 421,240 | 166,752 (162.8 KiB) |
| Mach-O `__TEXT` bytes | 212,992 | 376,832 | 163,840 (160 KiB) |
| Mach-O `__DATA` bytes | 16,384 | 16,384 | 0 |

The numbers include both codecs and importer integration. No size threshold is
asserted. Encoding and optimization entry points are not exported by modelc.

A release linker map attributes the retained code/data as follows (dead-stripped
symbols are excluded):

| Origin | Retained bytes | KiB |
| --- | ---: | ---: |
| Meshoptimizer archive | 13,236 | 12.9 |
| Draco decoder archive | 133,399 | 130.3 |
| Private compression adapter | 7,634 | 7.5 |
| Total codec archives and adapter | 154,269 | 150.7 |

The adapter total includes Draco/STL template code instantiated from upstream
headers. These are symbol sizes, excluding linker padding and metadata. The
remaining approximately 12.2 KiB of the total file growth comes from importer
integration and changes to padding/link metadata. This attribution was obtained
by relinking the same release objects with `-Wl,-map,<path>` and summing the live
symbols by input archive/object file.

For the later PR notes: the initial measurement was 166,672 bytes of total file
growth and 7,594 bytes in the adapter. The tables above reflect the review cleanup;
the Meshoptimizer and Draco archive contributions remain unchanged.

## Timing

Times below are microseconds per operation, using `dmTime::GetMonotonicTime()`
with ten warm-up iterations, measured before the fixture and allocation cleanup.
Inputs were read into memory before timing.
Import includes parsing, resolving cached source bytes, decoding, existing scene
readers, and scene destruction. The separate decode measurement calls the private
wrapper after parsing/loading once: Meshopt writes into preallocated output
buffers and applies filters; Draco decodes and destroys its opaque mesh, excluding
attribute/index extraction into importer accessors. These are observations, not
test assertions.

| Asset | Format | Vertices / indices | Import (µs) | Codec decode (µs) |
| --- | --- | ---: | ---: | ---: |
| Box | Uncompressed | 24 / 36 | 7.25 | — |
| Box | Draco | 24 / 36 | 10.47 | 5.76 |
| Box | EXT Meshopt | 24 / 36 | 6.37 | 0.13 |
| Box | KHR Meshopt | 24 / 36 | 6.32 | 0.11 |
| CesiumMan | Uncompressed | 3,273 / 14,016 | 175.64 | — |
| CesiumMan | Draco | 3,572 / 14,016 | 1,130.41 | 961.98 |
| CesiumMan | EXT Meshopt | 2,612 / 14,016 | 132.36 | 22.20 |
| CesiumMan | KHR Meshopt | 2,612 / 14,016 | 133.45 | 20.45 |

Import used 5,000 iterations for Box and 300 for CesiumMan; isolated decode used
10,000 and 1,000 respectively. The supplied Draco and uncompressed CesiumMan models
have different vertex counts. Meshopt CesiumMan was produced with upstream
gltfpack 1.2, which optimizes/reorders geometry and animation. These figures are
not an equal-layout codec comparison. Box Meshopt was encoded without optimizing
or quantizing its original buffers.

The larger reference model comes from
[Khronos glTF Sample Models / CesiumMan](https://github.com/KhronosGroup/glTF-Sample-Models/tree/main/2.0/CesiumMan),
using the supplied local `example-gltf-reference` checkout. It is not added as a
test fixture. Build gltfpack from the full Meshoptimizer v1.2 release separately
from `build_ext`, then generate its benchmark inputs with:

```sh
gltfpack -i CesiumMan/glTF/CesiumMan.gltf -o CesiumMan-EXT.glb \
  -c -ce ext -kn -km -vpf -tr
gltfpack -i CesiumMan/glTF/CesiumMan.gltf -o CesiumMan-KHR.glb \
  -c -ce khr -kn -km -vpf -tr
```
