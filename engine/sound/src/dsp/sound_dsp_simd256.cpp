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
    namespace Dsp
    {
        static void MixScaledMonoToStereo_Simd256(float* out[], const float* in, uint32_t num, float scale_l, float scale_r, float scale_delta_l, float scale_delta_r)
        {
            float* out_l = out[0];
            float* out_r = out[1];
            dsimd_float8_t sc_l = Simd256Ramp(scale_l, scale_delta_l);
            dsimd_float8_t step_l = Simd256Splat(scale_delta_l * 8);
            dsimd_float8_t sc_r = Simd256Ramp(scale_r, scale_delta_r);
            dsimd_float8_t step_r = Simd256Splat(scale_delta_r * 8);
            const unsigned vector_num = num & ~(8 - 1);
            unsigned i = 0;
            for (; i < vector_num; i += 8)
            {
                dsimd_float8_t sample = Simd256Load(in + i);
                Simd256Store(out_l + i, Simd256Add(Simd256Load(out_l + i), Simd256Mul(sample, sc_l)));
                Simd256Store(out_r + i, Simd256Add(Simd256Load(out_r + i), Simd256Mul(sample, sc_r)));
                sc_l = Simd256Add(sc_l, step_l);
                sc_r = Simd256Add(sc_r, step_r);
            }

            if (i == num)
            {
                Simd256Finish();
                return;
            }

            scale_l = Simd256Lane(sc_l, 0);
            scale_r = Simd256Lane(sc_r, 0);
            for (; i < num; ++i)
            {
                float sample = in[i];
                out_l[i] += sample * scale_l;
                out_r[i] += sample * scale_r;
                scale_l += scale_delta_l;
                scale_r += scale_delta_r;
            }

            Simd256Finish();
        }

        static uint64_t MixAndResampleMonoToStereo_Polyphase_Simd256(float* out[], const float* in, uint32_t num, uint64_t frac, uint64_t delta, float scale_l, float scale_r, float scale_delta_l, float scale_delta_r)
        {
            DM_PROFILE("MixAndResampleMonoToStereo_Polyphase_SIMD");

            dsimd_float8_t sc_l = Simd256Ramp(scale_l, scale_delta_l);
            dsimd_float8_t step_l = Simd256Splat(scale_delta_l * 8);
            dsimd_float8_t sc_r = Simd256Ramp(scale_r, scale_delta_r);
            dsimd_float8_t step_r = Simd256Splat(scale_delta_r * 8);
            const unsigned vector_num = num & ~(8 - 1);
            unsigned i = 0;
            for (; i < vector_num; i += 8)
            {
                dsimd_float8_t sample = Resample_Simd256(in, frac, delta);
                frac += delta * 8;
                Simd256Store(out[0] + i, Simd256Add(Simd256Load(out[0] + i), Simd256Mul(sample, sc_l)));
                Simd256Store(out[1] + i, Simd256Add(Simd256Load(out[1] + i), Simd256Mul(sample, sc_r)));
                sc_l = Simd256Add(sc_l, step_l);
                sc_r = Simd256Add(sc_r, step_r);
            }

            if (i == num)
            {
                Simd256Finish();
                return frac;
            }

            scale_l = Simd256Lane(sc_l, 0);
            scale_r = Simd256Lane(sc_r, 0);
            for (; i < num; ++i)
            {
                float sample = FilterSampleFIR8_Simd256(in + (frac >> RESAMPLE_FRACTION_BITS), frac);
                out[0][i] += sample * scale_l;
                out[1][i] += sample * scale_r;
                scale_l += scale_delta_l;
                scale_r += scale_delta_r;
                frac += delta;
            }

            Simd256Finish();
            return frac;
        }

#if defined(_MSC_VER) && !defined(__clang__)
        // MSVC cannot align functions directly. A private PE code section keeps this helper 64-byte aligned.
        __declspec(noinline) __declspec(code_seg(".dsp8mix"))
#endif
        static inline void MixConstantStereoToStereo_Simd256(float* out[], const float* in_l, const float* in_r, uint32_t num, float scale_l0, float scale_r0, float scale_l1, float scale_r1)
        {
            float* out_l = out[0];
            float* out_r = out[1];
            dsimd_float8_t sc_l0 = Simd256Splat(scale_l0);
            dsimd_float8_t sc_r0 = Simd256Splat(scale_r0);
            dsimd_float8_t sc_l1 = Simd256Splat(scale_l1);
            dsimd_float8_t sc_r1 = Simd256Splat(scale_r1);
            const unsigned vector_num = num & ~(8 - 1);
            unsigned i = 0;
            for (; i < vector_num; i += 8)
            {
                dsimd_float8_t s_l = Simd256Load(in_l + i);
                dsimd_float8_t s_r = Simd256Load(in_r + i);
                Simd256Store(out_l + i, Simd256Add(Simd256Load(out_l + i), Simd256Add(Simd256Mul(s_l, sc_l0), Simd256Mul(s_r, sc_l1))));
                Simd256Store(out_r + i, Simd256Add(Simd256Load(out_r + i), Simd256Add(Simd256Mul(s_l, sc_r0), Simd256Mul(s_r, sc_r1))));
            }

            for (; i < num; ++i)
            {
                float s_l = in_l[i];
                float s_r = in_r[i];
                out_l[i] += s_l * scale_l0 + s_r * scale_l1;
                out_r[i] += s_l * scale_r0 + s_r * scale_r1;
            }

            Simd256Finish();
        }

        static void MixScaledStereoToStereo_Simd256(float* out[], const float* in_l, const float* in_r, uint32_t num, float scale_l0, float scale_r0, float scale_delta_l0, float scale_delta_r0, float scale_l1, float scale_r1, float scale_delta_l1, float scale_delta_r1)
        {
            // Steady-volume playback does not need per-vector ramp updates.
            if (scale_delta_l0 == 0.0f && scale_delta_r0 == 0.0f && scale_delta_l1 == 0.0f && scale_delta_r1 == 0.0f)
            {
                MixConstantStereoToStereo_Simd256(out, in_l, in_r, num, scale_l0, scale_r0, scale_l1, scale_r1);
                Simd256Finish();
                return;
            }

            float* out_l = out[0];
            float* out_r = out[1];
            dsimd_float8_t sc_l0 = Simd256Ramp(scale_l0, scale_delta_l0);
            dsimd_float8_t step_l0 = Simd256Splat(scale_delta_l0 * 8);
            dsimd_float8_t sc_r0 = Simd256Ramp(scale_r0, scale_delta_r0);
            dsimd_float8_t step_r0 = Simd256Splat(scale_delta_r0 * 8);
            dsimd_float8_t sc_l1 = Simd256Ramp(scale_l1, scale_delta_l1);
            dsimd_float8_t step_l1 = Simd256Splat(scale_delta_l1 * 8);
            dsimd_float8_t sc_r1 = Simd256Ramp(scale_r1, scale_delta_r1);
            dsimd_float8_t step_r1 = Simd256Splat(scale_delta_r1 * 8);
            unsigned i = 0;
            for (; num - i >= 8; i += 8)
            {
                dsimd_float8_t s_l = Simd256Load(in_l + i);
                dsimd_float8_t s_r = Simd256Load(in_r + i);
                Simd256Store(out_l + i, Simd256Add(Simd256Load(out_l + i), Simd256Add(Simd256Mul(s_l, sc_l0), Simd256Mul(s_r, sc_l1))));
                Simd256Store(out_r + i, Simd256Add(Simd256Load(out_r + i), Simd256Add(Simd256Mul(s_l, sc_r0), Simd256Mul(s_r, sc_r1))));
                sc_l0 = Simd256Add(sc_l0, step_l0);
                sc_r0 = Simd256Add(sc_r0, step_r0);
                sc_l1 = Simd256Add(sc_l1, step_l1);
                sc_r1 = Simd256Add(sc_r1, step_r1);
            }

            if (i == num)
            {
                Simd256Finish();
                return;
            }

            scale_l0 = Simd256Lane(sc_l0, 0);
            scale_r0 = Simd256Lane(sc_r0, 0);
            scale_l1 = Simd256Lane(sc_l1, 0);
            scale_r1 = Simd256Lane(sc_r1, 0);
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

            Simd256Finish();
        }

        static inline uint64_t MixAndResampleStereoDiagonal_Simd256(float* out[], const float* in_l, const float* in_r, uint32_t num, uint64_t frac, uint64_t delta, float scale_l, float scale_r, float scale_delta_l, float scale_delta_r)
        {
            dsimd_float8_t sc_l = Simd256Ramp(scale_l, scale_delta_l);
            dsimd_float8_t sc_r = Simd256Ramp(scale_r, scale_delta_r);
            dsimd_float8_t step_l = Simd256Splat(scale_delta_l * 8);
            dsimd_float8_t step_r = Simd256Splat(scale_delta_r * 8);
            const unsigned vector_num = num & ~(8 - 1);
            unsigned i = 0;
            for (; i < vector_num; i += 8)
            {
                dsimd_float8_t s_l, s_r;
                ResampleStereo_Simd256(in_l, in_r, frac, delta, &s_l, &s_r);
                frac += delta * 8;
                Simd256Store(out[0] + i, Simd256Add(Simd256Load(out[0] + i), Simd256Mul(s_l, sc_l)));
                Simd256Store(out[1] + i, Simd256Add(Simd256Load(out[1] + i), Simd256Mul(s_r, sc_r)));
                sc_l = Simd256Add(sc_l, step_l);
                sc_r = Simd256Add(sc_r, step_r);
            }

            if (i == num)
            {
                Simd256Finish();
                return frac;
            }

            scale_l = Simd256Lane(sc_l, 0);
            scale_r = Simd256Lane(sc_r, 0);
            for (; i < num; ++i)
            {
                float s_l, s_r;
                FilterStereoFIR8_Simd256(in_l, in_r, frac, &s_l, &s_r);
                out[0][i] += s_l * scale_l;
                out[1][i] += s_r * scale_r;
                scale_l += scale_delta_l;
                scale_r += scale_delta_r;
                frac += delta;
            }

            Simd256Finish();
            return frac;
        }

        static uint64_t MixAndResampleStereoToStereo_Polyphase_Simd256(float* out[], const float* in_l, const float* in_r, uint32_t num, uint64_t frac, uint64_t delta, float scale_l0, float scale_r0, float scale_delta_l0, float scale_delta_r0, float scale_l1, float scale_r1, float scale_delta_l1, float scale_delta_r1)
        {
            DM_PROFILE("MixAndResampleStereoToStereo_Polyphase_SIMD");

            if (scale_r0 == 0.0f && scale_delta_r0 == 0.0f && scale_l1 == 0.0f && scale_delta_l1 == 0.0f)
            {
                uint64_t result = MixAndResampleStereoDiagonal_Simd256(out, in_l, in_r, num, frac, delta, scale_l0, scale_r1, scale_delta_l0, scale_delta_r1);
                Simd256Finish();
                return result;
            }

            dsimd_float8_t sc_l0 = Simd256Ramp(scale_l0, scale_delta_l0);
            dsimd_float8_t step_l0 = Simd256Splat(scale_delta_l0 * 8);
            dsimd_float8_t sc_r0 = Simd256Ramp(scale_r0, scale_delta_r0);
            dsimd_float8_t step_r0 = Simd256Splat(scale_delta_r0 * 8);
            dsimd_float8_t sc_l1 = Simd256Ramp(scale_l1, scale_delta_l1);
            dsimd_float8_t step_l1 = Simd256Splat(scale_delta_l1 * 8);
            dsimd_float8_t sc_r1 = Simd256Ramp(scale_r1, scale_delta_r1);
            dsimd_float8_t step_r1 = Simd256Splat(scale_delta_r1 * 8);
            const unsigned vector_num = num & ~(8 - 1);
            unsigned i = 0;
            for (; i < vector_num; i += 8)
            {
                dsimd_float8_t s_l, s_r;
                ResampleStereo_Simd256(in_l, in_r, frac, delta, &s_l, &s_r);
                frac += delta * 8;
                Simd256Store(out[0] + i, Simd256Add(Simd256Load(out[0] + i), Simd256Add(Simd256Mul(s_l, sc_l0), Simd256Mul(s_r, sc_l1))));
                Simd256Store(out[1] + i, Simd256Add(Simd256Load(out[1] + i), Simd256Add(Simd256Mul(s_l, sc_r0), Simd256Mul(s_r, sc_r1))));
                sc_l0 = Simd256Add(sc_l0, step_l0);
                sc_r0 = Simd256Add(sc_r0, step_r0);
                sc_l1 = Simd256Add(sc_l1, step_l1);
                sc_r1 = Simd256Add(sc_r1, step_r1);
            }

            if (i == num)
            {
                Simd256Finish();
                return frac;
            }

            scale_l0 = Simd256Lane(sc_l0, 0);
            scale_r0 = Simd256Lane(sc_r0, 0);
            scale_l1 = Simd256Lane(sc_l1, 0);
            scale_r1 = Simd256Lane(sc_r1, 0);
            for (; i < num; ++i)
            {
                float s_l = FilterSampleFIR8_Simd256(in_l + (frac >> RESAMPLE_FRACTION_BITS), frac);
                float s_r = FilterSampleFIR8_Simd256(in_r + (frac >> RESAMPLE_FRACTION_BITS), frac);
                out[0][i] += s_l * scale_l0 + s_r * scale_l1;
                out[1][i] += s_l * scale_r0 + s_r * scale_r1;
                scale_l0 += scale_delta_l0;
                scale_r0 += scale_delta_r0;
                scale_l1 += scale_delta_l1;
                scale_r1 += scale_delta_r1;
                frac += delta;
            }

            Simd256Finish();
            return frac;
        }

        static void ApplyClampedGain_Simd256(float* out[], float* in[], uint32_t num, float scale, float scale_delta)
        {
            float* in_l = in[0];
            float* in_r = in[1];
            float* out_l = out[0];
            float* out_r = out[1];
            dsimd_float8_t sc = Simd256Ramp(scale, scale_delta);
            dsimd_float8_t step = Simd256Splat(scale_delta * 8);
            unsigned i = 0;
            for (; num - i >= 8; i += 8)
            {
                dsimd_float8_t gain = Simd256Min(Simd256Max(sc, Simd256Splat(0)), Simd256Splat(1));
                dsimd_float8_t s0 = Simd256Mul(Simd256Load(in_l + i), gain);
                dsimd_float8_t s1 = Simd256Mul(Simd256Load(in_r + i), gain);
                Simd256Store(out_l + i, Simd256Add(Simd256Load(out_l + i), s0));
                Simd256Store(out_r + i, Simd256Add(Simd256Load(out_r + i), s1));
                sc = Simd256Add(sc, step);
            }

            if (i == num)
            {
                Simd256Finish();
                return;
            }

            scale = Simd256Lane(sc, 0);
            float* remaining[] = { in_l + i, in_r + i };
            float* destination[] = { out_l + i, out_r + i };
            CPU::ApplyClampedGain(destination, remaining, num - i, scale, scale_delta);

            Simd256Finish();
        }

        static void ApplyGain_Simd256(float* out[], float* in[], uint32_t num, float scale, float scale_delta)
        {
            float* in_l = in[0];
            float* in_r = in[1];
            float* out_l = out[0];
            float* out_r = out[1];
            dsimd_float8_t sc = Simd256Ramp(scale, scale_delta);
            dsimd_float8_t step = Simd256Splat(scale_delta * 8);
            const unsigned vector_num = num & ~(8 - 1);
            unsigned i = 0;
            for (; i < vector_num; i += 8)
            {
                dsimd_float8_t gain = sc;
                dsimd_float8_t s0 = Simd256Mul(Simd256Load(in_l + i), gain);
                dsimd_float8_t s1 = Simd256Mul(Simd256Load(in_r + i), gain);
                Simd256Store(out_l + i, s0);
                Simd256Store(out_r + i, s1);
                sc = Simd256Add(sc, step);
            }

            if (i == num)
            {
                Simd256Finish();
                return;
            }

            scale = Simd256Lane(sc, 0);
            float* remaining[] = { in_l + i, in_r + i };
            float* destination[] = { out_l + i, out_r + i };
            CPU::ApplyGain(destination, remaining, num - i, scale, scale_delta);

            Simd256Finish();
        }

        static void ApplyGainAndInterleaveToS16_Simd256(int16_t* out, float* in[], uint32_t num, float scale, float scale_delta)
        {
            float* in_l = in[0];
            float* in_r = in[1];
            dsimd_float8_t sc = Simd256Ramp(scale, scale_delta);
            dsimd_float8_t step = Simd256Splat(scale_delta * 8);
            const unsigned vector_num = num & ~(8 - 1);
            unsigned i = 0;
            for (; i < vector_num; i += 8)
            {
                dsimd_float8_t gain = sc;
                dsimd_float8_t s0 = Simd256Mul(Simd256Load(in_l + i), gain);
                dsimd_float8_t s1 = Simd256Mul(Simd256Load(in_r + i), gain);
                Simd256StoreInterleavedS16(out + 2 * i, s0, s1);
                sc = Simd256Add(sc, step);
            }

            if (i == num)
            {
                Simd256Finish();
                return;
            }

            scale = Simd256Lane(sc, 0);
            float* remaining[] = { in_l + i, in_r + i };
            CPU::ApplyGainAndInterleaveToS16(out + 2 * i, remaining, num - i, scale, scale_delta);

            Simd256Finish();
        }

        static void GatherPowerData_Simd256(float* in[], uint32_t num, float gain, float& sum_sq_left, float& sum_sq_right, float& max_sq_left, float& max_sq_right)
        {
            float* in_l = in[0];
            float* in_r = in[1];
            dsimd_float8_t sum_l = Simd256Splat(0), sum_r = sum_l;
            dsimd_float8_t max_l = sum_l, max_r = sum_l;
            dsimd_float8_t scale = Simd256Splat(gain);
            const unsigned vector_num = num & ~(8 - 1);
            unsigned i = 0;
            for (; i < vector_num; i += 8)
            {
                dsimd_float8_t l = Simd256Mul(Simd256Load(in_l + i), scale);
                dsimd_float8_t r = Simd256Mul(Simd256Load(in_r + i), scale);
                l = Simd256Mul(l, l);
                r = Simd256Mul(r, r);
                sum_l = Simd256Add(sum_l, l);
                sum_r = Simd256Add(sum_r, r);
                max_l = Simd256Max(max_l, l);
                max_r = Simd256Max(max_r, r);
            }

            sum_sq_left = Simd256Sum(sum_l);
            sum_sq_right = Simd256Sum(sum_r);
            max_sq_left = Simd256ReduceMax(max_l);
            max_sq_right = Simd256ReduceMax(max_r);
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

            Simd256Finish();
        }

        static void ConvertFromS16_Simd256(float* out, const int16_t* in, uint32_t num)
        {
            unsigned i = 0;
            for (; num - i >= 8; i += 8)
            {
                Simd256Store(out + i, Simd256LoadS16ToFloat(in + i));
            }

            CPU::ConvertFromS16(out + i, in + i, num - i);

            Simd256Finish();
        }

        static void ConvertFromS8_Simd256(float* out, const int8_t* in, uint32_t num)
        {
            const unsigned vector_num = num & ~(8 - 1);
            unsigned i = 0;
            for (; i < vector_num; i += 8)
            {
                Simd256Store(out + i, Simd256LoadS8ToS16Float(in + i));
            }

            CPU::ConvertFromS8(out + i, in + i, num - i);

            Simd256Finish();
        }

        static void Deinterleave_Simd256(float* out[], const float* in, uint32_t num)
        {
            float* out_l = out[0];
            float* out_r = out[1];
            const unsigned vector_num = num & ~(8 - 1);
            unsigned i = 0;
            for (; i < vector_num; i += 8)
            {
                dsimd_float8_t a = Simd256Load(in + 2 * i);
                dsimd_float8_t b = Simd256Load(in + 2 * i + 8);
                dsimd_float8_t left, right;
                Simd256Unzip(a, b, &left, &right);
                Simd256Store(out_l + i, left);
                Simd256Store(out_r + i, right);
            }

            float* destination[] = { out_l + i, out_r + i };
            CPU::Deinterleave(destination, in + 2 * i, num - i);

            Simd256Finish();
        }

        static void DeinterleaveFromS16_Simd256(float* out[], const int16_t* in, uint32_t num)
        {
            float* out_l = out[0];
            float* out_r = out[1];
            const unsigned vector_num = num & ~(8 - 1);
            unsigned i = 0;
            for (; i < vector_num; i += 8)
            {
                dsimd_float8_t a = Simd256LoadS16ToFloat(in + 2 * i);
                dsimd_float8_t b = Simd256LoadS16ToFloat(in + 2 * i + 8);
                dsimd_float8_t left, right;
                Simd256Unzip(a, b, &left, &right);
                Simd256Store(out_l + i, left);
                Simd256Store(out_r + i, right);
            }

            float* destination[] = { out_l + i, out_r + i };
            CPU::DeinterleaveFromS16(destination, in + 2 * i, num - i);

            Simd256Finish();
        }

        static void DeinterleaveFromS8_Simd256(float* out[], const int8_t* in, uint32_t num)
        {
            float* out_l = out[0];
            float* out_r = out[1];
            const unsigned vector_num = num & ~(8 - 1);
            unsigned i = 0;
            for (; i < vector_num; i += 8)
            {
                dsimd_float8_t a = Simd256LoadS8ToS16Float(in + 2 * i);
                dsimd_float8_t b = Simd256LoadS8ToS16Float(in + 2 * i + 8);
                dsimd_float8_t left, right;
                Simd256Unzip(a, b, &left, &right);
                Simd256Store(out_l + i, left);
                Simd256Store(out_r + i, right);
            }

            float* destination[] = { out_l + i, out_r + i };
            CPU::DeinterleaveFromS8(destination, in + 2 * i, num - i);

            Simd256Finish();
        }

    } // namespace Dsp

#if !defined(DM_TEST_SOUND_DSP)
    extern const DSPImpl simd256_impl =
    {
        8,
        Dsp::MixScaledMonoToStereo_Simd256,
        Dsp::MixScaledStereoToStereo_Simd256,
        Dsp::MixAndResampleMonoToStereo_Polyphase_Simd256,
        Dsp::MixAndResampleStereoToStereo_Polyphase_Simd256,
        Dsp::ApplyClampedGain_Simd256,
        Dsp::ApplyGainAndInterleaveToS16_Simd256,
        Dsp::ApplyGain_Simd256,
        Dsp::GatherPowerData_Simd256,
        Dsp::ConvertFromS16_Simd256,
        Dsp::ConvertFromS8_Simd256,
        Dsp::Deinterleave_Simd256,
        Dsp::DeinterleaveFromS16_Simd256,
        Dsp::DeinterleaveFromS8_Simd256,
    };

#endif

} // namespace dmSound
