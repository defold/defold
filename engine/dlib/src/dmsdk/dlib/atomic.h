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


#ifndef DMSDK_ATOMIC_H
#define DMSDK_ATOMIC_H

/*# Atomic functions
 * Atomic functions
 *
 * @document
 * @name Atomic
 * @namespace dmAtomic
 * @path engine/dlib/src/dmsdk/dlib/atomic.h
 */

// platform independent includes
#include <stdint.h>

// platform specific includes
#if defined(_MSC_VER)
#include <dmsdk/dlib/safe_windows.h>
#endif

/*# 32 bit signed integer atomic
 * @typedef
 * @name int32_atomic_t
 */
typedef volatile int32_t int32_atomic_t;

/*#
 * Atomic increment of an int32_atomic_t.
 * @name dmAtomicIncrement32
 * @param ptr [type: int32_atomic_t*] Pointer to an int32_atomic_t to increment.
 * @return prev [type: int32_t] Previous value
 */
inline int32_t dmAtomicIncrement32(int32_atomic_t* ptr)
{
#if defined(_MSC_VER)
    return InterlockedIncrement((volatile long*) ptr)-1;
#else
    return __sync_fetch_and_add((int32_atomic_t*) ptr, 1);
#endif
}

/*#
 * Atomic decrement of an int32_atomic_t.
 * @name dmAtomicDecrement32
 * @param ptr [type: int32_atomic_t*] Pointer to an int32_atomic_t to decrement.
 * @return prev [type: int32_t] Previous value
 */
inline int32_t dmAtomicDecrement32(int32_atomic_t* ptr)
{
#if defined(_MSC_VER)
    return InterlockedDecrement((volatile long*) ptr)+1;
#else
    return __sync_fetch_and_sub((int32_atomic_t*) ptr, 1);
#endif
}


/*#
 * Atomic addition of an int32_atomic_t.
 * @name dmAtomicAdd32
 * @param ptr [type: int32_atomic_t*] Pointer to an int32_atomic_t to add to.
 * @param value [type: int32_t] Value to add.
 * @return prev [type: int32_t] Previous value
 */
inline int32_t dmAtomicAdd32(int32_atomic_t *ptr, int32_t value)
{
#if defined(_MSC_VER)
    return InterlockedExchangeAdd((volatile long*) ptr, (long) value);
#else
    return __sync_fetch_and_add((int32_atomic_t*) ptr, (long) value);
#endif
}

/*#
 * Atomic subtraction of an int32_atomic_t.
 * @name dmAtomicSub32
 * @param ptr [type: int32_atomic_t*] Pointer to an int32_atomic_t to subtract from.
 * @param value [type: int32_t] Value to subtract.
 * @return prev [type: int32_t] Previous value
 */
inline int32_t dmAtomicSub32(int32_atomic_t *ptr, int32_t value)
{
#if defined(_MSC_VER)
    return InterlockedExchangeAdd((volatile long*) ptr, -((long)value));
#else
    return __sync_fetch_and_sub((int32_atomic_t*) ptr, (long) value);
#endif
}

/*#
 * Atomic set (or exchange) of an int32_atomic_t.
 * @name dmAtomicStore32
 * @param ptr [type: int32_atomic_t*] Pointer to an int32_atomic_t to store into.
 * @param value [type: int32_t] Value to set.
 * @return prev [type: int32_t] Previous value
 */
inline int32_t dmAtomicStore32(int32_atomic_t *ptr, int32_t value)
{
#if defined(_MSC_VER)
    return InterlockedExchange((volatile long*) ptr, (long) value);
#else
    return __sync_lock_test_and_set((int32_atomic_t*) ptr, value);
#endif
}

/*#
 * Atomic set (or exchange) of an int32_atomic_t if comparand is equal to the value of ptr.
 * @name dmAtomicCompareStore32
 * @param ptr [type: int32_atomic_t*] Pointer to an int32_atomic_t to store into.
 * @param value [type: int32_t] Value to set.
 * @param comparand [type: int32_t] Value to compare to.
 * @return prev [type: int32_t] Previous value
 */
inline int32_t dmAtomicCompareStore32(int32_atomic_t *ptr, int32_t value, int32_t comparand)
{
#if defined(_MSC_VER)
    return InterlockedCompareExchange((volatile long*) ptr, (long) value, (long) comparand);
#else
    return __sync_val_compare_and_swap((int32_atomic_t*) ptr, comparand, value);
#endif
}

/*#
 * Atomic get of an int32_atomic_t
 * @name dmAtomicGet32
 * @param ptr [type: int32_atomic_t*] Pointer to an int32_atomic_t to get from.
 * @return value [type: int32_t] Current value
 * @note Retrieves the current value by adding 0
 */
inline int32_t dmAtomicGet32(int32_atomic_t *ptr)
{
    return dmAtomicAdd32(ptr, 0);
}

#endif //DMSDK_ATOMIC_H
