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
    namespace DM_SOUND_DSP_NAMESPACE
    {
        // LLVM schedules the independent FIR samples faster from these loops.
        DM_SOUND_DSP_INLINE dsimd_float4_t Resample_Simd4(const float* in, uint64_t frac, uint64_t delta) DM_SOUND_DSP_STREAMING
        {
            float samples[4];
            for (unsigned i = 0; i < 4; ++i)
            {
                samples[i] = FilterSampleFIR8_Simd8(in + (frac >> RESAMPLE_FRACTION_BITS), frac);
                frac += delta;
            }

            return Simd4Load(samples);
        }

        DM_SOUND_DSP_INLINE dsimd_float8_t Resample_Simd8(const float* in, uint64_t frac, uint64_t delta) DM_SOUND_DSP_STREAMING
        {
            float samples[8];
            for (unsigned i = 0; i < 8; ++i)
            {
                samples[i] = FilterSampleFIR8_Simd8(in + (frac >> RESAMPLE_FRACTION_BITS), frac);
                frac += delta;
            }

            return Simd8Load(samples);
        }

        DM_SOUND_DSP_INLINE dsimd_float16_t Resample_Simd16(const float* in, uint64_t frac, uint64_t delta) DM_SOUND_DSP_STREAMING
        {
            float samples[16];
            for (unsigned i = 0; i < 16; ++i)
            {
                samples[i] = FilterSampleFIR8_Simd8(in + (frac >> RESAMPLE_FRACTION_BITS), frac);
                frac += delta;
            }

            return Simd16Load(samples);
        }

        DM_SOUND_DSP_INLINE void ResampleStereo_Simd4(const float* in_l, const float* in_r, uint64_t frac, uint64_t delta, dsimd_float4_t* left, dsimd_float4_t* right) DM_SOUND_DSP_STREAMING
        {
            float samples_l[4], samples_r[4];
            for (unsigned i = 0; i < 4; ++i)
            {
                FilterStereoFIR8_Simd8(in_l, in_r, frac, &samples_l[i], &samples_r[i]);
                frac += delta;
            }

            *left = Simd4Load(samples_l);
            *right = Simd4Load(samples_r);
        }

        DM_SOUND_DSP_INLINE void ResampleStereo_Simd8(const float* in_l, const float* in_r, uint64_t frac, uint64_t delta, dsimd_float8_t* left, dsimd_float8_t* right) DM_SOUND_DSP_STREAMING
        {
            float samples_l[8], samples_r[8];
            for (unsigned i = 0; i < 8; ++i)
            {
                FilterStereoFIR8_Simd8(in_l, in_r, frac, &samples_l[i], &samples_r[i]);
                frac += delta;
            }

            *left = Simd8Load(samples_l);
            *right = Simd8Load(samples_r);
        }

        DM_SOUND_DSP_INLINE void ResampleStereo_Simd16(const float* in_l, const float* in_r, uint64_t frac, uint64_t delta, dsimd_float16_t* left, dsimd_float16_t* right) DM_SOUND_DSP_STREAMING
        {
            float samples_l[16], samples_r[16];
            for (unsigned i = 0; i < 16; ++i)
            {
                FilterStereoFIR8_Simd8(in_l, in_r, frac, &samples_l[i], &samples_r[i]);
                frac += delta;
            }

            *left = Simd16Load(samples_l);
            *right = Simd16Load(samples_r);
        }

    } // namespace DM_SOUND_DSP_NAMESPACE
} // namespace dmSound

#endif // DM_SOUND_DSP_RESAMPLE_H
