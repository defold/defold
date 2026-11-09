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

#ifndef DMSDK_ALIGN_H
#define DMSDK_ALIGN_H

#include <stdint.h>

/*# Align API documentation
 * Alignment macros. Use for compiler compatibility
 *
 * @document
 * @name Align
 * @language C
 */

/*# data structure alignment macro
 *
 * Data structure alignment
 *
 * @macro
 * @name DM_ALIGNED
 * @param a [type:int] number of bytes to align to
 *
 * @examples
 *
 * Align m_Data to 16 bytes alignment boundary:
 *
 * ```cpp
 * int DM_ALIGNED(16) m_Data;
 * ```
 */
#define DM_ALIGNED(a)
#undef DM_ALIGNED


/*# value alignment macro
 *
 * Align a value to a boundary
 *
 * @macro
 * @name DM_ALIGN
 * @param x [type:int] value to align
 * @param a [type:int] alignment boundary
 *
 * @examples
 *
 * Align 24 to 16 alignment boundary. results is 32:
 *
 * ```cpp
 * int result = DM_ALIGN(24, 16);
 * ```
 */
#define DM_ALIGN(x, a) (((uintptr_t) (x) + (a-1)) & ~(uintptr_t) (a-1))

/*# 64-bit value alignment macro
 *
 * Round a nonnegative integer value up to an alignment boundary using 64-bit
 * unsigned arithmetic, including on 32-bit targets. The result is a uint64_t.
 * Use DM_ALIGN for pointer alignment.
 *
 * The alignment must be a nonzero power of two, and the rounded value must fit
 * in uint64_t. Overflow is not checked. Arguments must not have side effects.
 *
 * @macro
 * @name DM_ALIGN64
 * @param x [type:uint64_t] nonnegative integer value to align
 * @param a [type:uint64_t] nonzero power-of-two alignment boundary
 *
 * @examples
 *
 * Round up across the 32-bit size boundary without truncating the result:
 *
 * ```cpp
 * uint64_t result = DM_ALIGN64(UINT32_MAX, 16); // 4294967296
 * ```
 */
#define DM_ALIGN64(x, a) (((uint64_t)(x) + ((uint64_t)(a) - 1)) & ~((uint64_t)(a) - 1))

#if defined(__GNUC__)
#define DM_ALIGNED(a) __attribute__ ((aligned (a)))
#elif defined(_MSC_VER)
#define DM_ALIGNED(a) __declspec(align(a))
#else
#error "Unsupported compiler"
#endif

#endif // DMSDK_ALIGN_H
