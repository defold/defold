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

// Include the implementation to exercise private feature decoders on every host.
#define DM_TEST_SIMD 1
#include "../simd/simd.cpp"
#include <stdio.h>

// Missing CPU features or OS register state must reject AVX before any kernel executes.
static int CheckAvxDetection(void)
{
    const uint32_t sse = 1u | (1u << 9) | (1u << 19) | (1u << 20);
    const uint32_t state = (1u << 26) | (1u << 27) | (1u << 28);
    const uint32_t edx = (1u << 24) | (1u << 25) | (1u << 26);
    const uint32_t avx2 = 1u << 5;
    const uint32_t avx512 = avx2 | (1u << 16) | (1u << 17) | (1u << 30) | (1u << 31);
    unsigned       bit;
    if (SimdInternalRuntimeVersionX86(sse | state, edx, avx2, 6) != DSIMD_VERSION_AVX2 ||
        SimdInternalRuntimeVersionX86(sse | state, edx, avx512, 0xe6) != DSIMD_VERSION_AVX512)
    {
        return 0;
    }

    for (bit = 26; bit <= 28; ++bit)
    {
        if (SimdInternalRuntimeVersionX86((sse | state) & ~(1u << bit), edx, avx512, 0xe6) != DSIMD_VERSION_SSE42)
        {
            return 0;
        }
    }

    if (SimdInternalRuntimeVersionX86(sse | state, edx, avx512, 0) != DSIMD_VERSION_SSE42 ||
        SimdInternalRuntimeVersionX86(sse | state, edx, avx512, 2) != DSIMD_VERSION_SSE42 ||
        SimdInternalRuntimeVersionX86(sse | state, edx, avx512, 4) != DSIMD_VERSION_SSE42 ||
        SimdInternalRuntimeVersionX86(sse | state, edx, avx512 & ~avx2, 0xe6) != DSIMD_VERSION_SSE42)
    {
        return 0;
    }

    for (bit = 5; bit <= 7; ++bit)
    {
        if (SimdInternalRuntimeVersionX86(sse | state, edx, avx512, 0xe6u & ~(1u << bit)) != DSIMD_VERSION_AVX2)
        {
            return 0;
        }
    }

    if (SimdInternalRuntimeVersionX86(sse | state, edx, avx512 & ~(1u << 16), 0xe6) != DSIMD_VERSION_AVX2 ||
        SimdInternalRuntimeVersionX86(sse | state, edx, avx512 & ~(1u << 17), 0xe6) != DSIMD_VERSION_AVX2 ||
        SimdInternalRuntimeVersionX86(sse | state, edx, avx512 & ~(1u << 30), 0xe6) != DSIMD_VERSION_AVX2 ||
        SimdInternalRuntimeVersionX86(sse | state, edx, avx512 & ~(1u << 31), 0xe6) != DSIMD_VERSION_AVX2)
    {
        return 0;
    }

    if (SimdInternalDarwinAvx512State(0xe6, 21, 2, 1) != 6 ||
        SimdInternalDarwinAvx512State(6, 21, 3, 1) != 0xe6 ||
        SimdInternalDarwinAvx512State(6, 22, 0, 1) != 0xe6 ||
        SimdInternalDarwinAvx512State(0xe6, 22, 0, 0) != 6 ||
        SimdInternalDarwinAvx512State(0, 22, 0, 1) != 0)
    {
        return 0;
    }

    if (SimdInternalRuntimeVersionX86(UINT32_MAX, 0, UINT32_MAX, UINT64_MAX) != DSIMD_VERSION_SCALAR)
    {
        return 0;
    }

    if (SimdInternalRuntimeVersionX86(1u << 20, (1u << 24) | (1u << 25) | (1u << 26), 0, 0) != DSIMD_VERSION_SSE2)
    {
        return 0;
    }

    return 1;
}

// Native width describes one register, regardless of the current host or audio unrolling.
static int CheckLaneCounts(void)
{
    const uint32_t lanes[] = { 1, 4, 4, 4, 4, 4, 4, 4, 8, 16 };
    for (unsigned version = 0; version < sizeof(lanes) / sizeof(lanes[0]); ++version)
    {
        if (SimdGetLaneCount((dsimd_version_t)version) != lanes[version])
        {
            return 0;
        }
    }

    return SimdGetLaneCount((dsimd_version_t)10) == 0;
}

int main()
{
    int passed = CheckAvxDetection() && CheckLaneCounts();
    printf("SIMD CPU/OS capability combinations and native widths: %s\n", passed ? "PASS" : "FAIL");
    return passed ? 0 : 1;
}
