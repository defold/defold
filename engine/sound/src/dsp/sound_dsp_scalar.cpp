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

#include "sound_dsp.h"
#include "sound_dsp_scalar.h"
#include <dlib/math.h>
#include <dlib/profile.h>

namespace dmSound
{
    namespace CPU
    {
        static inline float FilterSampleFIR8(const float* frames, uint64_t frac_pos)
        {
            uint32_t pi = ((frac_pos >> (RESAMPLE_FRACTION_BITS - 11)) << 3) & (2047 << 3); // 8 tabs, 2048 (11 fractional bits) banks
            const float* c = &_pfb[pi];
            const float* t = &frames[-3];
            return t[0] * c[0] + t[1] * c[1] + t[2] * c[2] + t[3] * c[3] + t[4] * c[4] + t[5] * c[5] + t[6] * c[6] + t[7] * c[7];
        }

        static void MixScaledMonoToStereo(float* out[], const float* in, uint32_t num, float scale_l, float scale_r, float scale_delta_l, float scale_delta_r)
        {
            float* l = out[0];
            float* r = out[1];
            for (; num > 0; --num)
            {
                float s = *(in++);
                *(l++) += s * scale_l;
                *(r++) += s * scale_r;
                scale_l += scale_delta_l;
                scale_r += scale_delta_r;
            }
        }

        static void MixScaledStereoToStereo(float* out[], const float* in_l, const float* in_r, uint32_t num,
                                            float scale_l0, float scale_r0, float scale_delta_l0, float scale_delta_r0,
                                            float scale_l1, float scale_r1, float scale_delta_l1, float scale_delta_r1)
        {
            float* l = out[0];
            float* r = out[1];
            for (; num > 0; --num)
            {
                float sl = *(in_l++);
                float sr = *(in_r++);
                *(l++) += sl * scale_l0 + sr * scale_l1;
                *(r++) += sl * scale_r0 + sr * scale_r1;
                scale_l0 += scale_delta_l0;
                scale_r0 += scale_delta_r0;
                scale_l1 += scale_delta_l1;
                scale_r1 += scale_delta_r1;
            }
        }

        static uint64_t MixAndResampleMonoToStereo_Polyphase(float* out[], const float* in, uint32_t num, uint64_t frac, uint64_t delta, float scale_l, float scale_r, float scale_delta_l, float scale_delta_r)
        {
            DM_PROFILE("MixAndResampleMonoToStereo_Polyphase_CPU");

            float* out_l = out[0];
            float* out_r = out[1];

            for (uint32_t i = 0; i < num; ++i)
            {
                // Resample
                float s = FilterSampleFIR8(&in[frac >> RESAMPLE_FRACTION_BITS], frac);

                // Mix
                *(out_l++) += s * scale_l;
                *(out_r++) += s * scale_r;
                scale_l += scale_delta_l;
                scale_r += scale_delta_r;

                // Advance
                frac += delta;
            }

            return frac;
        }

        static uint64_t MixAndResampleStereoToStereo_Polyphase(float* out[], const float* in_l, const float* in_r, uint32_t num, uint64_t frac, uint64_t delta,
                                                               float scale_l0, float scale_r0, float scale_delta_l0, float scale_delta_r0,
                                                               float scale_l1, float scale_r1, float scale_delta_l1, float scale_delta_r1)
        {
            DM_PROFILE("MixAndResampleStereoToStereo_Polyphase_CPU");

            float* out_l = out[0];
            float* out_r = out[1];

            for (uint32_t i = 0; i < num; ++i)
            {
                // Resample
                float sl = FilterSampleFIR8(&in_l[frac >> RESAMPLE_FRACTION_BITS], frac);
                float sr = FilterSampleFIR8(&in_r[frac >> RESAMPLE_FRACTION_BITS], frac);

                // Mix
                *(out_l++) += sl * scale_l0 + sr * scale_l1;
                *(out_r++) += sl * scale_r0 + sr * scale_r1;
                scale_l0 += scale_delta_l0;
                scale_r0 += scale_delta_r0;
                scale_l1 += scale_delta_l1;
                scale_r1 += scale_delta_r1;

                // Advance
                frac += delta;
            }

            return frac;
        }

        static void GatherPowerData(float* in[], uint32_t num, float gain, float& sum_sq_left, float& sum_sq_right, float& max_sq_left, float& max_sq_right)
        {
            sum_sq_left = 0;
            sum_sq_right = 0;
            max_sq_left = 0;
            max_sq_right = 0;
            for (uint32_t j = 0; j < num; j++)
            {
                float left = in[0][j] * gain;
                float right = in[1][j] * gain;
                float left_sq = left * left;
                float right_sq = right * right;
                sum_sq_left += left_sq;
                sum_sq_right += right_sq;
                max_sq_left = dmMath::Max(max_sq_left, left_sq);
                max_sq_right = dmMath::Max(max_sq_right, right_sq);
            }
        }

    } // namespace CPU

    extern const DSPImpl scalar_impl =
    {
        1,
        CPU::MixScaledMonoToStereo,
        CPU::MixScaledStereoToStereo,
        CPU::MixAndResampleMonoToStereo_Polyphase,
        CPU::MixAndResampleStereoToStereo_Polyphase,
        CPU::ApplyClampedGain,
        CPU::ApplyGainAndInterleaveToS16,
        CPU::ApplyGain,
        CPU::GatherPowerData,
        CPU::ConvertFromS16,
        CPU::ConvertFromS8,
        CPU::Deinterleave,
        CPU::DeinterleaveFromS16,
        CPU::DeinterleaveFromS8
    };

} // namespace dmSound
