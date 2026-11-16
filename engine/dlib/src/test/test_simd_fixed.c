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

#include <simd/simd_fixed_uint.h>
#include <simd/simd_fixed_float.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned g_Checks = 0;
static void Check(int condition)
{
    ++g_Checks;
    if (!condition)
    {
        fprintf(stderr, "Fixed-lane check %u failed\n", g_Checks);
        abort();
    }
}

// Verifies four-lane gain ramps, including rounding at the float integer precision limit.
static void TestRamp(void)
{
    const float starts[] = {
        0.5f, 16777216.0f, -2.0f
    };
    const float steps[] = {
        -0.25f, 1.0f, 0.125f
    };
    for (unsigned trial = 0; trial < 3; ++trial)
    {
        float actual[4];
        SimdStore(actual, Simd4Ramp(starts[trial], steps[trial]));
        for (unsigned i = 0; i < 4; ++i)
        {
            Check(actual[i] == starts[trial] + (float)i * steps[trial]);
        }
    }
}

// Verifies stereo unzip lane ordering without changing signed zeros or NaN payloads.
static void TestUnzip(void)
{
    uint32_t bits[8] = {
        0x80000000u, 0x7fc12345u, 0x3f800000u, 0xff800000u, 0, 0xffc54321u, 0xbf800000u, 0x7f800000u
    };
    uint32_t actual_left[4], actual_right[4];
    dsimd_float4_t left, right;
    Simd4Unzip(Simd4Load(bits), Simd4Load(bits + 4), &left, &right);
    SimdStore(actual_left, left);
    SimdStore(actual_right, right);
    for (unsigned i = 0; i < 4; ++i)
    {
        Check(actual_left[i] == bits[2 * i]);
        Check(actual_right[i] == bits[2 * i + 1]);
    }
}

// Verifies adjacent pair ordering and cancellation before combining stereo FIR sums.
static void TestPairwiseAdd(void)
{
    float actual[4];
    SimdStore(actual, SimdPairwiseAdd(SimdMake(1, 2, 4, 8), SimdMake(-1, -2, -4, -8)));
    Check(actual[0] == 3 && actual[1] == 12 && actual[2] == -3 && actual[3] == -12);
    SimdStore(actual, SimdPairwiseAdd(SimdMake(16777216.0f, -16777216.0f, 1, 2), SimdMake(-3, 4, -5, 6)));
    Check(actual[0] == 0 && actual[1] == 3 && actual[2] == 1 && actual[3] == 1);
}

// Verifies every float lane, stereo order, PCM range, and exact unaligned transfers.
static void TestFloat8(void)
{
    for (unsigned offset = 0; offset < 64; ++offset)
    {
        unsigned char* input = (unsigned char*)malloc(offset + 8 * 4);
        unsigned char* output = (unsigned char*)malloc(offset + 8 * 4);
        float a[8], b[8], actual[8];
        int16_t pcm16[8], stereo[8 * 2];
        int8_t pcm8[8];
        for (unsigned i = 0; i < 8; ++i)
        {
            a[i] = (float)i - 5.25f;
            b[i] = (float)i * -0.5f;
            pcm16[i] = (int16_t)(i * 4096 - 32768);
            pcm8[i] = (int8_t)(i * 17 - 128);
        }

        memcpy(input + offset, a, sizeof(a));
        dsimd_float8_t va = Simd8Load(input + offset), vb = Simd8Load(b);
        Simd8Store(actual, Simd8Combine(SimdLoad(a + 0), SimdLoad(a + 4)));
        Check(memcmp(actual, a, sizeof(a)) == 0);
        Simd8Store(output + offset, va);
        Check(memcmp(output + offset, a, sizeof(a)) == 0);
        for (unsigned op = 0; op < 5; ++op)
        {
            dsimd_float8_t value = op == 0 ? Simd8Add(va, vb) : op == 1 ? Simd8Mul(va, vb) : op == 2 ? Simd8Min(va, vb) : op == 3 ? Simd8Max(va, vb) : Simd8Ramp(0.5f, -0.25f);
            Simd8Store(actual, value);
            for (unsigned i = 0; i < 8; ++i)
            {
                float expected = op == 0 ? a[i] + b[i] : op == 1 ? a[i] * b[i] : op == 2 ? (a[i] < b[i] ? a[i] : b[i]) : op == 3 ? (a[i] > b[i] ? a[i] : b[i]) : 0.5f - (float)i * 0.25f;
                Check(actual[i] == expected);
                Check(Simd8Lane(value, i) == expected);
            }
        }

        Check(Simd8Lane(Simd8Splat(2), 8 - 1) == 2);
        float sum = 0;
        for (unsigned i = 0; i < 8; ++i)
        {
            sum += a[i];
        }

        Check(Simd8Sum(va) == sum);
        Check(Simd8ReduceMax(va) == a[8 - 1]);
        dsimd_float8_t left, right;
        Simd8Unzip(va, vb, &left, &right);
        Simd8Store(actual, left);
        for (unsigned i = 0; i < 8; ++i)
        {
            Check(actual[i] == (i < 4 ? a[2 * i] : b[2 * i - 8]));
        }

        Simd8Store(actual, right);
        for (unsigned i = 0; i < 8; ++i)
        {
            Check(actual[i] == (i < 4 ? a[2 * i + 1] : b[2 * i - 8 + 1]));
        }

        // PCM loaders must not read beyond their exact input width.
        unsigned char* bytes16 = (unsigned char*)malloc(offset + sizeof(pcm16));
        unsigned char* bytes8 = (unsigned char*)malloc(offset + sizeof(pcm8));
        memcpy(bytes16 + offset, pcm16, sizeof(pcm16));
        memcpy(bytes8 + offset, pcm8, sizeof(pcm8));
        Simd8Store(actual, Simd8LoadS16ToFloat(bytes16 + offset));
        for (unsigned i = 0; i < 8; ++i)
        {
            Check(actual[i] == (float)pcm16[i]);
        }

        Simd8Store(actual, Simd8LoadS8ToS16Float(bytes8 + offset));
        for (unsigned i = 0; i < 8; ++i)
        {
            Check(actual[i] == (float)(pcm8[i] * 256 + (uint8_t)pcm8[i]));
        }

        for (unsigned i = 0; i < 8; ++i)
        {
            a[i] = i % 4 == 0 ? -1.0e20f : i % 4 == 1 ? 1.0e20f : (float)i + 0.75f;
            b[i] = -a[i];
        }

        Simd8StoreInterleavedS16(output + offset, Simd8Load(a), Simd8Load(b));
        memcpy(stereo, output + offset, sizeof(stereo));
        for (unsigned i = 0; i < 8; ++i)
        {
            Check(stereo[2 * i] == (int16_t)(a[i] < -32768 ? -32768 : a[i] > 32767 ? 32767 : a[i]));
            Check(stereo[2 * i + 1] == (int16_t)(b[i] < -32768 ? -32768 : b[i] > 32767 ? 32767 : b[i]));
        }

        free(bytes16);
        free(bytes8);
        free(input);
        free(output);
    }
}

// Verifies every float lane, stereo order, PCM range, and exact unaligned transfers.
static void TestFloat16(void)
{
    for (unsigned offset = 0; offset < 64; ++offset)
    {
        unsigned char* input = (unsigned char*)malloc(offset + 16 * 4);
        unsigned char* output = (unsigned char*)malloc(offset + 16 * 4);
        float a[16], b[16], actual[16];
        int16_t pcm16[16], stereo[16 * 2];
        int8_t pcm8[16];
        for (unsigned i = 0; i < 16; ++i)
        {
            a[i] = (float)i - 5.25f;
            b[i] = (float)i * -0.5f;
            pcm16[i] = (int16_t)(i * 4096 - 32768);
            pcm8[i] = (int8_t)(i * 17 - 128);
        }

        memcpy(input + offset, a, sizeof(a));
        dsimd_float16_t va = Simd16Load(input + offset), vb = Simd16Load(b);
        Simd16Store(actual, Simd16Combine(SimdLoad(a + 0), SimdLoad(a + 4), SimdLoad(a + 8), SimdLoad(a + 12)));
        Check(memcmp(actual, a, sizeof(a)) == 0);
        Simd16Store(output + offset, va);
        Check(memcmp(output + offset, a, sizeof(a)) == 0);
        for (unsigned op = 0; op < 5; ++op)
        {
            dsimd_float16_t value = op == 0 ? Simd16Add(va, vb) : op == 1 ? Simd16Mul(va, vb) : op == 2 ? Simd16Min(va, vb) : op == 3 ? Simd16Max(va, vb) : Simd16Ramp(0.5f, -0.25f);
            Simd16Store(actual, value);
            for (unsigned i = 0; i < 16; ++i)
            {
                float expected = op == 0 ? a[i] + b[i] : op == 1 ? a[i] * b[i] : op == 2 ? (a[i] < b[i] ? a[i] : b[i]) : op == 3 ? (a[i] > b[i] ? a[i] : b[i]) : 0.5f - (float)i * 0.25f;
                Check(actual[i] == expected);
                Check(Simd16Lane(value, i) == expected);
            }
        }

        Check(Simd16Lane(Simd16Splat(2), 16 - 1) == 2);
        float sum = 0;
        for (unsigned i = 0; i < 16; ++i)
        {
            sum += a[i];
        }

        Check(Simd16Sum(va) == sum);
        Check(Simd16ReduceMax(va) == a[16 - 1]);
        dsimd_float16_t left, right;
        Simd16Unzip(va, vb, &left, &right);
        Simd16Store(actual, left);
        for (unsigned i = 0; i < 16; ++i)
        {
            Check(actual[i] == (i < 8 ? a[2 * i] : b[2 * i - 16]));
        }

        Simd16Store(actual, right);
        for (unsigned i = 0; i < 16; ++i)
        {
            Check(actual[i] == (i < 8 ? a[2 * i + 1] : b[2 * i - 16 + 1]));
        }

        // PCM loaders must not read beyond their exact input width.
        unsigned char* bytes16 = (unsigned char*)malloc(offset + sizeof(pcm16));
        unsigned char* bytes8 = (unsigned char*)malloc(offset + sizeof(pcm8));
        memcpy(bytes16 + offset, pcm16, sizeof(pcm16));
        memcpy(bytes8 + offset, pcm8, sizeof(pcm8));
        Simd16Store(actual, Simd16LoadS16ToFloat(bytes16 + offset));
        for (unsigned i = 0; i < 16; ++i)
        {
            Check(actual[i] == (float)pcm16[i]);
        }

        Simd16Store(actual, Simd16LoadS8ToS16Float(bytes8 + offset));
        for (unsigned i = 0; i < 16; ++i)
        {
            Check(actual[i] == (float)(pcm8[i] * 256 + (uint8_t)pcm8[i]));
        }

        for (unsigned i = 0; i < 16; ++i)
        {
            a[i] = i % 4 == 0 ? -1.0e20f : i % 4 == 1 ? 1.0e20f : (float)i + 0.75f;
            b[i] = -a[i];
        }

        Simd16StoreInterleavedS16(output + offset, Simd16Load(a), Simd16Load(b));
        memcpy(stereo, output + offset, sizeof(stereo));
        for (unsigned i = 0; i < 16; ++i)
        {
            Check(stereo[2 * i] == (int16_t)(a[i] < -32768 ? -32768 : a[i] > 32767 ? 32767 : a[i]));
            Check(stereo[2 * i + 1] == (int16_t)(b[i] < -32768 ? -32768 : b[i] > 32767 ? 32767 : b[i]));
        }

        free(bytes16);
        free(bytes8);
        free(input);
        free(output);
    }
}

// Verifies all PCM8 values and clipping/truncation at signed PCM16 boundaries.
static void TestPCM(void)
{
    for (unsigned offset = 0; offset < 32; ++offset)
    {
        uint8_t* input = (uint8_t*)malloc(offset + 4);
        uint8_t* output = (uint8_t*)malloc(offset + 16);
        for (unsigned first = 0; first < 256; first += 4)
        {
            int8_t values[4];
            float actual[4];
            for (unsigned i = 0; i < 4; ++i)
            {
                values[i] = (int8_t)(first + i);
            }

            memcpy(input + offset, values, sizeof(values));
            SimdStore(actual, SimdLoadS8ToS16Float(input + offset));
            for (unsigned i = 0; i < 4; ++i)
            {
                Check(actual[i] == (float)(values[i] * 256 + (uint8_t)values[i]));
            }
        }

        SimdStoreInterleavedS16(output + offset, SimdMake(-1.0e20f, -1.9f, 1.9f, 1.0e20f), SimdMake(32767.9f, -32768.9f, -0.9f, 0.9f));
        int16_t actual[8], expected[8] = {
            -32768, 32767, -1, -32768, 1, 0, 32767, 0
        };
        memcpy(actual, output + offset, sizeof(actual));
        Check(memcmp(actual, expected, sizeof(actual)) == 0);
        free(input);
        free(output);
    }
}

// Verify all lane positions, unsigned wrapping, sign masks and exact unaligned transfers.
static void Test8(void)
{
    for (unsigned offset = 0; offset < 64; ++offset)
    {
        uint8_t* source = (uint8_t*)malloc(offset + 8 * 4);
        uint8_t* destination = (uint8_t*)malloc(offset + 8 * 4);
        for (unsigned trial = 0; trial < 32; ++trial)
        {
            uint32_t a[8], b[8], actual[8];
            for (unsigned i = 0; i < 8; ++i)
            {
                a[i] = (i + 1) * 0x9e3779b9u ^ (trial * 0xa341316cu);
                b[i] = (i + trial) % 3 ? a[i] : ~a[i];
            }

            memcpy(source + offset, a, sizeof(a));
            dsimd_uint8_t va = Simd8LoadUint(source + offset);
            dsimd_uint8_t vb = Simd8LoadUint(b);
            Simd8StoreUint(destination + offset, va);
            Check(memcmp(source + offset, destination + offset, sizeof(a)) == 0);
            Simd8StoreUint(actual, Simd8AndUint(va, vb));
            for (unsigned i = 0; i < 8; ++i)
            {
                Check(actual[i] == (a[i] & b[i]));
            }

            Simd8StoreUint(actual, Simd8OrUint(va, vb));
            for (unsigned i = 0; i < 8; ++i)
            {
                Check(actual[i] == (a[i] | b[i]));
            }

            Simd8StoreUint(actual, Simd8XorUint(va, vb));
            for (unsigned i = 0; i < 8; ++i)
            {
                Check(actual[i] == (a[i] ^ b[i]));
            }

            Simd8StoreUint(actual, Simd8SubUint(va, vb));
            for (unsigned i = 0; i < 8; ++i)
            {
                Check(actual[i] == (a[i] - b[i]));
            }

            Simd8StoreUint(actual, Simd8EqualUint(va, vb));
            for (unsigned i = 0; i < 8; ++i)
            {
                Check(actual[i] == (a[i] == b[i] ? UINT32_MAX : 0u));
            }

            Simd8StoreUint(actual, Simd8NotUint(va));
            for (unsigned i = 0; i < 8; ++i)
            {
                Check(actual[i] == ~a[i]);
            }

            Simd8StoreUint(actual, Simd8SplatUint(trial));
            for (unsigned i = 0; i < 8; ++i)
            {
                Check(actual[i] == trial);
            }

            for (unsigned i = 0; i < 8; ++i)
            {
                actual[i] = 0x80000000u | i;
            }

            Check(Simd8All(Simd8LoadUint(actual)) != 0);
            for (unsigned i = 0; i < 8; ++i)
            {
                actual[i] &= 0x7fffffffu;
                Check(Simd8All(Simd8LoadUint(actual)) == 0);
                actual[i] |= 0x80000000u;
            }
        }

        free(source);
        free(destination);
    }
}

// Verify all lane positions, unsigned wrapping, sign masks and exact unaligned transfers.
static void Test16(void)
{
    for (unsigned offset = 0; offset < 64; ++offset)
    {
        uint8_t* source = (uint8_t*)malloc(offset + 16 * 4);
        uint8_t* destination = (uint8_t*)malloc(offset + 16 * 4);
        for (unsigned trial = 0; trial < 32; ++trial)
        {
            uint32_t a[16], b[16], actual[16];
            for (unsigned i = 0; i < 16; ++i)
            {
                a[i] = (i + 1) * 0x9e3779b9u ^ (trial * 0xa341316cu);
                b[i] = (i + trial) % 3 ? a[i] : ~a[i];
            }

            memcpy(source + offset, a, sizeof(a));
            dsimd_uint16_t va = Simd16LoadUint(source + offset);
            dsimd_uint16_t vb = Simd16LoadUint(b);
            Simd16StoreUint(destination + offset, va);
            Check(memcmp(source + offset, destination + offset, sizeof(a)) == 0);
            Simd16StoreUint(actual, Simd16AndUint(va, vb));
            for (unsigned i = 0; i < 16; ++i)
            {
                Check(actual[i] == (a[i] & b[i]));
            }

            Simd16StoreUint(actual, Simd16OrUint(va, vb));
            for (unsigned i = 0; i < 16; ++i)
            {
                Check(actual[i] == (a[i] | b[i]));
            }

            Simd16StoreUint(actual, Simd16XorUint(va, vb));
            for (unsigned i = 0; i < 16; ++i)
            {
                Check(actual[i] == (a[i] ^ b[i]));
            }

            Simd16StoreUint(actual, Simd16SubUint(va, vb));
            for (unsigned i = 0; i < 16; ++i)
            {
                Check(actual[i] == (a[i] - b[i]));
            }

            Simd16StoreUint(actual, Simd16EqualUint(va, vb));
            for (unsigned i = 0; i < 16; ++i)
            {
                Check(actual[i] == (a[i] == b[i] ? UINT32_MAX : 0u));
            }

            Simd16StoreUint(actual, Simd16NotUint(va));
            for (unsigned i = 0; i < 16; ++i)
            {
                Check(actual[i] == ~a[i]);
            }

            Simd16StoreUint(actual, Simd16SplatUint(trial));
            for (unsigned i = 0; i < 16; ++i)
            {
                Check(actual[i] == trial);
            }

            for (unsigned i = 0; i < 16; ++i)
            {
                actual[i] = 0x80000000u | i;
            }

            Check(Simd16All(Simd16LoadUint(actual)) != 0);
            for (unsigned i = 0; i < 16; ++i)
            {
                actual[i] &= 0x7fffffffu;
                Check(Simd16All(Simd16LoadUint(actual)) == 0);
                actual[i] |= 0x80000000u;
            }
        }

        free(source);
        free(destination);
    }
}

// Verifies exact partial sixteen-lane reads, zero-filled tails and null zero-count input.
static void TestPartial16Loads(void)
{
    float output[16];
    for (unsigned count = 0; count <= 16; ++count)
    {
        float* storage = (float*)malloc((count + 1) * sizeof(float));
        float* source = storage + 1;
        for (unsigned i = 0; i < count; ++i)
        {
            source[i] = (float)i - 8.25f;
        }

        Simd16Store(output, Simd16LoadPartial(source, count));
        for (unsigned i = 0; i < 16; ++i)
        {
            Check(output[i] == (i < count ? source[i] : 0.0f));
        }

        free(storage);
    }

    Simd16Store(output, Simd16LoadPartial(0, 0));
    for (unsigned i = 0; i < 16; ++i)
    {
        Check(output[i] == 0.0f);
    }
}

// Runs the restored integer comparisons and the audio float/PCM checks.
int SimdTestFixed(void)
{
    TestPartial16Loads();
    TestRamp();
    TestUnzip();
    TestPairwiseAdd();
    TestPCM();
    TestFloat8();
    TestFloat16();
    Test8();
    Test16();
    printf("{\"fixed_lane_checks\":%u}\n", g_Checks);
    return 0;
}
