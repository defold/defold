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

#ifndef DMSDK_MUTEX_H
#define DMSDK_MUTEX_H

namespace dmMutex
{
    /*# SDK Mutex API documentation
     *
     * API for platform independent mutex synchronization primitive.
     *
     * @document
     * @name Mutex
     * @namespace dmMutex
     * @path engine/dlib/src/dmsdk/dlib/mutex.h
     */

    /*# HMutex type definition
     *
     * ```cpp
     * typedef struct Mutex* HMutex;
     * ```
     *
     * @typedef
     * @name dmMutex::HMutex
     *
     */
    typedef struct Mutex* HMutex;

    /*# create Mutex
     *
     * Creates a new HMutex.
     *
     * @name dmMutex::New
     * @return mutex [type:dmMutex::HMutex] A new Mutex handle.
     */
    HMutex New();

    /*# delete Mutex.
     *
     * Deletes a HMutex.
     *
     * @name dmMutex::Delete
     * @param mutex [type:dmMutex::HMutex] Mutex handle to delete.
     */
    void Delete(HMutex mutex);

    /*# lock Mutex.
     *
     * Lock a HMutex, will block until mutex is unlocked if already locked elsewhere.
     *
     * @name dmMutex::Lock
     * @param mutex [type:dmMutex::HMutex] Mutex handle to lock.
     */
    void Lock(HMutex mutex);

    /*# non-blocking lock of Mutex.
     *
     * Tries to lock a HMutex, if mutex is already locked it will return false and
     * continue without locking the mutex.
     *
     * @name dmMutex::TryLock
     * @param mutex [type:dmMutex::HMutex] Mutex handle to lock.
     * @return result [type:bool] True if mutex was successfully locked, false otherwise.
     */
    bool TryLock(HMutex mutex);

    /*# unlock Mutex.
     *
     * Unlock a HMutex.
     *
     * @name dmMutex::Unlock
     * @param mutex [type:dmMutex::HMutex] Mutex handle to unlock.
     */
    void Unlock(HMutex mutex);

    struct ScopedLock
    {
        HMutex m_Mutex;
        ScopedLock(HMutex mutex)
        {
            m_Mutex = mutex;
            Lock(m_Mutex);
        }

        ~ScopedLock()
        {
            Unlock(m_Mutex);
        }
    };

    /*# macro for scope lifetime Mutex locking
     *
     * Will lock a Mutex and automatically unlock it at the end of the scope.
     *
     * @macro
     * @name DM_MUTEX_SCOPED_LOCK
     * @param mutex [type:dmMutex::HMutex] Mutex handle to lock.
     *
     */
    #define SCOPED_LOCK_PASTE(x, y) x ## y
    #define SCOPED_LOCK_PASTE2(x, y) SCOPED_LOCK_PASTE(x, y)
    #define DM_MUTEX_SCOPED_LOCK(mutex) dmMutex::ScopedLock SCOPED_LOCK_PASTE2(lock, __LINE__)(mutex);

}  // namespace dmMutex

#endif // #ifndef DMSDK_MUTEX_H
