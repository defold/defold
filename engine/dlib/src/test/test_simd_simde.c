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

// The reference is test-only and uses portable implementations without native intrinsics.
#if defined(DM_SIMD_TEST_WIDE_BYTES)
#include "test_simd_wide_emulate.h"
#endif
#include "simde/simde.h"
#include <simd/simd.h>
#include <float.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SIMD_TEST_FUNCTIONS(F) \
    F(SimdMake) \
    F(SimdLane) \
    F(SimdAdd) \
    F(SimdSub) \
    F(SimdMul) \
    F(SimdMulAdd) \
    F(SimdDiv) \
    F(SimdSqrt) \
    F(SimdNeg) \
    F(SimdAbs) \
    F(SimdXor) \
    F(SimdDot3) \
    F(SimdDot4) \
    F(SimdShuffle) \
    F(SimdShuffle2) \
    F(SimdTranspose) \
    F(SimdLoad) \
    F(SimdStore) \
    F(SimdLoadUint) \
    F(SimdStoreUint) \
    F(SimdLoadInt) \
    F(SimdStoreInt) \
    F(SimdLoadU16) \
    F(SimdStoreU16) \
    F(SimdLoadBytes) \
    F(SimdStoreBytes) \
    F(SimdLoadInterleavedBytes4) \
    F(SimdLoadInterleavedU16x2) \
    F(SimdStoreInterleavedBytes4) \
    F(SimdSplat) \
    F(SimdMakeUint) \
    F(SimdSplatUint) \
    F(SimdSplatU16) \
    F(SimdSplatBytes) \
    F(SimdLoadBytesPartial) \
    F(SimdStoreBytesPartial) \
    F(SimdFloatBits) \
    F(SimdUintBits) \
    F(SimdBytesToUint) \
    F(SimdUintToBytes) \
    F(SimdAddUint) \
    F(SimdSubUint) \
    F(SimdMulUint) \
    F(SimdAndUint) \
    F(SimdOrUint) \
    F(SimdXorUint) \
    F(SimdNotUint) \
    F(SimdEqualUint) \
    F(SimdLessUint) \
    F(SimdSelectUint) \
    F(SimdMinUint) \
    F(SimdMaxUint) \
    F(SimdShiftLeftUint) \
    F(SimdShiftRightUint) \
    F(SimdShiftRightUintEach) \
    F(SimdEqual) \
    F(SimdLess) \
    F(SimdSelect) \
    F(SimdMin) \
    F(SimdMax) \
    F(SimdMaskBits) \
    F(SimdAny) \
    F(SimdAll) \
    F(SimdSum) \
    F(SimdReduceMin) \
    F(SimdReduceMax) \
    F(SimdTruncateToInt) \
    F(SimdConvertIntToFloat) \
    F(SimdConvertUintToFloat) \
    F(SimdAddU16) \
    F(SimdSubU16) \
    F(SimdAndU16) \
    F(SimdOrU16) \
    F(SimdMulU16) \
    F(SimdMulHighU16) \
    F(SimdShiftLeftU16) \
    F(SimdShiftRightU16) \
    F(SimdShuffleBytes) \
    F(SimdZipBytesLow) \
    F(SimdWidenBytesLow) \
    F(SimdZipBytesHigh) \
    F(SimdWidenBytesHigh) \
    F(SimdWidenU16Low) \
    F(SimdWidenU16High) \
    F(SimdPackBytes) \
    F(SimdPackU16) \
    F(SimdNarrowU16) \
    F(SimdPackSignedBytes) \
    F(SimdWideLoad) \
    F(SimdWideStore) \
    F(SimdWideSplat) \
    F(SimdWideAdd) \
    F(SimdWideMul) \
    F(SimdWideLoadBytes) \
    F(SimdWideStoreBytes) \
    F(SimdWideBytesToUint) \
    F(SimdWideUintToBytes) \
    F(SimdWideSplatUint) \
    F(SimdWideOrUint) \
    F(SimdWideSplatU16) \
    F(SimdWideAddU16) \
    F(SimdWideMulU16) \
    F(SimdWideShiftRightU16) \
    F(SimdWideWidenBytesLow) \
    F(SimdWideWidenBytesHigh) \
    F(SimdWidePackBytes) \
    F(SimdWideShuffleBytes)

enum Function
{
#define SIMD_TEST_FUNCTION(name) FUNCTION_##name,
    SIMD_TEST_FUNCTIONS(SIMD_TEST_FUNCTION)
#undef SIMD_TEST_FUNCTION
    FUNCTION_COUNT
};
static const char* g_Names[] = {
#define SIMD_TEST_FUNCTION(name) #name,
    SIMD_TEST_FUNCTIONS(SIMD_TEST_FUNCTION)
#undef SIMD_TEST_FUNCTION
};

#undef SIMD_TEST_FUNCTIONS

static unsigned g_Covered[FUNCTION_COUNT];
static unsigned g_Checks, g_Failures;
static unsigned g_Iteration;

static void     Check(enum Function function, int pass, unsigned lane)
{
    ++g_Checks;
    ++g_Covered[function];
    if (!pass)
    {
        if (g_Failures < 20)
            printf("%s: fixture %u, lane %u\n", g_Names[function], g_Iteration, lane);
        ++g_Failures;
    }
}
static void CheckRegister(enum Function function, const void* actual, simde__m128i reference)
{
    uint8_t  left[16], right[16];
    unsigned i;
    memcpy(left, actual, 16);
    simde_mm_storeu_si128(right, reference);
    for (i = 0; i < 16; ++i)
        Check(function, left[i] == right[i], i);
}
static void CheckFloat(enum Function function, dsimd_float4_t actual, simde__m128 reference, float tolerance)
{
    float    left[4], right[4];
    unsigned i;
    memcpy(left, &actual, 16);
    simde_mm_storeu_ps(right, reference);
    for (i = 0; i < 4; ++i)
    {
        int pass = isnan(right[i]) ? isnan(left[i]) : (isinf(right[i]) ? left[i] == right[i] : fabsf(left[i] - right[i]) <= tolerance * (1.0f + fabsf(right[i])));
        Check(function, pass, i);
    }
}
static void CheckScalar(enum Function function, float actual, float reference, float tolerance)
{
    int pass = isnan(reference) ? isnan(actual) : (isinf(reference) ? actual == reference : fabsf(actual - reference) <= tolerance * (1.0f + fabsf(reference)));
    Check(function, pass, 0);
}
#define CMP_REGISTER(type, name, args, reference) \
    do \
    { \
        type result = name args; \
        CheckRegister(FUNCTION_##name, &result, (reference)); \
    } while (0)
#define CMP_U(name, args, reference) CMP_REGISTER(dsimd_uint4_t, name, args, reference)
#define CMP_I(name, args, reference) CMP_REGISTER(dsimd_int4_t, name, args, reference)
#define CMP_H(name, args, reference) CMP_REGISTER(dsimd_ushort8_t, name, args, reference)
#define CMP_B(name, args, reference) CMP_REGISTER(dsimd_byte16_t, name, args, reference)
#define CMP_BITS(name, args, reference) CMP_REGISTER(dsimd_float4_t, name, args, simde_mm_castps_si128(reference))
#define CMP_F(name, args, reference, tolerance) CheckFloat(FUNCTION_##name, name args, reference, tolerance)
#define CMP_SCALAR(name, args, reference, tolerance) CheckScalar(FUNCTION_##name, name args, reference, tolerance)

static uint32_t RandomBits(uint32_t* state)
{
    *state = *state * 1664525u + 1013904223u;
    return *state;
}
static simde_uint32x4_t RefUnsigned(simde__m128i value)
{
    uint32_t words[4];
    simde_mm_storeu_si128(words, value);
    return simde_vld1q_u32(words);
}
static simde__m128i RefUnsignedBits(simde_uint32x4_t value)
{
    uint32_t words[4];
    simde_vst1q_u32(words, value);
    return simde_mm_loadu_si128(words);
}
static simde__m128 RefUnsignedFloat(simde__m128i value)
{
    float floats[4];
    simde_vst1q_f32(floats, simde_vcvtq_f32_u32(RefUnsigned(value)));
    return simde_mm_loadu_ps(floats);
}
static simde__m128i RefPackBytes(simde__m128i low, simde__m128i high)
{
    uint16_t a[8], b[8];
    uint8_t  result[16];
    simde_mm_storeu_si128(a, low);
    simde_mm_storeu_si128(b, high);
    simde_vst1q_u8(result, simde_vcombine_u8(simde_vqmovn_u16(simde_vld1q_u16(a)), simde_vqmovn_u16(simde_vld1q_u16(b))));
    return simde_mm_loadu_si128(result);
}
static simde__m128i RefPackU16(simde__m128i low, simde__m128i high)
{
    uint32_t a[4], b[4];
    uint16_t result[8];
    simde_mm_storeu_si128(a, low);
    simde_mm_storeu_si128(b, high);
    simde_vst1q_u16(result, simde_vcombine_u16(simde_vqmovn_u32(simde_vld1q_u32(a)), simde_vqmovn_u32(simde_vld1q_u32(b))));
    return simde_mm_loadu_si128(result);
}
static simde__m128i RefSelect(simde__m128i mask, simde__m128i yes, simde__m128i no)
{
    return simde_mm_or_si128(simde_mm_and_si128(mask, yes), simde_mm_andnot_si128(mask, no));
}
// Clamp reference shift operands before masking invalid counts, keeping the
// portable vector expression defined even for counts at/above 32.
static simde__m128i RefShiftEach(simde__m128i value, simde__m128i counts)
{
    simde__m128i valid = RefUnsignedBits(simde_vcltq_u32(RefUnsigned(counts), simde_vdupq_n_u32(32)));
    simde__m128i safe_counts = simde_mm_min_epu32(counts, simde_mm_set1_epi32(31));
    return RefSelect(valid, simde_mm_srlv_epi32(value, safe_counts), simde_mm_setzero_si128());
}
static simde__m128 RefShuffle(simde__m128 a, simde__m128 b, const unsigned indices[4])
{
    uint8_t  ma[16], mb[16];
    unsigned lane, byte;
    for (lane = 0; lane < 4; ++lane)
        for (byte = 0; byte < 4; ++byte)
        {
            ma[lane * 4 + byte] = (uint8_t)(indices[lane] < 4 ? indices[lane] * 4 + byte : 0x80);
            mb[lane * 4 + byte] = (uint8_t)(indices[lane] >= 4 ? (indices[lane] - 4) * 4 + byte : 0x80);
        }
    return simde_mm_castsi128_ps(simde_mm_or_si128(simde_mm_shuffle_epi8(simde_mm_castps_si128(a), simde_mm_loadu_si128(ma)),
                                                   simde_mm_shuffle_epi8(simde_mm_castps_si128(b), simde_mm_loadu_si128(mb))));
}
static simde__m128 RefReduce(simde__m128 value, int operation)
{
    simde__m128 pairs = simde_mm_shuffle_ps(value, value, 0xb1);
    pairs = operation == 0 ? simde_mm_add_ps(value, pairs) : (operation == 1 ? simde_mm_min_ps(value, pairs) : simde_mm_max_ps(value, pairs));
    simde__m128 swapped = simde_mm_shuffle_ps(pairs, pairs, 0x4e);
    return operation == 0 ? simde_mm_add_ps(pairs, swapped) : (operation == 1 ? simde_mm_min_ps(pairs, swapped) : simde_mm_max_ps(pairs, swapped));
}

// Directly compares each float operation, including conversions and mask contracts.
static void TestFloats(void)
{
    uint32_t state = 0x734625u;
    unsigned iteration, lane;
    for (iteration = 0; iteration < 512; ++iteration)
    {
        float    av[4], bv[4], cv[4], positive[4];
        uint32_t mask[4];
        for (lane = 0; lane < 4; ++lane)
        {
            av[lane] = (float)(int32_t)(RandomBits(&state) % 2049) / 64.0f - 16.0f;
            bv[lane] = (float)(RandomBits(&state) % 1023 + 1) / 64.0f;
            cv[lane] = (float)(int32_t)(RandomBits(&state) % 1025) / 64.0f - 8.0f;
            positive[lane] = bv[lane] * bv[lane];
            mask[lane] = RandomBits(&state);
        }
        g_Iteration = iteration;
        dsimd_float4_t a = SimdLoad(av), b = SimdLoad(bv), c = SimdLoad(cv);
        simde__m128    ra = simde_mm_loadu_ps(av), rb = simde_mm_loadu_ps(bv), rc = simde_mm_loadu_ps(cv);
        CMP_BITS(SimdMake, (av[0], av[1], av[2], av[3]), simde_mm_setr_ps(av[0], av[1], av[2], av[3]));
        for (lane = 0; lane < 4; ++lane)
            CMP_SCALAR(SimdLane, (a, (int)lane), simde_mm_cvtss_f32(RefShuffle(ra, ra, (unsigned[4]) { lane, lane, lane, lane })), 0);
        CMP_F(SimdAdd, (a, b), simde_mm_add_ps(ra, rb), 0);
        CMP_F(SimdSub, (a, b), simde_mm_sub_ps(ra, rb), 0);
        CMP_F(SimdMul, (a, b), simde_mm_mul_ps(ra, rb), 0);
        CMP_F(SimdDiv, (a, b), simde_mm_div_ps(ra, rb), 0);
        CMP_F(SimdSqrt, (SimdLoad(positive)), simde_mm_sqrt_ps(simde_mm_loadu_ps(positive)), 0);
        CMP_F(SimdMulAdd, (a, b, c), simde_mm_fmadd_ps(ra, rb, rc), 4 * FLT_EPSILON);
        CMP_BITS(SimdNeg, (a), simde_mm_xor_ps(ra, simde_mm_set1_ps(-0.0f)));
        CMP_BITS(SimdAbs, (a), simde_mm_andnot_ps(simde_mm_set1_ps(-0.0f), ra));
        CMP_BITS(SimdXor, (a, b), simde_mm_xor_ps(ra, rb));
        CMP_SCALAR(SimdDot3, (a, b), simde_mm_cvtss_f32(simde_mm_dp_ps(ra, rb, 0x71)), 1e-5f);
        CMP_SCALAR(SimdDot4, (a, b), simde_mm_cvtss_f32(simde_mm_dp_ps(ra, rb, 0xff)), 1e-5f);
        CMP_BITS(SimdSplat, (av[0]), simde_mm_set1_ps(av[0]));
        CMP_U(SimdFloatBits, (a), simde_mm_castps_si128(ra));
        CMP_BITS(SimdUintBits, (SimdLoadUint(mask)), simde_mm_castsi128_ps(simde_mm_loadu_si128(mask)));
        CMP_U(SimdEqual, (a, b), simde_mm_castps_si128(simde_mm_cmpeq_ps(ra, rb)));
        CMP_U(SimdLess, (a, b), simde_mm_castps_si128(simde_mm_cmplt_ps(ra, rb)));
        CMP_BITS(SimdSelect, (SimdLoadUint(mask), a, b), simde_mm_castsi128_ps(RefSelect(simde_mm_loadu_si128(mask), simde_mm_castps_si128(ra), simde_mm_castps_si128(rb))));
        CMP_BITS(SimdMin, (a, b), simde_mm_min_ps(ra, rb));
        CMP_BITS(SimdMax, (a, b), simde_mm_max_ps(ra, rb));
        CMP_SCALAR(SimdSum, (a), simde_mm_cvtss_f32(RefReduce(ra, 0)), 0);
        CMP_SCALAR(SimdReduceMin, (a), simde_mm_cvtss_f32(RefReduce(ra, 1)), 0);
        CMP_SCALAR(SimdReduceMax, (a), simde_mm_cvtss_f32(RefReduce(ra, 2)), 0);
        CMP_I(SimdTruncateToInt, (a), simde_mm_cvttps_epi32(ra));
        {
            int32_t ints[4];
            for (lane = 0; lane < 4; ++lane)
            {
                uint32_t raw = RandomBits(&state);
                memcpy(&ints[lane], &raw, 4);
            }
            CMP_F(SimdConvertIntToFloat, (SimdLoadInt(ints)), simde_mm_cvtepi32_ps(simde_mm_loadu_si128(ints)), 0);
        }
        CMP_F(SimdConvertUintToFloat, (SimdLoadUint(mask)), RefUnsignedFloat(simde_mm_loadu_si128(mask)), 0);
    }
}

// Guards conversion limits, rounding ties in unsigned conversion and zero/tie masks.
static void TestConversionAndComparisonBoundaries(void)
{
    float          input[4] = { -2147483648.0f, 2147483520.0f, -0.99f, 0.99f };
    uint32_t       uint_values[4] = { 0x8000007fu, 0x80000080u, 0x80000081u, UINT32_MAX };
    uint32_t       float_bits[4] = { 0, 0x80000000u, 0x7fc12345u, 0x7f800000u };
    dsimd_float4_t value = SimdLoad(input);
    simde__m128    ref = simde_mm_loadu_ps(input);
    CMP_I(SimdTruncateToInt, (value), simde_mm_cvttps_epi32(ref));
    CMP_F(SimdConvertUintToFloat, (SimdLoadUint(uint_values)), RefUnsignedFloat(simde_mm_loadu_si128(uint_values)), 0);
    memcpy(input, float_bits, 16);
    value = SimdLoad(input);
    ref = simde_mm_loadu_ps(input);
    CMP_U(SimdEqual, (value, value), simde_mm_castps_si128(simde_mm_cmpeq_ps(ref, ref)));
    CMP_U(SimdLess, (value, value), simde_mm_castps_si128(simde_mm_cmplt_ps(ref, ref)));
    CMP_BITS(SimdMin, (value, value), simde_mm_min_ps(ref, ref));
    CMP_BITS(SimdMax, (value, value), simde_mm_max_ps(ref, ref));
}

// Compares every integer operation over random and sign-boundary unsigned words.
static void TestIntegers(void)
{
    static const uint32_t edges[] = { 0, 1, 0x7fffffffu, 0x80000000u, UINT32_MAX, 16, 31, 32, 0xffffu, 0x10000u };
    uint32_t              state = 0x812574u;
    unsigned              iteration, lane, count;
    for (iteration = 0; iteration < 512; ++iteration)
    {
        uint32_t av[4], bv[4], mask[4], counts[4];
        g_Iteration = iteration;
        for (lane = 0; lane < 4; ++lane)
        {
            av[lane] = iteration < 10 ? edges[(iteration + lane) % 10] : RandomBits(&state);
            bv[lane] = iteration < 10 ? edges[(iteration + lane + 3) % 10] : RandomBits(&state);
            mask[lane] = RandomBits(&state);
            counts[lane] = iteration < 10 ? edges[(iteration + lane) % 10] : (iteration + lane) % 40;
        }
        dsimd_uint4_t a = SimdLoadUint(av), b = SimdLoadUint(bv);
        simde__m128i  ra = simde_mm_loadu_si128(av), rb = simde_mm_loadu_si128(bv), rm = simde_mm_loadu_si128(mask);
        CMP_U(SimdMakeUint, (av[0], av[1], av[2], av[3]), ra);
        CMP_U(SimdSplatUint, (av[0]), simde_mm_set1_epi32((int32_t)av[0]));
        // ARM's unsigned APIs avoid signed-C overflow in portable SSE add/sub fallbacks.
        CMP_U(SimdAddUint, (a, b), RefUnsignedBits(simde_vaddq_u32(RefUnsigned(ra), RefUnsigned(rb))));
        CMP_U(SimdSubUint, (a, b), RefUnsignedBits(simde_vsubq_u32(RefUnsigned(ra), RefUnsigned(rb))));
        CMP_U(SimdMulUint, (a, b), simde_mm_mullo_epi32(ra, rb));
        CMP_U(SimdAndUint, (a, b), simde_mm_and_si128(ra, rb));
        CMP_U(SimdOrUint, (a, b), simde_mm_or_si128(ra, rb));
        CMP_U(SimdXorUint, (a, b), simde_mm_xor_si128(ra, rb));
        CMP_U(SimdNotUint, (a), simde_mm_xor_si128(ra, simde_mm_set1_epi32(-1)));
        CMP_U(SimdEqualUint, (a, b), simde_mm_cmpeq_epi32(ra, rb));
        CMP_U(SimdEqualUint, (a, a), simde_mm_cmpeq_epi32(ra, ra));
        CMP_U(SimdLessUint, (a, b), RefUnsignedBits(simde_vcltq_u32(RefUnsigned(ra), RefUnsigned(rb))));
        CMP_U(SimdMinUint, (a, b), simde_mm_min_epu32(ra, rb));
        CMP_U(SimdMaxUint, (a, b), simde_mm_max_epu32(ra, rb));
        CMP_U(SimdSelectUint, (SimdLoadUint(mask), a, b), RefSelect(rm, ra, rb));
        CMP_U(SimdShiftRightUintEach, (a, SimdLoadUint(counts)), RefShiftEach(ra, simde_mm_loadu_si128(counts)));
        for (count = 0; count <= 33; ++count)
        {
            simde__m128i shift = simde_mm_cvtsi32_si128((int32_t)count);
            CMP_U(SimdShiftLeftUint, (a, count), simde_mm_sll_epi32(ra, shift));
            CMP_U(SimdShiftRightUint, (a, count), simde_mm_srl_epi32(ra, shift));
        }
        {
            dsimd_uint4_t m = SimdMakeUint(iteration & 1 ? UINT32_MAX : 0, iteration & 2 ? UINT32_MAX : 0, iteration & 4 ? UINT32_MAX : 0, iteration & 8 ? UINT32_MAX : 0);
            uint32_t      raw[4] = { iteration & 1 ? UINT32_MAX : 0, iteration & 2 ? UINT32_MAX : 0, iteration & 4 ? UINT32_MAX : 0, iteration & 8 ? UINT32_MAX : 0 };
            unsigned      expected = (unsigned)simde_mm_movemask_ps(simde_mm_castsi128_ps(simde_mm_loadu_si128(raw)));
            Check(FUNCTION_SimdMaskBits, SimdMaskBits(m) == expected, 0);
            Check(FUNCTION_SimdAny, SimdAny(m) == (expected != 0), 0);
            Check(FUNCTION_SimdAll, SimdAll(m) == (expected == 15), 0);
        }
    }
}

// Verifies byte layouts and unsigned pack/widen contracts against SIMDe equivalents.
static void TestBytesAndWords(void)
{
    uint32_t state = 0x189265u;
    unsigned iteration, lane, count;
    for (iteration = 0; iteration < 512; ++iteration)
    {
        uint8_t  bytes[16], other[16], indices[16], normalized[16];
        uint16_t av[8], bv[8];
        uint32_t u[4], v[4];
        g_Iteration = iteration;
        for (lane = 0; lane < 16; ++lane)
        {
            bytes[lane] = (uint8_t)RandomBits(&state);
            other[lane] = (uint8_t)RandomBits(&state);
            indices[lane] = (uint8_t)(iteration + lane);
            normalized[lane] = indices[lane] < 16 ? indices[lane] : 0x80;
        }
        for (lane = 0; lane < 8; ++lane)
        {
            av[lane] = (uint16_t)RandomBits(&state);
            bv[lane] = (uint16_t)RandomBits(&state);
        }
        for (lane = 0; lane < 4; ++lane)
        {
            u[lane] = RandomBits(&state);
            v[lane] = RandomBits(&state);
        }
        dsimd_byte16_t  a = SimdLoadBytes(bytes), b = SimdLoadBytes(other);
        dsimd_ushort8_t h = SimdLoadU16(av), k = SimdLoadU16(bv);
        simde__m128i    ra = simde_mm_loadu_si128(bytes), rb = simde_mm_loadu_si128(other), rh = simde_mm_loadu_si128(av), rk = simde_mm_loadu_si128(bv);
        CMP_B(SimdSplatBytes, (bytes[0]), simde_mm_set1_epi8((int8_t)bytes[0]));
        CMP_H(SimdSplatU16, (av[0]), simde_mm_set1_epi16((int16_t)av[0]));
        CMP_U(SimdBytesToUint, (a), ra);
        CMP_B(SimdUintToBytes, (SimdLoadUint(u)), simde_mm_loadu_si128(u));
        CMP_B(SimdShuffleBytes, (a, SimdLoadBytes(indices)), simde_mm_shuffle_epi8(ra, simde_mm_loadu_si128(normalized)));
        CMP_B(SimdZipBytesLow, (a, b), simde_mm_unpacklo_epi8(ra, rb));
        CMP_B(SimdZipBytesHigh, (a, b), simde_mm_unpackhi_epi8(ra, rb));
        CMP_H(SimdWidenBytesLow, (a), simde_mm_cvtepu8_epi16(ra));
        CMP_H(SimdWidenBytesHigh, (a), simde_mm_cvtepu8_epi16(simde_mm_srli_si128(ra, 8)));
        CMP_U(SimdWidenU16Low, (h), simde_mm_cvtepu16_epi32(rh));
        CMP_U(SimdWidenU16High, (h), simde_mm_cvtepu16_epi32(simde_mm_srli_si128(rh, 8)));
        CMP_H(SimdAddU16, (h, k), simde_mm_add_epi16(rh, rk));
        CMP_H(SimdSubU16, (h, k), simde_mm_sub_epi16(rh, rk));
        CMP_H(SimdAndU16, (h, k), simde_mm_and_si128(rh, rk));
        CMP_H(SimdOrU16, (h, k), simde_mm_or_si128(rh, rk));
        CMP_H(SimdMulU16, (h, k), simde_mm_mullo_epi16(rh, rk));
        CMP_H(SimdMulHighU16, (h, k), simde_mm_mulhi_epu16(rh, rk));
        CMP_B(SimdPackBytes, (h, k), RefPackBytes(rh, rk));
        CMP_H(SimdPackU16, (SimdLoadUint(u), SimdLoadUint(v)), RefPackU16(simde_mm_loadu_si128(u), simde_mm_loadu_si128(v)));
        CMP_H(SimdNarrowU16, (SimdLoadUint(u), SimdLoadUint(v)), RefPackU16(simde_mm_and_si128(simde_mm_loadu_si128(u), simde_mm_set1_epi32(0xffff)), simde_mm_and_si128(simde_mm_loadu_si128(v), simde_mm_set1_epi32(0xffff))));
        CMP_B(SimdPackSignedBytes, (h, k), simde_mm_packus_epi16(rh, rk));
        for (count = 0; count <= 17; ++count)
        {
            simde__m128i shift = simde_mm_cvtsi32_si128((int32_t)count);
            CMP_H(SimdShiftLeftU16, (h, count), simde_mm_sll_epi16(rh, shift));
            CMP_H(SimdShiftRightU16, (h, count), simde_mm_srl_epi16(rh, shift));
        }
    }
}

// Covers unaligned full transfers and every bounded byte tail without overreads.
static void TestMemory(void)
{
    uint8_t  source[33], actual[33], expected[33];
    unsigned i, count;
    for (i = 0; i < 33; ++i)
        source[i] = (uint8_t)(i * 31 + 5);
    simde__m128i reference = simde_mm_loadu_si128(source + 1);
    CMP_BITS(SimdLoad, (source + 1), simde_mm_castsi128_ps(reference));
    CMP_U(SimdLoadUint, (source + 1), reference);
    CMP_I(SimdLoadInt, (source + 1), reference);
    CMP_H(SimdLoadU16, (source + 1), reference);
    CMP_B(SimdLoadBytes, (source + 1), reference);
#define CHECK_STORE(name, value) \
    do \
    { \
        memset(actual, 0xa5, 33); \
        memset(expected, 0xa5, 33); \
        name(actual + 1, value); \
        simde_mm_storeu_si128(expected + 1, reference); \
        for (i = 0; i < 33; ++i) \
            Check(FUNCTION_##name, actual[i] == expected[i], i); \
    } while (0)
    CHECK_STORE(SimdStore, SimdLoad(source + 1));
    CHECK_STORE(SimdStoreUint, SimdLoadUint(source + 1));
    CHECK_STORE(SimdStoreInt, SimdLoadInt(source + 1));
    CHECK_STORE(SimdStoreU16, SimdLoadU16(source + 1));
    CHECK_STORE(SimdStoreBytes, SimdLoadBytes(source + 1));
#undef CHECK_STORE
    for (count = 0; count <= 16; ++count)
    {
        uint8_t* allocation = count ? (uint8_t*)malloc(count + 1) : 0;
        uint8_t* input = count ? allocation + 1 : 0;
        uint8_t* output = count ? (uint8_t*)malloc(count) : 0;
        uint8_t  padded[16] = { 0 }, full[16];
        for (i = 0; i < count; ++i)
        {
            input[i] = source[i];
            padded[i] = source[i];
        }
        CMP_B(SimdLoadBytesPartial, (input, count), simde_mm_loadu_si128(padded));
        dsimd_byte16_t value = SimdLoadBytes(source + 1);
        simde_mm_storeu_si128(full, reference);
        SimdStoreBytesPartial(output, value, count);
        Check(FUNCTION_SimdStoreBytesPartial, count == 0 || memcmp(output, full, count) == 0, 0);
        free(allocation);
        free(output);
    }
}

// Compares both word channels and every source alignment against portable SIMDe LD2.
static void TestInterleavedWords(void)
{
    uint8_t  input[48];
    uint16_t words[16], expected[8];
    unsigned offset, seed, lane;
    for (offset = 0; offset < 16; ++offset)
        for (seed = 0; seed < 256; ++seed)
        {
            dsimd_ushort8_t a, b;
            for (lane = 0; lane < sizeof(input); ++lane)
                input[lane] = (uint8_t)(lane * 37 + seed);
            memcpy(words, input + offset, sizeof(words));
            simde_uint16x8x2_t reference = simde_vld2q_u16(words);
            SimdLoadInterleavedU16x2(input + offset, &a, &b);
            simde_vst1q_u16(expected, reference.val[0]);
            CheckRegister(FUNCTION_SimdLoadInterleavedU16x2, &a, simde_mm_loadu_si128(expected));
            simde_vst1q_u16(expected, reference.val[1]);
            CheckRegister(FUNCTION_SimdLoadInterleavedU16x2, &b, simde_mm_loadu_si128(expected));
        }
}

// Compare every channel and unaligned transfer against portable SIMDe LD4/ST4.
static void TestInterleavedBytes(void)
{
    uint8_t  input[80], actual[80], expected[80], channel_bytes[16];
    unsigned offset, seed, lane, channel;
    for (offset = 0; offset < 16; ++offset)
        for (seed = 0; seed < 256; ++seed)
        {
            dsimd_byte16_t channels[4];
            g_Iteration = seed * 16 + offset;
            for (lane = 0; lane < sizeof(input); ++lane)
                input[lane] = (uint8_t)(lane * 37 + seed);
            simde_uint8x16x4_t reference = simde_vld4q_u8(input + offset);
            SimdLoadInterleavedBytes4(input + offset, &channels[0], &channels[1], &channels[2], &channels[3]);
            for (channel = 0; channel < 4; ++channel)
            {
                simde_vst1q_u8(channel_bytes, reference.val[channel]);
                CheckRegister(FUNCTION_SimdLoadInterleavedBytes4, &channels[channel], simde_mm_loadu_si128(channel_bytes));
            }
            // Store inputs come from the external reference, not our load's output.
            for (channel = 0; channel < 4; ++channel)
            {
                simde_vst1q_u8(channel_bytes, reference.val[channel]);
                channels[3 - channel] = SimdLoadBytes(channel_bytes);
            }
            memset(actual, 0xa5, sizeof(actual));
            memset(expected, 0xa5, sizeof(expected));
            SimdStoreInterleavedBytes4(actual + 15 - offset, channels[0], channels[1], channels[2], channels[3]);
            simde_uint8x16x4_t reversed;
            for (channel = 0; channel < 4; ++channel)
                reversed.val[channel] = reference.val[3 - channel];
            simde_vst4q_u8(expected + 15 - offset, reversed);
            for (lane = 0; lane < sizeof(actual); ++lane)
                Check(FUNCTION_SimdStoreInterleavedBytes4, actual[lane] == expected[lane], lane);
        }
}

// Exhausts runtime lane selections, preserving NaN payloads and zero sign bits.
static void TestShufflesAndSpecialValues(void)
{
    static const uint32_t bits[8] = { 0, 0x80000000u, 0x7fc12345u, 0xffc54321u, 0x7f800000u, 0xff800000u, 1, 0x80000001u };
    float                 left[4], right[4];
    unsigned              code, lane;
    memcpy(left, bits, 16);
    memcpy(right, bits + 4, 16);
    dsimd_float4_t a = SimdLoad(left), b = SimdLoad(right);
    simde__m128    ra = simde_mm_loadu_ps(left), rb = simde_mm_loadu_ps(right);
    for (code = 0; code < 4096; ++code)
    {
        unsigned idx[4];
        g_Iteration = code;
        for (lane = 0; lane < 4; ++lane)
            idx[lane] = (code >> (lane * 3)) & 7;
        CMP_BITS(SimdShuffle2, (a, b, (int)idx[0], (int)idx[1], (int)idx[2], (int)idx[3]), RefShuffle(ra, rb, idx));
        if (code < 256)
        {
            for (lane = 0; lane < 4; ++lane)
                idx[lane] = (code >> (lane * 2)) & 3;
            CMP_BITS(SimdShuffle, (a, (int)idx[0], (int)idx[1], (int)idx[2], (int)idx[3]), RefShuffle(ra, ra, idx));
        }
    }
    CMP_BITS(SimdNeg, (a), simde_mm_xor_ps(ra, simde_mm_set1_ps(-0.0f)));
    CMP_BITS(SimdAbs, (a), simde_mm_andnot_ps(simde_mm_set1_ps(-0.0f), ra));
    CMP_U(SimdEqual, (a, b), simde_mm_castps_si128(simde_mm_cmpeq_ps(ra, rb)));
    CMP_U(SimdLess, (a, b), simde_mm_castps_si128(simde_mm_cmplt_ps(ra, rb)));
    CMP_BITS(SimdMin, (a, b), simde_mm_min_ps(ra, rb));
    CMP_BITS(SimdMax, (a, b), simde_mm_max_ps(ra, rb));
    CMP_SCALAR(SimdReduceMin, (a), simde_mm_cvtss_f32(RefReduce(ra, 1)), 0);
    CMP_SCALAR(SimdReduceMax, (a), simde_mm_cvtss_f32(RefReduce(ra, 2)), 0);
    {
        dsimd_float4_t values[4];
        simde__m128    refs[4];
        unsigned       col;
        for (col = 0; col < 4; ++col)
        {
            uint32_t raw[4];
            for (lane = 0; lane < 4; ++lane)
                raw[lane] = bits[(col + lane) % 8];
            memcpy(left, raw, 16);
            values[col] = SimdLoad(left);
            refs[col] = simde_mm_loadu_ps(left);
        }
        SimdTranspose(&values[0], &values[1], &values[2], &values[3]);
        SIMDE_MM_TRANSPOSE4_PS(refs[0], refs[1], refs[2], refs[3]);
        for (col = 0; col < 4; ++col)
            CheckRegister(FUNCTION_SimdTranspose, &values[col], simde_mm_castps_si128(refs[col]));
    }
}

#if DM_SIMD_WIDE_BYTES == 64
#define WIDE_REF(name) simde_mm512_##name
#define WIDE_REF_LOAD simde_mm512_loadu_si512
#define WIDE_REF_ZERO simde_mm512_setzero_si512
#define WIDE_REF_OR simde_mm512_or_si512
typedef simde__m512i WideReferenceInt;
typedef simde__m512  WideReferenceFloat;
#else
#define WIDE_REF(name) simde_mm256_##name
#define WIDE_REF_LOAD simde_mm256_loadu_si256
#define WIDE_REF_ZERO simde_mm256_setzero_si256
#define WIDE_REF_OR simde_mm256_or_si256
typedef simde__m256i WideReferenceInt;
typedef simde__m256  WideReferenceFloat;
#endif

// Compares every wide primitive and each 128-bit sub-block against portable SIMDe.
static void TestWide(void)
{
    uint32_t state = 0x683412u;
    unsigned iteration, lane, count;
    for (iteration = 0; iteration < 128; ++iteration)
    {
        uint8_t  bytes[DM_SIMD_WIDE_BYTES], indices[DM_SIMD_WIDE_BYTES], normalized[DM_SIMD_WIDE_BYTES], output[DM_SIMD_WIDE_BYTES];
        uint16_t av[DM_SIMD_WIDE_BYTES / 2], bv[DM_SIMD_WIDE_BYTES / 2];
        float    fv[DM_SIMD_WIDE_BYTES / 4], gv[DM_SIMD_WIDE_BYTES / 4];
        g_Iteration = iteration;
        for (lane = 0; lane < DM_SIMD_WIDE_BYTES; ++lane)
        {
            bytes[lane] = (uint8_t)RandomBits(&state);
            indices[lane] = (uint8_t)(iteration * 7 + lane);
            normalized[lane] = indices[lane] < 16 ? indices[lane] : 0x80;
        }
        for (lane = 0; lane < DM_SIMD_WIDE_BYTES / 2; ++lane)
        {
            av[lane] = (uint16_t)RandomBits(&state);
            bv[lane] = (uint16_t)RandomBits(&state);
        }
        for (lane = 0; lane < DM_SIMD_WIDE_BYTES / 4; ++lane)
        {
            fv[lane] = (float)(lane + iteration) - 8.0f;
            gv[lane] = (float)(lane + 1) * 0.25f;
        }
        dsimd_wide_byte_t   bytes_value = SimdWideLoadBytes(bytes);
        dsimd_wide_ushort_t a, b;
        memcpy(&a, av, DM_SIMD_WIDE_BYTES);
        memcpy(&b, bv, DM_SIMD_WIDE_BYTES);
        WideReferenceInt   ra = WIDE_REF_LOAD(av), rb = WIDE_REF_LOAD(bv), rbytes = WIDE_REF_LOAD(bytes);
        WideReferenceFloat rf = WIDE_REF(loadu_ps)(fv), rg = WIDE_REF(loadu_ps)(gv);
#define CMP_WIDE(type, ref_type, name, args, reference) \
    do \
    { \
        type actual = name args; \
        ref_type           expected = reference; \
        uint8_t            left[DM_SIMD_WIDE_BYTES], right[DM_SIMD_WIDE_BYTES]; \
        unsigned           byte; \
        memcpy(left, &actual, DM_SIMD_WIDE_BYTES); \
        memcpy(right, &expected, DM_SIMD_WIDE_BYTES); \
        for (byte = 0; byte < DM_SIMD_WIDE_BYTES; ++byte) \
            Check(FUNCTION_##name, left[byte] == right[byte], byte); \
    } while (0)
        CMP_WIDE(dsimd_wide_byte_t, WideReferenceInt, SimdWideLoadBytes, (bytes), rbytes);
        CMP_WIDE(dsimd_wide_uint_t, WideReferenceInt, SimdWideBytesToUint, (bytes_value), rbytes);
        CMP_WIDE(dsimd_wide_byte_t, WideReferenceInt, SimdWideUintToBytes, (SimdWideBytesToUint(bytes_value)), rbytes);
        CMP_WIDE(dsimd_wide_uint_t, WideReferenceInt, SimdWideSplatUint, (0x87654321u), WIDE_REF(set1_epi32)((int32_t)0x87654321u));
        CMP_WIDE(dsimd_wide_uint_t, WideReferenceInt, SimdWideOrUint, (SimdWideBytesToUint(bytes_value), SimdWideSplatUint(0x87654321u)), WIDE_REF_OR(rbytes, WIDE_REF(set1_epi32)((int32_t)0x87654321u)));
        CMP_WIDE(dsimd_wide_ushort_t, WideReferenceInt, SimdWideSplatU16, (65535), WIDE_REF(set1_epi16)(-1));
        CMP_WIDE(dsimd_wide_ushort_t, WideReferenceInt, SimdWideAddU16, (a, b), WIDE_REF(add_epi16)(ra, rb));
        CMP_WIDE(dsimd_wide_ushort_t, WideReferenceInt, SimdWideMulU16, (a, b), WIDE_REF(mullo_epi16)(ra, rb));
        CMP_WIDE(dsimd_wide_ushort_t, WideReferenceInt, SimdWideWidenBytesLow, (bytes_value), WIDE_REF(unpacklo_epi8)(rbytes, WIDE_REF_ZERO()));
        CMP_WIDE(dsimd_wide_ushort_t, WideReferenceInt, SimdWideWidenBytesHigh, (bytes_value), WIDE_REF(unpackhi_epi8)(rbytes, WIDE_REF_ZERO()));
        CMP_WIDE(dsimd_wide_byte_t, WideReferenceInt, SimdWidePackBytes, (a, b), WIDE_REF(packus_epi16)(WIDE_REF(min_epu16)(ra, WIDE_REF(set1_epi16)(255)), WIDE_REF(min_epu16)(rb, WIDE_REF(set1_epi16)(255))));
        CMP_WIDE(dsimd_wide_byte_t, WideReferenceInt, SimdWideShuffleBytes, (bytes_value, SimdWideLoadBytes(indices)), WIDE_REF(shuffle_epi8)(rbytes, WIDE_REF_LOAD(normalized)));
        for (count = 0; count <= 17; ++count)
            CMP_WIDE(dsimd_wide_ushort_t, WideReferenceInt, SimdWideShiftRightU16, (a, count), WIDE_REF(srl_epi16)(ra, simde_mm_cvtsi32_si128((int32_t)count)));
        CMP_WIDE(dsimd_wide_float_t, WideReferenceFloat, SimdWideLoad, (fv), rf);
        CMP_WIDE(dsimd_wide_float_t, WideReferenceFloat, SimdWideSplat, (-3.5f), WIDE_REF(set1_ps)(-3.5f));
        CMP_WIDE(dsimd_wide_float_t, WideReferenceFloat, SimdWideAdd, (SimdWideLoad(fv), SimdWideLoad(gv)), WIDE_REF(add_ps)(rf, rg));
        CMP_WIDE(dsimd_wide_float_t, WideReferenceFloat, SimdWideMul, (SimdWideLoad(fv), SimdWideLoad(gv)), WIDE_REF(mul_ps)(rf, rg));
        SimdWideStoreBytes(output, bytes_value);
        for (lane = 0; lane < DM_SIMD_WIDE_BYTES; ++lane)
            Check(FUNCTION_SimdWideStoreBytes, output[lane] == bytes[lane], lane);
        SimdWideStore(output, SimdWideLoad(fv));
        for (lane = 0; lane < DM_SIMD_WIDE_BYTES; ++lane)
            Check(FUNCTION_SimdWideStore, output[lane] == ((uint8_t*)fv)[lane], lane);
#undef CMP_WIDE
    }
}
#undef WIDE_REF
#undef WIDE_REF_LOAD
#undef WIDE_REF_ZERO
#undef WIDE_REF_OR

// Compares all public primitive functions with the independent SIMDe reference.
int SimdTestReference(void)
{
    unsigned function, covered = 0;
    TestWide();
    TestFloats();
    TestIntegers();
    TestConversionAndComparisonBoundaries();
    TestBytesAndWords();
    TestMemory();
    TestInterleavedBytes();
    TestInterleavedWords();
    TestShufflesAndSpecialValues();
    for (function = 0; function < FUNCTION_COUNT; ++function)
    {
        if (g_Covered[function])
            ++covered;
        else
        {
            printf("Missing differential coverage: %s\n", g_Names[function]);
            ++g_Failures;
        }
    }
    printf("SIMDe portable reference: %s; %u/%u functions; %u checks; %u failures\n", DM_SIMD_BACKEND, covered, FUNCTION_COUNT, g_Checks, g_Failures);
    return g_Failures ? 1 : 0;
}
