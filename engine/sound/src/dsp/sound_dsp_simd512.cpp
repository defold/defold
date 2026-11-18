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

// 16-lane operations compiled for each supported ISA and selected at sound startup.
namespace dmSound
{
    namespace Dsp
    {
        static void MixScaledMonoToStereo_Simd512(float* out[], const float* in, uint32_t num, float scale_l, float scale_r, float scale_delta_l, float scale_delta_r)
        {
            float* out_l = out[0];
            float* out_r = out[1];
            dsimd_float16_t sc_l = Simd512Ramp(scale_l, scale_delta_l);
            dsimd_float16_t step_l = Simd512Splat(scale_delta_l * 16);
            dsimd_float16_t sc_r = Simd512Ramp(scale_r, scale_delta_r);
            dsimd_float16_t step_r = Simd512Splat(scale_delta_r * 16);
            const unsigned vector_num = num & ~(16 - 1);
            unsigned i = 0;
            for (; i < vector_num; i += 16)
            {
                dsimd_float16_t sample = Simd512Load(in + i);
                Simd512Store(out_l + i, Simd512Add(Simd512Load(out_l + i), Simd512Mul(sample, sc_l)));
                Simd512Store(out_r + i, Simd512Add(Simd512Load(out_r + i), Simd512Mul(sample, sc_r)));
                sc_l = Simd512Add(sc_l, step_l);
                sc_r = Simd512Add(sc_r, step_r);
            }

            if (i == num)
            {
                Simd512Finish();
                return;
            }

            scale_l = Simd512Lane(sc_l, 0);
            scale_r = Simd512Lane(sc_r, 0);
            for (; i < num; ++i)
            {
                float sample = in[i];
                out_l[i] += sample * scale_l;
                out_r[i] += sample * scale_r;
                scale_l += scale_delta_l;
                scale_r += scale_delta_r;
            }

            Simd512Finish();
        }

        static uint64_t MixAndResampleMonoToStereo_Polyphase_Simd512(float* out[], const float* in, uint32_t num, uint64_t frac, uint64_t delta, float scale_l, float scale_r, float scale_delta_l, float scale_delta_r)
        {
            DM_PROFILE("MixAndResampleMonoToStereo_Polyphase_SIMD");

            dsimd_float16_t sc_l = Simd512Ramp(scale_l, scale_delta_l);
            dsimd_float16_t step_l = Simd512Splat(scale_delta_l * 16);
            dsimd_float16_t sc_r = Simd512Ramp(scale_r, scale_delta_r);
            dsimd_float16_t step_r = Simd512Splat(scale_delta_r * 16);
            const unsigned vector_num = num & ~(16 - 1);
            unsigned i = 0;
            for (; i < vector_num; i += 16)
            {
                dsimd_float16_t sample = Resample_Simd512(in, frac, delta);
                frac += delta * 16;
                Simd512Store(out[0] + i, Simd512Add(Simd512Load(out[0] + i), Simd512Mul(sample, sc_l)));
                Simd512Store(out[1] + i, Simd512Add(Simd512Load(out[1] + i), Simd512Mul(sample, sc_r)));
                sc_l = Simd512Add(sc_l, step_l);
                sc_r = Simd512Add(sc_r, step_r);
            }

            if (i == num)
            {
                Simd512Finish();
                return frac;
            }

            scale_l = Simd512Lane(sc_l, 0);
            scale_r = Simd512Lane(sc_r, 0);
            for (; i < num; ++i)
            {
                float sample = FilterSampleFIR8_Simd512(in + (frac >> RESAMPLE_FRACTION_BITS), frac);
                out[0][i] += sample * scale_l;
                out[1][i] += sample * scale_r;
                scale_l += scale_delta_l;
                scale_r += scale_delta_r;
                frac += delta;
            }

            Simd512Finish();
            return frac;
        }

        static inline void MixConstantStereoToStereo_Simd512(float* out[], const float* in_l, const float* in_r, uint32_t num, float scale_l0, float scale_r0, float scale_l1, float scale_r1)
        {
            float* out_l = out[0];
            float* out_r = out[1];
            dsimd_float16_t sc_l0 = Simd512Splat(scale_l0);
            dsimd_float16_t sc_r0 = Simd512Splat(scale_r0);
            dsimd_float16_t sc_l1 = Simd512Splat(scale_l1);
            dsimd_float16_t sc_r1 = Simd512Splat(scale_r1);
            const unsigned vector_num = num & ~(16 - 1);
            unsigned i = 0;
            for (; i < vector_num; i += 16)
            {
                dsimd_float16_t s_l = Simd512Load(in_l + i);
                dsimd_float16_t s_r = Simd512Load(in_r + i);
                Simd512Store(out_l + i, Simd512Add(Simd512Load(out_l + i), Simd512Add(Simd512Mul(s_l, sc_l0), Simd512Mul(s_r, sc_l1))));
                Simd512Store(out_r + i, Simd512Add(Simd512Load(out_r + i), Simd512Add(Simd512Mul(s_l, sc_r0), Simd512Mul(s_r, sc_r1))));
            }

            for (; i < num; ++i)
            {
                float s_l = in_l[i];
                float s_r = in_r[i];
                out_l[i] += s_l * scale_l0 + s_r * scale_l1;
                out_r[i] += s_l * scale_r0 + s_r * scale_r1;
            }

            Simd512Finish();
        }

        static void MixScaledStereoToStereo_Simd512(float* out[], const float* in_l, const float* in_r, uint32_t num, float scale_l0, float scale_r0, float scale_delta_l0, float scale_delta_r0, float scale_l1, float scale_r1, float scale_delta_l1, float scale_delta_r1)
        {
            // Steady-volume playback does not need per-vector ramp updates.
            if (scale_delta_l0 == 0.0f && scale_delta_r0 == 0.0f && scale_delta_l1 == 0.0f && scale_delta_r1 == 0.0f)
            {
                MixConstantStereoToStereo_Simd512(out, in_l, in_r, num, scale_l0, scale_r0, scale_l1, scale_r1);
                Simd512Finish();
                return;
            }

            float* out_l = out[0];
            float* out_r = out[1];
            dsimd_float16_t sc_l0 = Simd512Ramp(scale_l0, scale_delta_l0);
            dsimd_float16_t step_l0 = Simd512Splat(scale_delta_l0 * 16);
            dsimd_float16_t sc_r0 = Simd512Ramp(scale_r0, scale_delta_r0);
            dsimd_float16_t step_r0 = Simd512Splat(scale_delta_r0 * 16);
            dsimd_float16_t sc_l1 = Simd512Ramp(scale_l1, scale_delta_l1);
            dsimd_float16_t step_l1 = Simd512Splat(scale_delta_l1 * 16);
            dsimd_float16_t sc_r1 = Simd512Ramp(scale_r1, scale_delta_r1);
            dsimd_float16_t step_r1 = Simd512Splat(scale_delta_r1 * 16);
            unsigned i = 0;
            for (; num - i >= 16; i += 16)
            {
                dsimd_float16_t s_l = Simd512Load(in_l + i);
                dsimd_float16_t s_r = Simd512Load(in_r + i);
                Simd512Store(out_l + i, Simd512Add(Simd512Load(out_l + i), Simd512Add(Simd512Mul(s_l, sc_l0), Simd512Mul(s_r, sc_l1))));
                Simd512Store(out_r + i, Simd512Add(Simd512Load(out_r + i), Simd512Add(Simd512Mul(s_l, sc_r0), Simd512Mul(s_r, sc_r1))));
                sc_l0 = Simd512Add(sc_l0, step_l0);
                sc_r0 = Simd512Add(sc_r0, step_r0);
                sc_l1 = Simd512Add(sc_l1, step_l1);
                sc_r1 = Simd512Add(sc_r1, step_r1);
            }

            if (i == num)
            {
                Simd512Finish();
                return;
            }

            scale_l0 = Simd512Lane(sc_l0, 0);
            scale_r0 = Simd512Lane(sc_r0, 0);
            scale_l1 = Simd512Lane(sc_l1, 0);
            scale_r1 = Simd512Lane(sc_r1, 0);
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

            Simd512Finish();
        }

        static inline uint64_t MixAndResampleStereoDiagonal_Simd512(float* out[], const float* in_l, const float* in_r, uint32_t num, uint64_t frac, uint64_t delta, float scale_l, float scale_r, float scale_delta_l, float scale_delta_r)
        {
            dsimd_float16_t sc_l = Simd512Ramp(scale_l, scale_delta_l);
            dsimd_float16_t sc_r = Simd512Ramp(scale_r, scale_delta_r);
            dsimd_float16_t step_l = Simd512Splat(scale_delta_l * 16);
            dsimd_float16_t step_r = Simd512Splat(scale_delta_r * 16);
            const unsigned vector_num = num & ~(16 - 1);
            unsigned i = 0;
            for (; i < vector_num; i += 16)
            {
                dsimd_float16_t s_l, s_r;
                ResampleStereo_Simd512(in_l, in_r, frac, delta, &s_l, &s_r);
                frac += delta * 16;
                Simd512Store(out[0] + i, Simd512Add(Simd512Load(out[0] + i), Simd512Mul(s_l, sc_l)));
                Simd512Store(out[1] + i, Simd512Add(Simd512Load(out[1] + i), Simd512Mul(s_r, sc_r)));
                sc_l = Simd512Add(sc_l, step_l);
                sc_r = Simd512Add(sc_r, step_r);
            }

            if (i == num)
            {
                Simd512Finish();
                return frac;
            }

            scale_l = Simd512Lane(sc_l, 0);
            scale_r = Simd512Lane(sc_r, 0);
            for (; i < num; ++i)
            {
                float s_l, s_r;
                FilterStereoFIR8_Simd512(in_l, in_r, frac, &s_l, &s_r);
                out[0][i] += s_l * scale_l;
                out[1][i] += s_r * scale_r;
                scale_l += scale_delta_l;
                scale_r += scale_delta_r;
                frac += delta;
            }

            Simd512Finish();
            return frac;
        }

        static uint64_t MixAndResampleStereoToStereo_Polyphase_Simd512(float* out[], const float* in_l, const float* in_r, uint32_t num, uint64_t frac, uint64_t delta, float scale_l0, float scale_r0, float scale_delta_l0, float scale_delta_r0, float scale_l1, float scale_r1, float scale_delta_l1, float scale_delta_r1)
        {
            DM_PROFILE("MixAndResampleStereoToStereo_Polyphase_SIMD");

            if (scale_r0 == 0.0f && scale_delta_r0 == 0.0f && scale_l1 == 0.0f && scale_delta_l1 == 0.0f)
            {
                uint64_t result = MixAndResampleStereoDiagonal_Simd512(out, in_l, in_r, num, frac, delta, scale_l0, scale_r1, scale_delta_l0, scale_delta_r1);
                Simd512Finish();
                return result;
            }

            dsimd_float16_t sc_l0 = Simd512Ramp(scale_l0, scale_delta_l0);
            dsimd_float16_t step_l0 = Simd512Splat(scale_delta_l0 * 16);
            dsimd_float16_t sc_r0 = Simd512Ramp(scale_r0, scale_delta_r0);
            dsimd_float16_t step_r0 = Simd512Splat(scale_delta_r0 * 16);
            dsimd_float16_t sc_l1 = Simd512Ramp(scale_l1, scale_delta_l1);
            dsimd_float16_t step_l1 = Simd512Splat(scale_delta_l1 * 16);
            dsimd_float16_t sc_r1 = Simd512Ramp(scale_r1, scale_delta_r1);
            dsimd_float16_t step_r1 = Simd512Splat(scale_delta_r1 * 16);
            const unsigned vector_num = num & ~(16 - 1);
            unsigned i = 0;
            for (; i < vector_num; i += 16)
            {
                dsimd_float16_t s_l, s_r;
                ResampleStereo_Simd512(in_l, in_r, frac, delta, &s_l, &s_r);
                frac += delta * 16;
                Simd512Store(out[0] + i, Simd512Add(Simd512Load(out[0] + i), Simd512Add(Simd512Mul(s_l, sc_l0), Simd512Mul(s_r, sc_l1))));
                Simd512Store(out[1] + i, Simd512Add(Simd512Load(out[1] + i), Simd512Add(Simd512Mul(s_l, sc_r0), Simd512Mul(s_r, sc_r1))));
                sc_l0 = Simd512Add(sc_l0, step_l0);
                sc_r0 = Simd512Add(sc_r0, step_r0);
                sc_l1 = Simd512Add(sc_l1, step_l1);
                sc_r1 = Simd512Add(sc_r1, step_r1);
            }

            if (i == num)
            {
                Simd512Finish();
                return frac;
            }

            scale_l0 = Simd512Lane(sc_l0, 0);
            scale_r0 = Simd512Lane(sc_r0, 0);
            scale_l1 = Simd512Lane(sc_l1, 0);
            scale_r1 = Simd512Lane(sc_r1, 0);
            for (; i < num; ++i)
            {
                float s_l = FilterSampleFIR8_Simd512(in_l + (frac >> RESAMPLE_FRACTION_BITS), frac);
                float s_r = FilterSampleFIR8_Simd512(in_r + (frac >> RESAMPLE_FRACTION_BITS), frac);
                out[0][i] += s_l * scale_l0 + s_r * scale_l1;
                out[1][i] += s_l * scale_r0 + s_r * scale_r1;
                scale_l0 += scale_delta_l0;
                scale_r0 += scale_delta_r0;
                scale_l1 += scale_delta_l1;
                scale_r1 += scale_delta_r1;
                frac += delta;
            }

            Simd512Finish();
            return frac;
        }

        static void ApplyClampedGain_Simd512(float* out[], float* in[], uint32_t num, float scale, float scale_delta)
        {
            float* in_l = in[0];
            float* in_r = in[1];
            float* out_l = out[0];
            float* out_r = out[1];
            dsimd_float16_t sc = Simd512Ramp(scale, scale_delta);
            dsimd_float16_t step = Simd512Splat(scale_delta * 16);
            unsigned i = 0;
            for (; num - i >= 16; i += 16)
            {
                dsimd_float16_t gain = Simd512Min(Simd512Max(sc, Simd512Splat(0)), Simd512Splat(1));
                dsimd_float16_t s0 = Simd512Mul(Simd512Load(in_l + i), gain);
                dsimd_float16_t s1 = Simd512Mul(Simd512Load(in_r + i), gain);
                Simd512Store(out_l + i, Simd512Add(Simd512Load(out_l + i), s0));
                Simd512Store(out_r + i, Simd512Add(Simd512Load(out_r + i), s1));
                sc = Simd512Add(sc, step);
            }

            if (i == num)
            {
                Simd512Finish();
                return;
            }

            scale = Simd512Lane(sc, 0);
            float* remaining[] = { in_l + i, in_r + i };
            float* destination[] = { out_l + i, out_r + i };
            CPU::ApplyClampedGain(destination, remaining, num - i, scale, scale_delta);

            Simd512Finish();
        }

        static void ApplyGain_Simd512(float* out[], float* in[], uint32_t num, float scale, float scale_delta)
        {
            float* in_l = in[0];
            float* in_r = in[1];
            float* out_l = out[0];
            float* out_r = out[1];
            dsimd_float16_t sc = Simd512Ramp(scale, scale_delta);
            dsimd_float16_t step = Simd512Splat(scale_delta * 16);
            const unsigned vector_num = num & ~(16 - 1);
            unsigned i = 0;
            for (; i < vector_num; i += 16)
            {
                dsimd_float16_t gain = sc;
                dsimd_float16_t s0 = Simd512Mul(Simd512Load(in_l + i), gain);
                dsimd_float16_t s1 = Simd512Mul(Simd512Load(in_r + i), gain);
                Simd512Store(out_l + i, s0);
                Simd512Store(out_r + i, s1);
                sc = Simd512Add(sc, step);
            }

            if (i == num)
            {
                Simd512Finish();
                return;
            }

            scale = Simd512Lane(sc, 0);
            float* remaining[] = { in_l + i, in_r + i };
            float* destination[] = { out_l + i, out_r + i };
            CPU::ApplyGain(destination, remaining, num - i, scale, scale_delta);

            Simd512Finish();
        }

        static void ApplyGainAndInterleaveToS16_Simd512(int16_t* out, float* in[], uint32_t num, float scale, float scale_delta)
        {
            float* in_l = in[0];
            float* in_r = in[1];
            dsimd_float16_t sc = Simd512Ramp(scale, scale_delta);
            dsimd_float16_t step = Simd512Splat(scale_delta * 16);
            const unsigned vector_num = num & ~(16 - 1);
            unsigned i = 0;
            for (; i < vector_num; i += 16)
            {
                dsimd_float16_t gain = sc;
                dsimd_float16_t s0 = Simd512Mul(Simd512Load(in_l + i), gain);
                dsimd_float16_t s1 = Simd512Mul(Simd512Load(in_r + i), gain);
                Simd512StoreInterleavedS16(out + 2 * i, s0, s1);
                sc = Simd512Add(sc, step);
            }

            if (i == num)
            {
                Simd512Finish();
                return;
            }

            scale = Simd512Lane(sc, 0);
            float* remaining[] = { in_l + i, in_r + i };
            CPU::ApplyGainAndInterleaveToS16(out + 2 * i, remaining, num - i, scale, scale_delta);

            Simd512Finish();
        }

        static void GatherPowerData_Simd512(float* in[], uint32_t num, float gain, float& sum_sq_left, float& sum_sq_right, float& max_sq_left, float& max_sq_right)
        {
            float* in_l = in[0];
            float* in_r = in[1];
            dsimd_float16_t sum_l = Simd512Splat(0), sum_r = sum_l;
            dsimd_float16_t max_l = sum_l, max_r = sum_l;
            dsimd_float16_t scale = Simd512Splat(gain);
            const unsigned vector_num = num & ~(16 - 1);
            unsigned i = 0;
            for (; i < vector_num; i += 16)
            {
                dsimd_float16_t l = Simd512Mul(Simd512Load(in_l + i), scale);
                dsimd_float16_t r = Simd512Mul(Simd512Load(in_r + i), scale);
                l = Simd512Mul(l, l);
                r = Simd512Mul(r, r);
                sum_l = Simd512Add(sum_l, l);
                sum_r = Simd512Add(sum_r, r);
                max_l = Simd512Max(max_l, l);
                max_r = Simd512Max(max_r, r);
            }

            sum_sq_left = Simd512Sum(sum_l);
            sum_sq_right = Simd512Sum(sum_r);
            max_sq_left = Simd512ReduceMax(max_l);
            max_sq_right = Simd512ReduceMax(max_r);
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

            Simd512Finish();
        }

        static void ConvertFromS16_Simd512(float* out, const int16_t* in, uint32_t num)
        {
            unsigned i = 0;
            for (; num - i >= 16; i += 16)
            {
                Simd512Store(out + i, Simd512LoadS16ToFloat(in + i));
            }

            CPU::ConvertFromS16(out + i, in + i, num - i);

            Simd512Finish();
        }

        static void ConvertFromS8_Simd512(float* out, const int8_t* in, uint32_t num)
        {
            const unsigned vector_num = num & ~(16 - 1);
            unsigned i = 0;
            for (; i < vector_num; i += 16)
            {
                Simd512Store(out + i, Simd512LoadS8ToS16Float(in + i));
            }

            CPU::ConvertFromS8(out + i, in + i, num - i);

            Simd512Finish();
        }

        static void Deinterleave_Simd512(float* out[], const float* in, uint32_t num)
        {
            float* out_l = out[0];
            float* out_r = out[1];
            const unsigned vector_num = num & ~(16 - 1);
            unsigned i = 0;
            for (; i < vector_num; i += 16)
            {
                dsimd_float16_t a = Simd512Load(in + 2 * i);
                dsimd_float16_t b = Simd512Load(in + 2 * i + 16);
                dsimd_float16_t left, right;
                Simd512Unzip(a, b, &left, &right);
                Simd512Store(out_l + i, left);
                Simd512Store(out_r + i, right);
            }

            float* destination[] = { out_l + i, out_r + i };
            CPU::Deinterleave(destination, in + 2 * i, num - i);

            Simd512Finish();
        }

        static void DeinterleaveFromS16_Simd512(float* out[], const int16_t* in, uint32_t num)
        {
            float* out_l = out[0];
            float* out_r = out[1];
            const unsigned vector_num = num & ~(16 - 1);
            unsigned i = 0;
            for (; i < vector_num; i += 16)
            {
                dsimd_float16_t a = Simd512LoadS16ToFloat(in + 2 * i);
                dsimd_float16_t b = Simd512LoadS16ToFloat(in + 2 * i + 16);
                dsimd_float16_t left, right;
                Simd512Unzip(a, b, &left, &right);
                Simd512Store(out_l + i, left);
                Simd512Store(out_r + i, right);
            }

            float* destination[] = { out_l + i, out_r + i };
            CPU::DeinterleaveFromS16(destination, in + 2 * i, num - i);

            Simd512Finish();
        }

        static void DeinterleaveFromS8_Simd512(float* out[], const int8_t* in, uint32_t num)
        {
            float* out_l = out[0];
            float* out_r = out[1];
            const unsigned vector_num = num & ~(16 - 1);
            unsigned i = 0;
            for (; i < vector_num; i += 16)
            {
                dsimd_float16_t a = Simd512LoadS8ToS16Float(in + 2 * i);
                dsimd_float16_t b = Simd512LoadS8ToS16Float(in + 2 * i + 16);
                dsimd_float16_t left, right;
                Simd512Unzip(a, b, &left, &right);
                Simd512Store(out_l + i, left);
                Simd512Store(out_r + i, right);
            }

            float* destination[] = { out_l + i, out_r + i };
            CPU::DeinterleaveFromS8(destination, in + 2 * i, num - i);

            Simd512Finish();
        }

    } // namespace Dsp

#if !defined(DM_TEST_SOUND_DSP)
    extern const DSPImpl simd512_impl =
    {
        16,
        Dsp::MixScaledMonoToStereo_Simd512,
        Dsp::MixScaledStereoToStereo_Simd512,
        Dsp::MixAndResampleMonoToStereo_Polyphase_Simd512,
        Dsp::MixAndResampleStereoToStereo_Polyphase_Simd512,
        Dsp::ApplyClampedGain_Simd512,
        Dsp::ApplyGainAndInterleaveToS16_Simd512,
        Dsp::ApplyGain_Simd512,
        Dsp::GatherPowerData_Simd512,
        Dsp::ConvertFromS16_Simd512,
        Dsp::ConvertFromS8_Simd512,
        Dsp::Deinterleave_Simd512,
        Dsp::DeinterleaveFromS16_Simd512,
        Dsp::DeinterleaveFromS8_Simd512,
    };

#endif

} // namespace dmSound
