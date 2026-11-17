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
#ifdef __cplusplus
extern "C"
{
#endif

    /*# Supported SIMD instruction set families.
     * SSE versions form an ascending capability chain; NEON and Wasm are separate
     * families. AVX-512 requires Foundation, Dword/Qword, Byte/Word and Vector
     * Length subsets, including instructions emitted by compiler lowering. Capability
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
     * Wasm probes host SIMD support; unsupported SIMD modules must still be
     * rejected/selected by the host before instantiation. Consumers must compile
     * their dispatcher to the platform baseline and isolate higher-ISA kernels.
     */
    dsimd_version_t SimdGetVersion(void);

    /*# Get the display name of a SIMD version for diagnostics.
     * @name SimdGetVersionName
     * @param version [type:dsimd_version_t] selected instruction set
     * @return name [type:const char*] static string; never freed by the caller
     */
    const char* SimdGetVersionName(dsimd_version_t version);

    /*# Get the native number of 32-bit lanes for a SIMD version.
     * This describes register width, not CPU support or consumer loop unrolling.
     * @name SimdGetLaneCount
     * @param version [type:dsimd_version_t] instruction set
     * @return lanes [type:uint32_t] 1 for scalar, 4, 8 or 16 for SIMD; 0 for an unknown version
     */
    uint32_t SimdGetLaneCount(dsimd_version_t version);

#ifdef __cplusplus
}
#endif
#endif // DM_SIMD_RUNTIME_H
