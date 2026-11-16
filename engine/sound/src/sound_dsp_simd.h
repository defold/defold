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

        DM_SOUND_DSP_INLINE float FilterSampleFIR8(const float* frames, uint64_t frac_pos) DM_SOUND_DSP_STREAMING
        {
            uint32_t pi = ((frac_pos >> (RESAMPLE_FRACTION_BITS - 11)) << 3) & (2047 << 3);
#if defined(DM_SIMD_SME_H)
            return Simd16Sum(Simd16Mul(Simd16LoadPartial(frames - 3, 8), Simd16LoadPartial(&_pfb[pi], 8)));
#else
            return Simd8Sum(Simd8Mul(Simd8Load(frames - 3), Simd8Load(&_pfb[pi])));
#endif
        }

#if defined(_MSC_VER) && !defined(__clang__)
        DM_SOUND_DSP_INLINE dsimd_float4_t Resample4(const float* in, uint64_t frac, uint64_t delta)
        {
            // Explicit construction keeps distinct FIR results in every lane on MSVC.
            float s0 = FilterSampleFIR8(in + (frac >> RESAMPLE_FRACTION_BITS), frac);
            frac += delta;
            float s1 = FilterSampleFIR8(in + (frac >> RESAMPLE_FRACTION_BITS), frac);
            frac += delta;
            float s2 = FilterSampleFIR8(in + (frac >> RESAMPLE_FRACTION_BITS), frac);
            frac += delta;
            float s3 = FilterSampleFIR8(in + (frac >> RESAMPLE_FRACTION_BITS), frac);
            return Simd4Make(s0, s1, s2, s3);
        }

#endif

        DM_SOUND_DSP_INLINE Vec Resample(const float* in, uint64_t frac, uint64_t delta) DM_SOUND_DSP_STREAMING
        {
#if defined(_MSC_VER) && !defined(__clang__)
#if DM_SOUND_DSP_LANES == 4
            return Resample4(in, frac, delta);
#elif DM_SOUND_DSP_LANES == 8
            return Simd8Combine(Resample4(in, frac, delta), Resample4(in, frac + delta * 4, delta));
#else
            return Simd16Combine(Resample4(in, frac, delta), Resample4(in, frac + delta * 4, delta),
                                 Resample4(in, frac + delta * 8, delta), Resample4(in, frac + delta * 12, delta));
#endif
#else
            // LLVM schedules the independent FIR samples faster from this loop.
            float samples[DM_SOUND_DSP_LANES];
            for (unsigned i = 0; i < DM_SOUND_DSP_LANES; ++i)
            {
                samples[i] = FilterSampleFIR8(in + (frac >> RESAMPLE_FRACTION_BITS), frac);
                frac += delta;
            }

#if DM_SOUND_DSP_LANES == 4
            return Simd4Load(samples);
#elif DM_SOUND_DSP_LANES == 8
            return Simd8Load(samples);
#else
            return Simd16Load(samples);
#endif
#endif
        }

        // Both channels use the same phase and coefficients. Preserve the FIR reduction order.
        DM_SOUND_DSP_INLINE void FilterStereoFIR8(const float* in_l, const float* in_r, uint64_t frac, float* sample_l, float* sample_r) DM_SOUND_DSP_STREAMING
        {
            uint32_t pi = ((frac >> (RESAMPLE_FRACTION_BITS - 11)) << 3) & (2047 << 3);
            uint64_t index = frac >> RESAMPLE_FRACTION_BITS;
#if defined(DM_SIMD_SME_H)
            dsimd_float16_t coefficients = Simd16LoadPartial(&_pfb[pi], 8);
            *sample_l = Simd16Sum(Simd16Mul(Simd16LoadPartial(in_l + index - 3, 8), coefficients));
            *sample_r = Simd16Sum(Simd16Mul(Simd16LoadPartial(in_r + index - 3, 8), coefficients));
#elif defined(DM_SIMD_NEON_H)
            dsimd_float4_t c0 = Simd4Load(&_pfb[pi]);
            dsimd_float4_t c1 = Simd4Load(&_pfb[pi + 4]);
            dsimd_float4_t l = Simd4Add(Simd4Mul(Simd4Load(in_l + index - 3), c0), Simd4Mul(Simd4Load(in_l + index + 1), c1));
            dsimd_float4_t r = Simd4Add(Simd4Mul(Simd4Load(in_r + index - 3), c0), Simd4Mul(Simd4Load(in_r + index + 1), c1));
            dsimd_float4_t pairs = Simd4PairwiseAdd(l, r);
            dsimd_float4_t sums = Simd4PairwiseAdd(pairs, pairs);
            *sample_l = Simd4Lane(sums, 0);
            *sample_r = Simd4Lane(sums, 1);
#else
            dsimd_float8_t coefficients = Simd8Load(&_pfb[pi]);
            *sample_l = Simd8Sum(Simd8Mul(Simd8Load(in_l + index - 3), coefficients));
            *sample_r = Simd8Sum(Simd8Mul(Simd8Load(in_r + index - 3), coefficients));
#endif
        }

#if defined(_MSC_VER) && !defined(__clang__)
        // MSVC needs explicit construction to retain distinct FIR results in every lane.
        DM_SOUND_DSP_INLINE void ResampleStereo4(const float* in_l, const float* in_r, uint64_t frac, uint64_t delta, dsimd_float4_t* left, dsimd_float4_t* right)
        {
            float l0, r0, l1, r1, l2, r2, l3, r3;
            FilterStereoFIR8(in_l, in_r, frac, &l0, &r0);
            FilterStereoFIR8(in_l, in_r, frac + delta, &l1, &r1);
            FilterStereoFIR8(in_l, in_r, frac + delta * 2, &l2, &r2);
            FilterStereoFIR8(in_l, in_r, frac + delta * 3, &l3, &r3);
            *left = Simd4Make(l0, l1, l2, l3);
            *right = Simd4Make(r0, r1, r2, r3);
        }

#endif

        DM_SOUND_DSP_INLINE void ResampleStereo(const float* in_l, const float* in_r, uint64_t frac, uint64_t delta, Vec* left, Vec* right) DM_SOUND_DSP_STREAMING
        {
#if defined(DM_SIMD_X86_H) && !defined(DM_SIMD_AVX2)
            // Narrow x86 kernels retain the faster independent FIR schedule.
            *left = Resample(in_l, frac, delta);
            *right = Resample(in_r, frac, delta);
#elif defined(_MSC_VER) && !defined(__clang__)
            dsimd_float4_t l0, r0;
            ResampleStereo4(in_l, in_r, frac, delta, &l0, &r0);
#if DM_SOUND_DSP_LANES == 4
            *left = l0;
            *right = r0;
#else
            dsimd_float4_t l1, r1;
            ResampleStereo4(in_l, in_r, frac + delta * 4, delta, &l1, &r1);
#if DM_SOUND_DSP_LANES == 8
            *left = Simd8Combine(l0, l1);
            *right = Simd8Combine(r0, r1);
#else
            dsimd_float4_t l2, r2, l3, r3;
            ResampleStereo4(in_l, in_r, frac + delta * 8, delta, &l2, &r2);
            ResampleStereo4(in_l, in_r, frac + delta * 12, delta, &l3, &r3);
            *left = Simd16Combine(l0, l1, l2, l3);
            *right = Simd16Combine(r0, r1, r2, r3);
#endif
#endif
#else
            float samples_l[DM_SOUND_DSP_LANES], samples_r[DM_SOUND_DSP_LANES];
            for (unsigned i = 0; i < DM_SOUND_DSP_LANES; ++i)
            {
                FilterStereoFIR8(in_l, in_r, frac, &samples_l[i], &samples_r[i]);
                frac += delta;
            }

#if DM_SOUND_DSP_LANES == 4
            *left = Simd4Load(samples_l);
            *right = Simd4Load(samples_r);
#elif DM_SOUND_DSP_LANES == 8
            *left = Simd8Load(samples_l);
            *right = Simd8Load(samples_r);
#else
            *left = Simd16Load(samples_l);
            *right = Simd16Load(samples_r);
#endif
#endif
        }

    } // namespace DM_SOUND_DSP_NAMESPACE
} // namespace dmSound

#endif // DM_SOUND_DSP_SIMD_H
