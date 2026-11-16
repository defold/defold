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

// 8-lane operations compiled for each supported ISA and selected at sound startup.
namespace dmSound
{
    namespace DM_SOUND_DSP_NAMESPACE
    {
        // Narrow x86 kernels retain the faster independent FIR schedule.
        DM_SOUND_DSP_INLINE void ResampleStereoIndependent_Simd8(const float* in_l, const float* in_r, uint64_t frac, uint64_t delta, dsimd_float8_t* left, dsimd_float8_t* right) DM_SOUND_DSP_STREAMING
        {
            *left = Resample_Simd8(in_l, frac, delta);
            *right = Resample_Simd8(in_r, frac, delta);
        }

        DM_SOUND_DSP_LOCALLY_STREAMING static void MixScaledMonoToStereo_Simd8(float* out[], const float* in, uint32_t num, float scale_l, float scale_r, float scale_delta_l, float scale_delta_r)
        {
            float* out_l = out[0];
            float* out_r = out[1];
            dsimd_float8_t sc_l = Simd8Ramp(scale_l, scale_delta_l);
            dsimd_float8_t step_l = Simd8Splat(scale_delta_l * 8);
            dsimd_float8_t sc_r = Simd8Ramp(scale_r, scale_delta_r);
            dsimd_float8_t step_r = Simd8Splat(scale_delta_r * 8);
            const unsigned vector_num = num & ~(8 - 1);
            unsigned i = 0;
            for (; i < vector_num; i += 8)
            {
                dsimd_float8_t sample = Simd8Load(in + i);
                Simd8Store(out_l + i, Simd8Add(Simd8Load(out_l + i), Simd8Mul(sample, sc_l)));
                Simd8Store(out_r + i, Simd8Add(Simd8Load(out_r + i), Simd8Mul(sample, sc_r)));
                sc_l = Simd8Add(sc_l, step_l);
                sc_r = Simd8Add(sc_r, step_r);
            }

            if (i == num)
            {
                Simd8Finish();
                return;
            }

            scale_l = Simd8Lane(sc_l, 0);
            scale_r = Simd8Lane(sc_r, 0);
            for (; i < num; ++i)
            {
                float sample = in[i];
                out_l[i] += sample * scale_l;
                out_r[i] += sample * scale_r;
                scale_l += scale_delta_l;
                scale_r += scale_delta_r;
            }

            Simd8Finish();
        }

        DM_SOUND_DSP_LOCALLY_STREAMING static uint64_t MixAndResampleMonoToStereo_Polyphase_Simd8(float* out[], const float* in, uint32_t num, uint64_t frac, uint64_t delta, float scale_l, float scale_r, float scale_delta_l, float scale_delta_r)
        {
            DM_PROFILE("MixAndResampleMonoToStereo_Polyphase_SIMD");

            dsimd_float8_t sc_l = Simd8Ramp(scale_l, scale_delta_l);
            dsimd_float8_t step_l = Simd8Splat(scale_delta_l * 8);
            dsimd_float8_t sc_r = Simd8Ramp(scale_r, scale_delta_r);
            dsimd_float8_t step_r = Simd8Splat(scale_delta_r * 8);
            const unsigned vector_num = num & ~(8 - 1);
            unsigned i = 0;
            for (; i < vector_num; i += 8)
            {
                dsimd_float8_t sample = Resample_Simd8(in, frac, delta);
                frac += delta * 8;
                Simd8Store(out[0] + i, Simd8Add(Simd8Load(out[0] + i), Simd8Mul(sample, sc_l)));
                Simd8Store(out[1] + i, Simd8Add(Simd8Load(out[1] + i), Simd8Mul(sample, sc_r)));
                sc_l = Simd8Add(sc_l, step_l);
                sc_r = Simd8Add(sc_r, step_r);
            }

            if (i == num)
            {
                Simd8Finish();
                return frac;
            }

            scale_l = Simd8Lane(sc_l, 0);
            scale_r = Simd8Lane(sc_r, 0);
            for (; i < num; ++i)
            {
                float sample = FilterSampleFIR8_Simd8(in + (frac >> RESAMPLE_FRACTION_BITS), frac);
                out[0][i] += sample * scale_l;
                out[1][i] += sample * scale_r;
                scale_l += scale_delta_l;
                scale_r += scale_delta_r;
                frac += delta;
            }

            Simd8Finish();
            return frac;
        }

        DM_SOUND_DSP_INLINE void MixConstantStereoToStereo_Simd8(float* out[], const float* in_l, const float* in_r, uint32_t num, float scale_l0, float scale_r0, float scale_l1, float scale_r1) DM_SOUND_DSP_STREAMING
        {
            float* out_l = out[0];
            float* out_r = out[1];
            dsimd_float8_t sc_l0 = Simd8Splat(scale_l0);
            dsimd_float8_t sc_r0 = Simd8Splat(scale_r0);
            dsimd_float8_t sc_l1 = Simd8Splat(scale_l1);
            dsimd_float8_t sc_r1 = Simd8Splat(scale_r1);
            const unsigned vector_num = num & ~(8 - 1);
            unsigned i = 0;
            for (; i < vector_num; i += 8)
            {
                dsimd_float8_t s_l = Simd8Load(in_l + i);
                dsimd_float8_t s_r = Simd8Load(in_r + i);
                Simd8Store(out_l + i, Simd8Add(Simd8Load(out_l + i), Simd8Add(Simd8Mul(s_l, sc_l0), Simd8Mul(s_r, sc_l1))));
                Simd8Store(out_r + i, Simd8Add(Simd8Load(out_r + i), Simd8Add(Simd8Mul(s_l, sc_r0), Simd8Mul(s_r, sc_r1))));
            }

            for (; i < num; ++i)
            {
                float s_l = in_l[i];
                float s_r = in_r[i];
                out_l[i] += s_l * scale_l0 + s_r * scale_l1;
                out_r[i] += s_l * scale_r0 + s_r * scale_r1;
            }

            Simd8Finish();
        }

        DM_SOUND_DSP_LOCALLY_STREAMING static void MixScaledStereoToStereo_Simd8(float* out[], const float* in_l, const float* in_r, uint32_t num, float scale_l0, float scale_r0, float scale_delta_l0, float scale_delta_r0, float scale_l1, float scale_r1, float scale_delta_l1, float scale_delta_r1)
        {
            // Steady-volume playback does not need per-vector ramp updates.
            if (scale_delta_l0 == 0.0f && scale_delta_r0 == 0.0f && scale_delta_l1 == 0.0f && scale_delta_r1 == 0.0f)
            {
                MixConstantStereoToStereo_Simd8(out, in_l, in_r, num, scale_l0, scale_r0, scale_l1, scale_r1);
                Simd8Finish();
                return;
            }

            float* out_l = out[0];
            float* out_r = out[1];
            dsimd_float8_t sc_l0 = Simd8Ramp(scale_l0, scale_delta_l0);
            dsimd_float8_t step_l0 = Simd8Splat(scale_delta_l0 * 8);
            dsimd_float8_t sc_r0 = Simd8Ramp(scale_r0, scale_delta_r0);
            dsimd_float8_t step_r0 = Simd8Splat(scale_delta_r0 * 8);
            dsimd_float8_t sc_l1 = Simd8Ramp(scale_l1, scale_delta_l1);
            dsimd_float8_t step_l1 = Simd8Splat(scale_delta_l1 * 8);
            dsimd_float8_t sc_r1 = Simd8Ramp(scale_r1, scale_delta_r1);
            dsimd_float8_t step_r1 = Simd8Splat(scale_delta_r1 * 8);
#if !defined(DM_SOUND_DSP_COUNT_REMAINING)
            const unsigned vector_num = num & ~(8 - 1);
#endif
            unsigned i = 0;
#if defined(DM_SOUND_DSP_COUNT_REMAINING)
            for (; num - i >= 8; i += 8)
#else
            for (; i < vector_num; i += 8)
#endif
            {
                dsimd_float8_t s_l = Simd8Load(in_l + i);
                dsimd_float8_t s_r = Simd8Load(in_r + i);
                Simd8Store(out_l + i, Simd8Add(Simd8Load(out_l + i), Simd8Add(Simd8Mul(s_l, sc_l0), Simd8Mul(s_r, sc_l1))));
                Simd8Store(out_r + i, Simd8Add(Simd8Load(out_r + i), Simd8Add(Simd8Mul(s_l, sc_r0), Simd8Mul(s_r, sc_r1))));
                sc_l0 = Simd8Add(sc_l0, step_l0);
                sc_r0 = Simd8Add(sc_r0, step_r0);
                sc_l1 = Simd8Add(sc_l1, step_l1);
                sc_r1 = Simd8Add(sc_r1, step_r1);
            }

            if (i == num)
            {
                Simd8Finish();
                return;
            }

            scale_l0 = Simd8Lane(sc_l0, 0);
            scale_r0 = Simd8Lane(sc_r0, 0);
            scale_l1 = Simd8Lane(sc_l1, 0);
            scale_r1 = Simd8Lane(sc_r1, 0);
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

            Simd8Finish();
        }

        DM_SOUND_DSP_INLINE uint64_t MixAndResampleStereoDiagonal_Simd8(float* out[], const float* in_l, const float* in_r, uint32_t num, uint64_t frac, uint64_t delta, float scale_l, float scale_r, float scale_delta_l, float scale_delta_r) DM_SOUND_DSP_STREAMING
        {
            dsimd_float8_t sc_l = Simd8Ramp(scale_l, scale_delta_l);
            dsimd_float8_t sc_r = Simd8Ramp(scale_r, scale_delta_r);
            dsimd_float8_t step_l = Simd8Splat(scale_delta_l * 8);
            dsimd_float8_t step_r = Simd8Splat(scale_delta_r * 8);
            const unsigned vector_num = num & ~(8 - 1);
            unsigned i = 0;
            for (; i < vector_num; i += 8)
            {
                dsimd_float8_t s_l, s_r;
                ResampleStereo_Simd8(in_l, in_r, frac, delta, &s_l, &s_r);
                frac += delta * 8;
                Simd8Store(out[0] + i, Simd8Add(Simd8Load(out[0] + i), Simd8Mul(s_l, sc_l)));
                Simd8Store(out[1] + i, Simd8Add(Simd8Load(out[1] + i), Simd8Mul(s_r, sc_r)));
                sc_l = Simd8Add(sc_l, step_l);
                sc_r = Simd8Add(sc_r, step_r);
            }

            if (i == num)
            {
                Simd8Finish();
                return frac;
            }

            scale_l = Simd8Lane(sc_l, 0);
            scale_r = Simd8Lane(sc_r, 0);
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

            Simd8Finish();
            return frac;
        }

        DM_SOUND_DSP_LOCALLY_STREAMING static uint64_t MixAndResampleStereoToStereo_Polyphase_Simd8(float* out[], const float* in_l, const float* in_r, uint32_t num, uint64_t frac, uint64_t delta, float scale_l0, float scale_r0, float scale_delta_l0, float scale_delta_r0, float scale_l1, float scale_r1, float scale_delta_l1, float scale_delta_r1)
        {
            DM_PROFILE("MixAndResampleStereoToStereo_Polyphase_SIMD");

            if (scale_r0 == 0.0f && scale_delta_r0 == 0.0f && scale_l1 == 0.0f && scale_delta_l1 == 0.0f)
            {
                uint64_t result = MixAndResampleStereoDiagonal_Simd8(out, in_l, in_r, num, frac, delta, scale_l0, scale_r1, scale_delta_l0, scale_delta_r1);
                Simd8Finish();
                return result;
            }

            dsimd_float8_t sc_l0 = Simd8Ramp(scale_l0, scale_delta_l0);
            dsimd_float8_t step_l0 = Simd8Splat(scale_delta_l0 * 8);
            dsimd_float8_t sc_r0 = Simd8Ramp(scale_r0, scale_delta_r0);
            dsimd_float8_t step_r0 = Simd8Splat(scale_delta_r0 * 8);
            dsimd_float8_t sc_l1 = Simd8Ramp(scale_l1, scale_delta_l1);
            dsimd_float8_t step_l1 = Simd8Splat(scale_delta_l1 * 8);
            dsimd_float8_t sc_r1 = Simd8Ramp(scale_r1, scale_delta_r1);
            dsimd_float8_t step_r1 = Simd8Splat(scale_delta_r1 * 8);
            const unsigned vector_num = num & ~(8 - 1);
            unsigned i = 0;
            for (; i < vector_num; i += 8)
            {
                dsimd_float8_t s_l, s_r;
                ResampleStereo_Simd8(in_l, in_r, frac, delta, &s_l, &s_r);
                frac += delta * 8;
                Simd8Store(out[0] + i, Simd8Add(Simd8Load(out[0] + i), Simd8Add(Simd8Mul(s_l, sc_l0), Simd8Mul(s_r, sc_l1))));
                Simd8Store(out[1] + i, Simd8Add(Simd8Load(out[1] + i), Simd8Add(Simd8Mul(s_l, sc_r0), Simd8Mul(s_r, sc_r1))));
                sc_l0 = Simd8Add(sc_l0, step_l0);
                sc_r0 = Simd8Add(sc_r0, step_r0);
                sc_l1 = Simd8Add(sc_l1, step_l1);
                sc_r1 = Simd8Add(sc_r1, step_r1);
            }

            if (i == num)
            {
                Simd8Finish();
                return frac;
            }

            scale_l0 = Simd8Lane(sc_l0, 0);
            scale_r0 = Simd8Lane(sc_r0, 0);
            scale_l1 = Simd8Lane(sc_l1, 0);
            scale_r1 = Simd8Lane(sc_r1, 0);
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

            Simd8Finish();
            return frac;
        }

        DM_SOUND_DSP_LOCALLY_STREAMING static void ApplyClampedGain_Simd8(float* out[], float* in[], uint32_t num, float scale, float scale_delta)
        {
            float* in_l = in[0];
            float* in_r = in[1];
            float* out_l = out[0];
            float* out_r = out[1];
            dsimd_float8_t sc = Simd8Ramp(scale, scale_delta);
            dsimd_float8_t step = Simd8Splat(scale_delta * 8);
            unsigned i = 0;
            for (; num - i >= 8; i += 8)
            {
                dsimd_float8_t gain = Simd8Min(Simd8Max(sc, Simd8Splat(0)), Simd8Splat(1));
                dsimd_float8_t s0 = Simd8Mul(Simd8Load(in_l + i), gain);
                dsimd_float8_t s1 = Simd8Mul(Simd8Load(in_r + i), gain);
                Simd8Store(out_l + i, Simd8Add(Simd8Load(out_l + i), s0));
                Simd8Store(out_r + i, Simd8Add(Simd8Load(out_r + i), s1));
                sc = Simd8Add(sc, step);
            }

            if (i == num)
            {
                Simd8Finish();
                return;
            }

            scale = Simd8Lane(sc, 0);
            float* remaining[] = { in_l + i, in_r + i };
            float* destination[] = { out_l + i, out_r + i };
            Fallback::ApplyClampedGain(destination, remaining, num - i, scale, scale_delta);

            Simd8Finish();
        }

        DM_SOUND_DSP_LOCALLY_STREAMING static void ApplyGain_Simd8(float* out[], float* in[], uint32_t num, float scale, float scale_delta)
        {
            float* in_l = in[0];
            float* in_r = in[1];
            float* out_l = out[0];
            float* out_r = out[1];
            dsimd_float8_t sc = Simd8Ramp(scale, scale_delta);
            dsimd_float8_t step = Simd8Splat(scale_delta * 8);
            const unsigned vector_num = num & ~(8 - 1);
            unsigned i = 0;
            for (; i < vector_num; i += 8)
            {
                dsimd_float8_t gain = sc;
                dsimd_float8_t s0 = Simd8Mul(Simd8Load(in_l + i), gain);
                dsimd_float8_t s1 = Simd8Mul(Simd8Load(in_r + i), gain);
                Simd8Store(out_l + i, s0);
                Simd8Store(out_r + i, s1);
                sc = Simd8Add(sc, step);
            }

            if (i == num)
            {
                Simd8Finish();
                return;
            }

            scale = Simd8Lane(sc, 0);
            float* remaining[] = { in_l + i, in_r + i };
            float* destination[] = { out_l + i, out_r + i };
            Fallback::ApplyGain(destination, remaining, num - i, scale, scale_delta);

            Simd8Finish();
        }

        DM_SOUND_DSP_LOCALLY_STREAMING static void ApplyGainAndInterleaveToS16_Simd8(int16_t* out, float* in[], uint32_t num, float scale, float scale_delta)
        {
            float* in_l = in[0];
            float* in_r = in[1];
            dsimd_float8_t sc = Simd8Ramp(scale, scale_delta);
            dsimd_float8_t step = Simd8Splat(scale_delta * 8);
            const unsigned vector_num = num & ~(8 - 1);
            unsigned i = 0;
            for (; i < vector_num; i += 8)
            {
                dsimd_float8_t gain = sc;
                dsimd_float8_t s0 = Simd8Mul(Simd8Load(in_l + i), gain);
                dsimd_float8_t s1 = Simd8Mul(Simd8Load(in_r + i), gain);
                Simd8StoreInterleavedS16(out + 2 * i, s0, s1);
                sc = Simd8Add(sc, step);
            }

            if (i == num)
            {
                Simd8Finish();
                return;
            }

            scale = Simd8Lane(sc, 0);
            float* remaining[] = { in_l + i, in_r + i };
            Fallback::ApplyGainAndInterleaveToS16(out + 2 * i, remaining, num - i, scale, scale_delta);

            Simd8Finish();
        }

        DM_SOUND_DSP_LOCALLY_STREAMING static void GatherPowerData_Simd8(float* in[], uint32_t num, float gain, float& sum_sq_left, float& sum_sq_right, float& max_sq_left, float& max_sq_right)
        {
            float* in_l = in[0];
            float* in_r = in[1];
            dsimd_float8_t sum_l = Simd8Splat(0), sum_r = sum_l;
            dsimd_float8_t max_l = sum_l, max_r = sum_l;
            dsimd_float8_t scale = Simd8Splat(gain);
            const unsigned vector_num = num & ~(8 - 1);
            unsigned i = 0;
            for (; i < vector_num; i += 8)
            {
                dsimd_float8_t l = Simd8Mul(Simd8Load(in_l + i), scale);
                dsimd_float8_t r = Simd8Mul(Simd8Load(in_r + i), scale);
                l = Simd8Mul(l, l);
                r = Simd8Mul(r, r);
                sum_l = Simd8Add(sum_l, l);
                sum_r = Simd8Add(sum_r, r);
                max_l = Simd8Max(max_l, l);
                max_r = Simd8Max(max_r, r);
            }

            sum_sq_left = Simd8Sum(sum_l);
            sum_sq_right = Simd8Sum(sum_r);
            max_sq_left = Simd8ReduceMax(max_l);
            max_sq_right = Simd8ReduceMax(max_r);
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

            Simd8Finish();
        }

        DM_SOUND_DSP_LOCALLY_STREAMING static void ConvertFromS16_Simd8(float* out, const int16_t* in, uint32_t num)
        {
            unsigned i = 0;
            for (; num - i >= 8; i += 8)
            {
                Simd8Store(out + i, Simd8LoadS16ToFloat(in + i));
            }

            Fallback::ConvertFromS16(out + i, in + i, num - i);

            Simd8Finish();
        }

        DM_SOUND_DSP_LOCALLY_STREAMING static void ConvertFromS8_Simd8(float* out, const int8_t* in, uint32_t num)
        {
            const unsigned vector_num = num & ~(8 - 1);
            unsigned i = 0;
            for (; i < vector_num; i += 8)
            {
                Simd8Store(out + i, Simd8LoadS8ToS16Float(in + i));
            }

            Fallback::ConvertFromS8(out + i, in + i, num - i);

            Simd8Finish();
        }

        DM_SOUND_DSP_LOCALLY_STREAMING static void Deinterleave_Simd8(float* out[], const float* in, uint32_t num)
        {
            float* out_l = out[0];
            float* out_r = out[1];
            const unsigned vector_num = num & ~(8 - 1);
            unsigned i = 0;
            for (; i < vector_num; i += 8)
            {
                dsimd_float8_t a = Simd8Load(in + 2 * i);
                dsimd_float8_t b = Simd8Load(in + 2 * i + 8);
                dsimd_float8_t left, right;
                Simd8Unzip(a, b, &left, &right);
                Simd8Store(out_l + i, left);
                Simd8Store(out_r + i, right);
            }

            float* destination[] = { out_l + i, out_r + i };
            Fallback::Deinterleave(destination, in + 2 * i, num - i);

            Simd8Finish();
        }

        DM_SOUND_DSP_LOCALLY_STREAMING static void DeinterleaveFromS16_Simd8(float* out[], const int16_t* in, uint32_t num)
        {
            float* out_l = out[0];
            float* out_r = out[1];
            const unsigned vector_num = num & ~(8 - 1);
            unsigned i = 0;
            for (; i < vector_num; i += 8)
            {
                dsimd_float8_t a = Simd8LoadS16ToFloat(in + 2 * i);
                dsimd_float8_t b = Simd8LoadS16ToFloat(in + 2 * i + 8);
                dsimd_float8_t left, right;
                Simd8Unzip(a, b, &left, &right);
                Simd8Store(out_l + i, left);
                Simd8Store(out_r + i, right);
            }

            float* destination[] = { out_l + i, out_r + i };
            Fallback::DeinterleaveFromS16(destination, in + 2 * i, num - i);

            Simd8Finish();
        }

        DM_SOUND_DSP_LOCALLY_STREAMING static void DeinterleaveFromS8_Simd8(float* out[], const int8_t* in, uint32_t num)
        {
            float* out_l = out[0];
            float* out_r = out[1];
            const unsigned vector_num = num & ~(8 - 1);
            unsigned i = 0;
            for (; i < vector_num; i += 8)
            {
                dsimd_float8_t a = Simd8LoadS8ToS16Float(in + 2 * i);
                dsimd_float8_t b = Simd8LoadS8ToS16Float(in + 2 * i + 8);
                dsimd_float8_t left, right;
                Simd8Unzip(a, b, &left, &right);
                Simd8Store(out_l + i, left);
                Simd8Store(out_r + i, right);
            }

            float* destination[] = { out_l + i, out_r + i };
            Fallback::DeinterleaveFromS8(destination, in + 2 * i, num - i);

            Simd8Finish();
        }

    } // namespace DM_SOUND_DSP_NAMESPACE

    const DSPImpl* DM_SOUND_DSP_GETTER()
    {
        static const DSPImpl impl =
        {
            DM_SOUND_DSP_NAME,
            8,
            DM_SOUND_DSP_NAMESPACE::MixScaledMonoToStereo_Simd8,
            DM_SOUND_DSP_NAMESPACE::MixScaledStereoToStereo_Simd8,
            DM_SOUND_DSP_NAMESPACE::MixAndResampleMonoToStereo_Polyphase_Simd8,
            DM_SOUND_DSP_NAMESPACE::MixAndResampleStereoToStereo_Polyphase_Simd8,
            DM_SOUND_DSP_NAMESPACE::ApplyClampedGain_Simd8,
            DM_SOUND_DSP_NAMESPACE::ApplyGainAndInterleaveToS16_Simd8,
            DM_SOUND_DSP_NAMESPACE::ApplyGain_Simd8,
            DM_SOUND_DSP_NAMESPACE::GatherPowerData_Simd8,
            DM_SOUND_DSP_NAMESPACE::ConvertFromS16_Simd8,
            DM_SOUND_DSP_NAMESPACE::ConvertFromS8_Simd8,
            DM_SOUND_DSP_NAMESPACE::Deinterleave_Simd8,
            DM_SOUND_DSP_NAMESPACE::DeinterleaveFromS16_Simd8,
            DM_SOUND_DSP_NAMESPACE::DeinterleaveFromS8_Simd8,
        };
        return &impl;
    }

} // namespace dmSound
