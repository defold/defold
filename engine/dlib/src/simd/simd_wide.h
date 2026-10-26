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

#ifndef DM_SIMD_WIDE_H
#define DM_SIMD_WIDE_H

#include "simd.h"

/*# Wide C SIMD primitives for bulk memory and image processing.
 * Registers hold DM_SIMD_WIDE_BYTES bytes: 32 by default, or 64 when the compiler
 * enables AVX-512F/BW. Other targets use consecutive 128-bit registers; forcing
 * scalar keeps the same width with scalar primitives. These temporary values
 * must not be used in exported ABI layouts or shared across differing settings.
 * Loads/stores accept unaligned buffers and access exactly that many bytes.
 * Float registers contain DM_SIMD_WIDE_BYTES/4 lanes. Splat repeats one value;
 * Add/Mul operate component-wise. BytesToUint/UintToBytes preserve every bit;
 * OrUint combines the bits of corresponding unsigned lanes.
 * AddU16/MulU16 wrap modulo 65536; ShiftRightU16 returns zero for counts >= 16.
 * ShuffleBytes selects within each 16-byte block; indices >= 16 produce zero.
 * WidenBytesLow/High widen each block's first/last eight bytes. PackBytes reverses
 * this arrangement, saturating unsigned words to 255. Float arithmetic follows
 * native compiler rounding/contraction rules; no approximations are introduced.
 * @document
 * @name SIMD Wide
 * @language C
 */
#ifndef DM_SIMD_WIDE_BYTES
#if defined(DM_SIMD_AVX512)
#define DM_SIMD_WIDE_BYTES 64
#else
#define DM_SIMD_WIDE_BYTES 32
#endif
#endif
#if DM_SIMD_WIDE_BYTES != 32 && DM_SIMD_WIDE_BYTES != 64
#error DM_SIMD_WIDE_BYTES must be 32 or 64
#endif

#ifdef __cplusplus
extern "C"
{
#endif

#if !DM_SIMD_FORCE_SCALAR && DM_SIMD_WIDE_BYTES == 64 && defined(DM_SIMD_AVX512)
#define DM_SIMD_WIDE_NATIVE
#define DM_WIDE_OP(name) _mm512_##name
#define DM_WIDE_LOAD _mm512_loadu_si512
#define DM_WIDE_STORE _mm512_storeu_si512
#define DM_WIDE_ZERO _mm512_setzero_si512
#define DM_WIDE_OR _mm512_or_si512
#define DM_WIDE_SHUFFLE(value, indices) _mm512_maskz_shuffle_epi8(_mm512_cmplt_epu8_mask(indices, _mm512_set1_epi8(16)), value, indices)
    typedef __m512  dsimd_wide_float_t;
    typedef __m512i dsimd_wide_uint_t;
    typedef __m512i dsimd_wide_ushort_t;
    typedef __m512i dsimd_wide_byte_t;
#elif !DM_SIMD_FORCE_SCALAR && DM_SIMD_WIDE_BYTES == 32 && defined(DM_SIMD_AVX2)
#define DM_SIMD_WIDE_NATIVE
#define DM_WIDE_OP(name) _mm256_##name
#define DM_WIDE_LOAD _mm256_loadu_si256
#define DM_WIDE_STORE _mm256_storeu_si256
#define DM_WIDE_ZERO _mm256_setzero_si256
#define DM_WIDE_OR _mm256_or_si256
#define DM_WIDE_SHUFFLE(value, indices) _mm256_shuffle_epi8(value, _mm256_or_si256(indices, _mm256_cmpgt_epi8(indices, _mm256_set1_epi8(15))))
typedef __m256  dsimd_wide_float_t;
typedef __m256i dsimd_wide_uint_t;
typedef __m256i dsimd_wide_ushort_t;
typedef __m256i dsimd_wide_byte_t;
#else
typedef struct
{
    dsimd_float4_t m_Lanes[DM_SIMD_WIDE_BYTES / 16];
} dsimd_wide_float_t;
typedef struct
{
    dsimd_uint4_t m_Lanes[DM_SIMD_WIDE_BYTES / 16];
} dsimd_wide_uint_t;
typedef struct
{
    dsimd_ushort8_t m_Lanes[DM_SIMD_WIDE_BYTES / 16];
} dsimd_wide_ushort_t;
typedef struct
{
    dsimd_byte16_t m_Lanes[DM_SIMD_WIDE_BYTES / 16];
} dsimd_wide_byte_t;
#endif

    static inline dsimd_wide_float_t  SimdWideLoad(const void* source);
    static inline void                SimdWideStore(void* destination, dsimd_wide_float_t value);
    static inline dsimd_wide_float_t  SimdWideSplat(float value);
    static inline dsimd_wide_float_t  SimdWideAdd(dsimd_wide_float_t a, dsimd_wide_float_t b);
    static inline dsimd_wide_float_t  SimdWideMul(dsimd_wide_float_t a, dsimd_wide_float_t b);
    static inline dsimd_wide_byte_t   SimdWideLoadBytes(const void* source);
    static inline void                SimdWideStoreBytes(void* destination, dsimd_wide_byte_t value);
    static inline dsimd_wide_uint_t   SimdWideBytesToUint(dsimd_wide_byte_t value);
    static inline dsimd_wide_byte_t   SimdWideUintToBytes(dsimd_wide_uint_t value);
    static inline dsimd_wide_uint_t   SimdWideSplatUint(uint32_t value);
    static inline dsimd_wide_uint_t   SimdWideOrUint(dsimd_wide_uint_t a, dsimd_wide_uint_t b);
    static inline dsimd_wide_ushort_t SimdWideSplatU16(uint16_t value);
    static inline dsimd_wide_ushort_t SimdWideAddU16(dsimd_wide_ushort_t a, dsimd_wide_ushort_t b);
    static inline dsimd_wide_ushort_t SimdWideMulU16(dsimd_wide_ushort_t a, dsimd_wide_ushort_t b);
    static inline dsimd_wide_ushort_t SimdWideShiftRightU16(dsimd_wide_ushort_t value, unsigned count);
    static inline dsimd_wide_ushort_t SimdWideWidenBytesLow(dsimd_wide_byte_t value);
    static inline dsimd_wide_ushort_t SimdWideWidenBytesHigh(dsimd_wide_byte_t value);
    static inline dsimd_wide_byte_t   SimdWidePackBytes(dsimd_wide_ushort_t low, dsimd_wide_ushort_t high);
    static inline dsimd_wide_byte_t   SimdWideShuffleBytes(dsimd_wide_byte_t value, dsimd_wide_byte_t indices);

    static inline dsimd_wide_float_t  SimdWideLoad(const void* source)
    {
#if defined(DM_SIMD_WIDE_NATIVE)
        dsimd_wide_float_t r;
        memcpy(&r, source, sizeof(r));
        return r;
#else
    dsimd_wide_float_t r;
    unsigned           i;
    for (i = 0; i < DM_SIMD_WIDE_BYTES / 16; ++i)
        r.m_Lanes[i] = SimdLoad((const uint8_t*)source + i * 16);
    return r;
#endif
    }
    static inline void SimdWideStore(void* destination, dsimd_wide_float_t value)
    {
#if defined(DM_SIMD_WIDE_NATIVE)
        memcpy(destination, &value, sizeof(value));
#else
    unsigned i;
    for (i = 0; i < DM_SIMD_WIDE_BYTES / 16; ++i)
        SimdStore((uint8_t*)destination + i * 16, value.m_Lanes[i]);
#endif
    }
    static inline dsimd_wide_float_t SimdWideSplat(float value)
    {
#if defined(DM_SIMD_WIDE_NATIVE)
        return DM_WIDE_OP(set1_ps)(value);
#else
    dsimd_wide_float_t r;
    unsigned           i;
    for (i = 0; i < DM_SIMD_WIDE_BYTES / 16; ++i)
        r.m_Lanes[i] = SimdSplat(value);
    return r;
#endif
    }
    static inline dsimd_wide_float_t SimdWideAdd(dsimd_wide_float_t a, dsimd_wide_float_t b)
    {
#if defined(DM_SIMD_WIDE_NATIVE)
        return DM_WIDE_OP(add_ps)(a, b);
#else
    dsimd_wide_float_t r;
    unsigned           i;
    for (i = 0; i < DM_SIMD_WIDE_BYTES / 16; ++i)
        r.m_Lanes[i] = SimdAdd(a.m_Lanes[i], b.m_Lanes[i]);
    return r;
#endif
    }
    static inline dsimd_wide_float_t SimdWideMul(dsimd_wide_float_t a, dsimd_wide_float_t b)
    {
#if defined(DM_SIMD_WIDE_NATIVE)
        return DM_WIDE_OP(mul_ps)(a, b);
#else
    dsimd_wide_float_t r;
    unsigned           i;
    for (i = 0; i < DM_SIMD_WIDE_BYTES / 16; ++i)
        r.m_Lanes[i] = SimdMul(a.m_Lanes[i], b.m_Lanes[i]);
    return r;
#endif
    }
    static inline dsimd_wide_byte_t SimdWideLoadBytes(const void* source)
    {
#if defined(DM_SIMD_WIDE_NATIVE)
        return DM_WIDE_LOAD((const dsimd_wide_byte_t*)source);
#else
    dsimd_wide_byte_t r;
    unsigned          i;
    for (i = 0; i < DM_SIMD_WIDE_BYTES / 16; ++i)
        r.m_Lanes[i] = SimdLoadBytes((const uint8_t*)source + i * 16);
    return r;
#endif
    }
    static inline void SimdWideStoreBytes(void* destination, dsimd_wide_byte_t value)
    {
#if defined(DM_SIMD_WIDE_NATIVE)
        DM_WIDE_STORE((dsimd_wide_byte_t*)destination, value);
#else
    unsigned i;
    for (i = 0; i < DM_SIMD_WIDE_BYTES / 16; ++i)
        SimdStoreBytes((uint8_t*)destination + i * 16, value.m_Lanes[i]);
#endif
    }
    static inline dsimd_wide_uint_t SimdWideBytesToUint(dsimd_wide_byte_t value)
    {
#if defined(DM_SIMD_WIDE_NATIVE)
        return value;
#else
    dsimd_wide_uint_t r;
    unsigned          i;
    for (i = 0; i < DM_SIMD_WIDE_BYTES / 16; ++i)
        r.m_Lanes[i] = SimdBytesToUint(value.m_Lanes[i]);
    return r;
#endif
    }
    static inline dsimd_wide_byte_t SimdWideUintToBytes(dsimd_wide_uint_t value)
    {
#if defined(DM_SIMD_WIDE_NATIVE)
        return value;
#else
    dsimd_wide_byte_t r;
    unsigned          i;
    for (i = 0; i < DM_SIMD_WIDE_BYTES / 16; ++i)
        r.m_Lanes[i] = SimdUintToBytes(value.m_Lanes[i]);
    return r;
#endif
    }
    static inline dsimd_wide_uint_t SimdWideSplatUint(uint32_t value)
    {
#if defined(DM_SIMD_WIDE_NATIVE)
        return DM_WIDE_OP(set1_epi32)((int32_t)value);
#else
    dsimd_wide_uint_t r;
    unsigned          i;
    for (i = 0; i < DM_SIMD_WIDE_BYTES / 16; ++i)
        r.m_Lanes[i] = SimdSplatUint(value);
    return r;
#endif
    }
    static inline dsimd_wide_uint_t SimdWideOrUint(dsimd_wide_uint_t a, dsimd_wide_uint_t b)
    {
#if defined(DM_SIMD_WIDE_NATIVE)
        return DM_WIDE_OR(a, b);
#else
    dsimd_wide_uint_t r;
    unsigned          i;
    for (i = 0; i < DM_SIMD_WIDE_BYTES / 16; ++i)
        r.m_Lanes[i] = SimdOrUint(a.m_Lanes[i], b.m_Lanes[i]);
    return r;
#endif
    }
    static inline dsimd_wide_ushort_t SimdWideSplatU16(uint16_t value)
    {
#if defined(DM_SIMD_WIDE_NATIVE)
        return DM_WIDE_OP(set1_epi16)((int16_t)value);
#else
    dsimd_wide_ushort_t r;
    unsigned            i;
    for (i = 0; i < DM_SIMD_WIDE_BYTES / 16; ++i)
        r.m_Lanes[i] = SimdSplatU16(value);
    return r;
#endif
    }
    static inline dsimd_wide_ushort_t SimdWideAddU16(dsimd_wide_ushort_t a, dsimd_wide_ushort_t b)
    {
#if defined(DM_SIMD_WIDE_NATIVE)
        return DM_WIDE_OP(add_epi16)(a, b);
#else
    dsimd_wide_ushort_t r;
    unsigned            i;
    for (i = 0; i < DM_SIMD_WIDE_BYTES / 16; ++i)
        r.m_Lanes[i] = SimdAddU16(a.m_Lanes[i], b.m_Lanes[i]);
    return r;
#endif
    }
    static inline dsimd_wide_ushort_t SimdWideMulU16(dsimd_wide_ushort_t a, dsimd_wide_ushort_t b)
    {
#if defined(DM_SIMD_WIDE_NATIVE)
        return DM_WIDE_OP(mullo_epi16)(a, b);
#else
    dsimd_wide_ushort_t r;
    unsigned            i;
    for (i = 0; i < DM_SIMD_WIDE_BYTES / 16; ++i)
        r.m_Lanes[i] = SimdMulU16(a.m_Lanes[i], b.m_Lanes[i]);
    return r;
#endif
    }
    static inline dsimd_wide_ushort_t SimdWideShiftRightU16(dsimd_wide_ushort_t value, unsigned count)
    {
#if defined(DM_SIMD_WIDE_NATIVE)
        return DM_WIDE_OP(srl_epi16)(value, _mm_cvtsi32_si128((int)(count < 16 ? count : 16)));
#else
    dsimd_wide_ushort_t r;
    unsigned            i;
    for (i = 0; i < DM_SIMD_WIDE_BYTES / 16; ++i)
        r.m_Lanes[i] = SimdShiftRightU16(value.m_Lanes[i], count);
    return r;
#endif
    }
    static inline dsimd_wide_ushort_t SimdWideWidenBytesLow(dsimd_wide_byte_t value)
    {
#if defined(DM_SIMD_WIDE_NATIVE)
        return DM_WIDE_OP(unpacklo_epi8)(value, DM_WIDE_ZERO());
#else
    dsimd_wide_ushort_t r;
    unsigned            i;
    for (i = 0; i < DM_SIMD_WIDE_BYTES / 16; ++i)
        r.m_Lanes[i] = SimdWidenBytesLow(value.m_Lanes[i]);
    return r;
#endif
    }
    static inline dsimd_wide_ushort_t SimdWideWidenBytesHigh(dsimd_wide_byte_t value)
    {
#if defined(DM_SIMD_WIDE_NATIVE)
        return DM_WIDE_OP(unpackhi_epi8)(value, DM_WIDE_ZERO());
#else
    dsimd_wide_ushort_t r;
    unsigned            i;
    for (i = 0; i < DM_SIMD_WIDE_BYTES / 16; ++i)
        r.m_Lanes[i] = SimdWidenBytesHigh(value.m_Lanes[i]);
    return r;
#endif
    }
    static inline dsimd_wide_byte_t SimdWidePackBytes(dsimd_wide_ushort_t low, dsimd_wide_ushort_t high)
    {
#if defined(DM_SIMD_WIDE_NATIVE)
        return DM_WIDE_OP(packus_epi16)(DM_WIDE_OP(min_epu16)(low, DM_WIDE_OP(set1_epi16)(255)), DM_WIDE_OP(min_epu16)(high, DM_WIDE_OP(set1_epi16)(255)));
#else
    dsimd_wide_byte_t r;
    unsigned          i;
    for (i = 0; i < DM_SIMD_WIDE_BYTES / 16; ++i)
        r.m_Lanes[i] = SimdPackBytes(low.m_Lanes[i], high.m_Lanes[i]);
    return r;
#endif
    }
    static inline dsimd_wide_byte_t SimdWideShuffleBytes(dsimd_wide_byte_t value, dsimd_wide_byte_t indices)
    {
#if defined(DM_SIMD_WIDE_NATIVE)
        return DM_WIDE_SHUFFLE(value, indices);
#else
    dsimd_wide_byte_t r;
    unsigned          i;
    for (i = 0; i < DM_SIMD_WIDE_BYTES / 16; ++i)
        r.m_Lanes[i] = SimdShuffleBytes(value.m_Lanes[i], indices.m_Lanes[i]);
    return r;
#endif
    }

#undef DM_WIDE_OP
#undef DM_WIDE_LOAD
#undef DM_WIDE_STORE
#undef DM_WIDE_ZERO
#undef DM_WIDE_OR
#undef DM_WIDE_SHUFFLE
#ifdef __cplusplus
} // extern "C"
#endif
#endif // DM_SIMD_WIDE_H
