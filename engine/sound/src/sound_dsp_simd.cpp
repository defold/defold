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

#include "sound_dsp.h"

// Included once per isolated kernel. SIMD register types never leave this file.
#include <simd/simd_fixed_float.h>

namespace dmSound
{
    namespace DSPKernel
    {

#if DM_SOUND_DSP_LANES == 4
        typedef dsimd_float4_t Vec;
#define DSP_SIMD(name) Simd##name
#elif DM_SOUND_DSP_LANES == 8
        typedef dsimd_float8_t Vec;
#define DSP_SIMD(name) Simd8##name
#elif DM_SOUND_DSP_LANES == 16
        typedef dsimd_float16_t Vec;
#define DSP_SIMD(name) Simd16##name
#endif

        static inline Vec Ramp(float start, float step)
        {
#if DM_SOUND_DSP_LANES == 4
            return SimdAdd(SimdSplat(start), SimdMul(SimdSplat(step), SimdMake(0, 1, 2, 3)));
#else
            return DSP_SIMD(Ramp)(start, step);
#endif
        }

        static inline void Unzip(Vec a, Vec b, Vec* left, Vec* right)
        {
#if DM_SOUND_DSP_LANES == 4
            *left = SimdShuffle2(a, b, 0, 2, 4, 6);
            *right = SimdShuffle2(a, b, 1, 3, 5, 7);
#else
            DSP_SIMD(Unzip)(a, b, left, right);
#endif
        }

        static inline float FilterSampleFIR8(const float* frames, uint64_t frac_pos)
        {
            uint32_t pi = ((frac_pos >> (RESAMPLE_FRACTION_BITS - 11)) << 3) & (2047 << 3);
            return Simd8Sum(Simd8Mul(Simd8Load(frames - 3), Simd8Load(&_pfb[pi])));
        }

#if defined(_MSC_VER) && !defined(__clang__)
        static inline dsimd_float4_t Resample4(const float* in, uint64_t frac, uint64_t delta)
        {
            // Explicit construction keeps distinct FIR results in every lane on MSVC.
            float s0 = FilterSampleFIR8(in + (frac >> RESAMPLE_FRACTION_BITS), frac);
            frac += delta;
            float s1 = FilterSampleFIR8(in + (frac >> RESAMPLE_FRACTION_BITS), frac);
            frac += delta;
            float s2 = FilterSampleFIR8(in + (frac >> RESAMPLE_FRACTION_BITS), frac);
            frac += delta;
            float s3 = FilterSampleFIR8(in + (frac >> RESAMPLE_FRACTION_BITS), frac);
            return SimdMake(s0, s1, s2, s3);
        }

#endif

        static inline Vec Resample(const float* in, uint64_t frac, uint64_t delta)
        {
#if defined(_MSC_VER) && !defined(__clang__)
#if DM_SOUND_DSP_LANES == 4
            return Resample4(in, frac, delta);
#elif DM_SOUND_DSP_LANES == 8
            return Simd8Combine(Resample4(in, frac, delta), Resample4(in, frac + delta * 4, delta));
#else
            return Simd16Combine(Resample4(in, frac, delta), Resample4(in, frac + delta * 4, delta),
                                 Resample4(in, frac + delta * 8, delta), Resample4(in, frac + delta * 12, delta));
#endif
#else
            // LLVM schedules the independent FIR samples faster from this loop.
            float samples[DM_SOUND_DSP_LANES];
            for (unsigned i = 0; i < DM_SOUND_DSP_LANES; ++i)
            {
                samples[i] = FilterSampleFIR8(in + (frac >> RESAMPLE_FRACTION_BITS), frac);
                frac += delta;
            }

            return DSP_SIMD(Load)(samples);
#endif
        }

        // Both channels use the same phase and coefficients. Preserve the FIR reduction order.
        static inline void FilterStereoFIR8(const float* in_l, const float* in_r, uint64_t frac, float* sample_l, float* sample_r)
        {
            uint32_t pi = ((frac >> (RESAMPLE_FRACTION_BITS - 11)) << 3) & (2047 << 3);
            uint64_t index = frac >> RESAMPLE_FRACTION_BITS;
#if defined(DM_SIMD_NEON_H)
            dsimd_float4_t c0 = SimdLoad(&_pfb[pi]);
            dsimd_float4_t c1 = SimdLoad(&_pfb[pi + 4]);
            dsimd_float4_t l = SimdAdd(SimdMul(SimdLoad(in_l + index - 3), c0), SimdMul(SimdLoad(in_l + index + 1), c1));
            dsimd_float4_t r = SimdAdd(SimdMul(SimdLoad(in_r + index - 3), c0), SimdMul(SimdLoad(in_r + index + 1), c1));
            dsimd_float4_t pairs = SimdPairwiseAdd(l, r);
            dsimd_float4_t sums = SimdPairwiseAdd(pairs, pairs);
            *sample_l = SimdLane(sums, 0);
            *sample_r = SimdLane(sums, 1);
#else
            dsimd_float8_t coefficients = Simd8Load(&_pfb[pi]);
            *sample_l = Simd8Sum(Simd8Mul(Simd8Load(in_l + index - 3), coefficients));
            *sample_r = Simd8Sum(Simd8Mul(Simd8Load(in_r + index - 3), coefficients));
#endif
        }

#if defined(_MSC_VER) && !defined(__clang__)
        // MSVC needs explicit construction to retain distinct FIR results in every lane.
        static inline void ResampleStereo4(const float* in_l, const float* in_r, uint64_t frac, uint64_t delta, dsimd_float4_t* left, dsimd_float4_t* right)
        {
            float l0, r0, l1, r1, l2, r2, l3, r3;
            FilterStereoFIR8(in_l, in_r, frac, &l0, &r0);
            FilterStereoFIR8(in_l, in_r, frac + delta, &l1, &r1);
            FilterStereoFIR8(in_l, in_r, frac + delta * 2, &l2, &r2);
            FilterStereoFIR8(in_l, in_r, frac + delta * 3, &l3, &r3);
            *left = SimdMake(l0, l1, l2, l3);
            *right = SimdMake(r0, r1, r2, r3);
        }

#endif

        static inline void ResampleStereo(const float* in_l, const float* in_r, uint64_t frac, uint64_t delta, Vec* left, Vec* right)
        {
#if defined(DM_SIMD_X86_H) && !defined(DM_SIMD_AVX2)
            // Narrow x86 kernels retain the faster independent FIR schedule.
            *left = Resample(in_l, frac, delta);
            *right = Resample(in_r, frac, delta);
#elif defined(_MSC_VER) && !defined(__clang__)
            dsimd_float4_t l0, r0;
            ResampleStereo4(in_l, in_r, frac, delta, &l0, &r0);
#if DM_SOUND_DSP_LANES == 4
            *left = l0;
            *right = r0;
#else
            dsimd_float4_t l1, r1;
            ResampleStereo4(in_l, in_r, frac + delta * 4, delta, &l1, &r1);
#if DM_SOUND_DSP_LANES == 8
            *left = Simd8Combine(l0, l1);
            *right = Simd8Combine(r0, r1);
#else
            dsimd_float4_t l2, r2, l3, r3;
            ResampleStereo4(in_l, in_r, frac + delta * 8, delta, &l2, &r2);
            ResampleStereo4(in_l, in_r, frac + delta * 12, delta, &l3, &r3);
            *left = Simd16Combine(l0, l1, l2, l3);
            *right = Simd16Combine(r0, r1, r2, r3);
#endif
#endif
#else
            float samples_l[DM_SOUND_DSP_LANES], samples_r[DM_SOUND_DSP_LANES];
            for (unsigned i = 0; i < DM_SOUND_DSP_LANES; ++i)
            {
                FilterStereoFIR8(in_l, in_r, frac, &samples_l[i], &samples_r[i]);
                frac += delta;
            }

            *left = DSP_SIMD(Load)(samples_l);
            *right = DSP_SIMD(Load)(samples_r);
#endif
        }

        static inline void MixScaledMonoToStereo(float* out[], const float* in, uint32_t num, float scale_l, float scale_r, float scale_delta_l, float scale_delta_r)
        {
            Vec sc_l = Ramp(scale_l, scale_delta_l);
            Vec step_l = DSP_SIMD(Splat)(scale_delta_l * DM_SOUND_DSP_LANES);
            Vec sc_r = Ramp(scale_r, scale_delta_r);
            Vec step_r = DSP_SIMD(Splat)(scale_delta_r * DM_SOUND_DSP_LANES);
            unsigned i = 0;
            for (; num - i >= DM_SOUND_DSP_LANES; i += DM_SOUND_DSP_LANES)
            {
                Vec sample = DSP_SIMD(Load)(in + i);
                DSP_SIMD(Store)(out[0] + i, DSP_SIMD(Add)(DSP_SIMD(Load)(out[0] + i), DSP_SIMD(Mul)(sample, sc_l)));
                DSP_SIMD(Store)(out[1] + i, DSP_SIMD(Add)(DSP_SIMD(Load)(out[1] + i), DSP_SIMD(Mul)(sample, sc_r)));
                sc_l = DSP_SIMD(Add)(sc_l, step_l);
                sc_r = DSP_SIMD(Add)(sc_r, step_r);
            }

            scale_l = DSP_SIMD(Lane)(sc_l, 0);
            scale_r = DSP_SIMD(Lane)(sc_r, 0);
            for (; i < num; ++i)
            {
                float sample = in[i];
                out[0][i] += sample * scale_l;
                out[1][i] += sample * scale_r;
                scale_l += scale_delta_l;
                scale_r += scale_delta_r;
            }
        }

        static inline uint64_t MixAndResampleMonoToStereo_Polyphase(float* out[], const float* in, uint32_t num, uint64_t frac, uint64_t delta, float scale_l, float scale_r, float scale_delta_l, float scale_delta_r)
        {
            DM_PROFILE("MixAndResampleMonoToStereo_Polyphase_SIMD");

            Vec sc_l = Ramp(scale_l, scale_delta_l);
            Vec step_l = DSP_SIMD(Splat)(scale_delta_l * DM_SOUND_DSP_LANES);
            Vec sc_r = Ramp(scale_r, scale_delta_r);
            Vec step_r = DSP_SIMD(Splat)(scale_delta_r * DM_SOUND_DSP_LANES);
            unsigned i = 0;
            for (; num - i >= DM_SOUND_DSP_LANES; i += DM_SOUND_DSP_LANES)
            {
                Vec sample = Resample(in, frac, delta);
                frac += delta * DM_SOUND_DSP_LANES;
                DSP_SIMD(Store)(out[0] + i, DSP_SIMD(Add)(DSP_SIMD(Load)(out[0] + i), DSP_SIMD(Mul)(sample, sc_l)));
                DSP_SIMD(Store)(out[1] + i, DSP_SIMD(Add)(DSP_SIMD(Load)(out[1] + i), DSP_SIMD(Mul)(sample, sc_r)));
                sc_l = DSP_SIMD(Add)(sc_l, step_l);
                sc_r = DSP_SIMD(Add)(sc_r, step_r);
            }

            scale_l = DSP_SIMD(Lane)(sc_l, 0);
            scale_r = DSP_SIMD(Lane)(sc_r, 0);
            for (; i < num; ++i)
            {
                float sample = FilterSampleFIR8(in + (frac >> RESAMPLE_FRACTION_BITS), frac);
                out[0][i] += sample * scale_l;
                out[1][i] += sample * scale_r;
                scale_l += scale_delta_l;
                scale_r += scale_delta_r;
                frac += delta;
            }

            return frac;
        }

        static inline void MixScaledStereoToStereo(float* out[], const float* in_l, const float* in_r, uint32_t num, float scale_l0, float scale_r0, float scale_delta_l0, float scale_delta_r0, float scale_l1, float scale_r1, float scale_delta_l1, float scale_delta_r1)
        {
            Vec sc_l0 = Ramp(scale_l0, scale_delta_l0);
            Vec step_l0 = DSP_SIMD(Splat)(scale_delta_l0 * DM_SOUND_DSP_LANES);
            Vec sc_r0 = Ramp(scale_r0, scale_delta_r0);
            Vec step_r0 = DSP_SIMD(Splat)(scale_delta_r0 * DM_SOUND_DSP_LANES);
            Vec sc_l1 = Ramp(scale_l1, scale_delta_l1);
            Vec step_l1 = DSP_SIMD(Splat)(scale_delta_l1 * DM_SOUND_DSP_LANES);
            Vec sc_r1 = Ramp(scale_r1, scale_delta_r1);
            Vec step_r1 = DSP_SIMD(Splat)(scale_delta_r1 * DM_SOUND_DSP_LANES);
            unsigned i = 0;
            for (; num - i >= DM_SOUND_DSP_LANES; i += DM_SOUND_DSP_LANES)
            {
                Vec s_l = DSP_SIMD(Load)(in_l + i);
                Vec s_r = DSP_SIMD(Load)(in_r + i);
                DSP_SIMD(Store)(out[0] + i, DSP_SIMD(Add)(DSP_SIMD(Load)(out[0] + i), DSP_SIMD(Add)(DSP_SIMD(Mul)(s_l, sc_l0), DSP_SIMD(Mul)(s_r, sc_l1))));
                DSP_SIMD(Store)(out[1] + i, DSP_SIMD(Add)(DSP_SIMD(Load)(out[1] + i), DSP_SIMD(Add)(DSP_SIMD(Mul)(s_l, sc_r0), DSP_SIMD(Mul)(s_r, sc_r1))));
                sc_l0 = DSP_SIMD(Add)(sc_l0, step_l0);
                sc_r0 = DSP_SIMD(Add)(sc_r0, step_r0);
                sc_l1 = DSP_SIMD(Add)(sc_l1, step_l1);
                sc_r1 = DSP_SIMD(Add)(sc_r1, step_r1);
            }

            scale_l0 = DSP_SIMD(Lane)(sc_l0, 0);
            scale_r0 = DSP_SIMD(Lane)(sc_r0, 0);
            scale_l1 = DSP_SIMD(Lane)(sc_l1, 0);
            scale_r1 = DSP_SIMD(Lane)(sc_r1, 0);
            for (; i < num; ++i)
            {
                float s_l = in_l[i];
                float s_r = in_r[i];
                out[0][i] += s_l * scale_l0 + s_r * scale_l1;
                out[1][i] += s_l * scale_r0 + s_r * scale_r1;
                scale_l0 += scale_delta_l0;
                scale_r0 += scale_delta_r0;
                scale_l1 += scale_delta_l1;
                scale_r1 += scale_delta_r1;
            }
        }

        static inline uint64_t MixAndResampleStereoDiagonal(float* out[], const float* in_l, const float* in_r, uint32_t num, uint64_t frac, uint64_t delta, float scale_l, float scale_r, float scale_delta_l, float scale_delta_r)
        {
            Vec sc_l = Ramp(scale_l, scale_delta_l);
            Vec sc_r = Ramp(scale_r, scale_delta_r);
            Vec step_l = DSP_SIMD(Splat)(scale_delta_l * DM_SOUND_DSP_LANES);
            Vec step_r = DSP_SIMD(Splat)(scale_delta_r * DM_SOUND_DSP_LANES);
            unsigned i = 0;
            for (; num - i >= DM_SOUND_DSP_LANES; i += DM_SOUND_DSP_LANES)
            {
                Vec s_l, s_r;
                ResampleStereo(in_l, in_r, frac, delta, &s_l, &s_r);
                frac += delta * DM_SOUND_DSP_LANES;
                DSP_SIMD(Store)(out[0] + i, DSP_SIMD(Add)(DSP_SIMD(Load)(out[0] + i), DSP_SIMD(Mul)(s_l, sc_l)));
                DSP_SIMD(Store)(out[1] + i, DSP_SIMD(Add)(DSP_SIMD(Load)(out[1] + i), DSP_SIMD(Mul)(s_r, sc_r)));
                sc_l = DSP_SIMD(Add)(sc_l, step_l);
                sc_r = DSP_SIMD(Add)(sc_r, step_r);
            }

            if (i == num)
            {
                return frac;
            }

            scale_l = DSP_SIMD(Lane)(sc_l, 0);
            scale_r = DSP_SIMD(Lane)(sc_r, 0);
            for (; i < num; ++i)
            {
                float s_l, s_r;
                FilterStereoFIR8(in_l, in_r, frac, &s_l, &s_r);
                out[0][i] += s_l * scale_l;
                out[1][i] += s_r * scale_r;
                scale_l += scale_delta_l;
                scale_r += scale_delta_r;
                frac += delta;
            }

            return frac;
        }

        static inline uint64_t MixAndResampleStereoToStereo_Polyphase(float* out[], const float* in_l, const float* in_r, uint32_t num, uint64_t frac, uint64_t delta, float scale_l0, float scale_r0, float scale_delta_l0, float scale_delta_r0, float scale_l1, float scale_r1, float scale_delta_l1, float scale_delta_r1)
        {
            DM_PROFILE("MixAndResampleStereoToStereo_Polyphase_SIMD");

            if (scale_r0 == 0.0f && scale_delta_r0 == 0.0f && scale_l1 == 0.0f && scale_delta_l1 == 0.0f)
            {
                return MixAndResampleStereoDiagonal(out, in_l, in_r, num, frac, delta, scale_l0, scale_r1, scale_delta_l0, scale_delta_r1);
            }

            Vec sc_l0 = Ramp(scale_l0, scale_delta_l0);
            Vec step_l0 = DSP_SIMD(Splat)(scale_delta_l0 * DM_SOUND_DSP_LANES);
            Vec sc_r0 = Ramp(scale_r0, scale_delta_r0);
            Vec step_r0 = DSP_SIMD(Splat)(scale_delta_r0 * DM_SOUND_DSP_LANES);
            Vec sc_l1 = Ramp(scale_l1, scale_delta_l1);
            Vec step_l1 = DSP_SIMD(Splat)(scale_delta_l1 * DM_SOUND_DSP_LANES);
            Vec sc_r1 = Ramp(scale_r1, scale_delta_r1);
            Vec step_r1 = DSP_SIMD(Splat)(scale_delta_r1 * DM_SOUND_DSP_LANES);
            unsigned i = 0;
            for (; num - i >= DM_SOUND_DSP_LANES; i += DM_SOUND_DSP_LANES)
            {
                Vec s_l, s_r;
                ResampleStereo(in_l, in_r, frac, delta, &s_l, &s_r);
                frac += delta * DM_SOUND_DSP_LANES;
                DSP_SIMD(Store)(out[0] + i, DSP_SIMD(Add)(DSP_SIMD(Load)(out[0] + i), DSP_SIMD(Add)(DSP_SIMD(Mul)(s_l, sc_l0), DSP_SIMD(Mul)(s_r, sc_l1))));
                DSP_SIMD(Store)(out[1] + i, DSP_SIMD(Add)(DSP_SIMD(Load)(out[1] + i), DSP_SIMD(Add)(DSP_SIMD(Mul)(s_l, sc_r0), DSP_SIMD(Mul)(s_r, sc_r1))));
                sc_l0 = DSP_SIMD(Add)(sc_l0, step_l0);
                sc_r0 = DSP_SIMD(Add)(sc_r0, step_r0);
                sc_l1 = DSP_SIMD(Add)(sc_l1, step_l1);
                sc_r1 = DSP_SIMD(Add)(sc_r1, step_r1);
            }

            if (i == num)
            {
                return frac;
            }

            scale_l0 = DSP_SIMD(Lane)(sc_l0, 0);
            scale_r0 = DSP_SIMD(Lane)(sc_r0, 0);
            scale_l1 = DSP_SIMD(Lane)(sc_l1, 0);
            scale_r1 = DSP_SIMD(Lane)(sc_r1, 0);
            for (; i < num; ++i)
            {
                float s_l = FilterSampleFIR8(in_l + (frac >> RESAMPLE_FRACTION_BITS), frac);
                float s_r = FilterSampleFIR8(in_r + (frac >> RESAMPLE_FRACTION_BITS), frac);
                out[0][i] += s_l * scale_l0 + s_r * scale_l1;
                out[1][i] += s_l * scale_r0 + s_r * scale_r1;
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
            Vec sc = Ramp(scale, scale_delta);
            Vec step = DSP_SIMD(Splat)(scale_delta * DM_SOUND_DSP_LANES);
            unsigned i = 0;
            for (; num - i >= DM_SOUND_DSP_LANES; i += DM_SOUND_DSP_LANES)
            {
                Vec gain = DSP_SIMD(Min)(DSP_SIMD(Max)(sc, DSP_SIMD(Splat)(0)), DSP_SIMD(Splat)(1));
                Vec s0 = DSP_SIMD(Mul)(DSP_SIMD(Load)(in[0] + i), gain);
                Vec s1 = DSP_SIMD(Mul)(DSP_SIMD(Load)(in[1] + i), gain);
                DSP_SIMD(Store)(out[0] + i, DSP_SIMD(Add)(DSP_SIMD(Load)(out[0] + i), s0));
                DSP_SIMD(Store)(out[1] + i, DSP_SIMD(Add)(DSP_SIMD(Load)(out[1] + i), s1));
                sc = DSP_SIMD(Add)(sc, step);
            }

            scale = DSP_SIMD(Lane)(sc, 0);
            float* remaining[] = { in[0] + i, in[1] + i };
            float* destination[] = { out[0] + i, out[1] + i };
            Fallback::ApplyClampedGain(destination, remaining, num - i, scale, scale_delta);
        }

        static inline void ApplyGain(float* out[], float* in[], uint32_t num, float scale, float scale_delta)
        {
            float* in_l = in[0];
            float* in_r = in[1];
            float* out_l = out[0];
            float* out_r = out[1];
            Vec sc = Ramp(scale, scale_delta);
            Vec step = DSP_SIMD(Splat)(scale_delta * DM_SOUND_DSP_LANES);
            unsigned i = 0;
            for (; num - i >= DM_SOUND_DSP_LANES; i += DM_SOUND_DSP_LANES)
            {
                Vec gain = sc;
                Vec s0 = DSP_SIMD(Mul)(DSP_SIMD(Load)(in_l + i), gain);
                Vec s1 = DSP_SIMD(Mul)(DSP_SIMD(Load)(in_r + i), gain);
                DSP_SIMD(Store)(out_l + i, s0);
                DSP_SIMD(Store)(out_r + i, s1);
                sc = DSP_SIMD(Add)(sc, step);
            }

            if (i == num)
            {
                return;
            }

            scale = DSP_SIMD(Lane)(sc, 0);
            float* remaining[] = { in_l + i, in_r + i };
            float* destination[] = { out_l + i, out_r + i };
            Fallback::ApplyGain(destination, remaining, num - i, scale, scale_delta);
        }

        static inline void ApplyGainAndInterleaveToS16(int16_t* out, float* in[], uint32_t num, float scale, float scale_delta)
        {
            Vec sc = Ramp(scale, scale_delta);
            Vec step = DSP_SIMD(Splat)(scale_delta * DM_SOUND_DSP_LANES);
            unsigned i = 0;
            for (; num - i >= DM_SOUND_DSP_LANES; i += DM_SOUND_DSP_LANES)
            {
                Vec gain = sc;
                Vec s0 = DSP_SIMD(Mul)(DSP_SIMD(Load)(in[0] + i), gain);
                Vec s1 = DSP_SIMD(Mul)(DSP_SIMD(Load)(in[1] + i), gain);
                DSP_SIMD(StoreInterleavedS16)(out + 2 * i, s0, s1);
                sc = DSP_SIMD(Add)(sc, step);
            }

            scale = DSP_SIMD(Lane)(sc, 0);
            float* remaining[] = { in[0] + i, in[1] + i };
            Fallback::ApplyGainAndInterleaveToS16(out + 2 * i, remaining, num - i, scale, scale_delta);
        }

        static inline void GatherPowerData(float* in[], uint32_t num, float gain, float& sum_sq_left, float& sum_sq_right, float& max_sq_left, float& max_sq_right)
        {
            Vec sum_l = DSP_SIMD(Splat)(0), sum_r = sum_l;
            Vec max_l = sum_l, max_r = sum_l;
            Vec scale = DSP_SIMD(Splat)(gain);
            unsigned i = 0;
            for (; num - i >= DM_SOUND_DSP_LANES; i += DM_SOUND_DSP_LANES)
            {
                Vec l = DSP_SIMD(Mul)(DSP_SIMD(Load)(in[0] + i), scale);
                Vec r = DSP_SIMD(Mul)(DSP_SIMD(Load)(in[1] + i), scale);
                l = DSP_SIMD(Mul)(l, l);
                r = DSP_SIMD(Mul)(r, r);
                sum_l = DSP_SIMD(Add)(sum_l, l);
                sum_r = DSP_SIMD(Add)(sum_r, r);
                max_l = DSP_SIMD(Max)(max_l, l);
                max_r = DSP_SIMD(Max)(max_r, r);
            }

            sum_sq_left = DSP_SIMD(Sum)(sum_l);
            sum_sq_right = DSP_SIMD(Sum)(sum_r);
            max_sq_left = DSP_SIMD(ReduceMax)(max_l);
            max_sq_right = DSP_SIMD(ReduceMax)(max_r);
            for (; i < num; ++i)
            {
                float l = in[0][i] * gain, r = in[1][i] * gain;
                l *= l;
                r *= r;
                sum_sq_left += l;
                sum_sq_right += r;
                max_sq_left = dmMath::Max(max_sq_left, l);
                max_sq_right = dmMath::Max(max_sq_right, r);
            }
        }

        static inline void ConvertFromS16(float* out, const int16_t* in, uint32_t num)
        {
            unsigned i = 0;
            for (; num - i >= DM_SOUND_DSP_LANES; i += DM_SOUND_DSP_LANES)
            {
                DSP_SIMD(Store)(out + i, DSP_SIMD(LoadS16ToFloat)(in + i));
            }

            Fallback::ConvertFromS16(out + i, in + i, num - i);
        }

        static inline void ConvertFromS8(float* out, const int8_t* in, uint32_t num)
        {
            unsigned i = 0;
            for (; num - i >= DM_SOUND_DSP_LANES; i += DM_SOUND_DSP_LANES)
            {
                DSP_SIMD(Store)(out + i, DSP_SIMD(LoadS8ToS16Float)(in + i));
            }

            Fallback::ConvertFromS8(out + i, in + i, num - i);
        }

        static inline void Deinterleave(float* out[], const float* in, uint32_t num)
        {
            unsigned i = 0;
            for (; num - i >= DM_SOUND_DSP_LANES; i += DM_SOUND_DSP_LANES)
            {
                Vec a = DSP_SIMD(Load)(in + 2 * i);
                Vec b = DSP_SIMD(Load)(in + 2 * i + DM_SOUND_DSP_LANES);
                Vec left, right;
                Unzip(a, b, &left, &right);
                DSP_SIMD(Store)(out[0] + i, left);
                DSP_SIMD(Store)(out[1] + i, right);
            }

            float* destination[] = { out[0] + i, out[1] + i };
            Fallback::Deinterleave(destination, in + 2 * i, num - i);
        }

        static inline void DeinterleaveFromS16(float* out[], const int16_t* in, uint32_t num)
        {
            unsigned i = 0;
            for (; num - i >= DM_SOUND_DSP_LANES; i += DM_SOUND_DSP_LANES)
            {
                Vec a = DSP_SIMD(LoadS16ToFloat)(in + 2 * i);
                Vec b = DSP_SIMD(LoadS16ToFloat)(in + 2 * i + DM_SOUND_DSP_LANES);
                Vec left, right;
                Unzip(a, b, &left, &right);
                DSP_SIMD(Store)(out[0] + i, left);
                DSP_SIMD(Store)(out[1] + i, right);
            }

            float* destination[] = { out[0] + i, out[1] + i };
            Fallback::DeinterleaveFromS16(destination, in + 2 * i, num - i);
        }

        static inline void DeinterleaveFromS8(float* out[], const int8_t* in, uint32_t num)
        {
            unsigned i = 0;
            for (; num - i >= DM_SOUND_DSP_LANES; i += DM_SOUND_DSP_LANES)
            {
                Vec a = DSP_SIMD(LoadS8ToS16Float)(in + 2 * i);
                Vec b = DSP_SIMD(LoadS8ToS16Float)(in + 2 * i + DM_SOUND_DSP_LANES);
                Vec left, right;
                Unzip(a, b, &left, &right);
                DSP_SIMD(Store)(out[0] + i, left);
                DSP_SIMD(Store)(out[1] + i, right);
            }

            float* destination[] = { out[0] + i, out[1] + i };
            Fallback::DeinterleaveFromS8(destination, in + 2 * i, num - i);
        }

#undef DSP_SIMD
    } // namespace DSPKernel

    const DSPImpl* DM_SOUND_DSP_GETTER()
    {
        static const DSPImpl impl =
        {
            DM_SOUND_DSP_NAME,
            DM_SOUND_DSP_LANES,
            DSPKernel::MixScaledMonoToStereo,
            DSPKernel::MixScaledStereoToStereo,
            DSPKernel::MixAndResampleMonoToStereo_Polyphase,
            DSPKernel::MixAndResampleStereoToStereo_Polyphase,
            DSPKernel::ApplyClampedGain,
            DSPKernel::ApplyGainAndInterleaveToS16,
            DSPKernel::ApplyGain,
            DSPKernel::GatherPowerData,
            DSPKernel::ConvertFromS16,
            DSPKernel::ConvertFromS8,
            DSPKernel::Deinterleave,
            DSPKernel::DeinterleaveFromS16,
            DSPKernel::DeinterleaveFromS8,
        };
        return &impl;
    }

} // namespace dmSound
