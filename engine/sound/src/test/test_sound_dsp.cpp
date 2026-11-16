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

#include "../sound_private.h"
#include "../sound_dsp.h"
#include "test_sound_dsp_expected.h"
#include <simd/simd_runtime.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

namespace dmSound
{
    const DSPImpl* GetDSPImplTest4();
    const DSPImpl* GetDSPImplTest8();
    const DSPImpl* GetDSPImplTest16();
}

static unsigned g_Checks;
static unsigned g_Failures;

#define CHECK(expression) do { ++g_Checks; if (!(expression)) { if (g_Failures++ < 10) printf("%s:%d: %s\n", __FILE__, __LINE__, #expression); } } while (0)

static uint64_t Run(const dmSound::DSPImpl* impl, unsigned op, float* out[], float* in[], const int16_t* pcm16, const int8_t* pcm8, int16_t* pcm_out, unsigned num, uint64_t delta, float* power)
{
    const float step = 1.0f / 1024.0f;
    const uint64_t frac = 1023;
    switch (op)
    {
        case 0:
        {
            impl->MixScaledMonoToStereo(out, in[0], num, 0.5f, -0.25f, step, -step);
            break;
        }
        case 1:
        {
            impl->MixScaledStereoToStereo(out, in[0], in[1], num, 0.5f, -0.25f, step, -step, 0.25f, 0.5f, -step, step);
            break;
        }
        case 2:
        {
            return impl->MixAndResampleMonoToStereo_Polyphase(out, in[0] + 4, num, frac, delta, 0.5f, -0.25f, step, -step);
        }
        case 3:
        {
            return impl->MixAndResampleStereoToStereo_Polyphase(out, in[0] + 4, in[1] + 4, num, frac, delta, 0.5f, -0.25f, step, -step, 0.25f, 0.5f, -step, step);
        }
        case 4:
        {
            impl->ApplyClampedGain(out, in, num, 1.25f, -step * 2);
            break;
        }
        case 5:
        {
            impl->ApplyGainAndInterleaveToS16(pcm_out, in, num, 1.5f, -step);
            break;
        }
        case 6:
        {
            impl->ApplyGain(out, in, num, 0.75f, -step);
            break;
        }
        case 7:
        {
            impl->GatherPowerData(in, num, 0.75f, power[0], power[1], power[2], power[3]);
            break;
        }
        case 8:
        {
            impl->ConvertFromS16(out[0], pcm16, num);
            break;
        }
        case 9:
        {
            impl->ConvertFromS8(out[0], pcm8, num);
            break;
        }
        case 10:
        {
            impl->Deinterleave(out, in[0], num);
            break;
        }
        case 11:
        {
            impl->DeinterleaveFromS16(out, pcm16, num);
            break;
        }
        case 12:
        {
            impl->DeinterleaveFromS8(out, pcm8, num);
            break;
        }
    }

    return 0;
}

// Compares every operation with recorded 1.14.1 scalar output independently of the current fallback.
static void TestLegacyOutput(const dmSound::DSPImpl* impl)
{
    const unsigned num = 17;
    float input_base[2][44], output_base[2][20];
    float* in[] = { input_base[0] + 1, input_base[1] + 1 };
    float* out[] = { output_base[0] + 1, output_base[1] + 1 };
    int16_t pcm16[34], pcm_out[36];
    int8_t pcm8[34];
    for (unsigned i = 0; i < 42; ++i)
    {
        in[0][i] = ((int)((i * 73) % 257) - 128) * 255.75f;
        in[1][i] = ((int)((i * 31 + 17) % 251) - 125) * 127.25f;
    }

    for (unsigned i = 0; i < 34; ++i)
    {
        pcm16[i] = (int16_t)((i * 4093 + 12345) & 65535);
        pcm8[i] = (int8_t)(i * 37 + 113);
    }

    for (unsigned op = 0; op < 13; ++op)
    {
        for (unsigned c = 0; c < 2; ++c)
        {
            for (unsigned i = 0; i < 20; ++i)
            {
                output_base[c][i] = 123.25f;
            }
        }

        for (unsigned i = 0; i < 36; ++i)
        {
            pcm_out[i] = 12345;
        }

        float power[4] = { -1, -1, -1, -1 };
        CHECK(Run(impl, op, out, in, pcm16, pcm8, pcm_out + 1, num, 3073, power) == g_ExpectedFraction[op]);
        for (unsigned c = 0; c < 2; ++c)
        {
            CHECK(out[c][-1] == 123.25f);
            CHECK(out[c][num] == 123.25f);
            for (unsigned i = 0; i < num; ++i)
            {
                float expected = g_ExpectedSamples[op][c][i];
                CHECK(fabsf(out[c][i] - expected) <= 0.02f + fabsf(expected) * 0.00001f);
            }
        }

        CHECK(pcm_out[0] == 12345);
        CHECK(pcm_out[35] == 12345);
        for (unsigned i = 0; i < 34; ++i)
        {
            CHECK(pcm_out[i + 1] == (op == 5 ? g_ExpectedPCM[i] : 12345));
        }

        for (unsigned i = 0; i < 4; ++i)
        {
            float expected = op == 7 ? g_ExpectedPower[i] : -1.0f;
            CHECK(fabsf(power[i] - expected) <= 0.01f + fabsf(expected) * 0.00001f);
        }
    }
}

// Verifies gain sign, ramp direction, zero crossing and in-place output with exact known values.
static void TestGainRamp(const dmSound::DSPImpl* impl)
{
    const unsigned num = 33;
    float input_base[2][35], output_base[2][35];
    float* in[] = { input_base[0] + 1, input_base[1] + 1 };
    float* out[] = { output_base[0] + 1, output_base[1] + 1 };
    for (unsigned ramp = 0; ramp < 2; ++ramp)
    {
        for (unsigned c = 0; c < 2; ++c)
        {
            input_base[c][0] = input_base[c][34] = 123.25f;
            output_base[c][0] = output_base[c][34] = 123.25f;
            for (unsigned i = 0; i < num; ++i)
            {
                in[c][i] = c == 0 ? 1024.0f : -512.0f;
                out[c][i] = 123.25f;
            }
        }

        float scale = ramp == 0 ? 0.75f : -0.5f;
        float step = ramp == 0 ? -1.0f / 32.0f : 1.0f / 64.0f;
        impl->ApplyGain(out, in, num, scale, step);
        impl->ApplyGain(in, in, num, scale, step);
        for (unsigned c = 0; c < 2; ++c)
        {
            CHECK(in[c][-1] == 123.25f);
            CHECK(in[c][num] == 123.25f);
            CHECK(out[c][-1] == 123.25f);
            CHECK(out[c][num] == 123.25f);
            for (unsigned i = 0; i < num; ++i)
            {
                float expected_l = ramp == 0 ? 768.0f - 32.0f * i : -512.0f + 16.0f * i;
                float expected = c == 0 ? expected_l : -expected_l / 2.0f;
                CHECK(out[c][i] == expected);
                CHECK(in[c][i] == expected);
            }
        }
    }
}

// Compares all kernels with the scalar reference across tails, offsets, clipping,
// ramps, in-place gain and nonintegral resampling, preserving output sentinels.
static void TestKernel(const dmSound::DSPImpl* impl)
{
    for (unsigned size = 0; size < 72; ++size)
    {
        unsigned num = size < 66 ? size : size == 66 ? 255 : size == 67 ? 256 : size == 68 ? 257 : size == 69 ? 511 : size == 70 ? 512 : 1023;
        for (unsigned offset = 0; offset < 4; ++offset)
        {
            unsigned input_count = 2 * num + 8;
            float* input_base[2];
            float* input[2];
            float* expected_base[2];
            float* actual_base[2];
            float* expected[2];
            float* actual[2];
            for (unsigned c = 0; c < 2; ++c)
            {
                input_base[c] = (float*)malloc((input_count + offset) * sizeof(float));
                input[c] = input_base[c] + offset;
                expected_base[c] = (float*)malloc((num + offset + 2) * sizeof(float));
                actual_base[c] = (float*)malloc((num + offset + 2) * sizeof(float));
                expected[c] = expected_base[c] + offset + 1;
                actual[c] = actual_base[c] + offset + 1;
                for (unsigned i = 0; i < input_count; ++i)
                {
                    input[c][i] = ((int)((i * 73 + c * 31) % 257) - 128) * 255.75f;
                }
            }

            int16_t* pcm16 = (int16_t*)malloc((2 * num + 1) * sizeof(int16_t));
            int8_t* pcm8 = (int8_t*)malloc(2 * num + 1);
            int16_t* pcm_expected = (int16_t*)malloc((2 * num + 2) * sizeof(int16_t));
            int16_t* pcm_actual = (int16_t*)malloc((2 * num + 2) * sizeof(int16_t));
            for (unsigned i = 0; i < 2 * num; ++i)
            {
                pcm16[i] = (int16_t)((i * 4093) & 65535);
                pcm8[i] = (int8_t)i;
            }

            for (unsigned op = 0; op < 13; ++op)
            {
                for (unsigned c = 0; c < 2; ++c)
                {
                    for (unsigned i = 0; i < num + offset + 2; ++i)
                    {
                        expected_base[c][i] = actual_base[c][i] = 123.25f;
                    }
                }

                for (unsigned i = 0; i < 2 * num + 2; ++i)
                {
                    pcm_expected[i] = pcm_actual[i] = 12345;
                }

                float power_expected[4] = { -1, -1, -1, -1 }, power_actual[4] = { -1, -1, -1, -1 };
                uint64_t delta = size & 1 ? 3073 : 1027;
                uint64_t frac_expected = Run(&dmSound::fallback_impl, op, expected, input, pcm16, pcm8, pcm_expected + 1, num, delta, power_expected);
                uint64_t frac_actual = Run(impl, op, actual, input, pcm16, pcm8, pcm_actual + 1, num, delta, power_actual);
                CHECK(frac_expected == frac_actual);
                for (unsigned c = 0; c < 2; ++c)
                {
                    CHECK(actual[c][-1] == 123.25f);
                    CHECK(actual[c][num] == 123.25f);
                    for (unsigned i = 0; i < num; ++i)
                    {
                        float tolerance = 0.02f + fabsf(expected[c][i]) * 0.00001f;
                        CHECK(fabsf(expected[c][i] - actual[c][i]) <= tolerance);
                    }
                }

                CHECK(pcm_actual[0] == 12345);
                CHECK(pcm_actual[2 * num + 1] == 12345);
                for (unsigned i = 0; i < 2 * num; ++i)
                {
                    CHECK(abs((int)pcm_expected[i + 1] - (int)pcm_actual[i + 1]) <= 1);
                }

                for (unsigned i = 0; i < 4; ++i)
                {
                    CHECK(fabsf(power_expected[i] - power_actual[i]) <= 0.01f + fabsf(power_expected[i]) * 0.00001f);
                }
            }

            for (unsigned c = 0; c < 2; ++c)
            {
                memcpy(expected[c], input[c], num * sizeof(float));
                memcpy(actual[c], input[c], num * sizeof(float));
            }

            dmSound::fallback_impl.ApplyGain(expected, expected, num, 0.731f, 0.00013f);
            impl->ApplyGain(actual, actual, num, 0.731f, 0.00013f);
            for (unsigned c = 0; c < 2; ++c)
            {
                CHECK(actual[c][-1] == 123.25f);
                CHECK(actual[c][num] == 123.25f);
                for (unsigned i = 0; i < num; ++i)
                {
                    float tolerance = 0.02f + fabsf(expected[c][i]) * 0.00001f;
                    CHECK(fabsf(expected[c][i] - actual[c][i]) <= tolerance);
                }
            }

            for (unsigned c = 0; c < 2; ++c)
            {
                free(input_base[c]);
                free(expected_base[c]);
                free(actual_base[c]);
            }

            free(pcm16);
            free(pcm8);
            free(pcm_expected);
            free(pcm_actual);
        }
    }

    printf("DSP %s: checked all 13 operations\n", impl->m_Name);
}

// Verifies stereo FIR sharing, diagonal routing, ramp transitions and scalar tails.
static void TestStereoRouting(const dmSound::DSPImpl* impl)
{
    // Binary ramp increments isolate FIR and routing checks from width-dependent ramp rounding.
    float left[2058], right[2058];
    float expected_base[2][1028], actual_base[2][1028];
    for (unsigned i = 0; i < 2058; ++i)
    {
        left[i] = ((int)((i * 73) % 257) - 128) * 255.75f;
        right[i] = ((int)((i * 31) % 251) - 125) * 127.25f;
    }

    for (unsigned size = 0; size < 72; ++size)
    {
        unsigned num = size < 66 ? size : size == 66 ? 255 : size == 67 ? 256 : size == 68 ? 257 : size == 69 ? 511 : size == 70 ? 512 : 1023;
        for (unsigned offset = 0; offset < 4; ++offset)
        {
            float* expected[] = { expected_base[0] + offset + 1, expected_base[1] + offset + 1 };
            float* actual[] = { actual_base[0] + offset + 1, actual_base[1] + offset + 1 };
            for (unsigned routing = 0; routing < 6; ++routing)
            {
                for (unsigned c = 0; c < 2; ++c)
                {
                    for (unsigned i = 0; i < num + offset + 2; ++i)
                    {
                        expected_base[c][i] = actual_base[c][i] = 123.25f;
                    }
                }

                float cross_r = routing == 1 ? 0.125f : routing == 5 ? -0.0f : 0.0f;
                float cross_dr = routing == 2 ? 1.0f / 4096.0f : 0.0f;
                float cross_l = routing == 3 ? -0.125f : 0.0f;
                float cross_dl = routing == 4 ? -1.0f / 8192.0f : 0.0f;
                uint64_t frac = size & 1 ? 2047 : 0;
                uint64_t delta = offset & 1 ? 3073 : 1027;
                uint64_t expected_frac = dmSound::fallback_impl.MixAndResampleStereoToStereo_Polyphase(expected, left + offset + 4, right + offset + 4, num, frac, delta,
                                                                                                       0.731f, cross_r, 1.0f / 4096.0f, cross_dr, cross_l, -0.29f, cross_dl, -1.0f / 8192.0f);
                uint64_t actual_frac = impl->MixAndResampleStereoToStereo_Polyphase(actual, left + offset + 4, right + offset + 4, num, frac, delta,
                                                                                    0.731f, cross_r, 1.0f / 4096.0f, cross_dr, cross_l, -0.29f, cross_dl, -1.0f / 8192.0f);
                CHECK(expected_frac == actual_frac);
                for (unsigned c = 0; c < 2; ++c)
                {
                    CHECK(actual[c][-1] == 123.25f);
                    CHECK(actual[c][num] == 123.25f);
                    for (unsigned i = 0; i < num; ++i)
                    {
                        float tolerance = 0.02f + fabsf(expected[c][i]) * 0.00001f;
                        CHECK(fabsf(expected[c][i] - actual[c][i]) <= tolerance);
                    }
                }
            }
        }
    }
}

// Verifies default selection, explicit CPU selection and rejection of unavailable ISAs.
static void TestSelection(void)
{
    CHECK(dmSound::SelectDSPImpl(dmSound::DSPIMPL_TYPE_DEFAULT));
    CHECK(dmSound::g_DSPImpl == dmSound::GetDSPImpl(dmSound::DSPIMPL_TYPE_DEFAULT));
#if defined(DM_SOUND_DSP_X86)
    dsimd_version_t version = SimdGetVersion();
    unsigned lanes = version == DSIMD_VERSION_AVX512 ? 16 : version == DSIMD_VERSION_AVX2 ? 8 : version >= DSIMD_VERSION_SSE3 && version <= DSIMD_VERSION_SSE42 ? 4 : 1;
    CHECK(dmSound::g_DSPImpl->m_Lanes == lanes);
#elif defined(DM_SOUND_DSP_WASM)
    CHECK(dmSound::g_DSPImpl->m_Lanes == 16);
#elif defined(DM_SOUND_DSP_NEON)
    CHECK(dmSound::g_DSPImpl->m_Lanes == (SimdGetVersion() == DSIMD_VERSION_NEON ? 4u : 1u));
#else
    CHECK(dmSound::g_DSPImpl->m_Lanes == 1);
#endif
    CHECK(dmSound::SelectDSPImpl(dmSound::DSPIMPL_TYPE_CPU));
    CHECK(strcmp(dmSound::g_DSPImpl->m_Name, "CPU") == 0);
    dmSound::DSPImplType unavailable = dmSound::GetDSPImpl(dmSound::DSPIMPL_TYPE_NEON) ? dmSound::DSPIMPL_TYPE_AVX512 : dmSound::DSPIMPL_TYPE_NEON;
    CHECK(!dmSound::SelectDSPImpl(unavailable));
    CHECK(strcmp(dmSound::g_DSPImpl->m_Name, "CPU") == 0);
}

int main()
{
    TestSelection();
    const dmSound::DSPImpl* kernels[12];
    unsigned count = 0;
    kernels[count++] = &dmSound::fallback_impl;
    for (int type = dmSound::DSPIMPL_TYPE_SSE2; type <= dmSound::DSPIMPL_TYPE_NEON; ++type)
    {
        const dmSound::DSPImpl* impl = dmSound::GetDSPImpl((dmSound::DSPImplType)type);
        if (impl)
        {
            kernels[count++] = impl;
        }
    }

    dsimd_version_t version = SimdGetVersion();
    bool supports_test = true;
#if defined(DM_SOUND_DSP_X86)
    supports_test = (version >= DSIMD_VERSION_SSE3 && version <= DSIMD_VERSION_SSE42) || version == DSIMD_VERSION_AVX2 || version == DSIMD_VERSION_AVX512;
#elif defined(DM_SOUND_DSP_NEON)
    supports_test = version == DSIMD_VERSION_NEON;
#endif
    (void)version;
    if (supports_test)
    {
        kernels[count++] = dmSound::GetDSPImplTest4();
        kernels[count++] = dmSound::GetDSPImplTest8();
        kernels[count++] = dmSound::GetDSPImplTest16();
    }

    for (unsigned i = 0; i < count; ++i)
    {
        TestLegacyOutput(kernels[i]);
        TestGainRamp(kernels[i]);
        TestKernel(kernels[i]);
        TestStereoRouting(kernels[i]);
    }

    printf("DSP: %u checks, %u failures\n", g_Checks, g_Failures);
    return g_Failures ? 1 : 0;
}
