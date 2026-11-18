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

#include "sound_dsp_resample_stereo.h"

namespace dmSound
{
    namespace Dsp
    {
        // Group four FIR reductions without changing their addition order.
        template <unsigned Unroll>
        static inline void Resample_Simd4(const float* in, uint64_t frac, uint64_t delta, dsimd_float4_t* output)
        {
            for (unsigned u = 0; u < Unroll; ++u)
            {
                dsimd_float4_t samples[4];
                for (unsigned i = 0; i < 4; ++i)
                {
                    samples[i] = FilterSampleFIR8Partial_Simd4(in + (frac >> RESAMPLE_FRACTION_BITS), frac);
                    frac += delta;
                }

                // Keep the original pairwise sum order, with one FIR result in each lane.
                output[u] = Simd4PairwiseAdd(Simd4PairwiseAdd(samples[0], samples[1]), Simd4PairwiseAdd(samples[2], samples[3]));
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
