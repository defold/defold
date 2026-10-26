// Copyright 2020-2026 The Defold Foundation
// Copyright 2014-2020 King
// Copyright 2009-2014 Ragnar Svensson, Christian Murray
// Licensed under the Defold License version 1.0 (the "License"); you may not use
// this file except in compliance with the License.
//
// You may obtain a copy of the License, together with FAQs at
// https://www.defold.com/license
//
// Unless required by applicable law or agreed to in writing, software distributed
// under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
// CONDITIONS OF ANY KIND, either express or implied. See the License for the
// specific language governing permissions and limitations under the License.

#ifndef DM_SIMD_RUNTIME_H
#define DM_SIMD_RUNTIME_H

#include <stdint.h>
#if defined(__APPLE__)
#include <sys/sysctl.h>
#endif
#if defined(__APPLE__) && (defined(__i386__) || defined(__x86_64__))
#include <stdlib.h>
#endif
#if defined(__i386__) || defined(__x86_64__) || defined(_M_IX86) || defined(_M_X64)
#if defined(_MSC_VER)
#include <intrin.h>
#else
#include <cpuid.h>
#endif
#elif (defined(__arm__) || defined(__aarch64__)) && defined(__linux__)
#include <sys/auxv.h>
#endif

#ifdef __cplusplus
extern "C"
{
#endif

    /*# Supported SIMD instruction set families.
     * SSE versions form an ascending capability chain; NEON and Wasm are separate
     * families. AVX-512 requires the Foundation and Byte/Word subsets. Capability
     * reporting does not change compile-time backend selection or register types.
     * @enum
     * @name dsimd_version_t
     */
    typedef enum dsimd_version_t
    {
        DSIMD_VERSION_SCALAR,
        DSIMD_VERSION_SSE2,
        DSIMD_VERSION_SSE3,
        DSIMD_VERSION_SSSE3,
        DSIMD_VERSION_SSE41,
        DSIMD_VERSION_SSE42,
        DSIMD_VERSION_NEON,
        DSIMD_VERSION_WASM_SIMD128,
        DSIMD_VERSION_AVX2,
        DSIMD_VERSION_AVX512
    } dsimd_version_t;

    /*# Query the highest SIMD family supported by both the CPU and OS at runtime.
     * @name SimdGetVersion
     * @return version [type:dsimd_version_t] CPU/OS capability, independent of
     * DM_SIMD_FORCE_SCALAR and the compiled consumer backend.
     * Wasm reports the loaded module's feature set; unsupported SIMD modules must
     * be rejected/selected by the host before instantiation. Consumers must compile
     * their dispatcher to the platform baseline and isolate higher-ISA kernels.
     */
    static inline dsimd_version_t SimdGetVersion(void);

    // Darwin's AVX-512 state is lazy; older kernels must stay on AVX2 even if XCR0 is set.
    static inline uint64_t SimdInternalDarwinAvx512State(uint64_t xcr0, uint32_t major, uint32_t minor, int available)
    {
        if (!available || major < 21 || (major == 21 && minor < 3))
            return xcr0 & ~(uint64_t)0xe0u;
        return (xcr0 & 6u) == 6u ? xcr0 | 0xe0u : xcr0;
    }

    // Internal decoder, separated from CPUID to exercise conservative feature combinations.
    static inline dsimd_version_t SimdInternalRuntimeVersionX86(uint32_t ecx, uint32_t edx, uint32_t leaf7_ebx, uint64_t xcr0)
    {
        const uint32_t sse2 = (1u << 24) | (1u << 25) | (1u << 26); // FXSR, SSE and SSE2.
        const uint32_t sse3 = 1u;
        const uint32_t ssse3 = sse3 | (1u << 9);
        const uint32_t sse41 = ssse3 | (1u << 19);
        const uint32_t sse42 = sse41 | (1u << 20);
        if ((edx & sse2) != sse2)
            return DSIMD_VERSION_SCALAR;
        const uint32_t avx = (1u << 26) | (1u << 27) | (1u << 28); // XSAVE, OSXSAVE, AVX.
        if ((ecx & (sse42 | avx)) == (sse42 | avx) && (xcr0 & 6u) == 6u && (leaf7_ebx & (1u << 5)))
        {
            const uint32_t avx512 = (1u << 16) | (1u << 30); // AVX-512F and AVX-512BW.
            if ((leaf7_ebx & avx512) == avx512 && (xcr0 & 0xe6u) == 0xe6u)
                return DSIMD_VERSION_AVX512;
            return DSIMD_VERSION_AVX2;
        }
        if ((ecx & sse42) == sse42)
            return DSIMD_VERSION_SSE42;
        if ((ecx & sse41) == sse41)
            return DSIMD_VERSION_SSE41;
        if ((ecx & ssse3) == ssse3)
            return DSIMD_VERSION_SSSE3;
        if ((ecx & sse3) == sse3)
            return DSIMD_VERSION_SSE3;
        return DSIMD_VERSION_SSE2;
    }

    static inline dsimd_version_t SimdGetVersion(void)
    {
#if defined(__wasm__)
#if defined(__wasm_simd128__)
        return DSIMD_VERSION_WASM_SIMD128;
#else
        return DSIMD_VERSION_SCALAR;
#endif
#elif defined(__i386__) || defined(__x86_64__) || defined(_M_IX86) || defined(_M_X64)
#if defined(_MSC_VER)
    int registers[4];
    __cpuid(registers, 0);
    if (registers[0] < 1)
        return DSIMD_VERSION_SCALAR;
    __cpuidex(registers, 1, 0);
    uint32_t ecx = (uint32_t)registers[2], edx = (uint32_t)registers[3];
    uint64_t xcr0 = 0;
    uint32_t ebx7 = 0;
    if ((ecx & ((1u << 26) | (1u << 27) | (1u << 28))) == ((1u << 26) | (1u << 27) | (1u << 28)))
        xcr0 = _xgetbv(0);
    __cpuid(registers, 0);
    if (registers[0] >= 7)
    {
        __cpuidex(registers, 7, 0);
        ebx7 = (uint32_t)registers[1];
    }
    return SimdInternalRuntimeVersionX86(ecx, edx, ebx7, xcr0);
#else
    unsigned eax, ebx, ecx, edx;
    if (!__get_cpuid(1, &eax, &ebx, &ecx, &edx))
        return DSIMD_VERSION_SCALAR;
    uint64_t xcr0 = 0;
    uint32_t ebx7 = 0;
    if ((ecx & ((1u << 26) | (1u << 27) | (1u << 28))) == ((1u << 26) | (1u << 27) | (1u << 28)))
    {
        unsigned low, high;
        __asm__ volatile("xgetbv" : "=a"(low), "=d"(high) : "c"(0));
        xcr0 = ((uint64_t)high << 32) | low;
    }
    if (__get_cpuid_max(0, 0) >= 7)
    {
        unsigned eax7, ecx7, edx7;
        __cpuid_count(7, 0, eax7, ebx7, ecx7, edx7);
    }
#if defined(__APPLE__)
    if ((ebx7 & ((1u << 16) | (1u << 30))) == ((1u << 16) | (1u << 30)))
    {
        char     release[32], *end;
        size_t   size = sizeof(release);
        uint32_t major = 0, minor = 0;
        int      available = 0;
        if (sysctlbyname("kern.osrelease", release, &size, 0, 0) == 0)
        {
            major = (uint32_t)strtoul(release, &end, 10);
            if (*end == '.')
                minor = (uint32_t)strtoul(end + 1, 0, 10);
        }
        size = sizeof(available);
        if (sysctlbyname("hw.optional.avx512f", &available, &size, 0, 0) != 0)
            available = 0;
        xcr0 = SimdInternalDarwinAvx512State(xcr0, major, minor, available);
    }
#endif
    return SimdInternalRuntimeVersionX86(ecx, edx, ebx7, xcr0);
#endif
#elif (defined(__aarch64__) || defined(__arm__)) && defined(__linux__)
    unsigned long capabilities = getauxval(AT_HWCAP);
#if defined(__aarch64__)
    return capabilities & (1ul << 1) ? DSIMD_VERSION_NEON : DSIMD_VERSION_SCALAR;
#else
    return capabilities & (1ul << 12) ? DSIMD_VERSION_NEON : DSIMD_VERSION_SCALAR;
#endif
#elif defined(__aarch64__) || defined(_M_ARM64)
    // Apple/Windows ARM64 ABIs guarantee Advanced SIMD register support.
    return DSIMD_VERSION_NEON;
#elif defined(__arm__) && defined(__APPLE__)
    int    available = 0;
    size_t size = sizeof(available);
    return sysctlbyname("hw.optional.neon", &available, &size, 0, 0) == 0 && available ? DSIMD_VERSION_NEON : DSIMD_VERSION_SCALAR;
#else
    return DSIMD_VERSION_SCALAR;
#endif
    }

#ifdef __cplusplus
}
#endif
#endif // DM_SIMD_RUNTIME_H
