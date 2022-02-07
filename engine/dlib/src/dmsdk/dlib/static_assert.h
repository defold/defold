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

#ifndef DMSDK_STATIC_ASSERT_H
#define DMSDK_STATIC_ASSERT_H

/*# Static assert
 *
 *
 * ```cpp
 * void test() {
 *     DM_STATIC_ASSERT(sizeof(int) == 4, Invalid_int_size);
 * }
 * ```
 *
 * @document
 * @name Static Assert
 * @namespace dmStaticAssert
 * @path engine/dlib/src/dmsdk/dlib/static_assert.h
 */

/*# compile time assert
 * This is using C++11 `static_assert` on platforms that support it and use c++11. Otherwise
 * it's using a c construct to check the condition.
 * As such, it is currently required to be used whithin a function scope.
 *
 * @macro
 * @name DM_STATIC_ASSERT
 * @param x [type:bool] expression
 * @param xmsg [type:string] expression
 *
 * @examples
 * Verify the size of a struct is within a limit
 *
 * ```cpp
 * DM_STATIC_ASSERT(sizeof(MyStruct) <= 32, Invalid_Struct_Size);
 * ```
 */

#if __cplusplus >= 201103L
    // For backwards compatibility, we need to keep the old error format
    #define DM_STATIC_ASSERT(x, error) static_assert(x, #error)
#else
    #define DM_STATIC_ASSERT(x, error) \
        do { \
            static const char error[(x)?1:-1] = {0};\
            (void) error;\
        } while(0)
#endif

// Trick to remove false positives when building using the clang analyzer

#if defined(__clang_analyzer__)
    inline void analyzer_use_pointer(void** pp){
        *((uintptr_t)p) = (uintptr_t)1;
    }
    #define ANALYZE_USE_POINTER(_P) analyzer_use_pointer((void**)&(_P))
#else
    #define ANALYZE_USE_POINTER(_P)
#endif

#endif // DMSDK_STATIC_ASSERT_H
