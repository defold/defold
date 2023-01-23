// Copyright 2020-2023 The Defold Foundation
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

#ifndef DM_SPINLOCKTYPES_MACH_H
#define DM_SPINLOCKTYPES_MACH_H

#include <libkern/OSAtomic.h>
namespace dmSpinlock
{
    typedef OSSpinLock Spinlock;

    static inline void Init(Spinlock* lock)
    {
        *lock = 0;
    }

    static inline void Lock(Spinlock* lock)
    {
        OSSpinLockLock(lock);
    }

    static inline void Unlock(Spinlock* lock)
    {
        OSSpinLockUnlock(lock);
    }
}

#endif
