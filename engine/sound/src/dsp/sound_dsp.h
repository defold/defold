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

#ifndef DM_SOUND_DSP_H
#define DM_SOUND_DSP_H

#include "../sound.h"
#include <dmsdk/dlib/align.h>
#include <simd/simd.h>

namespace dmSound
{

    const uint32_t RESAMPLE_FRACTION_BITS = 11; // 2048 polyphase filter banks.
    DM_ALIGNED(32) extern const float _pfb[];

    // Kernel function tables are copied into SoundSystem during startup selection.
    struct DSPImpl
    {
        // Native float lanes per register; audio unrolling is selected per callback.
        uint32_t    m_Lanes;
        void        (*MixScaledMonoToStereo)(float* out[], const float* in, uint32_t num, float scale_l, float scale_r, float scale_delta_l, float scale_delta_r);
        void        (*MixScaledStereoToStereo)(float* out[], const float* in_l, const float* in_r, uint32_t num, float scale_l0, float scale_r0, float scale_delta_l0, float scale_delta_r0, float scale_l1, float scale_r1, float scale_delta_l1, float scale_delta_r1);
        uint64_t    (*MixAndResampleMonoToStereo_Polyphase)(float* out[], const float* in, uint32_t num, uint64_t frac, uint64_t delta, float scale_l, float scale_r, float scale_delta_l, float scale_delta_r);
        uint64_t    (*MixAndResampleStereoToStereo_Polyphase)(float* out[], const float* in_l, const float* in_r, uint32_t num, uint64_t frac, uint64_t delta, float scale_l0, float scale_r0, float scale_delta_l0, float scale_delta_r0, float scale_l1, float scale_r1, float scale_delta_l1, float scale_delta_r1);
        void        (*ApplyClampedGain)(float* out[], float* in[], uint32_t num, float scale, float scale_delta);
        void        (*ApplyGainAndInterleaveToS16)(int16_t* out, float* in[], uint32_t num, float scale, float scale_delta);
        void        (*ApplyGain)(float* out[], float* in[], uint32_t num, float scale, float scale_delta);
        void        (*GatherPowerData)(float* in[], uint32_t num, float gain, float& sum_sq_left, float& sum_sq_right, float& max_sq_left, float& max_sq_right);
        void        (*ConvertFromS16)(float* out, const int16_t* in, uint32_t num);
        void        (*ConvertFromS8)(float* out, const int8_t* in, uint32_t num);
        void        (*Deinterleave)(float* out[], const float* in, uint32_t num);
        void        (*DeinterleaveFromS16)(float* out[], const int16_t* in, uint32_t num);
        void        (*DeinterleaveFromS8)(float* out[], const int8_t* in, uint32_t num);
    };

    extern const DSPImpl scalar_impl;

    // Initialize the per-system function table before the audio thread starts.
    struct DSPContext
    {
        DSPImpl         m_Impl;
        DSPImplType     m_Type;    // Requested implementation, including DEFAULT.
        dsimd_version_t m_Version; // Instruction set used by the selected callbacks.
    };

    void SelectDSPImpl(DSPContext* context, DSPImplType type);

    static inline void MixScaledMonoToStereo(const DSPContext& context, float* out[], const float* in, uint32_t num, float scale_l, float scale_r, float scale_delta_l, float scale_delta_r)
    {
        return context.m_Impl.MixScaledMonoToStereo(out, in, num, scale_l, scale_r, scale_delta_l, scale_delta_r);
    }

    static inline void MixScaledStereoToStereo(const DSPContext& context, float* out[], const float* in_l, const float* in_r, uint32_t num, float scale_l0, float scale_r0, float scale_delta_l0, float scale_delta_r0, float scale_l1, float scale_r1, float scale_delta_l1, float scale_delta_r1)
    {
        return context.m_Impl.MixScaledStereoToStereo(out, in_l, in_r, num, scale_l0, scale_r0, scale_delta_l0, scale_delta_r0, scale_l1, scale_r1, scale_delta_l1, scale_delta_r1);
    }

    static inline uint64_t MixAndResampleMonoToStereo_Polyphase(const DSPContext& context, float* out[], const float* in, uint32_t num, uint64_t frac, uint64_t delta, float scale_l, float scale_r, float scale_delta_l, float scale_delta_r)
    {
        return context.m_Impl.MixAndResampleMonoToStereo_Polyphase(out, in, num, frac, delta, scale_l, scale_r, scale_delta_l, scale_delta_r);
    }

    static inline uint64_t MixAndResampleStereoToStereo_Polyphase(const DSPContext& context, float* out[], const float* in_l, const float* in_r, uint32_t num, uint64_t frac, uint64_t delta, float scale_l0, float scale_r0, float scale_delta_l0, float scale_delta_r0, float scale_l1, float scale_r1, float scale_delta_l1, float scale_delta_r1)
    {
        return context.m_Impl.MixAndResampleStereoToStereo_Polyphase(out, in_l, in_r, num, frac, delta, scale_l0, scale_r0, scale_delta_l0, scale_delta_r0, scale_l1, scale_r1, scale_delta_l1, scale_delta_r1);
    }

    static inline void ApplyClampedGain(const DSPContext& context, float* out[], float* in[], uint32_t num, float scale, float scale_delta)
    {
        return context.m_Impl.ApplyClampedGain(out, in, num, scale, scale_delta);
    }

    static inline void ApplyGainAndInterleaveToS16(const DSPContext& context, int16_t* out, float* in[], uint32_t num, float scale, float scale_delta)
    {
        return context.m_Impl.ApplyGainAndInterleaveToS16(out, in, num, scale, scale_delta);
    }

    static inline void ApplyGain(const DSPContext& context, float* out[], float* in[], uint32_t num, float scale, float scale_delta)
    {
        return context.m_Impl.ApplyGain(out, in, num, scale, scale_delta);
    }

    static inline void GatherPowerData(const DSPContext& context, float* in[], uint32_t num, float gain, float& sum_sq_left, float& sum_sq_right, float& max_sq_left, float& max_sq_right)
    {
        return context.m_Impl.GatherPowerData(in, num, gain, sum_sq_left, sum_sq_right, max_sq_left, max_sq_right);
    }

    static inline void ConvertFromS16(const DSPContext& context, float* out, const int16_t* in, uint32_t num)
    {
        return context.m_Impl.ConvertFromS16(out, in, num);
    }

    static inline void ConvertFromS8(const DSPContext& context, float* out, const int8_t* in, uint32_t num)
    {
        return context.m_Impl.ConvertFromS8(out, in, num);
    }

    static inline void Deinterleave(const DSPContext& context, float* out[], const float* in, uint32_t num)
    {
        return context.m_Impl.Deinterleave(out, in, num);
    }

    static inline void DeinterleaveFromS16(const DSPContext& context, float* out[], const int16_t* in, uint32_t num)
    {
        return context.m_Impl.DeinterleaveFromS16(out, in, num);
    }

    static inline void DeinterleaveFromS8(const DSPContext& context, float* out[], const int8_t* in, uint32_t num)
    {
        return context.m_Impl.DeinterleaveFromS8(out, in, num);
    }

} // namespace dmSound

#endif // DM_SOUND_DSP_H
