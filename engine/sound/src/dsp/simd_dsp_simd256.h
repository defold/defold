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

#ifndef DM_SIMD_DSP_SIMD256_H
#define DM_SIMD_DSP_SIMD256_H

#include "sound_dsp.h"
#include <simd/simd_256.h>

#if !DM_SIMD_FORCE_SCALAR && defined(DM_SIMD_AVX2)

namespace dmSound
{
    namespace Dsp
    {
        static inline float FilterSampleFIR8_Simd256(const float* frames, uint64_t frac_pos)
        {
            uint32_t pi = ((frac_pos >> (RESAMPLE_FRACTION_BITS - 11)) << 3) & (2047 << 3);
            return Simd256Sum(Simd256Mul(Simd256Load(frames - 3), Simd256Load(&_pfb[pi])));
        }

        // Keep the tap products in registers so several FIR outputs can be reduced together.
        static inline void FilterStereoFIR8Products_Simd256(const float* in_l, const float* in_r, uint64_t frac, dsimd_float8_t* left, dsimd_float8_t* right)
        {
            uint32_t pi = ((frac >> (RESAMPLE_FRACTION_BITS - 11)) << 3) & (2047 << 3);
            uint64_t index = frac >> RESAMPLE_FRACTION_BITS;
            dsimd_float8_t coefficients = Simd256Load(&_pfb[pi]);
            *left = Simd256Mul(Simd256Load(in_l + index - 3), coefficients);
            *right = Simd256Mul(Simd256Load(in_r + index - 3), coefficients);
        }

        // Both channels use the same phase and coefficients. Preserve the FIR reduction order.
        static inline void FilterStereoFIR8_Simd256(const float* in_l, const float* in_r, uint64_t frac, float* sample_l, float* sample_r)
        {
            uint32_t pi = ((frac >> (RESAMPLE_FRACTION_BITS - 11)) << 3) & (2047 << 3);
            uint64_t index = frac >> RESAMPLE_FRACTION_BITS;
            dsimd_float8_t coefficients = Simd256Load(&_pfb[pi]);
            *sample_l = Simd256Sum(Simd256Mul(Simd256Load(in_l + index - 3), coefficients));
            *sample_r = Simd256Sum(Simd256Mul(Simd256Load(in_r + index - 3), coefficients));
        }

    } // namespace Dsp
} // namespace dmSound

#endif

#endif // DM_SIMD_DSP_SIMD256_H
