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

#ifndef DM_SIMD_FIXED_UINT_H
#define DM_SIMD_FIXED_UINT_H

#include <simd/simd.h>

/*# Native-width unsigned integer SIMD operations.
 * Simd256 uses AVX2; Simd512 uses AVX-512. Other backends expose Simd128.
 * Loads/stores are unaligned and access exactly 32 or 64 bytes.
 * Arithmetic wraps modulo 2^32; equality returns all-one or zero lane masks.
 * All examines the sign bit of every 32-bit lane, matching Simd128All.
 * Register values remain inside inlined code and never cross the dispatch ABI.
 * @document
 * @name SIMD Fixed Unsigned Integer
 * @language C
 */

#if !DM_SIMD_FORCE_SCALAR && defined(DM_SIMD_AVX2)
typedef __m256i dsimd_uint8_t;

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

#endif

#if !DM_SIMD_FORCE_SCALAR && defined(DM_SIMD_AVX512)
typedef __m512i dsimd_uint16_t;

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

#endif

#endif // DM_SIMD_FIXED_UINT_H
