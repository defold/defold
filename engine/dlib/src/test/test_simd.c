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

#include <simd/simd.h>
#include <float.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned g_Checks;
static unsigned g_Failures;

#define CHECK(expression) \
    do \
    { \
        ++g_Checks; \
        if (!(expression)) \
        { \
            printf("%s:%d: %s\n", __FILE__, __LINE__, #expression); \
            ++g_Failures; \
        } \
    } while (0)

static void Near(double expected, float actual, double tolerance)
{
    CHECK(fabs(expected - (double)actual) <= tolerance);
}

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

// Checks construction and every arithmetic lane against fixed independent fixtures.
static void TestArithmetic(void)
{
    static const struct
    {
        float a[4], b[4], add[4], sub[4], mul[4], div[4], root[4];
    } cases[] = {
        { { 1, 4, 9, 16 }, { 2, 2, 3, 4 }, { 3, 6, 12, 20 }, { -1, 2, 6, 12 }, { 2, 8, 27, 64 }, { 0.5f, 2, 3, 4 }, { 1, 2, 3, 4 } },
        { { 0, 0.25f, 25, 100 }, { -1, 0.5f, -5, 10 }, { -1, 0.75f, 20, 110 }, { 1, -0.25f, 30, 90 }, { 0, 0.125f, -125, 1000 }, { 0, 0.5f, -5, 10 }, { 0, 0.5f, 5, 10 } },
    };
    unsigned c;
    int      i;
    CHECK(sizeof(dsimd_float4_t) == 16);
    for (c = 0; c < sizeof(cases) / sizeof(cases[0]); ++c)
    {
        dsimd_float4_t a = SimdMake(cases[c].a[0], cases[c].a[1], cases[c].a[2], cases[c].a[3]);
        dsimd_float4_t b = SimdMake(cases[c].b[0], cases[c].b[1], cases[c].b[2], cases[c].b[3]);
        dsimd_float4_t add = SimdAdd(a, b), sub = SimdSub(a, b), mul = SimdMul(a, b), div = SimdDiv(a, b), root = SimdSqrt(a);
        for (i = 0; i < 4; ++i)
        {
            CHECK(SimdLane(a, i) == cases[c].a[i]);
            CHECK(SimdLane(add, i) == cases[c].add[i]);
            CHECK(SimdLane(sub, i) == cases[c].sub[i]);
            CHECK(SimdLane(mul, i) == cases[c].mul[i]);
            CHECK(SimdLane(div, i) == cases[c].div[i]);
            CHECK(SimdLane(root, i) == cases[c].root[i]);
        }
    }
}

static float Random(uint32_t* state)
{
    *state = *state * 1664525u + 1013904223u;
    return (float)(*state >> 8) / 16777216.0f;
}

// Verifies truncation at both representable limits, signed zero, subnormals and fractions.
static void TestTruncateToInt(void)
{
    static const struct
    {
        uint32_t m_Bits[4];
        int32_t  m_Expected[4];
    } cases[] = {
        { { 0xcf000000u, 0xceffffffu, 0x4effffffu, 0x4efffffeu }, { INT32_MIN, -2147483520, 2147483520, 2147483392 } },
        { { 0x3f7fffffu, 0xbf7fffffu, 0x3fffffffu, 0xbfffffffu }, { 0, 0, 1, -1 } },
        { { 0, 0x80000000u, 1, 0x80000001u }, { 0, 0, 0, 0 } },
        { { 0x3f800000u, 0xbf800000u, 0x4b800000u, 0xcb800000u }, { 1, -1, 16777216, -16777216 } }
    };
    unsigned c, lane;
    for (c = 0; c < sizeof(cases) / sizeof(cases[0]); ++c)
    {
        float   input[4];
        int32_t output[4];
        memcpy(input, cases[c].m_Bits, sizeof(input));
        SimdStoreInt(output, SimdTruncateToInt(SimdLoad(input)));
        for (lane = 0; lane < 4; ++lane)
            CHECK(output[lane] == cases[c].m_Expected[lane]);
    }
}

// Compares deterministic randomized SIMD results with scalar double references.
static void TestReferences(void)
{
    uint32_t state = 817263u;
    unsigned iteration;
    for (iteration = 0; iteration < 256; ++iteration)
    {
        float          av[4], bv[4];
        double         dot3 = 0, dot4 = 0, magnitude = 0;
        dsimd_float4_t a, b, add, sub, mul, div, roots, multiply_add;
        int            i;
        for (i = 0; i < 4; ++i)
        {
            av[i] = Random(&state) * 16 - 8;
            bv[i] = Random(&state) + 0.5f;
        }
        a = SimdMake(av[0], av[1], av[2], av[3]);
        b = SimdMake(bv[0], bv[1], bv[2], bv[3]);
        add = SimdAdd(a, b);
        sub = SimdSub(a, b);
        mul = SimdMul(a, b);
        div = SimdDiv(a, b);
        roots = SimdSqrt(b);
        multiply_add = SimdMulAdd(a, b, a);
        for (i = 0; i < 4; ++i)
        {
            double product = (double)av[i] * bv[i];
            if (i < 3)
                dot3 += product;
            dot4 += product;
            magnitude += fabs(product);
            Near((double)av[i] + bv[i], SimdLane(add, i), 2 * FLT_EPSILON * (1 + fabs(av[i]) + bv[i]));
            Near((double)av[i] - bv[i], SimdLane(sub, i), 2 * FLT_EPSILON * (1 + fabs(av[i]) + bv[i]));
            Near(product, SimdLane(mul, i), 2 * FLT_EPSILON * (1 + fabs(product)));
            Near(product + av[i], SimdLane(multiply_add, i), 3 * FLT_EPSILON * (fabs(product) + fabs(av[i])));
            Near((double)av[i] / bv[i], SimdLane(div, i), 2 * FLT_EPSILON * (1 + fabs((double)av[i] / bv[i])));
            Near(sqrt((double)bv[i]), SimdLane(roots, i), 2 * FLT_EPSILON * sqrt((double)bv[i]));
        }
        Near(dot3, SimdDot3(a, b), 8 * FLT_EPSILON * magnitude);
        Near(dot4, SimdDot4(a, b), 8 * FLT_EPSILON * magnitude);
    }
}

// Checks multiply-add operand order, cancellation and special values without
// requiring fused and unfused backends to round identically.
static void TestMultiplyAdd(void)
{
    dsimd_float4_t a = SimdMake(2, -3, FloatBits(0x3f800001u), FLT_MAX);
    dsimd_float4_t b = SimdMake(4, 5, FloatBits(0x3f7ffffeu), 0.5f);
    dsimd_float4_t c = SimdMake(7, 2, -1, -FLT_MAX * 0.5f);
    dsimd_float4_t result = SimdMulAdd(a, b, c);
    int            i;
    CHECK(SimdLane(result, 0) == 15);
    CHECK(SimdLane(result, 1) == -13);
    for (i = 2; i < 4; ++i)
    {
        double product = (double)SimdLane(a, i) * SimdLane(b, i);
        double expected = product + SimdLane(c, i);
        Near(expected, SimdLane(result, i), 3 * FLT_EPSILON * (fabs(product) + fabs(SimdLane(c, i))));
    }
    result = SimdMulAdd(SimdMake(0, 2, 1, 1), SimdMake(HUGE_VALF, 3, HUGE_VALF, 1), SimdMake(1, HUGE_VALF, -HUGE_VALF, FloatBits(0x7fc12345u)));
    CHECK(isnan(SimdLane(result, 0)));
    CHECK(SimdLane(result, 1) == HUGE_VALF);
    CHECK(isnan(SimdLane(result, 2)));
    CHECK(isnan(SimdLane(result, 3)));
}

// Preserves signed zero, quiet-NaN payloads in lane/sign operations, and accurate boundaries.
static void TestSpecialValues(void)
{
    static const uint32_t bits[] = { 0, 0x80000000u, 0x7f800000u, 0xff800000u, 0x7fc12345u, 0xffc54321u };
    unsigned              i;
    float                 infinity = HUGE_VALF;
    dsimd_float4_t        divided = SimdDiv(SimdMake(1, 0, -1, -0.0f), SimdMake(0, 0, 0, 2));
    dsimd_float4_t        roots = SimdSqrt(SimdMake(-0.0f, 4, -1, infinity));
    dsimd_float4_t        a = SimdMake(1, 2, 3, FloatBits(0x7fc12345u));
    CHECK(SimdLane(divided, 0) == infinity);
    CHECK(isnan(SimdLane(divided, 1)));
    CHECK(SimdLane(divided, 2) == -infinity);
    CHECK(Bits(SimdLane(divided, 3)) == 0x80000000u);
    CHECK(Bits(SimdLane(roots, 0)) == 0x80000000u);
    CHECK(SimdLane(roots, 1) == 2);
    CHECK(isnan(SimdLane(roots, 2)));
    CHECK(SimdLane(roots, 3) == infinity);
    CHECK(SimdDot3(a, a) == 14);
    CHECK(isnan(SimdDot4(a, a)));
    for (i = 0; i < sizeof(bits) / sizeof(bits[0]); ++i)
    {
        float          value = FloatBits(bits[i]);
        dsimd_float4_t v = SimdMake(value, value, value, value);
        dsimd_float4_t neg = SimdNeg(v), abs = SimdAbs(v);
        int            lane;
        for (lane = 0; lane < 4; ++lane)
        {
            CHECK(Bits(SimdLane(v, lane)) == bits[i]);
            CHECK(Bits(SimdLane(neg, lane)) == (bits[i] ^ 0x80000000u));
            CHECK(Bits(SimdLane(abs, lane)) == (bits[i] & 0x7fffffffu));
        }
    }
    {
        const float    inputs[4] = { FLT_MIN, 1.0f / 7.0f, 2, FLT_MAX };
        dsimd_float4_t input = SimdMake(inputs[0], inputs[1], inputs[2], inputs[3]);
        dsimd_float4_t root = SimdSqrt(input);
        dsimd_float4_t quot = SimdDiv(input, SimdMake(0.5f, 3, 3, 3));
        int            lane;
        for (lane = 0; lane < 4; ++lane)
        {
            double expected = sqrt((double)inputs[lane]);
            Near(expected, SimdLane(root, lane), 2 * FLT_EPSILON * expected);
            expected = (double)inputs[lane] / (lane == 0 ? 0.5 : 3.0);
            Near(expected, SimdLane(quot, lane), 2 * FLT_EPSILON * expected);
        }
    }
}

// Checks constant and runtime lane selection with distinct bit patterns, including
// NaN payloads, signed zeros, infinities and subnormals.
static void TestLaneBits(void)
{
    const uint32_t bits[8] = { 0x00000000u, 0x80000000u, 0x7fc12345u, 0xffc54321u, 0x7f800000u, 0xff800000u, 0x00000001u, 0x80000001u };
    unsigned       index;
    int            lane;
    for (index = 0; index < 8; ++index)
    {
        dsimd_float4_t value = SimdMake(FloatBits(bits[index]), FloatBits(bits[(index + 1) & 7]), FloatBits(bits[(index + 2) & 7]), FloatBits(bits[(index + 3) & 7]));
        CHECK(Bits(SimdLane(value, 0)) == bits[index]);
        CHECK(Bits(SimdLane(value, 1)) == bits[(index + 1) & 7]);
        CHECK(Bits(SimdLane(value, 2)) == bits[(index + 2) & 7]);
        CHECK(Bits(SimdLane(value, 3)) == bits[(index + 3) & 7]);
        for (lane = 0; lane < 4; ++lane)
        {
            volatile int runtime_lane = lane;
            CHECK(Bits(SimdLane(value, runtime_lane)) == bits[(index + lane) & 7]);
        }
    }
}

// Checks exact bitwise XOR, including sign masks, NaN payloads and all 32 bit positions.
static void TestXor(void)
{
    const uint32_t bits[8] = { 0x00000000u, 0x80000000u, 0x7fc12345u, 0xffc54321u, 0x7f800000u, 0xff800000u, 0x00000001u, 0xffffffffu };
    unsigned       index, bit;
    int            lane;
    for (index = 0; index < 8; ++index)
        for (bit = 0; bit < 32; ++bit)
        {
            uint32_t       mask = (uint32_t)1u << bit;
            dsimd_float4_t a = SimdMake(FloatBits(bits[index]), FloatBits(bits[(index + 1) & 7]), FloatBits(bits[(index + 2) & 7]), FloatBits(bits[(index + 3) & 7]));
            dsimd_float4_t b = SimdMake(FloatBits(mask), FloatBits(mask), FloatBits(mask), FloatBits(mask));
            dsimd_float4_t result = SimdXor(a, b);
            dsimd_float4_t restored = SimdXor(result, b);
            for (lane = 0; lane < 4; ++lane)
            {
                CHECK(Bits(SimdLane(result, lane)) == (bits[(index + lane) & 7] ^ mask));
                CHECK(Bits(SimdLane(restored, lane)) == bits[(index + lane) & 7]);
            }
        }
}

static unsigned       g_Evaluations;
static dsimd_float4_t NextValue(void)
{
    ++g_Evaluations;
    return SimdMake(1, 2, 3, 4);
}

// Exhausts all 256 shuffle patterns, including duplicated lanes and bit preservation.
static void TestShuffle(void)
{
    const uint32_t bits[4] = { 0x80000000u, 0x7fc12345u, 0x3f800000u, 0xff800000u };
    dsimd_float4_t a = SimdMake(FloatBits(bits[0]), FloatBits(bits[1]), FloatBits(bits[2]), FloatBits(bits[3]));
    int            x, y, z, w;
    for (x = 0; x < 4; ++x)
        for (y = 0; y < 4; ++y)
            for (z = 0; z < 4; ++z)
                for (w = 0; w < 4; ++w)
                {
                    dsimd_float4_t v = SimdShuffle(a, x, y, z, w);
                    CHECK(Bits(SimdLane(v, 0)) == bits[x]);
                    CHECK(Bits(SimdLane(v, 1)) == bits[y]);
                    CHECK(Bits(SimdLane(v, 2)) == bits[z]);
                    CHECK(Bits(SimdLane(v, 3)) == bits[w]);
                }
    a = SimdShuffle(NextValue(), 3, 2, 1, 0);
    CHECK(g_Evaluations == 1);
    CHECK(SimdLane(a, 0) == 4);
    // The function remains addressable; optimized and runtime calls agree.
    {
        dsimd_float4_t (*shuffle)(dsimd_float4_t, int, int, int, int) = SimdShuffle;
        dsimd_float4_t runtime = shuffle(a, 2, 0, 3, 1);
        dsimd_float4_t constant = SimdShuffle(a, 2, 0, 3, 1);
        int            lane;
        for (lane = 0; lane < 4; ++lane)
            CHECK(Bits(SimdLane(runtime, lane)) == Bits(SimdLane(constant, lane)));
    }
    // Variable index expressions must also be evaluated exactly once.
    x = 0;
    y = 1;
    z = 2;
    w = 3;
    a = SimdShuffle(a, x++, y++, z++, w++);
    CHECK(x == 1 && y == 2 && z == 3 && w == 4);
}

// Covers all 4096 two-register patterns and the constant shuffles used by quaternion multiplication.
static void TestShuffle2(void)
{
    const uint32_t bits[8] = { 0x80000000u, 0x7fc12345u, 0x3f800000u, 0xff800000u, 0x00000000u, 0xffc54321u, 0xbf800000u, 0x7f800000u };
    dsimd_float4_t a = SimdMake(FloatBits(bits[0]), FloatBits(bits[1]), FloatBits(bits[2]), FloatBits(bits[3]));
    dsimd_float4_t b = SimdMake(FloatBits(bits[4]), FloatBits(bits[5]), FloatBits(bits[6]), FloatBits(bits[7]));
    dsimd_float4_t (*shuffle)(dsimd_float4_t, dsimd_float4_t, int, int, int, int) = SimdShuffle2;
    int x, y, z, w;
    for (x = 0; x < 8; ++x)
        for (y = 0; y < 8; ++y)
            for (z = 0; z < 8; ++z)
                for (w = 0; w < 8; ++w)
                {
                    dsimd_float4_t value = SimdShuffle2(a, b, x, y, z, w);
                    dsimd_float4_t runtime = shuffle(a, b, x, y, z, w);
                    int            indices[4] = { x, y, z, w };
                    int            lane;
                    for (lane = 0; lane < 4; ++lane)
                    {
                        CHECK(Bits(SimdLane(value, lane)) == bits[indices[lane]]);
                        CHECK(Bits(SimdLane(runtime, lane)) == bits[indices[lane]]);
                    }
                }
#define CHECK_SHUFFLE2(X, Y, Z, W) \
    do \
    { \
        dsimd_float4_t value = SimdShuffle2(a, b, X, Y, Z, W); \
        CHECK(Bits(SimdLane(value, 0)) == bits[X]); \
        CHECK(Bits(SimdLane(value, 1)) == bits[Y]); \
        CHECK(Bits(SimdLane(value, 2)) == bits[Z]); \
        CHECK(Bits(SimdLane(value, 3)) == bits[W]); \
    } while (0)
    CHECK_SHUFFLE2(3, 6, 1, 4);
    CHECK_SHUFFLE2(2, 3, 4, 5);
    CHECK_SHUFFLE2(5, 0, 3, 6);
    CHECK_SHUFFLE2(7, 7, 0, 0);
    CHECK_SHUFFLE2(0, 1, 2, 3);
    CHECK_SHUFFLE2(4, 5, 6, 7);
#undef CHECK_SHUFFLE2
    {
        unsigned       before = g_Evaluations;
        dsimd_float4_t value = SimdShuffle2(NextValue(), NextValue(), 3, 6, 1, 4);
        CHECK(g_Evaluations == before + 2);
        CHECK(SimdLane(value, 0) == 4);
        CHECK(SimdLane(value, 1) == 3);
    }
    x = 0;
    y = 2;
    z = 4;
    w = 6;
    a = SimdShuffle2(a, b, x++, y++, z++, w++);
    CHECK(x == 1 && y == 3 && z == 5 && w == 7);
    CHECK(Bits(SimdLane(a, 2)) == bits[4]);
}

// Checks every transpose lane and the double-transpose identity with arbitrary bits.
static void TestTranspose(void)
{
    dsimd_float4_t values[4];
    uint32_t       bits[4][4];
    int            r, c;
    for (r = 0; r < 4; ++r)
    {
        for (c = 0; c < 4; ++c)
            bits[r][c] = 0x3f800000u + (uint32_t)(r * 4 + c);
        values[r] = SimdMake(FloatBits(bits[r][0]), FloatBits(bits[r][1]), FloatBits(bits[r][2]), FloatBits(bits[r][3]));
    }
    bits[1][2] = 0x7fc12345u;
    bits[3][0] = 0x80000000u;
    values[1] = SimdMake(FloatBits(bits[1][0]), FloatBits(bits[1][1]), FloatBits(bits[1][2]), FloatBits(bits[1][3]));
    values[3] = SimdMake(FloatBits(bits[3][0]), FloatBits(bits[3][1]), FloatBits(bits[3][2]), FloatBits(bits[3][3]));
    SimdTranspose(&values[0], &values[1], &values[2], &values[3]);
    for (r = 0; r < 4; ++r)
        for (c = 0; c < 4; ++c)
            CHECK(Bits(SimdLane(values[r], c)) == bits[c][r]);
    SimdTranspose(&values[0], &values[1], &values[2], &values[3]);
    for (r = 0; r < 4; ++r)
        for (c = 0; c < 4; ++c)
            CHECK(Bits(SimdLane(values[r], c)) == bits[r][c]);
}

// Checks unaligned transfers and exact tail bounds; ASan guards the last byte.
static void TestPipelineMemory(void)
{
    unsigned n, i;
    uint8_t  bytes[64], result[64];
    for (i = 0; i < 64; ++i)
        bytes[i] = (uint8_t)(i * 7);
    memset(result, 0xa5, sizeof(result));
    SimdStore(result + 1, SimdLoad(bytes + 1));
    CHECK(memcmp(result + 1, bytes + 1, 16) == 0 && result[0] == 0xa5 && result[17] == 0xa5);
    SimdStoreUint(result + 3, SimdLoadUint(bytes + 3));
    CHECK(memcmp(result + 3, bytes + 3, 16) == 0);
    SimdStoreInt(result + 5, SimdLoadInt(bytes + 5));
    CHECK(memcmp(result + 5, bytes + 5, 16) == 0);
    SimdStoreU16(result + 7, SimdLoadU16(bytes + 7));
    CHECK(memcmp(result + 7, bytes + 7, 16) == 0);
    for (n = 0; n <= 16; ++n)
    {
        uint8_t*       allocation = n ? (uint8_t*)malloc(n + 1) : 0;
        uint8_t*       source = n ? allocation + 1 : 0;
        uint8_t*       target = n ? (uint8_t*)malloc(n) : 0;
        dsimd_byte16_t value;
        for (i = 0; i < n; ++i)
            source[i] = (uint8_t)(i + 21);
        value = SimdLoadBytesPartial(source, n);
        SimdStoreBytes(result, value);
        for (i = 0; i < 16; ++i)
            CHECK(result[i] == (i < n ? i + 21 : 0));
        SimdStoreBytesPartial(target, value, n);
        if (n)
            CHECK(memcmp(source, target, n) == 0);
        free(allocation);
        free(target);
    }
    CHECK(sizeof(dsimd_uint4_t) == 16 && sizeof(dsimd_int4_t) == 16);
    CHECK(sizeof(dsimd_ushort8_t) == 16 && sizeof(dsimd_byte16_t) == 16);
}

// Checks four-channel ordering, independent alignments, guards and exact allocation ends.
static void TestPipelineInterleavedBytes(void)
{
    unsigned input_offset, output_offset, seed, channel, lane;
    for (input_offset = 0; input_offset < 16; ++input_offset)
        for (output_offset = 0; output_offset < 16; ++output_offset)
            for (seed = 0; seed < 16; ++seed)
            {
                uint8_t        input[80], original[80], output[80], lanes[4][16];
                dsimd_byte16_t a, b, c, d;
                memset(input, 0xa5, sizeof(input));
                memset(output, 0x5a, sizeof(output));
                for (lane = 0; lane < 64; ++lane)
                    input[input_offset + lane] = (uint8_t)(lane * 17 + seed * 31 + input_offset);
                memcpy(original, input, sizeof(input));
                SimdLoadInterleavedBytes4(input + input_offset, &a, &b, &c, &d);
                SimdStoreBytes(lanes[0], a);
                SimdStoreBytes(lanes[1], b);
                SimdStoreBytes(lanes[2], c);
                SimdStoreBytes(lanes[3], d);
                for (channel = 0; channel < 4; ++channel)
                    for (lane = 0; lane < 16; ++lane)
                        CHECK(lanes[channel][lane] == input[input_offset + 4 * lane + channel]);
                // Channel reversal exercises storing independently of a load/store round trip.
                SimdStoreInterleavedBytes4(output + output_offset, d, c, b, a);
                for (lane = 0; lane < 64; ++lane)
                    CHECK(output[output_offset + lane] == input[input_offset + (lane & ~3u) + 3 - (lane & 3)]);
                for (lane = 0; lane < output_offset; ++lane)
                    CHECK(output[lane] == 0x5a);
                for (lane = output_offset + 64; lane < sizeof(output); ++lane)
                    CHECK(output[lane] == 0x5a);
                CHECK(memcmp(input, original, sizeof(input)) == 0);
            }
    for (input_offset = 0; input_offset < 16; ++input_offset)
    {
        uint8_t*       input = (uint8_t*)malloc(input_offset + 64);
        uint8_t*       output = (uint8_t*)malloc(15 - input_offset + 64);
        dsimd_byte16_t a, b, c, d;
        for (lane = 0; lane < 64; ++lane)
            input[input_offset + lane] = (uint8_t)(lane * 53);
        SimdLoadInterleavedBytes4(input + input_offset, &a, &b, &c, &d);
        SimdStoreInterleavedBytes4(output + 15 - input_offset, a, b, c, d);
        CHECK(memcmp(input + input_offset, output + 15 - input_offset, 64) == 0);
        free(input);
        free(output);
    }
}

// Checks bit masks and unaligned two-word loads, including exact allocation ends.
static void TestPipelineWords(void)
{
    unsigned offset, seed, lane;
    for (offset = 0; offset < 16; ++offset)
        for (seed = 0; seed < 16; ++seed)
        {
            uint8_t*        input = (uint8_t*)malloc(offset + 32);
            uint16_t        words[16], a[8], b[8], result[8];
            dsimd_ushort8_t av, bv;
            for (lane = 0; lane < offset + 32; ++lane)
                input[lane] = (uint8_t)(lane * 73 + seed * 31);
            memcpy(words, input + offset, sizeof(words));
            SimdLoadInterleavedU16x2(input + offset, &av, &bv);
            SimdStoreU16(a, av);
            SimdStoreU16(b, bv);
            for (lane = 0; lane < 8; ++lane)
            {
                CHECK(a[lane] == words[2 * lane]);
                CHECK(b[lane] == words[2 * lane + 1]);
            }
            SimdStoreU16(result, SimdAndU16(av, bv));
            for (lane = 0; lane < 8; ++lane)
                CHECK(result[lane] == (uint16_t)(a[lane] & b[lane]));
            SimdStoreU16(result, SimdOrU16(av, bv));
            for (lane = 0; lane < 8; ++lane)
                CHECK(result[lane] == (uint16_t)(a[lane] | b[lane]));
            free(input);
        }
}

// Covers unsigned overflow, comparison at the sign boundary and every shift count.
static void TestPipelineIntegers(void)
{
    uint32_t state = 0x735297u;
    unsigned iteration, lane, count;
    for (iteration = 0; iteration < 256; ++iteration)
    {
        uint32_t      a[4], b[4], result[4], counts[4];
        dsimd_uint4_t av, bv, mask;
        for (lane = 0; lane < 4; ++lane)
        {
            state = state * 1664525u + 1013904223u;
            a[lane] = state;
            state = state * 1664525u + 1013904223u;
            b[lane] = state;
            counts[lane] = (iteration + lane) % 40;
        }
        av = SimdLoadUint(a);
        bv = SimdLoadUint(b);
#define CHECK_UINT_OP(function, expression) \
    SimdStoreUint(result, function(av, bv)); \
    for (lane = 0; lane < 4; ++lane) \
    CHECK(result[lane] == (uint32_t)(expression))
        CHECK_UINT_OP(SimdAddUint, a[lane] + b[lane]);
        CHECK_UINT_OP(SimdSubUint, a[lane] - b[lane]);
        CHECK_UINT_OP(SimdMulUint, (uint64_t)a[lane] * b[lane]);
        CHECK_UINT_OP(SimdAndUint, a[lane] & b[lane]);
        CHECK_UINT_OP(SimdOrUint, a[lane] | b[lane]);
        CHECK_UINT_OP(SimdXorUint, a[lane] ^ b[lane]);
        CHECK_UINT_OP(SimdLessUint, a[lane] < b[lane] ? UINT32_MAX : 0);
        CHECK_UINT_OP(SimdEqualUint, a[lane] == b[lane] ? UINT32_MAX : 0);
        CHECK_UINT_OP(SimdMinUint, a[lane] < b[lane] ? a[lane] : b[lane]);
        CHECK_UINT_OP(SimdMaxUint, a[lane] > b[lane] ? a[lane] : b[lane]);
#undef CHECK_UINT_OP
        mask = SimdMakeUint(0, UINT32_MAX, 0x80000000u, 0x55555555u);
        SimdStoreUint(result, SimdSelectUint(mask, av, bv));
        {
            uint32_t m[4];
            SimdStoreUint(m, mask);
            for (lane = 0; lane < 4; ++lane)
                CHECK(result[lane] == ((m[lane] & a[lane]) | (~m[lane] & b[lane])));
        }
        SimdStoreUint(result, SimdNotUint(av));
        for (lane = 0; lane < 4; ++lane)
            CHECK(result[lane] == ~a[lane]);
        SimdStoreUint(result, SimdShiftRightUintEach(av, SimdLoadUint(counts)));
        for (lane = 0; lane < 4; ++lane)
            CHECK(result[lane] == (counts[lane] < 32 ? a[lane] >> counts[lane] : 0));
        for (count = 0; count <= 33; ++count)
        {
            SimdStoreUint(result, SimdShiftLeftUint(av, count));
            for (lane = 0; lane < 4; ++lane)
                CHECK(result[lane] == (count < 32 ? a[lane] << count : 0));
            SimdStoreUint(result, SimdShiftRightUint(av, count));
            for (lane = 0; lane < 4; ++lane)
                CHECK(result[lane] == (count < 32 ? a[lane] >> count : 0));
        }
    }
    {
        uint32_t a[4] = { 0, 0x7fffffffu, 0x80000000u, UINT32_MAX }, b[4] = { 0, 0x80000000u, 0x7fffffffu, 0 };
        uint32_t result[4];
        SimdStoreUint(result, SimdLessUint(SimdLoadUint(a), SimdLoadUint(b)));
        CHECK(result[0] == 0 && result[1] == UINT32_MAX && result[2] == 0 && result[3] == 0);
    }
    for (iteration = 0; iteration < 16; ++iteration)
    {
        dsimd_uint4_t m = SimdMakeUint(iteration & 1 ? UINT32_MAX : 0, iteration & 2 ? UINT32_MAX : 0, iteration & 4 ? UINT32_MAX : 0, iteration & 8 ? UINT32_MAX : 0);
        CHECK(SimdMaskBits(m) == iteration);
        CHECK(SimdAny(m) == (iteration != 0));
        CHECK(SimdAll(m) == (iteration == 15));
    }
}

// Exhausts unsigned input lanes against boundary multipliers and exact divide-by-255 rounding.
static void TestPipelineMulHighU16(void)
{
    static const uint16_t multipliers[] = { 0, 1, 255, 256, 32767, 32768, 65534, 65535 };
    unsigned              factor, value, lane;
    uint16_t              input[8], output[8];
    for (factor = 0; factor < sizeof(multipliers) / sizeof(multipliers[0]); ++factor)
        for (value = 0; value < 65536; value += 8)
        {
            for (lane = 0; lane < 8; ++lane)
                input[lane] = (uint16_t)(value + lane);
            SimdStoreU16(output, SimdMulHighU16(SimdLoadU16(input), SimdSplatU16(multipliers[factor])));
            for (lane = 0; lane < 8; ++lane)
                CHECK(output[lane] == (((uint32_t)input[lane] * multipliers[factor]) >> 16));
        }
    for (value = 0; value < 65536; value += 8)
    {
        for (lane = 0; lane < 8; ++lane)
            input[lane] = (uint16_t)(value + lane);
        SimdStoreU16(output, SimdShiftRightU16(SimdMulHighU16(SimdLoadU16(input), SimdSplatU16(0x8081)), 7));
        for (lane = 0; lane < 8; ++lane)
            CHECK(output[lane] == input[lane] / 255);
    }
}

// Checks ordered float selection, bit preservation and quantization/reduction helpers.
static void TestPipelineFloats(void)
{
    uint32_t       a[4] = { 0x7fc12345u, 0x80000000u, 0x7f800000u, 0xff800000u };
    uint32_t       b[4] = { 0x40000000u, 0, 0xff800000u, 0x7f800000u }, result[4], mask[4];
    dsimd_float4_t av = SimdUintBits(SimdLoadUint(a)), bv = SimdUintBits(SimdLoadUint(b));
    unsigned       i;
    SimdStoreUint(result, SimdFloatBits(av));
    CHECK(memcmp(result, a, 16) == 0);
    SimdStoreUint(mask, SimdLess(av, bv));
    SimdStoreUint(result, SimdFloatBits(SimdMin(av, bv)));
    for (i = 0; i < 4; ++i)
        CHECK(result[i] == (FloatBits(a[i]) < FloatBits(b[i]) ? a[i] : b[i]));
    SimdStoreUint(result, SimdFloatBits(SimdMax(av, bv)));
    for (i = 0; i < 4; ++i)
        CHECK(result[i] == (FloatBits(a[i]) > FloatBits(b[i]) ? a[i] : b[i]));
    SimdStoreUint(result, SimdFloatBits(SimdSelect(SimdEqual(av, av), av, bv)));
    CHECK(result[0] == b[0]);
    for (i = 1; i < 4; ++i)
        CHECK(result[i] == a[i]);
    CHECK(SimdSum(SimdMake(1, 2, 3, 4)) == 10);
    CHECK(SimdReduceMin(SimdMake(3, -2, 1, 9)) == -2);
    CHECK(SimdReduceMax(SimdMake(3, -2, 1, 9)) == 9);
    {
        int32_t integers[4];
        float   floats[4];
        SimdStoreInt(integers, SimdTruncateToInt(SimdMake(-12.75f, 2.9f, -2147483648.0f, 2147483520.0f)));
        CHECK(integers[0] == -12 && integers[1] == 2 && integers[2] == INT32_MIN && integers[3] == 2147483520);
        SimdStore(floats, SimdConvertIntToFloat(SimdLoadInt(integers)));
        for (i = 0; i < 4; ++i)
            CHECK(floats[i] == (float)integers[i]);
    }
    {
        float values[4];
        SimdStore(values, SimdSplat(3.25f));
        for (i = 0; i < 4; ++i)
            CHECK(values[i] == 3.25f);
    }
    {
        uint32_t state = 0x782394u, values[4];
        float    converted[4];
        unsigned iteration;
        for (iteration = 0; iteration < 4096; ++iteration)
        {
            for (i = 0; i < 4; ++i)
            {
                state = state * 1664525u + 1013904223u;
                values[i] = state;
            }
            SimdStore(converted, SimdConvertUintToFloat(SimdLoadUint(values)));
            for (i = 0; i < 4; ++i)
                CHECK(converted[i] == (float)values[i]);
        }
    }
}

// Exhausts byte-table indices and unsigned narrowing, including values above INT16_MAX.
static void TestPipelinePacking(void)
{
    uint8_t  bytes[16], indices[16], result[16], other[16];
    uint16_t words[8], word_result[8];
    uint32_t wide[4];
    unsigned i, offset;
    for (i = 0; i < 16; ++i)
    {
        bytes[i] = (uint8_t)(i * 11);
        other[i] = (uint8_t)(255 - i);
    }
    for (offset = 0; offset < 256; offset += 16)
    {
        for (i = 0; i < 16; ++i)
            indices[i] = (uint8_t)(offset + i);
        SimdStoreBytes(result, SimdShuffleBytes(SimdLoadBytes(bytes), SimdLoadBytes(indices)));
        for (i = 0; i < 16; ++i)
            CHECK(result[i] == (indices[i] < 16 ? bytes[indices[i]] : 0));
    }
    SimdStoreBytes(result, SimdZipBytesLow(SimdLoadBytes(bytes), SimdLoadBytes(other)));
    for (i = 0; i < 8; ++i)
    {
        CHECK(result[2 * i] == bytes[i]);
        CHECK(result[2 * i + 1] == other[i]);
    }
    SimdStoreBytes(result, SimdZipBytesHigh(SimdLoadBytes(bytes), SimdLoadBytes(other)));
    for (i = 0; i < 8; ++i)
    {
        CHECK(result[2 * i] == bytes[i + 8]);
        CHECK(result[2 * i + 1] == other[i + 8]);
    }
    SimdStoreBytes(result, SimdPackBytes(SimdWidenBytesLow(SimdLoadBytes(bytes)), SimdWidenBytesHigh(SimdLoadBytes(bytes))));
    CHECK(memcmp(result, bytes, 16) == 0);
    for (offset = 0; offset < 65536; offset += 8)
    {
        for (i = 0; i < 8; ++i)
            words[i] = (uint16_t)(offset + i);
        SimdStoreBytes(result, SimdPackBytes(SimdLoadU16(words), SimdLoadU16(words)));
        for (i = 0; i < 16; ++i)
            CHECK(result[i] == (words[i & 7] > 255 ? 255 : words[i & 7]));
        SimdStoreBytes(result, SimdPackSignedBytes(SimdLoadU16(words), SimdLoadU16(words)));
        for (i = 0; i < 16; ++i)
        {
            int32_t value = (int32_t)(words[i & 7] & 0x7fff) - (int32_t)(words[i & 7] & 0x8000);
            CHECK(result[i] == (value < 0 ? 0 : value > 255 ? 255 :
                                                              value));
        }
    }
    {
        uint32_t low[4] = { 0, 65535, 65536, UINT32_MAX }, high[4] = { 32768, 0x80000000u, 1, 65534 };
        SimdStoreU16(word_result, SimdPackU16(SimdLoadUint(low), SimdLoadUint(high)));
        for (i = 0; i < 4; ++i)
        {
            CHECK(word_result[i] == (low[i] > 65535 ? 65535 : low[i]));
            CHECK(word_result[i + 4] == (high[i] > 65535 ? 65535 : high[i]));
        }
        SimdStoreU16(word_result, SimdNarrowU16(SimdLoadUint(low), SimdLoadUint(high)));
        for (i = 0; i < 4; ++i)
        {
            CHECK(word_result[i] == (uint16_t)low[i]);
            CHECK(word_result[i + 4] == (uint16_t)high[i]);
        }
        SimdStoreUint(wide, SimdWidenU16Low(SimdLoadU16(word_result)));
        for (i = 0; i < 4; ++i)
            CHECK(wide[i] == word_result[i]);
        SimdStoreUint(wide, SimdWidenU16High(SimdLoadU16(word_result)));
        for (i = 0; i < 4; ++i)
            CHECK(wide[i] == word_result[i + 4]);
    }
    for (offset = 0; offset <= 17; ++offset)
    {
        uint16_t a[8], b[8];
        for (i = 0; i < 8; ++i)
        {
            a[i] = (uint16_t)(65535 - i * 9001);
            b[i] = (uint16_t)(i * 19311);
        }
#define CHECK_U16_OP(function, expression) \
    SimdStoreU16(word_result, function(SimdLoadU16(a), SimdLoadU16(b))); \
    for (i = 0; i < 8; ++i) \
    CHECK(word_result[i] == (uint16_t)(expression))
        CHECK_U16_OP(SimdAddU16, (uint32_t)a[i] + b[i]);
        CHECK_U16_OP(SimdSubU16, (uint32_t)a[i] - b[i]);
        CHECK_U16_OP(SimdMulU16, (uint32_t)a[i] * b[i]);
#undef CHECK_U16_OP
        SimdStoreU16(word_result, SimdShiftLeftU16(SimdLoadU16(a), offset));
        for (i = 0; i < 8; ++i)
            CHECK(word_result[i] == (offset < 16 ? (uint16_t)((uint32_t)a[i] << offset) : 0));
        SimdStoreU16(word_result, SimdShiftRightU16(SimdLoadU16(a), offset));
        for (i = 0; i < 8; ++i)
            CHECK(word_result[i] == (offset < 16 ? a[i] >> offset : 0));
    }
    SimdStoreBytes(result, SimdSplatBytes(0xa5));
    for (i = 0; i < 16; ++i)
        CHECK(result[i] == 0xa5);
}

static dsimd_ushort8_t PipelineDivide255(dsimd_ushort8_t value)
{
    return SimdShiftRightU16(SimdAddU16(SimdAddU16(value, SimdSplatU16(1)), SimdShiftRightU16(value, 8)), 8);
}

// Exhausts all channel/alpha pairs, preserving texc's truncating division and alpha.
static void TestPipelinePremultiply(void)
{
    static const uint8_t alpha_indices[16] = { 3, 3, 3, 3, 7, 7, 7, 7, 11, 11, 11, 11, 15, 15, 15, 15 };
    unsigned             alpha, channel, i;
    uint8_t              source[16], result[16];
    for (alpha = 0; alpha < 256; ++alpha)
        for (channel = 0; channel < 256; ++channel)
        {
            dsimd_byte16_t pixels, alphas, packed;
            for (i = 0; i < 16; ++i)
                source[i] = (uint8_t)((i & 3) == 3 ? alpha : channel);
            pixels = SimdLoadBytes(source);
            alphas = SimdShuffleBytes(pixels, SimdLoadBytes(alpha_indices));
            packed = SimdPackBytes(PipelineDivide255(SimdMulU16(SimdWidenBytesLow(pixels), SimdWidenBytesLow(alphas))),
                                   PipelineDivide255(SimdMulU16(SimdWidenBytesHigh(pixels), SimdWidenBytesHigh(alphas))));
            packed = SimdUintToBytes(SimdSelectUint(SimdSplatUint(0xff000000u), SimdBytesToUint(pixels), SimdBytesToUint(packed)));
            SimdStoreBytes(result, packed);
            for (i = 0; i < 16; ++i)
                CHECK(result[i] == ((i & 3) == 3 ? alpha : channel * alpha / 255));
        }
}

// Uses byte shuffles and bounded tails for an odd-width RGB image and pixel reversal.
static void TestPipelinePixels(void)
{
    static const uint8_t expand[16] = { 0, 1, 2, 255, 3, 4, 5, 255, 6, 7, 8, 255, 9, 10, 11, 255 };
    static const uint8_t reverse[16] = { 12, 13, 14, 15, 8, 9, 10, 11, 4, 5, 6, 7, 0, 1, 2, 3 };
    uint8_t              rgb[27], rgba[36], result[16];
    unsigned             i, pixel;
    for (i = 0; i < 27; ++i)
        rgb[i] = (uint8_t)(i * 9);
    for (pixel = 0; pixel < 9; pixel += 4)
    {
        unsigned       count = 9 - pixel < 4 ? 9 - pixel : 4;
        dsimd_byte16_t value = SimdShuffleBytes(SimdLoadBytesPartial(rgb + pixel * 3, count * 3), SimdLoadBytes(expand));
        value = SimdUintToBytes(SimdOrUint(SimdBytesToUint(value), SimdSplatUint(0xff000000u)));
        SimdStoreBytesPartial(rgba + pixel * 4, value, count * 4);
    }
    for (pixel = 0; pixel < 9; ++pixel)
        for (i = 0; i < 4; ++i)
            CHECK(rgba[pixel * 4 + i] == (i == 3 ? 255 : rgb[pixel * 3 + i]));
    SimdStoreBytes(result, SimdShuffleBytes(SimdLoadBytes(rgba), SimdLoadBytes(reverse)));
    for (i = 0; i < 16; ++i)
        CHECK(result[i] == rgba[(3 - i / 4) * 4 + (i & 3)]);
}

// Keeps the integer 3x3 convolution's precision and division, with unchanged borders.
static void TestPipelineBlur(void)
{
    uint8_t  pixels[19 * 5], result[19 * 5];
    unsigned x, y, i;
    for (i = 0; i < sizeof(pixels); ++i)
        pixels[i] = (uint8_t)(i * 37);
    memcpy(result, pixels, sizeof(pixels));
    for (y = 1; y < 4; ++y)
        for (x = 1; x < 18; x += 8)
        {
            unsigned        count = 18 - x < 8 ? 18 - x : 8;
            dsimd_ushort8_t sum = SimdSplatU16(0);
            int             dy, dx;
            for (dy = -1; dy <= 1; ++dy)
                for (dx = -1; dx <= 1; ++dx)
                {
                    unsigned        weight = (dy == 0 ? 2 : 1) * (dx == 0 ? 2 : 1);
                    dsimd_ushort8_t values = SimdWidenBytesLow(SimdLoadBytesPartial(pixels + (y + dy) * 19 + x + dx, count));
                    sum = SimdAddU16(sum, SimdMulU16(values, SimdSplatU16((uint16_t)weight)));
                }
            SimdStoreBytesPartial(result + y * 19 + x, SimdPackBytes(SimdShiftRightU16(sum, 4), SimdSplatU16(0)), count);
        }
    for (y = 0; y < 5; ++y)
        for (x = 0; x < 19; ++x)
        {
            unsigned expected = pixels[y * 19 + x];
            if (x && x < 18 && y && y < 4)
            {
                int dy, dx;
                expected = 0;
                for (dy = -1; dy <= 1; ++dy)
                    for (dx = -1; dx <= 1; ++dx)
                        expected += pixels[(y + dy) * 19 + x + dx] * (dy == 0 ? 2 : 1) * (dx == 0 ? 2 : 1);
                expected /= 16;
            }
            CHECK(result[y * 19 + x] == expected);
        }
}

// Independent texc-compatible half reference; its existing overflow/NaN behavior is intentional here.
static uint16_t PipelineHalfReference(uint32_t bits)
{
    uint32_t sign = (bits >> 16) & 0x8000, mantissa = bits & 0x7fffff;
    int      exponent = (int)((bits >> 23) & 255) - 112;
    if (exponent <= 0)
    {
        if (exponent < -10)
            return (uint16_t)sign;
        mantissa = (mantissa | 0x800000) >> (1 - exponent);
        return (uint16_t)(sign | ((mantissa + 0x1000) >> 13));
    }
    if (exponent >= 31)
        return (uint16_t)(sign | 0x7c00 | (mantissa ? mantissa >> 13 : 0));
    return (uint16_t)(sign | ((exponent << 10) + ((mantissa + 0x1000) >> 13)));
}
static dsimd_uint4_t PipelineHalf(dsimd_float4_t value)
{
    dsimd_uint4_t bits = SimdFloatBits(value), zero = SimdSplatUint(0);
    dsimd_uint4_t sign = SimdAndUint(SimdShiftRightUint(bits, 16), SimdSplatUint(0x8000));
    dsimd_uint4_t exponent = SimdAndUint(SimdShiftRightUint(bits, 23), SimdSplatUint(255));
    dsimd_uint4_t mantissa = SimdAndUint(bits, SimdSplatUint(0x7fffff));
    dsimd_uint4_t normal = SimdAddUint(SimdShiftLeftUint(SimdSubUint(exponent, SimdSplatUint(112)), 10), SimdShiftRightUint(SimdAddUint(mantissa, SimdSplatUint(0x1000)), 13));
    dsimd_uint4_t subnormal = SimdShiftRightUint(SimdAddUint(SimdShiftRightUintEach(SimdOrUint(mantissa, SimdSplatUint(0x800000)), SimdSubUint(SimdSplatUint(113), exponent)), SimdSplatUint(0x1000)), 13);
    dsimd_uint4_t special = SimdOrUint(SimdSplatUint(0x7c00), SimdShiftRightUint(mantissa, 13));
    subnormal = SimdSelectUint(SimdLessUint(exponent, SimdSplatUint(102)), zero, subnormal);
    normal = SimdSelectUint(SimdLessUint(exponent, SimdSplatUint(113)), subnormal, normal);
    return SimdOrUint(sign, SimdSelectUint(SimdLessUint(exponent, SimdSplatUint(143)), normal, special));
}

// Covers half rounding ties, subnormal boundaries, special payloads and deterministic raw bits.
static void TestPipelineHalf(void)
{
    uint32_t              state = 0x479325u, bits[4], result[4];
    unsigned              i, lane;
    static const uint32_t edges[] = { 0, 0x80000000u, 0x33000000u, 0x33800000u, 0x387fe000u, 0x38800000u, 0x3f801000u, 0x477fe000u, 0x47800000u, 0x7f800000u, 0xff800000u, 0x7fc12345u, 0x7f800001u };
    for (i = 0; i < 8192; ++i)
    {
        for (lane = 0; lane < 4; ++lane)
        {
            state = state * 1664525u + 1013904223u;
            bits[lane] = i < sizeof(edges) / sizeof(edges[0]) ? edges[i] : state;
        }
        SimdStoreUint(result, PipelineHalf(SimdUintBits(SimdLoadUint(bits))));
        for (lane = 0; lane < 4; ++lane)
            CHECK(result[lane] == PipelineHalfReference(bits[lane]));
    }
}

// Processes four independent counters; scalar rounds verify exact modulo arithmetic.
static void TestPipelineXtea(void)
{
    static const uint32_t key[4] = { 0x73482019u, 0x98abcdefu, 0x12345678u, 0x76543210u };
    uint32_t              low[4], high[4], out_low[4], out_high[4];
    unsigned              batch, lane, round;
    for (batch = 0; batch < 64; ++batch)
    {
        dsimd_uint4_t a, b;
        uint32_t      sum = 0;
        for (lane = 0; lane < 4; ++lane)
        {
            low[lane] = batch * 4 + lane;
            high[lane] = 0;
        }
        a = SimdLoadUint(low);
        b = SimdLoadUint(high);
        for (round = 0; round < 32; ++round)
        {
            a = SimdAddUint(a, SimdXorUint(SimdAddUint(SimdXorUint(SimdShiftLeftUint(b, 4), SimdShiftRightUint(b, 5)), b), SimdSplatUint(sum + key[sum & 3])));
            sum += 0x9e3779b9u;
            b = SimdAddUint(b, SimdXorUint(SimdAddUint(SimdXorUint(SimdShiftLeftUint(a, 4), SimdShiftRightUint(a, 5)), a), SimdSplatUint(sum + key[(sum >> 11) & 3])));
        }
        SimdStoreUint(out_low, a);
        SimdStoreUint(out_high, b);
        for (lane = 0; lane < 4; ++lane)
        {
            uint32_t x = low[lane], y = high[lane];
            sum = 0;
            for (round = 0; round < 32; ++round)
            {
                x += (((y << 4) ^ (y >> 5)) + y) ^ (sum + key[sum & 3]);
                sum += 0x9e3779b9u;
                y += (((x << 4) ^ (x >> 5)) + x) ^ (sum + key[(sum >> 11) & 3]);
            }
            CHECK(out_low[lane] == x && out_high[lane] == y);
        }
    }
}

// Composes float primitives for HDR interpolation and bounds, and integer bytes for deltas.
static void TestPipelineResamplingAndDelta(void)
{
    float          pixels[4][4] = { { 1, 2, 3, 4 }, { 4, 3, 2, 1 }, { 10, 20, 30, 40 }, { 40, 30, 20, 10 } };
    float          result[4];
    unsigned       i;
    dsimd_float4_t tx = SimdSplat(0.3f), ty = SimdSplat(0.7f);
    dsimd_float4_t top = SimdMulAdd(SimdSub(SimdLoad(pixels[1]), SimdLoad(pixels[0])), tx, SimdLoad(pixels[0]));
    dsimd_float4_t bottom = SimdMulAdd(SimdSub(SimdLoad(pixels[3]), SimdLoad(pixels[2])), tx, SimdLoad(pixels[2]));
    SimdStore(result, SimdMulAdd(SimdSub(bottom, top), ty, top));
    for (i = 0; i < 4; ++i)
    {
        double a = pixels[0][i] + (pixels[1][i] - pixels[0][i]) * 0.3;
        double b = pixels[2][i] + (pixels[3][i] - pixels[2][i]) * 0.3;
        Near(a + (b - a) * 0.7, result[i], 5e-6);
    }
    {
        uint8_t input[16], previous[16], encoded[16];
        for (i = 0; i < 16; ++i)
        {
            input[i] = (uint8_t)(i * 97);
            previous[i] = (uint8_t)(255 - i * 41);
        }
        dsimd_ushort8_t lo = SimdSubU16(SimdWidenBytesLow(SimdLoadBytes(input)), SimdWidenBytesLow(SimdLoadBytes(previous)));
        dsimd_ushort8_t hi = SimdSubU16(SimdWidenBytesHigh(SimdLoadBytes(input)), SimdWidenBytesHigh(SimdLoadBytes(previous)));
        // Reduce modulo 256 before unsigned saturating narrowing.
        lo = SimdShiftRightU16(SimdShiftLeftU16(lo, 8), 8);
        hi = SimdShiftRightU16(SimdShiftLeftU16(hi, 8), 8);
        SimdStoreBytes(encoded, SimdPackBytes(lo, hi));
        for (i = 0; i < 16; ++i)
            CHECK(encoded[i] == (uint8_t)(input[i] - previous[i]));
    }
}

// Checks wide block ordering, unsigned boundaries, and every unaligned transfer against scalar references.
static void TestPipelineWide(void)
{
    uint8_t  source[DM_SIMD_WIDE_BYTES + 65], output[sizeof(source)], indices[DM_SIMD_WIDE_BYTES];
    uint16_t a[DM_SIMD_WIDE_BYTES / 2], b[DM_SIMD_WIDE_BYTES / 2], words[DM_SIMD_WIDE_BYTES / 2];
    uint32_t integers[DM_SIMD_WIDE_BYTES / 4];
    float    floats[DM_SIMD_WIDE_BYTES / 4], other[DM_SIMD_WIDE_BYTES / 4], values[DM_SIMD_WIDE_BYTES / 4];
    unsigned i, offset, count;
    CHECK(sizeof(dsimd_wide_byte_t) == DM_SIMD_WIDE_BYTES);
    CHECK(sizeof(dsimd_wide_float_t) == DM_SIMD_WIDE_BYTES);
    for (i = 0; i < sizeof(source); ++i)
        source[i] = (uint8_t)(i * 37 + 11);
    for (offset = 0; offset < 64; ++offset)
    {
        memset(output, 0xa5, sizeof(output));
        SimdWideStoreBytes(output + offset + 1, SimdWideLoadBytes(source + offset + 1));
        CHECK(memcmp(output + offset + 1, source + offset + 1, DM_SIMD_WIDE_BYTES) == 0);
        CHECK(output[offset] == 0xa5 && output[offset + 1 + DM_SIMD_WIDE_BYTES] == 0xa5);
        SimdWideStore(output + offset + 1, SimdWideLoad(source + offset + 1));
        CHECK(memcmp(output + offset + 1, source + offset + 1, DM_SIMD_WIDE_BYTES) == 0);
        {
            uint8_t* allocation = (uint8_t*)malloc(offset + 1 + DM_SIMD_WIDE_BYTES);
            memcpy(allocation + offset + 1, source, DM_SIMD_WIDE_BYTES);
            SimdWideStoreBytes(allocation + offset + 1, SimdWideLoadBytes(allocation + offset + 1));
            SimdWideStore(allocation + offset + 1, SimdWideLoad(allocation + offset + 1));
            free(allocation);
        }
    }
    SimdWideStoreBytes(output, SimdWideUintToBytes(SimdWideBytesToUint(SimdWideLoadBytes(source))));
    CHECK(memcmp(source, output, DM_SIMD_WIDE_BYTES) == 0);
    SimdWideStoreBytes(integers, SimdWideUintToBytes(SimdWideOrUint(SimdWideSplatUint(0x12345678u), SimdWideSplatUint(0x87654321u))));
    for (i = 0; i < DM_SIMD_WIDE_BYTES / 4; ++i)
        CHECK(integers[i] == (0x12345678u | 0x87654321u));
    SimdWideStoreBytes(words, SimdWidePackBytes(SimdWideWidenBytesLow(SimdWideLoadBytes(source)), SimdWideWidenBytesHigh(SimdWideLoadBytes(source))));
    CHECK(memcmp(words, source, DM_SIMD_WIDE_BYTES) == 0);
    for (i = 0; i < DM_SIMD_WIDE_BYTES / 2; ++i)
    {
        a[i] = (uint16_t)(65535 - i * 9001);
        b[i] = (uint16_t)(i * 19311);
    }
#define WIDE_WORDS(function, reference) \
    { \
        dsimd_wide_ushort_t result = function; \
        memcpy(words, &result, DM_SIMD_WIDE_BYTES); \
        for (i = 0; i < DM_SIMD_WIDE_BYTES / 2; ++i) \
            CHECK(words[i] == (uint16_t)(reference)); \
    }
    WIDE_WORDS(SimdWideSplatU16(65535), 65535);
    {
        dsimd_wide_ushort_t x, y;
        memcpy(&x, a, DM_SIMD_WIDE_BYTES);
        memcpy(&y, b, DM_SIMD_WIDE_BYTES);
        WIDE_WORDS(SimdWideAddU16(x, y), (uint32_t)a[i] + b[i]);
        WIDE_WORDS(SimdWideMulU16(x, y), (uint32_t)a[i] * b[i]);
        for (count = 0; count <= 17; ++count)
            WIDE_WORDS(SimdWideShiftRightU16(x, count), count < 16 ? a[i] >> count : 0);
        SimdWideStoreBytes(output, SimdWidePackBytes(x, y));
        for (i = 0; i < DM_SIMD_WIDE_BYTES; ++i)
        {
            unsigned block = i / 16, lane = i % 16;
            uint16_t value = lane < 8 ? a[block * 8 + lane] : b[block * 8 + lane - 8];
            CHECK(output[i] == (value > 255 ? 255 : value));
        }
    }
#undef WIDE_WORDS
    for (count = 0; count < 256; ++count)
    {
        for (i = 0; i < DM_SIMD_WIDE_BYTES; ++i)
            indices[i] = (uint8_t)(count + i);
        SimdWideStoreBytes(output, SimdWideShuffleBytes(SimdWideLoadBytes(source), SimdWideLoadBytes(indices)));
        for (i = 0; i < DM_SIMD_WIDE_BYTES; ++i)
            CHECK(output[i] == (indices[i] < 16 ? source[(i / 16) * 16 + indices[i]] : 0));
    }
    for (i = 0; i < DM_SIMD_WIDE_BYTES / 4; ++i)
    {
        floats[i] = (float)i - 8.0f;
        other[i] = (float)(i + 1) * 0.25f;
    }
    SimdWideStore(values, SimdWideSplat(-3.5f));
    for (i = 0; i < DM_SIMD_WIDE_BYTES / 4; ++i)
        CHECK(values[i] == -3.5f);
    SimdWideStore(values, SimdWideAdd(SimdWideLoad(floats), SimdWideLoad(other)));
    for (i = 0; i < DM_SIMD_WIDE_BYTES / 4; ++i)
        CHECK(values[i] == floats[i] + other[i]);
    SimdWideStore(values, SimdWideMul(SimdWideLoad(floats), SimdWideLoad(other)));
    for (i = 0; i < DM_SIMD_WIDE_BYTES / 4; ++i)
        CHECK(values[i] == floats[i] * other[i]);
    // Preserve signed zeros and non-finite behavior without requiring identical NaN payloads.
    {
        const uint32_t bits[] = { 0, 0x80000000u, 0x3f800000u, 0xbf800000u, 0x7f800000u, 0xff800000u, 0x7fc12345u, 0x7f7fffffu };
        for (count = 0; count < 8; ++count)
        {
            for (i = 0; i < DM_SIMD_WIDE_BYTES / 4; ++i)
            {
                floats[i] = FloatBits(bits[i % 8]);
                other[i] = FloatBits(bits[(i + count) % 8]);
            }
            SimdWideStore(values, SimdWideSplat(FloatBits(bits[count])));
            for (i = 0; i < DM_SIMD_WIDE_BYTES / 4; ++i)
                CHECK(Bits(values[i]) == bits[count]);
            SimdWideStore(values, SimdWideAdd(SimdWideLoad(floats), SimdWideLoad(other)));
            for (i = 0; i < DM_SIMD_WIDE_BYTES / 4; ++i)
            {
                float expected = floats[i] + other[i];
                CHECK(isnan(expected) ? isnan(values[i]) : Bits(values[i]) == Bits(expected));
            }
            SimdWideStore(values, SimdWideMul(SimdWideLoad(floats), SimdWideLoad(other)));
            for (i = 0; i < DM_SIMD_WIDE_BYTES / 4; ++i)
            {
                float expected = floats[i] * other[i];
                CHECK(isnan(expected) ? isnan(values[i]) : Bits(values[i]) == Bits(expected));
            }
        }
    }
}

// Runs generic primitives and representative content-pipeline compositions.
static void TestPipeline(void)
{
    TestPipelineWide();
    TestPipelineMemory();
    TestPipelineInterleavedBytes();
    TestPipelineWords();
    TestPipelineIntegers();
    TestPipelineMulHighU16();
    TestPipelineFloats();
    TestPipelinePacking();
    TestPipelinePremultiply();
    TestPipelinePixels();
    TestPipelineBlur();
    TestPipelineHalf();
    TestPipelineXtea();
    TestPipelineResamplingAndDelta();
}

// Missing CPU features or OS register state must reject AVX before any kernel executes.
static int CheckAvxDetection(void)
{
    const uint32_t sse = 1u | (1u << 9) | (1u << 19) | (1u << 20);
    const uint32_t state = (1u << 26) | (1u << 27) | (1u << 28);
    const uint32_t edx = (1u << 24) | (1u << 25) | (1u << 26);
    const uint32_t avx2 = 1u << 5;
    const uint32_t avx512 = avx2 | (1u << 16) | (1u << 30);
    unsigned       bit;
    if (SimdInternalRuntimeVersionX86(sse | state, edx, avx2, 6) != DSIMD_VERSION_AVX2 ||
        SimdInternalRuntimeVersionX86(sse | state, edx, avx512, 0xe6) != DSIMD_VERSION_AVX512)
        return 0;
    for (bit = 26; bit <= 28; ++bit)
        if (SimdInternalRuntimeVersionX86((sse | state) & ~(1u << bit), edx, avx512, 0xe6) != DSIMD_VERSION_SSE42)
            return 0;
    if (SimdInternalRuntimeVersionX86(sse | state, edx, avx512, 0) != DSIMD_VERSION_SSE42 ||
        SimdInternalRuntimeVersionX86(sse | state, edx, avx512, 2) != DSIMD_VERSION_SSE42 ||
        SimdInternalRuntimeVersionX86(sse | state, edx, avx512, 4) != DSIMD_VERSION_SSE42 ||
        SimdInternalRuntimeVersionX86(sse | state, edx, avx512 & ~avx2, 0xe6) != DSIMD_VERSION_SSE42)
        return 0;
    for (bit = 5; bit <= 7; ++bit)
        if (SimdInternalRuntimeVersionX86(sse | state, edx, avx512, 0xe6u & ~(1u << bit)) != DSIMD_VERSION_AVX2)
            return 0;
    if (SimdInternalRuntimeVersionX86(sse | state, edx, avx512 & ~(1u << 16), 0xe6) != DSIMD_VERSION_AVX2 ||
        SimdInternalRuntimeVersionX86(sse | state, edx, avx512 & ~(1u << 30), 0xe6) != DSIMD_VERSION_AVX2)
        return 0;
    if (SimdInternalDarwinAvx512State(0xe6, 21, 2, 1) != 6 ||
        SimdInternalDarwinAvx512State(6, 21, 3, 1) != 0xe6 ||
        SimdInternalDarwinAvx512State(6, 22, 0, 1) != 0xe6 ||
        SimdInternalDarwinAvx512State(0xe6, 22, 0, 0) != 6 ||
        SimdInternalDarwinAvx512State(0, 22, 0, 1) != 0)
        return 0;
    return 1;
}

// Capability detection must remain independent of forced scalar arithmetic.
static int TestRuntime(void)
{
    if (!CheckAvxDetection())
        return 1;
    dsimd_version_t version = SimdGetVersion();
#if defined(__wasm_simd128__)
    if (version != DSIMD_VERSION_WASM_SIMD128)
        return 1;
#elif defined(__wasm__)
    if (version != DSIMD_VERSION_SCALAR)
        return 1;
#elif defined(__APPLE__) && defined(__aarch64__)
    if (version != DSIMD_VERSION_NEON)
        return 1;
#elif defined(__x86_64__) || defined(_M_X64)
    if (version < DSIMD_VERSION_SSE2 || (version > DSIMD_VERSION_SSE42 && version != DSIMD_VERSION_AVX2 && version != DSIMD_VERSION_AVX512))
        return 1;
#else
    if (version != DSIMD_VERSION_SCALAR && version != DSIMD_VERSION_NEON)
        return 1;
#endif
    if (SimdGetVersion() != version)
        return 1;
    if (SimdInternalRuntimeVersionX86(UINT32_MAX, 0, UINT32_MAX, UINT64_MAX) != DSIMD_VERSION_SCALAR)
        return 1;
    if (SimdInternalRuntimeVersionX86(1u << 20, (1u << 24) | (1u << 25) | (1u << 26), 0, 0) != DSIMD_VERSION_SSE2)
        return 1;
    printf("Runtime SIMD capability: %u; scalar primitives forced: %u\n", (unsigned)version, DM_SIMD_FORCE_SCALAR);
    return 0;
}

// Runs primitive, numerical, pipeline and runtime detection checks for one backend.
int SimdTestPrimitives(void)
{
    CHECK(TestRuntime() == 0);
    TestPipeline();
    TestArithmetic();
    TestTruncateToInt();
    TestReferences();
    TestMultiplyAdd();
    TestSpecialValues();
    TestLaneBits();
    TestXor();
    TestShuffle();
    TestShuffle2();
    TestTranspose();
    printf("C SIMD backend: %s; %u checks, %u failures\n", DM_SIMD_BACKEND, g_Checks, g_Failures);
    return g_Failures ? 1 : 0;
}
