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

#ifndef DM_SIMD_GENERIC_H
#define DM_SIMD_GENERIC_H

static inline dsimd_float4_t SimdLoad(const void* source)
{
    dsimd_float4_t result;
    memcpy(&result, source, sizeof(result));
    return result;
}
static inline void SimdStore(void* destination, dsimd_float4_t value)
{
#if defined(DM_SIMD_NEON_H)
    // Avoid an addressable temporary in Android's fortified memcpy wrapper.
    vst1q_u8((uint8_t*)destination, vreinterpretq_u8_f32(value));
#else
    memcpy(destination, &value, sizeof(value));
#endif
}
static inline dsimd_uint4_t SimdLoadUint(const void* source)
{
    dsimd_uint4_t result;
    memcpy(&result, source, sizeof(result));
    return result;
}
static inline void SimdStoreUint(void* destination, dsimd_uint4_t value)
{
    memcpy(destination, &value, sizeof(value));
}
static inline dsimd_int4_t SimdLoadInt(const void* source)
{
    dsimd_int4_t result;
    memcpy(&result, source, sizeof(result));
    return result;
}
static inline void SimdStoreInt(void* destination, dsimd_int4_t value)
{
    memcpy(destination, &value, sizeof(value));
}
static inline dsimd_ushort8_t SimdLoadU16(const void* source)
{
    dsimd_ushort8_t result;
    memcpy(&result, source, sizeof(result));
    return result;
}
static inline void SimdStoreU16(void* destination, dsimd_ushort8_t value)
{
    memcpy(destination, &value, sizeof(value));
}
static inline dsimd_byte16_t SimdLoadBytes(const void* source)
{
    dsimd_byte16_t result;
    memcpy(&result, source, sizeof(result));
    return result;
}
static inline void SimdStoreBytes(void* destination, dsimd_byte16_t value)
{
    memcpy(destination, &value, sizeof(value));
}
static inline dsimd_float4_t SimdSplat(float value)
{
    return SimdMake(value, value, value, value);
}
#if !defined(DM_SIMD_WASM_H)
static inline dsimd_float4_t SimdConvertS8ToS16Float(dsimd_byte16_t bytes, unsigned quartet)
{
    uint8_t values[16];
    SimdStoreBytes(values, bytes);
    return SimdLoadS8ToS16Float(values + quartet * 4);
}
#endif

static inline dsimd_float4_t SimdRamp(float start, float step)
{
    return SimdAdd(SimdSplat(start), SimdMul(SimdSplat(step), SimdMake(0, 1, 2, 3)));
}
static inline dsimd_uint4_t SimdMakeUint(uint32_t x, uint32_t y, uint32_t z, uint32_t w)
{
    uint32_t lanes[4] = { x, y, z, w };
    return SimdLoadUint(lanes);
}
static inline dsimd_uint4_t SimdSplatUint(uint32_t value)
{
#if defined(DM_SIMD_NEON_H)
    return vdupq_n_u32(value);
#elif defined(DM_SIMD_X86_H)
    return _mm_set1_epi32((int32_t)value);
#elif defined(DM_SIMD_WASM_H)
    return wasm_i32x4_splat((int32_t)value);
#else
    dsimd_uint4_t result = { { value, value, value, value } };
    return result;
#endif
}
static inline dsimd_ushort8_t SimdSplatU16(uint16_t value)
{
#if defined(DM_SIMD_NEON_H)
    return vdupq_n_u16(value);
#elif defined(DM_SIMD_X86_H)
    return _mm_set1_epi16((int16_t)value);
#elif defined(DM_SIMD_WASM_H)
    return wasm_i16x8_splat((int16_t)value);
#else
    dsimd_ushort8_t result = { { value, value, value, value, value, value, value, value } };
    return result;
#endif
}
static inline dsimd_byte16_t SimdSplatBytes(uint8_t value)
{
    uint8_t  lanes[16];
    unsigned i;
    for (i = 0; i < 16; ++i)
        lanes[i] = value;
    return SimdLoadBytes(lanes);
}
static inline dsimd_byte16_t SimdLoadBytesPartial(const void* source, unsigned count)
{
    uint8_t bytes[16] = { 0 };
    if (count)
        memcpy(bytes, source, count);
    return SimdLoadBytes(bytes);
}
static inline void SimdStoreBytesPartial(void* destination, dsimd_byte16_t value, unsigned count)
{
    uint8_t bytes[16];
    SimdStoreBytes(bytes, value);
    if (count)
        memcpy(destination, bytes, count);
}
static inline dsimd_uint4_t SimdFloatBits(dsimd_float4_t value)
{
    dsimd_uint4_t result;
    memcpy(&result, &value, sizeof(result));
    return result;
}
static inline dsimd_float4_t SimdUintBits(dsimd_uint4_t value)
{
    dsimd_float4_t result;
    memcpy(&result, &value, sizeof(result));
    return result;
}
static inline dsimd_uint4_t SimdBytesToUint(dsimd_byte16_t value)
{
    dsimd_uint4_t result;
    memcpy(&result, &value, sizeof(result));
    return result;
}
static inline dsimd_byte16_t SimdUintToBytes(dsimd_uint4_t value)
{
    dsimd_byte16_t result;
    memcpy(&result, &value, sizeof(result));
    return result;
}
static inline dsimd_uint4_t SimdNotUint(dsimd_uint4_t value)
{
    return SimdXorUint(value, SimdSplatUint(UINT32_MAX));
}
static inline dsimd_uint4_t SimdMinUint(dsimd_uint4_t a, dsimd_uint4_t b)
{
    return SimdSelectUint(SimdLessUint(a, b), a, b);
}
static inline dsimd_uint4_t SimdMaxUint(dsimd_uint4_t a, dsimd_uint4_t b)
{
    return SimdSelectUint(SimdLessUint(a, b), b, a);
}
static inline dsimd_float4_t SimdSelect(dsimd_uint4_t mask, dsimd_float4_t yes, dsimd_float4_t no)
{
    return SimdUintBits(SimdSelectUint(mask, SimdFloatBits(yes), SimdFloatBits(no)));
}
static inline dsimd_float4_t SimdMin(dsimd_float4_t a, dsimd_float4_t b)
{
#if defined(DM_SIMD_X86_H)
    return _mm_min_ps(a, b);
#else
    return SimdSelect(SimdLess(a, b), a, b);
#endif
}
static inline dsimd_float4_t SimdMax(dsimd_float4_t a, dsimd_float4_t b)
{
#if defined(DM_SIMD_X86_H)
    return _mm_max_ps(a, b);
#else
    return SimdSelect(SimdLess(b, a), a, b);
#endif
}
static inline int SimdAny(dsimd_uint4_t mask)
{
#if defined(DM_SIMD_NEON_H) && (defined(__aarch64__) || defined(_M_ARM64))
    // Only the sign bits determine truth. Unsigned max preserves whether any
    // lane has that bit set, without constructing the four-bit lane mask.
    return (int)(vmaxvq_u32(mask) >> 31);
#else
    return SimdMaskBits(mask) != 0;
#endif
}
static inline int SimdAll(dsimd_uint4_t mask)
{
    return SimdMaskBits(mask) == 15;
}
static inline void SimdUnzip(dsimd_float4_t a, dsimd_float4_t b, dsimd_float4_t* left, dsimd_float4_t* right)
{
#if defined(DM_SIMD_X86_H)
    *left = _mm_shuffle_ps(a, b, _MM_SHUFFLE(2, 0, 2, 0));
    *right = _mm_shuffle_ps(a, b, _MM_SHUFFLE(3, 1, 3, 1));
#else
    *left = SimdShuffle2(a, b, 0, 2, 4, 6);
    *right = SimdShuffle2(a, b, 1, 3, 5, 7);
#endif
}
static inline dsimd_float4_t SimdPairwiseAdd(dsimd_float4_t a, dsimd_float4_t b)
{
#if defined(DM_SIMD_NEON_H)
#if defined(__aarch64__) || defined(_M_ARM64)
    return vpaddq_f32(a, b);
#else
    return vcombine_f32(vpadd_f32(vget_low_f32(a), vget_high_f32(a)), vpadd_f32(vget_low_f32(b), vget_high_f32(b)));
#endif
#elif defined(DM_SIMD_X86_H)
    return _mm_hadd_ps(a, b);
#else
    return SimdAdd(SimdShuffle2(a, b, 0, 2, 4, 6), SimdShuffle2(a, b, 1, 3, 5, 7));
#endif
}
static inline float SimdSum(dsimd_float4_t value)
{
#if defined(DM_SIMD_NEON_H)
    float32x2_t pairs = vpadd_f32(vget_low_f32(value), vget_high_f32(value));
    return vget_lane_f32(vpadd_f32(pairs, pairs), 0);
#elif defined(DM_SIMD_X86_H) && !defined(DM_SIMD_SSE41)
    // Keep reductions in registers on SSE3-only targets.
    dsimd_float4_t pairs = _mm_hadd_ps(value, value);
    return _mm_cvtss_f32(_mm_hadd_ps(pairs, pairs));
#else
    dsimd_float4_t pairs = SimdAdd(value, SimdShuffle(value, 1, 0, 3, 2));
    return SimdLane(SimdAdd(pairs, SimdShuffle(pairs, 2, 3, 0, 1)), 0);
#endif
}
static inline float SimdReduceMin(dsimd_float4_t value)
{
    dsimd_float4_t pairs = SimdMin(value, SimdShuffle(value, 1, 0, 3, 2));
    return SimdLane(SimdMin(pairs, SimdShuffle(pairs, 2, 3, 0, 1)), 0);
}
static inline float SimdReduceMax(dsimd_float4_t value)
{
    dsimd_float4_t pairs = SimdMax(value, SimdShuffle(value, 1, 0, 3, 2));
    return SimdLane(SimdMax(pairs, SimdShuffle(pairs, 2, 3, 0, 1)), 0);
}

static inline void SimdFinish(void)
{
#if defined(DM_SIMD_AVX)
    _mm256_zeroupper();
#endif
}

#endif // DM_SIMD_GENERIC_H
