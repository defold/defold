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

#include <assert.h>
#include <libkern/OSAtomic.h>
#include <stdlib.h>

namespace dmSpinlock
{
    static OSSpinLock* ToNative(Spinlock* lock)
    {
        return (OSSpinLock*) lock->m_Handle;
    }

    void Create(Spinlock* lock)
    {
        OSSpinLock* native_lock = (OSSpinLock*) malloc(sizeof(OSSpinLock));
        assert(native_lock != 0);
        *native_lock = 0;
        lock->m_Handle = native_lock;
        lock->m_Lock = 0;
    }

    void Destroy(Spinlock* lock)
    {
        free(ToNative(lock));
        lock->m_Handle = 0;
    }

    void Lock(Spinlock* lock)
    {
        OSSpinLockLock(ToNative(lock));
    }

    void Unlock(Spinlock* lock)
    {
        OSSpinLockUnlock(ToNative(lock));
    }
}
