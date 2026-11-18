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

#ifndef DM_SOUND_DSP_RESAMPLE_STEREO_H
#define DM_SOUND_DSP_RESAMPLE_STEREO_H

#include "sound_dsp_simd.h"

namespace dmSound
{
    namespace Dsp
    {
        template <unsigned Unroll>
        static inline void ResampleStereo_Simd128(const float* in_l, const float* in_r, uint64_t frac, uint64_t delta, dsimd_float4_t* left, dsimd_float4_t* right)
        {
            for (unsigned u = 0; u < Unroll; ++u)
            {
                dsimd_float4_t l[4], r[4];
                for (unsigned i = 0; i < 4; ++i)
                {
                    FilterStereoFIR8Partial_Simd128(in_l, in_r, frac, &l[i], &r[i]);
                    frac += delta;
                }

                // Keep the original pairwise sum order, with one FIR result in each lane.
                left[u] = Simd128PairwiseAdd(Simd128PairwiseAdd(l[0], l[1]), Simd128PairwiseAdd(l[2], l[3]));
                right[u] = Simd128PairwiseAdd(Simd128PairwiseAdd(r[0], r[1]), Simd128PairwiseAdd(r[2], r[3]));
            }
        }

#if !DM_SIMD_FORCE_SCALAR && defined(DM_SIMD_AVX2)
        static inline void ResampleStereo_Simd256(const float* in_l, const float* in_r, uint64_t frac, uint64_t delta, dsimd_float8_t* left, dsimd_float8_t* right)
        {
            dsimd_float4_t samples_l[2], samples_r[2];
            for (unsigned group = 0; group < 2; ++group)
            {
                dsimd_float8_t l[4], r[4];
                for (unsigned i = 0; i < 4; ++i)
                {
                    FilterStereoFIR8Products_Simd256(in_l, in_r, frac, &l[i], &r[i]);
                    frac += delta;
                }

                samples_l[group] = Simd256Sum4(l[0], l[1], l[2], l[3]);
                samples_r[group] = Simd256Sum4(r[0], r[1], r[2], r[3]);
            }

            *left = Simd256Combine(samples_l[0], samples_l[1]);
            *right = Simd256Combine(samples_r[0], samples_r[1]);
        }

#endif

    } // namespace Dsp
} // namespace dmSound

#endif // DM_SOUND_DSP_RESAMPLE_STEREO_H
