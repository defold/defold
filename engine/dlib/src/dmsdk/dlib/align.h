// Copyright 2020-2022 The Defold Foundation
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

/*# SDK Align API documentation
 * Alignment macros. Use for compiler compatibility
 *
 * @document
 * @name Align
 * @namespace dmAlign
 * @path engine/dlib/src/dmsdk/dlib/align.h
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
#define DM_ALIGN(x, a) (((uintptr_t) (x) + (a-1)) & ~(a-1))

#if defined(__GNUC__)
#define DM_ALIGNED(a) __attribute__ ((aligned (a)))
#elif defined(_MSC_VER)
#define DM_ALIGNED(a) __declspec(align(a))
#else
#error "Unsupported compiler"
#endif

#endif // DMSDK_ALIGN_H
