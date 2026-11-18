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

#ifndef DM_SIMD_SSE3_H
#define DM_SIMD_SSE3_H
#include <string.h>

// SSE3 is the configured baseline; SSE4.2 also permits SSE4.1 lane operations.
static inline dsimd_float4_t Simd128Make(float x, float y, float z, float w)
{
    return _mm_setr_ps(x, y, z, w);
}
static inline float Simd128Lane(dsimd_float4_t v, int i)
{
#if defined(__clang__) || defined(__GNUC__)
#if defined(DM_SIMD_SSE41) && (!defined(DM_SIMD_X86_ISA) || DM_SIMD_X86_ISA >= 42)
    // Keep runtime selection in registers; constant lanes still fold below.
    if (!__builtin_constant_p(i))
    {
        __m128i mask = _mm_cvtsi32_si128(0x03020100 + i * 0x04040404);
        return _mm_cvtss_f32(_mm_castsi128_ps(_mm_shuffle_epi8(_mm_castps_si128(v), mask)));
    }
#endif
    // Vector indexing lets the compiler load only the selected input lane.
    return v[i];
#else
    float lanes[4];
    _mm_storeu_ps(lanes, v);
    return lanes[i];
#endif
}
static inline dsimd_float4_t Simd128Add(dsimd_float4_t a, dsimd_float4_t b)
{
    return _mm_add_ps(a, b);
}
static inline dsimd_float4_t Simd128Sub(dsimd_float4_t a, dsimd_float4_t b)
{
    return _mm_sub_ps(a, b);
}
static inline dsimd_float4_t Simd128Mul(dsimd_float4_t a, dsimd_float4_t b)
{
    return _mm_mul_ps(a, b);
}
static inline dsimd_float4_t Simd128MulAdd(dsimd_float4_t a, dsimd_float4_t b, dsimd_float4_t c)
{
    return _mm_add_ps(_mm_mul_ps(a, b), c);
}
static inline dsimd_float4_t Simd128Div(dsimd_float4_t a, dsimd_float4_t b)
{
    return _mm_div_ps(a, b);
}
static inline dsimd_float4_t Simd128Sqrt(dsimd_float4_t a)
{
    return _mm_sqrt_ps(a);
}
static inline dsimd_float4_t Simd128Neg(dsimd_float4_t a)
{
    return _mm_xor_ps(a, _mm_set1_ps(-0.0f));
}
static inline dsimd_float4_t Simd128Abs(dsimd_float4_t a)
{
    return _mm_andnot_ps(_mm_set1_ps(-0.0f), a);
}
static inline dsimd_float4_t Simd128Xor(dsimd_float4_t a, dsimd_float4_t b)
{
    return _mm_xor_ps(a, b);
}
static inline float Simd128Dot3(dsimd_float4_t a, dsimd_float4_t b)
{
#if defined(DM_SIMD_SSE41) && (!defined(DM_SIMD_X86_ISA) || DM_SIMD_X86_ISA >= 42)
    dsimd_float4_t p = _mm_mul_ps(a, b);
    // Ignore lane 3 even when callers supply a nonzero fourth lane.
    // Horizontal adds also avoid the SSE4 dot instruction's throughput cost.
    p = _mm_and_ps(p, _mm_castsi128_ps(_mm_setr_epi32(-1, -1, -1, 0)));
    p = _mm_hadd_ps(p, p);
    return _mm_cvtss_f32(_mm_hadd_ps(p, p));
#else
    // Sum XYZ directly without adding a masked fourth-lane zero.
    dsimd_float4_t product = _mm_mul_ps(a, b);
    dsimd_float4_t sum01 = _mm_add_ss(product, _mm_shuffle_ps(product, product, _MM_SHUFFLE(1, 1, 1, 1)));
    return _mm_cvtss_f32(_mm_add_ss(sum01, _mm_movehl_ps(product, product)));
#endif
}
static inline float Simd128Dot4(dsimd_float4_t a, dsimd_float4_t b)
{
    // Horizontal adds avoid the SSE4 dot instruction's measured throughput cost.
    dsimd_float4_t product = _mm_mul_ps(a, b);
    dsimd_float4_t pairs = _mm_hadd_ps(product, product);
    return _mm_cvtss_f32(_mm_hadd_ps(pairs, pairs));
}
static inline dsimd_float4_t Simd128Shuffle(dsimd_float4_t a, int x, int y, int z, int w)
{
#if defined(DM_SIMD_SSE41) && (!defined(DM_SIMD_X86_ISA) || DM_SIMD_X86_ISA >= 42)
    __m128i indices = _mm_setr_epi32(0x03020100 + x * 0x04040404, 0x03020100 + y * 0x04040404, 0x03020100 + z * 0x04040404, 0x03020100 + w * 0x04040404);
    return _mm_castsi128_ps(_mm_shuffle_epi8(_mm_castps_si128(a), indices));
#else
    return Simd128Make(Simd128Lane(a, x), Simd128Lane(a, y), Simd128Lane(a, z), Simd128Lane(a, w));
#endif
}
static inline void Simd128Transpose(dsimd_float4_t* a, dsimd_float4_t* b, dsimd_float4_t* c, dsimd_float4_t* d)
{
    _MM_TRANSPOSE4_PS(*a, *b, *c, *d);
}

static inline dsimd_byte16_t SimdInternalExtractChannelBytes(dsimd_byte16_t a, dsimd_byte16_t b, dsimd_byte16_t c, dsimd_byte16_t d, unsigned shift)
{
    __m128i mask = _mm_set1_epi32(255);
    a = _mm_and_si128(_mm_srli_epi32(a, shift), mask);
    b = _mm_and_si128(_mm_srli_epi32(b, shift), mask);
    c = _mm_and_si128(_mm_srli_epi32(c, shift), mask);
    d = _mm_and_si128(_mm_srli_epi32(d, shift), mask);
    // Masked lanes fit in signed words and unsigned bytes before narrowing.
    return _mm_packus_epi16(_mm_packs_epi32(a, b), _mm_packs_epi32(c, d));
}
static inline void Simd128LoadInterleavedU16x2(const void* source, dsimd_ushort8_t* a, dsimd_ushort8_t* b)
{
    dsimd_uint4_t low = Simd128LoadUint(source);
    dsimd_uint4_t high = Simd128LoadUint((const uint8_t*)source + 16);
    *a = Simd128NarrowU16(low, high);
    *b = Simd128NarrowU16(Simd128ShiftRightUint(low, 16), Simd128ShiftRightUint(high, 16));
}
static inline void Simd128LoadInterleavedBytes4(const void* source, dsimd_byte16_t* a, dsimd_byte16_t* b, dsimd_byte16_t* c, dsimd_byte16_t* d)
{
    const uint8_t* bytes = (const uint8_t*)source;
    dsimd_byte16_t p0 = Simd128LoadBytes(bytes), p1 = Simd128LoadBytes(bytes + 16);
    dsimd_byte16_t p2 = Simd128LoadBytes(bytes + 32), p3 = Simd128LoadBytes(bytes + 48);
    *a = SimdInternalExtractChannelBytes(p0, p1, p2, p3, 0);
    *b = SimdInternalExtractChannelBytes(p0, p1, p2, p3, 8);
    *c = SimdInternalExtractChannelBytes(p0, p1, p2, p3, 16);
    *d = SimdInternalExtractChannelBytes(p0, p1, p2, p3, 24);
}
static inline void Simd128StoreInterleavedBytes4(void* destination, dsimd_byte16_t a, dsimd_byte16_t b, dsimd_byte16_t c, dsimd_byte16_t d)
{
    uint8_t* bytes = (uint8_t*)destination;
    __m128i  ab0 = _mm_unpacklo_epi8(a, b), cd0 = _mm_unpacklo_epi8(c, d);
    __m128i  ab1 = _mm_unpackhi_epi8(a, b), cd1 = _mm_unpackhi_epi8(c, d);
    Simd128StoreBytes(bytes, _mm_unpacklo_epi16(ab0, cd0));
    Simd128StoreBytes(bytes + 16, _mm_unpackhi_epi16(ab0, cd0));
    Simd128StoreBytes(bytes + 32, _mm_unpacklo_epi16(ab1, cd1));
    Simd128StoreBytes(bytes + 48, _mm_unpackhi_epi16(ab1, cd1));
}

static inline dsimd_uint4_t Simd128AddUint(dsimd_uint4_t a, dsimd_uint4_t b)
{
    return _mm_add_epi32(a, b);
}
static inline dsimd_uint4_t Simd128SubUint(dsimd_uint4_t a, dsimd_uint4_t b)
{
    return _mm_sub_epi32(a, b);
}
static inline dsimd_uint4_t Simd128MulUint(dsimd_uint4_t a, dsimd_uint4_t b)
{
#if defined(DM_SIMD_SSE41) && (!defined(DM_SIMD_X86_ISA) || DM_SIMD_X86_ISA >= 42)
    return _mm_mullo_epi32(a, b);
#else
    // SSE2/SSE3 multiply even lanes, then repeat for odd lanes and interleave.
    __m128i even = _mm_mul_epu32(a, b);
    __m128i odd = _mm_mul_epu32(_mm_srli_si128(a, 4), _mm_srli_si128(b, 4));
    return _mm_unpacklo_epi32(_mm_shuffle_epi32(even, 0x88), _mm_shuffle_epi32(odd, 0x88));
#endif
}
static inline dsimd_uint4_t Simd128AndUint(dsimd_uint4_t a, dsimd_uint4_t b)
{
    return _mm_and_si128(a, b);
}
static inline dsimd_uint4_t Simd128OrUint(dsimd_uint4_t a, dsimd_uint4_t b)
{
    return _mm_or_si128(a, b);
}
static inline dsimd_uint4_t Simd128XorUint(dsimd_uint4_t a, dsimd_uint4_t b)
{
    return _mm_xor_si128(a, b);
}
static inline dsimd_uint4_t Simd128EqualUint(dsimd_uint4_t a, dsimd_uint4_t b)
{
    return _mm_cmpeq_epi32(a, b);
}
static inline dsimd_uint4_t Simd128LessUint(dsimd_uint4_t a, dsimd_uint4_t b)
{
    // Bias unsigned ordering into the signed comparison domain.
    __m128i sign = _mm_set1_epi32(INT32_MIN);
    return _mm_cmpgt_epi32(_mm_xor_si128(b, sign), _mm_xor_si128(a, sign));
}
static inline dsimd_uint4_t Simd128SelectUint(dsimd_uint4_t mask, dsimd_uint4_t yes, dsimd_uint4_t no)
{
    return _mm_or_si128(_mm_and_si128(mask, yes), _mm_andnot_si128(mask, no));
}
static inline dsimd_uint4_t Simd128ShiftLeftUint(dsimd_uint4_t value, unsigned count)
{
    if (count >= 32)
        return _mm_setzero_si128();
    return _mm_sll_epi32(value, _mm_cvtsi32_si128((int)count));
}
static inline dsimd_uint4_t Simd128ShiftRightUint(dsimd_uint4_t value, unsigned count)
{
    if (count >= 32)
        return _mm_setzero_si128();
    return _mm_srl_epi32(value, _mm_cvtsi32_si128((int)count));
}
static inline dsimd_uint4_t Simd128ShiftRightUintEach(dsimd_uint4_t value, dsimd_uint4_t counts)
{
    // Baseline SSE lacks per-lane variable shifts. Apply the five count bits.
    unsigned      bit;
    dsimd_uint4_t result = value;
    for (bit = 1; bit <= 16; bit <<= 1)
    {
        dsimd_uint4_t mask = Simd128EqualUint(Simd128AndUint(counts, Simd128SplatUint(bit)), Simd128SplatUint(bit));
        result = Simd128SelectUint(mask, Simd128ShiftRightUint(result, bit), result);
    }
    return Simd128SelectUint(Simd128LessUint(counts, Simd128SplatUint(32)), result, Simd128SplatUint(0));
}
static inline dsimd_uint4_t Simd128Equal(dsimd_float4_t a, dsimd_float4_t b)
{
    return _mm_castps_si128(_mm_cmpeq_ps(a, b));
}
static inline dsimd_uint4_t Simd128Less(dsimd_float4_t a, dsimd_float4_t b)
{
    return _mm_castps_si128(_mm_cmplt_ps(a, b));
}
static inline unsigned Simd128MaskBits(dsimd_uint4_t mask)
{
    return (unsigned)_mm_movemask_ps(_mm_castsi128_ps(mask));
}
static inline dsimd_int4_t Simd128TruncateToInt(dsimd_float4_t value)
{
    return _mm_cvttps_epi32(value);
}
static inline dsimd_float4_t Simd128ConvertIntToFloat(dsimd_int4_t value)
{
    return _mm_cvtepi32_ps(value);
}
static inline dsimd_ushort8_t Simd128AddU16(dsimd_ushort8_t a, dsimd_ushort8_t b)
{
    return _mm_add_epi16(a, b);
}
static inline dsimd_ushort8_t Simd128SubU16(dsimd_ushort8_t a, dsimd_ushort8_t b)
{
    return _mm_sub_epi16(a, b);
}
static inline dsimd_ushort8_t Simd128AndU16(dsimd_ushort8_t a, dsimd_ushort8_t b)
{
    return _mm_and_si128(a, b);
}
static inline dsimd_ushort8_t Simd128OrU16(dsimd_ushort8_t a, dsimd_ushort8_t b)
{
    return _mm_or_si128(a, b);
}
static inline dsimd_ushort8_t Simd128MulU16(dsimd_ushort8_t a, dsimd_ushort8_t b)
{
    return _mm_mullo_epi16(a, b);
}
static inline dsimd_ushort8_t Simd128MulHighU16(dsimd_ushort8_t a, dsimd_ushort8_t b)
{
    return _mm_mulhi_epu16(a, b);
}
static inline dsimd_ushort8_t Simd128ShiftLeftU16(dsimd_ushort8_t value, unsigned count)
{
    if (count >= 16)
        return _mm_setzero_si128();
    return _mm_sll_epi16(value, _mm_cvtsi32_si128((int)count));
}
static inline dsimd_ushort8_t Simd128ShiftRightU16(dsimd_ushort8_t value, unsigned count)
{
    if (count >= 16)
        return _mm_setzero_si128();
    return _mm_srl_epi16(value, _mm_cvtsi32_si128((int)count));
}
static inline dsimd_byte16_t Simd128ShuffleBytes(dsimd_byte16_t value, dsimd_byte16_t indices)
{
#if defined(DM_SIMD_SSE41) && (!defined(DM_SIMD_X86_ISA) || DM_SIMD_X86_ISA >= 42)
    // PSHUFB wraps indices 16..127; make them zero like NEON/Wasm tables.
    __m128i invalid = _mm_cmpgt_epi8(indices, _mm_set1_epi8(15));
    return _mm_shuffle_epi8(value, _mm_or_si128(indices, invalid));
#else
    // Arbitrary byte lookup has no SSE3 instruction. Fixed layouts can use zip.
    uint8_t  bytes[16], lanes[16], result[16];
    unsigned i;
    Simd128StoreBytes(bytes, value);
    Simd128StoreBytes(lanes, indices);
    for (i = 0; i < 16; ++i)
        result[i] = lanes[i] < 16 ? bytes[lanes[i]] : 0;
    return Simd128LoadBytes(result);
#endif
}
static inline dsimd_byte16_t Simd128ZipBytesLow(dsimd_byte16_t a, dsimd_byte16_t b)
{
    return _mm_unpacklo_epi8(a, b);
}
static inline dsimd_ushort8_t Simd128WidenBytesLow(dsimd_byte16_t value)
{
    return _mm_unpacklo_epi8(value, _mm_setzero_si128());
}
static inline dsimd_byte16_t Simd128ZipBytesHigh(dsimd_byte16_t a, dsimd_byte16_t b)
{
    return _mm_unpackhi_epi8(a, b);
}
static inline dsimd_ushort8_t Simd128WidenBytesHigh(dsimd_byte16_t value)
{
    return _mm_unpackhi_epi8(value, _mm_setzero_si128());
}
static inline dsimd_uint4_t Simd128WidenU16Low(dsimd_ushort8_t value)
{
    return _mm_unpacklo_epi16(value, _mm_setzero_si128());
}
static inline dsimd_uint4_t Simd128WidenU16High(dsimd_ushort8_t value)
{
    return _mm_unpackhi_epi16(value, _mm_setzero_si128());
}
static inline dsimd_byte16_t Simd128PackBytes(dsimd_ushort8_t low, dsimd_ushort8_t high)
{
    // PACKUS treats its inputs as signed; first clamp unsigned words to 255.
    __m128i limit = _mm_set1_epi16(255);
    low = _mm_sub_epi16(low, _mm_subs_epu16(low, limit));
    high = _mm_sub_epi16(high, _mm_subs_epu16(high, limit));
    return _mm_packus_epi16(low, high);
}
static inline dsimd_ushort8_t Simd128PackU16(dsimd_uint4_t low, dsimd_uint4_t high)
{
    low = Simd128MinUint(low, Simd128SplatUint(65535));
    high = Simd128MinUint(high, Simd128SplatUint(65535));
#if defined(DM_SIMD_SSE41) && (!defined(DM_SIMD_X86_ISA) || DM_SIMD_X86_ISA >= 42)
    return _mm_packus_epi32(low, high);
#else
    // Bias [0,65535] into signed words for the SSE2 narrowing instruction.
    __m128i bias = _mm_set1_epi32(32768);
    return _mm_add_epi16(_mm_packs_epi32(_mm_sub_epi32(low, bias), _mm_sub_epi32(high, bias)), _mm_set1_epi16((short)-32768));
#endif
}

static inline dsimd_float4_t Simd128ConvertUintToFloat(dsimd_uint4_t value)
{
    // Halving with a sticky low bit preserves rounding for the unsigned high range.
    __m128i half = _mm_or_si128(_mm_srli_epi32(value, 1), _mm_and_si128(value, _mm_set1_epi32(1)));
    __m128  high = _mm_mul_ps(_mm_cvtepi32_ps(half), _mm_set1_ps(2.0f));
    __m128  low = _mm_cvtepi32_ps(value);
    __m128i mask = _mm_cmpgt_epi32(_mm_setzero_si128(), value);
    return _mm_or_ps(_mm_and_ps(_mm_castsi128_ps(mask), high), _mm_andnot_ps(_mm_castsi128_ps(mask), low));
}

static inline dsimd_ushort8_t Simd128NarrowU16(dsimd_uint4_t low, dsimd_uint4_t high)
{
    dsimd_uint4_t mask = Simd128SplatUint(0xffff);
    return Simd128PackU16(Simd128AndUint(low, mask), Simd128AndUint(high, mask));
}
static inline dsimd_byte16_t Simd128PackSignedBytes(dsimd_ushort8_t low, dsimd_ushort8_t high)
{
    return _mm_packus_epi16(low, high);
}

static inline dsimd_float4_t SimdInternalS16LowToFloat(__m128i value)
{
#if defined(DM_SIMD_SSE41)
    return _mm_cvtepi32_ps(_mm_cvtepi16_epi32(value));
#else
    return _mm_cvtepi32_ps(_mm_srai_epi32(_mm_unpacklo_epi16(value, value), 16));
#endif
}

static inline dsimd_float4_t Simd128LoadS16ToFloat(const void* source)
{
    return SimdInternalS16LowToFloat(_mm_loadl_epi64((const __m128i*)source));
}

static inline dsimd_float4_t Simd128LoadS8ToS16Float(const void* source)
{
    int32_t bits;
    memcpy(&bits, source, sizeof(bits));
    __m128i value = _mm_cvtsi32_si128(bits);
    value = _mm_unpacklo_epi8(value, value);
    return SimdInternalS16LowToFloat(value);
}

static inline __m128i SimdInternalFloatToS16Int(dsimd_float4_t value)
{
    __m128 minimum = _mm_set1_ps(-32768.0f), maximum = _mm_set1_ps(32767.0f);
    return _mm_cvttps_epi32(_mm_min_ps(_mm_max_ps(value, minimum), maximum));
}

static inline void Simd128StoreInterleavedS16(void* destination, dsimd_float4_t left, dsimd_float4_t right)
{
    __m128i l = SimdInternalFloatToS16Int(left);
    __m128i r = SimdInternalFloatToS16Int(right);
    _mm_storeu_si128((__m128i*)destination, _mm_packs_epi32(_mm_unpacklo_epi32(l, r), _mm_unpackhi_epi32(l, r)));
}

#endif // DM_SIMD_SSE3_H
