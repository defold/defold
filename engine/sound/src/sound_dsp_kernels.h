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

#ifndef DM_SOUND_DSP_KERNELS_H
#define DM_SOUND_DSP_KERNELS_H

#include <stdint.h>

namespace dmSound
{
// Private scalar/pointer interfaces to separately compiled ISA loops.


#if defined(DM_SOUND_DSP_X86)
    namespace DSPSSE3
    {
        void     MixScaledMonoToStereo(float* out[], const float* in, uint32_t num, float scale_l, float scale_r, float scale_delta_l, float scale_delta_r);
        void     MixScaledStereoToStereo(float* out[], const float* in_l, const float* in_r, uint32_t num, float scale_l0, float scale_r0, float scale_delta_l0, float scale_delta_r0, float scale_l1, float scale_r1, float scale_delta_l1, float scale_delta_r1);
        uint64_t MixAndResampleMonoToStereo_Polyphase(float* out[], const float* in, uint32_t num, uint64_t frac, uint64_t delta, float scale_l, float scale_r, float scale_delta_l, float scale_delta_r);
        uint64_t MixAndResampleStereoToStereo_Polyphase(float* out[], const float* in_l, const float* in_r, uint32_t num, uint64_t frac, uint64_t delta, float scale_l0, float scale_r0, float scale_delta_l0, float scale_delta_r0, float scale_l1, float scale_r1, float scale_delta_l1, float scale_delta_r1);
        void     ApplyClampedGain(float* out[], float* in[], uint32_t num, float scale, float scale_delta);
        void     ApplyGainAndInterleaveToS16(int16_t* out, float* in[], uint32_t num, float scale, float scale_delta);
        void     ApplyGain(float* out[], float* in[], uint32_t num, float scale, float scale_delta);
        void     GatherPowerData(float* in[], uint32_t num, float gain, float& sum_sq_left, float& sum_sq_right, float& max_sq_left, float& max_sq_right);
        void     ConvertFromS16(float* out, const int16_t* in, uint32_t num);
        void     ConvertFromS8(float* out, const int8_t* in, uint32_t num);
        void     Deinterleave(float* out[], const float* in, uint32_t num);
        void     DeinterleaveFromS16(float* out[], const int16_t* in, uint32_t num);
        void     DeinterleaveFromS8(float* out[], const int8_t* in, uint32_t num);
    }
#endif

#if defined(DM_SOUND_DSP_X86)
    namespace DSPSSE42
    {
        void     MixScaledMonoToStereo(float* out[], const float* in, uint32_t num, float scale_l, float scale_r, float scale_delta_l, float scale_delta_r);
        void     MixScaledStereoToStereo(float* out[], const float* in_l, const float* in_r, uint32_t num, float scale_l0, float scale_r0, float scale_delta_l0, float scale_delta_r0, float scale_l1, float scale_r1, float scale_delta_l1, float scale_delta_r1);
        uint64_t MixAndResampleMonoToStereo_Polyphase(float* out[], const float* in, uint32_t num, uint64_t frac, uint64_t delta, float scale_l, float scale_r, float scale_delta_l, float scale_delta_r);
        uint64_t MixAndResampleStereoToStereo_Polyphase(float* out[], const float* in_l, const float* in_r, uint32_t num, uint64_t frac, uint64_t delta, float scale_l0, float scale_r0, float scale_delta_l0, float scale_delta_r0, float scale_l1, float scale_r1, float scale_delta_l1, float scale_delta_r1);
        void     ApplyClampedGain(float* out[], float* in[], uint32_t num, float scale, float scale_delta);
        void     ApplyGainAndInterleaveToS16(int16_t* out, float* in[], uint32_t num, float scale, float scale_delta);
        void     ApplyGain(float* out[], float* in[], uint32_t num, float scale, float scale_delta);
        void     GatherPowerData(float* in[], uint32_t num, float gain, float& sum_sq_left, float& sum_sq_right, float& max_sq_left, float& max_sq_right);
        void     ConvertFromS16(float* out, const int16_t* in, uint32_t num);
        void     ConvertFromS8(float* out, const int8_t* in, uint32_t num);
        void     Deinterleave(float* out[], const float* in, uint32_t num);
        void     DeinterleaveFromS16(float* out[], const int16_t* in, uint32_t num);
        void     DeinterleaveFromS8(float* out[], const int8_t* in, uint32_t num);
    }
#endif

#if defined(DM_SOUND_DSP_X86)
    namespace DSPAVX2
    {
        void     MixScaledMonoToStereo(float* out[], const float* in, uint32_t num, float scale_l, float scale_r, float scale_delta_l, float scale_delta_r);
        void     MixScaledStereoToStereo(float* out[], const float* in_l, const float* in_r, uint32_t num, float scale_l0, float scale_r0, float scale_delta_l0, float scale_delta_r0, float scale_l1, float scale_r1, float scale_delta_l1, float scale_delta_r1);
        uint64_t MixAndResampleMonoToStereo_Polyphase(float* out[], const float* in, uint32_t num, uint64_t frac, uint64_t delta, float scale_l, float scale_r, float scale_delta_l, float scale_delta_r);
        uint64_t MixAndResampleStereoToStereo_Polyphase(float* out[], const float* in_l, const float* in_r, uint32_t num, uint64_t frac, uint64_t delta, float scale_l0, float scale_r0, float scale_delta_l0, float scale_delta_r0, float scale_l1, float scale_r1, float scale_delta_l1, float scale_delta_r1);
        void     ApplyClampedGain(float* out[], float* in[], uint32_t num, float scale, float scale_delta);
        void     ApplyGainAndInterleaveToS16(int16_t* out, float* in[], uint32_t num, float scale, float scale_delta);
        void     ApplyGain(float* out[], float* in[], uint32_t num, float scale, float scale_delta);
        void     GatherPowerData(float* in[], uint32_t num, float gain, float& sum_sq_left, float& sum_sq_right, float& max_sq_left, float& max_sq_right);
        void     ConvertFromS16(float* out, const int16_t* in, uint32_t num);
        void     ConvertFromS8(float* out, const int8_t* in, uint32_t num);
        void     Deinterleave(float* out[], const float* in, uint32_t num);
        void     DeinterleaveFromS16(float* out[], const int16_t* in, uint32_t num);
        void     DeinterleaveFromS8(float* out[], const int8_t* in, uint32_t num);
    }
#endif

#if defined(DM_SOUND_DSP_X86)
    namespace DSPAVX512
    {
        void     MixScaledMonoToStereo(float* out[], const float* in, uint32_t num, float scale_l, float scale_r, float scale_delta_l, float scale_delta_r);
        void     MixScaledStereoToStereo(float* out[], const float* in_l, const float* in_r, uint32_t num, float scale_l0, float scale_r0, float scale_delta_l0, float scale_delta_r0, float scale_l1, float scale_r1, float scale_delta_l1, float scale_delta_r1);
        uint64_t MixAndResampleMonoToStereo_Polyphase(float* out[], const float* in, uint32_t num, uint64_t frac, uint64_t delta, float scale_l, float scale_r, float scale_delta_l, float scale_delta_r);
        uint64_t MixAndResampleStereoToStereo_Polyphase(float* out[], const float* in_l, const float* in_r, uint32_t num, uint64_t frac, uint64_t delta, float scale_l0, float scale_r0, float scale_delta_l0, float scale_delta_r0, float scale_l1, float scale_r1, float scale_delta_l1, float scale_delta_r1);
        void     ApplyClampedGain(float* out[], float* in[], uint32_t num, float scale, float scale_delta);
        void     ApplyGainAndInterleaveToS16(int16_t* out, float* in[], uint32_t num, float scale, float scale_delta);
        void     ApplyGain(float* out[], float* in[], uint32_t num, float scale, float scale_delta);
        void     GatherPowerData(float* in[], uint32_t num, float gain, float& sum_sq_left, float& sum_sq_right, float& max_sq_left, float& max_sq_right);
        void     ConvertFromS16(float* out, const int16_t* in, uint32_t num);
        void     ConvertFromS8(float* out, const int8_t* in, uint32_t num);
        void     Deinterleave(float* out[], const float* in, uint32_t num);
        void     DeinterleaveFromS16(float* out[], const int16_t* in, uint32_t num);
        void     DeinterleaveFromS8(float* out[], const int8_t* in, uint32_t num);
    }
#endif

#if defined(DM_SOUND_DSP_WASM)
    namespace DSPWASM
    {
        void     MixScaledMonoToStereo(float* out[], const float* in, uint32_t num, float scale_l, float scale_r, float scale_delta_l, float scale_delta_r);
        void     MixScaledStereoToStereo(float* out[], const float* in_l, const float* in_r, uint32_t num, float scale_l0, float scale_r0, float scale_delta_l0, float scale_delta_r0, float scale_l1, float scale_r1, float scale_delta_l1, float scale_delta_r1);
        uint64_t MixAndResampleMonoToStereo_Polyphase(float* out[], const float* in, uint32_t num, uint64_t frac, uint64_t delta, float scale_l, float scale_r, float scale_delta_l, float scale_delta_r);
        uint64_t MixAndResampleStereoToStereo_Polyphase(float* out[], const float* in_l, const float* in_r, uint32_t num, uint64_t frac, uint64_t delta, float scale_l0, float scale_r0, float scale_delta_l0, float scale_delta_r0, float scale_l1, float scale_r1, float scale_delta_l1, float scale_delta_r1);
        void     ApplyClampedGain(float* out[], float* in[], uint32_t num, float scale, float scale_delta);
        void     ApplyGainAndInterleaveToS16(int16_t* out, float* in[], uint32_t num, float scale, float scale_delta);
        void     ApplyGain(float* out[], float* in[], uint32_t num, float scale, float scale_delta);
        void     GatherPowerData(float* in[], uint32_t num, float gain, float& sum_sq_left, float& sum_sq_right, float& max_sq_left, float& max_sq_right);
        void     ConvertFromS16(float* out, const int16_t* in, uint32_t num);
        void     ConvertFromS8(float* out, const int8_t* in, uint32_t num);
        void     Deinterleave(float* out[], const float* in, uint32_t num);
        void     DeinterleaveFromS16(float* out[], const int16_t* in, uint32_t num);
        void     DeinterleaveFromS8(float* out[], const int8_t* in, uint32_t num);
    }
#endif

#if defined(DM_SOUND_DSP_NEON)
    namespace DSPNEON
    {
        void     MixScaledMonoToStereo(float* out[], const float* in, uint32_t num, float scale_l, float scale_r, float scale_delta_l, float scale_delta_r);
        void     MixScaledStereoToStereo(float* out[], const float* in_l, const float* in_r, uint32_t num, float scale_l0, float scale_r0, float scale_delta_l0, float scale_delta_r0, float scale_l1, float scale_r1, float scale_delta_l1, float scale_delta_r1);
        uint64_t MixAndResampleMonoToStereo_Polyphase(float* out[], const float* in, uint32_t num, uint64_t frac, uint64_t delta, float scale_l, float scale_r, float scale_delta_l, float scale_delta_r);
        uint64_t MixAndResampleStereoToStereo_Polyphase(float* out[], const float* in_l, const float* in_r, uint32_t num, uint64_t frac, uint64_t delta, float scale_l0, float scale_r0, float scale_delta_l0, float scale_delta_r0, float scale_l1, float scale_r1, float scale_delta_l1, float scale_delta_r1);
        void     ApplyClampedGain(float* out[], float* in[], uint32_t num, float scale, float scale_delta);
        void     ApplyGainAndInterleaveToS16(int16_t* out, float* in[], uint32_t num, float scale, float scale_delta);
        void     ApplyGain(float* out[], float* in[], uint32_t num, float scale, float scale_delta);
        void     GatherPowerData(float* in[], uint32_t num, float gain, float& sum_sq_left, float& sum_sq_right, float& max_sq_left, float& max_sq_right);
        void     ConvertFromS16(float* out, const int16_t* in, uint32_t num);
        void     ConvertFromS8(float* out, const int8_t* in, uint32_t num);
        void     Deinterleave(float* out[], const float* in, uint32_t num);
        void     DeinterleaveFromS16(float* out[], const int16_t* in, uint32_t num);
        void     DeinterleaveFromS8(float* out[], const int8_t* in, uint32_t num);
    }
#endif

#if defined(DM_SOUND_DSP_NEON_WIDE)
    namespace DSPNEON8
    {
        void     MixScaledMonoToStereo(float* out[], const float* in, uint32_t num, float scale_l, float scale_r, float scale_delta_l, float scale_delta_r);
        void     MixScaledStereoToStereo(float* out[], const float* in_l, const float* in_r, uint32_t num, float scale_l0, float scale_r0, float scale_delta_l0, float scale_delta_r0, float scale_l1, float scale_r1, float scale_delta_l1, float scale_delta_r1);
        uint64_t MixAndResampleMonoToStereo_Polyphase(float* out[], const float* in, uint32_t num, uint64_t frac, uint64_t delta, float scale_l, float scale_r, float scale_delta_l, float scale_delta_r);
        uint64_t MixAndResampleStereoToStereo_Polyphase(float* out[], const float* in_l, const float* in_r, uint32_t num, uint64_t frac, uint64_t delta, float scale_l0, float scale_r0, float scale_delta_l0, float scale_delta_r0, float scale_l1, float scale_r1, float scale_delta_l1, float scale_delta_r1);
        void     ApplyClampedGain(float* out[], float* in[], uint32_t num, float scale, float scale_delta);
        void     ApplyGainAndInterleaveToS16(int16_t* out, float* in[], uint32_t num, float scale, float scale_delta);
        void     ApplyGain(float* out[], float* in[], uint32_t num, float scale, float scale_delta);
        void     GatherPowerData(float* in[], uint32_t num, float gain, float& sum_sq_left, float& sum_sq_right, float& max_sq_left, float& max_sq_right);
        void     ConvertFromS16(float* out, const int16_t* in, uint32_t num);
        void     ConvertFromS8(float* out, const int8_t* in, uint32_t num);
        void     Deinterleave(float* out[], const float* in, uint32_t num);
        void     DeinterleaveFromS16(float* out[], const int16_t* in, uint32_t num);
        void     DeinterleaveFromS8(float* out[], const int8_t* in, uint32_t num);
    }
#endif

#if defined(DM_SOUND_DSP_NEON_WIDE)
    namespace DSPNEON16
    {
        void     MixScaledMonoToStereo(float* out[], const float* in, uint32_t num, float scale_l, float scale_r, float scale_delta_l, float scale_delta_r);
        void     MixScaledStereoToStereo(float* out[], const float* in_l, const float* in_r, uint32_t num, float scale_l0, float scale_r0, float scale_delta_l0, float scale_delta_r0, float scale_l1, float scale_r1, float scale_delta_l1, float scale_delta_r1);
        uint64_t MixAndResampleMonoToStereo_Polyphase(float* out[], const float* in, uint32_t num, uint64_t frac, uint64_t delta, float scale_l, float scale_r, float scale_delta_l, float scale_delta_r);
        uint64_t MixAndResampleStereoToStereo_Polyphase(float* out[], const float* in_l, const float* in_r, uint32_t num, uint64_t frac, uint64_t delta, float scale_l0, float scale_r0, float scale_delta_l0, float scale_delta_r0, float scale_l1, float scale_r1, float scale_delta_l1, float scale_delta_r1);
        void     ApplyClampedGain(float* out[], float* in[], uint32_t num, float scale, float scale_delta);
        void     ApplyGainAndInterleaveToS16(int16_t* out, float* in[], uint32_t num, float scale, float scale_delta);
        void     ApplyGain(float* out[], float* in[], uint32_t num, float scale, float scale_delta);
        void     GatherPowerData(float* in[], uint32_t num, float gain, float& sum_sq_left, float& sum_sq_right, float& max_sq_left, float& max_sq_right);
        void     ConvertFromS16(float* out, const int16_t* in, uint32_t num);
        void     ConvertFromS8(float* out, const int8_t* in, uint32_t num);
        void     Deinterleave(float* out[], const float* in, uint32_t num);
        void     DeinterleaveFromS16(float* out[], const int16_t* in, uint32_t num);
        void     DeinterleaveFromS8(float* out[], const int8_t* in, uint32_t num);
    }
#endif

#if defined(DM_SOUND_DSP_SME)
    namespace DSPSME
    {
        void     MixScaledMonoToStereo(float* out[], const float* in, uint32_t num, float scale_l, float scale_r, float scale_delta_l, float scale_delta_r);
        void     MixScaledStereoToStereo(float* out[], const float* in_l, const float* in_r, uint32_t num, float scale_l0, float scale_r0, float scale_delta_l0, float scale_delta_r0, float scale_l1, float scale_r1, float scale_delta_l1, float scale_delta_r1);
        uint64_t MixAndResampleMonoToStereo_Polyphase(float* out[], const float* in, uint32_t num, uint64_t frac, uint64_t delta, float scale_l, float scale_r, float scale_delta_l, float scale_delta_r);
        uint64_t MixAndResampleStereoToStereo_Polyphase(float* out[], const float* in_l, const float* in_r, uint32_t num, uint64_t frac, uint64_t delta, float scale_l0, float scale_r0, float scale_delta_l0, float scale_delta_r0, float scale_l1, float scale_r1, float scale_delta_l1, float scale_delta_r1);
        void     ApplyClampedGain(float* out[], float* in[], uint32_t num, float scale, float scale_delta);
        void     ApplyGainAndInterleaveToS16(int16_t* out, float* in[], uint32_t num, float scale, float scale_delta);
        void     ApplyGain(float* out[], float* in[], uint32_t num, float scale, float scale_delta);
        void     GatherPowerData(float* in[], uint32_t num, float gain, float& sum_sq_left, float& sum_sq_right, float& max_sq_left, float& max_sq_right);
        void     ConvertFromS16(float* out, const int16_t* in, uint32_t num);
        void     ConvertFromS8(float* out, const int8_t* in, uint32_t num);
        void     Deinterleave(float* out[], const float* in, uint32_t num);
        void     DeinterleaveFromS16(float* out[], const int16_t* in, uint32_t num);
        void     DeinterleaveFromS8(float* out[], const int8_t* in, uint32_t num);
    }
#endif

} // namespace dmSound

#endif // DM_SOUND_DSP_KERNELS_H
