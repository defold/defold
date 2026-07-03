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

#ifndef DMSDK_SPINLOCK_H
#define DMSDK_SPINLOCK_H

/*# Spinlock API documentation
 *
 * API for platform independent spinlock synchronization primitive.
 *
 * @document
 * @name Spinlock
 * @namespace dmSpinlock
 * @language C++
 */

#include <dmsdk/dlib/atomic.h>

namespace dmSpinlock
{
    /*# Spinlock type definition
     *
     * ```cpp
     * struct Spinlock;
     * ```
     *
     * @typedef
     * @name dmSpinlock::Spinlock
     *
     */
    struct Spinlock
    {
        void*          m_Handle;
        int32_atomic_t m_Lock;
    };

    /*# initialize spinlock.
     *
     * Initialize a Spinlock
     *
     * @name dmSpinlock::Create
     * @param spinlock [type:dmSpinlock::Spinlock*] spinlock to initialize.
     */
    void Create(Spinlock* lock);

    /*# destroy spinlock.
     *
     * Destroy a Spinlock
     *
     * @name dmSpinlock::Destroy
     * @param spinlock [type:dmSpinlock::Spinlock*] spinlock to destroy.
     */
    void Destroy(Spinlock* lock);

    /*# lock spinlock.
     *
     * Lock a Spinlock
     *
     * @name dmSpinlock::Lock
     * @param spinlock [type:dmSpinlock::Spinlock*] spinlock to lock.
     */
    void Lock(Spinlock* lock);

    /*# unlock spinlock.
     *
     * Unlock a Spinlock
     *
     * @name dmSpinlock::Unlock
     * @param spinlock [type:dmSpinlock::Spinlock*] spinlock to unlock.
     */
    void Unlock(Spinlock* lock);

    /*# macro for using a spinlock during a scope
     *
     * Will lock a Spinlock and automatically unlock it at the end of the scope.
     *
     * @macro
     * @name DM_SPINLOCK_SCOPED_LOCK
     * @param mutex [type:dmSpinlock::Spinlock] Spinlock reference to lock.
     *
     */

    struct ScopedLock
    {
        dmSpinlock::Spinlock& m_Spinlock;
        ScopedLock(dmSpinlock::Spinlock& spinlock)
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

#endif // DMSDK_SPINLOCK_H
