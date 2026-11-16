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

#include "sound_dsp_simd.h"
#include DM_SOUND_DSP_RESAMPLE_HEADER

// 4-lane operations compiled for each supported ISA and selected at sound startup.
namespace dmSound
{
    namespace DM_SOUND_DSP_NAMESPACE
    {
        // Narrow x86 kernels retain the faster independent FIR schedule.
        DM_SOUND_DSP_INLINE void ResampleStereoIndependent_Simd4(const float* in_l, const float* in_r, uint64_t frac, uint64_t delta, dsimd_float4_t* left, dsimd_float4_t* right) DM_SOUND_DSP_STREAMING
        {
            *left = Resample_Simd4(in_l, frac, delta);
            *right = Resample_Simd4(in_r, frac, delta);
        }

        DM_SOUND_DSP_LOCALLY_STREAMING static void MixScaledMonoToStereo_Simd4(float* out[], const float* in, uint32_t num, float scale_l, float scale_r, float scale_delta_l, float scale_delta_r)
        {
            float* out_l = out[0];
            float* out_r = out[1];
            dsimd_float4_t sc_l = Simd4Ramp(scale_l, scale_delta_l);
            dsimd_float4_t step_l = Simd4Splat(scale_delta_l * 4);
            dsimd_float4_t sc_r = Simd4Ramp(scale_r, scale_delta_r);
            dsimd_float4_t step_r = Simd4Splat(scale_delta_r * 4);
            const unsigned vector_num = num & ~(4 - 1);
            unsigned i = 0;
            for (; i < vector_num; i += 4)
            {
                dsimd_float4_t sample = Simd4Load(in + i);
                Simd4Store(out_l + i, Simd4Add(Simd4Load(out_l + i), Simd4Mul(sample, sc_l)));
                Simd4Store(out_r + i, Simd4Add(Simd4Load(out_r + i), Simd4Mul(sample, sc_r)));
                sc_l = Simd4Add(sc_l, step_l);
                sc_r = Simd4Add(sc_r, step_r);
            }

            if (i == num)
            {
                Simd4Finish();
                return;
            }

            scale_l = Simd4Lane(sc_l, 0);
            scale_r = Simd4Lane(sc_r, 0);
            for (; i < num; ++i)
            {
                float sample = in[i];
                out_l[i] += sample * scale_l;
                out_r[i] += sample * scale_r;
                scale_l += scale_delta_l;
                scale_r += scale_delta_r;
            }

            Simd4Finish();
        }

        DM_SOUND_DSP_LOCALLY_STREAMING static uint64_t MixAndResampleMonoToStereo_Polyphase_Simd4(float* out[], const float* in, uint32_t num, uint64_t frac, uint64_t delta, float scale_l, float scale_r, float scale_delta_l, float scale_delta_r)
        {
            DM_PROFILE("MixAndResampleMonoToStereo_Polyphase_SIMD");

            dsimd_float4_t sc_l = Simd4Ramp(scale_l, scale_delta_l);
            dsimd_float4_t step_l = Simd4Splat(scale_delta_l * 4);
            dsimd_float4_t sc_r = Simd4Ramp(scale_r, scale_delta_r);
            dsimd_float4_t step_r = Simd4Splat(scale_delta_r * 4);
            const unsigned vector_num = num & ~(4 - 1);
            unsigned i = 0;
            for (; i < vector_num; i += 4)
            {
                dsimd_float4_t sample = Resample_Simd4(in, frac, delta);
                frac += delta * 4;
                Simd4Store(out[0] + i, Simd4Add(Simd4Load(out[0] + i), Simd4Mul(sample, sc_l)));
                Simd4Store(out[1] + i, Simd4Add(Simd4Load(out[1] + i), Simd4Mul(sample, sc_r)));
                sc_l = Simd4Add(sc_l, step_l);
                sc_r = Simd4Add(sc_r, step_r);
            }

            if (i == num)
            {
                Simd4Finish();
                return frac;
            }

            scale_l = Simd4Lane(sc_l, 0);
            scale_r = Simd4Lane(sc_r, 0);
            for (; i < num; ++i)
            {
                float sample = FilterSampleFIR8_Simd8(in + (frac >> RESAMPLE_FRACTION_BITS), frac);
                out[0][i] += sample * scale_l;
                out[1][i] += sample * scale_r;
                scale_l += scale_delta_l;
                scale_r += scale_delta_r;
                frac += delta;
            }

            Simd4Finish();
            return frac;
        }

        DM_SOUND_DSP_INLINE void MixConstantStereoToStereo_Simd4(float* out[], const float* in_l, const float* in_r, uint32_t num, float scale_l0, float scale_r0, float scale_l1, float scale_r1) DM_SOUND_DSP_STREAMING
        {
            float* out_l = out[0];
            float* out_r = out[1];
            dsimd_float4_t sc_l0 = Simd4Splat(scale_l0);
            dsimd_float4_t sc_r0 = Simd4Splat(scale_r0);
            dsimd_float4_t sc_l1 = Simd4Splat(scale_l1);
            dsimd_float4_t sc_r1 = Simd4Splat(scale_r1);
            const unsigned vector_num = num & ~(4 - 1);
            unsigned i = 0;
            for (; i < vector_num; i += 4)
            {
                dsimd_float4_t s_l = Simd4Load(in_l + i);
                dsimd_float4_t s_r = Simd4Load(in_r + i);
                Simd4Store(out_l + i, Simd4Add(Simd4Load(out_l + i), Simd4Add(Simd4Mul(s_l, sc_l0), Simd4Mul(s_r, sc_l1))));
                Simd4Store(out_r + i, Simd4Add(Simd4Load(out_r + i), Simd4Add(Simd4Mul(s_l, sc_r0), Simd4Mul(s_r, sc_r1))));
            }

            for (; i < num; ++i)
            {
                float s_l = in_l[i];
                float s_r = in_r[i];
                out_l[i] += s_l * scale_l0 + s_r * scale_l1;
                out_r[i] += s_l * scale_r0 + s_r * scale_r1;
            }

            Simd4Finish();
        }

        DM_SOUND_DSP_LOCALLY_STREAMING static void MixScaledStereoToStereo_Simd4(float* out[], const float* in_l, const float* in_r, uint32_t num, float scale_l0, float scale_r0, float scale_delta_l0, float scale_delta_r0, float scale_l1, float scale_r1, float scale_delta_l1, float scale_delta_r1)
        {
            // Steady-volume playback does not need per-vector ramp updates.
            if (scale_delta_l0 == 0.0f && scale_delta_r0 == 0.0f && scale_delta_l1 == 0.0f && scale_delta_r1 == 0.0f)
            {
                MixConstantStereoToStereo_Simd4(out, in_l, in_r, num, scale_l0, scale_r0, scale_l1, scale_r1);
                Simd4Finish();
                return;
            }

            float* out_l = out[0];
            float* out_r = out[1];
            dsimd_float4_t sc_l0 = Simd4Ramp(scale_l0, scale_delta_l0);
            dsimd_float4_t step_l0 = Simd4Splat(scale_delta_l0 * 4);
            dsimd_float4_t sc_r0 = Simd4Ramp(scale_r0, scale_delta_r0);
            dsimd_float4_t step_r0 = Simd4Splat(scale_delta_r0 * 4);
            dsimd_float4_t sc_l1 = Simd4Ramp(scale_l1, scale_delta_l1);
            dsimd_float4_t step_l1 = Simd4Splat(scale_delta_l1 * 4);
            dsimd_float4_t sc_r1 = Simd4Ramp(scale_r1, scale_delta_r1);
            dsimd_float4_t step_r1 = Simd4Splat(scale_delta_r1 * 4);
            const unsigned vector_num = num & ~(4 - 1);
            unsigned i = 0;
            for (; i < vector_num; i += 4)
            {
                dsimd_float4_t s_l = Simd4Load(in_l + i);
                dsimd_float4_t s_r = Simd4Load(in_r + i);
                Simd4Store(out_l + i, Simd4Add(Simd4Load(out_l + i), Simd4Add(Simd4Mul(s_l, sc_l0), Simd4Mul(s_r, sc_l1))));
                Simd4Store(out_r + i, Simd4Add(Simd4Load(out_r + i), Simd4Add(Simd4Mul(s_l, sc_r0), Simd4Mul(s_r, sc_r1))));
                sc_l0 = Simd4Add(sc_l0, step_l0);
                sc_r0 = Simd4Add(sc_r0, step_r0);
                sc_l1 = Simd4Add(sc_l1, step_l1);
                sc_r1 = Simd4Add(sc_r1, step_r1);
            }

            if (i == num)
            {
                Simd4Finish();
                return;
            }

            scale_l0 = Simd4Lane(sc_l0, 0);
            scale_r0 = Simd4Lane(sc_r0, 0);
            scale_l1 = Simd4Lane(sc_l1, 0);
            scale_r1 = Simd4Lane(sc_r1, 0);
            for (; i < num; ++i)
            {
                float s_l = in_l[i];
                float s_r = in_r[i];
                out_l[i] += s_l * scale_l0 + s_r * scale_l1;
                out_r[i] += s_l * scale_r0 + s_r * scale_r1;
                scale_l0 += scale_delta_l0;
                scale_r0 += scale_delta_r0;
                scale_l1 += scale_delta_l1;
                scale_r1 += scale_delta_r1;
            }

            Simd4Finish();
        }

        DM_SOUND_DSP_INLINE uint64_t MixAndResampleStereoDiagonal_Simd4(float* out[], const float* in_l, const float* in_r, uint32_t num, uint64_t frac, uint64_t delta, float scale_l, float scale_r, float scale_delta_l, float scale_delta_r) DM_SOUND_DSP_STREAMING
        {
            dsimd_float4_t sc_l = Simd4Ramp(scale_l, scale_delta_l);
            dsimd_float4_t sc_r = Simd4Ramp(scale_r, scale_delta_r);
            dsimd_float4_t step_l = Simd4Splat(scale_delta_l * 4);
            dsimd_float4_t step_r = Simd4Splat(scale_delta_r * 4);
            const unsigned vector_num = num & ~(4 - 1);
            unsigned i = 0;
            for (; i < vector_num; i += 4)
            {
                dsimd_float4_t s_l, s_r;
                ResampleStereo_Simd4(in_l, in_r, frac, delta, &s_l, &s_r);
                frac += delta * 4;
                Simd4Store(out[0] + i, Simd4Add(Simd4Load(out[0] + i), Simd4Mul(s_l, sc_l)));
                Simd4Store(out[1] + i, Simd4Add(Simd4Load(out[1] + i), Simd4Mul(s_r, sc_r)));
                sc_l = Simd4Add(sc_l, step_l);
                sc_r = Simd4Add(sc_r, step_r);
            }

            if (i == num)
            {
                Simd4Finish();
                return frac;
            }

            scale_l = Simd4Lane(sc_l, 0);
            scale_r = Simd4Lane(sc_r, 0);
            for (; i < num; ++i)
            {
                float s_l, s_r;
                FilterStereoFIR8_Simd8(in_l, in_r, frac, &s_l, &s_r);
                out[0][i] += s_l * scale_l;
                out[1][i] += s_r * scale_r;
                scale_l += scale_delta_l;
                scale_r += scale_delta_r;
                frac += delta;
            }

            Simd4Finish();
            return frac;
        }

        DM_SOUND_DSP_LOCALLY_STREAMING static uint64_t MixAndResampleStereoToStereo_Polyphase_Simd4(float* out[], const float* in_l, const float* in_r, uint32_t num, uint64_t frac, uint64_t delta, float scale_l0, float scale_r0, float scale_delta_l0, float scale_delta_r0, float scale_l1, float scale_r1, float scale_delta_l1, float scale_delta_r1)
        {
            DM_PROFILE("MixAndResampleStereoToStereo_Polyphase_SIMD");

            if (scale_r0 == 0.0f && scale_delta_r0 == 0.0f && scale_l1 == 0.0f && scale_delta_l1 == 0.0f)
            {
                uint64_t result = MixAndResampleStereoDiagonal_Simd4(out, in_l, in_r, num, frac, delta, scale_l0, scale_r1, scale_delta_l0, scale_delta_r1);
                Simd4Finish();
                return result;
            }

            dsimd_float4_t sc_l0 = Simd4Ramp(scale_l0, scale_delta_l0);
            dsimd_float4_t step_l0 = Simd4Splat(scale_delta_l0 * 4);
            dsimd_float4_t sc_r0 = Simd4Ramp(scale_r0, scale_delta_r0);
            dsimd_float4_t step_r0 = Simd4Splat(scale_delta_r0 * 4);
            dsimd_float4_t sc_l1 = Simd4Ramp(scale_l1, scale_delta_l1);
            dsimd_float4_t step_l1 = Simd4Splat(scale_delta_l1 * 4);
            dsimd_float4_t sc_r1 = Simd4Ramp(scale_r1, scale_delta_r1);
            dsimd_float4_t step_r1 = Simd4Splat(scale_delta_r1 * 4);
            const unsigned vector_num = num & ~(4 - 1);
            unsigned i = 0;
            for (; i < vector_num; i += 4)
            {
                dsimd_float4_t s_l, s_r;
                ResampleStereo_Simd4(in_l, in_r, frac, delta, &s_l, &s_r);
                frac += delta * 4;
                Simd4Store(out[0] + i, Simd4Add(Simd4Load(out[0] + i), Simd4Add(Simd4Mul(s_l, sc_l0), Simd4Mul(s_r, sc_l1))));
                Simd4Store(out[1] + i, Simd4Add(Simd4Load(out[1] + i), Simd4Add(Simd4Mul(s_l, sc_r0), Simd4Mul(s_r, sc_r1))));
                sc_l0 = Simd4Add(sc_l0, step_l0);
                sc_r0 = Simd4Add(sc_r0, step_r0);
                sc_l1 = Simd4Add(sc_l1, step_l1);
                sc_r1 = Simd4Add(sc_r1, step_r1);
            }

            if (i == num)
            {
                Simd4Finish();
                return frac;
            }

            scale_l0 = Simd4Lane(sc_l0, 0);
            scale_r0 = Simd4Lane(sc_r0, 0);
            scale_l1 = Simd4Lane(sc_l1, 0);
            scale_r1 = Simd4Lane(sc_r1, 0);
            for (; i < num; ++i)
            {
                float s_l = FilterSampleFIR8_Simd8(in_l + (frac >> RESAMPLE_FRACTION_BITS), frac);
                float s_r = FilterSampleFIR8_Simd8(in_r + (frac >> RESAMPLE_FRACTION_BITS), frac);
                out[0][i] += s_l * scale_l0 + s_r * scale_l1;
                out[1][i] += s_l * scale_r0 + s_r * scale_r1;
                scale_l0 += scale_delta_l0;
                scale_r0 += scale_delta_r0;
                scale_l1 += scale_delta_l1;
                scale_r1 += scale_delta_r1;
                frac += delta;
            }

            Simd4Finish();
            return frac;
        }

        DM_SOUND_DSP_LOCALLY_STREAMING static void ApplyClampedGain_Simd4(float* out[], float* in[], uint32_t num, float scale, float scale_delta)
        {
            float* in_l = in[0];
            float* in_r = in[1];
            float* out_l = out[0];
            float* out_r = out[1];
            dsimd_float4_t sc = Simd4Ramp(scale, scale_delta);
            dsimd_float4_t step = Simd4Splat(scale_delta * 4);
            unsigned i = 0;
            for (; num - i >= 4; i += 4)
            {
                dsimd_float4_t gain = Simd4Min(Simd4Max(sc, Simd4Splat(0)), Simd4Splat(1));
                dsimd_float4_t s0 = Simd4Mul(Simd4Load(in_l + i), gain);
                dsimd_float4_t s1 = Simd4Mul(Simd4Load(in_r + i), gain);
                Simd4Store(out_l + i, Simd4Add(Simd4Load(out_l + i), s0));
                Simd4Store(out_r + i, Simd4Add(Simd4Load(out_r + i), s1));
                sc = Simd4Add(sc, step);
            }

            if (i == num)
            {
                Simd4Finish();
                return;
            }

            scale = Simd4Lane(sc, 0);
            float* remaining[] = { in_l + i, in_r + i };
            float* destination[] = { out_l + i, out_r + i };
            Fallback::ApplyClampedGain(destination, remaining, num - i, scale, scale_delta);

            Simd4Finish();
        }

        DM_SOUND_DSP_LOCALLY_STREAMING static void ApplyGain_Simd4(float* out[], float* in[], uint32_t num, float scale, float scale_delta)
        {
            float* in_l = in[0];
            float* in_r = in[1];
            float* out_l = out[0];
            float* out_r = out[1];
            dsimd_float4_t sc = Simd4Ramp(scale, scale_delta);
            dsimd_float4_t step = Simd4Splat(scale_delta * 4);
            const unsigned vector_num = num & ~(4 - 1);
            unsigned i = 0;
            for (; i < vector_num; i += 4)
            {
                dsimd_float4_t gain = sc;
                dsimd_float4_t s0 = Simd4Mul(Simd4Load(in_l + i), gain);
                dsimd_float4_t s1 = Simd4Mul(Simd4Load(in_r + i), gain);
                Simd4Store(out_l + i, s0);
                Simd4Store(out_r + i, s1);
                sc = Simd4Add(sc, step);
            }

            if (i == num)
            {
                Simd4Finish();
                return;
            }

            scale = Simd4Lane(sc, 0);
            float* remaining[] = { in_l + i, in_r + i };
            float* destination[] = { out_l + i, out_r + i };
            Fallback::ApplyGain(destination, remaining, num - i, scale, scale_delta);

            Simd4Finish();
        }

        DM_SOUND_DSP_LOCALLY_STREAMING static void ApplyGainAndInterleaveToS16_Simd4(int16_t* out, float* in[], uint32_t num, float scale, float scale_delta)
        {
            float* in_l = in[0];
            float* in_r = in[1];
            dsimd_float4_t sc = Simd4Ramp(scale, scale_delta);
            dsimd_float4_t step = Simd4Splat(scale_delta * 4);
            const unsigned vector_num = num & ~(4 - 1);
            unsigned i = 0;
#if defined(DM_SOUND_DSP_BATCH_PCM)
            const unsigned pcm_num = num & ~7u;
            for (; i < pcm_num; i += 8)
            {
                dsimd_float4_t next_sc = Simd4Add(sc, step);
                dsimd_float8_t gain = Simd8Combine(sc, next_sc);
                Simd8StoreInterleavedS16(out + 2 * i, Simd8Mul(Simd8Load(in_l + i), gain), Simd8Mul(Simd8Load(in_r + i), gain));
                sc = Simd4Add(next_sc, step);
            }

#endif
            for (; i < vector_num; i += 4)
            {
                dsimd_float4_t gain = sc;
                dsimd_float4_t s0 = Simd4Mul(Simd4Load(in_l + i), gain);
                dsimd_float4_t s1 = Simd4Mul(Simd4Load(in_r + i), gain);
                Simd4StoreInterleavedS16(out + 2 * i, s0, s1);
                sc = Simd4Add(sc, step);
            }

            if (i == num)
            {
                Simd4Finish();
                return;
            }

            scale = Simd4Lane(sc, 0);
            float* remaining[] = { in_l + i, in_r + i };
            Fallback::ApplyGainAndInterleaveToS16(out + 2 * i, remaining, num - i, scale, scale_delta);

            Simd4Finish();
        }

        DM_SOUND_DSP_LOCALLY_STREAMING static void GatherPowerData_Simd4(float* in[], uint32_t num, float gain, float& sum_sq_left, float& sum_sq_right, float& max_sq_left, float& max_sq_right)
        {
            float* in_l = in[0];
            float* in_r = in[1];
            dsimd_float4_t sum_l = Simd4Splat(0), sum_r = sum_l;
            dsimd_float4_t max_l = sum_l, max_r = sum_l;
            dsimd_float4_t scale = Simd4Splat(gain);
            const unsigned vector_num = num & ~(4 - 1);
            unsigned i = 0;
            for (; i < vector_num; i += 4)
            {
                dsimd_float4_t l = Simd4Mul(Simd4Load(in_l + i), scale);
                dsimd_float4_t r = Simd4Mul(Simd4Load(in_r + i), scale);
                l = Simd4Mul(l, l);
                r = Simd4Mul(r, r);
                sum_l = Simd4Add(sum_l, l);
                sum_r = Simd4Add(sum_r, r);
                max_l = Simd4Max(max_l, l);
                max_r = Simd4Max(max_r, r);
            }

            sum_sq_left = Simd4Sum(sum_l);
            sum_sq_right = Simd4Sum(sum_r);
            max_sq_left = Simd4ReduceMax(max_l);
            max_sq_right = Simd4ReduceMax(max_r);
            for (; i < num; ++i)
            {
                float l = in_l[i] * gain, r = in_r[i] * gain;
                l *= l;
                r *= r;
                sum_sq_left += l;
                sum_sq_right += r;
                max_sq_left = dmMath::Max(max_sq_left, l);
                max_sq_right = dmMath::Max(max_sq_right, r);
            }

            Simd4Finish();
        }

        DM_SOUND_DSP_LOCALLY_STREAMING static void ConvertFromS16_Simd4(float* out, const int16_t* in, uint32_t num)
        {
            unsigned i = 0;
#if defined(DM_SOUND_DSP_BATCH_PCM)
            const unsigned pcm_num = num & ~7u;
            for (; i < pcm_num; i += 8)
            {
                Simd8Store(out + i, Simd8LoadS16ToFloat(in + i));
            }

#endif
            for (; num - i >= 4; i += 4)
            {
                Simd4Store(out + i, Simd4LoadS16ToFloat(in + i));
            }

            Fallback::ConvertFromS16(out + i, in + i, num - i);

            Simd4Finish();
        }

        DM_SOUND_DSP_LOCALLY_STREAMING static void ConvertFromS8_Simd4(float* out, const int8_t* in, uint32_t num)
        {
            const unsigned vector_num = num & ~(4 - 1);
            unsigned i = 0;
#if defined(DM_SOUND_DSP_BATCH_PCM)
            const unsigned pcm_num = num & ~15u;
            for (; i < pcm_num; i += 16)
            {
                Simd16Store(out + i, Simd16LoadS8ToS16Float(in + i));
            }

#endif
            for (; i < vector_num; i += 4)
            {
                Simd4Store(out + i, Simd4LoadS8ToS16Float(in + i));
            }

            Fallback::ConvertFromS8(out + i, in + i, num - i);

            Simd4Finish();
        }

        DM_SOUND_DSP_LOCALLY_STREAMING static void Deinterleave_Simd4(float* out[], const float* in, uint32_t num)
        {
            float* out_l = out[0];
            float* out_r = out[1];
            const unsigned vector_num = num & ~(4 - 1);
            unsigned i = 0;
            for (; i < vector_num; i += 4)
            {
                dsimd_float4_t a = Simd4Load(in + 2 * i);
                dsimd_float4_t b = Simd4Load(in + 2 * i + 4);
                dsimd_float4_t left, right;
                Simd4Unzip(a, b, &left, &right);
                Simd4Store(out_l + i, left);
                Simd4Store(out_r + i, right);
            }

            float* destination[] = { out_l + i, out_r + i };
            Fallback::Deinterleave(destination, in + 2 * i, num - i);

            Simd4Finish();
        }

        DM_SOUND_DSP_LOCALLY_STREAMING static void DeinterleaveFromS16_Simd4(float* out[], const int16_t* in, uint32_t num)
        {
            float* out_l = out[0];
            float* out_r = out[1];
            const unsigned vector_num = num & ~(4 - 1);
            unsigned i = 0;
            for (; i < vector_num; i += 4)
            {
                dsimd_float4_t a = Simd4LoadS16ToFloat(in + 2 * i);
                dsimd_float4_t b = Simd4LoadS16ToFloat(in + 2 * i + 4);
                dsimd_float4_t left, right;
                Simd4Unzip(a, b, &left, &right);
                Simd4Store(out_l + i, left);
                Simd4Store(out_r + i, right);
            }

            float* destination[] = { out_l + i, out_r + i };
            Fallback::DeinterleaveFromS16(destination, in + 2 * i, num - i);

            Simd4Finish();
        }

        DM_SOUND_DSP_LOCALLY_STREAMING static void DeinterleaveFromS8_Simd4(float* out[], const int8_t* in, uint32_t num)
        {
            float* out_l = out[0];
            float* out_r = out[1];
            const unsigned vector_num = num & ~(4 - 1);
            unsigned i = 0;
            for (; i < vector_num; i += 4)
            {
                dsimd_float4_t a = Simd4LoadS8ToS16Float(in + 2 * i);
                dsimd_float4_t b = Simd4LoadS8ToS16Float(in + 2 * i + 4);
                dsimd_float4_t left, right;
                Simd4Unzip(a, b, &left, &right);
                Simd4Store(out_l + i, left);
                Simd4Store(out_r + i, right);
            }

            float* destination[] = { out_l + i, out_r + i };
            Fallback::DeinterleaveFromS8(destination, in + 2 * i, num - i);

            Simd4Finish();
        }

    } // namespace DM_SOUND_DSP_NAMESPACE

    const DSPImpl* DM_SOUND_DSP_GETTER()
    {
        static const DSPImpl impl =
        {
            DM_SOUND_DSP_NAME,
            4,
            DM_SOUND_DSP_NAMESPACE::MixScaledMonoToStereo_Simd4,
            DM_SOUND_DSP_NAMESPACE::MixScaledStereoToStereo_Simd4,
            DM_SOUND_DSP_NAMESPACE::MixAndResampleMonoToStereo_Polyphase_Simd4,
            DM_SOUND_DSP_NAMESPACE::MixAndResampleStereoToStereo_Polyphase_Simd4,
            DM_SOUND_DSP_NAMESPACE::ApplyClampedGain_Simd4,
            DM_SOUND_DSP_NAMESPACE::ApplyGainAndInterleaveToS16_Simd4,
            DM_SOUND_DSP_NAMESPACE::ApplyGain_Simd4,
            DM_SOUND_DSP_NAMESPACE::GatherPowerData_Simd4,
            DM_SOUND_DSP_NAMESPACE::ConvertFromS16_Simd4,
            DM_SOUND_DSP_NAMESPACE::ConvertFromS8_Simd4,
            DM_SOUND_DSP_NAMESPACE::Deinterleave_Simd4,
            DM_SOUND_DSP_NAMESPACE::DeinterleaveFromS16_Simd4,
            DM_SOUND_DSP_NAMESPACE::DeinterleaveFromS8_Simd4,
        };
        return &impl;
    }

} // namespace dmSound
