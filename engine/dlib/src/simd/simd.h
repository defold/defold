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

/*# SIMD primitive API
 * 128-bit float, integer and byte registers with scalar, SSE3/SSE4.2, NEON
 * and Wasm implementations.
 * Simd* uses 128-bit registers. Include simd_128.h for explicit Simd128* names.
 * The element type determines the lane count.
 * Arithmetic functions are static inline and allocate no memory. Registers are temporary
 * computation values; their representation/alignment depends on the backend.
 * DM_SIMD_NATIVE is defined for a native 128-bit backend and undefined for scalar.
 * DM_SIMD_FORCE_SCALAR=1 forces scalar primitives. Capability macros remain
 * independent of this choice, so sound's DSP selection is unaffected.
 *
 * @document
 * @name SIMD
 * @language C
 */

/*
 * dsimd_float4_t: four 32-bit float lanes, numbered X=0, Y=1, Z=2, W=3.
 * SimdMake(x,y,z,w): construct all four lanes in that order.
 * SimdLane(value,index): extract a lane; index must be in [0,3].
 * SimdAdd/Sub/Mul/Div(a,b): component-wise arithmetic. Division is accurate.
 * SimdMulAdd(a,b,c): component-wise a*b+c; ARM64 uses fused multiply-add.
 * Other backends use multiply/add and may contract under native compiler rules.
 * This operation does not promise identical rounding across backends.
 * SimdSqrt(a): component-wise accurate square root, including IEEE NaN/zero.
 * SimdNeg/Abs(a): negate or take the absolute value of each lane.
 * SimdXor(a,b): bitwise XOR of each lane's 32-bit representation, including
 * NaN payloads and signed zeros. No floating-point arithmetic is performed.
 * SimdDot3/4(a,b): return the sum of the first three/all four products.
 * SimdShuffle(a,x,y,z,w): select lanes in [0,3], preserving their bits;
 * use constant indices in hot paths so compilers can use native shuffle ops.
 * SimdShuffle2(a,b,x,y,z,w): select from two registers, preserving their bits;
 * indices [0,3] select a and [4,7] select b. Constant indices use native shuffles.
 * SimdTranspose(a,b,c,d): replace four distinct registers with the columns
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

    /*# Supported SIMD instruction set families.
     * SSE versions form an ascending capability chain; NEON and Wasm are separate
     * families. AVX-512 requires Foundation, Dword/Qword, Byte/Word and Vector
     * Length subsets, including instructions emitted by compiler lowering. Capability
     * reporting does not change compile-time backend selection or register types.
     * @enum
     * @name dsimd_version_t
     */
    typedef enum dsimd_version_t
    {
        DSIMD_VERSION_SCALAR,
        DSIMD_VERSION_SSE2,
        DSIMD_VERSION_SSE3,
        DSIMD_VERSION_SSSE3,
        DSIMD_VERSION_SSE41,
        DSIMD_VERSION_SSE42,
        DSIMD_VERSION_NEON,
        DSIMD_VERSION_WASM_SIMD128,
        DSIMD_VERSION_AVX2,
        DSIMD_VERSION_AVX512
    } dsimd_version_t;

    /*# Query the highest SIMD family supported by both the CPU and OS at runtime.
     * @name SimdGetVersion
     * @return version [type:dsimd_version_t] CPU/OS capability, independent of
     * DM_SIMD_FORCE_SCALAR and the compiled consumer backend.
     * Wasm probes host SIMD support; unsupported SIMD modules must still be
     * rejected/selected by the host before instantiation. Consumers must compile
     * their dispatcher to the platform baseline and isolate higher-ISA kernels.
     */
    dsimd_version_t SimdGetVersion(void);

    /*# Get the display name of a SIMD version for diagnostics.
     * @name SimdGetVersionName
     * @param version [type:dsimd_version_t] selected instruction set
     * @return name [type:const char*] static string; never freed by the caller
     */
    const char* SimdGetVersionName(dsimd_version_t version);

    /*# Get the native number of 32-bit lanes for a SIMD version.
     * This describes register width, not CPU support or consumer loop unrolling.
     * @name SimdGetLaneCount
     * @param version [type:dsimd_version_t] instruction set
     * @return lanes [type:uint32_t] 1 for scalar, 4, 8 or 16 for SIMD; 0 for an unknown version
     */
    uint32_t SimdGetLaneCount(dsimd_version_t version);

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
    static inline dsimd_float4_t SimdMake(float x, float y, float z, float w);
    static inline float          SimdLane(dsimd_float4_t value, int index);

    static inline dsimd_float4_t SimdAdd(dsimd_float4_t a, dsimd_float4_t b);
    static inline dsimd_float4_t SimdSub(dsimd_float4_t a, dsimd_float4_t b);
    static inline dsimd_float4_t SimdMul(dsimd_float4_t a, dsimd_float4_t b);
    static inline dsimd_float4_t SimdMulAdd(dsimd_float4_t a, dsimd_float4_t b, dsimd_float4_t c);
    static inline dsimd_float4_t SimdDiv(dsimd_float4_t a, dsimd_float4_t b);
    static inline dsimd_float4_t SimdSqrt(dsimd_float4_t a);
    static inline dsimd_float4_t SimdNeg(dsimd_float4_t a);
    static inline dsimd_float4_t SimdAbs(dsimd_float4_t a);
    static inline dsimd_float4_t SimdXor(dsimd_float4_t a, dsimd_float4_t b);

    static inline float          SimdDot3(dsimd_float4_t a, dsimd_float4_t b);
    static inline float          SimdDot4(dsimd_float4_t a, dsimd_float4_t b);

    static inline dsimd_float4_t SimdShuffle(dsimd_float4_t a, int x, int y, int z, int w);
    static inline dsimd_float4_t SimdShuffle2(dsimd_float4_t a, dsimd_float4_t b, int x, int y, int z, int w);
    // Split eight interleaved lanes in a,b into distinct left/right output registers,
    // preserving all bits: left=[a0,a2,b0,b2], right=[a1,a3,b1,b3].
    static inline void SimdUnzip(dsimd_float4_t a, dsimd_float4_t b, dsimd_float4_t* left, dsimd_float4_t* right);
    static inline void           SimdTranspose(dsimd_float4_t* a, dsimd_float4_t* b, dsimd_float4_t* c, dsimd_float4_t* d);

    // End a kernel after its last vector use, before an ordinary pointer/scalar return.
    // AVX targets clear upper register state; other targets need no cleanup here.
    static inline void SimdFinish(void);

    // Unaligned memory transfers.
    static inline dsimd_float4_t  SimdLoad(const void* source);
    static inline void            SimdStore(void* destination, dsimd_float4_t value);
    static inline dsimd_uint4_t   SimdLoadUint(const void* source);
    static inline void            SimdStoreUint(void* destination, dsimd_uint4_t value);
    static inline dsimd_int4_t    SimdLoadInt(const void* source);
    static inline void            SimdStoreInt(void* destination, dsimd_int4_t value);
    static inline dsimd_ushort8_t SimdLoadU16(const void* source);
    static inline void            SimdStoreU16(void* destination, dsimd_ushort8_t value);
    static inline dsimd_byte16_t  SimdLoadBytes(const void* source);
    static inline void            SimdStoreBytes(void* destination, dsimd_byte16_t value);

    // Unaligned transfers of sixteen four-byte elements, accessing exactly 64 bytes.
    // Load produces channel lanes a[i]=source[4*i], b[i]=source[4*i+1], etc.
    // Caller provides four distinct output registers. Store interleaves those lanes.
    static inline void SimdLoadInterleavedBytes4(const void* source, dsimd_byte16_t* a, dsimd_byte16_t* b, dsimd_byte16_t* c, dsimd_byte16_t* d);
    static inline void SimdStoreInterleavedBytes4(void* destination, dsimd_byte16_t a, dsimd_byte16_t b, dsimd_byte16_t c, dsimd_byte16_t d);

    // Unaligned load of eight two-word elements, accessing exactly 32 bytes.
    // Produces a[i]=source_words[2*i] and b[i]=source_words[2*i+1] in distinct output registers.
    static inline void SimdLoadInterleavedU16x2(const void* source, dsimd_ushort8_t* a, dsimd_ushort8_t* b);

    // Broadcasts, tail-safe transfers and bit reinterpretation.
    static inline dsimd_float4_t  SimdSplat(float value);
    // Four consecutive gains: start+i*step, i in [0,4).
    static inline dsimd_float4_t SimdRamp(float start, float step);
    static inline dsimd_uint4_t   SimdMakeUint(uint32_t x, uint32_t y, uint32_t z, uint32_t w);
    static inline dsimd_uint4_t   SimdSplatUint(uint32_t value);
    static inline dsimd_ushort8_t SimdSplatU16(uint16_t value);
    static inline dsimd_byte16_t  SimdSplatBytes(uint8_t value);
    // Partial transfers require count in [0,16]. Loads zero the remaining lanes;
    // stores leave destination bytes beyond count unchanged. Count zero accesses no memory.
    static inline dsimd_byte16_t SimdLoadBytesPartial(const void* source, unsigned count);
    static inline void           SimdStoreBytesPartial(void* destination, dsimd_byte16_t value, unsigned count);
    static inline dsimd_uint4_t  SimdFloatBits(dsimd_float4_t value);
    static inline dsimd_float4_t SimdUintBits(dsimd_uint4_t value);
    static inline dsimd_uint4_t  SimdBytesToUint(dsimd_byte16_t value);
    static inline dsimd_byte16_t SimdUintToBytes(dsimd_uint4_t value);

    // Bitwise OR of corresponding byte lanes.
    static inline dsimd_byte16_t SimdOrBytes(dsimd_byte16_t a, dsimd_byte16_t b);
    // Byte comparisons are unsigned and return 0xff for true, 0 for false.
    static inline dsimd_byte16_t SimdEqualBytes(dsimd_byte16_t a, dsimd_byte16_t b);
    static inline dsimd_byte16_t SimdLessBytes(dsimd_byte16_t a, dsimd_byte16_t b);
    // Return the first byte lane with its high bit set, or 16 if none is set.
    // Lane zero is the first byte loaded from memory; other bits are ignored.
    static inline unsigned SimdFirstTrueByte(dsimd_byte16_t mask);

    // Wrapping uint32 arithmetic and bitwise operations.
    static inline dsimd_uint4_t SimdAddUint(dsimd_uint4_t a, dsimd_uint4_t b);
    static inline dsimd_uint4_t SimdSubUint(dsimd_uint4_t a, dsimd_uint4_t b);
    static inline dsimd_uint4_t SimdMulUint(dsimd_uint4_t a, dsimd_uint4_t b);
    static inline dsimd_uint4_t SimdAndUint(dsimd_uint4_t a, dsimd_uint4_t b);
    static inline dsimd_uint4_t SimdOrUint(dsimd_uint4_t a, dsimd_uint4_t b);
    static inline dsimd_uint4_t SimdXorUint(dsimd_uint4_t a, dsimd_uint4_t b);
    static inline dsimd_uint4_t SimdNotUint(dsimd_uint4_t value);

    // Unsigned comparison and selection.
    static inline dsimd_uint4_t SimdEqualUint(dsimd_uint4_t a, dsimd_uint4_t b);
    static inline dsimd_uint4_t SimdLessUint(dsimd_uint4_t a, dsimd_uint4_t b);
    static inline dsimd_uint4_t SimdSelectUint(dsimd_uint4_t mask, dsimd_uint4_t yes, dsimd_uint4_t no);
    static inline dsimd_uint4_t SimdMinUint(dsimd_uint4_t a, dsimd_uint4_t b);
    static inline dsimd_uint4_t SimdMaxUint(dsimd_uint4_t a, dsimd_uint4_t b);

    // Logical shifts; out-of-width counts produce zero.
    static inline dsimd_uint4_t SimdShiftLeftUint(dsimd_uint4_t value, unsigned count);
    static inline dsimd_uint4_t SimdShiftRightUint(dsimd_uint4_t value, unsigned count);
    static inline dsimd_uint4_t SimdShiftRightUintEach(dsimd_uint4_t value, dsimd_uint4_t counts);

    // Ordered float comparison, selection and reductions.
    static inline dsimd_uint4_t  SimdEqual(dsimd_float4_t a, dsimd_float4_t b);
    static inline dsimd_uint4_t  SimdLess(dsimd_float4_t a, dsimd_float4_t b);
    static inline dsimd_float4_t SimdSelect(dsimd_uint4_t mask, dsimd_float4_t yes, dsimd_float4_t no);
    static inline dsimd_float4_t SimdMin(dsimd_float4_t a, dsimd_float4_t b);
    static inline dsimd_float4_t SimdMax(dsimd_float4_t a, dsimd_float4_t b);
    // MaskBits maps each lane's sign bit to result bit 0..3. Any/All test whether
    // any/all lane sign bits are set, rather than whether the lanes are nonzero.
    static inline unsigned SimdMaskBits(dsimd_uint4_t mask);
    static inline int      SimdAny(dsimd_uint4_t mask);
    static inline int      SimdAll(dsimd_uint4_t mask);
    // Adjacent pair sums: [a0+a1, a2+a3, b0+b1, b2+b3].
    static inline dsimd_float4_t SimdPairwiseAdd(dsimd_float4_t a, dsimd_float4_t b);
    static inline float    SimdSum(dsimd_float4_t value);
    static inline float    SimdReduceMin(dsimd_float4_t value);
    static inline float    SimdReduceMax(dsimd_float4_t value);

    // Numeric conversions (distinct from bit reinterpretation).
    // TruncateToInt rounds toward zero. Each lane must be finite and in
    // [-2147483648, 2147483648); NaN, infinity and out-of-range inputs are unsupported.
    static inline dsimd_int4_t   SimdTruncateToInt(dsimd_float4_t value);
    static inline dsimd_float4_t SimdConvertIntToFloat(dsimd_int4_t value);
    static inline dsimd_float4_t SimdConvertUintToFloat(dsimd_uint4_t value);

    // Unaligned signed PCM transfers. Loads access exactly four samples (8/4 bytes).
    // S16 converts signed numeric values without normalization. S8 replicates each
    // byte into a signed 16-bit word before conversion, matching PCM8 decoding.
    static inline dsimd_float4_t SimdLoadS16ToFloat(const void* source);
    static inline dsimd_float4_t SimdLoadS8ToS16Float(const void* source);
    // Convert quartet 0..3 of a native sixteen-byte register using the same PCM8 expansion.
    static inline dsimd_float4_t SimdConvertS8ToS16Float(dsimd_byte16_t bytes, unsigned quartet);
    // Clamp finite floats to [-32768,32767], truncate toward zero, and store
    // exactly four interleaved stereo frames (16 bytes), left lane first.
    static inline void SimdStoreInterleavedS16(void* destination, dsimd_float4_t left, dsimd_float4_t right);

    // Unsigned word arithmetic for pixels and convolution.
    static inline dsimd_ushort8_t SimdAddU16(dsimd_ushort8_t a, dsimd_ushort8_t b);
    static inline dsimd_ushort8_t SimdSubU16(dsimd_ushort8_t a, dsimd_ushort8_t b);
    // Bitwise AND/OR of corresponding sixteen-bit word lanes.
    static inline dsimd_ushort8_t SimdAndU16(dsimd_ushort8_t a, dsimd_ushort8_t b);
    static inline dsimd_ushort8_t SimdOrU16(dsimd_ushort8_t a, dsimd_ushort8_t b);
    static inline dsimd_ushort8_t SimdMulU16(dsimd_ushort8_t a, dsimd_ushort8_t b);
    // High sixteen bits of each unsigned sixteen-bit lane product.
    static inline dsimd_ushort8_t SimdMulHighU16(dsimd_ushort8_t a, dsimd_ushort8_t b);
    static inline dsimd_ushort8_t SimdShiftLeftU16(dsimd_ushort8_t value, unsigned count);
    static inline dsimd_ushort8_t SimdShiftRightU16(dsimd_ushort8_t value, unsigned count);

    // Byte layout conversion, unsigned widening and saturated packing.
    static inline dsimd_byte16_t  SimdShuffleBytes(dsimd_byte16_t value, dsimd_byte16_t indices);
    static inline dsimd_byte16_t  SimdZipBytesLow(dsimd_byte16_t a, dsimd_byte16_t b);
    static inline dsimd_ushort8_t SimdWidenBytesLow(dsimd_byte16_t value);
    static inline dsimd_byte16_t  SimdZipBytesHigh(dsimd_byte16_t a, dsimd_byte16_t b);
    static inline dsimd_ushort8_t SimdWidenBytesHigh(dsimd_byte16_t value);
    static inline dsimd_uint4_t   SimdWidenU16Low(dsimd_ushort8_t value);
    static inline dsimd_uint4_t   SimdWidenU16High(dsimd_ushort8_t value);
    static inline dsimd_byte16_t  SimdPackBytes(dsimd_ushort8_t low, dsimd_ushort8_t high);
    static inline dsimd_ushort8_t SimdPackU16(dsimd_uint4_t low, dsimd_uint4_t high);

    // Narrow eight unsigned 32-bit lanes to their low sixteen bits, without saturation.
    static inline dsimd_ushort8_t SimdNarrowU16(dsimd_uint4_t low, dsimd_uint4_t high);
    // Interpret word lanes as signed two's-complement values and clamp each to [0,255].
    // The sixteen result bytes are the eight low lanes followed by the eight high lanes.
    static inline dsimd_byte16_t SimdPackSignedBytes(dsimd_ushort8_t low, dsimd_ushort8_t high);

#if !DM_SIMD_FORCE_SCALAR && defined(DM_SIMD_WASM)
#include "simd_wasm.h"
#define DM_SIMD_BACKEND "wasm"
#define DM_SIMD_NATIVE
#elif !DM_SIMD_FORCE_SCALAR && defined(DM_SIMD_NEON)
#include "simd_neon.h"
#define DM_SIMD_BACKEND "neon"
#define DM_SIMD_NATIVE
#elif !DM_SIMD_FORCE_SCALAR && defined(DM_SIMD_SSE3)
#include "simd_sse3.h"
#define DM_SIMD_BACKEND "x86"
#define DM_SIMD_NATIVE
#else
#include "simd_scalar.h"
#define DM_SIMD_BACKEND "scalar"
#endif

#if !defined(DM_SIMD_SCALAR_H)
    static inline dsimd_float4_t SimdShuffle2(dsimd_float4_t a, dsimd_float4_t b, int x, int y, int z, int w)
    {
        return SimdMake(SimdLane(x < 4 ? a : b, x & 3), SimdLane(y < 4 ? a : b, y & 3), SimdLane(z < 4 ? a : b, z & 3), SimdLane(w < 4 ? a : b, w & 3));
    }
#endif

// Preserve immediate shuffles through the C API, including in C++98 callers.
// Indices select only the first register; the unused second operand avoids
// evaluating a twice. Runtime indices use the inline function fallback.
#if (defined(__clang__) || (defined(__GNUC__) && __GNUC__ >= 12)) && !defined(DM_SIMD_SCALAR_H)
#define SimdShuffle(a, x, y, z, w) \
    ((__builtin_constant_p(x) && __builtin_constant_p(y) && __builtin_constant_p(z) && __builtin_constant_p(w)) ? __builtin_shufflevector((a), SimdMake(0, 0, 0, 0), __builtin_constant_p(x) ? (x) : 0, __builtin_constant_p(y) ? (y) : 0, __builtin_constant_p(z) ? (z) : 0, __builtin_constant_p(w) ? (w) : 0) : SimdShuffle((a), (x), (y), (z), (w)))
#define SimdShuffle2(a, b, x, y, z, w) \
    ((__builtin_constant_p(x) && __builtin_constant_p(y) && __builtin_constant_p(z) && __builtin_constant_p(w)) ? __builtin_shufflevector((a), (b), __builtin_constant_p(x) ? (x) : 0, __builtin_constant_p(y) ? (y) : 0, __builtin_constant_p(z) ? (z) : 0, __builtin_constant_p(w) ? (w) : 0) : SimdShuffle2((a), (b), (x), (y), (z), (w)))
#endif

    // Shared implementations using the selected backend.
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
#elif defined(DM_SIMD_SSE3_H)
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
#elif defined(DM_SIMD_SSE3_H)
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
        {
            lanes[i] = value;
        }

        return SimdLoadBytes(lanes);
    }

    static inline dsimd_byte16_t SimdLoadBytesPartial(const void* source, unsigned count)
    {
        uint8_t bytes[16] = { 0 };
        if (count)
        {
            memcpy(bytes, source, count);
        }

        return SimdLoadBytes(bytes);
    }

    static inline void SimdStoreBytesPartial(void* destination, dsimd_byte16_t value, unsigned count)
    {
        uint8_t bytes[16];
        SimdStoreBytes(bytes, value);
        if (count)
        {
            memcpy(destination, bytes, count);
        }
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

    static inline dsimd_byte16_t SimdOrBytes(dsimd_byte16_t a, dsimd_byte16_t b)
    {
        return SimdUintToBytes(SimdOrUint(SimdBytesToUint(a), SimdBytesToUint(b)));
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
#if defined(DM_SIMD_SSE3_H)
        return _mm_min_ps(a, b);
#else
        return SimdSelect(SimdLess(a, b), a, b);
#endif
    }

    static inline dsimd_float4_t SimdMax(dsimd_float4_t a, dsimd_float4_t b)
    {
#if defined(DM_SIMD_SSE3_H)
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
#if defined(DM_SIMD_SSE3_H)
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
#elif defined(DM_SIMD_SSE3_H)
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
#elif defined(DM_SIMD_SSE3_H) && !defined(DM_SIMD_SSE41)
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


#ifdef __cplusplus
} // extern "C"
#endif

#endif // DM_SIMD_H
