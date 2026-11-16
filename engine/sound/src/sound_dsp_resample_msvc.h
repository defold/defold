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
    namespace DM_SOUND_DSP_NAMESPACE
    {
        // Explicit construction keeps distinct FIR results in every lane on MSVC.
        DM_SOUND_DSP_INLINE dsimd_float4_t Resample_Simd4(const float* in, uint64_t frac, uint64_t delta) DM_SOUND_DSP_STREAMING
        {
            float s0 = FilterSampleFIR8_Simd8(in + (frac >> RESAMPLE_FRACTION_BITS), frac);
            frac += delta;
            float s1 = FilterSampleFIR8_Simd8(in + (frac >> RESAMPLE_FRACTION_BITS), frac);
            frac += delta;
            float s2 = FilterSampleFIR8_Simd8(in + (frac >> RESAMPLE_FRACTION_BITS), frac);
            frac += delta;
            float s3 = FilterSampleFIR8_Simd8(in + (frac >> RESAMPLE_FRACTION_BITS), frac);
            return Simd4Make(s0, s1, s2, s3);
        }

        DM_SOUND_DSP_INLINE dsimd_float8_t Resample_Simd8(const float* in, uint64_t frac, uint64_t delta) DM_SOUND_DSP_STREAMING
        {
            return Simd8Combine(Resample_Simd4(in, frac, delta), Resample_Simd4(in, frac + delta * 4, delta));
        }

        DM_SOUND_DSP_INLINE dsimd_float16_t Resample_Simd16(const float* in, uint64_t frac, uint64_t delta) DM_SOUND_DSP_STREAMING
        {
            return Simd16Combine(Resample_Simd4(in, frac, delta), Resample_Simd4(in, frac + delta * 4, delta),
                                 Resample_Simd4(in, frac + delta * 8, delta), Resample_Simd4(in, frac + delta * 12, delta));
        }

        // MSVC needs explicit construction to retain distinct FIR results in every lane.
        DM_SOUND_DSP_INLINE void ResampleStereo_Simd4(const float* in_l, const float* in_r, uint64_t frac, uint64_t delta, dsimd_float4_t* left, dsimd_float4_t* right) DM_SOUND_DSP_STREAMING
        {
            float l0, r0, l1, r1, l2, r2, l3, r3;
            FilterStereoFIR8_Simd8(in_l, in_r, frac, &l0, &r0);
            FilterStereoFIR8_Simd8(in_l, in_r, frac + delta, &l1, &r1);
            FilterStereoFIR8_Simd8(in_l, in_r, frac + delta * 2, &l2, &r2);
            FilterStereoFIR8_Simd8(in_l, in_r, frac + delta * 3, &l3, &r3);
            *left = Simd4Make(l0, l1, l2, l3);
            *right = Simd4Make(r0, r1, r2, r3);
        }

        DM_SOUND_DSP_INLINE void ResampleStereo_Simd8(const float* in_l, const float* in_r, uint64_t frac, uint64_t delta, dsimd_float8_t* left, dsimd_float8_t* right) DM_SOUND_DSP_STREAMING
        {
            dsimd_float4_t l0, r0, l1, r1;
            ResampleStereo_Simd4(in_l, in_r, frac, delta, &l0, &r0);
            ResampleStereo_Simd4(in_l, in_r, frac + delta * 4, delta, &l1, &r1);
            *left = Simd8Combine(l0, l1);
            *right = Simd8Combine(r0, r1);
        }

        DM_SOUND_DSP_INLINE void ResampleStereo_Simd16(const float* in_l, const float* in_r, uint64_t frac, uint64_t delta, dsimd_float16_t* left, dsimd_float16_t* right) DM_SOUND_DSP_STREAMING
        {
            dsimd_float4_t l0, r0, l1, r1, l2, r2, l3, r3;
            ResampleStereo_Simd4(in_l, in_r, frac, delta, &l0, &r0);
            ResampleStereo_Simd4(in_l, in_r, frac + delta * 4, delta, &l1, &r1);
            ResampleStereo_Simd4(in_l, in_r, frac + delta * 8, delta, &l2, &r2);
            ResampleStereo_Simd4(in_l, in_r, frac + delta * 12, delta, &l3, &r3);
            *left = Simd16Combine(l0, l1, l2, l3);
            *right = Simd16Combine(r0, r1, r2, r3);
        }

    } // namespace DM_SOUND_DSP_NAMESPACE
} // namespace dmSound

#endif // DM_SOUND_DSP_RESAMPLE_MSVC_H
