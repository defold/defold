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

#ifndef DM_SIMD_SCALAR_H
#define DM_SIMD_SCALAR_H
#include <stdint.h>
#include <string.h>

static inline dsimd_float4_t Simd128Make(float x, float y, float z, float w)
{
    dsimd_float4_t r = { { x, y, z, w } };
    return r;
}
static inline float Simd128Lane(dsimd_float4_t v, int i)
{
    return v.m[i];
}
static inline dsimd_float4_t Simd128Add(dsimd_float4_t a, dsimd_float4_t b)
{
    return Simd128Make(a.m[0] + b.m[0], a.m[1] + b.m[1], a.m[2] + b.m[2], a.m[3] + b.m[3]);
}
static inline dsimd_float4_t Simd128Sub(dsimd_float4_t a, dsimd_float4_t b)
{
    return Simd128Make(a.m[0] - b.m[0], a.m[1] - b.m[1], a.m[2] - b.m[2], a.m[3] - b.m[3]);
}
static inline dsimd_float4_t Simd128Mul(dsimd_float4_t a, dsimd_float4_t b)
{
    return Simd128Make(a.m[0] * b.m[0], a.m[1] * b.m[1], a.m[2] * b.m[2], a.m[3] * b.m[3]);
}
static inline dsimd_float4_t Simd128MulAdd(dsimd_float4_t a, dsimd_float4_t b, dsimd_float4_t c)
{
    return Simd128Make(a.m[0] * b.m[0] + c.m[0], a.m[1] * b.m[1] + c.m[1], a.m[2] * b.m[2] + c.m[2], a.m[3] * b.m[3] + c.m[3]);
}
static inline dsimd_float4_t Simd128Div(dsimd_float4_t a, dsimd_float4_t b)
{
    return Simd128Make(a.m[0] / b.m[0], a.m[1] / b.m[1], a.m[2] / b.m[2], a.m[3] / b.m[3]);
}
static inline dsimd_float4_t Simd128Sqrt(dsimd_float4_t a)
{
    return Simd128Make(sqrtf(a.m[0]), sqrtf(a.m[1]), sqrtf(a.m[2]), sqrtf(a.m[3]));
}
static inline dsimd_float4_t Simd128Neg(dsimd_float4_t a)
{
    return Simd128Make(-a.m[0], -a.m[1], -a.m[2], -a.m[3]);
}
static inline dsimd_float4_t Simd128Abs(dsimd_float4_t a)
{
    return Simd128Make(fabsf(a.m[0]), fabsf(a.m[1]), fabsf(a.m[2]), fabsf(a.m[3]));
}
static inline dsimd_float4_t Simd128Xor(dsimd_float4_t a, dsimd_float4_t b)
{
    uint32_t       av[4], bv[4];
    dsimd_float4_t result;
    memcpy(av, a.m, sizeof(av));
    memcpy(bv, b.m, sizeof(bv));
    for (int lane = 0; lane < 4; ++lane)
        av[lane] ^= bv[lane];
    memcpy(result.m, av, sizeof(av));
    return result;
}
static inline float Simd128Dot3(dsimd_float4_t a, dsimd_float4_t b)
{
    // Keep the legacy accumulation boundaries. A single expression lets
    // ARM compilers contract the first two products in a different order.
    float result = a.m[0] * b.m[0];
    result += a.m[1] * b.m[1];
    result += a.m[2] * b.m[2];
    return result;
}
static inline float Simd128Dot4(dsimd_float4_t a, dsimd_float4_t b)
{
    float result = Simd128Dot3(a, b);
    result += a.m[3] * b.m[3];
    return result;
}
static inline dsimd_float4_t(Simd128Shuffle)(dsimd_float4_t a, int x, int y, int z, int w)
{
    return Simd128Make(Simd128Lane(a, x), Simd128Lane(a, y), Simd128Lane(a, z), Simd128Lane(a, w));
}
static inline void Simd128Transpose(dsimd_float4_t* a, dsimd_float4_t* b, dsimd_float4_t* c, dsimd_float4_t* d)
{
    dsimd_float4_t x = Simd128Make(a->m[0], b->m[0], c->m[0], d->m[0]), y = Simd128Make(a->m[1], b->m[1], c->m[1], d->m[1]);
    dsimd_float4_t z = Simd128Make(a->m[2], b->m[2], c->m[2], d->m[2]), w = Simd128Make(a->m[3], b->m[3], c->m[3], d->m[3]);
    *a = x;
    *b = y;
    *c = z;
    *d = w;
}
static inline dsimd_float4_t(Simd128Shuffle2)(dsimd_float4_t a, dsimd_float4_t b, int x, int y, int z, int w)
{
    return Simd128Make(Simd128Lane(x < 4 ? a : b, x & 3), Simd128Lane(y < 4 ? a : b, y & 3), Simd128Lane(z < 4 ? a : b, z & 3), Simd128Lane(w < 4 ? a : b, w & 3));
}

static inline void Simd128LoadInterleavedU16x2(const void* source, dsimd_ushort8_t* a, dsimd_ushort8_t* b)
{
    uint16_t words[16];
    unsigned i;
    memcpy(words, source, sizeof(words));
    for (i = 0; i < 8; ++i)
    {
        a->m[i] = words[2 * i];
        b->m[i] = words[2 * i + 1];
    }
}
static inline void Simd128LoadInterleavedBytes4(const void* source, dsimd_byte16_t* a, dsimd_byte16_t* b, dsimd_byte16_t* c, dsimd_byte16_t* d)
{
    const uint8_t* bytes = (const uint8_t*)source;
    dsimd_byte16_t av, bv, cv, dv;
    unsigned       i;
    for (i = 0; i < 16; ++i)
    {
        av.m[i] = bytes[4 * i];
        bv.m[i] = bytes[4 * i + 1];
        cv.m[i] = bytes[4 * i + 2];
        dv.m[i] = bytes[4 * i + 3];
    }
    *a = av;
    *b = bv;
    *c = cv;
    *d = dv;
}
static inline void Simd128StoreInterleavedBytes4(void* destination, dsimd_byte16_t a, dsimd_byte16_t b, dsimd_byte16_t c, dsimd_byte16_t d)
{
    uint8_t* bytes = (uint8_t*)destination;
    unsigned i;
    for (i = 0; i < 16; ++i)
    {
        bytes[4 * i] = a.m[i];
        bytes[4 * i + 1] = b.m[i];
        bytes[4 * i + 2] = c.m[i];
        bytes[4 * i + 3] = d.m[i];
    }
}

static inline dsimd_uint4_t Simd128AddUint(dsimd_uint4_t a, dsimd_uint4_t b)
{
    dsimd_uint4_t r;
    unsigned      i;
    for (i = 0; i < 4; ++i)
        r.m[i] = a.m[i] + b.m[i];
    return r;
}
static inline dsimd_uint4_t Simd128SubUint(dsimd_uint4_t a, dsimd_uint4_t b)
{
    dsimd_uint4_t r;
    unsigned      i;
    for (i = 0; i < 4; ++i)
        r.m[i] = a.m[i] - b.m[i];
    return r;
}
static inline dsimd_uint4_t Simd128MulUint(dsimd_uint4_t a, dsimd_uint4_t b)
{
    dsimd_uint4_t r;
    unsigned      i;
    for (i = 0; i < 4; ++i)
        r.m[i] = a.m[i] * b.m[i];
    return r;
}
static inline dsimd_uint4_t Simd128AndUint(dsimd_uint4_t a, dsimd_uint4_t b)
{
    dsimd_uint4_t r;
    unsigned      i;
    for (i = 0; i < 4; ++i)
        r.m[i] = a.m[i] & b.m[i];
    return r;
}
static inline dsimd_uint4_t Simd128OrUint(dsimd_uint4_t a, dsimd_uint4_t b)
{
    dsimd_uint4_t r;
    unsigned      i;
    for (i = 0; i < 4; ++i)
        r.m[i] = a.m[i] | b.m[i];
    return r;
}
static inline dsimd_uint4_t Simd128XorUint(dsimd_uint4_t a, dsimd_uint4_t b)
{
    dsimd_uint4_t r;
    unsigned      i;
    for (i = 0; i < 4; ++i)
        r.m[i] = a.m[i] ^ b.m[i];
    return r;
}
static inline dsimd_uint4_t Simd128EqualUint(dsimd_uint4_t a, dsimd_uint4_t b)
{
    dsimd_uint4_t r;
    unsigned      i;
    for (i = 0; i < 4; ++i)
        r.m[i] = a.m[i] == b.m[i] ? UINT32_MAX : 0;
    return r;
}
static inline dsimd_uint4_t Simd128LessUint(dsimd_uint4_t a, dsimd_uint4_t b)
{
    dsimd_uint4_t r;
    unsigned      i;
    for (i = 0; i < 4; ++i)
        r.m[i] = a.m[i] < b.m[i] ? UINT32_MAX : 0;
    return r;
}
static inline dsimd_uint4_t Simd128SelectUint(dsimd_uint4_t mask, dsimd_uint4_t yes, dsimd_uint4_t no)
{
    dsimd_uint4_t r;
    unsigned      i;
    for (i = 0; i < 4; ++i)
        r.m[i] = (mask.m[i] & yes.m[i]) | (~mask.m[i] & no.m[i]);
    return r;
}
static inline dsimd_uint4_t Simd128ShiftLeftUint(dsimd_uint4_t value, unsigned count)
{
    dsimd_uint4_t r;
    unsigned      i;
    for (i = 0; i < 4; ++i)
        r.m[i] = count < 32 ? value.m[i] << count : 0;
    return r;
}
static inline dsimd_uint4_t Simd128ShiftRightUint(dsimd_uint4_t value, unsigned count)
{
    dsimd_uint4_t r;
    unsigned      i;
    for (i = 0; i < 4; ++i)
        r.m[i] = count < 32 ? value.m[i] >> count : 0;
    return r;
}
static inline dsimd_uint4_t Simd128ShiftRightUintEach(dsimd_uint4_t value, dsimd_uint4_t counts)
{
    dsimd_uint4_t r;
    unsigned      i;
    for (i = 0; i < 4; ++i)
        r.m[i] = counts.m[i] < 32 ? value.m[i] >> counts.m[i] : 0;
    return r;
}
static inline dsimd_uint4_t Simd128Equal(dsimd_float4_t a, dsimd_float4_t b)
{
    dsimd_uint4_t r;
    unsigned      i;
    for (i = 0; i < 4; ++i)
        r.m[i] = a.m[i] == b.m[i] ? UINT32_MAX : 0;
    return r;
}
static inline dsimd_uint4_t Simd128Less(dsimd_float4_t a, dsimd_float4_t b)
{
    dsimd_uint4_t r;
    unsigned      i;
    for (i = 0; i < 4; ++i)
        r.m[i] = a.m[i] < b.m[i] ? UINT32_MAX : 0;
    return r;
}
static inline unsigned Simd128MaskBits(dsimd_uint4_t mask)
{
    return (mask.m[0] >> 31) | ((mask.m[1] >> 31) << 1) | ((mask.m[2] >> 31) << 2) | ((mask.m[3] >> 31) << 3);
}
static inline dsimd_int4_t Simd128TruncateToInt(dsimd_float4_t value)
{
    dsimd_int4_t r;
    unsigned     i;
    for (i = 0; i < 4; ++i)
        r.m[i] = (int32_t)value.m[i];
    return r;
}
static inline dsimd_float4_t Simd128ConvertIntToFloat(dsimd_int4_t value)
{
    dsimd_float4_t r;
    unsigned       i;
    for (i = 0; i < 4; ++i)
        r.m[i] = (float)value.m[i];
    return r;
}
static inline dsimd_ushort8_t Simd128AddU16(dsimd_ushort8_t a, dsimd_ushort8_t b)
{
    dsimd_ushort8_t r;
    unsigned        i;
    for (i = 0; i < 8; ++i)
        r.m[i] = (uint16_t)((uint32_t)a.m[i] + b.m[i]);
    return r;
}
static inline dsimd_ushort8_t Simd128SubU16(dsimd_ushort8_t a, dsimd_ushort8_t b)
{
    dsimd_ushort8_t r;
    unsigned        i;
    for (i = 0; i < 8; ++i)
        r.m[i] = (uint16_t)((uint32_t)a.m[i] - b.m[i]);
    return r;
}
static inline dsimd_ushort8_t Simd128AndU16(dsimd_ushort8_t a, dsimd_ushort8_t b)
{
    dsimd_ushort8_t result;
    unsigned        i;
    for (i = 0; i < 8; ++i)
        result.m[i] = a.m[i] & b.m[i];
    return result;
}
static inline dsimd_ushort8_t Simd128OrU16(dsimd_ushort8_t a, dsimd_ushort8_t b)
{
    dsimd_ushort8_t result;
    unsigned        i;
    for (i = 0; i < 8; ++i)
        result.m[i] = a.m[i] | b.m[i];
    return result;
}
static inline dsimd_ushort8_t Simd128MulU16(dsimd_ushort8_t a, dsimd_ushort8_t b)
{
    dsimd_ushort8_t r;
    unsigned        i;
    for (i = 0; i < 8; ++i)
        r.m[i] = (uint16_t)((uint32_t)a.m[i] * b.m[i]);
    return r;
}
static inline dsimd_ushort8_t Simd128MulHighU16(dsimd_ushort8_t a, dsimd_ushort8_t b)
{
    dsimd_ushort8_t result;
    unsigned        i;
    for (i = 0; i < 8; ++i)
        result.m[i] = (uint16_t)(((uint32_t)a.m[i] * b.m[i]) >> 16);
    return result;
}
static inline dsimd_ushort8_t Simd128ShiftLeftU16(dsimd_ushort8_t value, unsigned count)
{
    dsimd_ushort8_t r;
    unsigned        i;
    for (i = 0; i < 8; ++i)
        r.m[i] = count < 16 ? (uint16_t)((uint32_t)value.m[i] << count) : 0;
    return r;
}
static inline dsimd_ushort8_t Simd128ShiftRightU16(dsimd_ushort8_t value, unsigned count)
{
    dsimd_ushort8_t r;
    unsigned        i;
    for (i = 0; i < 8; ++i)
        r.m[i] = count < 16 ? (uint16_t)((uint32_t)value.m[i] >> count) : 0;
    return r;
}
static inline dsimd_byte16_t Simd128ShuffleBytes(dsimd_byte16_t value, dsimd_byte16_t indices)
{
    dsimd_byte16_t r;
    unsigned       i;
    for (i = 0; i < 16; ++i)
        r.m[i] = indices.m[i] < 16 ? value.m[indices.m[i]] : 0;
    return r;
}
static inline dsimd_byte16_t Simd128ZipBytesLow(dsimd_byte16_t a, dsimd_byte16_t b)
{
    dsimd_byte16_t r;
    unsigned       i;
    for (i = 0; i < 8; ++i)
    {
        r.m[2 * i] = a.m[i + 0];
        r.m[2 * i + 1] = b.m[i + 0];
    }
    return r;
}
static inline dsimd_ushort8_t Simd128WidenBytesLow(dsimd_byte16_t value)
{
    dsimd_ushort8_t r;
    unsigned        i;
    for (i = 0; i < 8; ++i)
        r.m[i] = value.m[i + 0];
    return r;
}
static inline dsimd_byte16_t Simd128ZipBytesHigh(dsimd_byte16_t a, dsimd_byte16_t b)
{
    dsimd_byte16_t r;
    unsigned       i;
    for (i = 0; i < 8; ++i)
    {
        r.m[2 * i] = a.m[i + 8];
        r.m[2 * i + 1] = b.m[i + 8];
    }
    return r;
}
static inline dsimd_ushort8_t Simd128WidenBytesHigh(dsimd_byte16_t value)
{
    dsimd_ushort8_t r;
    unsigned        i;
    for (i = 0; i < 8; ++i)
        r.m[i] = value.m[i + 8];
    return r;
}
static inline dsimd_uint4_t Simd128WidenU16Low(dsimd_ushort8_t value)
{
    dsimd_uint4_t r;
    unsigned      i;
    for (i = 0; i < 4; ++i)
        r.m[i] = value.m[i + 0];
    return r;
}
static inline dsimd_uint4_t Simd128WidenU16High(dsimd_ushort8_t value)
{
    dsimd_uint4_t r;
    unsigned      i;
    for (i = 0; i < 4; ++i)
        r.m[i] = value.m[i + 4];
    return r;
}
static inline dsimd_byte16_t Simd128PackBytes(dsimd_ushort8_t low, dsimd_ushort8_t high)
{
    dsimd_byte16_t r;
    unsigned       i;
    for (i = 0; i < 8; ++i)
    {
        r.m[i] = (uint8_t)(low.m[i] > 255 ? 255 : low.m[i]);
        r.m[i + 8] = (uint8_t)(high.m[i] > 255 ? 255 : high.m[i]);
    }
    return r;
}
static inline dsimd_ushort8_t Simd128PackU16(dsimd_uint4_t low, dsimd_uint4_t high)
{
    dsimd_ushort8_t r;
    unsigned        i;
    for (i = 0; i < 4; ++i)
    {
        r.m[i] = (uint16_t)(low.m[i] > 65535 ? 65535 : low.m[i]);
        r.m[i + 4] = (uint16_t)(high.m[i] > 65535 ? 65535 : high.m[i]);
    }
    return r;
}

static inline dsimd_float4_t Simd128ConvertUintToFloat(dsimd_uint4_t value)
{
    dsimd_float4_t result;
    unsigned       i;
    for (i = 0; i < 4; ++i)
        result.m[i] = (float)value.m[i];
    return result;
}

static inline dsimd_ushort8_t Simd128NarrowU16(dsimd_uint4_t low, dsimd_uint4_t high)
{
    dsimd_ushort8_t result;
    unsigned        i;
    for (i = 0; i < 4; ++i)
    {
        result.m[i] = (uint16_t)low.m[i];
        result.m[i + 4] = (uint16_t)high.m[i];
    }
    return result;
}
static inline dsimd_byte16_t Simd128PackSignedBytes(dsimd_ushort8_t low, dsimd_ushort8_t high)
{
    dsimd_byte16_t result;
    unsigned       i;
    for (i = 0; i < 16; ++i)
    {
        uint16_t bits = i < 8 ? low.m[i] : high.m[i - 8];
        int32_t  value = (int32_t)(bits & 0x7fff) - (int32_t)(bits & 0x8000);
        result.m[i] = (uint8_t)(value < 0 ? 0 : value > 255 ? 255 :
                                                              value);
    }
    return result;
}

static inline dsimd_float4_t Simd128LoadS16ToFloat(const void* source)
{
    int16_t samples[4];
    memcpy(samples, source, sizeof(samples));
    return Simd128Make((float)samples[0], (float)samples[1], (float)samples[2], (float)samples[3]);
}

static inline dsimd_float4_t Simd128LoadS8ToS16Float(const void* source)
{
    int8_t samples[4];
    memcpy(samples, source, sizeof(samples));
    return Simd128Make((float)(samples[0] * 256 + (uint8_t)samples[0]), (float)(samples[1] * 256 + (uint8_t)samples[1]),
                    (float)(samples[2] * 256 + (uint8_t)samples[2]), (float)(samples[3] * 256 + (uint8_t)samples[3]));
}

static inline void Simd128StoreInterleavedS16(void* destination, dsimd_float4_t left, dsimd_float4_t right)
{
    int16_t samples[8];
    unsigned i;
    for (i = 0; i < 4; ++i)
    {
        float l = Simd128Lane(left, i), r = Simd128Lane(right, i);
        samples[2 * i] = (int16_t)(l < -32768.0f ? -32768.0f : l > 32767.0f ? 32767.0f : l);
        samples[2 * i + 1] = (int16_t)(r < -32768.0f ? -32768.0f : r > 32767.0f ? 32767.0f : r);
    }

    memcpy(destination, samples, sizeof(samples));
}

#endif // DM_SIMD_SCALAR_H
