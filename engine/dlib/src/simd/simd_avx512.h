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

#ifndef DM_SIMD_AVX512_H
#define DM_SIMD_AVX512_H

static inline dsimd_float16_t Simd512Combine(dsimd_float4_t a, dsimd_float4_t b, dsimd_float4_t c, dsimd_float4_t d)
{
    __m512 result = _mm512_castps128_ps512(a);
    result = _mm512_insertf32x4(result, b, 1);
    result = _mm512_insertf32x4(result, c, 2);
    return _mm512_insertf32x4(result, d, 3);
}

static inline dsimd_float16_t Simd512Load(const void* source)
{
    return _mm512_loadu_ps((const float*)source);
}

static inline void Simd512Store(void* destination, dsimd_float16_t value)
{
    _mm512_storeu_ps((float*)destination, value);
}

static inline dsimd_float16_t Simd512Splat(float value)
{
    return _mm512_set1_ps(value);
}

static inline dsimd_float16_t Simd512Add(dsimd_float16_t a, dsimd_float16_t b)
{
    return _mm512_add_ps(a, b);
}

static inline dsimd_float16_t Simd512Mul(dsimd_float16_t a, dsimd_float16_t b)
{
    return _mm512_mul_ps(a, b);
}

static inline dsimd_float16_t Simd512Min(dsimd_float16_t a, dsimd_float16_t b)
{
    return _mm512_min_ps(a, b);
}

static inline dsimd_float16_t Simd512Max(dsimd_float16_t a, dsimd_float16_t b)
{
    return _mm512_max_ps(a, b);
}

static inline dsimd_float16_t Simd512Ramp(float start, float step)
{
    static const float indices[16] = {
        0.0f, 1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f, 8.0f, 9.0f, 10.0f, 11.0f, 12.0f, 13.0f, 14.0f, 15.0f
    };
    return Simd512Add(Simd512Splat(start), Simd512Mul(Simd512Splat(step), Simd512Load(indices)));
}

static inline float Simd512Lane(dsimd_float16_t value, unsigned index)
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

static inline float Simd512Sum(dsimd_float16_t value)
{
    return _mm512_reduce_add_ps(value);
}

static inline float Simd512ReduceMax(dsimd_float16_t value)
{
    return _mm512_reduce_max_ps(value);
}

static inline dsimd_float16_t Simd512LoadS8ToS16Float(const void* source)
{
    return _mm512_cvtepi32_ps(_mm512_cvtepi16_epi32(_mm256_mullo_epi16(_mm256_cvtepu8_epi16(_mm_loadu_si128((const __m128i*)source)), _mm256_set1_epi16(257))));
}

static inline dsimd_float16_t Simd512LoadS16ToFloat(const void* source)
{
    return _mm512_cvtepi32_ps(_mm512_cvtepi16_epi32(_mm256_loadu_si256((const __m256i*)source)));
}

static inline void Simd512Unzip(dsimd_float16_t a, dsimd_float16_t b, dsimd_float16_t* left, dsimd_float16_t* right)
{
    *left = _mm512_permutex2var_ps(a, _mm512_setr_epi32(0, 2, 4, 6, 8, 10, 12, 14, 16, 18, 20, 22, 24, 26, 28, 30), b);
    *right = _mm512_permutex2var_ps(a, _mm512_setr_epi32(1, 3, 5, 7, 9, 11, 13, 15, 17, 19, 21, 23, 25, 27, 29, 31), b);
}

static inline void Simd512StoreInterleavedS16(void* destination, dsimd_float16_t left, dsimd_float16_t right)
{
    dsimd_float16_t minimum = _mm512_set1_ps(-32768.0f), maximum = _mm512_set1_ps(32767.0f);
    __m512i l = _mm512_cvttps_epi32(_mm512_min_ps(_mm512_max_ps(left, minimum), maximum));
    __m512i r = _mm512_cvttps_epi32(_mm512_min_ps(_mm512_max_ps(right, minimum), maximum));
    _mm512_storeu_si512(destination, _mm512_packs_epi32(_mm512_unpacklo_epi32(l, r), _mm512_unpackhi_epi32(l, r)));
}

static inline void Simd512Finish(void)
{
    Simd128Finish();
}


// Count is in [0,16]; inactive lanes are zero and do not access memory.
static inline dsimd_float16_t Simd512LoadPartial(const void* source, unsigned count)
{
    float values[16] = {
        0
    };
    if (count)
    {
        memcpy(values, source, count * sizeof(float));
    }

    return Simd512Load(values);
}

static inline dsimd_uint16_t Simd512LoadUint(const void* source)
{
    return _mm512_loadu_si512((const void*)source);
}

static inline void Simd512StoreUint(void* destination, dsimd_uint16_t value)
{
    _mm512_storeu_si512((void*)destination, value);
}

static inline dsimd_uint16_t Simd512SplatUint(uint32_t value)
{
    return _mm512_set1_epi32((int32_t)value);
}

static inline dsimd_uint16_t Simd512AndUint(dsimd_uint16_t a, dsimd_uint16_t b)
{
    return _mm512_and_si512(a, b);
}

static inline dsimd_uint16_t Simd512OrUint(dsimd_uint16_t a, dsimd_uint16_t b)
{
    return _mm512_or_si512(a, b);
}

static inline dsimd_uint16_t Simd512XorUint(dsimd_uint16_t a, dsimd_uint16_t b)
{
    return _mm512_xor_si512(a, b);
}

static inline dsimd_uint16_t Simd512SubUint(dsimd_uint16_t a, dsimd_uint16_t b)
{
    return _mm512_sub_epi32(a, b);
}

static inline dsimd_uint16_t Simd512EqualUint(dsimd_uint16_t a, dsimd_uint16_t b)
{
    return _mm512_maskz_set1_epi32(_mm512_cmpeq_epi32_mask(a, b), -1);
}

static inline dsimd_uint16_t Simd512NotUint(dsimd_uint16_t value)
{
    return _mm512_xor_si512(value, _mm512_set1_epi32(-1));
}

static inline int Simd512All(dsimd_uint16_t value)
{
    return _mm512_cmp_epi32_mask(value, _mm512_setzero_si512(), _MM_CMPINT_LT) == 0xffff;
}

#endif // DM_SIMD_AVX512_H
