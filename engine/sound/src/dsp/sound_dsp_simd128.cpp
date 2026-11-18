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
#include "sound_dsp_scalar.h"
#include DM_SOUND_DSP_RESAMPLE_HEADER
#include <dlib/math.h>
#include <dlib/profile.h>

// Unroll is 1, 2, 4 or 8 native four-lane vectors per audio iteration.
// Each backend selects its measured specializations in its startup callback table.
namespace dmSound
{
    namespace Dsp
    {
        template <unsigned Unroll>
        static inline void MixConstantMonoToStereo_Simd128(float* out[], const float* in, uint32_t num, float scale_l, float scale_r)
        {
            float* out_l = out[0];
            float* out_r = out[1];
            dsimd_float4_t gain_l = Simd128Splat(scale_l);
            dsimd_float4_t gain_r = Simd128Splat(scale_r);
            const unsigned vector_num = num & ~(4 * Unroll - 1);
            unsigned i = 0;
            for (; i < vector_num; i += 4 * Unroll)
            {
                for (unsigned u = 0; u < Unroll; ++u)
                {
                    dsimd_float4_t sample = Simd128Load((in + i) + u * 4);
                    Simd128Store((out_l + i) + u * 4, Simd128Add(Simd128Load((out_l + i) + u * 4), Simd128Mul(sample, gain_l)));
                    Simd128Store((out_r + i) + u * 4, Simd128Add(Simd128Load((out_r + i) + u * 4), Simd128Mul(sample, gain_r)));
                }
            }

            for (; i < num; ++i)
            {
                out_l[i] += in[i] * scale_l;
                out_r[i] += in[i] * scale_r;
            }
        }

        template <unsigned Unroll>
        static void MixScaledMonoToStereo_Simd128(float* out[], const float* in, uint32_t num, float scale_l, float scale_r, float scale_delta_l, float scale_delta_r)
        {
            if (scale_delta_l == 0.0f && scale_delta_r == 0.0f)
            {
                MixConstantMonoToStereo_Simd128<Unroll>(out, in, num, scale_l, scale_r);
                Simd128Finish();
                return;
            }

            float* out_l = out[0];
            float* out_r = out[1];
            dsimd_float4_t sc_l[Unroll];
            dsimd_float4_t step_l = Simd128Splat(scale_delta_l * (4 * Unroll));
            dsimd_float4_t sc_r[Unroll];
            dsimd_float4_t step_r = Simd128Splat(scale_delta_r * (4 * Unroll));
            for (unsigned u = 0; u < Unroll; ++u)
            {
                sc_l[u] = Simd128Ramp(scale_l + (float)(u * 4) * scale_delta_l, scale_delta_l);
                sc_r[u] = Simd128Ramp(scale_r + (float)(u * 4) * scale_delta_r, scale_delta_r);
            }

            const unsigned vector_num = num & ~(4 * Unroll - 1);
            unsigned i = 0;
            for (; i < vector_num; i += 4 * Unroll)
            {
                for (unsigned u = 0; u < Unroll; ++u)
                {
                    dsimd_float4_t sample = Simd128Load((in + i) + u * 4);
                    Simd128Store((out_l + i) + u * 4, Simd128Add(Simd128Load((out_l + i) + u * 4), Simd128Mul(sample, sc_l[u])));
                    Simd128Store((out_r + i) + u * 4, Simd128Add(Simd128Load((out_r + i) + u * 4), Simd128Mul(sample, sc_r[u])));
                    sc_l[u] = Simd128Add(sc_l[u], step_l);
                    sc_r[u] = Simd128Add(sc_r[u], step_r);
                }
            }

            if (i == num)
            {
                Simd128Finish();
                return;
            }

            scale_l = Simd128Lane(sc_l[0], 0);
            scale_r = Simd128Lane(sc_r[0], 0);
            for (; i < num; ++i)
            {
                float sample = in[i];
                out_l[i] += sample * scale_l;
                out_r[i] += sample * scale_r;
                scale_l += scale_delta_l;
                scale_r += scale_delta_r;
            }

            Simd128Finish();
        }

        template <unsigned Unroll>
        static uint64_t MixAndResampleMonoToStereo_Polyphase_Simd128(float* out[], const float* in, uint32_t num, uint64_t frac, uint64_t delta, float scale_l, float scale_r, float scale_delta_l, float scale_delta_r)
        {
            float* out_l = out[0];
            float* out_r = out[1];
            DM_PROFILE("MixAndResampleMonoToStereo_Polyphase_SIMD");

            dsimd_float4_t sc_l[Unroll];
            dsimd_float4_t step_l = Simd128Splat(scale_delta_l * (4 * Unroll));
            dsimd_float4_t sc_r[Unroll];
            dsimd_float4_t step_r = Simd128Splat(scale_delta_r * (4 * Unroll));
            for (unsigned u = 0; u < Unroll; ++u)
            {
                sc_l[u] = Simd128Ramp(scale_l + (float)(u * 4) * scale_delta_l, scale_delta_l);
                sc_r[u] = Simd128Ramp(scale_r + (float)(u * 4) * scale_delta_r, scale_delta_r);
            }

            const unsigned vector_num = num & ~(4 * Unroll - 1);
            unsigned i = 0;
            for (; i < vector_num; i += 4 * Unroll)
            {
                dsimd_float4_t samples[Unroll];
                Resample_Simd128<Unroll>(in, frac, delta, samples);
                frac += delta * (4 * Unroll);
                for (unsigned u = 0; u < Unroll; ++u)
                {
                    Simd128Store((out_l + i) + u * 4, Simd128Add(Simd128Load((out_l + i) + u * 4), Simd128Mul(samples[u], sc_l[u])));
                    Simd128Store((out_r + i) + u * 4, Simd128Add(Simd128Load((out_r + i) + u * 4), Simd128Mul(samples[u], sc_r[u])));
                    sc_l[u] = Simd128Add(sc_l[u], step_l);
                    sc_r[u] = Simd128Add(sc_r[u], step_r);
                }
            }

            if (i == num)
            {
                Simd128Finish();
                return frac;
            }

            scale_l = Simd128Lane(sc_l[0], 0);
            scale_r = Simd128Lane(sc_r[0], 0);
            for (; i < num; ++i)
            {
                float sample = FilterSampleFIR8_Simd128(in + (frac >> RESAMPLE_FRACTION_BITS), frac);
                out_l[i] += sample * scale_l;
                out_r[i] += sample * scale_r;
                scale_l += scale_delta_l;
                scale_r += scale_delta_r;
                frac += delta;
            }

            Simd128Finish();
            return frac;
        }

        template <unsigned Unroll>
        static inline void MixConstantStereoToStereo_Simd128(float* out[], const float* in_l, const float* in_r, uint32_t num, float scale_l0, float scale_r0, float scale_l1, float scale_r1)
        {
            float* out_l = out[0];
            float* out_r = out[1];
            dsimd_float4_t sc_l0 = Simd128Splat(scale_l0);
            dsimd_float4_t sc_r0 = Simd128Splat(scale_r0);
            dsimd_float4_t sc_l1 = Simd128Splat(scale_l1);
            dsimd_float4_t sc_r1 = Simd128Splat(scale_r1);
            const unsigned vector_num = num & ~(4 * Unroll - 1);
            unsigned i = 0;
            for (; i < vector_num; i += 4 * Unroll)
            {
                for (unsigned u = 0; u < Unroll; ++u)
                {
                    dsimd_float4_t s_l = Simd128Load((in_l + i) + u * 4);
                    dsimd_float4_t s_r = Simd128Load((in_r + i) + u * 4);
                    Simd128Store((out_l + i) + u * 4, Simd128Add(Simd128Load((out_l + i) + u * 4), Simd128Add(Simd128Mul(s_l, sc_l0), Simd128Mul(s_r, sc_l1))));
                    Simd128Store((out_r + i) + u * 4, Simd128Add(Simd128Load((out_r + i) + u * 4), Simd128Add(Simd128Mul(s_l, sc_r0), Simd128Mul(s_r, sc_r1))));
                }
            }

            for (; i < num; ++i)
            {
                float s_l = in_l[i];
                float s_r = in_r[i];
                out_l[i] += s_l * scale_l0 + s_r * scale_l1;
                out_r[i] += s_l * scale_r0 + s_r * scale_r1;
            }

            Simd128Finish();
        }

        template <unsigned Unroll>
        static void MixScaledStereoToStereo_Simd128(float* out[], const float* in_l, const float* in_r, uint32_t num, float scale_l0, float scale_r0, float scale_delta_l0, float scale_delta_r0, float scale_l1, float scale_r1, float scale_delta_l1, float scale_delta_r1)
        {
            // Steady-volume playback does not need per-vector ramp updates.
            if (scale_delta_l0 == 0.0f && scale_delta_r0 == 0.0f && scale_delta_l1 == 0.0f && scale_delta_r1 == 0.0f)
            {
                MixConstantStereoToStereo_Simd128<Unroll>(out, in_l, in_r, num, scale_l0, scale_r0, scale_l1, scale_r1);
                Simd128Finish();
                return;
            }

            float* out_l = out[0];
            float* out_r = out[1];
            dsimd_float4_t sc_l0[Unroll];
            dsimd_float4_t step_l0 = Simd128Splat(scale_delta_l0 * (4 * Unroll));
            dsimd_float4_t sc_r0[Unroll];
            dsimd_float4_t step_r0 = Simd128Splat(scale_delta_r0 * (4 * Unroll));
            dsimd_float4_t sc_l1[Unroll];
            dsimd_float4_t step_l1 = Simd128Splat(scale_delta_l1 * (4 * Unroll));
            dsimd_float4_t sc_r1[Unroll];
            dsimd_float4_t step_r1 = Simd128Splat(scale_delta_r1 * (4 * Unroll));
            for (unsigned u = 0; u < Unroll; ++u)
            {
                sc_l0[u] = Simd128Ramp(scale_l0 + (float)(u * 4) * scale_delta_l0, scale_delta_l0);
                sc_r0[u] = Simd128Ramp(scale_r0 + (float)(u * 4) * scale_delta_r0, scale_delta_r0);
                sc_l1[u] = Simd128Ramp(scale_l1 + (float)(u * 4) * scale_delta_l1, scale_delta_l1);
                sc_r1[u] = Simd128Ramp(scale_r1 + (float)(u * 4) * scale_delta_r1, scale_delta_r1);
            }

            const unsigned vector_num = num & ~(4 * Unroll - 1);
            unsigned i = 0;
            for (; i < vector_num; i += 4 * Unroll)
            {
                for (unsigned u = 0; u < Unroll; ++u)
                {
                    dsimd_float4_t s_l = Simd128Load((in_l + i) + u * 4);
                    dsimd_float4_t s_r = Simd128Load((in_r + i) + u * 4);
                    Simd128Store((out_l + i) + u * 4, Simd128Add(Simd128Load((out_l + i) + u * 4), Simd128Add(Simd128Mul(s_l, sc_l0[u]), Simd128Mul(s_r, sc_l1[u]))));
                    Simd128Store((out_r + i) + u * 4, Simd128Add(Simd128Load((out_r + i) + u * 4), Simd128Add(Simd128Mul(s_l, sc_r0[u]), Simd128Mul(s_r, sc_r1[u]))));
                    sc_l0[u] = Simd128Add(sc_l0[u], step_l0);
                    sc_r0[u] = Simd128Add(sc_r0[u], step_r0);
                    sc_l1[u] = Simd128Add(sc_l1[u], step_l1);
                    sc_r1[u] = Simd128Add(sc_r1[u], step_r1);
                }
            }

            if (i == num)
            {
                Simd128Finish();
                return;
            }

            scale_l0 = Simd128Lane(sc_l0[0], 0);
            scale_r0 = Simd128Lane(sc_r0[0], 0);
            scale_l1 = Simd128Lane(sc_l1[0], 0);
            scale_r1 = Simd128Lane(sc_r1[0], 0);
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

            Simd128Finish();
        }

        template <unsigned Unroll>
        static inline uint64_t MixAndResampleStereoDiagonal_Simd128(float* out[], const float* in_l, const float* in_r, uint32_t num, uint64_t frac, uint64_t delta, float scale_l, float scale_r, float scale_delta_l, float scale_delta_r)
        {
            float* out_l = out[0];
            float* out_r = out[1];
            dsimd_float4_t sc_l[Unroll];
            dsimd_float4_t sc_r[Unroll];
            dsimd_float4_t step_l = Simd128Splat(scale_delta_l * (4 * Unroll));
            dsimd_float4_t step_r = Simd128Splat(scale_delta_r * (4 * Unroll));
            for (unsigned u = 0; u < Unroll; ++u)
            {
                sc_l[u] = Simd128Ramp(scale_l + (float)(u * 4) * scale_delta_l, scale_delta_l);
                sc_r[u] = Simd128Ramp(scale_r + (float)(u * 4) * scale_delta_r, scale_delta_r);
            }

            const unsigned vector_num = num & ~(4 * Unroll - 1);
            unsigned i = 0;
            for (; i < vector_num; i += 4 * Unroll)
            {
                dsimd_float4_t samples_l[Unroll], samples_r[Unroll];
                ResampleStereo_Simd128<Unroll>(in_l, in_r, frac, delta, samples_l, samples_r);
                frac += delta * (4 * Unroll);
                for (unsigned u = 0; u < Unroll; ++u)
                {
                    Simd128Store((out_l + i) + u * 4, Simd128Add(Simd128Load((out_l + i) + u * 4), Simd128Mul(samples_l[u], sc_l[u])));
                    Simd128Store((out_r + i) + u * 4, Simd128Add(Simd128Load((out_r + i) + u * 4), Simd128Mul(samples_r[u], sc_r[u])));
                    sc_l[u] = Simd128Add(sc_l[u], step_l);
                    sc_r[u] = Simd128Add(sc_r[u], step_r);
                }
            }

            if (i == num)
            {
                Simd128Finish();
                return frac;
            }

            scale_l = Simd128Lane(sc_l[0], 0);
            scale_r = Simd128Lane(sc_r[0], 0);
            for (; i < num; ++i)
            {
                float s_l, s_r;
                FilterStereoFIR8_Simd128(in_l, in_r, frac, &s_l, &s_r);
                out_l[i] += s_l * scale_l;
                out_r[i] += s_r * scale_r;
                scale_l += scale_delta_l;
                scale_r += scale_delta_r;
                frac += delta;
            }

            Simd128Finish();
            return frac;
        }

        template <unsigned Unroll>
        static uint64_t MixAndResampleStereoToStereo_Polyphase_Simd128(float* out[], const float* in_l, const float* in_r, uint32_t num, uint64_t frac, uint64_t delta, float scale_l0, float scale_r0, float scale_delta_l0, float scale_delta_r0, float scale_l1, float scale_r1, float scale_delta_l1, float scale_delta_r1)
        {
            float* out_l = out[0];
            float* out_r = out[1];
            DM_PROFILE("MixAndResampleStereoToStereo_Polyphase_SIMD");

            if (scale_r0 == 0.0f && scale_delta_r0 == 0.0f && scale_l1 == 0.0f && scale_delta_l1 == 0.0f)
            {
                uint64_t result = MixAndResampleStereoDiagonal_Simd128<Unroll>(out, in_l, in_r, num, frac, delta, scale_l0, scale_r1, scale_delta_l0, scale_delta_r1);
                Simd128Finish();
                return result;
            }

            dsimd_float4_t sc_l0[Unroll];
            dsimd_float4_t step_l0 = Simd128Splat(scale_delta_l0 * (4 * Unroll));
            dsimd_float4_t sc_r0[Unroll];
            dsimd_float4_t step_r0 = Simd128Splat(scale_delta_r0 * (4 * Unroll));
            dsimd_float4_t sc_l1[Unroll];
            dsimd_float4_t step_l1 = Simd128Splat(scale_delta_l1 * (4 * Unroll));
            dsimd_float4_t sc_r1[Unroll];
            dsimd_float4_t step_r1 = Simd128Splat(scale_delta_r1 * (4 * Unroll));
            for (unsigned u = 0; u < Unroll; ++u)
            {
                sc_l0[u] = Simd128Ramp(scale_l0 + (float)(u * 4) * scale_delta_l0, scale_delta_l0);
                sc_r0[u] = Simd128Ramp(scale_r0 + (float)(u * 4) * scale_delta_r0, scale_delta_r0);
                sc_l1[u] = Simd128Ramp(scale_l1 + (float)(u * 4) * scale_delta_l1, scale_delta_l1);
                sc_r1[u] = Simd128Ramp(scale_r1 + (float)(u * 4) * scale_delta_r1, scale_delta_r1);
            }

            const unsigned vector_num = num & ~(4 * Unroll - 1);
            unsigned i = 0;
            for (; i < vector_num; i += 4 * Unroll)
            {
                dsimd_float4_t samples_l[Unroll], samples_r[Unroll];
                ResampleStereo_Simd128<Unroll>(in_l, in_r, frac, delta, samples_l, samples_r);
                frac += delta * (4 * Unroll);
                for (unsigned u = 0; u < Unroll; ++u)
                {
                    Simd128Store((out_l + i) + u * 4, Simd128Add(Simd128Load((out_l + i) + u * 4), Simd128Add(Simd128Mul(samples_l[u], sc_l0[u]), Simd128Mul(samples_r[u], sc_l1[u]))));
                    Simd128Store((out_r + i) + u * 4, Simd128Add(Simd128Load((out_r + i) + u * 4), Simd128Add(Simd128Mul(samples_l[u], sc_r0[u]), Simd128Mul(samples_r[u], sc_r1[u]))));
                    sc_l0[u] = Simd128Add(sc_l0[u], step_l0);
                    sc_r0[u] = Simd128Add(sc_r0[u], step_r0);
                    sc_l1[u] = Simd128Add(sc_l1[u], step_l1);
                    sc_r1[u] = Simd128Add(sc_r1[u], step_r1);
                }
            }

            if (i == num)
            {
                Simd128Finish();
                return frac;
            }

            scale_l0 = Simd128Lane(sc_l0[0], 0);
            scale_r0 = Simd128Lane(sc_r0[0], 0);
            scale_l1 = Simd128Lane(sc_l1[0], 0);
            scale_r1 = Simd128Lane(sc_r1[0], 0);
            for (; i < num; ++i)
            {
                float s_l = FilterSampleFIR8_Simd128(in_l + (frac >> RESAMPLE_FRACTION_BITS), frac);
                float s_r = FilterSampleFIR8_Simd128(in_r + (frac >> RESAMPLE_FRACTION_BITS), frac);
                out_l[i] += s_l * scale_l0 + s_r * scale_l1;
                out_r[i] += s_l * scale_r0 + s_r * scale_r1;
                scale_l0 += scale_delta_l0;
                scale_r0 += scale_delta_r0;
                scale_l1 += scale_delta_l1;
                scale_r1 += scale_delta_r1;
                frac += delta;
            }

            Simd128Finish();
            return frac;
        }

        template <unsigned Unroll>
        static void ApplyClampedGain_Simd128(float* out[], float* in[], uint32_t num, float scale, float scale_delta)
        {
            float* in_l = in[0];
            float* in_r = in[1];
            float* out_l = out[0];
            float* out_r = out[1];
            dsimd_float4_t sc[Unroll];
            dsimd_float4_t step = Simd128Splat(scale_delta * (4 * Unroll));
            for (unsigned u = 0; u < Unroll; ++u)
            {
                sc[u] = Simd128Ramp(scale + (float)(u * 4) * scale_delta, scale_delta);
            }

            const unsigned vector_num = num & ~(4 * Unroll - 1);
            unsigned i = 0;
            for (; i < vector_num; i += 4 * Unroll)
            {
                for (unsigned u = 0; u < Unroll; ++u)
                {
                    dsimd_float4_t gain = Simd128Min(Simd128Max(sc[u], Simd128Splat(0)), Simd128Splat(1));
                    dsimd_float4_t s0 = Simd128Mul(Simd128Load((in_l + i) + u * 4), gain);
                    dsimd_float4_t s1 = Simd128Mul(Simd128Load((in_r + i) + u * 4), gain);
                    Simd128Store((out_l + i) + u * 4, Simd128Add(Simd128Load((out_l + i) + u * 4), s0));
                    Simd128Store((out_r + i) + u * 4, Simd128Add(Simd128Load((out_r + i) + u * 4), s1));
                    sc[u] = Simd128Add(sc[u], step);
                }
            }

            if (i == num)
            {
                Simd128Finish();
                return;
            }

            scale = Simd128Lane(sc[0], 0);
            float* remaining[] = { in_l + i, in_r + i };
            float* destination[] = { out_l + i, out_r + i };
            CPU::ApplyClampedGain(destination, remaining, num - i, scale, scale_delta);

            Simd128Finish();
        }

        template <unsigned Unroll>
        static inline void ApplyConstantGain_Simd128(float* out[], float* in[], uint32_t num, float scale)
        {
            float* out_l = out[0];
            float* out_r = out[1];
            float* in_l = in[0];
            float* in_r = in[1];
            dsimd_float4_t gain = Simd128Splat(scale);
            const unsigned vector_num = num & ~(4 * Unroll - 1);
            unsigned i = 0;
            for (; i < vector_num; i += 4 * Unroll)
            {
                for (unsigned u = 0; u < Unroll; ++u)
                {
                    dsimd_float4_t l = Simd128Load((in_l + i) + u * 4);
                    dsimd_float4_t r = Simd128Load((in_r + i) + u * 4);
                    Simd128Store((out_l + i) + u * 4, Simd128Mul(l, gain));
                    Simd128Store((out_r + i) + u * 4, Simd128Mul(r, gain));
                }
            }

            for (; i < num; ++i)
            {
                out_l[i] = in_l[i] * scale;
                out_r[i] = in_r[i] * scale;
            }
        }

        template <unsigned Unroll>
        static void ApplyGain_Simd128(float* out[], float* in[], uint32_t num, float scale, float scale_delta)
        {
            if (scale_delta == 0.0f)
            {
                ApplyConstantGain_Simd128<Unroll>(out, in, num, scale);
                Simd128Finish();
                return;
            }

            float* in_l = in[0];
            float* in_r = in[1];
            float* out_l = out[0];
            float* out_r = out[1];
            dsimd_float4_t sc[Unroll];
            dsimd_float4_t step = Simd128Splat(scale_delta * (4 * Unroll));
            for (unsigned u = 0; u < Unroll; ++u)
            {
                sc[u] = Simd128Ramp(scale + (float)(u * 4) * scale_delta, scale_delta);
            }

            const unsigned vector_num = num & ~(4 * Unroll - 1);
            unsigned i = 0;
            for (; i < vector_num; i += 4 * Unroll)
            {
                for (unsigned u = 0; u < Unroll; ++u)
                {
                    dsimd_float4_t gain = sc[u];
                    dsimd_float4_t s0 = Simd128Mul(Simd128Load((in_l + i) + u * 4), gain);
                    dsimd_float4_t s1 = Simd128Mul(Simd128Load((in_r + i) + u * 4), gain);
                    Simd128Store((out_l + i) + u * 4, s0);
                    Simd128Store((out_r + i) + u * 4, s1);
                    sc[u] = Simd128Add(sc[u], step);
                }
            }

            if (i == num)
            {
                Simd128Finish();
                return;
            }

            scale = Simd128Lane(sc[0], 0);
            float* remaining[] = { in_l + i, in_r + i };
            float* destination[] = { out_l + i, out_r + i };
            CPU::ApplyGain(destination, remaining, num - i, scale, scale_delta);

            Simd128Finish();
        }

        template <unsigned Unroll>
        static void ApplyGainAndInterleaveToS16_Simd128(int16_t* out, float* in[], uint32_t num, float scale, float scale_delta)
        {
            float* in_l = in[0];
            float* in_r = in[1];
            dsimd_float4_t sc[Unroll];
            dsimd_float4_t step = Simd128Splat(scale_delta * (4 * Unroll));
            for (unsigned u = 0; u < Unroll; ++u)
            {
                sc[u] = Simd128Ramp(scale + (float)(u * 4) * scale_delta, scale_delta);
            }

            const unsigned vector_num = num & ~(4 * Unroll - 1);
            unsigned i = 0;
            for (; i < vector_num; i += 4 * Unroll)
            {
                for (unsigned u = 0; u < Unroll; ++u)
                {
                    dsimd_float4_t gain = sc[u];
                    dsimd_float4_t s0 = Simd128Mul(Simd128Load((in_l + i) + u * 4), gain);
                    dsimd_float4_t s1 = Simd128Mul(Simd128Load((in_r + i) + u * 4), gain);
                    Simd128StoreInterleavedS16((out + 2 * i) + 2 * u * 4, s0, s1);
                    sc[u] = Simd128Add(sc[u], step);
                }
            }

            if (i == num)
            {
                Simd128Finish();
                return;
            }

            scale = Simd128Lane(sc[0], 0);
            float* remaining[] = { in_l + i, in_r + i };
            CPU::ApplyGainAndInterleaveToS16(out + 2 * i, remaining, num - i, scale, scale_delta);

            Simd128Finish();
        }

        template <unsigned Unroll>
        static void GatherPowerData_Simd128(float* in[], uint32_t num, float gain, float& sum_sq_left, float& sum_sq_right, float& max_sq_left, float& max_sq_right)
        {
            float* in_l = in[0];
            float* in_r = in[1];
            dsimd_float4_t sum_l[Unroll], sum_r[Unroll], max_l[Unroll], max_r[Unroll];
            for (unsigned u = 0; u < Unroll; ++u)
            {
                sum_l[u] = sum_r[u] = max_l[u] = max_r[u] = Simd128Splat(0);
            }

            dsimd_float4_t scale = Simd128Splat(gain);
            const unsigned vector_num = num & ~(4 * Unroll - 1);
            unsigned i = 0;
            for (; i < vector_num; i += 4 * Unroll)
            {
                for (unsigned u = 0; u < Unroll; ++u)
                {
                    dsimd_float4_t l = Simd128Mul(Simd128Load((in_l + i) + u * 4), scale);
                    dsimd_float4_t r = Simd128Mul(Simd128Load((in_r + i) + u * 4), scale);
                    l = Simd128Mul(l, l);
                    r = Simd128Mul(r, r);
                    sum_l[u] = Simd128Add(sum_l[u], l);
                    sum_r[u] = Simd128Add(sum_r[u], r);
                    max_l[u] = Simd128Max(max_l[u], l);
                    max_r[u] = Simd128Max(max_r[u], r);
                }
            }

            for (unsigned u = 1; u < Unroll; ++u)
            {
                sum_l[0] = Simd128Add(sum_l[0], sum_l[u]);
                sum_r[0] = Simd128Add(sum_r[0], sum_r[u]);
                max_l[0] = Simd128Max(max_l[0], max_l[u]);
                max_r[0] = Simd128Max(max_r[0], max_r[u]);
            }

            sum_sq_left = Simd128Sum(sum_l[0]);
            sum_sq_right = Simd128Sum(sum_r[0]);
            max_sq_left = Simd128ReduceMax(max_l[0]);
            max_sq_right = Simd128ReduceMax(max_r[0]);
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

            Simd128Finish();
        }

        template <unsigned Unroll>
        static void ConvertFromS16_Simd128(float* out, const int16_t* in, uint32_t num)
        {
            const unsigned vector_num = num & ~(4 * Unroll - 1);
            unsigned i = 0;
            for (; i < vector_num; i += 4 * Unroll)
            {
                for (unsigned u = 0; u < Unroll; ++u)
                {
                    Simd128Store((out + i) + u * 4, Simd128LoadS16ToFloat((in + i) + u * 4));
                }
            }

            CPU::ConvertFromS16(out + i, in + i, num - i);

            Simd128Finish();
        }

        template <unsigned Unroll>
        static void ConvertFromS8_Simd128(float* out, const int8_t* in, uint32_t num)
        {
            const unsigned vector_num = num & ~(4 * Unroll - 1);
            unsigned i = 0;
            for (; i < vector_num; i += 4 * Unroll)
            {
                if (Unroll >= 4)
                {
                    for (unsigned u = 0; u < Unroll; u += 4)
                    {
                        dsimd_byte16_t bytes = Simd128LoadBytes((in + i) + u * 4);
                        Simd128Store((out + i) + u * 4, Simd128ConvertS8ToS16Float(bytes, 0));
                        Simd128Store((out + i) + u * 4 + 4, Simd128ConvertS8ToS16Float(bytes, 1));
                        Simd128Store((out + i) + u * 4 + 8, Simd128ConvertS8ToS16Float(bytes, 2));
                        Simd128Store((out + i) + u * 4 + 12, Simd128ConvertS8ToS16Float(bytes, 3));
                    }
                }
                else
                {
                    for (unsigned u = 0; u < Unroll; ++u)
                    {
                        Simd128Store((out + i) + u * 4, Simd128LoadS8ToS16Float((in + i) + u * 4));
                    }
                }
            }

            CPU::ConvertFromS8(out + i, in + i, num - i);

            Simd128Finish();
        }

        template <unsigned Unroll>
        static void Deinterleave_Simd128(float* out[], const float* in, uint32_t num)
        {
            float* out_l = out[0];
            float* out_r = out[1];
            const unsigned vector_num = num & ~(4 * Unroll - 1);
            unsigned i = 0;
            for (; i < vector_num; i += 4 * Unroll)
            {
                for (unsigned u = 0; u < Unroll; ++u)
                {
                    dsimd_float4_t a = Simd128Load((in + 2 * i) + 2 * u * 4);
                    dsimd_float4_t b = Simd128Load((in + 2 * i) + 2 * u * 4 + 4);
                    dsimd_float4_t left, right;
                    Simd128Unzip(a, b, &left, &right);
                    Simd128Store((out_l + i) + u * 4, left);
                    Simd128Store((out_r + i) + u * 4, right);
                }
            }

            float* destination[] = { out_l + i, out_r + i };
            CPU::Deinterleave(destination, in + 2 * i, num - i);

            Simd128Finish();
        }

        template <unsigned Unroll>
        static void DeinterleaveFromS16_Simd128(float* out[], const int16_t* in, uint32_t num)
        {
            float* out_l = out[0];
            float* out_r = out[1];
            const unsigned vector_num = num & ~(4 * Unroll - 1);
            unsigned i = 0;
            for (; i < vector_num; i += 4 * Unroll)
            {
                for (unsigned u = 0; u < Unroll; ++u)
                {
                    dsimd_float4_t a = Simd128LoadS16ToFloat((in + 2 * i) + 2 * u * 4);
                    dsimd_float4_t b = Simd128LoadS16ToFloat((in + 2 * i) + 2 * u * 4 + 4);
                    dsimd_float4_t left, right;
                    Simd128Unzip(a, b, &left, &right);
                    Simd128Store((out_l + i) + u * 4, left);
                    Simd128Store((out_r + i) + u * 4, right);
                }
            }

            float* destination[] = { out_l + i, out_r + i };
            CPU::DeinterleaveFromS16(destination, in + 2 * i, num - i);

            Simd128Finish();
        }

        template <unsigned Unroll>
        static void DeinterleaveFromS8_Simd128(float* out[], const int8_t* in, uint32_t num)
        {
            float* out_l = out[0];
            float* out_r = out[1];
            const unsigned vector_num = num & ~(4 * Unroll - 1);
            unsigned i = 0;
            for (; i < vector_num; i += 4 * Unroll)
            {
                if (Unroll >= 2)
                {
                    for (unsigned u = 0; u < Unroll; u += 2)
                    {
                        dsimd_byte16_t bytes = Simd128LoadBytes((in + 2 * i) + u * 8);
                        dsimd_float4_t left, right;
                        Simd128Unzip(Simd128ConvertS8ToS16Float(bytes, 0), Simd128ConvertS8ToS16Float(bytes, 1), &left, &right);
                        Simd128Store((out_l + i) + u * 4, left);
                        Simd128Store((out_r + i) + u * 4, right);
                        Simd128Unzip(Simd128ConvertS8ToS16Float(bytes, 2), Simd128ConvertS8ToS16Float(bytes, 3), &left, &right);
                        Simd128Store((out_l + i) + u * 4 + 4, left);
                        Simd128Store((out_r + i) + u * 4 + 4, right);
                    }
                }
                else
                {
                    for (unsigned u = 0; u < Unroll; ++u)
                    {
                        dsimd_float4_t a = Simd128LoadS8ToS16Float((in + 2 * i) + 2 * u * 4);
                        dsimd_float4_t b = Simd128LoadS8ToS16Float((in + 2 * i) + 2 * u * 4 + 4);
                        dsimd_float4_t left, right;
                        Simd128Unzip(a, b, &left, &right);
                        Simd128Store((out_l + i) + u * 4, left);
                        Simd128Store((out_r + i) + u * 4, right);
                    }
                }
            }

            float* destination[] = { out_l + i, out_r + i };
            CPU::DeinterleaveFromS8(destination, in + 2 * i, num - i);

            Simd128Finish();
        }

    } // namespace Dsp

#if !defined(DM_TEST_SOUND_DSP)
    // Per-operation unroll counts from offline measurements; every vector has four lanes.
#if defined(DM_SIMD_WASM)
    static const unsigned UNROLL = 4;
    static const unsigned UNROLL_MIX_STEREO = 2;
    static const unsigned UNROLL_RESAMPLE_MONO = 2;
    static const unsigned UNROLL_RESAMPLE_STEREO = 1;
    static const unsigned UNROLL_CLAMPED_GAIN = 4;
    static const unsigned UNROLL_GAIN_INTERLEAVE = 2;
    static const unsigned UNROLL_POWER = 2;
#elif defined(DM_SIMD_NEON)
    static const unsigned UNROLL = 2;
    static const unsigned UNROLL_MIX_STEREO = 2;
    static const unsigned UNROLL_RESAMPLE_MONO = 2;
    static const unsigned UNROLL_RESAMPLE_STEREO = 1;
    static const unsigned UNROLL_CLAMPED_GAIN = 4;
    static const unsigned UNROLL_GAIN_INTERLEAVE = 2;
    static const unsigned UNROLL_POWER = 2;
#else
    static const unsigned UNROLL = 1;
    static const unsigned UNROLL_MIX_STEREO = 1;
    static const unsigned UNROLL_RESAMPLE_MONO = 1;
    static const unsigned UNROLL_RESAMPLE_STEREO = 1;
    static const unsigned UNROLL_CLAMPED_GAIN = 1;
    static const unsigned UNROLL_GAIN_INTERLEAVE = 1;
    static const unsigned UNROLL_POWER = 1;
#endif

    extern const DSPImpl simd128_impl =
    {
        4,
        Dsp::MixScaledMonoToStereo_Simd128<UNROLL>,
        Dsp::MixScaledStereoToStereo_Simd128<UNROLL_MIX_STEREO>,
        Dsp::MixAndResampleMonoToStereo_Polyphase_Simd128<UNROLL_RESAMPLE_MONO>,
        Dsp::MixAndResampleStereoToStereo_Polyphase_Simd128<UNROLL_RESAMPLE_STEREO>,
        Dsp::ApplyClampedGain_Simd128<UNROLL_CLAMPED_GAIN>,
        Dsp::ApplyGainAndInterleaveToS16_Simd128<UNROLL_GAIN_INTERLEAVE>,
        Dsp::ApplyGain_Simd128<UNROLL>,
        Dsp::GatherPowerData_Simd128<UNROLL_POWER>,
        Dsp::ConvertFromS16_Simd128<UNROLL>,
        Dsp::ConvertFromS8_Simd128<UNROLL>,
        Dsp::Deinterleave_Simd128<UNROLL>,
        Dsp::DeinterleaveFromS16_Simd128<UNROLL>,
        Dsp::DeinterleaveFromS8_Simd128<UNROLL>,
    };

#endif

} // namespace dmSound
