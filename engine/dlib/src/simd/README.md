# Internal SIMD primitives

Include `<simd/simd.h>` for the C register types and `Simd*` functions. The
header lists the API and its contracts; each backend has one implementation
header. These headers are internal and are not installed in dmsdk.

Compiler capabilities select scalar, SSE3/SSE4.2, NEON or Wasm SIMD128.
`DM_SIMD_FORCE_SCALAR=1` forces scalar primitives without changing capability
reporting. Division and square root use accurate operations.

`SimdWide*` operations use AVX2 (32 bytes), AVX-512F/BW (64 bytes), or compose
128-bit registers. `DM_SIMD_WIDE_BYTES` can select 32 or 64 bytes explicitly.
Shuffles and widening/packing operate within consecutive 16-byte blocks.
Register types are temporary computation values: do not pass them between
objects compiled with different backend flags.

`SimdGetVersion()` reports CPU and OS support, including XSAVE/OSXSAVE and XCR0
for AVX. Intel macOS also checks OS support and requires macOS 12.2 for AVX-512
register preservation, following Highway's platform handling. It does not
change the inline primitive implementation. Texc builds isolated kernels and
selects its internal function pointers once at library load; its dispatcher
retains the SSE2 baseline. ARM64 selects NEON. Both retain scalar fallbacks.
Texc's scalar, SSE, AVX and NEON kernels live in separate `texc_simd_*.cpp`
files. Scalar is the runtime fallback and uses ordinary loops. The shared
resizer in `texc_resize.cpp` owns coefficients and scratch storage; backends
provide complete-row kernels, with common SIMD math in `texc_simd_resize.cpp`.
`texc_resize.h` contains only private types and declarations. CMake selects ISA
flags without renaming functions.

Every SIMD test target runs `test_simd.c` and `test_simd_simde.c`: independent
numerical/pipeline checks and comparisons of all 107 functions with the small
[in-repo SIMDe scalar reference](../test/simde/simde.h). No external checkout
is needed. Scalar and portable wide64 tests run on every platform.

| Target | Native test suites |
|---|---|
| x86-64 Windows/Linux/macOS/Android | `test_simd` (SSE3), `test_simd_sse42`, `test_simd_avx2`, `test_simd_avx512` |
| ARM macOS/iOS/Android/Linux | `test_simd` (NEON) |
| Wasm/web, including pthread | `test_simd` (SIMD128) |

Run them with the corresponding CMake `run_<target>` targets. Native runners
compile at the platform baseline, check CPU/OS support and print `SKIP` before
calling an unsupported ISA suite. The suites are isolated objects with LTO
disabled. AVX-512 requires both F and BW plus OS register-state support. The
Wasm Node/Bun runner probes SIMD support before module instantiation; scalar
Wasm tests compile and link without SIMD128. Texc also uses this reference to
test emulated AVX kernels on machines without native AVX support.
