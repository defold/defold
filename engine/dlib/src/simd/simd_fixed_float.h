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
 * Simd8 is available only with AVX2; Simd16 only with AVX-512.
 * Other backends expose Simd4; callers own any loop unrolling.
 * Values stay within one kernel and must not cross an ISA dispatch
 * boundary or appear in exported layouts.
 * Load/Store access exactly 32/64 unaligned bytes. Splat repeats one value;
 * Add/Mul/Min/Max operate component-wise. Ramp(start,step) produces start+i*step.
 * Combine concatenates two/four 128-bit blocks in argument order.
 * Lane indexes must be in [0,8) or [0,16). Sum/ReduceMax reduce all lanes.
 * Simd8Sum4(a,b,c,d) returns four sums in argument order, matching Simd8Sum.
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
typedef __m256 dsimd_float8_t;

static inline dsimd_float8_t Simd8Combine(dsimd_float4_t a, dsimd_float4_t b)
{
    return _mm256_insertf128_ps(_mm256_castps128_ps256(a), b, 1);
}

static inline dsimd_float8_t Simd8Load(const void* source)
{
    return _mm256_loadu_ps((const float*)source);
}

static inline void Simd8Store(void* destination, dsimd_float8_t value)
{
    _mm256_storeu_ps((float*)destination, value);
}

static inline dsimd_float8_t Simd8Splat(float value)
{
    return _mm256_set1_ps(value);
}

static inline dsimd_float8_t Simd8Add(dsimd_float8_t a, dsimd_float8_t b)
{
    return _mm256_add_ps(a, b);
}

static inline dsimd_float8_t Simd8Mul(dsimd_float8_t a, dsimd_float8_t b)
{
    return _mm256_mul_ps(a, b);
}

static inline dsimd_float8_t Simd8Min(dsimd_float8_t a, dsimd_float8_t b)
{
    return _mm256_min_ps(a, b);
}

static inline dsimd_float8_t Simd8Max(dsimd_float8_t a, dsimd_float8_t b)
{
    return _mm256_max_ps(a, b);
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
    // Scalar tails only need the low lane; keep its extraction in registers.
    if (index == 0)
    {
        return _mm_cvtss_f32(_mm256_castps256_ps128(value));
    }

    float lanes[8];
    _mm256_storeu_ps(lanes, value);
    return lanes[index];
}

static inline float Simd8Sum(dsimd_float8_t value)
{
    return SimdSum(_mm_add_ps(_mm256_castps256_ps128(value), _mm256_extractf128_ps(value, 1)));
}

// Reduce four eight-lane inputs to four consecutive sums, preserving Simd8Sum's addition order.
static inline dsimd_float4_t Simd8Sum4(dsimd_float8_t a, dsimd_float8_t b, dsimd_float8_t c, dsimd_float8_t d)
{
    __m128 sa = _mm_add_ps(_mm256_castps256_ps128(a), _mm256_extractf128_ps(a, 1));
    __m128 sb = _mm_add_ps(_mm256_castps256_ps128(b), _mm256_extractf128_ps(b, 1));
    __m128 sc = _mm_add_ps(_mm256_castps256_ps128(c), _mm256_extractf128_ps(c, 1));
    __m128 sd = _mm_add_ps(_mm256_castps256_ps128(d), _mm256_extractf128_ps(d, 1));
    return _mm_hadd_ps(_mm_hadd_ps(sa, sb), _mm_hadd_ps(sc, sd));
}

static inline float Simd8ReduceMax(dsimd_float8_t value)
{
    return SimdReduceMax(_mm_max_ps(_mm256_castps256_ps128(value), _mm256_extractf128_ps(value, 1)));
}

static inline dsimd_float8_t Simd8LoadS8ToS16Float(const void* source)
{
    return _mm256_cvtepi32_ps(_mm256_cvtepi16_epi32(_mm_unpacklo_epi8(_mm_loadl_epi64((const __m128i*)source), _mm_loadl_epi64((const __m128i*)source))));
}

static inline dsimd_float8_t Simd8LoadS16ToFloat(const void* source)
{
    return _mm256_cvtepi32_ps(_mm256_cvtepi16_epi32(_mm_loadu_si128((const __m128i*)source)));
}

static inline void Simd8Unzip(dsimd_float8_t a, dsimd_float8_t b, dsimd_float8_t* left, dsimd_float8_t* right)
{
    __m256i order = _mm256_setr_epi32(0, 1, 4, 5, 2, 3, 6, 7);
    *left = _mm256_permutevar8x32_ps(_mm256_shuffle_ps(a, b, _MM_SHUFFLE(2, 0, 2, 0)), order);
    *right = _mm256_permutevar8x32_ps(_mm256_shuffle_ps(a, b, _MM_SHUFFLE(3, 1, 3, 1)), order);
}

static inline void Simd8StoreInterleavedS16(void* destination, dsimd_float8_t left, dsimd_float8_t right)
{
    dsimd_float8_t minimum = _mm256_set1_ps(-32768.0f), maximum = _mm256_set1_ps(32767.0f);
    __m256i l = _mm256_cvttps_epi32(_mm256_min_ps(_mm256_max_ps(left, minimum), maximum));
    __m256i r = _mm256_cvttps_epi32(_mm256_min_ps(_mm256_max_ps(right, minimum), maximum));
    _mm256_storeu_si256((__m256i*)destination, _mm256_packs_epi32(_mm256_unpacklo_epi32(l, r), _mm256_unpackhi_epi32(l, r)));
}

static inline void Simd8Finish(void)
{
    SimdFinish();
}

#endif

#if !DM_SIMD_FORCE_SCALAR && defined(DM_SIMD_AVX512)
typedef __m512 dsimd_float16_t;

static inline dsimd_float16_t Simd16Combine(dsimd_float4_t a, dsimd_float4_t b, dsimd_float4_t c, dsimd_float4_t d)
{
    __m512 result = _mm512_castps128_ps512(a);
    result = _mm512_insertf32x4(result, b, 1);
    result = _mm512_insertf32x4(result, c, 2);
    return _mm512_insertf32x4(result, d, 3);
}

static inline dsimd_float16_t Simd16Load(const void* source)
{
    return _mm512_loadu_ps((const float*)source);
}

static inline void Simd16Store(void* destination, dsimd_float16_t value)
{
    _mm512_storeu_ps((float*)destination, value);
}

static inline dsimd_float16_t Simd16Splat(float value)
{
    return _mm512_set1_ps(value);
}

static inline dsimd_float16_t Simd16Add(dsimd_float16_t a, dsimd_float16_t b)
{
    return _mm512_add_ps(a, b);
}

static inline dsimd_float16_t Simd16Mul(dsimd_float16_t a, dsimd_float16_t b)
{
    return _mm512_mul_ps(a, b);
}

static inline dsimd_float16_t Simd16Min(dsimd_float16_t a, dsimd_float16_t b)
{
    return _mm512_min_ps(a, b);
}

static inline dsimd_float16_t Simd16Max(dsimd_float16_t a, dsimd_float16_t b)
{
    return _mm512_max_ps(a, b);
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
    // Scalar tails only need the low lane; keep its extraction in registers.
    if (index == 0)
    {
        return _mm_cvtss_f32(_mm512_castps512_ps128(value));
    }

    float lanes[16];
    _mm512_storeu_ps(lanes, value);
    return lanes[index];
}

static inline float Simd16Sum(dsimd_float16_t value)
{
    return _mm512_reduce_add_ps(value);
}

static inline float Simd16ReduceMax(dsimd_float16_t value)
{
    return _mm512_reduce_max_ps(value);
}

static inline dsimd_float16_t Simd16LoadS8ToS16Float(const void* source)
{
    return _mm512_cvtepi32_ps(_mm512_cvtepi16_epi32(_mm256_mullo_epi16(_mm256_cvtepu8_epi16(_mm_loadu_si128((const __m128i*)source)), _mm256_set1_epi16(257))));
}

static inline dsimd_float16_t Simd16LoadS16ToFloat(const void* source)
{
    return _mm512_cvtepi32_ps(_mm512_cvtepi16_epi32(_mm256_loadu_si256((const __m256i*)source)));
}

static inline void Simd16Unzip(dsimd_float16_t a, dsimd_float16_t b, dsimd_float16_t* left, dsimd_float16_t* right)
{
    *left = _mm512_permutex2var_ps(a, _mm512_setr_epi32(0, 2, 4, 6, 8, 10, 12, 14, 16, 18, 20, 22, 24, 26, 28, 30), b);
    *right = _mm512_permutex2var_ps(a, _mm512_setr_epi32(1, 3, 5, 7, 9, 11, 13, 15, 17, 19, 21, 23, 25, 27, 29, 31), b);
}

static inline void Simd16StoreInterleavedS16(void* destination, dsimd_float16_t left, dsimd_float16_t right)
{
    dsimd_float16_t minimum = _mm512_set1_ps(-32768.0f), maximum = _mm512_set1_ps(32767.0f);
    __m512i l = _mm512_cvttps_epi32(_mm512_min_ps(_mm512_max_ps(left, minimum), maximum));
    __m512i r = _mm512_cvttps_epi32(_mm512_min_ps(_mm512_max_ps(right, minimum), maximum));
    _mm512_storeu_si512(destination, _mm512_packs_epi32(_mm512_unpacklo_epi32(l, r), _mm512_unpackhi_epi32(l, r)));
}

static inline void Simd16Finish(void)
{
    SimdFinish();
}


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

#endif // DM_SIMD_AVX512

#ifdef __cplusplus
} // extern "C"
#endif
#endif // DM_SIMD_FIXED_FLOAT_H
