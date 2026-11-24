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

#include <simd/simd_runtime.h>
#include <stdio.h>

// Runs independent numerical, boundary and pipeline checks for the compiled backend.
int SimdTestPrimitives(void);
// Compares every primitive function with the portable SIMDe implementation.
int SimdTestReference(void);

// Keep this translation unit at the platform baseline; the suites use their own ISA flags.
static int IsSupported(dsimd_version_t required, dsimd_version_t available)
{
    if (required == DSIMD_VERSION_SCALAR)
        return 1;
    if (required == DSIMD_VERSION_NEON || required == DSIMD_VERSION_WASM_SIMD128)
        return available == required;
    if ((available >= DSIMD_VERSION_SSE2 && available <= DSIMD_VERSION_SSE42) ||
        available == DSIMD_VERSION_AVX2 || available == DSIMD_VERSION_AVX512)
        return available >= required;
    return 0;
}

int main(void)
{
    const dsimd_version_t required = DM_SIMD_TEST_VERSION;
    dsimd_version_t       available = SimdGetVersion();
#if defined(__wasm__)
    // The JS feature probe skips unsupported hosts before module instantiation.
    if (required == DSIMD_VERSION_WASM_SIMD128)
        available = DSIMD_VERSION_WASM_SIMD128;
#endif
    if (!IsSupported(required, available))
    {
        printf("SKIP: %s is unsupported by this CPU/OS\n", DM_SIMD_TEST_NAME);
        return 0;
    }
    int primitives = SimdTestPrimitives();
    int reference = SimdTestReference();
    return primitives || reference;
}
