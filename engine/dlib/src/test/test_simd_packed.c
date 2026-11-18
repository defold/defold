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

#if defined(DM_SIMD_TEST_EMULATED)
#include "test_simd_avx_emulate.h"
#endif
#include "simde/simde.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if DM_SIMD_TEST_AVX_BITS == 256
#include <simd/simd256.h>
#define SIMD(name) Simd256##name
#define REF(name) simde_mm256_##name
#define REF_LOAD simde_mm256_loadu_si256
#define REF_ZERO simde_mm256_setzero_si256
#define REF_OR simde_mm256_or_si256
typedef dsimd_byte32_t   TestBytes;
typedef dsimd_ushort16_t TestWords;
typedef dsimd_uint8_t    TestUint;
typedef dsimd_float8_t   TestFloat;
typedef simde__m256i     ReferenceInt;
typedef simde__m256      ReferenceFloat;
#elif DM_SIMD_TEST_AVX_BITS == 512
#include <simd/simd512.h>
#define SIMD(name) Simd512##name
#define REF(name) simde_mm512_##name
#define REF_LOAD simde_mm512_loadu_si512
#define REF_ZERO simde_mm512_setzero_si512
#define REF_OR simde_mm512_or_si512
typedef dsimd_byte64_t   TestBytes;
typedef dsimd_ushort32_t TestWords;
typedef dsimd_uint16_t   TestUint;
typedef dsimd_float16_t  TestFloat;
typedef simde__m512i     ReferenceInt;
typedef simde__m512      ReferenceFloat;
#else
#error Fixed-width tests require 256 or 512 bits
#endif

enum
{
    VECTOR_BYTES = DM_SIMD_TEST_AVX_BITS / 8
};

static unsigned g_Checks;
static unsigned g_Failures;

static void Check(int passed, const char* expression, int line)
{
    ++g_Checks;
    if (!passed)
    {
        if (g_Failures < 20)
        {
            fprintf(stderr, "Line %d: %s\n", line, expression);
        }

        ++g_Failures;
    }
}

#define CHECK(condition) Check((condition), #condition, __LINE__)

static uint32_t Bits(float value)
{
    uint32_t bits;
    memcpy(&bits, &value, sizeof(bits));
    return bits;
}

static float FloatBits(uint32_t bits)
{
    float value;
    memcpy(&value, &bits, sizeof(value));
    return value;
}

static uint32_t RandomBits(uint32_t* state)
{
    *state = *state * 1664525u + 1013904223u;
    return *state;
}

// Checks block ordering, unsigned boundaries and every unaligned transfer.
static void TestTransfersAndBoundaries(void)
{
    uint8_t  source[VECTOR_BYTES + 65], output[sizeof(source)], indices[VECTOR_BYTES];
    uint16_t a[VECTOR_BYTES / 2], b[VECTOR_BYTES / 2], words[VECTOR_BYTES / 2];
    uint32_t integers[VECTOR_BYTES / 4];
    float    floats[VECTOR_BYTES / 4], other[VECTOR_BYTES / 4], values[VECTOR_BYTES / 4];
    unsigned i, offset, count;
    CHECK(sizeof(TestBytes) == VECTOR_BYTES);
    CHECK(sizeof(TestFloat) == VECTOR_BYTES);
    for (i = 0; i < sizeof(source); ++i)
    {
        source[i] = (uint8_t)(i * 37 + 11);
    }

    for (offset = 0; offset < 64; ++offset)
    {
        memset(output, 0xa5, sizeof(output));
        SIMD(StoreBytes)(output + offset + 1, SIMD(LoadBytes)(source + offset + 1));
        CHECK(memcmp(output + offset + 1, source + offset + 1, VECTOR_BYTES) == 0);
        CHECK(output[offset] == 0xa5 && output[offset + 1 + VECTOR_BYTES] == 0xa5);
        SIMD(Store)(output + offset + 1, SIMD(Load)(source + offset + 1));
        CHECK(memcmp(output + offset + 1, source + offset + 1, VECTOR_BYTES) == 0);
        {
            uint8_t* allocation = (uint8_t*)malloc(offset + 1 + VECTOR_BYTES);
            memcpy(allocation + offset + 1, source, VECTOR_BYTES);
            SIMD(StoreBytes)(allocation + offset + 1, SIMD(LoadBytes)(allocation + offset + 1));
            SIMD(Store)(allocation + offset + 1, SIMD(Load)(allocation + offset + 1));
            free(allocation);
        }
    }

    SIMD(StoreBytes)(output, SIMD(UintToBytes)(SIMD(BytesToUint)(SIMD(LoadBytes)(source))));
    CHECK(memcmp(source, output, VECTOR_BYTES) == 0);
    SIMD(StoreBytes)(integers, SIMD(UintToBytes)(SIMD(OrUint)(SIMD(SplatUint)(0x12345678u), SIMD(SplatUint)(0x87654321u))));
    for (i = 0; i < VECTOR_BYTES / 4; ++i)
    {
        CHECK(integers[i] == (0x12345678u | 0x87654321u));
    }

    SIMD(StoreBytes)(words, SIMD(PackBytes)(SIMD(WidenBytesLow)(SIMD(LoadBytes)(source)), SIMD(WidenBytesHigh)(SIMD(LoadBytes)(source))));
    CHECK(memcmp(words, source, VECTOR_BYTES) == 0);
    for (i = 0; i < VECTOR_BYTES / 2; ++i)
    {
        a[i] = (uint16_t)(65535 - i * 9001);
        b[i] = (uint16_t)(i * 19311);
    }

#define CHECK_WORDS(function, reference) \
    { \
        TestWords result = function; \
        memcpy(words, &result, VECTOR_BYTES); \
        for (i = 0; i < VECTOR_BYTES / 2; ++i) \
            CHECK(words[i] == (uint16_t)(reference)); \
    }
    CHECK_WORDS(SIMD(SplatU16)(65535), 65535);
    {
        TestWords x, y;
        memcpy(&x, a, VECTOR_BYTES);
        memcpy(&y, b, VECTOR_BYTES);
        CHECK_WORDS(SIMD(AddU16)(x, y), (uint32_t)a[i] + b[i]);
        CHECK_WORDS(SIMD(MulU16)(x, y), (uint32_t)a[i] * b[i]);
        for (count = 0; count <= 17; ++count)
        {
            CHECK_WORDS(SIMD(ShiftRightU16)(x, count), count < 16 ? a[i] >> count : 0);
        }

        SIMD(StoreBytes)(output, SIMD(PackBytes)(x, y));
        for (i = 0; i < VECTOR_BYTES; ++i)
        {
            unsigned block = i / 16, lane = i % 16;
            uint16_t value = lane < 8 ? a[block * 8 + lane] : b[block * 8 + lane - 8];
            CHECK(output[i] == (value > 255 ? 255 : value));
        }
    }
#undef CHECK_WORDS
    for (count = 0; count < 256; ++count)
    {
        for (i = 0; i < VECTOR_BYTES; ++i)
        {
            indices[i] = (uint8_t)(count + i);
        }

        SIMD(StoreBytes)(output, SIMD(ShuffleBytes)(SIMD(LoadBytes)(source), SIMD(LoadBytes)(indices)));
        for (i = 0; i < VECTOR_BYTES; ++i)
        {
            CHECK(output[i] == (indices[i] < 16 ? source[(i / 16) * 16 + indices[i]] : 0));
        }
    }

    for (i = 0; i < VECTOR_BYTES / 4; ++i)
    {
        floats[i] = (float)i - 8.0f;
        other[i] = (float)(i + 1) * 0.25f;
    }

    SIMD(Store)(values, SIMD(Splat)(-3.5f));
    for (i = 0; i < VECTOR_BYTES / 4; ++i)
    {
        CHECK(values[i] == -3.5f);
    }

    SIMD(Store)(values, SIMD(Add)(SIMD(Load)(floats), SIMD(Load)(other)));
    for (i = 0; i < VECTOR_BYTES / 4; ++i)
    {
        CHECK(values[i] == floats[i] + other[i]);
    }

    SIMD(Store)(values, SIMD(Mul)(SIMD(Load)(floats), SIMD(Load)(other)));
    for (i = 0; i < VECTOR_BYTES / 4; ++i)
    {
        CHECK(values[i] == floats[i] * other[i]);
    }

    // Preserve signed zeros and non-finite behavior without requiring identical NaN payloads.
    {
        const uint32_t bits[] = {
            0, 0x80000000u, 0x3f800000u, 0xbf800000u, 0x7f800000u, 0xff800000u, 0x7fc12345u, 0x7f7fffffu
        };
        for (count = 0; count < 8; ++count)
        {
            for (i = 0; i < VECTOR_BYTES / 4; ++i)
            {
                floats[i] = FloatBits(bits[i % 8]);
                other[i] = FloatBits(bits[(i + count) % 8]);
            }

            SIMD(Store)(values, SIMD(Splat)(FloatBits(bits[count])));
            for (i = 0; i < VECTOR_BYTES / 4; ++i)
            {
                CHECK(Bits(values[i]) == bits[count]);
            }

            SIMD(Store)(values, SIMD(Add)(SIMD(Load)(floats), SIMD(Load)(other)));
            for (i = 0; i < VECTOR_BYTES / 4; ++i)
            {
                float expected = floats[i] + other[i];
                CHECK(isnan(expected) ? isnan(values[i]) : Bits(values[i]) == Bits(expected));
            }

            SIMD(Store)(values, SIMD(Mul)(SIMD(Load)(floats), SIMD(Load)(other)));
            for (i = 0; i < VECTOR_BYTES / 4; ++i)
            {
                float expected = floats[i] * other[i];
                CHECK(isnan(expected) ? isnan(values[i]) : Bits(values[i]) == Bits(expected));
            }
        }
    }
}

// Compares both float arithmetic and packed lane operations with portable SIMDe.
static void TestReference(void)
{
    uint32_t state = 0x683412u;
    unsigned iteration, lane, count;
    for (iteration = 0; iteration < 128; ++iteration)
    {
        uint8_t  bytes[VECTOR_BYTES], indices[VECTOR_BYTES], normalized[VECTOR_BYTES], output[VECTOR_BYTES];
        uint16_t av[VECTOR_BYTES / 2], bv[VECTOR_BYTES / 2];
        float    fv[VECTOR_BYTES / 4], gv[VECTOR_BYTES / 4];
        for (lane = 0; lane < VECTOR_BYTES; ++lane)
        {
            bytes[lane] = (uint8_t)RandomBits(&state);
            indices[lane] = (uint8_t)(iteration * 7 + lane);
            normalized[lane] = indices[lane] < 16 ? indices[lane] : 0x80;
        }

        for (lane = 0; lane < VECTOR_BYTES / 2; ++lane)
        {
            av[lane] = (uint16_t)RandomBits(&state);
            bv[lane] = (uint16_t)RandomBits(&state);
        }

        for (lane = 0; lane < VECTOR_BYTES / 4; ++lane)
        {
            fv[lane] = (float)(lane + iteration) - 8.0f;
            gv[lane] = (float)(lane + 1) * 0.25f;
        }

        TestBytes   bytes_value = SIMD(LoadBytes)(bytes);
        TestWords a, b;
        memcpy(&a, av, VECTOR_BYTES);
        memcpy(&b, bv, VECTOR_BYTES);
        ReferenceInt   ra = REF_LOAD(av), rb = REF_LOAD(bv), rbytes = REF_LOAD(bytes);
        ReferenceFloat rf = REF(loadu_ps)(fv), rg = REF(loadu_ps)(gv);
#define COMPARE_REGISTER(type, ref_type, name, args, reference) \
    do \
    { \
        type actual = name args; \
        ref_type           expected = reference; \
        uint8_t            left[VECTOR_BYTES], right[VECTOR_BYTES]; \
        unsigned           byte; \
        memcpy(left, &actual, VECTOR_BYTES); \
        memcpy(right, &expected, VECTOR_BYTES); \
        for (byte = 0; byte < VECTOR_BYTES; ++byte) \
            Check(left[byte] == right[byte], #name, __LINE__); \
    } while (0)
        COMPARE_REGISTER(TestBytes, ReferenceInt, SIMD(LoadBytes), (bytes), rbytes);
        COMPARE_REGISTER(TestUint, ReferenceInt, SIMD(BytesToUint), (bytes_value), rbytes);
        COMPARE_REGISTER(TestBytes, ReferenceInt, SIMD(UintToBytes), (SIMD(BytesToUint)(bytes_value)), rbytes);
        COMPARE_REGISTER(TestUint, ReferenceInt, SIMD(SplatUint), (0x87654321u), REF(set1_epi32)((int32_t)0x87654321u));
        COMPARE_REGISTER(TestUint, ReferenceInt, SIMD(OrUint), (SIMD(BytesToUint)(bytes_value), SIMD(SplatUint)(0x87654321u)), REF_OR(rbytes, REF(set1_epi32)((int32_t)0x87654321u)));
        COMPARE_REGISTER(TestWords, ReferenceInt, SIMD(SplatU16), (65535), REF(set1_epi16)(-1));
        COMPARE_REGISTER(TestWords, ReferenceInt, SIMD(AddU16), (a, b), REF(add_epi16)(ra, rb));
        COMPARE_REGISTER(TestWords, ReferenceInt, SIMD(MulU16), (a, b), REF(mullo_epi16)(ra, rb));
        COMPARE_REGISTER(TestWords, ReferenceInt, SIMD(WidenBytesLow), (bytes_value), REF(unpacklo_epi8)(rbytes, REF_ZERO()));
        COMPARE_REGISTER(TestWords, ReferenceInt, SIMD(WidenBytesHigh), (bytes_value), REF(unpackhi_epi8)(rbytes, REF_ZERO()));
        COMPARE_REGISTER(TestBytes, ReferenceInt, SIMD(PackBytes), (a, b), REF(packus_epi16)(REF(min_epu16)(ra, REF(set1_epi16)(255)), REF(min_epu16)(rb, REF(set1_epi16)(255))));
        COMPARE_REGISTER(TestBytes, ReferenceInt, SIMD(ShuffleBytes), (bytes_value, SIMD(LoadBytes)(indices)), REF(shuffle_epi8)(rbytes, REF_LOAD(normalized)));
        for (count = 0; count <= 17; ++count)
        {
            COMPARE_REGISTER(TestWords, ReferenceInt, SIMD(ShiftRightU16), (a, count), REF(srl_epi16)(ra, simde_mm_cvtsi32_si128((int32_t)count)));
        }

        COMPARE_REGISTER(TestFloat, ReferenceFloat, SIMD(Load), (fv), rf);
        COMPARE_REGISTER(TestFloat, ReferenceFloat, SIMD(Splat), (-3.5f), REF(set1_ps)(-3.5f));
        COMPARE_REGISTER(TestFloat, ReferenceFloat, SIMD(Add), (SIMD(Load)(fv), SIMD(Load)(gv)), REF(add_ps)(rf, rg));
        COMPARE_REGISTER(TestFloat, ReferenceFloat, SIMD(Mul), (SIMD(Load)(fv), SIMD(Load)(gv)), REF(mul_ps)(rf, rg));
        SIMD(StoreBytes)(output, bytes_value);
        for (lane = 0; lane < VECTOR_BYTES; ++lane)
        {
            CHECK(output[lane] == bytes[lane]);
        }

        SIMD(Store)(output, SIMD(Load)(fv));
        for (lane = 0; lane < VECTOR_BYTES; ++lane)
        {
            CHECK(output[lane] == ((uint8_t*)fv)[lane]);
        }

#undef COMPARE_REGISTER
    }
}

// Checks the fixed-width primitives shared by the packed pixel kernels.
int SimdTestPacked(void)
{
    TestTransfersAndBoundaries();
    TestReference();
    printf("SIMD%d packed primitives: %u checks, %u failures\n", DM_SIMD_TEST_AVX_BITS, g_Checks, g_Failures);
    return g_Failures != 0;
}

#if defined(DM_SIMD_TEST_EMULATED)
int main(void)
{
    return SimdTestPacked();
}

#endif
