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

#if defined(DM_SIMD_SME_H)
#define DM_SOUND_DSP_STREAMING __arm_streaming
#define DM_SOUND_DSP_INLINE __attribute__((always_inline)) static inline
#define DM_SOUND_DSP_LOCALLY_STREAMING __arm_locally_streaming
#else
#define DM_SOUND_DSP_STREAMING
#define DM_SOUND_DSP_INLINE static inline
#define DM_SOUND_DSP_LOCALLY_STREAMING
#endif

namespace dmSound
{
    namespace DM_SOUND_DSP_NAMESPACE
    {
#if DM_SOUND_DSP_LANES == 4
        typedef dsimd_float4_t Vec;
#elif DM_SOUND_DSP_LANES == 8
        typedef dsimd_float8_t Vec;
#elif DM_SOUND_DSP_LANES == 16
        typedef dsimd_float16_t Vec;
#endif

        // FIR8 is the tap count; the suffix identifies the SIMD arithmetic width.
#if !defined(DM_SIMD_SME_H)
        DM_SOUND_DSP_INLINE float FilterSampleFIR8_Simd8(const float* frames, uint64_t frac_pos) DM_SOUND_DSP_STREAMING
        {
            uint32_t pi = ((frac_pos >> (RESAMPLE_FRACTION_BITS - 11)) << 3) & (2047 << 3);
            return Simd8Sum(Simd8Mul(Simd8Load(frames - 3), Simd8Load(&_pfb[pi])));
        }

        // Both channels use the same phase and coefficients. Preserve the FIR reduction order.
        DM_SOUND_DSP_INLINE void FilterStereoFIR8_Simd4(const float* in_l, const float* in_r, uint64_t frac, float* sample_l, float* sample_r) DM_SOUND_DSP_STREAMING
        {
            uint32_t pi = ((frac >> (RESAMPLE_FRACTION_BITS - 11)) << 3) & (2047 << 3);
            uint64_t index = frac >> RESAMPLE_FRACTION_BITS;
            dsimd_float4_t c0 = Simd4Load(&_pfb[pi]);
            dsimd_float4_t c1 = Simd4Load(&_pfb[pi + 4]);
            dsimd_float4_t l = Simd4Add(Simd4Mul(Simd4Load(in_l + index - 3), c0), Simd4Mul(Simd4Load(in_l + index + 1), c1));
            dsimd_float4_t r = Simd4Add(Simd4Mul(Simd4Load(in_r + index - 3), c0), Simd4Mul(Simd4Load(in_r + index + 1), c1));
            dsimd_float4_t pairs = Simd4PairwiseAdd(l, r);
            dsimd_float4_t sums = Simd4PairwiseAdd(pairs, pairs);
            *sample_l = Simd4Lane(sums, 0);
            *sample_r = Simd4Lane(sums, 1);
        }

        // Both channels use the same phase and coefficients. Preserve the FIR reduction order.
        DM_SOUND_DSP_INLINE void FilterStereoFIR8_Simd8(const float* in_l, const float* in_r, uint64_t frac, float* sample_l, float* sample_r) DM_SOUND_DSP_STREAMING
        {
            uint32_t pi = ((frac >> (RESAMPLE_FRACTION_BITS - 11)) << 3) & (2047 << 3);
            uint64_t index = frac >> RESAMPLE_FRACTION_BITS;
            dsimd_float8_t coefficients = Simd8Load(&_pfb[pi]);
            *sample_l = Simd8Sum(Simd8Mul(Simd8Load(in_l + index - 3), coefficients));
            *sample_r = Simd8Sum(Simd8Mul(Simd8Load(in_r + index - 3), coefficients));
        }

#else
        DM_SOUND_DSP_INLINE float FilterSampleFIR8_Simd16(const float* frames, uint64_t frac_pos) DM_SOUND_DSP_STREAMING
        {
            uint32_t pi = ((frac_pos >> (RESAMPLE_FRACTION_BITS - 11)) << 3) & (2047 << 3);
            return Simd16Sum(Simd16Mul(Simd16LoadPartial(frames - 3, 8), Simd16LoadPartial(&_pfb[pi], 8)));
        }

        DM_SOUND_DSP_INLINE void FilterStereoFIR8_Simd16(const float* in_l, const float* in_r, uint64_t frac, float* sample_l, float* sample_r) DM_SOUND_DSP_STREAMING
        {
            uint32_t pi = ((frac >> (RESAMPLE_FRACTION_BITS - 11)) << 3) & (2047 << 3);
            uint64_t index = frac >> RESAMPLE_FRACTION_BITS;
            dsimd_float16_t coefficients = Simd16LoadPartial(&_pfb[pi], 8);
            *sample_l = Simd16Sum(Simd16Mul(Simd16LoadPartial(in_l + index - 3, 8), coefficients));
            *sample_r = Simd16Sum(Simd16Mul(Simd16LoadPartial(in_r + index - 3, 8), coefficients));
        }

#endif
    } // namespace DM_SOUND_DSP_NAMESPACE
} // namespace dmSound

#endif // DM_SOUND_DSP_SIMD_H
