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

#ifndef DM_SIMD256_H
#define DM_SIMD256_H

#include "simd.h"

/*# Native 256-bit SIMD primitives for AVX2.
 * Values stay within one kernel and must not cross an ISA dispatch
 * boundary or appear in exported layouts.
 * Full-width loads and stores access 32 unaligned bytes.
 * Splat repeats one value. Add/Mul/Min/Max operate component-wise.
 * Ramp(start,step) produces start+i*step.
 * Combine concatenates 2 128-bit blocks in argument order.
 * Lane indexes must be in [0,8). Sum/ReduceMax reduce all lanes.
 * Simd256Sum4(a,b,c,d) returns four sums in argument order, matching Simd256Sum.
 * Unzip splits consecutive interleaved lanes in a,b into distinct left/right
 * output registers. LoadS16ToFloat/LoadS8ToS16Float access exactly 8 signed
 * samples. S16 preserves their numeric values. S8 replicates each byte into a
 * signed 16-bit word before conversion, matching PCM8 decoding.
 * StoreInterleavedS16 clamps finite floats to [-32768,32767], truncates toward
 * zero and writes exactly 8 stereo frames, left first. Floating-point
 * reductions and ramps may round differently across widths.
 * Floating-point arithmetic inputs must be finite.
 * Unsigned arithmetic wraps modulo 2^32. Equality returns all-one or zero lane
 * masks. All examines the sign bit of every 32-bit lane, matching Simd128All.
 * AddU16 and MulU16 wrap modulo 65536. ShiftRightU16 returns zero for counts
 * of 16 or greater. ShuffleBytes selects within each 16-byte block and returns
 * zero for indices of 16 or greater. WidenBytesLow/High widen the first/last
 * eight bytes of each block. PackBytes reverses this layout and saturates
 * unsigned words to 255.
 * @document
 * @name SIMD 256
 * @language C
 */

#if !DM_SIMD_FORCE_SCALAR && defined(DM_SIMD_AVX2)

#ifdef __cplusplus
extern "C"
{
#endif

typedef __m256  dsimd_float8_t;
typedef __m256i dsimd_uint8_t;
typedef __m256i dsimd_ushort16_t;
typedef __m256i dsimd_byte32_t;

#include "simd_avx2.h"

#ifdef __cplusplus
} // extern "C"
#endif

#endif // DM_SIMD_AVX2

#endif // DM_SIMD256_H
