// Copyright 2020 The Defold Foundation
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

#ifndef DM_SPINLOCK_H
#define DM_SPINLOCK_H

#include <stdint.h>
#include <dlib/spinlocktypes.h>

namespace dmSpinlock
{
    struct ScopedLock
    {
        dmSpinlock::lock_t& m_Spinlock;
        ScopedLock(dmSpinlock::lock_t& spinlock)
            : m_Spinlock(spinlock)
        {
            dmSpinlock::Lock(&m_Spinlock);
        }

        ~ScopedLock()
        {
            dmSpinlock::Unlock(&m_Spinlock);
        }
    };
}

#define SCOPED_SPINLOCK_PASTE(x, y) x ## y
#define SCOPED_SPINLOCK_PASTE2(x, y) SCOPED_SPINLOCK_PASTE(x, y)
#define DM_SPINLOCK_SCOPED_LOCK(spinlock) dmSpinlock::ScopedLock SCOPED_SPINLOCK_PASTE2(lock, __LINE__)(spinlock);

#endif
