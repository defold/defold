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

#ifndef DM_SIMD_AVX2_H
#define DM_SIMD_AVX2_H

static inline dsimd_float8_t Simd256Combine(dsimd_float4_t a, dsimd_float4_t b)
{
    return _mm256_insertf128_ps(_mm256_castps128_ps256(a), b, 1);
}

static inline dsimd_float8_t Simd256Load(const void* source)
{
    return _mm256_loadu_ps((const float*)source);
}

static inline void Simd256Store(void* destination, dsimd_float8_t value)
{
    _mm256_storeu_ps((float*)destination, value);
}

static inline dsimd_float8_t Simd256Splat(float value)
{
    return _mm256_set1_ps(value);
}

static inline dsimd_float8_t Simd256Add(dsimd_float8_t a, dsimd_float8_t b)
{
    return _mm256_add_ps(a, b);
}

static inline dsimd_float8_t Simd256Mul(dsimd_float8_t a, dsimd_float8_t b)
{
    return _mm256_mul_ps(a, b);
}

static inline dsimd_float8_t Simd256Min(dsimd_float8_t a, dsimd_float8_t b)
{
    return _mm256_min_ps(a, b);
}

static inline dsimd_float8_t Simd256Max(dsimd_float8_t a, dsimd_float8_t b)
{
    return _mm256_max_ps(a, b);
}

static inline dsimd_float8_t Simd256Ramp(float start, float step)
{
    static const float indices[8] = {
        0.0f, 1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f
    };
    return Simd256Add(Simd256Splat(start), Simd256Mul(Simd256Splat(step), Simd256Load(indices)));
}

static inline float Simd256Lane(dsimd_float8_t value, unsigned index)
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

static inline float Simd256Sum(dsimd_float8_t value)
{
    return Simd128Sum(_mm_add_ps(_mm256_castps256_ps128(value), _mm256_extractf128_ps(value, 1)));
}

// Reduce four eight-lane inputs to four consecutive sums, preserving Simd256Sum's addition order.
static inline dsimd_float4_t Simd256Sum4(dsimd_float8_t a, dsimd_float8_t b, dsimd_float8_t c, dsimd_float8_t d)
{
    __m128 sa = _mm_add_ps(_mm256_castps256_ps128(a), _mm256_extractf128_ps(a, 1));
    __m128 sb = _mm_add_ps(_mm256_castps256_ps128(b), _mm256_extractf128_ps(b, 1));
    __m128 sc = _mm_add_ps(_mm256_castps256_ps128(c), _mm256_extractf128_ps(c, 1));
    __m128 sd = _mm_add_ps(_mm256_castps256_ps128(d), _mm256_extractf128_ps(d, 1));
    return _mm_hadd_ps(_mm_hadd_ps(sa, sb), _mm_hadd_ps(sc, sd));
}

static inline float Simd256ReduceMax(dsimd_float8_t value)
{
    return Simd128ReduceMax(_mm_max_ps(_mm256_castps256_ps128(value), _mm256_extractf128_ps(value, 1)));
}

static inline dsimd_float8_t Simd256LoadS8ToS16Float(const void* source)
{
    return _mm256_cvtepi32_ps(_mm256_cvtepi16_epi32(_mm_unpacklo_epi8(_mm_loadl_epi64((const __m128i*)source), _mm_loadl_epi64((const __m128i*)source))));
}

static inline dsimd_float8_t Simd256LoadS16ToFloat(const void* source)
{
    return _mm256_cvtepi32_ps(_mm256_cvtepi16_epi32(_mm_loadu_si128((const __m128i*)source)));
}

static inline void Simd256Unzip(dsimd_float8_t a, dsimd_float8_t b, dsimd_float8_t* left, dsimd_float8_t* right)
{
    __m256i order = _mm256_setr_epi32(0, 1, 4, 5, 2, 3, 6, 7);
    *left = _mm256_permutevar8x32_ps(_mm256_shuffle_ps(a, b, _MM_SHUFFLE(2, 0, 2, 0)), order);
    *right = _mm256_permutevar8x32_ps(_mm256_shuffle_ps(a, b, _MM_SHUFFLE(3, 1, 3, 1)), order);
}

static inline void Simd256StoreInterleavedS16(void* destination, dsimd_float8_t left, dsimd_float8_t right)
{
    dsimd_float8_t minimum = _mm256_set1_ps(-32768.0f), maximum = _mm256_set1_ps(32767.0f);
    __m256i l = _mm256_cvttps_epi32(_mm256_min_ps(_mm256_max_ps(left, minimum), maximum));
    __m256i r = _mm256_cvttps_epi32(_mm256_min_ps(_mm256_max_ps(right, minimum), maximum));
    _mm256_storeu_si256((__m256i*)destination, _mm256_packs_epi32(_mm256_unpacklo_epi32(l, r), _mm256_unpackhi_epi32(l, r)));
}

static inline void Simd256Finish(void)
{
    Simd128Finish();
}

static inline dsimd_uint8_t Simd256LoadUint(const void* source)
{
    return _mm256_loadu_si256((const __m256i*)source);
}

static inline void Simd256StoreUint(void* destination, dsimd_uint8_t value)
{
    _mm256_storeu_si256((__m256i*)destination, value);
}

static inline dsimd_uint8_t Simd256SplatUint(uint32_t value)
{
    return _mm256_set1_epi32((int32_t)value);
}

static inline dsimd_uint8_t Simd256AndUint(dsimd_uint8_t a, dsimd_uint8_t b)
{
    return _mm256_and_si256(a, b);
}

static inline dsimd_uint8_t Simd256OrUint(dsimd_uint8_t a, dsimd_uint8_t b)
{
    return _mm256_or_si256(a, b);
}

static inline dsimd_uint8_t Simd256XorUint(dsimd_uint8_t a, dsimd_uint8_t b)
{
    return _mm256_xor_si256(a, b);
}

static inline dsimd_uint8_t Simd256SubUint(dsimd_uint8_t a, dsimd_uint8_t b)
{
    return _mm256_sub_epi32(a, b);
}

static inline dsimd_uint8_t Simd256EqualUint(dsimd_uint8_t a, dsimd_uint8_t b)
{
    return _mm256_cmpeq_epi32(a, b);
}

static inline dsimd_uint8_t Simd256NotUint(dsimd_uint8_t value)
{
    return _mm256_xor_si256(value, _mm256_set1_epi32(-1));
}

static inline int Simd256All(dsimd_uint8_t value)
{
    return _mm256_movemask_ps(_mm256_castsi256_ps(value)) == 0xff;
}

#endif // DM_SIMD_AVX2_H
