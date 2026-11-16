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

#ifndef DM_SIMD4_H
#define DM_SIMD4_H

#include "simd.h"

/*# Explicit names for the 128-bit SIMD primitive API.
 * Simd4 operations are equivalent to the corresponding Simd operations.
 * Four denotes the float32/int32 width; byte and word operations retain the
 * element counts documented in simd.h. Existing Simd names remain available.
 * @document
 * @name SIMD Four Lanes
 * @language C
 */

#ifdef __cplusplus
extern "C"
{
#endif

static inline dsimd_float4_t Simd4Make(float x, float y, float z, float w)
{
    return SimdMake(x, y, z, w);
}

static inline float Simd4Lane(dsimd_float4_t value, int index)
{
    return SimdLane(value, index);
}

static inline dsimd_float4_t Simd4Add(dsimd_float4_t a, dsimd_float4_t b)
{
    return SimdAdd(a, b);
}

static inline dsimd_float4_t Simd4Sub(dsimd_float4_t a, dsimd_float4_t b)
{
    return SimdSub(a, b);
}

static inline dsimd_float4_t Simd4Mul(dsimd_float4_t a, dsimd_float4_t b)
{
    return SimdMul(a, b);
}

static inline dsimd_float4_t Simd4MulAdd(dsimd_float4_t a, dsimd_float4_t b, dsimd_float4_t c)
{
    return SimdMulAdd(a, b, c);
}

static inline dsimd_float4_t Simd4Div(dsimd_float4_t a, dsimd_float4_t b)
{
    return SimdDiv(a, b);
}

static inline dsimd_float4_t Simd4Sqrt(dsimd_float4_t a)
{
    return SimdSqrt(a);
}

static inline dsimd_float4_t Simd4Neg(dsimd_float4_t a)
{
    return SimdNeg(a);
}

static inline dsimd_float4_t Simd4Abs(dsimd_float4_t a)
{
    return SimdAbs(a);
}

static inline dsimd_float4_t Simd4Xor(dsimd_float4_t a, dsimd_float4_t b)
{
    return SimdXor(a, b);
}

static inline float Simd4Dot3(dsimd_float4_t a, dsimd_float4_t b)
{
    return SimdDot3(a, b);
}

static inline float Simd4Dot4(dsimd_float4_t a, dsimd_float4_t b)
{
    return SimdDot4(a, b);
}

static inline dsimd_float4_t Simd4Shuffle(dsimd_float4_t a, int x, int y, int z, int w)
{
    return (SimdShuffle)(a, x, y, z, w);
}

static inline dsimd_float4_t Simd4Shuffle2(dsimd_float4_t a, dsimd_float4_t b, int x, int y, int z, int w)
{
    return (SimdShuffle2)(a, b, x, y, z, w);
}

static inline void Simd4Unzip(dsimd_float4_t a, dsimd_float4_t b, dsimd_float4_t* left, dsimd_float4_t* right)
{
    SimdUnzip(a, b, left, right);
}

static inline void Simd4Transpose(dsimd_float4_t* a, dsimd_float4_t* b, dsimd_float4_t* c, dsimd_float4_t* d)
{
    SimdTranspose(a, b, c, d);
}

static inline dsimd_float4_t Simd4Load(const void* source)
{
    return SimdLoad(source);
}

static inline void Simd4Store(void* destination, dsimd_float4_t value)
{
    SimdStore(destination, value);
}

static inline dsimd_uint4_t Simd4LoadUint(const void* source)
{
    return SimdLoadUint(source);
}

static inline void Simd4StoreUint(void* destination, dsimd_uint4_t value)
{
    SimdStoreUint(destination, value);
}

static inline dsimd_int4_t Simd4LoadInt(const void* source)
{
    return SimdLoadInt(source);
}

static inline void Simd4StoreInt(void* destination, dsimd_int4_t value)
{
    SimdStoreInt(destination, value);
}

static inline dsimd_ushort8_t Simd4LoadU16(const void* source)
{
    return SimdLoadU16(source);
}

static inline void Simd4StoreU16(void* destination, dsimd_ushort8_t value)
{
    SimdStoreU16(destination, value);
}

static inline dsimd_byte16_t Simd4LoadBytes(const void* source)
{
    return SimdLoadBytes(source);
}

static inline void Simd4StoreBytes(void* destination, dsimd_byte16_t value)
{
    SimdStoreBytes(destination, value);
}

static inline void Simd4LoadInterleavedBytes4(const void* source, dsimd_byte16_t* a, dsimd_byte16_t* b, dsimd_byte16_t* c, dsimd_byte16_t* d)
{
    SimdLoadInterleavedBytes4(source, a, b, c, d);
}

static inline void Simd4StoreInterleavedBytes4(void* destination, dsimd_byte16_t a, dsimd_byte16_t b, dsimd_byte16_t c, dsimd_byte16_t d)
{
    SimdStoreInterleavedBytes4(destination, a, b, c, d);
}

static inline void Simd4LoadInterleavedU16x2(const void* source, dsimd_ushort8_t* a, dsimd_ushort8_t* b)
{
    SimdLoadInterleavedU16x2(source, a, b);
}

static inline dsimd_float4_t Simd4Splat(float value)
{
    return SimdSplat(value);
}

static inline dsimd_float4_t Simd4Ramp(float start, float step)
{
    return SimdRamp(start, step);
}

static inline dsimd_uint4_t Simd4MakeUint(uint32_t x, uint32_t y, uint32_t z, uint32_t w)
{
    return SimdMakeUint(x, y, z, w);
}

static inline dsimd_uint4_t Simd4SplatUint(uint32_t value)
{
    return SimdSplatUint(value);
}

static inline dsimd_ushort8_t Simd4SplatU16(uint16_t value)
{
    return SimdSplatU16(value);
}

static inline dsimd_byte16_t Simd4SplatBytes(uint8_t value)
{
    return SimdSplatBytes(value);
}

static inline dsimd_byte16_t Simd4LoadBytesPartial(const void* source, unsigned count)
{
    return SimdLoadBytesPartial(source, count);
}

static inline void Simd4StoreBytesPartial(void* destination, dsimd_byte16_t value, unsigned count)
{
    SimdStoreBytesPartial(destination, value, count);
}

static inline dsimd_uint4_t Simd4FloatBits(dsimd_float4_t value)
{
    return SimdFloatBits(value);
}

static inline dsimd_float4_t Simd4UintBits(dsimd_uint4_t value)
{
    return SimdUintBits(value);
}

static inline dsimd_uint4_t Simd4BytesToUint(dsimd_byte16_t value)
{
    return SimdBytesToUint(value);
}

static inline dsimd_byte16_t Simd4UintToBytes(dsimd_uint4_t value)
{
    return SimdUintToBytes(value);
}

static inline dsimd_uint4_t Simd4AddUint(dsimd_uint4_t a, dsimd_uint4_t b)
{
    return SimdAddUint(a, b);
}

static inline dsimd_uint4_t Simd4SubUint(dsimd_uint4_t a, dsimd_uint4_t b)
{
    return SimdSubUint(a, b);
}

static inline dsimd_uint4_t Simd4MulUint(dsimd_uint4_t a, dsimd_uint4_t b)
{
    return SimdMulUint(a, b);
}

static inline dsimd_uint4_t Simd4AndUint(dsimd_uint4_t a, dsimd_uint4_t b)
{
    return SimdAndUint(a, b);
}

static inline dsimd_uint4_t Simd4OrUint(dsimd_uint4_t a, dsimd_uint4_t b)
{
    return SimdOrUint(a, b);
}

static inline dsimd_uint4_t Simd4XorUint(dsimd_uint4_t a, dsimd_uint4_t b)
{
    return SimdXorUint(a, b);
}

static inline dsimd_uint4_t Simd4NotUint(dsimd_uint4_t value)
{
    return SimdNotUint(value);
}

static inline dsimd_uint4_t Simd4EqualUint(dsimd_uint4_t a, dsimd_uint4_t b)
{
    return SimdEqualUint(a, b);
}

static inline dsimd_uint4_t Simd4LessUint(dsimd_uint4_t a, dsimd_uint4_t b)
{
    return SimdLessUint(a, b);
}

static inline dsimd_uint4_t Simd4SelectUint(dsimd_uint4_t mask, dsimd_uint4_t yes, dsimd_uint4_t no)
{
    return SimdSelectUint(mask, yes, no);
}

static inline dsimd_uint4_t Simd4MinUint(dsimd_uint4_t a, dsimd_uint4_t b)
{
    return SimdMinUint(a, b);
}

static inline dsimd_uint4_t Simd4MaxUint(dsimd_uint4_t a, dsimd_uint4_t b)
{
    return SimdMaxUint(a, b);
}

static inline dsimd_uint4_t Simd4ShiftLeftUint(dsimd_uint4_t value, unsigned count)
{
    return SimdShiftLeftUint(value, count);
}

static inline dsimd_uint4_t Simd4ShiftRightUint(dsimd_uint4_t value, unsigned count)
{
    return SimdShiftRightUint(value, count);
}

static inline dsimd_uint4_t Simd4ShiftRightUintEach(dsimd_uint4_t value, dsimd_uint4_t counts)
{
    return SimdShiftRightUintEach(value, counts);
}

static inline dsimd_uint4_t Simd4Equal(dsimd_float4_t a, dsimd_float4_t b)
{
    return SimdEqual(a, b);
}

static inline dsimd_uint4_t Simd4Less(dsimd_float4_t a, dsimd_float4_t b)
{
    return SimdLess(a, b);
}

static inline dsimd_float4_t Simd4Select(dsimd_uint4_t mask, dsimd_float4_t yes, dsimd_float4_t no)
{
    return SimdSelect(mask, yes, no);
}

static inline dsimd_float4_t Simd4Min(dsimd_float4_t a, dsimd_float4_t b)
{
    return SimdMin(a, b);
}

static inline dsimd_float4_t Simd4Max(dsimd_float4_t a, dsimd_float4_t b)
{
    return SimdMax(a, b);
}

static inline unsigned Simd4MaskBits(dsimd_uint4_t mask)
{
    return SimdMaskBits(mask);
}

static inline int Simd4Any(dsimd_uint4_t mask)
{
    return SimdAny(mask);
}

static inline int Simd4All(dsimd_uint4_t mask)
{
    return SimdAll(mask);
}

static inline dsimd_float4_t Simd4PairwiseAdd(dsimd_float4_t a, dsimd_float4_t b)
{
    return SimdPairwiseAdd(a, b);
}

static inline float Simd4Sum(dsimd_float4_t value)
{
    return SimdSum(value);
}

static inline float Simd4ReduceMin(dsimd_float4_t value)
{
    return SimdReduceMin(value);
}

static inline float Simd4ReduceMax(dsimd_float4_t value)
{
    return SimdReduceMax(value);
}

static inline dsimd_int4_t Simd4TruncateToInt(dsimd_float4_t value)
{
    return SimdTruncateToInt(value);
}

static inline dsimd_float4_t Simd4ConvertIntToFloat(dsimd_int4_t value)
{
    return SimdConvertIntToFloat(value);
}

static inline dsimd_float4_t Simd4ConvertUintToFloat(dsimd_uint4_t value)
{
    return SimdConvertUintToFloat(value);
}

static inline dsimd_float4_t Simd4LoadS16ToFloat(const void* source)
{
    return SimdLoadS16ToFloat(source);
}

static inline dsimd_float4_t Simd4LoadS8ToS16Float(const void* source)
{
    return SimdLoadS8ToS16Float(source);
}

static inline void Simd4StoreInterleavedS16(void* destination, dsimd_float4_t left, dsimd_float4_t right)
{
    SimdStoreInterleavedS16(destination, left, right);
}

static inline dsimd_ushort8_t Simd4AddU16(dsimd_ushort8_t a, dsimd_ushort8_t b)
{
    return SimdAddU16(a, b);
}

static inline dsimd_ushort8_t Simd4SubU16(dsimd_ushort8_t a, dsimd_ushort8_t b)
{
    return SimdSubU16(a, b);
}

static inline dsimd_ushort8_t Simd4AndU16(dsimd_ushort8_t a, dsimd_ushort8_t b)
{
    return SimdAndU16(a, b);
}

static inline dsimd_ushort8_t Simd4OrU16(dsimd_ushort8_t a, dsimd_ushort8_t b)
{
    return SimdOrU16(a, b);
}

static inline dsimd_ushort8_t Simd4MulU16(dsimd_ushort8_t a, dsimd_ushort8_t b)
{
    return SimdMulU16(a, b);
}

static inline dsimd_ushort8_t Simd4MulHighU16(dsimd_ushort8_t a, dsimd_ushort8_t b)
{
    return SimdMulHighU16(a, b);
}

static inline dsimd_ushort8_t Simd4ShiftLeftU16(dsimd_ushort8_t value, unsigned count)
{
    return SimdShiftLeftU16(value, count);
}

static inline dsimd_ushort8_t Simd4ShiftRightU16(dsimd_ushort8_t value, unsigned count)
{
    return SimdShiftRightU16(value, count);
}

static inline dsimd_byte16_t Simd4ShuffleBytes(dsimd_byte16_t value, dsimd_byte16_t indices)
{
    return SimdShuffleBytes(value, indices);
}

static inline dsimd_byte16_t Simd4ZipBytesLow(dsimd_byte16_t a, dsimd_byte16_t b)
{
    return SimdZipBytesLow(a, b);
}

static inline dsimd_ushort8_t Simd4WidenBytesLow(dsimd_byte16_t value)
{
    return SimdWidenBytesLow(value);
}

static inline dsimd_byte16_t Simd4ZipBytesHigh(dsimd_byte16_t a, dsimd_byte16_t b)
{
    return SimdZipBytesHigh(a, b);
}

static inline dsimd_ushort8_t Simd4WidenBytesHigh(dsimd_byte16_t value)
{
    return SimdWidenBytesHigh(value);
}

static inline dsimd_uint4_t Simd4WidenU16Low(dsimd_ushort8_t value)
{
    return SimdWidenU16Low(value);
}

static inline dsimd_uint4_t Simd4WidenU16High(dsimd_ushort8_t value)
{
    return SimdWidenU16High(value);
}

static inline dsimd_byte16_t Simd4PackBytes(dsimd_ushort8_t low, dsimd_ushort8_t high)
{
    return SimdPackBytes(low, high);
}

static inline dsimd_ushort8_t Simd4PackU16(dsimd_uint4_t low, dsimd_uint4_t high)
{
    return SimdPackU16(low, high);
}

static inline dsimd_ushort8_t Simd4NarrowU16(dsimd_uint4_t low, dsimd_uint4_t high)
{
    return SimdNarrowU16(low, high);
}

static inline dsimd_byte16_t Simd4PackSignedBytes(dsimd_ushort8_t low, dsimd_ushort8_t high)
{
    return SimdPackSignedBytes(low, high);
}

static inline void Simd4Finish(void)
{
    SimdFinish();
}

#ifdef __cplusplus
} // extern "C"
#endif

// Keep constant-index shuffle optimization available to both spellings.
#if defined(SimdShuffle)
#define Simd4Shuffle(a, x, y, z, w) SimdShuffle((a), (x), (y), (z), (w))
#define Simd4Shuffle2(a, b, x, y, z, w) SimdShuffle2((a), (b), (x), (y), (z), (w))
#endif

#endif // DM_SIMD4_H
