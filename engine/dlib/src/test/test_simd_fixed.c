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

#include <simd/simd_128.h>
#include <simd/simd256.h>
#include <simd/simd512.h>
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
        Simd128Store(actual, Simd128Ramp(starts[trial], steps[trial]));
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
    Simd128Unzip(Simd128Load(bits), Simd128Load(bits + 4), &left, &right);
    Simd128Store(actual_left, left);
    Simd128Store(actual_right, right);
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
    Simd128Store(actual, Simd128PairwiseAdd(Simd128Make(1, 2, 4, 8), Simd128Make(-1, -2, -4, -8)));
    Check(actual[0] == 3 && actual[1] == 12 && actual[2] == -3 && actual[3] == -12);
    Simd128Store(actual, Simd128PairwiseAdd(Simd128Make(16777216.0f, -16777216.0f, 1, 2), Simd128Make(-3, 4, -5, 6)));
    Check(actual[0] == 0 && actual[1] == 3 && actual[2] == 1 && actual[3] == 1);
}

#if !DM_SIMD_FORCE_SCALAR && defined(DM_SIMD_AVX2)
// Verifies batched sums preserve input order, cancellation and the existing scalar reduction bits.
static void TestSum4Float8(void)
{
    const float samples[8][8] = {
        {
            1, 2, 4, 8, 16, 32, 64, 128
        },
        {
            16777216.0f, 1, -16777216.0f, 2, 0, 0, 0, 0
        },
        {
            16777216.0f, -16777216.0f, 1, 2, 0, 0, 0, 0
        },
        {
            16777216.0f, 1, 2, 3, -16777216.0f, 4, 5, 6
        },
        {
            -0.0f, -0.0f, -0.0f, -0.0f, -0.0f, -0.0f, -0.0f, -0.0f
        },
        {
            -1, -2, -4, -8, -16, -32, -64, -128
        },
        {
            1.25f, -2.5f, 3.75f, -4.125f, 5.5f, -6.25f, 7.875f, -8.0625f
        },
        {
            1.0e20f, -1.0e20f, 1, -2, -1.0e20f, 1.0e20f, 3, -4
        }
    };
    for (unsigned trial = 0; trial < 8; ++trial)
    {
        dsimd_float8_t input[4];
        float actual[4], expected[4];
        for (unsigned i = 0; i < 4; ++i)
        {
            input[i] = Simd256Load(samples[(trial + i) & 7]);
            expected[i] = Simd256Sum(input[i]);
        }

        Simd128Store(actual, Simd256Sum4(input[0], input[1], input[2], input[3]));
        Check(memcmp(actual, expected, sizeof(actual)) == 0);
    }
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
        dsimd_float8_t va = Simd256Load(input + offset), vb = Simd256Load(b);
        Simd256Store(actual, Simd256Combine(Simd128Load(a + 0), Simd128Load(a + 4)));
        Check(memcmp(actual, a, sizeof(a)) == 0);
        Simd256Store(output + offset, va);
        Check(memcmp(output + offset, a, sizeof(a)) == 0);
        for (unsigned op = 0; op < 5; ++op)
        {
            dsimd_float8_t value = op == 0 ? Simd256Add(va, vb) : op == 1 ? Simd256Mul(va, vb) : op == 2 ? Simd256Min(va, vb) : op == 3 ? Simd256Max(va, vb) : Simd256Ramp(0.5f, -0.25f);
            Simd256Store(actual, value);
            for (unsigned i = 0; i < 8; ++i)
            {
                float expected = op == 0 ? a[i] + b[i] : op == 1 ? a[i] * b[i] : op == 2 ? (a[i] < b[i] ? a[i] : b[i]) : op == 3 ? (a[i] > b[i] ? a[i] : b[i]) : 0.5f - (float)i * 0.25f;
                Check(actual[i] == expected);
                Check(Simd256Lane(value, i) == expected);
            }
        }

        Check(Simd256Lane(Simd256Splat(2), 8 - 1) == 2);
        float sum = 0;
        for (unsigned i = 0; i < 8; ++i)
        {
            sum += a[i];
        }

        Check(Simd256Sum(va) == sum);
        Check(Simd256ReduceMax(va) == a[8 - 1]);
        dsimd_float8_t left, right;
        Simd256Unzip(va, vb, &left, &right);
        Simd256Store(actual, left);
        for (unsigned i = 0; i < 8; ++i)
        {
            Check(actual[i] == (i < 4 ? a[2 * i] : b[2 * i - 8]));
        }

        Simd256Store(actual, right);
        for (unsigned i = 0; i < 8; ++i)
        {
            Check(actual[i] == (i < 4 ? a[2 * i + 1] : b[2 * i - 8 + 1]));
        }

        // PCM loaders must not read beyond their exact input width.
        unsigned char* bytes16 = (unsigned char*)malloc(offset + sizeof(pcm16));
        unsigned char* bytes8 = (unsigned char*)malloc(offset + sizeof(pcm8));
        memcpy(bytes16 + offset, pcm16, sizeof(pcm16));
        memcpy(bytes8 + offset, pcm8, sizeof(pcm8));
        Simd256Store(actual, Simd256LoadS16ToFloat(bytes16 + offset));
        for (unsigned i = 0; i < 8; ++i)
        {
            Check(actual[i] == (float)pcm16[i]);
        }

        Simd256Store(actual, Simd256LoadS8ToS16Float(bytes8 + offset));
        for (unsigned i = 0; i < 8; ++i)
        {
            Check(actual[i] == (float)(pcm8[i] * 256 + (uint8_t)pcm8[i]));
        }

        for (unsigned i = 0; i < 8; ++i)
        {
            a[i] = i % 4 == 0 ? -1.0e20f : i % 4 == 1 ? 1.0e20f : (float)i + 0.75f;
            b[i] = -a[i];
        }

        Simd256StoreInterleavedS16(output + offset, Simd256Load(a), Simd256Load(b));
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

#endif

#if !DM_SIMD_FORCE_SCALAR && defined(DM_SIMD_AVX512)
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
        dsimd_float16_t va = Simd512Load(input + offset), vb = Simd512Load(b);
        Simd512Store(actual, Simd512Combine(Simd128Load(a + 0), Simd128Load(a + 4), Simd128Load(a + 8), Simd128Load(a + 12)));
        Check(memcmp(actual, a, sizeof(a)) == 0);
        Simd512Store(output + offset, va);
        Check(memcmp(output + offset, a, sizeof(a)) == 0);
        for (unsigned op = 0; op < 5; ++op)
        {
            dsimd_float16_t value = op == 0 ? Simd512Add(va, vb) : op == 1 ? Simd512Mul(va, vb) : op == 2 ? Simd512Min(va, vb) : op == 3 ? Simd512Max(va, vb) : Simd512Ramp(0.5f, -0.25f);
            Simd512Store(actual, value);
            for (unsigned i = 0; i < 16; ++i)
            {
                float expected = op == 0 ? a[i] + b[i] : op == 1 ? a[i] * b[i] : op == 2 ? (a[i] < b[i] ? a[i] : b[i]) : op == 3 ? (a[i] > b[i] ? a[i] : b[i]) : 0.5f - (float)i * 0.25f;
                Check(actual[i] == expected);
                Check(Simd512Lane(value, i) == expected);
            }
        }

        Check(Simd512Lane(Simd512Splat(2), 16 - 1) == 2);
        float sum = 0;
        for (unsigned i = 0; i < 16; ++i)
        {
            sum += a[i];
        }

        Check(Simd512Sum(va) == sum);
        Check(Simd512ReduceMax(va) == a[16 - 1]);
        dsimd_float16_t left, right;
        Simd512Unzip(va, vb, &left, &right);
        Simd512Store(actual, left);
        for (unsigned i = 0; i < 16; ++i)
        {
            Check(actual[i] == (i < 8 ? a[2 * i] : b[2 * i - 16]));
        }

        Simd512Store(actual, right);
        for (unsigned i = 0; i < 16; ++i)
        {
            Check(actual[i] == (i < 8 ? a[2 * i + 1] : b[2 * i - 16 + 1]));
        }

        // PCM loaders must not read beyond their exact input width.
        unsigned char* bytes16 = (unsigned char*)malloc(offset + sizeof(pcm16));
        unsigned char* bytes8 = (unsigned char*)malloc(offset + sizeof(pcm8));
        memcpy(bytes16 + offset, pcm16, sizeof(pcm16));
        memcpy(bytes8 + offset, pcm8, sizeof(pcm8));
        Simd512Store(actual, Simd512LoadS16ToFloat(bytes16 + offset));
        for (unsigned i = 0; i < 16; ++i)
        {
            Check(actual[i] == (float)pcm16[i]);
        }

        Simd512Store(actual, Simd512LoadS8ToS16Float(bytes8 + offset));
        for (unsigned i = 0; i < 16; ++i)
        {
            Check(actual[i] == (float)(pcm8[i] * 256 + (uint8_t)pcm8[i]));
        }

        for (unsigned i = 0; i < 16; ++i)
        {
            a[i] = i % 4 == 0 ? -1.0e20f : i % 4 == 1 ? 1.0e20f : (float)i + 0.75f;
            b[i] = -a[i];
        }

        Simd512StoreInterleavedS16(output + offset, Simd512Load(a), Simd512Load(b));
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

#endif

// Verifies signed PCM8 expansion in each native-byte-vector quartet and exact unaligned loads.
static void TestPCMQuartets(void)
{
    for (unsigned offset = 0; offset < 32; ++offset)
    {
        uint8_t* storage = (uint8_t*)malloc(offset + 16);
        for (unsigned start = 0; start < 256; start += 16)
        {
            for (unsigned i = 0; i < 16; ++i)
            {
                storage[offset + i] = (uint8_t)(start + i);
            }

            dsimd_byte16_t bytes = Simd128LoadBytes(storage + offset);
            for (unsigned quartet = 0; quartet < 4; ++quartet)
            {
                float actual[4];
                Simd128Store(actual, Simd128ConvertS8ToS16Float(bytes, quartet));
                for (unsigned lane = 0; lane < 4; ++lane)
                {
                    uint8_t value = storage[offset + quartet * 4 + lane];
                    int expected = (int)(int8_t)value * 256 + value;
                    Check(actual[lane] == (float)expected);
                }
            }
        }

        free(storage);
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
            Simd128Store(actual, Simd128LoadS8ToS16Float(input + offset));
            for (unsigned i = 0; i < 4; ++i)
            {
                Check(actual[i] == (float)(values[i] * 256 + (uint8_t)values[i]));
            }
        }

        Simd128StoreInterleavedS16(output + offset, Simd128Make(-1.0e20f, -1.9f, 1.9f, 1.0e20f), Simd128Make(32767.9f, -32768.9f, -0.9f, 0.9f));
        int16_t actual[8], expected[8] = {
            -32768, 32767, -1, -32768, 1, 0, 32767, 0
        };
        memcpy(actual, output + offset, sizeof(actual));
        Check(memcmp(actual, expected, sizeof(actual)) == 0);
        free(input);
        free(output);
    }
}

#if !DM_SIMD_FORCE_SCALAR && defined(DM_SIMD_AVX2)
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
            dsimd_uint8_t va = Simd256LoadUint(source + offset);
            dsimd_uint8_t vb = Simd256LoadUint(b);
            Simd256StoreUint(destination + offset, va);
            Check(memcmp(source + offset, destination + offset, sizeof(a)) == 0);
            Simd256StoreUint(actual, Simd256AndUint(va, vb));
            for (unsigned i = 0; i < 8; ++i)
            {
                Check(actual[i] == (a[i] & b[i]));
            }

            Simd256StoreUint(actual, Simd256OrUint(va, vb));
            for (unsigned i = 0; i < 8; ++i)
            {
                Check(actual[i] == (a[i] | b[i]));
            }

            Simd256StoreUint(actual, Simd256XorUint(va, vb));
            for (unsigned i = 0; i < 8; ++i)
            {
                Check(actual[i] == (a[i] ^ b[i]));
            }

            Simd256StoreUint(actual, Simd256SubUint(va, vb));
            for (unsigned i = 0; i < 8; ++i)
            {
                Check(actual[i] == (a[i] - b[i]));
            }

            Simd256StoreUint(actual, Simd256EqualUint(va, vb));
            for (unsigned i = 0; i < 8; ++i)
            {
                Check(actual[i] == (a[i] == b[i] ? UINT32_MAX : 0u));
            }

            Simd256StoreUint(actual, Simd256NotUint(va));
            for (unsigned i = 0; i < 8; ++i)
            {
                Check(actual[i] == ~a[i]);
            }

            Simd256StoreUint(actual, Simd256SplatUint(trial));
            for (unsigned i = 0; i < 8; ++i)
            {
                Check(actual[i] == trial);
            }

            for (unsigned i = 0; i < 8; ++i)
            {
                actual[i] = 0x80000000u | i;
            }

            Check(Simd256All(Simd256LoadUint(actual)) != 0);
            for (unsigned i = 0; i < 8; ++i)
            {
                actual[i] &= 0x7fffffffu;
                Check(Simd256All(Simd256LoadUint(actual)) == 0);
                actual[i] |= 0x80000000u;
            }
        }

        free(source);
        free(destination);
    }
}

#endif

#if !DM_SIMD_FORCE_SCALAR && defined(DM_SIMD_AVX512)
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
            dsimd_uint16_t va = Simd512LoadUint(source + offset);
            dsimd_uint16_t vb = Simd512LoadUint(b);
            Simd512StoreUint(destination + offset, va);
            Check(memcmp(source + offset, destination + offset, sizeof(a)) == 0);
            Simd512StoreUint(actual, Simd512AndUint(va, vb));
            for (unsigned i = 0; i < 16; ++i)
            {
                Check(actual[i] == (a[i] & b[i]));
            }

            Simd512StoreUint(actual, Simd512OrUint(va, vb));
            for (unsigned i = 0; i < 16; ++i)
            {
                Check(actual[i] == (a[i] | b[i]));
            }

            Simd512StoreUint(actual, Simd512XorUint(va, vb));
            for (unsigned i = 0; i < 16; ++i)
            {
                Check(actual[i] == (a[i] ^ b[i]));
            }

            Simd512StoreUint(actual, Simd512SubUint(va, vb));
            for (unsigned i = 0; i < 16; ++i)
            {
                Check(actual[i] == (a[i] - b[i]));
            }

            Simd512StoreUint(actual, Simd512EqualUint(va, vb));
            for (unsigned i = 0; i < 16; ++i)
            {
                Check(actual[i] == (a[i] == b[i] ? UINT32_MAX : 0u));
            }

            Simd512StoreUint(actual, Simd512NotUint(va));
            for (unsigned i = 0; i < 16; ++i)
            {
                Check(actual[i] == ~a[i]);
            }

            Simd512StoreUint(actual, Simd512SplatUint(trial));
            for (unsigned i = 0; i < 16; ++i)
            {
                Check(actual[i] == trial);
            }

            for (unsigned i = 0; i < 16; ++i)
            {
                actual[i] = 0x80000000u | i;
            }

            Check(Simd512All(Simd512LoadUint(actual)) != 0);
            for (unsigned i = 0; i < 16; ++i)
            {
                actual[i] &= 0x7fffffffu;
                Check(Simd512All(Simd512LoadUint(actual)) == 0);
                actual[i] |= 0x80000000u;
            }
        }

        free(source);
        free(destination);
    }
}

#endif

#if !DM_SIMD_FORCE_SCALAR && defined(DM_SIMD_AVX512)
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

        Simd512Store(output, Simd512LoadPartial(source, count));
        for (unsigned i = 0; i < 16; ++i)
        {
            Check(output[i] == (i < count ? source[i] : 0.0f));
        }

        free(storage);
    }

    Simd512Store(output, Simd512LoadPartial(0, 0));
    for (unsigned i = 0; i < 16; ++i)
    {
        Check(output[i] == 0.0f);
    }
}

#endif

// Runs the restored integer comparisons and the audio float/PCM checks.
int SimdTestFixed(void)
{
#if !DM_SIMD_FORCE_SCALAR && defined(DM_SIMD_AVX512)
    TestPartial16Loads();
#endif
    TestRamp();
    TestUnzip();
    TestPairwiseAdd();
    TestPCM();
    TestPCMQuartets();
#if !DM_SIMD_FORCE_SCALAR && defined(DM_SIMD_AVX2)
    TestFloat8();
    TestSum4Float8();
#endif
#if !DM_SIMD_FORCE_SCALAR && defined(DM_SIMD_AVX512)
    TestFloat16();
#endif
#if !DM_SIMD_FORCE_SCALAR && defined(DM_SIMD_AVX2)
    Test8();
#endif
#if !DM_SIMD_FORCE_SCALAR && defined(DM_SIMD_AVX512)
    Test16();
#endif
    printf("{\"fixed_lane_checks\":%u}\n", g_Checks);
    return 0;
}
