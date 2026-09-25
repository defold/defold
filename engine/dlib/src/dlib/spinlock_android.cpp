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

#include "spinlock.h"

#include <stdint.h>

namespace dmSpinlock
{
    void Create(Spinlock* lock)
    {
        lock->m_Lock = 0;
    }

    void Destroy(Spinlock* lock)
    {
        (void) lock;
    }

    void Lock(Spinlock* lock)
    {
#if defined(__aarch64__)
        uint32_t* native_lock = (uint32_t*) &lock->m_Lock;
        uint32_t tmp;
        __asm__ __volatile__(
        "   sevl\n"
        "1: wfe\n"
        "2: ldaxr   %w0, [%1]\n"
        "   cbnz    %w0, 1b\n"
        "   stxr    %w0, %w2, [%1]\n"
        "   cbnz    %w0, 2b\n"
        : "=&r" (tmp)
        : "r" (native_lock), "r" (1)
        : "memory");
#elif defined(__arm__)
        uint32_t* native_lock = (uint32_t*) &lock->m_Lock;
        uint32_t tmp;
        __asm__ __volatile__(
"1:     ldrex   %0, [%1]\n"
"       teq     %0, #0\n"
// "       wfene\n"  -- WFENE gives SIGILL for still unknown reasons on some 64-bit ARMs Aarch32 mode.
//                      Disassembly of libc.so and pthread_mutex_lock shows no use of the instruction.
"       strexeq %0, %2, [%1]\n"
"       teqeq   %0, #0\n"
"       bne     1b\n"
"       dsb\n"
        : "=&r" (tmp)
        : "r" (native_lock), "r" (1)
        : "cc");
#else
        while (dmAtomicCompareStore32(&lock->m_Lock, 1, 0) != 0) {
        }
#endif
    }

    void Unlock(Spinlock* lock)
    {
#if defined(__aarch64__)
        uint32_t* native_lock = (uint32_t*) &lock->m_Lock;
        __asm__ __volatile__(
        "   stlr    %w1, [%0]\n"
        : : "r" (native_lock), "r" (0) : "memory");
#elif defined(__arm__)
        uint32_t* native_lock = (uint32_t*) &lock->m_Lock;
        __asm__ __volatile__(
"       dsb\n"
"       str     %1, [%0]\n"
        :
        : "r" (native_lock), "r" (0)
        : "cc");
#else
        dmAtomicStore32(&lock->m_Lock, 0);
#endif
    }
}
