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

#ifndef DM_SIMD_WASM_H
#define DM_SIMD_WASM_H

static inline dsimd_float4_t Simd128Make(float x, float y, float z, float w)
{
    return wasm_f32x4_make(x, y, z, w);
}
static inline float Simd128Lane(dsimd_float4_t v, int i)
{
    float a[4];
    wasm_v128_store(a, v);
    return a[i];
}
static inline dsimd_float4_t Simd128Add(dsimd_float4_t a, dsimd_float4_t b)
{
    return wasm_f32x4_add(a, b);
}
static inline dsimd_float4_t Simd128Sub(dsimd_float4_t a, dsimd_float4_t b)
{
    return wasm_f32x4_sub(a, b);
}
static inline dsimd_float4_t Simd128Mul(dsimd_float4_t a, dsimd_float4_t b)
{
    return wasm_f32x4_mul(a, b);
}
static inline dsimd_float4_t Simd128MulAdd(dsimd_float4_t a, dsimd_float4_t b, dsimd_float4_t c)
{
    return wasm_f32x4_add(wasm_f32x4_mul(a, b), c);
}
static inline dsimd_float4_t Simd128Div(dsimd_float4_t a, dsimd_float4_t b)
{
    return wasm_f32x4_div(a, b);
}
static inline dsimd_float4_t Simd128Sqrt(dsimd_float4_t a)
{
    return wasm_f32x4_sqrt(a);
}
static inline dsimd_float4_t Simd128Neg(dsimd_float4_t a)
{
    return wasm_f32x4_neg(a);
}
static inline dsimd_float4_t Simd128Abs(dsimd_float4_t a)
{
    return wasm_f32x4_abs(a);
}
static inline dsimd_float4_t Simd128Xor(dsimd_float4_t a, dsimd_float4_t b)
{
    return wasm_v128_xor(a, b);
}
static inline float Simd128Dot3(dsimd_float4_t a, dsimd_float4_t b)
{
    dsimd_float4_t p = Simd128Mul(a, b);
    dsimd_float4_t sum = wasm_f32x4_add(p, wasm_i32x4_shuffle(p, p, 1, 1, 1, 1));
    return wasm_f32x4_extract_lane(wasm_f32x4_add(sum, wasm_i32x4_shuffle(p, p, 2, 2, 2, 2)), 0);
}
static inline float Simd128Dot4(dsimd_float4_t a, dsimd_float4_t b)
{
    dsimd_float4_t p = Simd128Mul(a, b);
    dsimd_float4_t sum = wasm_f32x4_add(p, wasm_i32x4_shuffle(p, p, 1, 1, 1, 1));
    sum = wasm_f32x4_add(sum, wasm_i32x4_shuffle(p, p, 2, 2, 2, 2));
    return wasm_f32x4_extract_lane(wasm_f32x4_add(sum, wasm_i32x4_shuffle(p, p, 3, 3, 3, 3)), 0);
}
static inline dsimd_float4_t Simd128Shuffle(dsimd_float4_t a, int x, int y, int z, int w)
{
    v128_t indices = wasm_i32x4_make(0x03020100 + x * 0x04040404, 0x03020100 + y * 0x04040404, 0x03020100 + z * 0x04040404, 0x03020100 + w * 0x04040404);
    return wasm_i8x16_swizzle(a, indices);
}
static inline void Simd128Transpose(dsimd_float4_t* a, dsimd_float4_t* b, dsimd_float4_t* c, dsimd_float4_t* d)
{
    dsimd_float4_t ab0 = wasm_i32x4_shuffle(*a, *b, 0, 4, 1, 5), ab1 = wasm_i32x4_shuffle(*a, *b, 2, 6, 3, 7);
    dsimd_float4_t cd0 = wasm_i32x4_shuffle(*c, *d, 0, 4, 1, 5), cd1 = wasm_i32x4_shuffle(*c, *d, 2, 6, 3, 7);
    *a = wasm_i32x4_shuffle(ab0, cd0, 0, 1, 4, 5);
    *b = wasm_i32x4_shuffle(ab0, cd0, 2, 3, 6, 7);
    *c = wasm_i32x4_shuffle(ab1, cd1, 0, 1, 4, 5);
    *d = wasm_i32x4_shuffle(ab1, cd1, 2, 3, 6, 7);
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
    v128_t         p0 = Simd128LoadBytes(bytes), p1 = Simd128LoadBytes(bytes + 16);
    v128_t         p2 = Simd128LoadBytes(bytes + 32), p3 = Simd128LoadBytes(bytes + 48);
    v128_t         ab0 = wasm_i8x16_shuffle(p0, p1, 0, 4, 8, 12, 16, 20, 24, 28, 1, 5, 9, 13, 17, 21, 25, 29);
    v128_t         cd0 = wasm_i8x16_shuffle(p0, p1, 2, 6, 10, 14, 18, 22, 26, 30, 3, 7, 11, 15, 19, 23, 27, 31);
    v128_t         ab1 = wasm_i8x16_shuffle(p2, p3, 0, 4, 8, 12, 16, 20, 24, 28, 1, 5, 9, 13, 17, 21, 25, 29);
    v128_t         cd1 = wasm_i8x16_shuffle(p2, p3, 2, 6, 10, 14, 18, 22, 26, 30, 3, 7, 11, 15, 19, 23, 27, 31);
    *a = wasm_i8x16_shuffle(ab0, ab1, 0, 1, 2, 3, 4, 5, 6, 7, 16, 17, 18, 19, 20, 21, 22, 23);
    *b = wasm_i8x16_shuffle(ab0, ab1, 8, 9, 10, 11, 12, 13, 14, 15, 24, 25, 26, 27, 28, 29, 30, 31);
    *c = wasm_i8x16_shuffle(cd0, cd1, 0, 1, 2, 3, 4, 5, 6, 7, 16, 17, 18, 19, 20, 21, 22, 23);
    *d = wasm_i8x16_shuffle(cd0, cd1, 8, 9, 10, 11, 12, 13, 14, 15, 24, 25, 26, 27, 28, 29, 30, 31);
}
static inline void Simd128StoreInterleavedBytes4(void* destination, dsimd_byte16_t a, dsimd_byte16_t b, dsimd_byte16_t c, dsimd_byte16_t d)
{
    uint8_t* bytes = (uint8_t*)destination;
    v128_t   ab0 = wasm_i8x16_shuffle(a, b, 0, 16, 1, 17, 2, 18, 3, 19, 4, 20, 5, 21, 6, 22, 7, 23);
    v128_t   cd0 = wasm_i8x16_shuffle(c, d, 0, 16, 1, 17, 2, 18, 3, 19, 4, 20, 5, 21, 6, 22, 7, 23);
    v128_t   ab1 = wasm_i8x16_shuffle(a, b, 8, 24, 9, 25, 10, 26, 11, 27, 12, 28, 13, 29, 14, 30, 15, 31);
    v128_t   cd1 = wasm_i8x16_shuffle(c, d, 8, 24, 9, 25, 10, 26, 11, 27, 12, 28, 13, 29, 14, 30, 15, 31);
    Simd128StoreBytes(bytes, wasm_i8x16_shuffle(ab0, cd0, 0, 1, 16, 17, 2, 3, 18, 19, 4, 5, 20, 21, 6, 7, 22, 23));
    Simd128StoreBytes(bytes + 16, wasm_i8x16_shuffle(ab0, cd0, 8, 9, 24, 25, 10, 11, 26, 27, 12, 13, 28, 29, 14, 15, 30, 31));
    Simd128StoreBytes(bytes + 32, wasm_i8x16_shuffle(ab1, cd1, 0, 1, 16, 17, 2, 3, 18, 19, 4, 5, 20, 21, 6, 7, 22, 23));
    Simd128StoreBytes(bytes + 48, wasm_i8x16_shuffle(ab1, cd1, 8, 9, 24, 25, 10, 11, 26, 27, 12, 13, 28, 29, 14, 15, 30, 31));
}

static inline dsimd_uint4_t Simd128AddUint(dsimd_uint4_t a, dsimd_uint4_t b)
{
    return wasm_i32x4_add(a, b);
}
static inline dsimd_uint4_t Simd128SubUint(dsimd_uint4_t a, dsimd_uint4_t b)
{
    return wasm_i32x4_sub(a, b);
}
static inline dsimd_uint4_t Simd128MulUint(dsimd_uint4_t a, dsimd_uint4_t b)
{
    return wasm_i32x4_mul(a, b);
}
static inline dsimd_uint4_t Simd128AndUint(dsimd_uint4_t a, dsimd_uint4_t b)
{
    return wasm_v128_and(a, b);
}
static inline dsimd_uint4_t Simd128OrUint(dsimd_uint4_t a, dsimd_uint4_t b)
{
    return wasm_v128_or(a, b);
}
static inline dsimd_uint4_t Simd128XorUint(dsimd_uint4_t a, dsimd_uint4_t b)
{
    return wasm_v128_xor(a, b);
}
static inline dsimd_uint4_t Simd128EqualUint(dsimd_uint4_t a, dsimd_uint4_t b)
{
    return wasm_i32x4_eq(a, b);
}
static inline dsimd_uint4_t Simd128LessUint(dsimd_uint4_t a, dsimd_uint4_t b)
{
    return wasm_u32x4_lt(a, b);
}
static inline dsimd_uint4_t Simd128SelectUint(dsimd_uint4_t mask, dsimd_uint4_t yes, dsimd_uint4_t no)
{
    return wasm_v128_bitselect(yes, no, mask);
}
static inline dsimd_uint4_t Simd128ShiftLeftUint(dsimd_uint4_t value, unsigned count)
{
    if (count >= 32)
        return wasm_i32x4_splat(0);
    return wasm_i32x4_shl(value, count);
}
static inline dsimd_uint4_t Simd128ShiftRightUint(dsimd_uint4_t value, unsigned count)
{
    if (count >= 32)
        return wasm_i32x4_splat(0);
    return wasm_u32x4_shr(value, count);
}
static inline dsimd_uint4_t Simd128ShiftRightUintEach(dsimd_uint4_t value, dsimd_uint4_t counts)
{
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
    return wasm_f32x4_eq(a, b);
}
static inline dsimd_uint4_t Simd128Less(dsimd_float4_t a, dsimd_float4_t b)
{
    return wasm_f32x4_lt(a, b);
}
static inline unsigned Simd128MaskBits(dsimd_uint4_t mask)
{
    return (unsigned)wasm_i32x4_bitmask(mask);
}
static inline dsimd_int4_t Simd128TruncateToInt(dsimd_float4_t value)
{
    return wasm_i32x4_trunc_sat_f32x4(value);
}
static inline dsimd_float4_t Simd128ConvertIntToFloat(dsimd_int4_t value)
{
    return wasm_f32x4_convert_i32x4(value);
}
static inline dsimd_ushort8_t Simd128AddU16(dsimd_ushort8_t a, dsimd_ushort8_t b)
{
    return wasm_i16x8_add(a, b);
}
static inline dsimd_ushort8_t Simd128SubU16(dsimd_ushort8_t a, dsimd_ushort8_t b)
{
    return wasm_i16x8_sub(a, b);
}
static inline dsimd_ushort8_t Simd128AndU16(dsimd_ushort8_t a, dsimd_ushort8_t b)
{
    return wasm_v128_and(a, b);
}
static inline dsimd_ushort8_t Simd128OrU16(dsimd_ushort8_t a, dsimd_ushort8_t b)
{
    return wasm_v128_or(a, b);
}
static inline dsimd_ushort8_t Simd128MulU16(dsimd_ushort8_t a, dsimd_ushort8_t b)
{
    return wasm_i16x8_mul(a, b);
}
static inline dsimd_ushort8_t Simd128MulHighU16(dsimd_ushort8_t a, dsimd_ushort8_t b)
{
    v128_t low = wasm_u32x4_extmul_low_u16x8(a, b);
    v128_t high = wasm_u32x4_extmul_high_u16x8(a, b);
    low = wasm_u32x4_shr(low, 16);
    high = wasm_u32x4_shr(high, 16);
    return wasm_i16x8_shuffle(low, high, 0, 2, 4, 6, 8, 10, 12, 14);
}
static inline dsimd_ushort8_t Simd128ShiftLeftU16(dsimd_ushort8_t value, unsigned count)
{
    if (count >= 16)
        return wasm_i16x8_splat(0);
    return wasm_i16x8_shl(value, count);
}
static inline dsimd_ushort8_t Simd128ShiftRightU16(dsimd_ushort8_t value, unsigned count)
{
    if (count >= 16)
        return wasm_i16x8_splat(0);
    return wasm_u16x8_shr(value, count);
}
static inline dsimd_byte16_t Simd128ShuffleBytes(dsimd_byte16_t value, dsimd_byte16_t indices)
{
    return wasm_i8x16_swizzle(value, indices);
}
static inline dsimd_byte16_t Simd128ZipBytesLow(dsimd_byte16_t a, dsimd_byte16_t b)
{
    return wasm_i8x16_shuffle(a, b, 0, 16, 1, 17, 2, 18, 3, 19, 4, 20, 5, 21, 6, 22, 7, 23);
}
static inline dsimd_ushort8_t Simd128WidenBytesLow(dsimd_byte16_t value)
{
    return wasm_u16x8_extend_low_u8x16(value);
}
static inline dsimd_byte16_t Simd128ZipBytesHigh(dsimd_byte16_t a, dsimd_byte16_t b)
{
    return wasm_i8x16_shuffle(a, b, 8, 24, 9, 25, 10, 26, 11, 27, 12, 28, 13, 29, 14, 30, 15, 31);
}
static inline dsimd_ushort8_t Simd128WidenBytesHigh(dsimd_byte16_t value)
{
    return wasm_u16x8_extend_high_u8x16(value);
}
static inline dsimd_uint4_t Simd128WidenU16Low(dsimd_ushort8_t value)
{
    return wasm_u32x4_extend_low_u16x8(value);
}
static inline dsimd_uint4_t Simd128WidenU16High(dsimd_ushort8_t value)
{
    return wasm_u32x4_extend_high_u16x8(value);
}
static inline dsimd_byte16_t Simd128PackBytes(dsimd_ushort8_t low, dsimd_ushort8_t high)
{
    return wasm_u8x16_narrow_i16x8(wasm_u16x8_min(low, wasm_i16x8_splat(255)), wasm_u16x8_min(high, wasm_i16x8_splat(255)));
}
static inline dsimd_ushort8_t Simd128PackU16(dsimd_uint4_t low, dsimd_uint4_t high)
{
    return wasm_u16x8_narrow_i32x4(wasm_u32x4_min(low, wasm_i32x4_splat(65535)), wasm_u32x4_min(high, wasm_i32x4_splat(65535)));
}

static inline dsimd_float4_t Simd128ConvertUintToFloat(dsimd_uint4_t value)
{
    return wasm_f32x4_convert_u32x4(value);
}

static inline dsimd_ushort8_t Simd128NarrowU16(dsimd_uint4_t low, dsimd_uint4_t high)
{
    return wasm_i16x8_shuffle(low, high, 0, 2, 4, 6, 8, 10, 12, 14);
}
static inline dsimd_byte16_t Simd128PackSignedBytes(dsimd_ushort8_t low, dsimd_ushort8_t high)
{
    return wasm_u8x16_narrow_i16x8(low, high);
}

static inline dsimd_float4_t Simd128LoadS16ToFloat(const void* source)
{
    return wasm_f32x4_convert_i32x4(wasm_i32x4_load16x4(source));
}

static inline dsimd_float4_t Simd128LoadS8ToS16Float(const void* source)
{
    v128_t bytes = wasm_v128_load32_zero(source);
    return wasm_f32x4_convert_i32x4(wasm_i32x4_extend_low_i16x8(wasm_i8x16_shuffle(bytes, bytes, 0, 0, 1, 1, 2, 2, 3, 3, 0, 0, 0, 0, 0, 0, 0, 0)));
}

static inline dsimd_float4_t Simd128ConvertS8ToS16Float(dsimd_byte16_t bytes, unsigned quartet)
{
    v128_t words = quartet < 2 ? wasm_i8x16_shuffle(bytes, bytes, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7)
                              : wasm_i8x16_shuffle(bytes, bytes, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13, 14, 14, 15, 15);
    v128_t integers = quartet & 1 ? wasm_i32x4_extend_high_i16x8(words) : wasm_i32x4_extend_low_i16x8(words);
    return wasm_f32x4_convert_i32x4(integers);
}

static inline void Simd128StoreInterleavedS16(void* destination, dsimd_float4_t left, dsimd_float4_t right)
{
    v128_t l = wasm_i32x4_trunc_sat_f32x4(left);
    v128_t r = wasm_i32x4_trunc_sat_f32x4(right);
    v128_t packed = wasm_i16x8_narrow_i32x4(l, r);
    wasm_v128_store(destination, wasm_i16x8_shuffle(packed, packed, 0, 4, 1, 5, 2, 6, 3, 7));
}

#endif // DM_SIMD_WASM_H
