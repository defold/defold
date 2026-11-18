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

#ifndef DM_TEST_SIMD_AVX_EMULATE_H
#define DM_TEST_SIMD_AVX_EMULATE_H

// Use the scalar default API and compile the native AVX primitives against
// the test reference intrinsics so the kernels can be exercised on any host.
// Intrinsics outside that subset are declared only. Calling one from a test
// kernel produces a link error.
#ifndef DM_SIMD_FORCE_SCALAR
#define DM_SIMD_FORCE_SCALAR 1
#endif
#include <simd/simd.h>
#undef DM_SIMD_FORCE_SCALAR
#define DM_SIMD_FORCE_SCALAR 0

#include "simde/simde.h"

#define __m128 dsimd_float4_t
#define __m128i simde__m128i
#define __m256 simde__m256
#define __m256i simde__m256i
#define __m512 simde__m512
#define __m512i simde__m512i

#ifndef _MM_SHUFFLE
#define _MM_SHUFFLE(a, b, c, d) (((a) << 6) | ((b) << 4) | ((c) << 2) | (d))
#endif
#ifndef _MM_CMPINT_LT
#define _MM_CMPINT_LT 1
#endif

#if DM_SIMD_TEST_AVX_BITS == 256
#define _mm256_add_epi16 simde_mm256_add_epi16
#define _mm256_add_ps simde_mm256_add_ps
#define _mm256_and_si256 Unemulated_mm256_and_si256
#define _mm256_castps128_ps256 Unemulated_mm256_castps128_ps256
#define _mm256_castps256_ps128 Unemulated_mm256_castps256_ps128
#define _mm256_castsi256_ps Unemulated_mm256_castsi256_ps
#define _mm256_cmpeq_epi32 Unemulated_mm256_cmpeq_epi32
#define _mm256_cmpgt_epi8 simde_mm256_cmpgt_epi8
#define _mm256_cvtepi16_epi32 Unemulated_mm256_cvtepi16_epi32
#define _mm256_cvtepi32_ps Unemulated_mm256_cvtepi32_ps
#define _mm256_cvttps_epi32 Unemulated_mm256_cvttps_epi32
#define _mm256_extractf128_ps Unemulated_mm256_extractf128_ps
#define _mm256_insertf128_ps Unemulated_mm256_insertf128_ps
#define _mm256_loadu_ps simde_mm256_loadu_ps
#define _mm256_loadu_si256 simde_mm256_loadu_si256
#define _mm256_max_ps Unemulated_mm256_max_ps
#define _mm256_min_epu16 simde_mm256_min_epu16
#define _mm256_min_ps Unemulated_mm256_min_ps
#define _mm256_movemask_ps Unemulated_mm256_movemask_ps
#define _mm256_mul_ps simde_mm256_mul_ps
#define _mm256_mullo_epi16 simde_mm256_mullo_epi16
#define _mm256_or_si256 simde_mm256_or_si256
#define _mm256_packs_epi32 Unemulated_mm256_packs_epi32
#define _mm256_packus_epi16 simde_mm256_packus_epi16
#define _mm256_permutevar8x32_ps Unemulated_mm256_permutevar8x32_ps
#define _mm256_set1_epi16 simde_mm256_set1_epi16
#define _mm256_set1_epi32 simde_mm256_set1_epi32
#define _mm256_set1_epi8 simde_mm256_set1_epi8
#define _mm256_set1_ps simde_mm256_set1_ps
#define _mm256_setr_epi32 Unemulated_mm256_setr_epi32
#define _mm256_setzero_si256 simde_mm256_setzero_si256
#define _mm256_shuffle_epi8 simde_mm256_shuffle_epi8
#define _mm256_shuffle_ps Unemulated_mm256_shuffle_ps
#define _mm256_srl_epi16 simde_mm256_srl_epi16
#define _mm256_storeu_ps simde_mm256_storeu_ps
#define _mm256_storeu_si256 simde_mm256_storeu_si256
#define _mm256_sub_epi32 Unemulated_mm256_sub_epi32
#define _mm256_unpackhi_epi32 Unemulated_mm256_unpackhi_epi32
#define _mm256_unpackhi_epi8 simde_mm256_unpackhi_epi8
#define _mm256_unpacklo_epi32 Unemulated_mm256_unpacklo_epi32
#define _mm256_unpacklo_epi8 simde_mm256_unpacklo_epi8
#define _mm256_xor_si256 Unemulated_mm256_xor_si256
#define _mm_add_ps Simd128Add
#define _mm_cvtsi32_si128 simde_mm_cvtsi32_si128
#define _mm_cvtss_f32(value) Simd128Lane((value), 0)
#define _mm_hadd_ps Simd128PairwiseAdd
#define _mm_loadl_epi64 Unemulated_mm_loadl_epi64
#define _mm_loadu_si128 simde_mm_loadu_si128
#define _mm_max_ps Simd128Max
#define _mm_unpacklo_epi8 simde_mm_unpacklo_epi8

// Declarations for intrinsics outside the emulated kernels' scope.
extern __m256i Unemulated_mm256_and_si256(__m256i, __m256i);
extern __m256  Unemulated_mm256_castps128_ps256(__m128);
extern __m128  Unemulated_mm256_castps256_ps128(__m256);
extern __m256  Unemulated_mm256_castsi256_ps(__m256i);
extern __m256i Unemulated_mm256_cmpeq_epi32(__m256i, __m256i);
extern __m256i Unemulated_mm256_cvtepi16_epi32(__m128i);
extern __m256  Unemulated_mm256_cvtepi32_ps(__m256i);
extern __m256i Unemulated_mm256_cvttps_epi32(__m256);
extern __m128  Unemulated_mm256_extractf128_ps(__m256, int);
extern __m256  Unemulated_mm256_insertf128_ps(__m256, __m128, int);
extern __m256  Unemulated_mm256_max_ps(__m256, __m256);
extern __m256  Unemulated_mm256_min_ps(__m256, __m256);
extern int     Unemulated_mm256_movemask_ps(__m256);
extern __m256i Unemulated_mm256_packs_epi32(__m256i, __m256i);
extern __m256  Unemulated_mm256_permutevar8x32_ps(__m256, __m256i);
extern __m256i Unemulated_mm256_setr_epi32(int, int, int, int, int, int, int, int);
extern __m256  Unemulated_mm256_shuffle_ps(__m256, __m256, int);
extern __m256i Unemulated_mm256_sub_epi32(__m256i, __m256i);
extern __m256i Unemulated_mm256_unpackhi_epi32(__m256i, __m256i);
extern __m256i Unemulated_mm256_unpacklo_epi32(__m256i, __m256i);
extern __m256i Unemulated_mm256_xor_si256(__m256i, __m256i);
extern __m128i Unemulated_mm_loadl_epi64(const __m128i*);

#define DM_SIMD_AVX2
#include <simd/simd256.h>
#undef DM_SIMD_AVX2

#undef _mm256_add_epi16
#undef _mm256_add_ps
#undef _mm256_and_si256
#undef _mm256_castps128_ps256
#undef _mm256_castps256_ps128
#undef _mm256_castsi256_ps
#undef _mm256_cmpeq_epi32
#undef _mm256_cmpgt_epi8
#undef _mm256_cvtepi16_epi32
#undef _mm256_cvtepi32_ps
#undef _mm256_cvttps_epi32
#undef _mm256_extractf128_ps
#undef _mm256_insertf128_ps
#undef _mm256_loadu_ps
#undef _mm256_loadu_si256
#undef _mm256_max_ps
#undef _mm256_min_epu16
#undef _mm256_min_ps
#undef _mm256_movemask_ps
#undef _mm256_mul_ps
#undef _mm256_mullo_epi16
#undef _mm256_or_si256
#undef _mm256_packs_epi32
#undef _mm256_packus_epi16
#undef _mm256_permutevar8x32_ps
#undef _mm256_set1_epi16
#undef _mm256_set1_epi32
#undef _mm256_set1_epi8
#undef _mm256_set1_ps
#undef _mm256_setr_epi32
#undef _mm256_setzero_si256
#undef _mm256_shuffle_epi8
#undef _mm256_shuffle_ps
#undef _mm256_srl_epi16
#undef _mm256_storeu_ps
#undef _mm256_storeu_si256
#undef _mm256_sub_epi32
#undef _mm256_unpackhi_epi32
#undef _mm256_unpackhi_epi8
#undef _mm256_unpacklo_epi32
#undef _mm256_unpacklo_epi8
#undef _mm256_xor_si256
#undef _mm_add_ps
#undef _mm_cvtsi32_si128
#undef _mm_cvtss_f32
#undef _mm_hadd_ps
#undef _mm_loadl_epi64
#undef _mm_loadu_si128
#undef _mm_max_ps
#undef _mm_unpacklo_epi8
#elif DM_SIMD_TEST_AVX_BITS == 512
#define _mm256_cvtepu8_epi16 Unemulated_mm256_cvtepu8_epi16
#define _mm256_loadu_si256 simde_mm256_loadu_si256
#define _mm256_mullo_epi16 simde_mm256_mullo_epi16
#define _mm256_set1_epi16 simde_mm256_set1_epi16
#define _mm512_add_epi16 simde_mm512_add_epi16
#define _mm512_add_ps simde_mm512_add_ps
#define _mm512_and_si512 Unemulated_mm512_and_si512
#define _mm512_castps128_ps512 Unemulated_mm512_castps128_ps512
#define _mm512_castps512_ps128 Unemulated_mm512_castps512_ps128
#define _mm512_cmp_epi32_mask Unemulated_mm512_cmp_epi32_mask
#define _mm512_cmpeq_epi32_mask Unemulated_mm512_cmpeq_epi32_mask
#define _mm512_cmplt_epu8_mask simde_mm512_cmplt_epu8_mask
#define _mm512_cvtepi16_epi32 Unemulated_mm512_cvtepi16_epi32
#define _mm512_cvtepi32_ps Unemulated_mm512_cvtepi32_ps
#define _mm512_cvttps_epi32 Unemulated_mm512_cvttps_epi32
#define _mm512_insertf32x4 Unemulated_mm512_insertf32x4
#define _mm512_loadu_ps simde_mm512_loadu_ps
#define _mm512_loadu_si512 simde_mm512_loadu_si512
#define _mm512_maskz_set1_epi32 Unemulated_mm512_maskz_set1_epi32
#define _mm512_maskz_shuffle_epi8 simde_mm512_maskz_shuffle_epi8
#define _mm512_max_ps Unemulated_mm512_max_ps
#define _mm512_min_epu16 simde_mm512_min_epu16
#define _mm512_min_ps Unemulated_mm512_min_ps
#define _mm512_mul_ps simde_mm512_mul_ps
#define _mm512_mullo_epi16 simde_mm512_mullo_epi16
#define _mm512_or_si512 simde_mm512_or_si512
#define _mm512_packs_epi32 Unemulated_mm512_packs_epi32
#define _mm512_packus_epi16 simde_mm512_packus_epi16
#define _mm512_permutex2var_ps Unemulated_mm512_permutex2var_ps
#define _mm512_reduce_add_ps Unemulated_mm512_reduce_add_ps
#define _mm512_reduce_max_ps Unemulated_mm512_reduce_max_ps
#define _mm512_set1_epi16 simde_mm512_set1_epi16
#define _mm512_set1_epi32 simde_mm512_set1_epi32
#define _mm512_set1_epi8 simde_mm512_set1_epi8
#define _mm512_set1_ps simde_mm512_set1_ps
#define _mm512_setr_epi32 Unemulated_mm512_setr_epi32
#define _mm512_setzero_si512 simde_mm512_setzero_si512
#define _mm512_srl_epi16 simde_mm512_srl_epi16
#define _mm512_storeu_ps simde_mm512_storeu_ps
#define _mm512_storeu_si512 simde_mm512_storeu_si512
#define _mm512_sub_epi32 Unemulated_mm512_sub_epi32
#define _mm512_unpackhi_epi32 Unemulated_mm512_unpackhi_epi32
#define _mm512_unpackhi_epi8 simde_mm512_unpackhi_epi8
#define _mm512_unpacklo_epi32 Unemulated_mm512_unpacklo_epi32
#define _mm512_unpacklo_epi8 simde_mm512_unpacklo_epi8
#define _mm512_xor_si512 Unemulated_mm512_xor_si512
#define _mm_cvtsi32_si128 simde_mm_cvtsi32_si128
#define _mm_cvtss_f32(value) Simd128Lane((value), 0)
#define _mm_loadu_si128 simde_mm_loadu_si128

// Declarations for intrinsics outside the emulated kernels' scope.
extern __m256i  Unemulated_mm256_cvtepu8_epi16(__m128i);
extern __m512i  Unemulated_mm512_and_si512(__m512i, __m512i);
extern __m512   Unemulated_mm512_castps128_ps512(__m128);
extern __m128   Unemulated_mm512_castps512_ps128(__m512);
extern uint16_t Unemulated_mm512_cmp_epi32_mask(__m512i, __m512i, int);
extern uint16_t Unemulated_mm512_cmpeq_epi32_mask(__m512i, __m512i);
extern __m512i  Unemulated_mm512_cvtepi16_epi32(__m256i);
extern __m512   Unemulated_mm512_cvtepi32_ps(__m512i);
extern __m512i  Unemulated_mm512_cvttps_epi32(__m512);
extern __m512   Unemulated_mm512_insertf32x4(__m512, __m128, int);
extern __m512i  Unemulated_mm512_maskz_set1_epi32(uint16_t, int);
extern __m512   Unemulated_mm512_max_ps(__m512, __m512);
extern __m512   Unemulated_mm512_min_ps(__m512, __m512);
extern __m512i  Unemulated_mm512_packs_epi32(__m512i, __m512i);
extern __m512   Unemulated_mm512_permutex2var_ps(__m512, __m512i, __m512);
extern float    Unemulated_mm512_reduce_add_ps(__m512);
extern float    Unemulated_mm512_reduce_max_ps(__m512);
extern __m512i  Unemulated_mm512_setr_epi32(int, int, int, int, int, int, int, int, int, int, int, int, int, int, int, int);
extern __m512i  Unemulated_mm512_sub_epi32(__m512i, __m512i);
extern __m512i  Unemulated_mm512_unpackhi_epi32(__m512i, __m512i);
extern __m512i  Unemulated_mm512_unpacklo_epi32(__m512i, __m512i);
extern __m512i  Unemulated_mm512_xor_si512(__m512i, __m512i);

#define DM_SIMD_AVX512
#include <simd/simd512.h>
#undef DM_SIMD_AVX512

#undef _mm256_cvtepu8_epi16
#undef _mm256_loadu_si256
#undef _mm256_mullo_epi16
#undef _mm256_set1_epi16
#undef _mm512_add_epi16
#undef _mm512_add_ps
#undef _mm512_and_si512
#undef _mm512_castps128_ps512
#undef _mm512_castps512_ps128
#undef _mm512_cmp_epi32_mask
#undef _mm512_cmpeq_epi32_mask
#undef _mm512_cmplt_epu8_mask
#undef _mm512_cvtepi16_epi32
#undef _mm512_cvtepi32_ps
#undef _mm512_cvttps_epi32
#undef _mm512_insertf32x4
#undef _mm512_loadu_ps
#undef _mm512_loadu_si512
#undef _mm512_maskz_set1_epi32
#undef _mm512_maskz_shuffle_epi8
#undef _mm512_max_ps
#undef _mm512_min_epu16
#undef _mm512_min_ps
#undef _mm512_mul_ps
#undef _mm512_mullo_epi16
#undef _mm512_or_si512
#undef _mm512_packs_epi32
#undef _mm512_packus_epi16
#undef _mm512_permutex2var_ps
#undef _mm512_reduce_add_ps
#undef _mm512_reduce_max_ps
#undef _mm512_set1_epi16
#undef _mm512_set1_epi32
#undef _mm512_set1_epi8
#undef _mm512_set1_ps
#undef _mm512_setr_epi32
#undef _mm512_setzero_si512
#undef _mm512_srl_epi16
#undef _mm512_storeu_ps
#undef _mm512_storeu_si512
#undef _mm512_sub_epi32
#undef _mm512_unpackhi_epi32
#undef _mm512_unpackhi_epi8
#undef _mm512_unpacklo_epi32
#undef _mm512_unpacklo_epi8
#undef _mm512_xor_si512
#undef _mm_cvtsi32_si128
#undef _mm_cvtss_f32
#undef _mm_loadu_si128
#else
#error AVX emulation requires a test width of 256 or 512 bits
#endif

#undef __m128
#undef __m128i
#undef __m256
#undef __m256i
#undef __m512
#undef __m512i

#undef DM_SIMD_FORCE_SCALAR
#define DM_SIMD_FORCE_SCALAR 1

#undef DM_SIMD_X86_ISA
#define DM_SIMD_X86_ISA DM_SIMD_TEST_AVX_BITS

#endif // DM_TEST_SIMD_AVX_EMULATE_H
