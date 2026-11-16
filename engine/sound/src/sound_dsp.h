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

#include "sound.h"
#include "sound_dsp_kernels.h"
#include <dlib/math.h>
#include <dlib/profile.h>
#include <dlib/log.h>

namespace dmSound
{

const uint32_t RESAMPLE_FRACTION_BITS = 11; // 2048 polyphase filter banks.
extern const float _pfb[];

namespace Fallback {

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

} // namespace Fallback

// Raw kernel interfaces for output validation and startup selection metadata.
struct DSPImpl
{
    const char* m_Name;
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
    "CPU",
    1,
    Fallback::MixScaledMonoToStereo,
    Fallback::MixScaledStereoToStereo,
    Fallback::MixAndResampleMonoToStereo_Polyphase,
    Fallback::MixAndResampleStereoToStereo_Polyphase,
    Fallback::ApplyClampedGain,
    Fallback::ApplyGainAndInterleaveToS16,
    Fallback::ApplyGain,
    Fallback::GatherPowerData,
    Fallback::ConvertFromS16,
    Fallback::ConvertFromS8,
    Fallback::Deinterleave,
    Fallback::DeinterleaveFromS16,
    Fallback::DeinterleaveFromS8
};


    // Selection is cached by SoundSystem; hot paths branch on the recommended width.
    // Initialize with SelectDSPImpl before calling any dispatch wrapper.
    struct DSPContext
    {
        DSPImplType m_Type;
        uint32_t    m_Lanes;
        uint32_t    m_ResampleLanes;
    };

    bool           SelectDSPImpl(DSPContext* context, DSPImplType type);
    const DSPImpl* GetDSPImpl(DSPImplType type);


    static inline void MixScaledMonoToStereo(const DSPContext& context, float* out[], const float* in, uint32_t num, float scale_l, float scale_r, float scale_delta_l, float scale_delta_r)
    {
#if defined(DM_SOUND_DSP_X86)
        if (context.m_Lanes == 16)
        {
            return DSPAVX512::MixScaledMonoToStereo(out, in, num, scale_l, scale_r, scale_delta_l, scale_delta_r);
        }

        if (context.m_Lanes == 8)
        {
            return DSPAVX2::MixScaledMonoToStereo(out, in, num, scale_l, scale_r, scale_delta_l, scale_delta_r);
        }

        if (context.m_Lanes == 4)
        {
            if (context.m_Type == DSPIMPL_TYPE_SSE42)
            {
                return DSPSSE42::MixScaledMonoToStereo(out, in, num, scale_l, scale_r, scale_delta_l, scale_delta_r);
            }

            return DSPSSE3::MixScaledMonoToStereo(out, in, num, scale_l, scale_r, scale_delta_l, scale_delta_r);
        }

#elif defined(DM_SOUND_DSP_WASM)
        if (context.m_Lanes == 16)
        {
            return DSPWASM::MixScaledMonoToStereo(out, in, num, scale_l, scale_r, scale_delta_l, scale_delta_r);
        }

#elif defined(DM_SOUND_DSP_NEON)
#if defined(DM_SOUND_DSP_SME)
        if (context.m_Lanes == 16 && context.m_Type == DSPIMPL_TYPE_SME)
        {
            return DSPSME::MixScaledMonoToStereo(out, in, num, scale_l, scale_r, scale_delta_l, scale_delta_r);
        }

#endif
#if defined(DM_SOUND_DSP_NEON_WIDE)
        if (context.m_Lanes == 16)
        {
            return DSPNEON16::MixScaledMonoToStereo(out, in, num, scale_l, scale_r, scale_delta_l, scale_delta_r);
        }

        if (context.m_Lanes == 8)
        {
            return DSPNEON8::MixScaledMonoToStereo(out, in, num, scale_l, scale_r, scale_delta_l, scale_delta_r);
        }

#endif
        if (context.m_Lanes == 4)
        {
            return DSPNEON::MixScaledMonoToStereo(out, in, num, scale_l, scale_r, scale_delta_l, scale_delta_r);
        }

#endif
        return Fallback::MixScaledMonoToStereo(out, in, num, scale_l, scale_r, scale_delta_l, scale_delta_r);
    }

    static inline void MixScaledStereoToStereo(const DSPContext& context, float* out[], const float* in_l, const float* in_r, uint32_t num, float scale_l0, float scale_r0, float scale_delta_l0, float scale_delta_r0, float scale_l1, float scale_r1, float scale_delta_l1, float scale_delta_r1)
    {
#if defined(DM_SOUND_DSP_X86)
        if (context.m_Lanes == 16)
        {
            return DSPAVX512::MixScaledStereoToStereo(out, in_l, in_r, num, scale_l0, scale_r0, scale_delta_l0, scale_delta_r0, scale_l1, scale_r1, scale_delta_l1, scale_delta_r1);
        }

        if (context.m_Lanes == 8)
        {
            return DSPAVX2::MixScaledStereoToStereo(out, in_l, in_r, num, scale_l0, scale_r0, scale_delta_l0, scale_delta_r0, scale_l1, scale_r1, scale_delta_l1, scale_delta_r1);
        }

        if (context.m_Lanes == 4)
        {
            if (context.m_Type == DSPIMPL_TYPE_SSE42)
            {
                return DSPSSE42::MixScaledStereoToStereo(out, in_l, in_r, num, scale_l0, scale_r0, scale_delta_l0, scale_delta_r0, scale_l1, scale_r1, scale_delta_l1, scale_delta_r1);
            }

            return DSPSSE3::MixScaledStereoToStereo(out, in_l, in_r, num, scale_l0, scale_r0, scale_delta_l0, scale_delta_r0, scale_l1, scale_r1, scale_delta_l1, scale_delta_r1);
        }

#elif defined(DM_SOUND_DSP_WASM)
        if (context.m_Lanes == 16)
        {
            return DSPWASM::MixScaledStereoToStereo(out, in_l, in_r, num, scale_l0, scale_r0, scale_delta_l0, scale_delta_r0, scale_l1, scale_r1, scale_delta_l1, scale_delta_r1);
        }

#elif defined(DM_SOUND_DSP_NEON)
#if defined(DM_SOUND_DSP_SME)
        if (context.m_Lanes == 16 && context.m_Type == DSPIMPL_TYPE_SME)
        {
            return DSPSME::MixScaledStereoToStereo(out, in_l, in_r, num, scale_l0, scale_r0, scale_delta_l0, scale_delta_r0, scale_l1, scale_r1, scale_delta_l1, scale_delta_r1);
        }

#endif
#if defined(DM_SOUND_DSP_NEON_WIDE)
        if (context.m_Lanes == 16)
        {
            return DSPNEON16::MixScaledStereoToStereo(out, in_l, in_r, num, scale_l0, scale_r0, scale_delta_l0, scale_delta_r0, scale_l1, scale_r1, scale_delta_l1, scale_delta_r1);
        }

        if (context.m_Lanes == 8)
        {
            return DSPNEON8::MixScaledStereoToStereo(out, in_l, in_r, num, scale_l0, scale_r0, scale_delta_l0, scale_delta_r0, scale_l1, scale_r1, scale_delta_l1, scale_delta_r1);
        }

#endif
        if (context.m_Lanes == 4)
        {
            return DSPNEON::MixScaledStereoToStereo(out, in_l, in_r, num, scale_l0, scale_r0, scale_delta_l0, scale_delta_r0, scale_l1, scale_r1, scale_delta_l1, scale_delta_r1);
        }

#endif
        return Fallback::MixScaledStereoToStereo(out, in_l, in_r, num, scale_l0, scale_r0, scale_delta_l0, scale_delta_r0, scale_l1, scale_r1, scale_delta_l1, scale_delta_r1);
    }

    static inline uint64_t MixAndResampleMonoToStereo_Polyphase(const DSPContext& context, float* out[], const float* in, uint32_t num, uint64_t frac, uint64_t delta, float scale_l, float scale_r, float scale_delta_l, float scale_delta_r)
    {
#if defined(DM_SOUND_DSP_X86)
        if (context.m_ResampleLanes == 16)
        {
            return DSPAVX512::MixAndResampleMonoToStereo_Polyphase(out, in, num, frac, delta, scale_l, scale_r, scale_delta_l, scale_delta_r);
        }

        if (context.m_ResampleLanes == 8)
        {
            return DSPAVX2::MixAndResampleMonoToStereo_Polyphase(out, in, num, frac, delta, scale_l, scale_r, scale_delta_l, scale_delta_r);
        }

        if (context.m_ResampleLanes == 4)
        {
            if (context.m_Type == DSPIMPL_TYPE_SSE42)
            {
                return DSPSSE42::MixAndResampleMonoToStereo_Polyphase(out, in, num, frac, delta, scale_l, scale_r, scale_delta_l, scale_delta_r);
            }

            return DSPSSE3::MixAndResampleMonoToStereo_Polyphase(out, in, num, frac, delta, scale_l, scale_r, scale_delta_l, scale_delta_r);
        }

#elif defined(DM_SOUND_DSP_WASM)
        if (context.m_ResampleLanes == 16)
        {
            return DSPWASM::MixAndResampleMonoToStereo_Polyphase(out, in, num, frac, delta, scale_l, scale_r, scale_delta_l, scale_delta_r);
        }

#elif defined(DM_SOUND_DSP_NEON)
#if defined(DM_SOUND_DSP_SME)
        if (context.m_ResampleLanes == 16 && context.m_Type == DSPIMPL_TYPE_SME)
        {
            return DSPSME::MixAndResampleMonoToStereo_Polyphase(out, in, num, frac, delta, scale_l, scale_r, scale_delta_l, scale_delta_r);
        }

#endif
#if defined(DM_SOUND_DSP_NEON_WIDE)
        if (context.m_ResampleLanes == 16)
        {
            return DSPNEON16::MixAndResampleMonoToStereo_Polyphase(out, in, num, frac, delta, scale_l, scale_r, scale_delta_l, scale_delta_r);
        }

        if (context.m_ResampleLanes == 8)
        {
            return DSPNEON8::MixAndResampleMonoToStereo_Polyphase(out, in, num, frac, delta, scale_l, scale_r, scale_delta_l, scale_delta_r);
        }

#endif
        if (context.m_ResampleLanes == 4)
        {
            return DSPNEON::MixAndResampleMonoToStereo_Polyphase(out, in, num, frac, delta, scale_l, scale_r, scale_delta_l, scale_delta_r);
        }

#endif
        return Fallback::MixAndResampleMonoToStereo_Polyphase(out, in, num, frac, delta, scale_l, scale_r, scale_delta_l, scale_delta_r);
    }

    static inline uint64_t MixAndResampleStereoToStereo_Polyphase(const DSPContext& context, float* out[], const float* in_l, const float* in_r, uint32_t num, uint64_t frac, uint64_t delta, float scale_l0, float scale_r0, float scale_delta_l0, float scale_delta_r0, float scale_l1, float scale_r1, float scale_delta_l1, float scale_delta_r1)
    {
#if defined(DM_SOUND_DSP_X86)
        if (context.m_ResampleLanes == 16)
        {
            return DSPAVX512::MixAndResampleStereoToStereo_Polyphase(out, in_l, in_r, num, frac, delta, scale_l0, scale_r0, scale_delta_l0, scale_delta_r0, scale_l1, scale_r1, scale_delta_l1, scale_delta_r1);
        }

        if (context.m_ResampleLanes == 8)
        {
            return DSPAVX2::MixAndResampleStereoToStereo_Polyphase(out, in_l, in_r, num, frac, delta, scale_l0, scale_r0, scale_delta_l0, scale_delta_r0, scale_l1, scale_r1, scale_delta_l1, scale_delta_r1);
        }

        if (context.m_ResampleLanes == 4)
        {
            if (context.m_Type == DSPIMPL_TYPE_SSE42)
            {
                return DSPSSE42::MixAndResampleStereoToStereo_Polyphase(out, in_l, in_r, num, frac, delta, scale_l0, scale_r0, scale_delta_l0, scale_delta_r0, scale_l1, scale_r1, scale_delta_l1, scale_delta_r1);
            }

            return DSPSSE3::MixAndResampleStereoToStereo_Polyphase(out, in_l, in_r, num, frac, delta, scale_l0, scale_r0, scale_delta_l0, scale_delta_r0, scale_l1, scale_r1, scale_delta_l1, scale_delta_r1);
        }

#elif defined(DM_SOUND_DSP_WASM)
        if (context.m_ResampleLanes == 16)
        {
            return DSPWASM::MixAndResampleStereoToStereo_Polyphase(out, in_l, in_r, num, frac, delta, scale_l0, scale_r0, scale_delta_l0, scale_delta_r0, scale_l1, scale_r1, scale_delta_l1, scale_delta_r1);
        }

#elif defined(DM_SOUND_DSP_NEON)
#if defined(DM_SOUND_DSP_SME)
        if (context.m_ResampleLanes == 16 && context.m_Type == DSPIMPL_TYPE_SME)
        {
            return DSPSME::MixAndResampleStereoToStereo_Polyphase(out, in_l, in_r, num, frac, delta, scale_l0, scale_r0, scale_delta_l0, scale_delta_r0, scale_l1, scale_r1, scale_delta_l1, scale_delta_r1);
        }

#endif
#if defined(DM_SOUND_DSP_NEON_WIDE)
        if (context.m_ResampleLanes == 16)
        {
            return DSPNEON16::MixAndResampleStereoToStereo_Polyphase(out, in_l, in_r, num, frac, delta, scale_l0, scale_r0, scale_delta_l0, scale_delta_r0, scale_l1, scale_r1, scale_delta_l1, scale_delta_r1);
        }

        if (context.m_ResampleLanes == 8)
        {
            return DSPNEON8::MixAndResampleStereoToStereo_Polyphase(out, in_l, in_r, num, frac, delta, scale_l0, scale_r0, scale_delta_l0, scale_delta_r0, scale_l1, scale_r1, scale_delta_l1, scale_delta_r1);
        }

#endif
        if (context.m_ResampleLanes == 4)
        {
            return DSPNEON::MixAndResampleStereoToStereo_Polyphase(out, in_l, in_r, num, frac, delta, scale_l0, scale_r0, scale_delta_l0, scale_delta_r0, scale_l1, scale_r1, scale_delta_l1, scale_delta_r1);
        }

#endif
        return Fallback::MixAndResampleStereoToStereo_Polyphase(out, in_l, in_r, num, frac, delta, scale_l0, scale_r0, scale_delta_l0, scale_delta_r0, scale_l1, scale_r1, scale_delta_l1, scale_delta_r1);
    }

    static inline void ApplyClampedGain(const DSPContext& context, float* out[], float* in[], uint32_t num, float scale, float scale_delta)
    {
#if defined(DM_SOUND_DSP_X86)
        if (context.m_Lanes == 16)
        {
            return DSPAVX512::ApplyClampedGain(out, in, num, scale, scale_delta);
        }

        if (context.m_Lanes == 8)
        {
            return DSPAVX2::ApplyClampedGain(out, in, num, scale, scale_delta);
        }

        if (context.m_Lanes == 4)
        {
            if (context.m_Type == DSPIMPL_TYPE_SSE42)
            {
                return DSPSSE42::ApplyClampedGain(out, in, num, scale, scale_delta);
            }

            return DSPSSE3::ApplyClampedGain(out, in, num, scale, scale_delta);
        }

#elif defined(DM_SOUND_DSP_WASM)
        if (context.m_Lanes == 16)
        {
            return DSPWASM::ApplyClampedGain(out, in, num, scale, scale_delta);
        }

#elif defined(DM_SOUND_DSP_NEON)
#if defined(DM_SOUND_DSP_SME)
        if (context.m_Lanes == 16 && context.m_Type == DSPIMPL_TYPE_SME)
        {
            return DSPSME::ApplyClampedGain(out, in, num, scale, scale_delta);
        }

#endif
#if defined(DM_SOUND_DSP_NEON_WIDE)
        if (context.m_Lanes == 16)
        {
            return DSPNEON16::ApplyClampedGain(out, in, num, scale, scale_delta);
        }

        if (context.m_Lanes == 8)
        {
            return DSPNEON8::ApplyClampedGain(out, in, num, scale, scale_delta);
        }

#endif
        if (context.m_Lanes == 4)
        {
            return DSPNEON::ApplyClampedGain(out, in, num, scale, scale_delta);
        }

#endif
        return Fallback::ApplyClampedGain(out, in, num, scale, scale_delta);
    }

    static inline void ApplyGainAndInterleaveToS16(const DSPContext& context, int16_t* out, float* in[], uint32_t num, float scale, float scale_delta)
    {
#if defined(DM_SOUND_DSP_X86)
        if (context.m_Lanes == 16)
        {
            return DSPAVX512::ApplyGainAndInterleaveToS16(out, in, num, scale, scale_delta);
        }

        if (context.m_Lanes == 8)
        {
            return DSPAVX2::ApplyGainAndInterleaveToS16(out, in, num, scale, scale_delta);
        }

        if (context.m_Lanes == 4)
        {
            if (context.m_Type == DSPIMPL_TYPE_SSE42)
            {
                return DSPSSE42::ApplyGainAndInterleaveToS16(out, in, num, scale, scale_delta);
            }

            return DSPSSE3::ApplyGainAndInterleaveToS16(out, in, num, scale, scale_delta);
        }

#elif defined(DM_SOUND_DSP_WASM)
        if (context.m_Lanes == 16)
        {
            return DSPWASM::ApplyGainAndInterleaveToS16(out, in, num, scale, scale_delta);
        }

#elif defined(DM_SOUND_DSP_NEON)
#if defined(DM_SOUND_DSP_SME)
        if (context.m_Lanes == 16 && context.m_Type == DSPIMPL_TYPE_SME)
        {
            return DSPSME::ApplyGainAndInterleaveToS16(out, in, num, scale, scale_delta);
        }

#endif
#if defined(DM_SOUND_DSP_NEON_WIDE)
        if (context.m_Lanes == 16)
        {
            return DSPNEON16::ApplyGainAndInterleaveToS16(out, in, num, scale, scale_delta);
        }

        if (context.m_Lanes == 8)
        {
            return DSPNEON8::ApplyGainAndInterleaveToS16(out, in, num, scale, scale_delta);
        }

#endif
        if (context.m_Lanes == 4)
        {
            return DSPNEON::ApplyGainAndInterleaveToS16(out, in, num, scale, scale_delta);
        }

#endif
        return Fallback::ApplyGainAndInterleaveToS16(out, in, num, scale, scale_delta);
    }

    static inline void ApplyGain(const DSPContext& context, float* out[], float* in[], uint32_t num, float scale, float scale_delta)
    {
#if defined(DM_SOUND_DSP_X86)
        if (context.m_Lanes == 16)
        {
            return DSPAVX512::ApplyGain(out, in, num, scale, scale_delta);
        }

        if (context.m_Lanes == 8)
        {
            return DSPAVX2::ApplyGain(out, in, num, scale, scale_delta);
        }

        if (context.m_Lanes == 4)
        {
            if (context.m_Type == DSPIMPL_TYPE_SSE42)
            {
                return DSPSSE42::ApplyGain(out, in, num, scale, scale_delta);
            }

            return DSPSSE3::ApplyGain(out, in, num, scale, scale_delta);
        }

#elif defined(DM_SOUND_DSP_WASM)
        if (context.m_Lanes == 16)
        {
            return DSPWASM::ApplyGain(out, in, num, scale, scale_delta);
        }

#elif defined(DM_SOUND_DSP_NEON)
#if defined(DM_SOUND_DSP_SME)
        if (context.m_Lanes == 16 && context.m_Type == DSPIMPL_TYPE_SME)
        {
            return DSPSME::ApplyGain(out, in, num, scale, scale_delta);
        }

#endif
#if defined(DM_SOUND_DSP_NEON_WIDE)
        if (context.m_Lanes == 16)
        {
            return DSPNEON16::ApplyGain(out, in, num, scale, scale_delta);
        }

        if (context.m_Lanes == 8)
        {
            return DSPNEON8::ApplyGain(out, in, num, scale, scale_delta);
        }

#endif
        if (context.m_Lanes == 4)
        {
            return DSPNEON::ApplyGain(out, in, num, scale, scale_delta);
        }

#endif
        return Fallback::ApplyGain(out, in, num, scale, scale_delta);
    }

    static inline void GatherPowerData(const DSPContext& context, float* in[], uint32_t num, float gain, float& sum_sq_left, float& sum_sq_right, float& max_sq_left, float& max_sq_right)
    {
#if defined(DM_SOUND_DSP_X86)
        if (context.m_Lanes == 16)
        {
            return DSPAVX512::GatherPowerData(in, num, gain, sum_sq_left, sum_sq_right, max_sq_left, max_sq_right);
        }

        if (context.m_Lanes == 8)
        {
            return DSPAVX2::GatherPowerData(in, num, gain, sum_sq_left, sum_sq_right, max_sq_left, max_sq_right);
        }

        if (context.m_Lanes == 4)
        {
            if (context.m_Type == DSPIMPL_TYPE_SSE42)
            {
                return DSPSSE42::GatherPowerData(in, num, gain, sum_sq_left, sum_sq_right, max_sq_left, max_sq_right);
            }

            return DSPSSE3::GatherPowerData(in, num, gain, sum_sq_left, sum_sq_right, max_sq_left, max_sq_right);
        }

#elif defined(DM_SOUND_DSP_WASM)
        if (context.m_Lanes == 16)
        {
            return DSPWASM::GatherPowerData(in, num, gain, sum_sq_left, sum_sq_right, max_sq_left, max_sq_right);
        }

#elif defined(DM_SOUND_DSP_NEON)
#if defined(DM_SOUND_DSP_SME)
        if (context.m_Lanes == 16 && context.m_Type == DSPIMPL_TYPE_SME)
        {
            return DSPSME::GatherPowerData(in, num, gain, sum_sq_left, sum_sq_right, max_sq_left, max_sq_right);
        }

#endif
#if defined(DM_SOUND_DSP_NEON_WIDE)
        if (context.m_Lanes == 16)
        {
            return DSPNEON16::GatherPowerData(in, num, gain, sum_sq_left, sum_sq_right, max_sq_left, max_sq_right);
        }

        if (context.m_Lanes == 8)
        {
            return DSPNEON8::GatherPowerData(in, num, gain, sum_sq_left, sum_sq_right, max_sq_left, max_sq_right);
        }

#endif
        if (context.m_Lanes == 4)
        {
            return DSPNEON::GatherPowerData(in, num, gain, sum_sq_left, sum_sq_right, max_sq_left, max_sq_right);
        }

#endif
        return Fallback::GatherPowerData(in, num, gain, sum_sq_left, sum_sq_right, max_sq_left, max_sq_right);
    }

    static inline void ConvertFromS16(const DSPContext& context, float* out, const int16_t* in, uint32_t num)
    {
#if defined(DM_SOUND_DSP_X86)
        if (context.m_Lanes == 16)
        {
            return DSPAVX512::ConvertFromS16(out, in, num);
        }

        if (context.m_Lanes == 8)
        {
            return DSPAVX2::ConvertFromS16(out, in, num);
        }

        if (context.m_Lanes == 4)
        {
            if (context.m_Type == DSPIMPL_TYPE_SSE42)
            {
                return DSPSSE42::ConvertFromS16(out, in, num);
            }

            return DSPSSE3::ConvertFromS16(out, in, num);
        }

#elif defined(DM_SOUND_DSP_WASM)
        if (context.m_Lanes == 16)
        {
            return DSPWASM::ConvertFromS16(out, in, num);
        }

#elif defined(DM_SOUND_DSP_NEON)
#if defined(DM_SOUND_DSP_SME)
        if (context.m_Lanes == 16 && context.m_Type == DSPIMPL_TYPE_SME)
        {
            return DSPSME::ConvertFromS16(out, in, num);
        }

#endif
#if defined(DM_SOUND_DSP_NEON_WIDE)
        if (context.m_Lanes == 16)
        {
            return DSPNEON16::ConvertFromS16(out, in, num);
        }

        if (context.m_Lanes == 8)
        {
            return DSPNEON8::ConvertFromS16(out, in, num);
        }

#endif
        if (context.m_Lanes == 4)
        {
            return DSPNEON::ConvertFromS16(out, in, num);
        }

#endif
        return Fallback::ConvertFromS16(out, in, num);
    }

    static inline void ConvertFromS8(const DSPContext& context, float* out, const int8_t* in, uint32_t num)
    {
#if defined(DM_SOUND_DSP_X86)
        if (context.m_Lanes == 16)
        {
            return DSPAVX512::ConvertFromS8(out, in, num);
        }

        if (context.m_Lanes == 8)
        {
            return DSPAVX2::ConvertFromS8(out, in, num);
        }

        if (context.m_Lanes == 4)
        {
            if (context.m_Type == DSPIMPL_TYPE_SSE42)
            {
                return DSPSSE42::ConvertFromS8(out, in, num);
            }

            return DSPSSE3::ConvertFromS8(out, in, num);
        }

#elif defined(DM_SOUND_DSP_WASM)
        if (context.m_Lanes == 16)
        {
            return DSPWASM::ConvertFromS8(out, in, num);
        }

#elif defined(DM_SOUND_DSP_NEON)
#if defined(DM_SOUND_DSP_SME)
        if (context.m_Lanes == 16 && context.m_Type == DSPIMPL_TYPE_SME)
        {
            return DSPSME::ConvertFromS8(out, in, num);
        }

#endif
#if defined(DM_SOUND_DSP_NEON_WIDE)
        if (context.m_Lanes == 16)
        {
            return DSPNEON16::ConvertFromS8(out, in, num);
        }

        if (context.m_Lanes == 8)
        {
            return DSPNEON8::ConvertFromS8(out, in, num);
        }

#endif
        if (context.m_Lanes == 4)
        {
            return DSPNEON::ConvertFromS8(out, in, num);
        }

#endif
        return Fallback::ConvertFromS8(out, in, num);
    }

    static inline void Deinterleave(const DSPContext& context, float* out[], const float* in, uint32_t num)
    {
#if defined(DM_SOUND_DSP_X86)
        if (context.m_Lanes == 16)
        {
            return DSPAVX512::Deinterleave(out, in, num);
        }

        if (context.m_Lanes == 8)
        {
            return DSPAVX2::Deinterleave(out, in, num);
        }

        if (context.m_Lanes == 4)
        {
            if (context.m_Type == DSPIMPL_TYPE_SSE42)
            {
                return DSPSSE42::Deinterleave(out, in, num);
            }

            return DSPSSE3::Deinterleave(out, in, num);
        }

#elif defined(DM_SOUND_DSP_WASM)
        if (context.m_Lanes == 16)
        {
            return DSPWASM::Deinterleave(out, in, num);
        }

#elif defined(DM_SOUND_DSP_NEON)
#if defined(DM_SOUND_DSP_SME)
        if (context.m_Lanes == 16 && context.m_Type == DSPIMPL_TYPE_SME)
        {
            return DSPSME::Deinterleave(out, in, num);
        }

#endif
#if defined(DM_SOUND_DSP_NEON_WIDE)
        if (context.m_Lanes == 16)
        {
            return DSPNEON16::Deinterleave(out, in, num);
        }

        if (context.m_Lanes == 8)
        {
            return DSPNEON8::Deinterleave(out, in, num);
        }

#endif
        if (context.m_Lanes == 4)
        {
            return DSPNEON::Deinterleave(out, in, num);
        }

#endif
        return Fallback::Deinterleave(out, in, num);
    }

    static inline void DeinterleaveFromS16(const DSPContext& context, float* out[], const int16_t* in, uint32_t num)
    {
#if defined(DM_SOUND_DSP_X86)
        if (context.m_Lanes == 16)
        {
            return DSPAVX512::DeinterleaveFromS16(out, in, num);
        }

        if (context.m_Lanes == 8)
        {
            return DSPAVX2::DeinterleaveFromS16(out, in, num);
        }

        if (context.m_Lanes == 4)
        {
            if (context.m_Type == DSPIMPL_TYPE_SSE42)
            {
                return DSPSSE42::DeinterleaveFromS16(out, in, num);
            }

            return DSPSSE3::DeinterleaveFromS16(out, in, num);
        }

#elif defined(DM_SOUND_DSP_WASM)
        if (context.m_Lanes == 16)
        {
            return DSPWASM::DeinterleaveFromS16(out, in, num);
        }

#elif defined(DM_SOUND_DSP_NEON)
#if defined(DM_SOUND_DSP_SME)
        if (context.m_Lanes == 16 && context.m_Type == DSPIMPL_TYPE_SME)
        {
            return DSPSME::DeinterleaveFromS16(out, in, num);
        }

#endif
#if defined(DM_SOUND_DSP_NEON_WIDE)
        if (context.m_Lanes == 16)
        {
            return DSPNEON16::DeinterleaveFromS16(out, in, num);
        }

        if (context.m_Lanes == 8)
        {
            return DSPNEON8::DeinterleaveFromS16(out, in, num);
        }

#endif
        if (context.m_Lanes == 4)
        {
            return DSPNEON::DeinterleaveFromS16(out, in, num);
        }

#endif
        return Fallback::DeinterleaveFromS16(out, in, num);
    }

    static inline void DeinterleaveFromS8(const DSPContext& context, float* out[], const int8_t* in, uint32_t num)
    {
#if defined(DM_SOUND_DSP_X86)
        if (context.m_Lanes == 16)
        {
            return DSPAVX512::DeinterleaveFromS8(out, in, num);
        }

        if (context.m_Lanes == 8)
        {
            return DSPAVX2::DeinterleaveFromS8(out, in, num);
        }

        if (context.m_Lanes == 4)
        {
            if (context.m_Type == DSPIMPL_TYPE_SSE42)
            {
                return DSPSSE42::DeinterleaveFromS8(out, in, num);
            }

            return DSPSSE3::DeinterleaveFromS8(out, in, num);
        }

#elif defined(DM_SOUND_DSP_WASM)
        if (context.m_Lanes == 16)
        {
            return DSPWASM::DeinterleaveFromS8(out, in, num);
        }

#elif defined(DM_SOUND_DSP_NEON)
#if defined(DM_SOUND_DSP_SME)
        if (context.m_Lanes == 16 && context.m_Type == DSPIMPL_TYPE_SME)
        {
            return DSPSME::DeinterleaveFromS8(out, in, num);
        }

#endif
#if defined(DM_SOUND_DSP_NEON_WIDE)
        if (context.m_Lanes == 16)
        {
            return DSPNEON16::DeinterleaveFromS8(out, in, num);
        }

        if (context.m_Lanes == 8)
        {
            return DSPNEON8::DeinterleaveFromS8(out, in, num);
        }

#endif
        if (context.m_Lanes == 4)
        {
            return DSPNEON::DeinterleaveFromS8(out, in, num);
        }

#endif
        return Fallback::DeinterleaveFromS8(out, in, num);
    }

} // namespace dmSound

#endif // DM_SOUND_DSP_H
