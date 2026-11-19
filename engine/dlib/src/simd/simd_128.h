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

#ifndef DM_SIMD_128_H
#define DM_SIMD_128_H

#include "simd.h"

/*# Explicit names for the 128-bit SIMD primitive API.
 * Simd128* aliases the Simd* primitives in simd.h.
 * @document
 * @name SIMD 128
 * @language C
 */

#define Simd128Make SimdMake
#define Simd128Lane SimdLane
#define Simd128Add SimdAdd
#define Simd128Sub SimdSub
#define Simd128Mul SimdMul
#define Simd128MulAdd SimdMulAdd
#define Simd128Div SimdDiv
#define Simd128Sqrt SimdSqrt
#define Simd128Neg SimdNeg
#define Simd128Abs SimdAbs
#define Simd128Xor SimdXor
#define Simd128Dot3 SimdDot3
#define Simd128Dot4 SimdDot4
#define Simd128Shuffle SimdShuffle
#define Simd128Shuffle2 SimdShuffle2
#define Simd128Unzip SimdUnzip
#define Simd128Transpose SimdTranspose
#define Simd128Load SimdLoad
#define Simd128Store SimdStore
#define Simd128LoadUint SimdLoadUint
#define Simd128StoreUint SimdStoreUint
#define Simd128LoadInt SimdLoadInt
#define Simd128StoreInt SimdStoreInt
#define Simd128LoadU16 SimdLoadU16
#define Simd128StoreU16 SimdStoreU16
#define Simd128LoadBytes SimdLoadBytes
#define Simd128StoreBytes SimdStoreBytes
#define Simd128LoadInterleavedBytes4 SimdLoadInterleavedBytes4
#define Simd128StoreInterleavedBytes4 SimdStoreInterleavedBytes4
#define Simd128LoadInterleavedU16x2 SimdLoadInterleavedU16x2
#define Simd128Splat SimdSplat
#define Simd128Ramp SimdRamp
#define Simd128MakeUint SimdMakeUint
#define Simd128SplatUint SimdSplatUint
#define Simd128SplatU16 SimdSplatU16
#define Simd128SplatBytes SimdSplatBytes
#define Simd128LoadBytesPartial SimdLoadBytesPartial
#define Simd128StoreBytesPartial SimdStoreBytesPartial
#define Simd128FloatBits SimdFloatBits
#define Simd128UintBits SimdUintBits
#define Simd128BytesToUint SimdBytesToUint
#define Simd128UintToBytes SimdUintToBytes
#define Simd128OrBytes SimdOrBytes
#define Simd128EqualBytes SimdEqualBytes
#define Simd128LessBytes SimdLessBytes
#define Simd128FirstTrueByte SimdFirstTrueByte
#define Simd128AddUint SimdAddUint
#define Simd128SubUint SimdSubUint
#define Simd128MulUint SimdMulUint
#define Simd128AndUint SimdAndUint
#define Simd128OrUint SimdOrUint
#define Simd128XorUint SimdXorUint
#define Simd128NotUint SimdNotUint
#define Simd128EqualUint SimdEqualUint
#define Simd128LessUint SimdLessUint
#define Simd128SelectUint SimdSelectUint
#define Simd128MinUint SimdMinUint
#define Simd128MaxUint SimdMaxUint
#define Simd128ShiftLeftUint SimdShiftLeftUint
#define Simd128ShiftRightUint SimdShiftRightUint
#define Simd128ShiftRightUintEach SimdShiftRightUintEach
#define Simd128Equal SimdEqual
#define Simd128Less SimdLess
#define Simd128Select SimdSelect
#define Simd128Min SimdMin
#define Simd128Max SimdMax
#define Simd128MaskBits SimdMaskBits
#define Simd128Any SimdAny
#define Simd128All SimdAll
#define Simd128PairwiseAdd SimdPairwiseAdd
#define Simd128Sum SimdSum
#define Simd128ReduceMin SimdReduceMin
#define Simd128ReduceMax SimdReduceMax
#define Simd128TruncateToInt SimdTruncateToInt
#define Simd128ConvertIntToFloat SimdConvertIntToFloat
#define Simd128ConvertUintToFloat SimdConvertUintToFloat
#define Simd128LoadS16ToFloat SimdLoadS16ToFloat
#define Simd128LoadS8ToS16Float SimdLoadS8ToS16Float
#define Simd128ConvertS8ToS16Float SimdConvertS8ToS16Float
#define Simd128StoreInterleavedS16 SimdStoreInterleavedS16
#define Simd128AddU16 SimdAddU16
#define Simd128SubU16 SimdSubU16
#define Simd128AndU16 SimdAndU16
#define Simd128OrU16 SimdOrU16
#define Simd128MulU16 SimdMulU16
#define Simd128MulHighU16 SimdMulHighU16
#define Simd128ShiftLeftU16 SimdShiftLeftU16
#define Simd128ShiftRightU16 SimdShiftRightU16
#define Simd128ShuffleBytes SimdShuffleBytes
#define Simd128ZipBytesLow SimdZipBytesLow
#define Simd128WidenBytesLow SimdWidenBytesLow
#define Simd128ZipBytesHigh SimdZipBytesHigh
#define Simd128WidenBytesHigh SimdWidenBytesHigh
#define Simd128WidenU16Low SimdWidenU16Low
#define Simd128WidenU16High SimdWidenU16High
#define Simd128PackBytes SimdPackBytes
#define Simd128PackU16 SimdPackU16
#define Simd128NarrowU16 SimdNarrowU16
#define Simd128PackSignedBytes SimdPackSignedBytes
#define Simd128Finish SimdFinish

#endif // DM_SIMD_128_H
