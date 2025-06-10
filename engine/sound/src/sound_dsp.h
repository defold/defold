// Copyright 2020-2024 The Defold Foundation
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


#include "sound_pfb.h"
#include "dsimd.h"

#include <dlib/log.h>

#if defined(DM_SIMD_WASM)
    #define DM_SOUND_DSP_WASM
#elif defined(DM_SIMD_SSE2)
    #define DM_SOUND_DSP_SSE2
#endif

// Make sure we use compile time selected fallback code if nothing is selected at all
#if !defined(DM_SOUND_DSP_IMPL)
#define DM_SOUND_DSP_IMPL Fallback
#endif

namespace dmSound
{

// ----------------------------------------------------------------------------------------------------------------------------------------------------------------
// ----------------------------------------------------------------------------------------------------------------------------------------------------------------

#if defined(DM_SOUND_DSP_WASM)

namespace WASM {
//
// WASM
//

typedef __f32x4 vec4;

static inline float FilterSampleFIR8(const float* frames, uint64_t frac_pos)
{
    uint32_t pi = ((frac_pos >> (RESAMPLE_FRACTION_BITS - 11)) << 3) & (2047 << 3); // 8 tabs, 2048 (11 fractional bits) banks
    const float* c = &_pfb[pi];

    vec4 tmp0 = (vec4)wasm_v128_load(&frames[-3]) * *(const vec4*)&c[0]; // intrinsic is unaligned load
    vec4 tmp1 = (vec4)wasm_v128_load(&frames[ 1]) * *(const vec4*)&c[4];
    tmp0 = (vec4)wasm_i32x4_shuffle(tmp0, tmp1, 0, 4, 2, 6) + (vec4)wasm_i32x4_shuffle(tmp0, tmp1, 1, 5, 3, 7); // X0+Y0, X1+Y1, Z0+W0, Z1+W1
    tmp0 += (vec4)wasm_i32x4_shuffle(tmp0, tmp0, 2, 3, 0, 0);
    return tmp0[0] + tmp0[1];
}

static inline void MixScaledMonoToStereo(float* out[], const float* in, uint32_t num, float scale_l, float scale_r, float scale_delta_l, float scale_delta_r)
{
    // setup ramps
    vec4 scld = wasm_f32x4_splat(scale_delta_l);
    vec4 scrd = wasm_f32x4_splat(scale_delta_r);
    vec4 scl  = wasm_f32x4_splat(scale_l) + scld * wasm_f32x4_make(0.0f, 1.0f, 2.0f, 3.0f);
    vec4 scr  = wasm_f32x4_splat(scale_r) + scrd * wasm_f32x4_make(0.0f, 1.0f, 2.0f, 3.0f);
    scld *= 4.0f;
    scrd *= 4.0f;

    // vectorized mix
    vec4* vl = (vec4*)out[0];
    vec4* vr = (vec4*)out[1];
    vec4* vin = (vec4*)in;
    for(; num>3; num-=4)
    {
        vec4 s = *(vin++);
        *(vl++) += s * scl;
        *(vr++) += s * scr;
        scl += scld;
        scr += scrd;
    }
    // process any remaining samples
    float* fl = (float*)vl;
    float* fr = (float*)vr;
    float* fin = (float*)vin;
    scale_l = scl[0];
    scale_r = scr[0];
    for(; num>0; --num)
    {
        float s = *(fin++);
        *(fl++) += s * scale_l;
        *(fr++) += s * scale_r;
        scale_l += scale_delta_l;
        scale_r += scale_delta_r;
    }
}

static inline void MixScaledStereoToStereo(float* out[], const float* in_l, const float* in_r, uint32_t num, float scale_l0, float scale_r0, float scale_delta_l0, float scale_delta_r0,
                                                                                                                     float scale_l1, float scale_r1, float scale_delta_l1, float scale_delta_r1)
{
    // setup ramps
    vec4 scld0 = wasm_f32x4_splat(scale_delta_l0);
    vec4 scrd0 = wasm_f32x4_splat(scale_delta_r0);
    vec4 scld1 = wasm_f32x4_splat(scale_delta_l1);
    vec4 scrd1 = wasm_f32x4_splat(scale_delta_r1);
    vec4 scl0  = wasm_f32x4_splat(scale_l0) + scld0 * wasm_f32x4_make(0.0f, 1.0f, 2.0f, 3.0f);
    vec4 scr0  = wasm_f32x4_splat(scale_r0) + scrd0 * wasm_f32x4_make(0.0f, 1.0f, 2.0f, 3.0f);
    vec4 scl1  = wasm_f32x4_splat(scale_l1) + scld1 * wasm_f32x4_make(0.0f, 1.0f, 2.0f, 3.0f);
    vec4 scr1  = wasm_f32x4_splat(scale_r1) + scrd1 * wasm_f32x4_make(0.0f, 1.0f, 2.0f, 3.0f);
    scld0 *= 4.0f;
    scrd0 *= 4.0f;
    scld1 *= 4.0f;
    scrd1 *= 4.0f;

    // vectorized mix
    vec4* vl = (vec4*)out[0];
    vec4* vr = (vec4*)out[1];
    vec4* vin_l = (vec4*)in_l;
    vec4* vin_r = (vec4*)in_r;
    for(; num>3; num-=4)
    {
        vec4 sl = *(vin_l++);
        vec4 sr = *(vin_r++);
        *(vl++) += sl * scl0 + sr * scl1;
        *(vr++) += sl * scr0 + sr * scr1;
        scl0 += scld0;
        scr0 += scrd0;
        scl1 += scld1;
        scr1 += scrd1;
    }
    // process any remaining samples
    float* fl = (float*)vl;
    float* fr = (float*)vr;
    float* fin_l = (float*)vin_l;
    float* fin_r = (float*)vin_r;
    scale_l0 = scl0[0];
    scale_r0 = scr0[0];
    scale_l1 = scl1[0];
    scale_r1 = scr1[0];
    for(; num>0; --num)
    {
        float sl = *(fin_l++);
        float sr = *(fin_r++);
        *(fl++) += sl * scale_l0 + sr * scale_l1;
        *(fr++) += sl * scale_r0 + sr * scale_r1;
        scale_l0 += scale_delta_l0;
        scale_r0 += scale_delta_r0;
        scale_l1 += scale_delta_l1;
        scale_r1 += scale_delta_r1;
    }
}

static inline uint64_t MixAndResampleMonoToStereo_Polyphase(float* out[], const float* in, uint32_t num, uint64_t frac, uint64_t delta, float scale_l, float scale_r, float scale_delta_l, float scale_delta_r)
{
    DM_PROFILE("MixAndResampleMonoToStereo_Polyphase_WASM");

    // setup ramps
    vec4 scld = wasm_f32x4_splat(scale_delta_l);
    vec4 scrd = wasm_f32x4_splat(scale_delta_r);
    vec4 scl  = wasm_f32x4_splat(scale_l) + scld * wasm_f32x4_make(0.0f, 1.0f, 2.0f, 3.0f);
    vec4 scr  = wasm_f32x4_splat(scale_r) + scrd * wasm_f32x4_make(0.0f, 1.0f, 2.0f, 3.0f);
    scld *= 4.0f;
    scrd *= 4.0f;

    // vectorized mix
    vec4* vl = (vec4*)out[0];
    vec4* vr = (vec4*)out[1];
    for(; num>3; num-=4)
    {
        float s0 = FilterSampleFIR8(&in[frac >> RESAMPLE_FRACTION_BITS], frac);
        frac += delta;
        float s1 = FilterSampleFIR8(&in[frac >> RESAMPLE_FRACTION_BITS], frac);
        frac += delta;
        float s2 = FilterSampleFIR8(&in[frac >> RESAMPLE_FRACTION_BITS], frac);
        frac += delta;
        float s3 = FilterSampleFIR8(&in[frac >> RESAMPLE_FRACTION_BITS], frac);
        frac += delta;
        vec4 s = wasm_f32x4_make(s0, s1, s2, s3);

        *(vl++) += s * scl;
        *(vr++) += s * scr;
        scl += scld;
        scr += scrd;
    }
    // process any remaining samples
    float* fl = (float*)vl;
    float* fr = (float*)vr;
    scale_l = scl[0];
    scale_r = scr[0];
    for(; num>0; --num)
    {
        float s = FilterSampleFIR8(&in[frac >> RESAMPLE_FRACTION_BITS], frac);

        *(fl++) += s * scale_l;
        *(fr++) += s * scale_r;
        scale_l += scale_delta_l;
        scale_r += scale_delta_r;

        frac += delta;
    }
    return frac;
}

static inline uint64_t MixAndResampleStereoToStereo_Polyphase(float* out[], const float* in_l, const float* in_r, uint32_t num, uint64_t frac, uint64_t delta, float scale_l0, float scale_r0, float scale_delta_l0, float scale_delta_r0,
                                                                                                                                                              float scale_l1, float scale_r1, float scale_delta_l1, float scale_delta_r1)
{
    DM_PROFILE("MixAndResampleStereoToStereo_Polyphase_WASM");

    // setup ramps
    vec4 scld0 = wasm_f32x4_splat(scale_delta_l0);
    vec4 scrd0 = wasm_f32x4_splat(scale_delta_r0);
    vec4 scld1 = wasm_f32x4_splat(scale_delta_l1);
    vec4 scrd1 = wasm_f32x4_splat(scale_delta_r1);
    vec4 scl0  = wasm_f32x4_splat(scale_l0) + scld0 * wasm_f32x4_make(0.0f, 1.0f, 2.0f, 3.0f);
    vec4 scr0  = wasm_f32x4_splat(scale_r0) + scrd0 * wasm_f32x4_make(0.0f, 1.0f, 2.0f, 3.0f);
    vec4 scl1  = wasm_f32x4_splat(scale_l1) + scld1 * wasm_f32x4_make(0.0f, 1.0f, 2.0f, 3.0f);
    vec4 scr1  = wasm_f32x4_splat(scale_r1) + scrd1 * wasm_f32x4_make(0.0f, 1.0f, 2.0f, 3.0f);
    scld0 *= 4.0f;
    scrd0 *= 4.0f;
    scld1 *= 4.0f;
    scrd1 *= 4.0f;

    // vectorized mix
    vec4* vl = (vec4*)out[0];
    vec4* vr = (vec4*)out[1];
    for(; num>3; num-=4)
    {
        float s0l = FilterSampleFIR8(&in_l[frac >> RESAMPLE_FRACTION_BITS], frac);
        float s0r = FilterSampleFIR8(&in_r[frac >> RESAMPLE_FRACTION_BITS], frac);
        frac += delta;
        float s1l = FilterSampleFIR8(&in_l[frac >> RESAMPLE_FRACTION_BITS], frac);
        float s1r = FilterSampleFIR8(&in_r[frac >> RESAMPLE_FRACTION_BITS], frac);
        frac += delta;
        float s2l = FilterSampleFIR8(&in_l[frac >> RESAMPLE_FRACTION_BITS], frac);
        float s2r = FilterSampleFIR8(&in_r[frac >> RESAMPLE_FRACTION_BITS], frac);
        frac += delta;
        float s3l = FilterSampleFIR8(&in_l[frac >> RESAMPLE_FRACTION_BITS], frac);
        float s3r = FilterSampleFIR8(&in_r[frac >> RESAMPLE_FRACTION_BITS], frac);
        frac += delta;
        vec4 sl = wasm_f32x4_make(s0l, s1l, s2l, s3l);
        vec4 sr = wasm_f32x4_make(s0r, s1r, s2r, s3r);

        *(vl++) += sl * scl0 + sr * scl1;
        *(vr++) += sl * scr0 + sr * scr1;
        scl0 += scld0;
        scr0 += scrd0;
        scl1 += scld1;
        scr1 += scrd1;
    }
    // process any remaining samples
    float* fl = (float*)vl;
    float* fr = (float*)vr;
    scale_l0 = scl0[0];
    scale_r0 = scr0[0];
    scale_l1 = scl1[0];
    scale_r1 = scr1[0];
    for(; num>0; --num)
    {
        float sl = FilterSampleFIR8(&in_l[frac >> RESAMPLE_FRACTION_BITS], frac);
        float sr = FilterSampleFIR8(&in_r[frac >> RESAMPLE_FRACTION_BITS], frac);

        *(fl++) += sl * scale_l0 + sr * scale_l1;
        *(fr++) += sl * scale_r0 + sr * scale_r1;
        scale_l0 += scale_delta_l0;
        scale_r0 += scale_delta_r0;
        scale_l1 += scale_delta_l1;
        scale_r1 += scale_delta_r1;

        frac += delta;
    }
    return frac;
}

static inline void ApplyClampedGain(float* out[], float* in[], uint32_t num, float scale, float scale_delta)
{
    // setup ramp
    vec4 sc = wasm_f32x4_splat(scale);
    vec4 scd = wasm_f32x4_splat(scale_delta);
    sc = wasm_f32x4_splat(scale) + scd * wasm_f32x4_make(0.0f, 1.0f, 2.0f, 3.0f);
    scd *= 4.0f;

    vec4 zero = wasm_f32x4_splat(0.0f);
    vec4 one = wasm_f32x4_splat(1.0f);

    vec4* vin_l = (vec4*)in[0];
    vec4* vin_r = (vec4*)in[1];
    vec4* vout_l = (vec4*)out[0];
    vec4* vout_r = (vec4*)out[1];
    for(; num>3; num-=4)
    {
        vec4 sc_clamped = wasm_f32x4_pmin(wasm_f32x4_pmax(sc, zero), one);
        *(vout_l++) += *(vin_l++) * sc_clamped;
        *(vout_r++) += *(vin_r++) * sc_clamped;
        sc += scd;
    }

    float* in_l = (float*)vin_l;
    float* in_r = (float*)vin_r;
    float* out_l = (float*)vout_l;
    float* out_r = (float*)vout_r;
    scale = sc[0];
    for(; num>0; --num)
    {
        float scale_clamped = dmMath::Clamp(scale, 0.0f, 1.0f);
        *(out_l++) += *(in_l++) * scale_clamped;
        *(out_r++) += *(in_r++) * scale_clamped;
        scale += scale_delta;
    }
}

static inline void ApplyGainAndInterleaveToS16(int16_t* out, float* in[], uint32_t num, float scale, float scale_delta)
{
    // setup ramp
    vec4 sc = wasm_f32x4_splat(scale);
    vec4 scd = wasm_f32x4_splat(scale_delta);
    sc = wasm_f32x4_splat(scale) + scd * wasm_f32x4_make(0.0f, 1.0f, 2.0f, 3.0f);
    scd *= 4.0f;

    vec4* vin_l = (vec4*)in[0];
    vec4* vin_r = (vec4*)in[1];
    v128_t* vout = (v128_t*)out;
    for(; num>7; num-=8)
    {
        vec4 l0 = *(vin_l++) * sc;
        vec4 r0 = *(vin_r++) * sc;
        sc += scd;
        vec4 l1 = *(vin_l++) * sc;
        vec4 r1 = *(vin_r++) * sc;
        sc += scd;
        v128_t li = wasm_i16x8_narrow_i32x4(wasm_i32x4_trunc_sat_f32x4(l0), wasm_i32x4_trunc_sat_f32x4(l1));
        v128_t ri = wasm_i16x8_narrow_i32x4(wasm_i32x4_trunc_sat_f32x4(r0), wasm_i32x4_trunc_sat_f32x4(r1));
        *(vout++) = wasm_i16x8_shuffle(li, ri, 0,  8, 1,  9, 2, 10, 3, 11);
        *(vout++) = wasm_i16x8_shuffle(li, ri, 4, 12, 5, 13, 6, 14, 7, 15);
    }

    float* in_l = (float*)vin_l;
    float* in_r = (float*)vin_r;
    scale = sc[0];
    out = (int16_t*)vout;
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
    // setup ramp
    vec4 sc = wasm_f32x4_splat(scale);
    vec4 scd = wasm_f32x4_splat(scale_delta);
    sc = wasm_f32x4_splat(scale) + scd * wasm_f32x4_make(0.0f, 1.0f, 2.0f, 3.0f);
    scd *= 4.0f;

    vec4* vin_l = (vec4*)in[0];
    vec4* vin_r = (vec4*)in[1];
    vec4* vout_l = (vec4*)out[0];
    vec4* vout_r = (vec4*)out[1];
    for(; num>3; num-=4)
    {
        *(vout_l++) = *(vin_l++) * sc;
        *(vout_r++) = *(vin_r++) * sc;
        sc += scd;
    }

    float* in_l = (float*)vin_l;
    float* in_r = (float*)vin_r;
    float* out_l = (float*)vout_l;
    float* out_r = (float*)vout_r;
    scale = sc[0];
    for(; num>0; --num)
    {
        *(out_l++) = *(in_l++) * scale;
        *(out_r++) = *(in_r++) * scale;
        scale += scale_delta;
    }
}

static inline void GatherPowerData(float* in[], uint32_t num, float gain, float& sum_sq_left, float& sum_sq_right, float& max_sq_left, float& max_sq_right)
{
    vec4* in_l = (vec4*)in[0];
    vec4* in_r = (vec4*)in[1];

    vec4 maxl = wasm_f32x4_splat(0.0f);
    vec4 suml = maxl;
    vec4 maxr = maxl;
    vec4 sumr = maxl;

    vec4 scale = wasm_f32x4_splat(gain);

    for(; num>3; num-=4)
    {
        vec4 sl = *(in_l++) * scale;
        vec4 sr = *(in_r++) * scale;
        sl *= sl;
        sr *= sr;
        maxl = wasm_f32x4_pmax(maxl, sl);
        maxr = wasm_f32x4_pmax(maxr, sr);
        suml += sl;
        sumr += sr;
    }

    maxl = wasm_f32x4_pmax(maxl, (vec4)wasm_i32x4_shuffle(maxl, maxl, 1, 0, 3, 0));
    maxl = wasm_f32x4_pmax(maxl, (vec4)wasm_i32x4_shuffle(maxl, maxl, 2, 0, 0, 0));
    max_sq_left = maxl[0];

    maxr = wasm_f32x4_pmax(maxr, (vec4)wasm_i32x4_shuffle(maxr, maxr, 1, 0, 3, 0));
    maxr = wasm_f32x4_pmax(maxr, (vec4)wasm_i32x4_shuffle(maxr, maxr, 2, 0, 0, 0));
    max_sq_right = maxr[0];

    suml += (vec4)wasm_i32x4_shuffle(suml, suml, 1, 0, 3, 0);
    suml += (vec4)wasm_i32x4_shuffle(suml, suml, 2, 0, 0, 0);
    sum_sq_left = suml[0];

    sumr += (vec4)wasm_i32x4_shuffle(sumr, sumr, 1, 0, 3, 0);
    sumr += (vec4)wasm_i32x4_shuffle(sumr, sumr, 2, 0, 0, 0);
    sum_sq_right = sumr[0];

    float* fin_l = (float*)in_l;
    float* fin_r = (float*)in_r;
    for (; num>0; --num)
    {
        float left = *(fin_l++) * gain;
        float right = *(fin_r++) * gain;
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
    for(; num>3; num-=4)
    {
        wasm_v128_store(out, wasm_f32x4_convert_i32x4(wasm_i32x4_load16x4(in)));
        in +=4;
        out += 4;
    }
    for(; num>0; --num)
    {
        *(out++) = (float)*(in++);
    }
}

static inline void ConvertFromS8(float* out, const int8_t* in, uint32_t num)
{
    for(; num>7; num-=8)
    {
        v128_t i = wasm_i16x8_load8x8(in);
        in += 8;
        wasm_v128_store(out, wasm_f32x4_convert_i32x4(wasm_i32x4_extend_low_i16x8(i)));
        out += 4;
        wasm_v128_store(out, wasm_f32x4_convert_i32x4(wasm_i32x4_extend_high_i16x8(i)));
        out += 4;
    }
    for(; num>0; --num)
    {
        int16_t s = *(in++);
        *(out++) = (float)((s << 8) | (s & 0xff));
    }
}

static inline void Deinterleave(float* out[], const float* in, uint32_t num)
{
    const vec4* vin = (const vec4*)in;
    vec4* vout_l = (vec4*)out[0];
    vec4* vout_r = (vec4*)out[1];
    for(; num>3; num-=4)
    {
        vec4 in0 = wasm_v128_load(vin++);
        vec4 in1 = wasm_v128_load(vin++);
        wasm_v128_store(vout_l++, wasm_i32x4_shuffle(in0, in1, 0, 2, 4, 6));
        wasm_v128_store(vout_r++, wasm_i32x4_shuffle(in0, in1, 1, 3, 5, 7));
    }

    float* out_l = (float*)vout_l;
    float* out_r = (float*)vout_r;
    in = (const float*)vin;
    for(; num>0; --num)
    {
        *(out_l++) = *(in++);
        *(out_r++) = *(in++);
    }
}

static inline void DeinterleaveFromS16(float* out[], const int16_t* in, uint32_t num)
{
    vec4* vout_l = (vec4*)out[0];
    vec4* vout_r = (vec4*)out[1];
    for(; num>3; num-=4)
    {
        vec4 in0 = wasm_f32x4_convert_i32x4(wasm_i32x4_load16x4(in));
        vec4 in1 = wasm_f32x4_convert_i32x4(wasm_i32x4_load16x4(in + 4));
        in += 8;
        wasm_v128_store(vout_l++, wasm_i32x4_shuffle(in0, in1, 0, 2, 4, 6));
        wasm_v128_store(vout_r++, wasm_i32x4_shuffle(in0, in1, 1, 3, 5, 7));
    }

    float* out_l = (float*)vout_l;
    float* out_r = (float*)vout_r;
    for(; num>0; --num)
    {
        *(out_l++) = (float)*(in++);
        *(out_r++) = (float)*(in++);
    }
}

static inline void DeinterleaveFromS8(float* out[], const int8_t* in, uint32_t num)
{
    vec4* vout_l = (vec4*)out[0];
    vec4* vout_r = (vec4*)out[1];
    for(; num>3; num-=4)
    {
        v128_t i = wasm_i16x8_load8x8(in);
        in += 8;
        vec4 in0 = wasm_f32x4_convert_i32x4(wasm_i32x4_extend_low_i16x8(i));
        vec4 in1 = wasm_f32x4_convert_i32x4(wasm_i32x4_extend_high_i16x8(i));
        wasm_v128_store(vout_l++, wasm_i32x4_shuffle(in0, in1, 0, 2, 4, 6));
        wasm_v128_store(vout_r++, wasm_i32x4_shuffle(in0, in1, 1, 3, 5, 7));
    }

    float* out_l = (float*)vout_l;
    float* out_r = (float*)vout_r;
    for(; num>0; --num)
    {
        int16_t sl = *(in++);
        int16_t sr = *(in++);
        *(out_l++) = (float)((sl << 8) | (sl & 0xff));
        *(out_r++) = (float)((sr << 8) | (sr & 0xff));
    }
}

} // namespace WASM

#endif // __wasm_simd128__

// ----------------------------------------------------------------------------------------------------------------------------------------------------------------
// ----------------------------------------------------------------------------------------------------------------------------------------------------------------

#if defined(DM_SOUND_DSP_SSE2)

//
// SSE2
//
namespace SSE {

    typedef __m128 vec4;


    static inline float FilterSampleFIR8(const float* frames, uint64_t frac_pos)
    {
        uint32_t pi = ((frac_pos >> (RESAMPLE_FRACTION_BITS - 11)) << 3) & (2047 << 3); // 8 tabs, 2048 (11 fractional bits) banks
        const float* c = &_pfb[pi];

//TODO: RTCD needed to make selection (outside inner loop), so we don't demand SSE4.1
#if 0 //defined(__SSE4_1__)
        vec4 taps0 = _mm_loadu_ps(&frames[-3]);
        vec4 taps1 = _mm_loadu_ps(&frames[ 1]);
        return _mm_cvtss_f32(_mm_add_ps(_mm_dp_ps(taps0, *(const vec4*)&c[0], 0xf1), _mm_dp_ps(taps1, *(const vec4*)&c[4], 0xf1)));      // at least on intel: 11 cycles latency, but good throughput -> shuffles and adds are rather FAST, without hiding the latency the old code could be comparable in speed!
#else
        vec4 tmp0 = _mm_mul_ps(_mm_loadu_ps(&frames[-3]), *(const vec4*)&c[0]);
        vec4 tmp1 = _mm_mul_ps(_mm_loadu_ps(&frames[ 1]), *(const vec4*)&c[4]);
        tmp0 = _mm_add_ps(tmp0, _mm_shuffle_ps(tmp0, tmp0, _MM_SHUFFLE(0, 3, 0, 1))); // X=X+Y; Z=Z+W
        tmp1 = _mm_add_ps(tmp1, _mm_shuffle_ps(tmp1, tmp1, _MM_SHUFFLE(0, 3, 0, 1))); // X=X+Y; Z=Z+W
        tmp0 = _mm_add_ps(tmp0, _mm_shuffle_ps(tmp0, tmp0, _MM_SHUFFLE(0, 0, 0, 2))); // X=X+Y+Z+W
        tmp1 = _mm_add_ps(tmp1, _mm_shuffle_ps(tmp1, tmp1, _MM_SHUFFLE(0, 0, 0, 2))); // X=X+Y+Z+W
        return _mm_cvtss_f32(_mm_add_ps(tmp0, tmp1));
#endif
    }


static inline void MixScaledMonoToStereo(float* out[], const float* in, uint32_t num, float scale_l, float scale_r, float scale_delta_l, float scale_delta_r)
{
    // setup ramps
    vec4 scld = _mm_set1_ps(scale_delta_l);
    vec4 scrd = _mm_set1_ps(scale_delta_r);
    vec4 scl  = _mm_add_ps(_mm_set1_ps(scale_l), _mm_mul_ps(scld, _mm_set_ps(3.0f, 2.0f, 1.0f, 0.0f)));
    vec4 scr  = _mm_add_ps(_mm_set1_ps(scale_r), _mm_mul_ps(scrd, _mm_set_ps(3.0f, 2.0f, 1.0f, 0.0f)));
    scld = _mm_mul_ps(scld, _mm_set1_ps(4.0f));
    scrd = _mm_mul_ps(scrd, _mm_set1_ps(4.0f));

    // vectorized mix
    vec4* vl = (vec4*)out[0];
    vec4* vr = (vec4*)out[1];
    vec4* vin = (vec4*)in;
    for(; num>3; num-=4)
    {
        vec4 s = *(vin++);
        *vl = _mm_add_ps(*vl, _mm_mul_ps(s, scl));
        ++vl;
        *vr = _mm_add_ps(*vr, _mm_mul_ps(s, scr));
        ++vr;
        scl = _mm_add_ps(scl, scld);
        scr = _mm_add_ps(scr, scrd);
    }
    // process any remaining samples
    float* fl = (float*)vl;
    float* fr = (float*)vr;
    float* fin = (float*)vin;
    scale_l = _mm_cvtss_f32(scl);
    scale_r = _mm_cvtss_f32(scr);
    for(; num>0; --num)
    {
        float s = *(fin++);
        *(fl++) += s * scale_l;
        *(fr++) += s * scale_r;
        scale_l += scale_delta_l;
        scale_r += scale_delta_r;
    }
}

static inline void MixScaledStereoToStereo(float* out[], const float* in_l, const float* in_r, uint32_t num,
                                           float scale_l0, float scale_r0, float scale_delta_l0, float scale_delta_r0,
                                           float scale_l1, float scale_r1, float scale_delta_l1, float scale_delta_r1)
{
    // setup ramps
    vec4 scld0 = _mm_set1_ps(scale_delta_l0);
    vec4 scrd0 = _mm_set1_ps(scale_delta_r0);
    vec4 scld1 = _mm_set1_ps(scale_delta_l1);
    vec4 scrd1 = _mm_set1_ps(scale_delta_r1);
    vec4 scl0  = _mm_add_ps(_mm_set1_ps(scale_l0), _mm_mul_ps(scld0, _mm_set_ps(3.0f, 2.0f, 1.0f, 0.0f)));
    vec4 scr0  = _mm_add_ps(_mm_set1_ps(scale_r0), _mm_mul_ps(scrd0, _mm_set_ps(3.0f, 2.0f, 1.0f, 0.0f)));
    vec4 scl1  = _mm_add_ps(_mm_set1_ps(scale_l1), _mm_mul_ps(scld1, _mm_set_ps(3.0f, 2.0f, 1.0f, 0.0f)));
    vec4 scr1  = _mm_add_ps(_mm_set1_ps(scale_r1), _mm_mul_ps(scrd1, _mm_set_ps(3.0f, 2.0f, 1.0f, 0.0f)));
    scld0 = _mm_mul_ps(scld0, _mm_set1_ps(4.0f));
    scrd0 = _mm_mul_ps(scrd0, _mm_set1_ps(4.0f));
    scld1 = _mm_mul_ps(scld1, _mm_set1_ps(4.0f));
    scrd1 = _mm_mul_ps(scrd1, _mm_set1_ps(4.0f));

    // vectorized mix
    vec4* vl = (vec4*)out[0];
    vec4* vr = (vec4*)out[1];
    vec4* vin_l = (vec4*)in_l;
    vec4* vin_r = (vec4*)in_r;
    for(; num>3; num-=4)
    {
        vec4 sl = *(vin_l++);
        vec4 sr = *(vin_r++);
        *vl = _mm_add_ps(*vl, _mm_add_ps(_mm_mul_ps(sl, scl0), _mm_mul_ps(sr, scl1)));
        ++vl;
        *vr = _mm_add_ps(*vr, _mm_add_ps(_mm_mul_ps(sl, scr0), _mm_mul_ps(sr, scr1)));
        ++vr;
        scl0 = _mm_add_ps(scl0, scld0);
        scr0 = _mm_add_ps(scr0, scrd0);
        scl1 = _mm_add_ps(scl1, scld1);
        scr1 = _mm_add_ps(scr1, scrd1);
    }
    // process any remaining samples
    float* fl = (float*)vl;
    float* fr = (float*)vr;
    float* fin_l = (float*)vin_l;
    float* fin_r = (float*)vin_r;
    scale_l0 = _mm_cvtss_f32(scl0);
    scale_r0 = _mm_cvtss_f32(scr0);
    scale_l1 = _mm_cvtss_f32(scl1);
    scale_r1 = _mm_cvtss_f32(scr1);
    for(; num>0; --num)
    {
        float sl = *(fin_l++);
        float sr = *(fin_r++);
        *(fl++) += sl * scale_l0 + sr * scale_l1;
        *(fr++) += sl * scale_r0 + sr * scale_r1;
        scale_l0 += scale_delta_l0;
        scale_r0 += scale_delta_r0;
        scale_l1 += scale_delta_l1;
        scale_r1 += scale_delta_r1;
    }
}

static inline uint64_t MixAndResampleMonoToStereo_Polyphase(float* out[], const float* in, uint32_t num, uint64_t frac, uint64_t delta, float scale_l, float scale_r, float scale_delta_l, float scale_delta_r)
{
    DM_PROFILE("MixAndResampleMonoToStereo_Polyphase_SSE2");

    // setup ramps
    vec4 scld = _mm_set1_ps(scale_delta_l);
    vec4 scrd = _mm_set1_ps(scale_delta_r);
    vec4 scl  = _mm_add_ps(_mm_set1_ps(scale_l), _mm_mul_ps(scld, _mm_set_ps(3.0f, 2.0f, 1.0f, 0.0f)));
    vec4 scr  = _mm_add_ps(_mm_set1_ps(scale_r), _mm_mul_ps(scrd, _mm_set_ps(3.0f, 2.0f, 1.0f, 0.0f)));
    scld = _mm_mul_ps(scld, _mm_set1_ps(4.0f));
    scrd = _mm_mul_ps(scrd, _mm_set1_ps(4.0f));

    // vectorized mix
    vec4* vl = (vec4*)out[0];
    vec4* vr = (vec4*)out[1];
    for(; num>3; num-=4)
    {
        float s0 = FilterSampleFIR8(&in[frac >> RESAMPLE_FRACTION_BITS], frac);
        frac += delta;
        float s1 = FilterSampleFIR8(&in[frac >> RESAMPLE_FRACTION_BITS], frac);
        frac += delta;
        float s2 = FilterSampleFIR8(&in[frac >> RESAMPLE_FRACTION_BITS], frac);
        frac += delta;
        float s3 = FilterSampleFIR8(&in[frac >> RESAMPLE_FRACTION_BITS], frac);
        frac += delta;
        vec4 s = _mm_set_ps(s3, s2, s1, s0);

        *vl = _mm_add_ps(*vl, _mm_mul_ps(s, scl));
        ++vl;
        *vr = _mm_add_ps(*vr, _mm_mul_ps(s, scr));
        ++vr;
        scl = _mm_add_ps(scl, scld);
        scr = _mm_add_ps(scr, scrd);
    }
    // process any remaining samples
    float* fl = (float*)vl;
    float* fr = (float*)vr;
    scale_l = _mm_cvtss_f32(scl);
    scale_r = _mm_cvtss_f32(scr);
    for(; num>0; --num)
    {
        float s = FilterSampleFIR8(&in[frac >> RESAMPLE_FRACTION_BITS], frac);

        *(fl++) += s * scale_l;
        *(fr++) += s * scale_r;
        scale_l += scale_delta_l;
        scale_r += scale_delta_r;

        frac += delta;
    }
    return frac;
}

static inline uint64_t MixAndResampleStereoToStereo_Polyphase(float* out[], const float* in_l, const float* in_r, uint32_t num, uint64_t frac, uint64_t delta,
                                                              float scale_l0, float scale_r0, float scale_delta_l0, float scale_delta_r0,
                                                              float scale_l1, float scale_r1, float scale_delta_l1, float scale_delta_r1)
{
    DM_PROFILE("MixAndResampleStereoToStereo_Polyphase_SSE2");
    // setup ramps
    vec4 scld0 = _mm_set1_ps(scale_delta_l0);
    vec4 scrd0 = _mm_set1_ps(scale_delta_r0);
    vec4 scld1 = _mm_set1_ps(scale_delta_l1);
    vec4 scrd1 = _mm_set1_ps(scale_delta_r1);
    vec4 scl0  = _mm_add_ps(_mm_set1_ps(scale_l0), _mm_mul_ps(scld0, _mm_set_ps(3.0f, 2.0f, 1.0f, 0.0f)));
    vec4 scr0  = _mm_add_ps(_mm_set1_ps(scale_r0), _mm_mul_ps(scrd0, _mm_set_ps(3.0f, 2.0f, 1.0f, 0.0f)));
    vec4 scl1  = _mm_add_ps(_mm_set1_ps(scale_l1), _mm_mul_ps(scld1, _mm_set_ps(3.0f, 2.0f, 1.0f, 0.0f)));
    vec4 scr1  = _mm_add_ps(_mm_set1_ps(scale_r1), _mm_mul_ps(scrd1, _mm_set_ps(3.0f, 2.0f, 1.0f, 0.0f)));
    scld0 = _mm_mul_ps(scld0, _mm_set1_ps(4.0f));
    scrd0 = _mm_mul_ps(scrd0, _mm_set1_ps(4.0f));
    scld1 = _mm_mul_ps(scld1, _mm_set1_ps(4.0f));
    scrd1 = _mm_mul_ps(scrd1, _mm_set1_ps(4.0f));

    // vectorized mix
    vec4* vl = (vec4*)out[0];
    vec4* vr = (vec4*)out[1];
    for(; num>3; num-=4)
    {
        float s0l = FilterSampleFIR8(&in_l[frac >> RESAMPLE_FRACTION_BITS], frac);
        float s0r = FilterSampleFIR8(&in_r[frac >> RESAMPLE_FRACTION_BITS], frac);
        frac += delta;
        float s1l = FilterSampleFIR8(&in_l[frac >> RESAMPLE_FRACTION_BITS], frac);
        float s1r = FilterSampleFIR8(&in_r[frac >> RESAMPLE_FRACTION_BITS], frac);
        frac += delta;
        float s2l = FilterSampleFIR8(&in_l[frac >> RESAMPLE_FRACTION_BITS], frac);
        float s2r = FilterSampleFIR8(&in_r[frac >> RESAMPLE_FRACTION_BITS], frac);
        frac += delta;
        float s3l = FilterSampleFIR8(&in_l[frac >> RESAMPLE_FRACTION_BITS], frac);
        float s3r = FilterSampleFIR8(&in_r[frac >> RESAMPLE_FRACTION_BITS], frac);
        frac += delta;
        vec4 sl = _mm_set_ps(s3l, s2l, s1l, s0l);
        vec4 sr = _mm_set_ps(s3r, s2r, s1r, s0r);

        *vl = _mm_add_ps(*vl, _mm_add_ps(_mm_mul_ps(sl, scl0), _mm_mul_ps(sr, scl1)));
        ++vl;
        *vr = _mm_add_ps(*vr, _mm_add_ps(_mm_mul_ps(sl, scr0), _mm_mul_ps(sr, scr1)));
        ++vr;
        scl0 = _mm_add_ps(scl0, scld0);
        scr0 = _mm_add_ps(scr0, scrd0);
        scl1 = _mm_add_ps(scl1, scld1);
        scr1 = _mm_add_ps(scr1, scrd1);
    }
    // process any remaining samples
    float* fl = (float*)vl;
    float* fr = (float*)vr;
    scale_l0 = _mm_cvtss_f32(scl0);
    scale_r0 = _mm_cvtss_f32(scr0);
    scale_l1 = _mm_cvtss_f32(scl1);
    scale_r1 = _mm_cvtss_f32(scr1);
    for(; num>0; --num)
    {
        float sl = FilterSampleFIR8(&in_l[frac >> RESAMPLE_FRACTION_BITS], frac);
        float sr = FilterSampleFIR8(&in_r[frac >> RESAMPLE_FRACTION_BITS], frac);

        *(fl++) += sl * scale_l0 + sr * scale_l1;
        *(fr++) += sl * scale_r0 + sr * scale_r1;
        scale_l0 += scale_delta_l0;
        scale_r0 += scale_delta_r0;
        scale_l1 += scale_delta_l1;
        scale_r1 += scale_delta_r1;

        frac += delta;
    }
    return frac;
}

static inline void ApplyClampedGain(float* out[], float* in[], uint32_t num, float scale, float scale_delta)
{
    // setup ramp
    vec4 sc = _mm_set1_ps(scale);
    vec4 scd = _mm_set1_ps(scale_delta);
    sc = _mm_add_ps(_mm_set1_ps(scale), _mm_mul_ps(scd, _mm_set_ps(3.0f, 2.0f, 1.0f, 0.0f)));
    scd = _mm_mul_ps(scd, _mm_set1_ps(4.0f));

    vec4 zero = _mm_set1_ps(0.0f);
    vec4 one = _mm_set1_ps(1.0f);

    vec4* vin_l = (vec4*)in[0];
    vec4* vin_r = (vec4*)in[1];
    vec4* vout_l = (vec4*)out[0];
    vec4* vout_r = (vec4*)out[1];
    for(; num>3; num-=4)
    {
        vec4 sc_clamped = _mm_min_ps(_mm_max_ps(sc, zero), one);

        *vout_l = _mm_add_ps(*vout_l, _mm_mul_ps(*(vin_l++), sc_clamped));
        ++vout_l;
        *vout_r = _mm_add_ps(*vout_r, _mm_mul_ps(*(vin_r++), sc_clamped));
        ++vout_r;
        sc = _mm_add_ps(sc, scd);
    }

    float* in_l = (float*)vin_l;
    float* in_r = (float*)vin_r;
    float* out_l = (float*)vout_l;
    float* out_r = (float*)vout_r;
    scale = _mm_cvtss_f32(sc);
    for(; num>0; --num)
    {
        float scale_clamped = dmMath::Clamp(scale, 0.0f, 1.0f);
        *(out_l++) += *(in_l++) * scale_clamped;
        *(out_r++) += *(in_r++) * scale_clamped;
        scale += scale_delta;
    }
}

static inline void ApplyGainAndInterleaveToS16(int16_t* out, float* in[], uint32_t num, float scale, float scale_delta)
{
    // setup ramp
    vec4 sc = _mm_set1_ps(scale);
    vec4 scd = _mm_set1_ps(scale_delta);
    sc = _mm_add_ps(_mm_set1_ps(scale), _mm_mul_ps(scd, _mm_set_ps(3.0f, 2.0f, 1.0f, 0.0f)));
    scd = _mm_mul_ps(scd, _mm_set1_ps(4.0f));

    vec4* vin_l = (vec4*)in[0];
    vec4* vin_r = (vec4*)in[1];
    __m128i* vout = (__m128i*)out;
    for(; num>7; num-=8)
    {
        vec4 l0 = _mm_mul_ps(*(vin_l++), sc);                                       // Apply gain
        vec4 r0 = _mm_mul_ps(*(vin_r++), sc);
        sc = _mm_add_ps(sc, scd);
        vec4 r1 = _mm_mul_ps(*(vin_r++), sc);
        vec4 l1 = _mm_mul_ps(*(vin_l++), sc);
        sc = _mm_add_ps(sc, scd);
        __m128i li = _mm_packs_epi32(_mm_cvtps_epi32(l0), _mm_cvtps_epi32(l1));     // L0-7 to s16
        __m128i ri = _mm_packs_epi32(_mm_cvtps_epi32(r0), _mm_cvtps_epi32(r1));     // R0-7 to s16
        *(vout++) = _mm_unpacklo_epi16(li, ri);                                     // LR0-LR3 interleaved out
        *(vout++) = _mm_unpackhi_epi16(li, ri);                                     // LR4-LR7 interleaved out
    }

    float* in_l = (float*)vin_l;
    float* in_r = (float*)vin_r;
    scale = _mm_cvtss_f32(sc);
    out = (int16_t*)vout;
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
    // setup ramp
    vec4 sc = _mm_set1_ps(scale);
    vec4 scd = _mm_set1_ps(scale_delta);
    sc = _mm_add_ps(_mm_set1_ps(scale), _mm_mul_ps(scd, _mm_set_ps(3.0f, 2.0f, 1.0f, 0.0f)));
    scd = _mm_mul_ps(scd, _mm_set1_ps(4.0f));

    vec4* vin_l = (vec4*)in[0];
    vec4* vin_r = (vec4*)in[1];
    vec4* vout_l = (vec4*)out[0];
    vec4* vout_r = (vec4*)out[1];
    for(; num>3; num-=4)
    {
        *vout_l = _mm_mul_ps(*(vin_l++), sc);
        ++vout_l;
        *vout_r = _mm_mul_ps(*(vin_r++), sc);
        ++vout_r;
        sc = _mm_add_ps(sc, scd);
    }

    float* in_l = (float*)vin_l;
    float* in_r = (float*)vin_r;
    float* out_l = (float*)vout_l;
    float* out_r = (float*)vout_r;
    scale = _mm_cvtss_f32(sc);
    for(; num>0; --num)
    {
        *(out_l++) = *(in_l++) * scale;
        *(out_r++) = *(in_r++) * scale;
        scale += scale_delta;
    }
}

static inline void GatherPowerData(float* in[], uint32_t num, float gain, float& sum_sq_left, float& sum_sq_right, float& max_sq_left, float& max_sq_right)
{
    vec4* in_l = (vec4*)in[0];
    vec4* in_r = (vec4*)in[1];

    vec4 maxl = _mm_set1_ps(0.0f);
    vec4 suml = maxl;
    vec4 maxr = maxl;
    vec4 sumr = maxl;

    vec4 scale = _mm_set1_ps(gain);

    for(; num>3; num-=4)
    {
        vec4 sl = _mm_mul_ps(*(in_l++), scale);
        vec4 sr = _mm_mul_ps(*(in_r++), scale);
        sl = _mm_mul_ps(sl, sl);
        sr = _mm_mul_ps(sr, sr);
        maxl = _mm_max_ps(maxl, sl);
        maxr = _mm_max_ps(maxr, sr);
        suml = _mm_add_ps(suml, sl);
        sumr = _mm_add_ps(sumr, sr);
    }

    maxl = _mm_max_ps(maxl, _mm_shuffle_ps(maxl, maxl, _MM_SHUFFLE(0,3,0,1)));
    maxl = _mm_max_ps(maxl, _mm_shuffle_ps(maxl, maxl, _MM_SHUFFLE(0,0,0,2)));
    max_sq_left = _mm_cvtss_f32(maxl);

    maxr = _mm_max_ps(maxr, _mm_shuffle_ps(maxr, maxr, _MM_SHUFFLE(0,3,0,1)));
    maxr = _mm_max_ps(maxr, _mm_shuffle_ps(maxr, maxr, _MM_SHUFFLE(0,0,0,2)));
    max_sq_right = _mm_cvtss_f32(maxr);

    suml = _mm_add_ps(suml, _mm_shuffle_ps(suml, suml, _MM_SHUFFLE(0,3,0,1)));
    suml = _mm_add_ps(suml, _mm_shuffle_ps(suml, suml, _MM_SHUFFLE(0,0,0,2)));
    sum_sq_left = _mm_cvtss_f32(suml);

    sumr = _mm_add_ps(sumr, _mm_shuffle_ps(sumr, sumr, _MM_SHUFFLE(0,3,0,1)));
    sumr = _mm_add_ps(sumr, _mm_shuffle_ps(sumr, sumr, _MM_SHUFFLE(0,0,0,2)));
    sum_sq_right = _mm_cvtss_f32(sumr);

    float* fin_l = (float*)in_l;
    float* fin_r = (float*)in_r;
    for (; num>0; --num)
    {
        float left = *(fin_l++) * gain;
        float right = *(fin_r++) * gain;
        float left_sq = left * left;
        float right_sq = right * right;
        sum_sq_left += left_sq;
        sum_sq_right += right_sq;
        max_sq_left = dmMath::Max(max_sq_left, left_sq);
        max_sq_right = dmMath::Max(max_sq_right, right_sq);
    }
}

// note: supports unaligned src & dest
static inline void ConvertFromS16(float* out, const int16_t* in, uint32_t num)
{
    const __m128i* vin = (const __m128i*)in;
    vec4* vout = (vec4*)out;
    for(; num>7; num-=8)
    {
        __m128i iin = _mm_loadu_si128(vin++);
        _mm_storeu_ps((float*)vout++, _mm_cvtepi32_ps(_mm_srai_epi32(_mm_unpacklo_epi16(iin, iin), 16)));
        _mm_storeu_ps((float*)vout++, _mm_cvtepi32_ps(_mm_srai_epi32(_mm_unpackhi_epi16(iin, iin), 16)));
    }

    out = (float*)vout;
    in = (const int16_t*)vin;
    for(; num>0; --num)
    {
        *(out++) = (float)*(in++);
    }
}

// note: supports unaligned src & dest
static inline void ConvertFromS8(float* out, const int8_t* in, uint32_t num)
{
    const __m128i* vin = (const __m128i*)in;
    vec4* vout = (vec4*)out;
    for(; num>15; num-=16)
    {
        __m128i iin = _mm_loadu_si128(vin++);
        __m128i iin16a = _mm_unpacklo_epi8(iin, iin);   // s0-7  as int16 (we duplicate the high bits into the low bits to approximate overall value better)
        __m128i iin16b = _mm_unpackhi_epi8(iin, iin);   // s8-15 as int16
        _mm_storeu_ps((float*)vout++, _mm_cvtepi32_ps(_mm_srai_epi32(_mm_unpacklo_epi16(iin16a, iin16a), 16)));
        _mm_storeu_ps((float*)vout++, _mm_cvtepi32_ps(_mm_srai_epi32(_mm_unpackhi_epi16(iin16a, iin16a), 16)));
        _mm_storeu_ps((float*)vout++, _mm_cvtepi32_ps(_mm_srai_epi32(_mm_unpacklo_epi16(iin16b, iin16b), 16)));
        _mm_storeu_ps((float*)vout++, _mm_cvtepi32_ps(_mm_srai_epi32(_mm_unpackhi_epi16(iin16b, iin16b), 16)));
    }

    out = (float*)vout;
    in = (const int8_t*)vin;
    for(; num>0; --num)
    {
        int16_t s = *(in++);
        *(out++) = (float)((s << 8) | (s & 0xff));
    }
}

// note: supports unaligned src & dest
static inline void Deinterleave(float* out[], const float* in, uint32_t num)
{
    const vec4* vin = (const vec4*)in;
    vec4* vout_l = (vec4*)out[0];
    vec4* vout_r = (vec4*)out[1];
    for(; num>3; num-=4)
    {
        vec4 in0 = _mm_loadu_ps((float*)vin++);
        vec4 in1 = _mm_loadu_ps((float*)vin++);
        in0 = _mm_shuffle_ps(in0, in0, _MM_SHUFFLE(3,1,2,0));     // L0L1R0R1
        in1 = _mm_shuffle_ps(in1, in1, _MM_SHUFFLE(3,1,2,0));     // L2L3R2R3
        _mm_storeu_ps((float*)vout_l++, _mm_shuffle_ps(in0, in1, _MM_SHUFFLE(1,0,1,0)));
        _mm_storeu_ps((float*)vout_r++, _mm_shuffle_ps(in0, in1, _MM_SHUFFLE(3,2,3,2)));
    }

    float* out_l = (float*)vout_l;
    float* out_r = (float*)vout_r;
    in = (const float*)vin;
    for(; num>0; --num)
    {
        *(out_l++) = *(in++);
        *(out_r++) = *(in++);
    }
}

// note: supports unaligned src & dest
static inline void DeinterleaveFromS16(float* out[], const int16_t* in, uint32_t num)
{
    const __m128i* vin = (const __m128i*)in;
    vec4* vout_l = (vec4*)out[0];
    vec4* vout_r = (vec4*)out[1];
    for(; num>3; num-=4)
    {
        __m128i iin = _mm_loadu_si128(vin++);
        vec4 in0 = _mm_cvtepi32_ps(_mm_srai_epi32(_mm_unpacklo_epi16(iin, iin), 16));
        vec4 in1 = _mm_cvtepi32_ps(_mm_srai_epi32(_mm_unpackhi_epi16(iin, iin), 16));

        in0 = _mm_shuffle_ps(in0, in0, _MM_SHUFFLE(3,1,2,0));     // L0L1R0R1
        in1 = _mm_shuffle_ps(in1, in1, _MM_SHUFFLE(3,1,2,0));     // L2L3R2R3
        _mm_storeu_ps((float*)vout_l++, _mm_shuffle_ps(in0, in1, _MM_SHUFFLE(1,0,1,0)));
        _mm_storeu_ps((float*)vout_r++, _mm_shuffle_ps(in0, in1, _MM_SHUFFLE(3,2,3,2)));
    }

    float* out_l = (float*)vout_l;
    float* out_r = (float*)vout_r;
    in = (const int16_t*)vin;
    for(; num>0; --num)
    {
        *(out_l++) = (float)*(in++);
        *(out_r++) = (float)*(in++);
    }
}

// note: supports unaligned src & dest
static inline void DeinterleaveFromS8(float* out[], const int8_t* in, uint32_t num)
{
    const __m128i* vin = (const __m128i*)in;
    vec4* vout_l = (vec4*)out[0];
    vec4* vout_r = (vec4*)out[1];
    for(; num>7; num-=8)
    {
        __m128i iin = _mm_loadu_si128(vin++);
        __m128i iin16a = _mm_unpacklo_epi8(iin, iin);   // s0-7  as int16 (we duplicate the high bits into the low bits to approximate overall value better)
        __m128i iin16b = _mm_unpackhi_epi8(iin, iin);   // s8-15 as int16
        vec4 in0 = _mm_cvtepi32_ps(_mm_srai_epi32(_mm_unpacklo_epi16(iin16a, iin16a), 16));
        vec4 in1 = _mm_cvtepi32_ps(_mm_srai_epi32(_mm_unpackhi_epi16(iin16a, iin16a), 16));
        vec4 in2 = _mm_cvtepi32_ps(_mm_srai_epi32(_mm_unpacklo_epi16(iin16b, iin16b), 16));
        vec4 in3 = _mm_cvtepi32_ps(_mm_srai_epi32(_mm_unpackhi_epi16(iin16b, iin16b), 16));

        in0 = _mm_shuffle_ps(in0, in0, _MM_SHUFFLE(3,1,2,0));     // L0L1R0R1
        in1 = _mm_shuffle_ps(in1, in1, _MM_SHUFFLE(3,1,2,0));     // L2L3R2R3
        in2 = _mm_shuffle_ps(in2, in2, _MM_SHUFFLE(3,1,2,0));     // L4L5R4R5
        in3 = _mm_shuffle_ps(in3, in3, _MM_SHUFFLE(3,1,2,0));     // L6L7R6R7
        _mm_storeu_ps((float*)vout_l++, _mm_shuffle_ps(in0, in1, _MM_SHUFFLE(1,0,1,0)));
        _mm_storeu_ps((float*)vout_l++, _mm_shuffle_ps(in2, in3, _MM_SHUFFLE(1,0,1,0)));
        _mm_storeu_ps((float*)vout_r++, _mm_shuffle_ps(in0, in1, _MM_SHUFFLE(3,2,3,2)));
        _mm_storeu_ps((float*)vout_r++, _mm_shuffle_ps(in2, in3, _MM_SHUFFLE(3,2,3,2)));
    }

    float* out_l = (float*)vout_l;
    float* out_r = (float*)vout_r;
    in = (const int8_t*)vin;
    for(; num>0; --num)
    {
        int16_t sl = *(in++);
        int16_t sr = *(in++);
        *(out_l++) = (float)((sl << 8) | (sl & 0xff));
        *(out_r++) = (float)((sr << 8) | (sr & 0xff));
    }
}

} // namespace SSE

#endif

// ----------------------------------------------------------------------------------------------------------------------------------------------------------------
// ----------------------------------------------------------------------------------------------------------------------------------------------------------------

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
        *(out++) = (float)((s << 8) | (s & 0xff));
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
        *(out_l++) = (float)((sl << 8) | (sl & 0xff));
        *(out_r++) = (float)((sr << 8) | (sr & 0xff));
    }
}

} // namespace Fallback

// ----------------------------------------------------------------------------------------------------------------------------------------------------------------
// ----------------------------------------------------------------------------------------------------------------------------------------------------------------

#ifdef DM_SOUND_DSP_IMPL

//
// Compiletime selected DSP implementation
//

#define SoundImpl DM_SOUND_DSP_IMPL

static inline void SelectDSPImpl(DSPImplType /*impl_type*/)
{
    #define DM_TO_STRING_MACRO(x) _DM_TO_STRING_MACRO(x)
    #define _DM_TO_STRING_MACRO(x) #x

    // nothing to do in this implementation
    dmLogOnceInfo("  DSP backend: " DM_TO_STRING_MACRO(DM_SOUND_DSP_IMPL) );

    #undef DM_TO_STRING_MACRO
    #undef _DM_TO_STRING_MACRO
}

static inline void MixScaledMonoToStereo(float* out[], const float* in, uint32_t num, float scale_l, float scale_r, float scale_delta_l, float scale_delta_r)
{
    SoundImpl::MixScaledMonoToStereo(out, in, num, scale_l, scale_r, scale_delta_l, scale_delta_r);
}

static inline void MixScaledStereoToStereo(float* out[], const float* in_l, const float* in_r, uint32_t num,
                                           float scale_l0, float scale_r0, float scale_delta_l0, float scale_delta_r0,
                                           float scale_l1, float scale_r1, float scale_delta_l1, float scale_delta_r1)
{
    SoundImpl::MixScaledStereoToStereo(out, in_l, in_r, num, scale_l0, scale_r0, scale_delta_l0, scale_delta_r0,
                                                       scale_l1, scale_r1, scale_delta_l1, scale_delta_r1);
}

static inline uint64_t MixAndResampleMonoToStereo_Polyphase(float* out[], const float* in, uint32_t num, uint64_t frac, uint64_t delta, float scale_l, float scale_r, float scale_delta_l, float scale_delta_r)
{
    return SoundImpl::MixAndResampleMonoToStereo_Polyphase(out, in, num, frac, delta, scale_l, scale_r, scale_delta_l, scale_delta_r);
}

static inline uint64_t MixAndResampleStereoToStereo_Polyphase(float* out[], const float* in_l, const float* in_r, uint32_t num, uint64_t frac, uint64_t delta,
                                                             float scale_l0, float scale_r0, float scale_delta_l0, float scale_delta_r0,
                                                             float scale_l1, float scale_r1, float scale_delta_l1, float scale_delta_r1)
{
    return SoundImpl::MixAndResampleStereoToStereo_Polyphase(out, in_l, in_r, num, frac, delta, scale_l0, scale_r0, scale_delta_l0, scale_delta_r0,
                                                                                          scale_l1, scale_r1, scale_delta_l1, scale_delta_r1);
}

static inline void ApplyClampedGain(float* out[], float* in[], uint32_t num, float scale, float scale_delta)
{
    SoundImpl::ApplyClampedGain(out, in, num, scale, scale_delta);
}

static inline void ApplyGainAndInterleaveToS16(int16_t* out, float* in[], uint32_t num, float scale, float scale_delta)
{
    SoundImpl::ApplyGainAndInterleaveToS16(out, in, num, scale, scale_delta);
}

static inline void ApplyGain(float* out[], float* in[], uint32_t num, float scale, float scale_delta)
{
    SoundImpl::ApplyGain(out, in, num, scale, scale_delta);
}

static inline void GatherPowerData(float* in[], uint32_t num, float gain, float& sum_sq_left, float& sum_sq_right, float& max_sq_left, float& max_sq_right)
{
    SoundImpl::GatherPowerData(in, num, gain, sum_sq_left, sum_sq_right, max_sq_left, max_sq_right);
}

static inline void ConvertFromS16(float* out, const int16_t* in, uint32_t num)
{
    SoundImpl::ConvertFromS16(out, in, num);
}

static inline void ConvertFromS8(float* out, const int8_t* in, uint32_t num)
{
    SoundImpl::ConvertFromS8(out, in, num);
}

static inline void Deinterleave(float* out[], const float* in, uint32_t num)
{
    SoundImpl::Deinterleave(out, in, num);
}

static inline void DeinterleaveFromS16(float* out[], const int16_t* in, uint32_t num)
{
    SoundImpl::DeinterleaveFromS16(out, in, num);
}

static inline void DeinterleaveFromS8(float* out[], const int8_t* in, uint32_t num)
{
    SoundImpl::DeinterleaveFromS8(out, in, num);
}

#undef SoundImpl

#else

//
// Runtime selected DSP implementation
//

struct DSPImpl {
     void (*MixScaledMonoToStereo)(float* out[], const float* in, uint32_t num, float scale_l, float scale_r, float scale_delta_l, float scale_delta_r);
     void (*MixScaledStereoToStereo)(float* out[], const float* in_l, const float* in_r, uint32_t num, float scale_l0, float scale_r0, float scale_delta_l0, float scale_delta_r0, float scale_l1, float scale_r1, float scale_delta_l1, float scale_delta_r1);
     uint64_t (*MixAndResampleMonoToStereo_Polyphase)(float* out[], const float* in, uint32_t num, uint64_t frac, uint64_t delta, float scale_l, float scale_r, float scale_delta_l, float scale_delta_r);
     uint64_t (*MixAndResampleStereoToStereo_Polyphase)(float* out[], const float* in_l, const float* in_r, uint32_t num, uint64_t frac, uint64_t delta, float scale_l0, float scale_r0, float scale_delta_l0, float scale_delta_r0, float scale_l1, float scale_r1, float scale_delta_l1, float scale_delta_r1);
     void (*ApplyClampedGain)(float* out[], float* in[], uint32_t num, float scale, float scale_delta);
     void (*ApplyGainAndInterleaveToS16)(int16_t* out, float* in[], uint32_t num, float scale, float scale_delta);
     void (*ApplyGain)(float* out[], float* in[], uint32_t num, float scale, float scale_delta);
     void (*GatherPowerData)(float* in[], uint32_t num, float gain, float& sum_sq_left, float& sum_sq_right, float& max_sq_left, float& max_sq_right);
     void (*ConvertFromS16)(float* out, const int16_t* in, uint32_t num);
     void (*ConvertFromS8)(float* out, const int8_t* in, uint32_t num);
     void (*Deinterleave)(float* out[], const float* in, uint32_t num);
     void (*DeinterleaveFromS16)(float* out[], const int16_t* in, uint32_t num);
     void (*DeinterleaveFromS8)(float* out[], const int8_t* in, uint32_t num);
};

#if defined(DM_SIMD_WASM)
static DSPImpl wasm_impl =
{
    WASM::MixScaledMonoToStereo,
    WASM::MixScaledStereoToStereo,
    WASM::MixAndResampleMonoToStereo_Polyphase,
    WASM::MixAndResampleStereoToStereo_Polyphase,
    WASM::ApplyClampedGain,
    WASM::ApplyGainAndInterleaveToS16,
    WASM::ApplyGain,
    WASM::GatherPowerData,
    WASM::ConvertFromS16,
    WASM::ConvertFromS8,
    WASM::Deinterleave,
    WASM::DeinterleaveFromS16,
    WASM::DeinterleaveFromS8
};
#endif

#if defined(DM_SIMD_SSE2)
static DSPImpl sse_impl =
{
    SSE::MixScaledMonoToStereo,
    SSE::MixScaledStereoToStereo,
    SSE::MixAndResampleMonoToStereo_Polyphase,
    SSE::MixAndResampleStereoToStereo_Polyphase,
    SSE::ApplyClampedGain,
    SSE::ApplyGainAndInterleaveToS16,
    SSE::ApplyGain,
    SSE::GatherPowerData,
    SSE::ConvertFromS16,
    SSE::ConvertFromS8,
    SSE::Deinterleave,
    SSE::DeinterleaveFromS16,
    SSE::DeinterleaveFromS8
};
#endif

static DSPImpl fallback_impl =
{
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

static DSPImpl* g_DSPImpl = &fallback_impl;

static inline bool SelectDSPImpl(DSPImplType impl_type)
{
    switch(impl_type)
    {
        case DSPIMPL_TYPE_CPU:
            dmLogOnceInfo("  DSP backend: CPU");
            g_DSPImpl = &fallback_impl;
            return true;
#if defined(DM_SOUND_DSP_SSE2)
        case DSPIMPL_TYPE_SSE2:
            dmLogOnceInfo("  DSP backend: SSE2");
            g_DSPImpl = &sse_impl;
            return true;
#endif
#if defined(DM_SOUND_DSP_WASM)
        case DSPIMPL_TYPE_WASM_SIMD128:
            dmLogOnceInfo("  DSP backend: WASM");
            g_DSPImpl = &wasm_impl;
            return true;
#endif
        default: break;
    }

    g_DSPImpl = &fallback_impl;
    return false;
}

static inline void MixScaledMonoToStereo(float* out[], const float* in, uint32_t num, float scale_l, float scale_r, float scale_delta_l, float scale_delta_r)
{
    g_DSPImpl->MixScaledMonoToStereo(out, in, num, scale_l, scale_r, scale_delta_l, scale_delta_r);
}

static inline void MixScaledStereoToStereo(float* out[], const float* in_l, const float* in_r, uint32_t num,
                                           float scale_l0, float scale_r0, float scale_delta_l0, float scale_delta_r0,
                                           float scale_l1, float scale_r1, float scale_delta_l1, float scale_delta_r1)
{
    g_DSPImpl->MixScaledStereoToStereo(out, in_l, in_r, num, scale_l0, scale_r0, scale_delta_l0, scale_delta_r0,
                                                             scale_l1, scale_r1, scale_delta_l1, scale_delta_r1);
}

static inline uint64_t MixAndResampleMonoToStereo_Polyphase(float* out[], const float* in, uint32_t num, uint64_t frac, uint64_t delta, float scale_l, float scale_r, float scale_delta_l, float scale_delta_r)
{
    return g_DSPImpl->MixAndResampleMonoToStereo_Polyphase(out, in, num, frac, delta, scale_l, scale_r, scale_delta_l, scale_delta_r);
}

static inline uint64_t MixAndResampleStereoToStereo_Polyphase(float* out[], const float* in_l, const float* in_r, uint32_t num, uint64_t frac, uint64_t delta,
                                                             float scale_l0, float scale_r0, float scale_delta_l0, float scale_delta_r0,
                                                             float scale_l1, float scale_r1, float scale_delta_l1, float scale_delta_r1)
{
    return g_DSPImpl->MixAndResampleStereoToStereo_Polyphase(out, in_l, in_r, num, frac, delta, scale_l0, scale_r0, scale_delta_l0, scale_delta_r0,
                                                                                                scale_l1, scale_r1, scale_delta_l1, scale_delta_r1);
}

static inline void ApplyClampedGain(float* out[], float* in[], uint32_t num, float scale, float scale_delta)
{
    g_DSPImpl->ApplyClampedGain(out, in, num, scale, scale_delta);
}

static inline void ApplyGainAndInterleaveToS16(int16_t* out, float* in[], uint32_t num, float scale, float scale_delta)
{
    g_DSPImpl->ApplyGainAndInterleaveToS16(out, in, num, scale, scale_delta);
}

static inline void ApplyGain(float* out[], float* in[], uint32_t num, float scale, float scale_delta)
{
    g_DSPImpl->ApplyGain(out, in, num, scale, scale_delta);
}

static inline void GatherPowerData(float* in[], uint32_t num, float gain, float& sum_sq_left, float& sum_sq_right, float& max_sq_left, float& max_sq_right)
{
    g_DSPImpl->GatherPowerData(in, num, gain, sum_sq_left, sum_sq_right, max_sq_left, max_sq_right);
}

static inline void ConvertFromS16(float* out, const int16_t* in, uint32_t num)
{
    g_DSPImpl->ConvertFromS16(out, in, num);
}

static inline void ConvertFromS8(float* out, const int8_t* in, uint32_t num)
{
    g_DSPImpl->ConvertFromS8(out, in, num);
}

static inline void Deinterleave(float* out[], const float* in, uint32_t num)
{
    g_DSPImpl->Deinterleave(out, in, num);
}

static inline void DeinterleaveFromS16(float* out[], const int16_t* in, uint32_t num)
{
    g_DSPImpl->DeinterleaveFromS16(out, in, num);
}

static inline void DeinterleaveFromS8(float* out[], const int8_t* in, uint32_t num)
{
    g_DSPImpl->DeinterleaveFromS8(out, in, num);
}

#endif


} // namespace dmSound


#endif // DM_SOUND_DSP_H
