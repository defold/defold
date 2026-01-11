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

#include <assert.h>
#include "mutex.h"
#include <dmsdk/dlib/log.h>

namespace dmMutex
{
    #define CHECK_RET(ret) \
        if (ret) { \
            dmLogError("%s:%d failed: %d", __FUNCTION__, __LINE__, ret); \
            assert(ret == 0); \
        }

    HMutex New()
    {
        pthread_mutexattr_t attr;
        int ret = pthread_mutexattr_init(&attr);

        // NOTE: We should perhaps consider non-recursive mutex:
        // from http://pubs.opengroup.org/onlinepubs/7908799/xsh/pthread_mutexattr_settype.html
        // It is advised that an application should not use a PTHREAD_MUTEX_RECURSIVE mutex
        // with condition variables because the implicit unlock performed for a pthread_cond_wait()
        // or pthread_cond_timedwait() may not actually release the mutex (if it had been locked
        // multiple times). If this happens, no other thread can satisfy the condition of the predicate.
        pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);

        assert(ret == 0);

        Mutex* mutex = new Mutex();

        ret = pthread_mutex_init(&mutex->m_NativeHandle, &attr);
        CHECK_RET(ret);
        ret = pthread_mutexattr_destroy(&attr);
        CHECK_RET(ret);

        return mutex;
    }

    void Delete(HMutex mutex)
    {
        assert(mutex);
        int ret = pthread_mutex_destroy(&mutex->m_NativeHandle);
        CHECK_RET(ret);
        delete mutex;
    }

    void Lock(HMutex mutex)
    {
        assert(mutex);
        int ret = pthread_mutex_lock(&mutex->m_NativeHandle);
        CHECK_RET(ret);
    }

    bool TryLock(HMutex mutex)
    {
        assert(mutex);
        return (pthread_mutex_trylock(&mutex->m_NativeHandle) == 0) ? true : false;
    }

    void Unlock(HMutex mutex)
    {
        assert(mutex);
        int ret = pthread_mutex_unlock(&mutex->m_NativeHandle);
        CHECK_RET(ret);
    }

    #undef CHECK_RET
}
