# Internal SIMD primitives

Include `<simd/simd.h>` for the default 128-bit `Simd*` primitives, C register
types and runtime capability queries. Include `<simd/simd_128.h>` for the
explicit `Simd128*` aliases. The element type determines the lane count.
These headers are internal and are not installed in dmsdk.

Compiler capabilities select scalar, SSE3/SSE4.2, NEON or Wasm SIMD128.
The 128-bit x86 implementation lives in `simd_sse3.h`.
`DM_SIMD_FORCE_SCALAR=1` forces scalar primitives without changing capability
reporting. Division and square root use accurate operations.

Include `<simd/simd_256.h>` for `Simd256*` on AVX2 and `<simd/simd_512.h>` for
`Simd512*` on AVX-512F/BW. These headers select `simd_avx2.h` and `simd_avx512.h`
respectively.

Packed byte shuffles and widening/packing operate within consecutive
16-byte blocks.
Register types are temporary computation values: do not pass them between
objects compiled with different backend flags.

`SimdGetVersion()` is implemented in `simd.cpp` and requires linking with dlib.
It reports CPU and OS support, including XSAVE/OSXSAVE and XCR0 for AVX. On Wasm,
it probes host SIMD support independently of the caller's compiler flags; the
host must still select a compatible module before instantiation.
Intel macOS also checks OS support and requires macOS 12.2 for AVX-512
register preservation, following Highway's platform handling. It does not
change the inline primitive implementation. Texc builds isolated kernels and
selects its internal function pointers once at library load; its dispatcher
retains the SSE2 baseline. ARM64 selects NEON. Both retain scalar fallbacks.
Texc's scalar, SSE, AVX2, AVX-512 and NEON kernels live in separate
`texc_simd_*.cpp` files. The SIMD kernels use the explicit width APIs. Scalar
is the runtime fallback and uses ordinary loops. The shared
resizer in `texc_resize.cpp` owns coefficients and scratch storage; backends
provide complete-row kernels, with common SIMD math in `texc_simd_resize.cpp`.
`texc_resize.h` contains only private types and declarations. CMake selects ISA
flags without renaming functions.

Scalar and native SIMD suites run `test_simd.c` and compare 92 primitives with
the [in-repo SIMDe scalar reference](../test/simde/simde.h) in `test_simd_simde.c`.
The AVX suites also check the native pixel primitives in `test_simd_packed.c`.
The `test_simd256_emulated` and `test_simd512_emulated` targets run those packed
checks on all platforms.

| Target | Native test suites |
|---|---|
| x86-64 Windows/Linux/macOS/Android | `test_simd` (SSE3), `test_simd_sse42`, `test_simd_avx2`, `test_simd_avx512` |
| ARM macOS/iOS/Android/Linux | `test_simd` (NEON) |
| Wasm/web, including pthread | `test_simd` (SIMD128) |

Run them with the corresponding CMake `run_<target>` targets. Native runners
compile at the platform baseline, check CPU/OS support and print `SKIP` before
calling an unsupported ISA suite. The suites are isolated objects with LTO
disabled. Running the AVX-512 suites requires AVX-512F, DQ, BW and VL plus OS
register-state support. The Wasm Node/Bun runner probes SIMD support before
module instantiation. Scalar Wasm tests compile and link without SIMD128. Texc
uses the same reference to test both AVX kernels on machines without native AVX
support.
