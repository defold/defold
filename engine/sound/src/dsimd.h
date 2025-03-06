// Copyright 2020-2024 The Defold Foundation
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

#if defined(__AVX2__)
    #define DM_SIMD_AVX2
#endif
#if defined(__AVX__)
    #define DM_SIMD_AVX
#endif
#if defined(__SSE4_2__)
    #define DM_SIMD_SSE42
#endif
#if defined(__SSE4_1__)
    #define DM_SIMD_SSE41
#endif
#if defined(__SSE3__)
    #define DM_SIMD_SSE3
#endif
#if defined(__SSE2__)
    #define DM_SIMD_SSE2
#endif
#if defined(__SSE__)
    #define DM_SIMD_SSE
#endif
#if defined(__wasm_simd128__)
    #define DM_SIMD_WASM
#endif

// Windows x64
#if (defined(_M_AMD64) || defined(_M_X64))
    #define DM_SIMD_SSE
    #define DM_SIMD_SSE2
#endif
// windows x86
#if defined(_M_IX86_FP)
    #if _M_IX86_FP >= 2
        #define DM_SIMD_SSE2
    #endif
    #if _M_IX86_FP >= 1
        #define DM_SIMD_SSE
    #endif
#endif


// Add the implicit defines
#if defined(DM_SIMD_AVX2) && !defined(DM_SIMD_AVX)
    #define DM_SIMD_AVX
#endif
#if defined(DM_SIMD_AVX) && !defined(DM_SIMD_SSE42)
    #define DM_SIMD_SSE42
#endif
#if defined(DM_SIMD_SSE42) && !defined(DM_SIMD_SSE41)
    #define DM_SIMD_SSE41
#endif
#if defined(DM_SIMD_SSE41) && !defined(DM_SIMD_SSE3)
    #define DM_SIMD_SSE3
#endif
#if defined(DM_SIMD_SSE3) && !defined(DM_SIMD_SSE2)
    #define DM_SIMD_SSE2
#endif
#if defined(DM_SIMD_SSE2) && !defined(DM_SIMD_SSE)
    #define DM_SIMD_SSE
#endif

// https://www.g-truc.net/post-0359.html

#if defined(DM_SIMD_AVX) || defined(DM_SIMD_AVX2)
    #include <immintrin.h>
#endif
#if defined(DM_SIMD_SSE42)
    #include <nmmintrin.h>
#endif
#if defined(DM_SIMD_SSE41)
    #include <smmintrin.h>
#endif
#if defined(DM_SIMD_SSE3)
    #include <pmmintrin.h>
#endif
#if defined(DM_SIMD_SSE2)
    #include <emmintrin.h>
#endif
#if defined(DM_SIMD_SSE)
    #include <xmmintrin.h>
#endif

#if defined(DM_SIMD_WASM)
    #include <wasm_simd128.h>
#endif
