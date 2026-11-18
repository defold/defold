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

#ifndef DM_SIMD_H
#define DM_SIMD_H

#include <math.h>
#include <stdint.h>
#include <string.h>
#include "simd_runtime.h"

/*# SIMD primitive API
 * 128-bit float, integer and byte registers with scalar, SSE3/SSE4.2, NEON
 * and Wasm implementations.
 * Simd128* names the vector width in bits. Element types determine lane count.
 * The default Simd* names alias the Simd128* functions.
 * Arithmetic functions are static inline and allocate no memory. Registers are temporary
 * computation values; their representation/alignment depends on the backend.
 * DM_SIMD_FORCE_SCALAR=1 forces scalar primitives. Capability macros remain
 * independent of this choice, so sound's DSP selection is unaffected.
 *
 * @document
 * @name SIMD
 * @language C
 */

/*
 * dsimd_float4_t: four 32-bit float lanes, numbered X=0, Y=1, Z=2, W=3.
 * Simd128Make(x,y,z,w): construct all four lanes in that order.
 * Simd128Lane(value,index): extract a lane; index must be in [0,3].
 * Simd128Add/Sub/Mul/Div(a,b): component-wise arithmetic. Division is accurate.
 * Simd128MulAdd(a,b,c): component-wise a*b+c; ARM64 uses fused multiply-add.
 * Other backends use multiply/add and may contract under native compiler rules.
 * This operation does not promise identical rounding across backends.
 * Simd128Sqrt(a): component-wise accurate square root, including IEEE NaN/zero.
 * Simd128Neg/Abs(a): negate or take the absolute value of each lane.
 * Simd128Xor(a,b): bitwise XOR of each lane's 32-bit representation, including
 * NaN payloads and signed zeros. No floating-point arithmetic is performed.
 * Simd128Dot3/4(a,b): return the sum of the first three/all four products.
 * Simd128Shuffle(a,x,y,z,w): select lanes in [0,3], preserving their bits;
 * use constant indices in hot paths so compilers can use native shuffle ops.
 * Simd128Shuffle2(a,b,x,y,z,w): select from two registers, preserving their bits;
 * indices [0,3] select a and [4,7] select b. Constant indices use native shuffles.
 * Simd128Transpose(a,b,c,d): replace four distinct registers with the columns
 * of the matrix formed by their lanes. All pointers must be non-null.
 * Native floating-point modes apply. No reciprocal/square-root estimates are
 * used. Reduction rounding can differ between SIMD backends.
 */

// Shared compiler capabilities and intrinsic includes. Consumer backend choices
// are independent of these capabilities.
#if defined(__wasm_simd128__)
#define DM_SIMD_WASM
#include <wasm_simd128.h>
#elif defined(__ARM_NEON) || defined(__ARM_NEON__) || defined(_M_ARM64)
#define DM_SIMD_NEON
#if defined(_MSC_VER) && !defined(__clang__) && defined(_M_ARM64)
#include <arm64_neon.h>
#else
#include <arm_neon.h>
#endif
#endif

#if !defined(__wasm__)
#if (defined(__AVX512F__) && defined(__AVX512BW__)) || (defined(_MSC_VER) && defined(DM_SIMD_X86_ISA) && DM_SIMD_X86_ISA == 512)
#define DM_SIMD_AVX512
#endif
#if defined(__AVX2__) || defined(DM_SIMD_AVX512) || (defined(_MSC_VER) && defined(DM_SIMD_X86_ISA) && DM_SIMD_X86_ISA == 256)
#define DM_SIMD_AVX2
#endif
#if defined(__AVX__) || defined(DM_SIMD_AVX2)
#define DM_SIMD_AVX
#endif
#if defined(__SSE4_2__) || defined(DM_SIMD_AVX2) || (defined(_MSC_VER) && defined(DM_SIMD_X86_ISA) && DM_SIMD_X86_ISA == 42)
#define DM_SIMD_SSE42
#endif
#if defined(__SSE4_1__) || defined(DM_SIMD_SSE42)
#define DM_SIMD_SSE41
#endif
#if defined(__SSE3__) || defined(DM_SIMD_SSE41) || (defined(_MSC_VER) && defined(DM_SIMD_X86_ISA) && DM_SIMD_X86_ISA == 3)
#define DM_SIMD_SSE3
#endif
#if defined(__SSE2__) || defined(DM_SIMD_SSE3) || defined(_M_X64) || (defined(_M_IX86_FP) && _M_IX86_FP >= 2)
#define DM_SIMD_SSE2
#endif
#if defined(__SSE__) || defined(DM_SIMD_SSE2) || (defined(_M_IX86_FP) && _M_IX86_FP >= 1)
#define DM_SIMD_SSE
#endif
#if defined(DM_SIMD_AVX)
#include <immintrin.h>
#elif defined(DM_SIMD_SSE42)
#include <nmmintrin.h>
#elif defined(DM_SIMD_SSE41)
#include <smmintrin.h>
#elif defined(DM_SIMD_SSE3)
#include <pmmintrin.h>
#elif defined(DM_SIMD_SSE2)
#include <emmintrin.h>
#elif defined(DM_SIMD_SSE)
#include <xmmintrin.h>
#endif
#endif

#ifndef DM_SIMD_FORCE_SCALAR
#define DM_SIMD_FORCE_SCALAR 0
#endif

#ifdef __cplusplus
extern "C"
{
#endif

#if !DM_SIMD_FORCE_SCALAR && defined(DM_SIMD_WASM)
    typedef v128_t dsimd_float4_t;
    typedef v128_t dsimd_uint4_t;
    typedef v128_t dsimd_int4_t;
    typedef v128_t dsimd_ushort8_t;
    typedef v128_t dsimd_byte16_t;
#elif !DM_SIMD_FORCE_SCALAR && defined(DM_SIMD_NEON)
typedef float32x4_t dsimd_float4_t;
typedef uint32x4_t  dsimd_uint4_t;
typedef int32x4_t   dsimd_int4_t;
typedef uint16x8_t  dsimd_ushort8_t;
typedef uint8x16_t  dsimd_byte16_t;
#elif !DM_SIMD_FORCE_SCALAR && defined(DM_SIMD_SSE3)
typedef __m128  dsimd_float4_t;
typedef __m128i dsimd_uint4_t;
typedef __m128i dsimd_int4_t;
typedef __m128i dsimd_ushort8_t;
typedef __m128i dsimd_byte16_t;
#else
typedef struct dsimd_float4_t
{
    float m[4];
} dsimd_float4_t;
typedef struct dsimd_uint4_t
{
    uint32_t m[4];
} dsimd_uint4_t;
typedef struct dsimd_int4_t
{
    int32_t m[4];
} dsimd_int4_t;
typedef struct dsimd_ushort8_t
{
    uint16_t m[8];
} dsimd_ushort8_t;
typedef struct dsimd_byte16_t
{
    uint8_t m[16];
} dsimd_byte16_t;
#endif

    // Public C interface. Backend definitions follow below.
    static inline dsimd_float4_t Simd128Make(float x, float y, float z, float w);
    static inline float          Simd128Lane(dsimd_float4_t value, int index);

    static inline dsimd_float4_t Simd128Add(dsimd_float4_t a, dsimd_float4_t b);
    static inline dsimd_float4_t Simd128Sub(dsimd_float4_t a, dsimd_float4_t b);
    static inline dsimd_float4_t Simd128Mul(dsimd_float4_t a, dsimd_float4_t b);
    static inline dsimd_float4_t Simd128MulAdd(dsimd_float4_t a, dsimd_float4_t b, dsimd_float4_t c);
    static inline dsimd_float4_t Simd128Div(dsimd_float4_t a, dsimd_float4_t b);
    static inline dsimd_float4_t Simd128Sqrt(dsimd_float4_t a);
    static inline dsimd_float4_t Simd128Neg(dsimd_float4_t a);
    static inline dsimd_float4_t Simd128Abs(dsimd_float4_t a);
    static inline dsimd_float4_t Simd128Xor(dsimd_float4_t a, dsimd_float4_t b);

    static inline float          Simd128Dot3(dsimd_float4_t a, dsimd_float4_t b);
    static inline float          Simd128Dot4(dsimd_float4_t a, dsimd_float4_t b);

    static inline dsimd_float4_t Simd128Shuffle(dsimd_float4_t a, int x, int y, int z, int w);
    static inline dsimd_float4_t Simd128Shuffle2(dsimd_float4_t a, dsimd_float4_t b, int x, int y, int z, int w);
    // Split eight interleaved lanes in a,b into distinct left/right output registers,
    // preserving all bits: left=[a0,a2,b0,b2], right=[a1,a3,b1,b3].
    static inline void Simd128Unzip(dsimd_float4_t a, dsimd_float4_t b, dsimd_float4_t* left, dsimd_float4_t* right);
    static inline void           Simd128Transpose(dsimd_float4_t* a, dsimd_float4_t* b, dsimd_float4_t* c, dsimd_float4_t* d);

    // End a kernel after its last vector use, before an ordinary pointer/scalar return.
    // AVX targets clear upper register state; other targets need no cleanup here.
    static inline void Simd128Finish(void);

    // Unaligned memory transfers.
    static inline dsimd_float4_t  Simd128Load(const void* source);
    static inline void            Simd128Store(void* destination, dsimd_float4_t value);
    static inline dsimd_uint4_t   Simd128LoadUint(const void* source);
    static inline void            Simd128StoreUint(void* destination, dsimd_uint4_t value);
    static inline dsimd_int4_t    Simd128LoadInt(const void* source);
    static inline void            Simd128StoreInt(void* destination, dsimd_int4_t value);
    static inline dsimd_ushort8_t Simd128LoadU16(const void* source);
    static inline void            Simd128StoreU16(void* destination, dsimd_ushort8_t value);
    static inline dsimd_byte16_t  Simd128LoadBytes(const void* source);
    static inline void            Simd128StoreBytes(void* destination, dsimd_byte16_t value);

    // Unaligned transfers of sixteen four-byte elements, accessing exactly 64 bytes.
    // Load produces channel lanes a[i]=source[4*i], b[i]=source[4*i+1], etc.
    // Caller provides four distinct output registers. Store interleaves those lanes.
    static inline void Simd128LoadInterleavedBytes4(const void* source, dsimd_byte16_t* a, dsimd_byte16_t* b, dsimd_byte16_t* c, dsimd_byte16_t* d);
    static inline void Simd128StoreInterleavedBytes4(void* destination, dsimd_byte16_t a, dsimd_byte16_t b, dsimd_byte16_t c, dsimd_byte16_t d);

    // Unaligned load of eight two-word elements, accessing exactly 32 bytes.
    // Produces a[i]=source_words[2*i] and b[i]=source_words[2*i+1] in distinct output registers.
    static inline void Simd128LoadInterleavedU16x2(const void* source, dsimd_ushort8_t* a, dsimd_ushort8_t* b);

    // Broadcasts, tail-safe transfers and bit reinterpretation.
    static inline dsimd_float4_t  Simd128Splat(float value);
    // Four consecutive gains: start+i*step, i in [0,4).
    static inline dsimd_float4_t Simd128Ramp(float start, float step);
    static inline dsimd_uint4_t   Simd128MakeUint(uint32_t x, uint32_t y, uint32_t z, uint32_t w);
    static inline dsimd_uint4_t   Simd128SplatUint(uint32_t value);
    static inline dsimd_ushort8_t Simd128SplatU16(uint16_t value);
    static inline dsimd_byte16_t  Simd128SplatBytes(uint8_t value);
    // Partial transfers require count in [0,16]. Loads zero the remaining lanes;
    // stores leave destination bytes beyond count unchanged. Count zero accesses no memory.
    static inline dsimd_byte16_t Simd128LoadBytesPartial(const void* source, unsigned count);
    static inline void           Simd128StoreBytesPartial(void* destination, dsimd_byte16_t value, unsigned count);
    static inline dsimd_uint4_t  Simd128FloatBits(dsimd_float4_t value);
    static inline dsimd_float4_t Simd128UintBits(dsimd_uint4_t value);
    static inline dsimd_uint4_t  Simd128BytesToUint(dsimd_byte16_t value);
    static inline dsimd_byte16_t Simd128UintToBytes(dsimd_uint4_t value);

    // Wrapping uint32 arithmetic and bitwise operations.
    static inline dsimd_uint4_t Simd128AddUint(dsimd_uint4_t a, dsimd_uint4_t b);
    static inline dsimd_uint4_t Simd128SubUint(dsimd_uint4_t a, dsimd_uint4_t b);
    static inline dsimd_uint4_t Simd128MulUint(dsimd_uint4_t a, dsimd_uint4_t b);
    static inline dsimd_uint4_t Simd128AndUint(dsimd_uint4_t a, dsimd_uint4_t b);
    static inline dsimd_uint4_t Simd128OrUint(dsimd_uint4_t a, dsimd_uint4_t b);
    static inline dsimd_uint4_t Simd128XorUint(dsimd_uint4_t a, dsimd_uint4_t b);
    static inline dsimd_uint4_t Simd128NotUint(dsimd_uint4_t value);

    // Unsigned comparison and selection.
    static inline dsimd_uint4_t Simd128EqualUint(dsimd_uint4_t a, dsimd_uint4_t b);
    static inline dsimd_uint4_t Simd128LessUint(dsimd_uint4_t a, dsimd_uint4_t b);
    static inline dsimd_uint4_t Simd128SelectUint(dsimd_uint4_t mask, dsimd_uint4_t yes, dsimd_uint4_t no);
    static inline dsimd_uint4_t Simd128MinUint(dsimd_uint4_t a, dsimd_uint4_t b);
    static inline dsimd_uint4_t Simd128MaxUint(dsimd_uint4_t a, dsimd_uint4_t b);

    // Logical shifts; out-of-width counts produce zero.
    static inline dsimd_uint4_t Simd128ShiftLeftUint(dsimd_uint4_t value, unsigned count);
    static inline dsimd_uint4_t Simd128ShiftRightUint(dsimd_uint4_t value, unsigned count);
    static inline dsimd_uint4_t Simd128ShiftRightUintEach(dsimd_uint4_t value, dsimd_uint4_t counts);

    // Ordered float comparison, selection and reductions.
    static inline dsimd_uint4_t  Simd128Equal(dsimd_float4_t a, dsimd_float4_t b);
    static inline dsimd_uint4_t  Simd128Less(dsimd_float4_t a, dsimd_float4_t b);
    static inline dsimd_float4_t Simd128Select(dsimd_uint4_t mask, dsimd_float4_t yes, dsimd_float4_t no);
    static inline dsimd_float4_t Simd128Min(dsimd_float4_t a, dsimd_float4_t b);
    static inline dsimd_float4_t Simd128Max(dsimd_float4_t a, dsimd_float4_t b);
    // MaskBits maps each lane's sign bit to result bit 0..3. Any/All test whether
    // any/all lane sign bits are set, rather than whether the lanes are nonzero.
    static inline unsigned Simd128MaskBits(dsimd_uint4_t mask);
    static inline int      Simd128Any(dsimd_uint4_t mask);
    static inline int      Simd128All(dsimd_uint4_t mask);
    // Adjacent pair sums: [a0+a1, a2+a3, b0+b1, b2+b3].
    static inline dsimd_float4_t Simd128PairwiseAdd(dsimd_float4_t a, dsimd_float4_t b);
    static inline float    Simd128Sum(dsimd_float4_t value);
    static inline float    Simd128ReduceMin(dsimd_float4_t value);
    static inline float    Simd128ReduceMax(dsimd_float4_t value);

    // Numeric conversions (distinct from bit reinterpretation).
    // TruncateToInt rounds toward zero. Each lane must be finite and in
    // [-2147483648, 2147483648); NaN, infinity and out-of-range inputs are unsupported.
    static inline dsimd_int4_t   Simd128TruncateToInt(dsimd_float4_t value);
    static inline dsimd_float4_t Simd128ConvertIntToFloat(dsimd_int4_t value);
    static inline dsimd_float4_t Simd128ConvertUintToFloat(dsimd_uint4_t value);

    // Unaligned signed PCM transfers. Loads access exactly four samples (8/4 bytes).
    // S16 converts signed numeric values without normalization. S8 replicates each
    // byte into a signed 16-bit word before conversion, matching PCM8 decoding.
    static inline dsimd_float4_t Simd128LoadS16ToFloat(const void* source);
    static inline dsimd_float4_t Simd128LoadS8ToS16Float(const void* source);
    // Convert quartet 0..3 of a native sixteen-byte register using the same PCM8 expansion.
    static inline dsimd_float4_t Simd128ConvertS8ToS16Float(dsimd_byte16_t bytes, unsigned quartet);
    // Clamp finite floats to [-32768,32767], truncate toward zero, and store
    // exactly four interleaved stereo frames (16 bytes), left lane first.
    static inline void Simd128StoreInterleavedS16(void* destination, dsimd_float4_t left, dsimd_float4_t right);

    // Unsigned word arithmetic for pixels and convolution.
    static inline dsimd_ushort8_t Simd128AddU16(dsimd_ushort8_t a, dsimd_ushort8_t b);
    static inline dsimd_ushort8_t Simd128SubU16(dsimd_ushort8_t a, dsimd_ushort8_t b);
    // Bitwise AND/OR of corresponding sixteen-bit word lanes.
    static inline dsimd_ushort8_t Simd128AndU16(dsimd_ushort8_t a, dsimd_ushort8_t b);
    static inline dsimd_ushort8_t Simd128OrU16(dsimd_ushort8_t a, dsimd_ushort8_t b);
    static inline dsimd_ushort8_t Simd128MulU16(dsimd_ushort8_t a, dsimd_ushort8_t b);
    // High sixteen bits of each unsigned sixteen-bit lane product.
    static inline dsimd_ushort8_t Simd128MulHighU16(dsimd_ushort8_t a, dsimd_ushort8_t b);
    static inline dsimd_ushort8_t Simd128ShiftLeftU16(dsimd_ushort8_t value, unsigned count);
    static inline dsimd_ushort8_t Simd128ShiftRightU16(dsimd_ushort8_t value, unsigned count);

    // Byte layout conversion, unsigned widening and saturated packing.
    static inline dsimd_byte16_t  Simd128ShuffleBytes(dsimd_byte16_t value, dsimd_byte16_t indices);
    static inline dsimd_byte16_t  Simd128ZipBytesLow(dsimd_byte16_t a, dsimd_byte16_t b);
    static inline dsimd_ushort8_t Simd128WidenBytesLow(dsimd_byte16_t value);
    static inline dsimd_byte16_t  Simd128ZipBytesHigh(dsimd_byte16_t a, dsimd_byte16_t b);
    static inline dsimd_ushort8_t Simd128WidenBytesHigh(dsimd_byte16_t value);
    static inline dsimd_uint4_t   Simd128WidenU16Low(dsimd_ushort8_t value);
    static inline dsimd_uint4_t   Simd128WidenU16High(dsimd_ushort8_t value);
    static inline dsimd_byte16_t  Simd128PackBytes(dsimd_ushort8_t low, dsimd_ushort8_t high);
    static inline dsimd_ushort8_t Simd128PackU16(dsimd_uint4_t low, dsimd_uint4_t high);

    // Narrow eight unsigned 32-bit lanes to their low sixteen bits, without saturation.
    static inline dsimd_ushort8_t Simd128NarrowU16(dsimd_uint4_t low, dsimd_uint4_t high);
    // Interpret word lanes as signed two's-complement values and clamp each to [0,255].
    // The sixteen result bytes are the eight low lanes followed by the eight high lanes.
    static inline dsimd_byte16_t Simd128PackSignedBytes(dsimd_ushort8_t low, dsimd_ushort8_t high);

#if !DM_SIMD_FORCE_SCALAR && defined(DM_SIMD_WASM)
#include "simd_wasm.h"
#define DM_SIMD_BACKEND "wasm"
#elif !DM_SIMD_FORCE_SCALAR && defined(DM_SIMD_NEON)
#include "simd_neon.h"
#define DM_SIMD_BACKEND "neon"
#elif !DM_SIMD_FORCE_SCALAR && defined(DM_SIMD_SSE3)
#include "simd_sse3.h"
#define DM_SIMD_BACKEND "x86"
#else
#include "simd_scalar.h"
#define DM_SIMD_BACKEND "scalar"
#endif

#if !defined(DM_SIMD_SCALAR_H)
    static inline dsimd_float4_t Simd128Shuffle2(dsimd_float4_t a, dsimd_float4_t b, int x, int y, int z, int w)
    {
        return Simd128Make(Simd128Lane(x < 4 ? a : b, x & 3), Simd128Lane(y < 4 ? a : b, y & 3), Simd128Lane(z < 4 ? a : b, z & 3), Simd128Lane(w < 4 ? a : b, w & 3));
    }
#endif

// Preserve immediate shuffles through the C API, including in C++98 callers.
// Indices select only the first register; the unused second operand avoids
// evaluating a twice. Runtime indices use the inline function fallback.
#if (defined(__clang__) || (defined(__GNUC__) && __GNUC__ >= 12)) && !defined(DM_SIMD_SCALAR_H)
#define Simd128Shuffle(a, x, y, z, w) \
    ((__builtin_constant_p(x) && __builtin_constant_p(y) && __builtin_constant_p(z) && __builtin_constant_p(w)) ? __builtin_shufflevector((a), Simd128Make(0, 0, 0, 0), __builtin_constant_p(x) ? (x) : 0, __builtin_constant_p(y) ? (y) : 0, __builtin_constant_p(z) ? (z) : 0, __builtin_constant_p(w) ? (w) : 0) : Simd128Shuffle((a), (x), (y), (z), (w)))
#define Simd128Shuffle2(a, b, x, y, z, w) \
    ((__builtin_constant_p(x) && __builtin_constant_p(y) && __builtin_constant_p(z) && __builtin_constant_p(w)) ? __builtin_shufflevector((a), (b), __builtin_constant_p(x) ? (x) : 0, __builtin_constant_p(y) ? (y) : 0, __builtin_constant_p(z) ? (z) : 0, __builtin_constant_p(w) ? (w) : 0) : Simd128Shuffle2((a), (b), (x), (y), (z), (w)))
#endif

#include "simd_generic.h"

#ifdef __cplusplus
} // extern "C"
#endif

// Default names for the 128-bit primitive API.
#define SimdMake Simd128Make
#define SimdLane Simd128Lane
#define SimdAdd Simd128Add
#define SimdSub Simd128Sub
#define SimdMul Simd128Mul
#define SimdMulAdd Simd128MulAdd
#define SimdDiv Simd128Div
#define SimdSqrt Simd128Sqrt
#define SimdNeg Simd128Neg
#define SimdAbs Simd128Abs
#define SimdXor Simd128Xor
#define SimdDot3 Simd128Dot3
#define SimdDot4 Simd128Dot4
#define SimdShuffle Simd128Shuffle
#define SimdShuffle2 Simd128Shuffle2
#define SimdUnzip Simd128Unzip
#define SimdTranspose Simd128Transpose
#define SimdLoad Simd128Load
#define SimdStore Simd128Store
#define SimdLoadUint Simd128LoadUint
#define SimdStoreUint Simd128StoreUint
#define SimdLoadInt Simd128LoadInt
#define SimdStoreInt Simd128StoreInt
#define SimdLoadU16 Simd128LoadU16
#define SimdStoreU16 Simd128StoreU16
#define SimdLoadBytes Simd128LoadBytes
#define SimdStoreBytes Simd128StoreBytes
#define SimdLoadInterleavedBytes4 Simd128LoadInterleavedBytes4
#define SimdStoreInterleavedBytes4 Simd128StoreInterleavedBytes4
#define SimdLoadInterleavedU16x2 Simd128LoadInterleavedU16x2
#define SimdSplat Simd128Splat
#define SimdRamp Simd128Ramp
#define SimdMakeUint Simd128MakeUint
#define SimdSplatUint Simd128SplatUint
#define SimdSplatU16 Simd128SplatU16
#define SimdSplatBytes Simd128SplatBytes
#define SimdLoadBytesPartial Simd128LoadBytesPartial
#define SimdStoreBytesPartial Simd128StoreBytesPartial
#define SimdFloatBits Simd128FloatBits
#define SimdUintBits Simd128UintBits
#define SimdBytesToUint Simd128BytesToUint
#define SimdUintToBytes Simd128UintToBytes
#define SimdAddUint Simd128AddUint
#define SimdSubUint Simd128SubUint
#define SimdMulUint Simd128MulUint
#define SimdAndUint Simd128AndUint
#define SimdOrUint Simd128OrUint
#define SimdXorUint Simd128XorUint
#define SimdNotUint Simd128NotUint
#define SimdEqualUint Simd128EqualUint
#define SimdLessUint Simd128LessUint
#define SimdSelectUint Simd128SelectUint
#define SimdMinUint Simd128MinUint
#define SimdMaxUint Simd128MaxUint
#define SimdShiftLeftUint Simd128ShiftLeftUint
#define SimdShiftRightUint Simd128ShiftRightUint
#define SimdShiftRightUintEach Simd128ShiftRightUintEach
#define SimdEqual Simd128Equal
#define SimdLess Simd128Less
#define SimdSelect Simd128Select
#define SimdMin Simd128Min
#define SimdMax Simd128Max
#define SimdMaskBits Simd128MaskBits
#define SimdAny Simd128Any
#define SimdAll Simd128All
#define SimdPairwiseAdd Simd128PairwiseAdd
#define SimdSum Simd128Sum
#define SimdReduceMin Simd128ReduceMin
#define SimdReduceMax Simd128ReduceMax
#define SimdTruncateToInt Simd128TruncateToInt
#define SimdConvertIntToFloat Simd128ConvertIntToFloat
#define SimdConvertUintToFloat Simd128ConvertUintToFloat
#define SimdLoadS16ToFloat Simd128LoadS16ToFloat
#define SimdLoadS8ToS16Float Simd128LoadS8ToS16Float
#define SimdConvertS8ToS16Float Simd128ConvertS8ToS16Float
#define SimdStoreInterleavedS16 Simd128StoreInterleavedS16
#define SimdAddU16 Simd128AddU16
#define SimdSubU16 Simd128SubU16
#define SimdAndU16 Simd128AndU16
#define SimdOrU16 Simd128OrU16
#define SimdMulU16 Simd128MulU16
#define SimdMulHighU16 Simd128MulHighU16
#define SimdShiftLeftU16 Simd128ShiftLeftU16
#define SimdShiftRightU16 Simd128ShiftRightU16
#define SimdShuffleBytes Simd128ShuffleBytes
#define SimdZipBytesLow Simd128ZipBytesLow
#define SimdWidenBytesLow Simd128WidenBytesLow
#define SimdZipBytesHigh Simd128ZipBytesHigh
#define SimdWidenBytesHigh Simd128WidenBytesHigh
#define SimdWidenU16Low Simd128WidenU16Low
#define SimdWidenU16High Simd128WidenU16High
#define SimdPackBytes Simd128PackBytes
#define SimdPackU16 Simd128PackU16
#define SimdNarrowU16 Simd128NarrowU16
#define SimdPackSignedBytes Simd128PackSignedBytes
#define SimdFinish Simd128Finish

#endif // DM_SIMD_H
