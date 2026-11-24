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

#ifndef DM_SIMD_NEON_H
#define DM_SIMD_NEON_H
#include <math.h>
#include <string.h>

// ARMv7 uses scalar division and sqrt to retain full-precision semantics.
static inline dsimd_float4_t SimdMake(float x, float y, float z, float w)
{
    float a[4] = { x, y, z, w };
    return vld1q_f32(a);
}
static inline float SimdLane(dsimd_float4_t v, int i)
{
#if defined(__clang__) || defined(__GNUC__)
    // Vector indexing avoids an addressable temporary for runtime lanes.
    return v[i];
#else
    float a[4];
    vst1q_f32(a, v);
    return a[i];
#endif
}
static inline dsimd_float4_t SimdAdd(dsimd_float4_t a, dsimd_float4_t b)
{
    return vaddq_f32(a, b);
}
static inline dsimd_float4_t SimdSub(dsimd_float4_t a, dsimd_float4_t b)
{
    return vsubq_f32(a, b);
}
static inline dsimd_float4_t SimdMul(dsimd_float4_t a, dsimd_float4_t b)
{
    return vmulq_f32(a, b);
}
static inline dsimd_float4_t SimdMulAdd(dsimd_float4_t a, dsimd_float4_t b, dsimd_float4_t c)
{
#if defined(__aarch64__) || defined(_M_ARM64)
    return vfmaq_f32(c, a, b);
#else
    return vmlaq_f32(c, a, b);
#endif
}
static inline dsimd_float4_t SimdDiv(dsimd_float4_t a, dsimd_float4_t b)
{
#if defined(__aarch64__) || defined(_M_ARM64)
    return vdivq_f32(a, b);
#else
    return SimdMake(SimdLane(a, 0) / SimdLane(b, 0), SimdLane(a, 1) / SimdLane(b, 1), SimdLane(a, 2) / SimdLane(b, 2), SimdLane(a, 3) / SimdLane(b, 3));
#endif
}
static inline dsimd_float4_t SimdSqrt(dsimd_float4_t a)
{
#if defined(__aarch64__) || defined(_M_ARM64)
    return vsqrtq_f32(a);
#else
    return SimdMake(sqrtf(SimdLane(a, 0)), sqrtf(SimdLane(a, 1)), sqrtf(SimdLane(a, 2)), sqrtf(SimdLane(a, 3)));
#endif
}
static inline dsimd_float4_t SimdNeg(dsimd_float4_t a)
{
    return vnegq_f32(a);
}
static inline dsimd_float4_t SimdAbs(dsimd_float4_t a)
{
    return vabsq_f32(a);
}
static inline dsimd_float4_t SimdXor(dsimd_float4_t a, dsimd_float4_t b)
{
    return vreinterpretq_f32_u32(veorq_u32(vreinterpretq_u32_f32(a), vreinterpretq_u32_f32(b)));
}
static inline float SimdDot3(dsimd_float4_t a, dsimd_float4_t b)
{
    dsimd_float4_t p = SimdMul(a, b);
    float32x2_t    sum = vpadd_f32(vget_low_f32(p), vget_low_f32(p));
    return vget_lane_f32(vadd_f32(sum, vget_high_f32(p)), 0);
}
static inline float SimdDot4(dsimd_float4_t a, dsimd_float4_t b)
{
    dsimd_float4_t p = SimdMul(a, b);
    float32x2_t    sum = vpadd_f32(vget_low_f32(p), vget_high_f32(p));
    return vget_lane_f32(vpadd_f32(sum, sum), 0);
}
static inline dsimd_float4_t SimdShuffle(dsimd_float4_t a, int x, int y, int z, int w)
{
    uint32_t   indices[4] = { 0x03020100u + x * 0x04040404u, 0x03020100u + y * 0x04040404u, 0x03020100u + z * 0x04040404u, 0x03020100u + w * 0x04040404u };
    uint8x16_t bytes = vreinterpretq_u8_f32(a), lanes = vreinterpretq_u8_u32(vld1q_u32(indices));
#if defined(__aarch64__) || defined(_M_ARM64)
    return vreinterpretq_f32_u8(vqtbl1q_u8(bytes, lanes));
#else
    uint8x8x2_t table;
    table.val[0] = vget_low_u8(bytes);
    table.val[1] = vget_high_u8(bytes);
    return vreinterpretq_f32_u8(vcombine_u8(vtbl2_u8(table, vget_low_u8(lanes)), vtbl2_u8(table, vget_high_u8(lanes))));
#endif
}
static inline void SimdTranspose(dsimd_float4_t* a, dsimd_float4_t* b, dsimd_float4_t* c, dsimd_float4_t* d)
{
    float32x4x2_t ab = vtrnq_f32(*a, *b), cd = vtrnq_f32(*c, *d);
    *a = vcombine_f32(vget_low_f32(ab.val[0]), vget_low_f32(cd.val[0]));
    *b = vcombine_f32(vget_low_f32(ab.val[1]), vget_low_f32(cd.val[1]));
    *c = vcombine_f32(vget_high_f32(ab.val[0]), vget_high_f32(cd.val[0]));
    *d = vcombine_f32(vget_high_f32(ab.val[1]), vget_high_f32(cd.val[1]));
}

static inline void SimdLoadInterleavedU16x2(const void* source, dsimd_ushort8_t* a, dsimd_ushort8_t* b)
{
    uint16x8x2_t pair = vld2q_u16((const uint16_t*)source);
    *a = pair.val[0];
    *b = pair.val[1];
}
static inline void SimdLoadInterleavedBytes4(const void* source, dsimd_byte16_t* a, dsimd_byte16_t* b, dsimd_byte16_t* c, dsimd_byte16_t* d)
{
    uint8x16x4_t channels = vld4q_u8((const uint8_t*)source);
    *a = channels.val[0];
    *b = channels.val[1];
    *c = channels.val[2];
    *d = channels.val[3];
}
static inline void SimdStoreInterleavedBytes4(void* destination, dsimd_byte16_t a, dsimd_byte16_t b, dsimd_byte16_t c, dsimd_byte16_t d)
{
    uint8x16x4_t channels;
    channels.val[0] = a;
    channels.val[1] = b;
    channels.val[2] = c;
    channels.val[3] = d;
    vst4q_u8((uint8_t*)destination, channels);
}

static inline dsimd_uint4_t SimdAddUint(dsimd_uint4_t a, dsimd_uint4_t b)
{
    return vaddq_u32(a, b);
}
static inline dsimd_uint4_t SimdSubUint(dsimd_uint4_t a, dsimd_uint4_t b)
{
    return vsubq_u32(a, b);
}
static inline dsimd_uint4_t SimdMulUint(dsimd_uint4_t a, dsimd_uint4_t b)
{
    return vmulq_u32(a, b);
}
static inline dsimd_uint4_t SimdAndUint(dsimd_uint4_t a, dsimd_uint4_t b)
{
    return vandq_u32(a, b);
}
static inline dsimd_uint4_t SimdOrUint(dsimd_uint4_t a, dsimd_uint4_t b)
{
    return vorrq_u32(a, b);
}
static inline dsimd_uint4_t SimdXorUint(dsimd_uint4_t a, dsimd_uint4_t b)
{
    return veorq_u32(a, b);
}
static inline dsimd_uint4_t SimdEqualUint(dsimd_uint4_t a, dsimd_uint4_t b)
{
    return vceqq_u32(a, b);
}
static inline dsimd_uint4_t SimdLessUint(dsimd_uint4_t a, dsimd_uint4_t b)
{
    return vcltq_u32(a, b);
}
static inline dsimd_uint4_t SimdSelectUint(dsimd_uint4_t mask, dsimd_uint4_t yes, dsimd_uint4_t no)
{
    return vbslq_u32(mask, yes, no);
}
static inline dsimd_uint4_t SimdShiftLeftUint(dsimd_uint4_t value, unsigned count)
{
    if (count >= 32)
        return vdupq_n_u32(0);
    return vshlq_u32(value, vdupq_n_s32((int32_t)count));
}
static inline dsimd_uint4_t SimdShiftRightUint(dsimd_uint4_t value, unsigned count)
{
    if (count >= 32)
        return vdupq_n_u32(0);
    return vshlq_u32(value, vdupq_n_s32(-(int32_t)count));
}
static inline dsimd_uint4_t SimdShiftRightUintEach(dsimd_uint4_t value, dsimd_uint4_t counts)
{
    uint32x4_t valid = vcltq_u32(counts, vdupq_n_u32(32));
    int32x4_t  shifts = vnegq_s32(vreinterpretq_s32_u32(vandq_u32(counts, vdupq_n_u32(31))));
    return vandq_u32(vshlq_u32(value, shifts), valid);
}
static inline dsimd_uint4_t SimdEqual(dsimd_float4_t a, dsimd_float4_t b)
{
    return vceqq_f32(a, b);
}
static inline dsimd_uint4_t SimdLess(dsimd_float4_t a, dsimd_float4_t b)
{
    return vcltq_f32(a, b);
}
static inline unsigned SimdMaskBits(dsimd_uint4_t mask)
{
    static const int32_t shifts[4] = { 0, 1, 2, 3 };
    uint32x4_t           bits = vshlq_u32(vshrq_n_u32(mask, 31), vld1q_s32(shifts));
    uint32x2_t           pairs = vadd_u32(vget_low_u32(bits), vget_high_u32(bits));
    return vget_lane_u32(vpadd_u32(pairs, pairs), 0);
}
static inline dsimd_int4_t SimdTruncateToInt(dsimd_float4_t value)
{
    return vcvtq_s32_f32(value);
}
static inline dsimd_float4_t SimdConvertIntToFloat(dsimd_int4_t value)
{
    return vcvtq_f32_s32(value);
}
static inline dsimd_ushort8_t SimdAddU16(dsimd_ushort8_t a, dsimd_ushort8_t b)
{
    return vaddq_u16(a, b);
}
static inline dsimd_ushort8_t SimdSubU16(dsimd_ushort8_t a, dsimd_ushort8_t b)
{
    return vsubq_u16(a, b);
}
static inline dsimd_ushort8_t SimdAndU16(dsimd_ushort8_t a, dsimd_ushort8_t b)
{
    return vandq_u16(a, b);
}
static inline dsimd_ushort8_t SimdOrU16(dsimd_ushort8_t a, dsimd_ushort8_t b)
{
    return vorrq_u16(a, b);
}
static inline dsimd_ushort8_t SimdMulU16(dsimd_ushort8_t a, dsimd_ushort8_t b)
{
    return vmulq_u16(a, b);
}
static inline dsimd_ushort8_t SimdMulHighU16(dsimd_ushort8_t a, dsimd_ushort8_t b)
{
    uint32x4_t low = vmull_u16(vget_low_u16(a), vget_low_u16(b));
    uint32x4_t high = vmull_u16(vget_high_u16(a), vget_high_u16(b));
    return vcombine_u16(vshrn_n_u32(low, 16), vshrn_n_u32(high, 16));
}
static inline dsimd_ushort8_t SimdShiftLeftU16(dsimd_ushort8_t value, unsigned count)
{
    if (count >= 16)
        return vdupq_n_u16(0);
    return vshlq_u16(value, vdupq_n_s16((int16_t)count));
}
static inline dsimd_ushort8_t SimdShiftRightU16(dsimd_ushort8_t value, unsigned count)
{
    if (count >= 16)
        return vdupq_n_u16(0);
    return vshlq_u16(value, vdupq_n_s16(-(int16_t)count));
}
// The ARMv7 two-table lookup is adapted from SIMDe (MIT).
/*
 * Copyright (c) 2017 Evan Nemerson <evan@nemerson.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
 * LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
 * OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
 * WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */
static inline dsimd_byte16_t SimdShuffleBytes(dsimd_byte16_t value, dsimd_byte16_t indices)
{
#if defined(__aarch64__) || defined(_M_ARM64)
    return vqtbl1q_u8(value, indices);
#else
    uint8x8x2_t table;
    table.val[0] = vget_low_u8(value);
    table.val[1] = vget_high_u8(value);
    return vcombine_u8(vtbl2_u8(table, vget_low_u8(indices)), vtbl2_u8(table, vget_high_u8(indices)));
#endif
}
static inline dsimd_byte16_t SimdZipBytesLow(dsimd_byte16_t a, dsimd_byte16_t b)
{
    uint8x8x2_t pair = vzip_u8(vget_low_u8(a), vget_low_u8(b));
    return vcombine_u8(pair.val[0], pair.val[1]);
}
static inline dsimd_ushort8_t SimdWidenBytesLow(dsimd_byte16_t value)
{
    return vmovl_u8(vget_low_u8(value));
}
static inline dsimd_byte16_t SimdZipBytesHigh(dsimd_byte16_t a, dsimd_byte16_t b)
{
    uint8x8x2_t pair = vzip_u8(vget_high_u8(a), vget_high_u8(b));
    return vcombine_u8(pair.val[0], pair.val[1]);
}
static inline dsimd_ushort8_t SimdWidenBytesHigh(dsimd_byte16_t value)
{
    return vmovl_u8(vget_high_u8(value));
}
static inline dsimd_uint4_t SimdWidenU16Low(dsimd_ushort8_t value)
{
    return vmovl_u16(vget_low_u16(value));
}
static inline dsimd_uint4_t SimdWidenU16High(dsimd_ushort8_t value)
{
    return vmovl_u16(vget_high_u16(value));
}
static inline dsimd_byte16_t SimdPackBytes(dsimd_ushort8_t low, dsimd_ushort8_t high)
{
    return vcombine_u8(vqmovn_u16(low), vqmovn_u16(high));
}
static inline dsimd_ushort8_t SimdPackU16(dsimd_uint4_t low, dsimd_uint4_t high)
{
    return vcombine_u16(vqmovn_u32(low), vqmovn_u32(high));
}

static inline dsimd_float4_t SimdConvertUintToFloat(dsimd_uint4_t value)
{
    return vcvtq_f32_u32(value);
}

static inline dsimd_ushort8_t SimdNarrowU16(dsimd_uint4_t low, dsimd_uint4_t high)
{
    return vcombine_u16(vmovn_u32(low), vmovn_u32(high));
}
static inline dsimd_byte16_t SimdPackSignedBytes(dsimd_ushort8_t low, dsimd_ushort8_t high)
{
    return vcombine_u8(vqmovun_s16(vreinterpretq_s16_u16(low)), vqmovun_s16(vreinterpretq_s16_u16(high)));
}

#endif // DM_SIMD_NEON_H
