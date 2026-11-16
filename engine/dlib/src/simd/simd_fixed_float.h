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

#ifndef DM_SIMD_FIXED_FLOAT_H
#define DM_SIMD_FIXED_FLOAT_H

#include "simd.h"

/*# Fixed-width float SIMD for audio and other bulk processing.
 * Simd8 and Simd16 hold exactly eight and sixteen consecutive float lanes.
 * AVX2/AVX-512 kernels use native registers. Opt-in SME kernels use a 512-bit
 * streaming vector for Simd16; other backends compose two/four 128-bit registers.
 * DM_SIMD_FORCE_SCALAR keeps the logical widths with scalar
 * primitives. Values stay within one kernel and must not cross an ISA dispatch
 * boundary or appear in exported layouts.
 * Load/Store access exactly 32/64 unaligned bytes. Splat repeats one value;
 * Add/Mul/Min/Max operate component-wise. Ramp(start,step) produces start+i*step.
 * Combine concatenates two/four 128-bit blocks in argument order.
 * Lane indexes must be in [0,8) or [0,16). Sum/ReduceMax reduce all lanes.
 * Unzip splits consecutive interleaved lanes in a,b into distinct left/right
 * output registers. LoadS16ToFloat/LoadS8ToS16Float access exactly 8/16 signed
 * samples. S16 preserves their numeric values; S8 replicates each byte into a
 * signed 16-bit word before conversion, matching PCM8 decoding. StoreInterleavedS16
 * clamps finite floats to [-32768,32767], truncates toward zero and writes
 * exactly 8/16 stereo frames, left first. Floating-point reductions and ramps
 * may round differently across widths. Arithmetic inputs must be finite.
 * @document
 * @name SIMD Fixed Float
 * @language C
 */

#ifdef __cplusplus
extern "C"
{
#endif

#if !DM_SIMD_FORCE_SCALAR && defined(DM_SIMD_AVX2)
#define DM_SIMD_FIXED8_NATIVE
typedef __m256 dsimd_float8_t;
#else
typedef struct dsimd_float8_t
{
    dsimd_float4_t m_Blocks[2];
} dsimd_float8_t;
#endif

static inline dsimd_float8_t Simd8Combine(dsimd_float4_t a, dsimd_float4_t b)
{
#if defined(DM_SIMD_FIXED8_NATIVE)
    return _mm256_insertf128_ps(_mm256_castps128_ps256(a), b, 1);
#else
    dsimd_float8_t result;
    result.m_Blocks[0] = a;
    result.m_Blocks[1] = b;
    return result;
#endif
}

static inline dsimd_float8_t Simd8Load(const void* source)
{
#if defined(DM_SIMD_FIXED8_NATIVE)
    return _mm256_loadu_ps((const float*)source);
#else
    dsimd_float8_t result;
    unsigned i;
    for (i = 0; i < 2; ++i)
    {
        result.m_Blocks[i] = SimdLoad((const uint8_t*)source + i * 16);
    }

    return result;
#endif
}

static inline void Simd8Store(void* destination, dsimd_float8_t value)
{
#if defined(DM_SIMD_FIXED8_NATIVE)
    _mm256_storeu_ps((float*)destination, value);
#else
    unsigned i;
    for (i = 0; i < 2; ++i)
    {
        SimdStore((uint8_t*)destination + i * 16, value.m_Blocks[i]);
    }

#endif
}

static inline dsimd_float8_t Simd8Splat(float value)
{
#if defined(DM_SIMD_FIXED8_NATIVE)
    return _mm256_set1_ps(value);
#else
    dsimd_float8_t result;
    unsigned i;
    for (i = 0; i < 2; ++i)
    {
        result.m_Blocks[i] = SimdSplat(value);
    }

    return result;
#endif
}

static inline dsimd_float8_t Simd8Add(dsimd_float8_t a, dsimd_float8_t b)
{
#if defined(DM_SIMD_FIXED8_NATIVE)
    return _mm256_add_ps(a, b);
#else
    dsimd_float8_t result;
    unsigned i;
    for (i = 0; i < 2; ++i)
    {
        result.m_Blocks[i] = SimdAdd(a.m_Blocks[i], b.m_Blocks[i]);
    }

    return result;
#endif
}

static inline dsimd_float8_t Simd8Mul(dsimd_float8_t a, dsimd_float8_t b)
{
#if defined(DM_SIMD_FIXED8_NATIVE)
    return _mm256_mul_ps(a, b);
#else
    dsimd_float8_t result;
    unsigned i;
    for (i = 0; i < 2; ++i)
    {
        result.m_Blocks[i] = SimdMul(a.m_Blocks[i], b.m_Blocks[i]);
    }

    return result;
#endif
}

static inline dsimd_float8_t Simd8Min(dsimd_float8_t a, dsimd_float8_t b)
{
#if defined(DM_SIMD_FIXED8_NATIVE)
    return _mm256_min_ps(a, b);
#else
    dsimd_float8_t result;
    unsigned i;
    for (i = 0; i < 2; ++i)
    {
        result.m_Blocks[i] = SimdMin(a.m_Blocks[i], b.m_Blocks[i]);
    }

    return result;
#endif
}

static inline dsimd_float8_t Simd8Max(dsimd_float8_t a, dsimd_float8_t b)
{
#if defined(DM_SIMD_FIXED8_NATIVE)
    return _mm256_max_ps(a, b);
#else
    dsimd_float8_t result;
    unsigned i;
    for (i = 0; i < 2; ++i)
    {
        result.m_Blocks[i] = SimdMax(a.m_Blocks[i], b.m_Blocks[i]);
    }

    return result;
#endif
}

static inline dsimd_float8_t Simd8Ramp(float start, float step)
{
    static const float indices[8] = {
        0.0f, 1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f
    };
    return Simd8Add(Simd8Splat(start), Simd8Mul(Simd8Splat(step), Simd8Load(indices)));
}

static inline float Simd8Lane(dsimd_float8_t value, unsigned index)
{
#if defined(DM_SIMD_FIXED8_NATIVE)
    // Scalar tails only need the low lane; keep its extraction in registers.
    if (index == 0)
    {
        return _mm_cvtss_f32(_mm256_castps256_ps128(value));
    }

    float lanes[8];
    _mm256_storeu_ps(lanes, value);
    return lanes[index];
#else
    return SimdLane(value.m_Blocks[index / 4], index & 3);
#endif
}

static inline float Simd8Sum(dsimd_float8_t value)
{
#if defined(DM_SIMD_FIXED8_NATIVE)
    return SimdSum(_mm_add_ps(_mm256_castps256_ps128(value), _mm256_extractf128_ps(value, 1)));
#else
    dsimd_float4_t sum = value.m_Blocks[0];
    unsigned i;
    for (i = 1; i < 2; ++i)
    {
        sum = SimdAdd(sum, value.m_Blocks[i]);
    }

    return SimdSum(sum);
#endif
}

static inline float Simd8ReduceMax(dsimd_float8_t value)
{
#if defined(DM_SIMD_FIXED8_NATIVE)
    return SimdReduceMax(_mm_max_ps(_mm256_castps256_ps128(value), _mm256_extractf128_ps(value, 1)));
#else
    dsimd_float4_t maximum = value.m_Blocks[0];
    unsigned i;
    for (i = 1; i < 2; ++i)
    {
        maximum = SimdMax(maximum, value.m_Blocks[i]);
    }

    return SimdReduceMax(maximum);
#endif
}

static inline dsimd_float8_t Simd8LoadS8ToS16Float(const void* source)
{
#if defined(DM_SIMD_FIXED8_NATIVE)
    return _mm256_cvtepi32_ps(_mm256_cvtepi16_epi32(_mm_unpacklo_epi8(_mm_loadl_epi64((const __m128i*)source), _mm_loadl_epi64((const __m128i*)source))));
#elif !DM_SIMD_FORCE_SCALAR && defined(DM_SIMD_WASM)
    v128_t bytes = wasm_v128_load64_splat(source);
    v128_t words = wasm_i8x16_shuffle(bytes, bytes, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7);
    dsimd_float8_t result;
    result.m_Blocks[0] = wasm_f32x4_convert_i32x4(wasm_i32x4_extend_low_i16x8(words));
    result.m_Blocks[1] = wasm_f32x4_convert_i32x4(wasm_i32x4_extend_high_i16x8(words));
    return result;
#else
    dsimd_float8_t result;
    unsigned i;
    for (i = 0; i < 2; ++i)
    {
        result.m_Blocks[i] = SimdLoadS8ToS16Float((const uint8_t*)source + i * 4);
    }

    return result;
#endif
}

static inline dsimd_float8_t Simd8LoadS16ToFloat(const void* source)
{
#if defined(DM_SIMD_FIXED8_NATIVE)
    return _mm256_cvtepi32_ps(_mm256_cvtepi16_epi32(_mm_loadu_si128((const __m128i*)source)));
#elif defined(DM_SIMD_X86_H)
    __m128i value = _mm_loadu_si128((const __m128i*)source);
    dsimd_float8_t result;
    result.m_Blocks[0] = SimdInternalS16LowToFloat(value);
    result.m_Blocks[1] = SimdInternalS16HighToFloat(value);
    return result;
#else
    dsimd_float8_t result;
    unsigned i;
    for (i = 0; i < 2; ++i)
    {
        result.m_Blocks[i] = SimdLoadS16ToFloat((const uint8_t*)source + i * 8);
    }

    return result;
#endif
}

static inline void Simd8Unzip(dsimd_float8_t a, dsimd_float8_t b, dsimd_float8_t* left, dsimd_float8_t* right)
{
#if defined(DM_SIMD_FIXED8_NATIVE)
    __m256i order = _mm256_setr_epi32(0, 1, 4, 5, 2, 3, 6, 7);
    *left = _mm256_permutevar8x32_ps(_mm256_shuffle_ps(a, b, _MM_SHUFFLE(2, 0, 2, 0)), order);
    *right = _mm256_permutevar8x32_ps(_mm256_shuffle_ps(a, b, _MM_SHUFFLE(3, 1, 3, 1)), order);
#else
    unsigned i;
    for (i = 0; i < 1; ++i)
    {
        SimdUnzip(a.m_Blocks[2 * i], a.m_Blocks[2 * i + 1], &left->m_Blocks[i], &right->m_Blocks[i]);
        SimdUnzip(b.m_Blocks[2 * i], b.m_Blocks[2 * i + 1], &left->m_Blocks[i + 1], &right->m_Blocks[i + 1]);
    }

#endif
}

static inline void Simd8StoreInterleavedS16(void* destination, dsimd_float8_t left, dsimd_float8_t right)
{
#if defined(DM_SIMD_FIXED8_NATIVE)
    dsimd_float8_t minimum = _mm256_set1_ps(-32768.0f), maximum = _mm256_set1_ps(32767.0f);
    __m256i l = _mm256_cvttps_epi32(_mm256_min_ps(_mm256_max_ps(left, minimum), maximum));
    __m256i r = _mm256_cvttps_epi32(_mm256_min_ps(_mm256_max_ps(right, minimum), maximum));
    _mm256_storeu_si256((__m256i*)destination, _mm256_packs_epi32(_mm256_unpacklo_epi32(l, r), _mm256_unpackhi_epi32(l, r)));
#elif defined(DM_SIMD_X86_H)
    __m128i l = _mm_packs_epi32(SimdInternalFloatToS16Int(left.m_Blocks[0]), SimdInternalFloatToS16Int(left.m_Blocks[1]));
    __m128i r = _mm_packs_epi32(SimdInternalFloatToS16Int(right.m_Blocks[0]), SimdInternalFloatToS16Int(right.m_Blocks[1]));
    _mm_storeu_si128((__m128i*)destination, _mm_unpacklo_epi16(l, r));
    _mm_storeu_si128((__m128i*)((uint8_t*)destination + 16), _mm_unpackhi_epi16(l, r));
#else
    unsigned i;
    for (i = 0; i < 2; ++i)
    {
        SimdStoreInterleavedS16((uint8_t*)destination + i * 16, left.m_Blocks[i], right.m_Blocks[i]);
    }

#endif
}

static inline void Simd8Finish(void)
{
    SimdFinish();
}

#undef DM_SIMD_FIXED8_NATIVE

#if defined(DM_SIMD_USE_SME) && !DM_SIMD_FORCE_SCALAR
#include "simd_sme.h"
#else

#if !DM_SIMD_FORCE_SCALAR && defined(DM_SIMD_AVX512)
#define DM_SIMD_FIXED16_NATIVE
typedef __m512 dsimd_float16_t;
#else
typedef struct dsimd_float16_t
{
    dsimd_float4_t m_Blocks[4];
} dsimd_float16_t;
#endif

static inline dsimd_float16_t Simd16Combine(dsimd_float4_t a, dsimd_float4_t b, dsimd_float4_t c, dsimd_float4_t d)
{
#if defined(DM_SIMD_FIXED16_NATIVE)
    __m512 result = _mm512_castps128_ps512(a);
    result = _mm512_insertf32x4(result, b, 1);
    result = _mm512_insertf32x4(result, c, 2);
    return _mm512_insertf32x4(result, d, 3);
#else
    dsimd_float16_t result;
    result.m_Blocks[0] = a;
    result.m_Blocks[1] = b;
    result.m_Blocks[2] = c;
    result.m_Blocks[3] = d;
    return result;
#endif
}

static inline dsimd_float16_t Simd16Load(const void* source)
{
#if defined(DM_SIMD_FIXED16_NATIVE)
    return _mm512_loadu_ps((const float*)source);
#else
    dsimd_float16_t result;
    unsigned i;
    for (i = 0; i < 4; ++i)
    {
        result.m_Blocks[i] = SimdLoad((const uint8_t*)source + i * 16);
    }

    return result;
#endif
}

static inline void Simd16Store(void* destination, dsimd_float16_t value)
{
#if defined(DM_SIMD_FIXED16_NATIVE)
    _mm512_storeu_ps((float*)destination, value);
#else
    unsigned i;
    for (i = 0; i < 4; ++i)
    {
        SimdStore((uint8_t*)destination + i * 16, value.m_Blocks[i]);
    }

#endif
}

static inline dsimd_float16_t Simd16Splat(float value)
{
#if defined(DM_SIMD_FIXED16_NATIVE)
    return _mm512_set1_ps(value);
#else
    dsimd_float16_t result;
    unsigned i;
    for (i = 0; i < 4; ++i)
    {
        result.m_Blocks[i] = SimdSplat(value);
    }

    return result;
#endif
}

static inline dsimd_float16_t Simd16Add(dsimd_float16_t a, dsimd_float16_t b)
{
#if defined(DM_SIMD_FIXED16_NATIVE)
    return _mm512_add_ps(a, b);
#else
    dsimd_float16_t result;
    unsigned i;
    for (i = 0; i < 4; ++i)
    {
        result.m_Blocks[i] = SimdAdd(a.m_Blocks[i], b.m_Blocks[i]);
    }

    return result;
#endif
}

static inline dsimd_float16_t Simd16Mul(dsimd_float16_t a, dsimd_float16_t b)
{
#if defined(DM_SIMD_FIXED16_NATIVE)
    return _mm512_mul_ps(a, b);
#else
    dsimd_float16_t result;
    unsigned i;
    for (i = 0; i < 4; ++i)
    {
        result.m_Blocks[i] = SimdMul(a.m_Blocks[i], b.m_Blocks[i]);
    }

    return result;
#endif
}

static inline dsimd_float16_t Simd16Min(dsimd_float16_t a, dsimd_float16_t b)
{
#if defined(DM_SIMD_FIXED16_NATIVE)
    return _mm512_min_ps(a, b);
#else
    dsimd_float16_t result;
    unsigned i;
    for (i = 0; i < 4; ++i)
    {
        result.m_Blocks[i] = SimdMin(a.m_Blocks[i], b.m_Blocks[i]);
    }

    return result;
#endif
}

static inline dsimd_float16_t Simd16Max(dsimd_float16_t a, dsimd_float16_t b)
{
#if defined(DM_SIMD_FIXED16_NATIVE)
    return _mm512_max_ps(a, b);
#else
    dsimd_float16_t result;
    unsigned i;
    for (i = 0; i < 4; ++i)
    {
        result.m_Blocks[i] = SimdMax(a.m_Blocks[i], b.m_Blocks[i]);
    }

    return result;
#endif
}

static inline dsimd_float16_t Simd16Ramp(float start, float step)
{
    static const float indices[16] = {
        0.0f, 1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f, 8.0f, 9.0f, 10.0f, 11.0f, 12.0f, 13.0f, 14.0f, 15.0f
    };
    return Simd16Add(Simd16Splat(start), Simd16Mul(Simd16Splat(step), Simd16Load(indices)));
}

static inline float Simd16Lane(dsimd_float16_t value, unsigned index)
{
#if defined(DM_SIMD_FIXED16_NATIVE)
    // Scalar tails only need the low lane; keep its extraction in registers.
    if (index == 0)
    {
        return _mm_cvtss_f32(_mm512_castps512_ps128(value));
    }

    float lanes[16];
    _mm512_storeu_ps(lanes, value);
    return lanes[index];
#else
    return SimdLane(value.m_Blocks[index / 4], index & 3);
#endif
}

static inline float Simd16Sum(dsimd_float16_t value)
{
#if defined(DM_SIMD_FIXED16_NATIVE)
    return _mm512_reduce_add_ps(value);
#else
    dsimd_float4_t sum = value.m_Blocks[0];
    unsigned i;
    for (i = 1; i < 4; ++i)
    {
        sum = SimdAdd(sum, value.m_Blocks[i]);
    }

    return SimdSum(sum);
#endif
}

static inline float Simd16ReduceMax(dsimd_float16_t value)
{
#if defined(DM_SIMD_FIXED16_NATIVE)
    return _mm512_reduce_max_ps(value);
#else
    dsimd_float4_t maximum = value.m_Blocks[0];
    unsigned i;
    for (i = 1; i < 4; ++i)
    {
        maximum = SimdMax(maximum, value.m_Blocks[i]);
    }

    return SimdReduceMax(maximum);
#endif
}

static inline dsimd_float16_t Simd16LoadS8ToS16Float(const void* source)
{
#if defined(DM_SIMD_FIXED16_NATIVE)
    return _mm512_cvtepi32_ps(_mm512_cvtepi16_epi32(_mm256_mullo_epi16(_mm256_cvtepu8_epi16(_mm_loadu_si128((const __m128i*)source)), _mm256_set1_epi16(257))));
#elif defined(DM_SIMD_X86_H)
    __m128i bytes = _mm_loadu_si128((const __m128i*)source);
    __m128i low = _mm_unpacklo_epi8(bytes, bytes);
    __m128i high = _mm_unpackhi_epi8(bytes, bytes);
    dsimd_float16_t result;
    result.m_Blocks[0] = SimdInternalS16LowToFloat(low);
    result.m_Blocks[1] = SimdInternalS16HighToFloat(low);
    result.m_Blocks[2] = SimdInternalS16LowToFloat(high);
    result.m_Blocks[3] = SimdInternalS16HighToFloat(high);
    return result;
#elif !DM_SIMD_FORCE_SCALAR && defined(DM_SIMD_WASM)
    v128_t bytes = wasm_v128_load(source);
    v128_t low = wasm_i8x16_shuffle(bytes, bytes, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7);
    v128_t high = wasm_i8x16_shuffle(bytes, bytes, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13, 14, 14, 15, 15);
    dsimd_float16_t result;
    result.m_Blocks[0] = wasm_f32x4_convert_i32x4(wasm_i32x4_extend_low_i16x8(low));
    result.m_Blocks[1] = wasm_f32x4_convert_i32x4(wasm_i32x4_extend_high_i16x8(low));
    result.m_Blocks[2] = wasm_f32x4_convert_i32x4(wasm_i32x4_extend_low_i16x8(high));
    result.m_Blocks[3] = wasm_f32x4_convert_i32x4(wasm_i32x4_extend_high_i16x8(high));
    return result;
#else
    dsimd_float16_t result;
    unsigned i;
    for (i = 0; i < 4; ++i)
    {
        result.m_Blocks[i] = SimdLoadS8ToS16Float((const uint8_t*)source + i * 4);
    }

    return result;
#endif
}

static inline dsimd_float16_t Simd16LoadS16ToFloat(const void* source)
{
#if defined(DM_SIMD_FIXED16_NATIVE)
    return _mm512_cvtepi32_ps(_mm512_cvtepi16_epi32(_mm256_loadu_si256((const __m256i*)source)));
#else
    dsimd_float16_t result;
    unsigned i;
    for (i = 0; i < 4; ++i)
    {
        result.m_Blocks[i] = SimdLoadS16ToFloat((const uint8_t*)source + i * 8);
    }

    return result;
#endif
}

static inline void Simd16Unzip(dsimd_float16_t a, dsimd_float16_t b, dsimd_float16_t* left, dsimd_float16_t* right)
{
#if defined(DM_SIMD_FIXED16_NATIVE)
    *left = _mm512_permutex2var_ps(a, _mm512_setr_epi32(0, 2, 4, 6, 8, 10, 12, 14, 16, 18, 20, 22, 24, 26, 28, 30), b);
    *right = _mm512_permutex2var_ps(a, _mm512_setr_epi32(1, 3, 5, 7, 9, 11, 13, 15, 17, 19, 21, 23, 25, 27, 29, 31), b);
#else
    unsigned i;
    for (i = 0; i < 2; ++i)
    {
        SimdUnzip(a.m_Blocks[2 * i], a.m_Blocks[2 * i + 1], &left->m_Blocks[i], &right->m_Blocks[i]);
        SimdUnzip(b.m_Blocks[2 * i], b.m_Blocks[2 * i + 1], &left->m_Blocks[i + 2], &right->m_Blocks[i + 2]);
    }

#endif
}

static inline void Simd16StoreInterleavedS16(void* destination, dsimd_float16_t left, dsimd_float16_t right)
{
#if defined(DM_SIMD_FIXED16_NATIVE)
    dsimd_float16_t minimum = _mm512_set1_ps(-32768.0f), maximum = _mm512_set1_ps(32767.0f);
    __m512i l = _mm512_cvttps_epi32(_mm512_min_ps(_mm512_max_ps(left, minimum), maximum));
    __m512i r = _mm512_cvttps_epi32(_mm512_min_ps(_mm512_max_ps(right, minimum), maximum));
    _mm512_storeu_si512(destination, _mm512_packs_epi32(_mm512_unpacklo_epi32(l, r), _mm512_unpackhi_epi32(l, r)));
#else
    unsigned i;
    for (i = 0; i < 4; ++i)
    {
        SimdStoreInterleavedS16((uint8_t*)destination + i * 16, left.m_Blocks[i], right.m_Blocks[i]);
    }

#endif
}

static inline void Simd16Finish(void)
{
    SimdFinish();
}

#undef DM_SIMD_FIXED16_NATIVE

// Count is in [0,16]; inactive lanes are zero and do not access memory.
static inline dsimd_float16_t Simd16LoadPartial(const void* source, unsigned count)
{
    float values[16] = {
        0
    };
    if (count)
    {
        memcpy(values, source, count * sizeof(float));
    }

    return Simd16Load(values);
}

#endif // DM_SIMD_USE_SME

#ifdef __cplusplus
} // extern "C"
#endif
#endif // DM_SIMD_FIXED_FLOAT_H
