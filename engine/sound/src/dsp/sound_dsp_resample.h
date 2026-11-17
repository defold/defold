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

#ifndef DM_SOUND_DSP_RESAMPLE_H
#define DM_SOUND_DSP_RESAMPLE_H

#include "sound_dsp_simd.h"

namespace dmSound
{
    namespace Dsp
    {
        // LLVM schedules the independent FIR samples faster from these loops.
        template <unsigned Unroll>
        static inline void Resample_Simd4(const float* in, uint64_t frac, uint64_t delta, dsimd_float4_t* output)
        {
            float samples[4 * Unroll];
            for (unsigned i = 0; i < 4 * Unroll; ++i)
            {
                samples[i] = FilterSampleFIR8_Simd4(in + (frac >> RESAMPLE_FRACTION_BITS), frac);
                frac += delta;
            }

            for (unsigned u = 0; u < Unroll; ++u)
            {
                output[u] = Simd4Load(samples + u * 4);
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
            float samples[8];
            for (unsigned i = 0; i < 8; ++i)
            {
                samples[i] = FilterSampleFIR8_Simd8(in + (frac >> RESAMPLE_FRACTION_BITS), frac);
                frac += delta;
            }

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
            float samples[16];
            for (unsigned i = 0; i < 16; ++i)
            {
                samples[i] = FilterSampleFIR8_Simd16(in + (frac >> RESAMPLE_FRACTION_BITS), frac);
                frac += delta;
            }

            return Simd16Load(samples);
        }

        static inline void ResampleStereo_Simd16(const float* in_l, const float* in_r, uint64_t frac, uint64_t delta, dsimd_float16_t* left, dsimd_float16_t* right)
        {
            float samples_l[16], samples_r[16];
            for (unsigned i = 0; i < 16; ++i)
            {
                FilterStereoFIR8_Simd16(in_l, in_r, frac, &samples_l[i], &samples_r[i]);
                frac += delta;
            }

            *left = Simd16Load(samples_l);
            *right = Simd16Load(samples_r);
        }

#endif

    } // namespace Dsp
} // namespace dmSound

#endif // DM_SOUND_DSP_RESAMPLE_H
