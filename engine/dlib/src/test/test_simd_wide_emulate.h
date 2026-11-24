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

#ifndef DM_TEST_SIMD_WIDE_EMULATE_H
#define DM_TEST_SIMD_WIDE_EMULATE_H

// Exercise the native wide implementation through portable SIMDe on CPUs without AVX.
#define DM_SIMD_WIDE_H
#define DM_SIMD_FORCE_SCALAR 1
#include <simd/simd.h>
#undef DM_SIMD_WIDE_H
#undef DM_SIMD_FORCE_SCALAR
#define DM_SIMD_FORCE_SCALAR 0
#include "simde/simde.h"
#define DM_SIMD_WIDE_BYTES DM_SIMD_TEST_WIDE_BYTES
#define _mm_cvtsi32_si128 simde_mm_cvtsi32_si128
#define __m256 simde__m256
#define __m256i simde__m256i
#define _mm256_loadu_ps simde_mm256_loadu_ps
#define _mm256_storeu_ps simde_mm256_storeu_ps
#define _mm256_set1_ps simde_mm256_set1_ps
#define _mm256_add_ps simde_mm256_add_ps
#define _mm256_mul_ps simde_mm256_mul_ps
#define _mm256_loadu_si256 simde_mm256_loadu_si256
#define _mm256_storeu_si256 simde_mm256_storeu_si256
#define _mm256_setzero_si256 simde_mm256_setzero_si256
#define _mm256_or_si256 simde_mm256_or_si256
#define _mm256_set1_epi32 simde_mm256_set1_epi32
#define _mm256_set1_epi16 simde_mm256_set1_epi16
#define _mm256_add_epi16 simde_mm256_add_epi16
#define _mm256_mullo_epi16 simde_mm256_mullo_epi16
#define _mm256_srl_epi16 simde_mm256_srl_epi16
#define _mm256_unpacklo_epi8 simde_mm256_unpacklo_epi8
#define _mm256_unpackhi_epi8 simde_mm256_unpackhi_epi8
#define _mm256_min_epu16 simde_mm256_min_epu16
#define _mm256_packus_epi16 simde_mm256_packus_epi16
#define _mm256_shuffle_epi8 simde_mm256_shuffle_epi8
#define _mm256_cmpgt_epi8 simde_mm256_cmpgt_epi8
#define _mm256_set1_epi8 simde_mm256_set1_epi8
#define _mm256_cmplt_epu8_mask simde_mm256_cmplt_epu8_mask
#define _mm256_maskz_shuffle_epi8 simde_mm256_maskz_shuffle_epi8
#define __m512 simde__m512
#define __m512i simde__m512i
#define _mm512_loadu_ps simde_mm512_loadu_ps
#define _mm512_storeu_ps simde_mm512_storeu_ps
#define _mm512_set1_ps simde_mm512_set1_ps
#define _mm512_add_ps simde_mm512_add_ps
#define _mm512_mul_ps simde_mm512_mul_ps
#define _mm512_loadu_si512 simde_mm512_loadu_si512
#define _mm512_storeu_si512 simde_mm512_storeu_si512
#define _mm512_setzero_si512 simde_mm512_setzero_si512
#define _mm512_or_si512 simde_mm512_or_si512
#define _mm512_set1_epi32 simde_mm512_set1_epi32
#define _mm512_set1_epi16 simde_mm512_set1_epi16
#define _mm512_add_epi16 simde_mm512_add_epi16
#define _mm512_mullo_epi16 simde_mm512_mullo_epi16
#define _mm512_srl_epi16 simde_mm512_srl_epi16
#define _mm512_unpacklo_epi8 simde_mm512_unpacklo_epi8
#define _mm512_unpackhi_epi8 simde_mm512_unpackhi_epi8
#define _mm512_min_epu16 simde_mm512_min_epu16
#define _mm512_packus_epi16 simde_mm512_packus_epi16
#define _mm512_shuffle_epi8 simde_mm512_shuffle_epi8
#define _mm512_cmpgt_epi8 simde_mm512_cmpgt_epi8
#define _mm512_set1_epi8 simde_mm512_set1_epi8
#define _mm512_cmplt_epu8_mask simde_mm512_cmplt_epu8_mask
#define _mm512_maskz_shuffle_epi8 simde_mm512_maskz_shuffle_epi8
#if DM_SIMD_TEST_WIDE_BYTES == 64
#define DM_SIMD_AVX512
#else
#define DM_SIMD_AVX2
#endif
#include <simd/simd_wide.h>
#endif // DM_TEST_SIMD_WIDE_EMULATE_H
