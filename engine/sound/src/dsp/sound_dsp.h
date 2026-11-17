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
#include <dlib/math.h>
#include <dlib/profile.h>
#include <dlib/log.h>
#include <dmsdk/dlib/align.h>
#include <simd/simd_runtime.h>

namespace dmSound
{

const uint32_t RESAMPLE_FRACTION_BITS = 11; // 2048 polyphase filter banks.
DM_ALIGNED(32) extern const float _pfb[];

namespace CPU {

//
// Non SIMD
//

static inline float FilterSampleFIR8(const float* frames, uint64_t frac_pos)
{
        uint32_t pi = ((frac_pos >> (RESAMPLE_FRACTION_BITS - 11)) << 3) & (2047 << 3); // 8 tabs, 2048 (11 fractional bits) banks
        const float* c = &_pfb[pi];
        const float *t = &frames[-3];
        return t[0] * c[0] + t[1] * c[1] + t[2] * c[2] + t[3] * c[3] + t[4] * c[4] + t[5] * c[5] + t[6] * c[6] + t[7] * c[7];
}

static inline void MixScaledMonoToStereo(float* out[], const float* in, uint32_t num, float scale_l, float scale_r, float scale_delta_l, float scale_delta_r)
{
    float* l = out[0];
    float* r = out[1];
    for(; num>0; --num)
    {
        float s = *(in++);
        *(l++) += s * scale_l;
        *(r++) += s * scale_r;
        scale_l += scale_delta_l;
        scale_r += scale_delta_r;
    }
}

static inline void MixScaledStereoToStereo(float* out[], const float* in_l, const float* in_r, uint32_t num,
                                           float scale_l0, float scale_r0, float scale_delta_l0, float scale_delta_r0,
                                           float scale_l1, float scale_r1, float scale_delta_l1, float scale_delta_r1)
{
    float* l = out[0];
    float* r = out[1];
    for(; num>0; --num)
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

static inline uint64_t MixAndResampleMonoToStereo_Polyphase(float* out[], const float* in, uint32_t num, uint64_t frac, uint64_t delta, float scale_l, float scale_r, float scale_delta_l, float scale_delta_r)
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

static inline uint64_t MixAndResampleStereoToStereo_Polyphase(float* out[], const float* in_l, const float* in_r, uint32_t num, uint64_t frac, uint64_t delta,
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

static inline void ApplyClampedGain(float* out[], float* in[], uint32_t num, float scale, float scale_delta)
{
    float* in_l = in[0];
    float* in_r = in[1];
    float* out_l = out[0];
    float* out_r = out[1];
    for(; num>0; --num)
    {
        float scale_clamped = dmMath::Clamp(scale, 0.0f, 1.0f);
        float sl = *(in_l++);
        float sr = *(in_r++);
        *(out_l++) += sl * scale_clamped;
        *(out_r++) += sr * scale_clamped;
        scale += scale_delta;
    }
}

static inline void ApplyGainAndInterleaveToS16(int16_t* out, float* in[], uint32_t num, float scale, float scale_delta)
{
    float* in_l = in[0];
    float* in_r = in[1];
    for(; num>0; --num)
    {
        float sl = *(in_l++);
        float sr = *(in_r++);
        out[0] = (int16_t)dmMath::Clamp(sl * scale, -32768.0f, 32767.0f);
        out[1] = (int16_t)dmMath::Clamp(sr * scale, -32768.0f, 32767.0f);
        out += 2;
        scale += scale_delta;
    }
}

static inline void ApplyGain(float* out[], float* in[], uint32_t num, float scale, float scale_delta)
{
    float* in_l = in[0];
    float* in_r = in[1];
    float* out_l = out[0];
    float* out_r = out[1];
    for(; num>0; --num)
    {
        float sl = *(in_l++);
        float sr = *(in_r++);
        *(out_l++) = sl * scale;
        *(out_r++) = sr * scale;
        scale += scale_delta;
    }
}

static inline void GatherPowerData(float* in[], uint32_t num, float gain, float& sum_sq_left, float& sum_sq_right, float& max_sq_left, float& max_sq_right)
{
    sum_sq_left = 0;
    sum_sq_right = 0;
    max_sq_left = 0;
    max_sq_right = 0;
    for (uint32_t j = 0; j < num; j++) {
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

static inline void ConvertFromS16(float* out, const int16_t* in, uint32_t num)
{
    for(; num>0; --num)
    {
        *(out++) = (float)*(in++);
    }
}

static inline void ConvertFromS8(float* out, const int8_t* in, uint32_t num)
{
    for(; num>0; --num)
    {
        int16_t s = *(in++);
        *(out++) = (float)(s * 256 + (s & 0xff));
    }
}

static inline void Deinterleave(float* out[], const float* in, uint32_t num)
{
    float* out_l = out[0];
    float* out_r = out[1];
    for(; num>0; --num)
    {
        *(out_l++) = *(in++);
        *(out_r++) = *(in++);
    }
}

static inline void DeinterleaveFromS16(float* out[], const int16_t* in, uint32_t num)
{
    float* out_l = out[0];
    float* out_r = out[1];
    for(; num>0; --num)
    {
        *(out_l++) = (float)*(in++);
        *(out_r++) = (float)*(in++);
    }
}

static inline void DeinterleaveFromS8(float* out[], const int8_t* in, uint32_t num)
{
    float* out_l = out[0];
    float* out_r = out[1];
    for(; num>0; --num)
    {
        int16_t sl = *(in++);
        int16_t sr = *(in++);
        *(out_l++) = (float)(sl * 256 + (sl & 0xff));
        *(out_r++) = (float)(sr * 256 + (sr & 0xff));
    }
}

} // namespace CPU

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

static const DSPImpl fallback_impl =
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
