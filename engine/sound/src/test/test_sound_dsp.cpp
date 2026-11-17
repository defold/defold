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
#include "../dsp/sound_dsp.h"
#include "test_sound_dsp_expected.h"
#include <simd/simd_runtime.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

namespace dmSound
{
    const DSPImpl* GetDSPImplTest4(unsigned unroll);
#if defined(DM_SOUND_DSP_X86)
    const DSPImpl* GetDSPImplTest8();
    const DSPImpl* GetDSPImplTest16();
#endif
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

static unsigned FrameCountForTest(unsigned size)
{
    static const unsigned large_sizes[] = { 255, 256, 257, 511, 512, 1023 };
    return size < 66 ? size : large_sizes[size - 66];
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

// Verifies constant mono mixing and gain fast paths, including signed-zero deltas,
// negative gains, in-place output and every unroll boundary with guard samples.
static void TestConstantMonoAndGain(const dmSound::DSPImpl* impl)
{
    const float scales[] = { 0.0f, -0.5f, 0.75f, 1.25f };
    float input[1024], left[1026], right[1026];
    float* out[] = { left + 1, right + 1 };
    for (unsigned i = 0; i < 1024; ++i)
    {
        input[i] = (float)((int)(i % 251) - 125) * 0.5f;
    }

    for (unsigned size = 0; size < 72; ++size)
    {
        unsigned num = FrameCountForTest(size);
        for (unsigned g = 0; g < sizeof(scales) / sizeof(scales[0]); ++g)
        {
            float scale = scales[g];
            for (unsigned c = 0; c < 2; ++c)
            {
                out[c][-1] = out[c][num] = 123.25f;
                for (unsigned i = 0; i < num; ++i)
                {
                    out[c][i] = 32.0f;
                }
            }

            impl->MixScaledMonoToStereo(out, input, num, scale, -scale, 0.0f, -0.0f);
            for (unsigned i = 0; i < num; ++i)
            {
                CHECK(out[0][i] == 32.0f + input[i] * scale);
                CHECK(out[1][i] == 32.0f - input[i] * scale);
            }

            impl->ApplyGain(out, out, num, scale, -0.0f);
            for (unsigned c = 0; c < 2; ++c)
            {
                CHECK(out[c][-1] == 123.25f);
                CHECK(out[c][num] == 123.25f);
                for (unsigned i = 0; i < num; ++i)
                {
                    float mixed = c == 0 ? 32.0f + input[i] * scale : 32.0f - input[i] * scale;
                    CHECK(out[c][i] == mixed * scale);
                }
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
        unsigned num = FrameCountForTest(size);
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
                        if (fabsf(expected[c][i] - actual[c][i]) > tolerance && g_Failures < 10)
                        {
                            printf("DSP %u lanes op %u frames %u channel %u lane %u: expected %.9g actual %.9g\n", impl->m_Lanes, op, num, c, i, expected[c][i], actual[c][i]);
                        }

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

    printf("DSP %u lanes: checked all 13 operations\n", impl->m_Lanes);
}

// Verifies ramped stereo mixing when output channels have independent alignments.
static void TestStereoMixAlignment(const dmSound::DSPImpl* impl)
{
    const unsigned num = 17;
    float input[2][24], expected_base[2][24], actual_base[2][24];
    for (unsigned i = 0; i < 24; ++i)
    {
        input[0][i] = ((int)i * 13 - 127) * 255.75f;
        input[1][i] = ((int)i * 7 - 83) * 127.25f;
    }

    for (unsigned left = 0; left < 4; ++left)
    {
        for (unsigned right = 0; right < 4; ++right)
        {
            float* expected[] = { expected_base[0] + left + 1, expected_base[1] + right + 1 };
            float* actual[] = { actual_base[0] + left + 1, actual_base[1] + right + 1 };
            for (unsigned c = 0; c < 2; ++c)
            {
                for (unsigned i = 0; i < 24; ++i)
                {
                    expected_base[c][i] = actual_base[c][i] = 123.25f;
                }
            }

            const float step = 1.0f / 1024.0f;
            dmSound::fallback_impl.MixScaledStereoToStereo(expected, input[0] + 1, input[1] + 2, num, 0.5f, -0.25f, step, -step, 0.25f, 0.5f, -step, step);
            impl->MixScaledStereoToStereo(actual, input[0] + 1, input[1] + 2, num, 0.5f, -0.25f, step, -step, 0.25f, 0.5f, -step, step);
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

// Verifies steady-volume stereo mixing across routes, negative zero deltas, widths and tails.
static void TestConstantStereo(const dmSound::DSPImpl* impl)
{
    static const float gains[][4] =
    {
        { 0.5f, -0.25f, 0.25f, 0.5f },
        { 1.0f, 0.0f, 0.0f, -0.5f },
        { -0.0f, 0.0f, -0.0f, 0.0f },
        { 0.731f, -0.29f, -0.13f, 0.42f }
    };
    float left[1028], right[1028];
    float expected_base[2][1028], actual_base[2][1028];
    for (unsigned i = 0; i < 1028; ++i)
    {
        left[i] = ((int)((i * 73) % 257) - 128) * 255.75f;
        right[i] = ((int)((i * 31) % 251) - 125) * 127.25f;
    }

    for (unsigned size = 0; size < 72; ++size)
    {
        unsigned num = FrameCountForTest(size);
        for (unsigned offset = 0; offset < 4; ++offset)
        {
            float* expected[] = { expected_base[0] + offset + 1, expected_base[1] + offset + 1 };
            float* actual[] = { actual_base[0] + offset + 1, actual_base[1] + offset + 1 };
            for (unsigned g = 0; g < sizeof(gains) / sizeof(gains[0]); ++g)
            {
                for (unsigned c = 0; c < 2; ++c)
                {
                    for (unsigned i = 0; i < num + offset + 2; ++i)
                    {
                        expected_base[c][i] = actual_base[c][i] = 123.25f;
                    }
                }

                dmSound::fallback_impl.MixScaledStereoToStereo(expected, left + offset, right + offset, num, gains[g][0], gains[g][1], 0.0f, -0.0f, gains[g][2], gains[g][3], -0.0f, 0.0f);
                impl->MixScaledStereoToStereo(actual, left + offset, right + offset, num, gains[g][0], gains[g][1], 0.0f, -0.0f, gains[g][2], gains[g][3], -0.0f, 0.0f);
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
        unsigned num = FrameCountForTest(size);
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

// Grouping stereo outputs must match individual calls exactly, including fractional phases and tails.
static void TestStereoResamplingBatches(const dmSound::DSPImpl* impl)
{
    float left[272], right[272];
    float expected_base[2][70], actual_base[2][70];
    const uint64_t deltas[] = { 0, 1024, 1882, 2048, 2229, 3072, 3073, 4096, 8193 };
    for (unsigned i = 0; i < 272; ++i)
    {
        left[i] = ((int)((i * 73) % 257) - 128) * 1.125f;
        right[i] = ((int)((i * 31) % 251) - 125) * 0.375f;
    }

    for (uint64_t phase = 0; phase < 2048; ++phase)
    {
        unsigned num = phase & 1 ? 65 : 17;
        unsigned offset = (phase >> 4) & 3;
        const float* in_l = left + 3 + (phase & 3);
        const float* in_r = right + 3 + ((phase >> 2) & 3);
        float* expected[] = { expected_base[0] + offset + 1, expected_base[1] + offset + 1 };
        float* actual[] = { actual_base[0] + offset + 1, actual_base[1] + offset + 1 };
        for (unsigned c = 0; c < 2; ++c)
        {
            for (unsigned i = 0; i < 70; ++i)
            {
                expected_base[c][i] = actual_base[c][i] = 123.25f;
            }
        }

        uint64_t delta = deltas[phase % (sizeof(deltas) / sizeof(deltas[0]))];
        float cross_l = phase & 64 ? 0.125f : 0.0f;
        float cross_r = phase & 64 ? -0.25f : 0.0f;
        uint64_t expected_frac = phase;
        for (unsigned i = 0; i < num; ++i)
        {
            float* sample[] = { expected[0] + i, expected[1] + i };
            expected_frac = impl->MixAndResampleStereoToStereo_Polyphase(sample, in_l, in_r, 1, expected_frac, delta,
                                                                         0.5f, cross_r, 0.0f, 0.0f, cross_l, -0.25f, 0.0f, 0.0f);
        }

        uint64_t actual_frac = impl->MixAndResampleStereoToStereo_Polyphase(actual, in_l, in_r, num, phase, delta,
                                                                            0.5f, cross_r, 0.0f, 0.0f, cross_l, -0.25f, 0.0f, 0.0f);
        CHECK(expected_frac == actual_frac);
        for (unsigned c = 0; c < 2; ++c)
        {
            CHECK(actual[c][-1] == 123.25f);
            CHECK(actual[c][num] == 123.25f);
            for (unsigned i = 0; i < num; ++i)
            {
                CHECK(memcmp(&expected[c][i], &actual[c][i], sizeof(float)) == 0);
            }
        }
    }
}

static dmSound::DSPContext g_DispatchContext;

static void DispatchMixScaledMonoToStereo(float* out[], const float* in, uint32_t num, float scale_l, float scale_r, float scale_delta_l, float scale_delta_r)
{
    return dmSound::MixScaledMonoToStereo(g_DispatchContext, out, in, num, scale_l, scale_r, scale_delta_l, scale_delta_r);
}

static void DispatchMixScaledStereoToStereo(float* out[], const float* in_l, const float* in_r, uint32_t num, float scale_l0, float scale_r0, float scale_delta_l0, float scale_delta_r0, float scale_l1, float scale_r1, float scale_delta_l1, float scale_delta_r1)
{
    return dmSound::MixScaledStereoToStereo(g_DispatchContext, out, in_l, in_r, num, scale_l0, scale_r0, scale_delta_l0, scale_delta_r0, scale_l1, scale_r1, scale_delta_l1, scale_delta_r1);
}

static uint64_t DispatchMixAndResampleMonoToStereo_Polyphase(float* out[], const float* in, uint32_t num, uint64_t frac, uint64_t delta, float scale_l, float scale_r, float scale_delta_l, float scale_delta_r)
{
    return dmSound::MixAndResampleMonoToStereo_Polyphase(g_DispatchContext, out, in, num, frac, delta, scale_l, scale_r, scale_delta_l, scale_delta_r);
}

static uint64_t DispatchMixAndResampleStereoToStereo_Polyphase(float* out[], const float* in_l, const float* in_r, uint32_t num, uint64_t frac, uint64_t delta, float scale_l0, float scale_r0, float scale_delta_l0, float scale_delta_r0, float scale_l1, float scale_r1, float scale_delta_l1, float scale_delta_r1)
{
    return dmSound::MixAndResampleStereoToStereo_Polyphase(g_DispatchContext, out, in_l, in_r, num, frac, delta, scale_l0, scale_r0, scale_delta_l0, scale_delta_r0, scale_l1, scale_r1, scale_delta_l1, scale_delta_r1);
}

static void DispatchApplyClampedGain(float* out[], float* in[], uint32_t num, float scale, float scale_delta)
{
    return dmSound::ApplyClampedGain(g_DispatchContext, out, in, num, scale, scale_delta);
}

static void DispatchApplyGainAndInterleaveToS16(int16_t* out, float* in[], uint32_t num, float scale, float scale_delta)
{
    return dmSound::ApplyGainAndInterleaveToS16(g_DispatchContext, out, in, num, scale, scale_delta);
}

static void DispatchApplyGain(float* out[], float* in[], uint32_t num, float scale, float scale_delta)
{
    return dmSound::ApplyGain(g_DispatchContext, out, in, num, scale, scale_delta);
}

static void DispatchGatherPowerData(float* in[], uint32_t num, float gain, float& sum_sq_left, float& sum_sq_right, float& max_sq_left, float& max_sq_right)
{
    return dmSound::GatherPowerData(g_DispatchContext, in, num, gain, sum_sq_left, sum_sq_right, max_sq_left, max_sq_right);
}

static void DispatchConvertFromS16(float* out, const int16_t* in, uint32_t num)
{
    return dmSound::ConvertFromS16(g_DispatchContext, out, in, num);
}

static void DispatchConvertFromS8(float* out, const int8_t* in, uint32_t num)
{
    return dmSound::ConvertFromS8(g_DispatchContext, out, in, num);
}

static void DispatchDeinterleave(float* out[], const float* in, uint32_t num)
{
    return dmSound::Deinterleave(g_DispatchContext, out, in, num);
}

static void DispatchDeinterleaveFromS16(float* out[], const int16_t* in, uint32_t num)
{
    return dmSound::DeinterleaveFromS16(g_DispatchContext, out, in, num);
}

static void DispatchDeinterleaveFromS8(float* out[], const int8_t* in, uint32_t num)
{
    return dmSound::DeinterleaveFromS8(g_DispatchContext, out, in, num);
}

// Verifies startup-selected function pointers with the independent output and layout cases.
static void TestFunctionDispatch(void)
{
    dmSound::DSPImpl dispatch =
    {
        1,
        DispatchMixScaledMonoToStereo,
        DispatchMixScaledStereoToStereo,
        DispatchMixAndResampleMonoToStereo_Polyphase,
        DispatchMixAndResampleStereoToStereo_Polyphase,
        DispatchApplyClampedGain,
        DispatchApplyGainAndInterleaveToS16,
        DispatchApplyGain,
        DispatchGatherPowerData,
        DispatchConvertFromS16,
        DispatchConvertFromS8,
        DispatchDeinterleave,
        DispatchDeinterleaveFromS16,
        DispatchDeinterleaveFromS8,
    };
    for (int type = dmSound::DSPIMPL_TYPE_DEFAULT; type <= dmSound::DSPIMPL_TYPE_AVX512; ++type)
    {
        dmSound::SelectDSPImpl(&g_DispatchContext, (dmSound::DSPImplType)type);
        if (type > dmSound::DSPIMPL_TYPE_CPU && g_DispatchContext.m_Version == DSIMD_VERSION_SCALAR)
        {
            continue;
        }

        dispatch.m_Lanes = g_DispatchContext.m_Impl.m_Lanes;
        TestKernel(&dispatch);
        TestStereoMixAlignment(&dispatch);
        TestConstantStereo(&dispatch);
        TestStereoRouting(&dispatch);
        TestStereoResamplingBatches(&dispatch);
    }

#if defined(DM_SOUND_DSP_X86)
    dmSound::SelectDSPImpl(&g_DispatchContext, dmSound::DSPIMPL_TYPE_SSE3);
    if (g_DispatchContext.m_Version != DSIMD_VERSION_SSE3)
    {
        return;
    }

#elif defined(DM_SOUND_DSP_NEON)
    dmSound::SelectDSPImpl(&g_DispatchContext, dmSound::DSPIMPL_TYPE_NEON);
    if (g_DispatchContext.m_Version != DSIMD_VERSION_NEON)
    {
        return;
    }

#endif
    // Unroll variants share a native width and remain independently selectable.
    const dmSound::DSPImpl* variants[] = { dmSound::GetDSPImplTest4(1), dmSound::GetDSPImplTest4(2), dmSound::GetDSPImplTest4(4), dmSound::GetDSPImplTest4(8) };
    for (unsigned i = 0; i < 4; ++i)
    {
        g_DispatchContext.m_Impl = *variants[i];
        CHECK(g_DispatchContext.m_Impl.m_Lanes == 4);
        CHECK(g_DispatchContext.m_Impl.ApplyGain != variants[(i + 1) % 4]->ApplyGain);
        dispatch.m_Lanes = g_DispatchContext.m_Impl.m_Lanes;
        TestKernel(&dispatch);
        TestStereoMixAlignment(&dispatch);
        TestConstantStereo(&dispatch);
        TestStereoRouting(&dispatch);
        TestStereoResamplingBatches(&dispatch);
    }

}

// Verifies widest-supported default callbacks, explicit CPU selection and rejection of unavailable ISAs.
static void TestSelection(void)
{
    dmSound::DSPContext context;
    dmSound::SelectDSPImpl(&context, dmSound::DSPIMPL_TYPE_DEFAULT);
    CHECK(context.m_Type == dmSound::DSPIMPL_TYPE_DEFAULT);
#if defined(DM_SOUND_DSP_X86)
    dsimd_version_t version = SimdGetVersion();
    unsigned lanes = version == DSIMD_VERSION_AVX512 ? 16 : version == DSIMD_VERSION_AVX2 ? 8 : version >= DSIMD_VERSION_SSE3 && version <= DSIMD_VERSION_SSE42 ? 4 : 1;
    CHECK(context.m_Impl.m_Lanes == lanes);
    dsimd_version_t selected = lanes > 4 ? version : lanes == 4 ? DSIMD_VERSION_SSE3 : DSIMD_VERSION_SCALAR;
    CHECK(context.m_Version == selected);
    if (lanes >= 4)
    {
        dmSound::SelectDSPImpl(&context, dmSound::DSPIMPL_TYPE_SSE3);
        CHECK(context.m_Type == dmSound::DSPIMPL_TYPE_SSE3);
        CHECK(context.m_Version == DSIMD_VERSION_SSE3);
        CHECK(context.m_Impl.m_Lanes == 4);
    }

#elif defined(DM_SOUND_DSP_WASM)
    CHECK(context.m_Impl.m_Lanes == 4);
    CHECK(context.m_Version == DSIMD_VERSION_WASM_SIMD128);
#elif defined(DM_SOUND_DSP_NEON)
    CHECK(context.m_Impl.m_Lanes == (SimdGetVersion() == DSIMD_VERSION_NEON ? 4u : 1u));
    CHECK(context.m_Version == (context.m_Impl.m_Lanes == 1 ? DSIMD_VERSION_SCALAR : DSIMD_VERSION_NEON));
#else
    CHECK(context.m_Impl.m_Lanes == 1);
    CHECK(context.m_Version == DSIMD_VERSION_SCALAR);
#endif
    dmSound::SelectDSPImpl(&context, dmSound::DSPIMPL_TYPE_CPU);
    CHECK(context.m_Type == dmSound::DSPIMPL_TYPE_CPU);
    CHECK(context.m_Impl.m_Lanes == 1);
    CHECK(context.m_Version == DSIMD_VERSION_SCALAR);
    CHECK(context.m_Impl.ApplyGain != 0);
    dmSound::SelectDSPImpl(&context, dmSound::DSPIMPL_TYPE_SSE2);
    CHECK(context.m_Type == dmSound::DSPIMPL_TYPE_SSE2);
    CHECK(context.m_Version == DSIMD_VERSION_SCALAR);
    CHECK(context.m_Impl.m_Lanes == 1);
}

// The reported name and width must follow runtime selection, including explicit CPU fallback.
static void TestSelectionNames(void)
{
    const char* names[] = { "CPU", "CPU", "SSE3", "SSE3", "SSE3", "SSE3", "NEON", "WASM SIMD128", "AVX2", "AVX512" };
    const dsimd_version_t versions[] = { DSIMD_VERSION_SCALAR, DSIMD_VERSION_SCALAR, DSIMD_VERSION_SSE3, DSIMD_VERSION_SSE3, DSIMD_VERSION_SSE3, DSIMD_VERSION_SSE3, DSIMD_VERSION_NEON, DSIMD_VERSION_WASM_SIMD128, DSIMD_VERSION_AVX2, DSIMD_VERSION_AVX512 };
    const unsigned lanes[] = { 1, 1, 4, 4, 4, 4, 4, 4, 8, 16 };
    dmSound::DSPContext context;
    for (int type = dmSound::DSPIMPL_TYPE_CPU; type <= dmSound::DSPIMPL_TYPE_AVX512; ++type)
    {
        dmSound::SelectDSPImpl(&context, (dmSound::DSPImplType)type);
        bool selected = context.m_Version != DSIMD_VERSION_SCALAR;
        CHECK(context.m_Type == (dmSound::DSPImplType)type);
        CHECK(context.m_Version == (selected ? versions[type] : DSIMD_VERSION_SCALAR));
        CHECK(strcmp(SimdGetVersionName(context.m_Version), selected ? names[type] : "CPU") == 0);
        CHECK(context.m_Impl.m_Lanes == (selected ? lanes[type] : 1));
    }
}

// Equal lane counts must not allow a foreign ISA or an unavailable width.
static void TestUnavailableVersions(void)
{
    const dmSound::DSPImplType types[] = {
        dmSound::DSPIMPL_TYPE_SSE3, dmSound::DSPIMPL_TYPE_NEON, dmSound::DSPIMPL_TYPE_WASM_SIMD128,
        dmSound::DSPIMPL_TYPE_AVX2, dmSound::DSPIMPL_TYPE_AVX512
    };
    dsimd_version_t available = SimdGetVersion();
    dmSound::DSPContext scalar;
    dmSound::SelectDSPImpl(&scalar, dmSound::DSPIMPL_TYPE_CPU);
    for (unsigned i = 0; i < sizeof(types) / sizeof(types[0]); ++i)
    {
        bool supported = false;
#if defined(DM_SOUND_DSP_X86)
        supported = types[i] == dmSound::DSPIMPL_TYPE_SSE3 && available >= DSIMD_VERSION_SSE3;
        supported |= types[i] == dmSound::DSPIMPL_TYPE_AVX2 && (available == DSIMD_VERSION_AVX2 || available == DSIMD_VERSION_AVX512);
        supported |= types[i] == dmSound::DSPIMPL_TYPE_AVX512 && available == DSIMD_VERSION_AVX512;
#elif defined(DM_SOUND_DSP_NEON)
        supported = types[i] == dmSound::DSPIMPL_TYPE_NEON && available == DSIMD_VERSION_NEON;
#elif defined(DM_SOUND_DSP_WASM)
        supported = types[i] == dmSound::DSPIMPL_TYPE_WASM_SIMD128 && available == DSIMD_VERSION_WASM_SIMD128;
#endif
        dmSound::DSPContext context;
        dmSound::SelectDSPImpl(&context, types[i]);
        CHECK(context.m_Type == types[i]);
        CHECK(context.m_Version == (supported ? (dsimd_version_t)types[i] : DSIMD_VERSION_SCALAR));
        if (!supported)
        {
            CHECK(context.m_Impl.m_Lanes == 1);
            CHECK(context.m_Impl.ApplyGain == scalar.m_Impl.ApplyGain);
            TestLegacyOutput(&context.m_Impl);
        }
    }

    (void)available;
}

// Selecting another system must not modify an existing context or the shared kernel tables.
static void TestIndependentSelection(void)
{
    dmSound::DSPContext selected, other;
    dmSound::SelectDSPImpl(&selected, dmSound::DSPIMPL_TYPE_DEFAULT);
    dmSound::DSPContext before = selected;
    dmSound::SelectDSPImpl(&other, dmSound::DSPIMPL_TYPE_CPU);
    CHECK(selected.m_Impl.ApplyGain == before.m_Impl.ApplyGain);
    CHECK(selected.m_Impl.MixAndResampleMonoToStereo_Polyphase == before.m_Impl.MixAndResampleMonoToStereo_Polyphase);
    CHECK(selected.m_Impl.MixAndResampleStereoToStereo_Polyphase == before.m_Impl.MixAndResampleStereoToStereo_Polyphase);
    CHECK(selected.m_Type == before.m_Type);
    CHECK(selected.m_Version == before.m_Version);
    CHECK(selected.m_Impl.m_Lanes == before.m_Impl.m_Lanes);
    dmSound::SelectDSPImpl(&other, dmSound::DSPIMPL_TYPE_DEFAULT);
    CHECK(other.m_Impl.ApplyGain == before.m_Impl.ApplyGain);
    CHECK(other.m_Impl.MixAndResampleMonoToStereo_Polyphase == before.m_Impl.MixAndResampleMonoToStereo_Polyphase);
    CHECK(other.m_Impl.MixAndResampleStereoToStereo_Polyphase == before.m_Impl.MixAndResampleStereoToStereo_Polyphase);
}

int main()
{
    TestSelection();
    TestSelectionNames();
    TestUnavailableVersions();
    TestIndependentSelection();
    TestFunctionDispatch();
    const dmSound::DSPImpl* kernels[16];
    dmSound::DSPContext contexts[10];
    unsigned count = 0;
    kernels[count++] = &dmSound::fallback_impl;
    for (int type = dmSound::DSPIMPL_TYPE_SSE3; type <= dmSound::DSPIMPL_TYPE_AVX512; ++type)
    {
        dmSound::SelectDSPImpl(&contexts[type], (dmSound::DSPImplType)type);
        if (contexts[type].m_Version != DSIMD_VERSION_SCALAR)
        {
            kernels[count++] = &contexts[type].m_Impl;
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
        for (unsigned unroll = 1; unroll <= 8; unroll *= 2)
        {
            kernels[count++] = dmSound::GetDSPImplTest4(unroll);
        }

#if defined(DM_SOUND_DSP_X86)
        if (contexts[dmSound::DSPIMPL_TYPE_AVX2].m_Version == DSIMD_VERSION_AVX2)
        {
            kernels[count++] = dmSound::GetDSPImplTest8();
        }

        if (contexts[dmSound::DSPIMPL_TYPE_AVX512].m_Version == DSIMD_VERSION_AVX512)
        {
            kernels[count++] = dmSound::GetDSPImplTest16();
        }

#endif
    }

    for (unsigned i = 0; i < count; ++i)
    {
        TestLegacyOutput(kernels[i]);
        TestGainRamp(kernels[i]);
        TestConstantMonoAndGain(kernels[i]);
        TestKernel(kernels[i]);
        TestStereoMixAlignment(kernels[i]);
        TestConstantStereo(kernels[i]);
        TestStereoRouting(kernels[i]);
        TestStereoResamplingBatches(kernels[i]);
    }

    printf("DSP: %u checks, %u failures\n", g_Checks, g_Failures);
    return g_Failures ? 1 : 0;
}
