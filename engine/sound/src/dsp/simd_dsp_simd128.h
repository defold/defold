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

#ifndef DM_SIMD_DSP_SIMD128_H
#define DM_SIMD_DSP_SIMD128_H

#include "sound_dsp.h"
#include <simd/simd_128.h>

namespace dmSound
{
    namespace Dsp
    {
        // FIR8 is the tap count; the suffix identifies the SIMD arithmetic width.
        static inline dsimd_float4_t FilterSampleFIR8Partial_Simd128(const float* frames, uint64_t frac_pos)
        {
            uint32_t pi = ((frac_pos >> (RESAMPLE_FRACTION_BITS - 11)) << 3) & (2047 << 3);
            dsimd_float4_t lo = Simd128Mul(Simd128Load(frames - 3), Simd128Load(&_pfb[pi]));
            dsimd_float4_t hi = Simd128Mul(Simd128Load(frames + 1), Simd128Load(&_pfb[pi + 4]));
            return Simd128Add(lo, hi);
        }

        static inline float FilterSampleFIR8_Simd128(const float* frames, uint64_t frac_pos)
        {
            return Simd128Sum(FilterSampleFIR8Partial_Simd128(frames, frac_pos));
        }

        // Leave four partial sums per channel so consecutive outputs can be reduced together.
        static inline void FilterStereoFIR8Partial_Simd128(const float* in_l, const float* in_r, uint64_t frac, dsimd_float4_t* left, dsimd_float4_t* right)
        {
            uint32_t pi = ((frac >> (RESAMPLE_FRACTION_BITS - 11)) << 3) & (2047 << 3);
            uint64_t index = frac >> RESAMPLE_FRACTION_BITS;
            dsimd_float4_t c0 = Simd128Load(&_pfb[pi]);
            dsimd_float4_t c1 = Simd128Load(&_pfb[pi + 4]);
            *left = Simd128Add(Simd128Mul(Simd128Load(in_l + index - 3), c0), Simd128Mul(Simd128Load(in_l + index + 1), c1));
            *right = Simd128Add(Simd128Mul(Simd128Load(in_r + index - 3), c0), Simd128Mul(Simd128Load(in_r + index + 1), c1));
        }

        // Both channels use the same phase and coefficients. Preserve the FIR reduction order.
        static inline void FilterStereoFIR8_Simd128(const float* in_l, const float* in_r, uint64_t frac, float* sample_l, float* sample_r)
        {
            dsimd_float4_t l, r;
            FilterStereoFIR8Partial_Simd128(in_l, in_r, frac, &l, &r);
            *sample_l = Simd128Sum(l);
            *sample_r = Simd128Sum(r);
        }

    } // namespace Dsp
} // namespace dmSound

#endif // DM_SIMD_DSP_SIMD128_H
