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

#ifndef DM_SIMD_SME_H
#define DM_SIMD_SME_H

#include "simd.h"
#include <arm_sve.h>
#include <arm_sme.h>

/*# Native sixteen-float operations in SME streaming mode.
 * Opt in with DM_SIMD_USE_SME in a separately compiled SME kernel. Call these
 * functions from streaming code only. The caller must verify a 512-bit
 * streaming vector length before entry. No vector values cross the ordinary
 * pointer/scalar dispatch boundary. These operations use streaming SVE;
 * they do not allocate or modify the ZA matrix state.
 * @document
 * @name SIMD SME
 * @language C
 */

typedef svfloat32_t dsimd_float16_t;

__attribute__((always_inline)) static inline dsimd_float16_t Simd16Load(const void* source) __arm_streaming
{
    return svld1_f32(svptrue_b32(), (const float*)source);
}

// Count is in [0,16]; inactive lanes are zero and do not access memory.
__attribute__((always_inline)) static inline dsimd_float16_t Simd16LoadPartial(const void* source, unsigned count) __arm_streaming
{
    return svld1_f32(svwhilelt_b32_u32(0, count), (const float*)source);
}

__attribute__((always_inline)) static inline void Simd16Store(void* destination, dsimd_float16_t value) __arm_streaming
{
    svst1_f32(svptrue_b32(), (float*)destination, value);
}

__attribute__((always_inline)) static inline dsimd_float16_t Simd16Combine(dsimd_float4_t a, dsimd_float4_t b, dsimd_float4_t c, dsimd_float4_t d) __arm_streaming
{
    float values[16];
    memcpy(values, &a, 16);
    memcpy(values + 4, &b, 16);
    memcpy(values + 8, &c, 16);
    memcpy(values + 12, &d, 16);
    return Simd16Load(values);
}

__attribute__((always_inline)) static inline dsimd_float16_t Simd16Splat(float value) __arm_streaming
{
    return svdup_n_f32(value);
}

__attribute__((always_inline)) static inline dsimd_float16_t Simd16Add(dsimd_float16_t a, dsimd_float16_t b) __arm_streaming
{
    return svadd_f32_x(svptrue_b32(), a, b);
}

__attribute__((always_inline)) static inline dsimd_float16_t Simd16Mul(dsimd_float16_t a, dsimd_float16_t b) __arm_streaming
{
    return svmul_f32_x(svptrue_b32(), a, b);
}

__attribute__((always_inline)) static inline dsimd_float16_t Simd16Min(dsimd_float16_t a, dsimd_float16_t b) __arm_streaming
{
    return svmin_f32_x(svptrue_b32(), a, b);
}

__attribute__((always_inline)) static inline dsimd_float16_t Simd16Max(dsimd_float16_t a, dsimd_float16_t b) __arm_streaming
{
    return svmax_f32_x(svptrue_b32(), a, b);
}

__attribute__((always_inline)) static inline dsimd_float16_t Simd16Ramp(float start, float step) __arm_streaming
{
    svfloat32_t indices = svcvt_f32_s32_x(svptrue_b32(), svindex_s32(0, 1));
    return svadd_n_f32_x(svptrue_b32(), svmul_n_f32_x(svptrue_b32(), indices, step), start);
}

__attribute__((always_inline)) static inline float Simd16Lane(dsimd_float16_t value, unsigned index) __arm_streaming
{
    return svlastb_f32(svwhilelt_b32_u32(0, index + 1), value);
}

__attribute__((always_inline)) static inline float Simd16Sum(dsimd_float16_t value) __arm_streaming
{
    svbool_t first_four = svptrue_pat_b32(SV_VL4);
    svfloat32_t sum = svadd_f32_x(first_four, value, svext_f32(value, value, 4));
    sum = svadd_f32_x(first_four, sum, svext_f32(value, value, 8));
    sum = svadd_f32_x(first_four, sum, svext_f32(value, value, 12));
    return svaddv_f32(first_four, sum);
}

__attribute__((always_inline)) static inline float Simd16ReduceMax(dsimd_float16_t value) __arm_streaming
{
    return svmaxv_f32(svptrue_b32(), value);
}

__attribute__((always_inline)) static inline dsimd_float16_t Simd16LoadS16ToFloat(const void* source) __arm_streaming
{
    return svcvt_f32_s32_x(svptrue_b32(), svld1sh_s32(svptrue_b32(), (const int16_t*)source));
}

__attribute__((always_inline)) static inline dsimd_float16_t Simd16LoadS8ToS16Float(const void* source) __arm_streaming
{
    svbool_t all = svptrue_b32();
    svint32_t bytes = svld1sb_s32(all, (const int8_t*)source);
    svint32_t words = svorr_s32_x(all, svlsl_n_s32_x(all, bytes, 8), svand_n_s32_x(all, bytes, 255));
    return svcvt_f32_s32_x(svptrue_b32(), words);
}

__attribute__((always_inline)) static inline void Simd16Unzip(dsimd_float16_t a, dsimd_float16_t b, dsimd_float16_t* left, dsimd_float16_t* right) __arm_streaming
{
    *left = svuzp1_f32(a, b);
    *right = svuzp2_f32(a, b);
}

__attribute__((always_inline)) static inline void Simd16StoreInterleavedS16(void* destination, dsimd_float16_t left, dsimd_float16_t right) __arm_streaming
{
    svbool_t all = svptrue_b32();
    left = svmin_n_f32_x(all, svmax_n_f32_x(all, left, -32768.0f), 32767.0f);
    right = svmin_n_f32_x(all, svmax_n_f32_x(all, right, -32768.0f), 32767.0f);
    svint32_t l = svcvt_s32_f32_x(all, left);
    svint32_t r = svcvt_s32_f32_x(all, right);
    svst1h_s32(all, (int16_t*)destination, svzip1_s32(l, r));
    svst1h_s32(all, (int16_t*)destination + 16, svzip2_s32(l, r));
}

// The function attribute restores streaming mode at the kernel boundary.
__attribute__((always_inline)) static inline void Simd16Finish(void) __arm_streaming
{
}

#endif // DM_SIMD_SME_H
