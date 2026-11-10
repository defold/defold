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
    memcpy(destination, &value, sizeof(value));
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
    return SimdSelect(SimdLess(a, b), a, b);
}
static inline dsimd_float4_t SimdMax(dsimd_float4_t a, dsimd_float4_t b)
{
    return SimdSelect(SimdLess(b, a), a, b);
}
static inline int SimdAny(dsimd_uint4_t mask)
{
    return SimdMaskBits(mask) != 0;
}
static inline int SimdAll(dsimd_uint4_t mask)
{
    return SimdMaskBits(mask) == 15;
}
static inline float SimdSum(dsimd_float4_t value)
{
    dsimd_float4_t pairs = SimdAdd(value, SimdShuffle(value, 1, 0, 3, 2));
    return SimdLane(SimdAdd(pairs, SimdShuffle(pairs, 2, 3, 0, 1)), 0);
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

#endif // DM_SIMD_GENERIC_H
