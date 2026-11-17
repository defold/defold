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

#ifndef DM_SOUND_DSP_RESAMPLE_MSVC_H
#define DM_SOUND_DSP_RESAMPLE_MSVC_H

#include "sound_dsp_simd.h"

namespace dmSound
{
    namespace Dsp
    {
        // Explicit construction keeps distinct FIR results in every lane on MSVC.
        template <unsigned Unroll>
        static inline void Resample_Simd4(const float* in, uint64_t frac, uint64_t delta, dsimd_float4_t* output)
        {
            for (unsigned u = 0; u < Unroll; ++u)
            {
                float s0 = FilterSampleFIR8_Simd4(in + (frac >> RESAMPLE_FRACTION_BITS), frac);
                frac += delta;
                float s1 = FilterSampleFIR8_Simd4(in + (frac >> RESAMPLE_FRACTION_BITS), frac);
                frac += delta;
                float s2 = FilterSampleFIR8_Simd4(in + (frac >> RESAMPLE_FRACTION_BITS), frac);
                frac += delta;
                float s3 = FilterSampleFIR8_Simd4(in + (frac >> RESAMPLE_FRACTION_BITS), frac);
                output[u] = Simd4Make(s0, s1, s2, s3);
                frac += delta;
            }
        }

        template <unsigned Unroll>
        static inline void ResampleStereo_Simd4(const float* in_l, const float* in_r, uint64_t frac, uint64_t delta, dsimd_float4_t* left, dsimd_float4_t* right)
        {
            for (unsigned u = 0; u < Unroll; ++u)
            {
                dsimd_float4_t l[4], r[4];
                for (unsigned i = 0; i < 4; ++i)
                {
                    FilterStereoFIR8Partial_Simd4(in_l, in_r, frac, &l[i], &r[i]);
                    frac += delta;
                }

                // Keep the original pairwise sum order, with one FIR result in each lane.
                left[u] = Simd4PairwiseAdd(Simd4PairwiseAdd(l[0], l[1]), Simd4PairwiseAdd(l[2], l[3]));
                right[u] = Simd4PairwiseAdd(Simd4PairwiseAdd(r[0], r[1]), Simd4PairwiseAdd(r[2], r[3]));
            }
        }

#if !DM_SIMD_FORCE_SCALAR && defined(DM_SIMD_AVX2)
        static inline dsimd_float8_t Resample_Simd8(const float* in, uint64_t frac, uint64_t delta)
        {
            float s0 = FilterSampleFIR8_Simd8(in + (frac >> RESAMPLE_FRACTION_BITS), frac);
            frac += delta;
            float s1 = FilterSampleFIR8_Simd8(in + (frac >> RESAMPLE_FRACTION_BITS), frac);
            frac += delta;
            float s2 = FilterSampleFIR8_Simd8(in + (frac >> RESAMPLE_FRACTION_BITS), frac);
            frac += delta;
            float s3 = FilterSampleFIR8_Simd8(in + (frac >> RESAMPLE_FRACTION_BITS), frac);
            frac += delta;
            float s4 = FilterSampleFIR8_Simd8(in + (frac >> RESAMPLE_FRACTION_BITS), frac);
            frac += delta;
            float s5 = FilterSampleFIR8_Simd8(in + (frac >> RESAMPLE_FRACTION_BITS), frac);
            frac += delta;
            float s6 = FilterSampleFIR8_Simd8(in + (frac >> RESAMPLE_FRACTION_BITS), frac);
            frac += delta;
            float s7 = FilterSampleFIR8_Simd8(in + (frac >> RESAMPLE_FRACTION_BITS), frac);
            float samples[8] = { s0, s1, s2, s3, s4, s5, s6, s7 };
            return Simd8Load(samples);
        }

        static inline void ResampleStereo_Simd8(const float* in_l, const float* in_r, uint64_t frac, uint64_t delta, dsimd_float8_t* left, dsimd_float8_t* right)
        {
            dsimd_float4_t samples_l[2], samples_r[2];
            for (unsigned group = 0; group < 2; ++group)
            {
                dsimd_float8_t l[4], r[4];
                for (unsigned i = 0; i < 4; ++i)
                {
                    FilterStereoFIR8Products_Simd8(in_l, in_r, frac, &l[i], &r[i]);
                    frac += delta;
                }

                samples_l[group] = Simd8Sum4(l[0], l[1], l[2], l[3]);
                samples_r[group] = Simd8Sum4(r[0], r[1], r[2], r[3]);
            }

            *left = Simd8Combine(samples_l[0], samples_l[1]);
            *right = Simd8Combine(samples_r[0], samples_r[1]);
        }

#endif

#if !DM_SIMD_FORCE_SCALAR && defined(DM_SIMD_AVX512)
        static inline dsimd_float16_t Resample_Simd16(const float* in, uint64_t frac, uint64_t delta)
        {
            float s0 = FilterSampleFIR8_Simd16(in + (frac >> RESAMPLE_FRACTION_BITS), frac);
            frac += delta;
            float s1 = FilterSampleFIR8_Simd16(in + (frac >> RESAMPLE_FRACTION_BITS), frac);
            frac += delta;
            float s2 = FilterSampleFIR8_Simd16(in + (frac >> RESAMPLE_FRACTION_BITS), frac);
            frac += delta;
            float s3 = FilterSampleFIR8_Simd16(in + (frac >> RESAMPLE_FRACTION_BITS), frac);
            frac += delta;
            float s4 = FilterSampleFIR8_Simd16(in + (frac >> RESAMPLE_FRACTION_BITS), frac);
            frac += delta;
            float s5 = FilterSampleFIR8_Simd16(in + (frac >> RESAMPLE_FRACTION_BITS), frac);
            frac += delta;
            float s6 = FilterSampleFIR8_Simd16(in + (frac >> RESAMPLE_FRACTION_BITS), frac);
            frac += delta;
            float s7 = FilterSampleFIR8_Simd16(in + (frac >> RESAMPLE_FRACTION_BITS), frac);
            frac += delta;
            float s8 = FilterSampleFIR8_Simd16(in + (frac >> RESAMPLE_FRACTION_BITS), frac);
            frac += delta;
            float s9 = FilterSampleFIR8_Simd16(in + (frac >> RESAMPLE_FRACTION_BITS), frac);
            frac += delta;
            float s10 = FilterSampleFIR8_Simd16(in + (frac >> RESAMPLE_FRACTION_BITS), frac);
            frac += delta;
            float s11 = FilterSampleFIR8_Simd16(in + (frac >> RESAMPLE_FRACTION_BITS), frac);
            frac += delta;
            float s12 = FilterSampleFIR8_Simd16(in + (frac >> RESAMPLE_FRACTION_BITS), frac);
            frac += delta;
            float s13 = FilterSampleFIR8_Simd16(in + (frac >> RESAMPLE_FRACTION_BITS), frac);
            frac += delta;
            float s14 = FilterSampleFIR8_Simd16(in + (frac >> RESAMPLE_FRACTION_BITS), frac);
            frac += delta;
            float s15 = FilterSampleFIR8_Simd16(in + (frac >> RESAMPLE_FRACTION_BITS), frac);
            float samples[16] = { s0, s1, s2, s3, s4, s5, s6, s7, s8, s9, s10, s11, s12, s13, s14, s15 };
            return Simd16Load(samples);
        }

        static inline void ResampleStereo_Simd16(const float* in_l, const float* in_r, uint64_t frac, uint64_t delta, dsimd_float16_t* left, dsimd_float16_t* right)
        {
            float samples_l[16], samples_r[16];
            FilterStereoFIR8_Simd16(in_l, in_r, frac, &samples_l[0], &samples_r[0]);
            FilterStereoFIR8_Simd16(in_l, in_r, frac + delta, &samples_l[1], &samples_r[1]);
            FilterStereoFIR8_Simd16(in_l, in_r, frac + delta * 2, &samples_l[2], &samples_r[2]);
            FilterStereoFIR8_Simd16(in_l, in_r, frac + delta * 3, &samples_l[3], &samples_r[3]);
            FilterStereoFIR8_Simd16(in_l, in_r, frac + delta * 4, &samples_l[4], &samples_r[4]);
            FilterStereoFIR8_Simd16(in_l, in_r, frac + delta * 5, &samples_l[5], &samples_r[5]);
            FilterStereoFIR8_Simd16(in_l, in_r, frac + delta * 6, &samples_l[6], &samples_r[6]);
            FilterStereoFIR8_Simd16(in_l, in_r, frac + delta * 7, &samples_l[7], &samples_r[7]);
            FilterStereoFIR8_Simd16(in_l, in_r, frac + delta * 8, &samples_l[8], &samples_r[8]);
            FilterStereoFIR8_Simd16(in_l, in_r, frac + delta * 9, &samples_l[9], &samples_r[9]);
            FilterStereoFIR8_Simd16(in_l, in_r, frac + delta * 10, &samples_l[10], &samples_r[10]);
            FilterStereoFIR8_Simd16(in_l, in_r, frac + delta * 11, &samples_l[11], &samples_r[11]);
            FilterStereoFIR8_Simd16(in_l, in_r, frac + delta * 12, &samples_l[12], &samples_r[12]);
            FilterStereoFIR8_Simd16(in_l, in_r, frac + delta * 13, &samples_l[13], &samples_r[13]);
            FilterStereoFIR8_Simd16(in_l, in_r, frac + delta * 14, &samples_l[14], &samples_r[14]);
            FilterStereoFIR8_Simd16(in_l, in_r, frac + delta * 15, &samples_l[15], &samples_r[15]);
            *left = Simd16Load(samples_l);
            *right = Simd16Load(samples_r);
        }

#endif

    } // namespace Dsp
} // namespace dmSound

#endif // DM_SOUND_DSP_RESAMPLE_MSVC_H
