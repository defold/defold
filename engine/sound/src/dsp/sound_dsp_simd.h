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

#ifndef DM_SOUND_DSP_SIMD_H
#define DM_SOUND_DSP_SIMD_H

#include "sound_dsp.h"
#include <simd/simd_fixed_float.h>

namespace dmSound
{
    namespace Dsp
    {
        // FIR8 is the tap count; the suffix identifies the SIMD arithmetic width.
        static inline float FilterSampleFIR8_Simd4(const float* frames, uint64_t frac_pos)
        {
            uint32_t pi = ((frac_pos >> (RESAMPLE_FRACTION_BITS - 11)) << 3) & (2047 << 3);
            dsimd_float4_t lo = Simd4Mul(Simd4Load(frames - 3), Simd4Load(&_pfb[pi]));
            dsimd_float4_t hi = Simd4Mul(Simd4Load(frames + 1), Simd4Load(&_pfb[pi + 4]));
            return Simd4Sum(Simd4Add(lo, hi));
        }

        // Leave four partial sums per channel so consecutive outputs can be reduced together.
        static inline void FilterStereoFIR8Partial_Simd4(const float* in_l, const float* in_r, uint64_t frac, dsimd_float4_t* left, dsimd_float4_t* right)
        {
            uint32_t pi = ((frac >> (RESAMPLE_FRACTION_BITS - 11)) << 3) & (2047 << 3);
            uint64_t index = frac >> RESAMPLE_FRACTION_BITS;
            dsimd_float4_t c0 = Simd4Load(&_pfb[pi]);
            dsimd_float4_t c1 = Simd4Load(&_pfb[pi + 4]);
            *left = Simd4Add(Simd4Mul(Simd4Load(in_l + index - 3), c0), Simd4Mul(Simd4Load(in_l + index + 1), c1));
            *right = Simd4Add(Simd4Mul(Simd4Load(in_r + index - 3), c0), Simd4Mul(Simd4Load(in_r + index + 1), c1));
        }

        // Both channels use the same phase and coefficients. Preserve the FIR reduction order.
        static inline void FilterStereoFIR8_Simd4(const float* in_l, const float* in_r, uint64_t frac, float* sample_l, float* sample_r)
        {
            dsimd_float4_t l, r;
            FilterStereoFIR8Partial_Simd4(in_l, in_r, frac, &l, &r);
            *sample_l = Simd4Sum(l);
            *sample_r = Simd4Sum(r);
        }

#if !DM_SIMD_FORCE_SCALAR && defined(DM_SIMD_AVX2)
        static inline float FilterSampleFIR8_Simd8(const float* frames, uint64_t frac_pos)
        {
            uint32_t pi = ((frac_pos >> (RESAMPLE_FRACTION_BITS - 11)) << 3) & (2047 << 3);
            return Simd8Sum(Simd8Mul(Simd8Load(frames - 3), Simd8Load(&_pfb[pi])));
        }

        // Keep the tap products in registers so several FIR outputs can be reduced together.
        static inline void FilterStereoFIR8Products_Simd8(const float* in_l, const float* in_r, uint64_t frac, dsimd_float8_t* left, dsimd_float8_t* right)
        {
            uint32_t pi = ((frac >> (RESAMPLE_FRACTION_BITS - 11)) << 3) & (2047 << 3);
            uint64_t index = frac >> RESAMPLE_FRACTION_BITS;
            dsimd_float8_t coefficients = Simd8Load(&_pfb[pi]);
            *left = Simd8Mul(Simd8Load(in_l + index - 3), coefficients);
            *right = Simd8Mul(Simd8Load(in_r + index - 3), coefficients);
        }

        // Both channels use the same phase and coefficients. Preserve the FIR reduction order.
        static inline void FilterStereoFIR8_Simd8(const float* in_l, const float* in_r, uint64_t frac, float* sample_l, float* sample_r)
        {
            uint32_t pi = ((frac >> (RESAMPLE_FRACTION_BITS - 11)) << 3) & (2047 << 3);
            uint64_t index = frac >> RESAMPLE_FRACTION_BITS;
            dsimd_float8_t coefficients = Simd8Load(&_pfb[pi]);
            *sample_l = Simd8Sum(Simd8Mul(Simd8Load(in_l + index - 3), coefficients));
            *sample_r = Simd8Sum(Simd8Mul(Simd8Load(in_r + index - 3), coefficients));
        }

#endif

#if !DM_SIMD_FORCE_SCALAR && defined(DM_SIMD_AVX512)
        static inline float FilterSampleFIR8_Simd16(const float* frames, uint64_t frac_pos)
        {
            uint32_t pi = ((frac_pos >> (RESAMPLE_FRACTION_BITS - 11)) << 3) & (2047 << 3);
            return Simd16Sum(Simd16Mul(Simd16LoadPartial(frames - 3, 8), Simd16LoadPartial(&_pfb[pi], 8)));
        }

        static inline void FilterStereoFIR8_Simd16(const float* in_l, const float* in_r, uint64_t frac, float* sample_l, float* sample_r)
        {
            uint32_t pi = ((frac >> (RESAMPLE_FRACTION_BITS - 11)) << 3) & (2047 << 3);
            uint64_t index = frac >> RESAMPLE_FRACTION_BITS;
            dsimd_float16_t coefficients = Simd16LoadPartial(&_pfb[pi], 8);
            *sample_l = Simd16Sum(Simd16Mul(Simd16LoadPartial(in_l + index - 3, 8), coefficients));
            *sample_r = Simd16Sum(Simd16Mul(Simd16LoadPartial(in_r + index - 3, 8), coefficients));
        }

#endif

    } // namespace Dsp
} // namespace dmSound

#endif // DM_SOUND_DSP_SIMD_H
