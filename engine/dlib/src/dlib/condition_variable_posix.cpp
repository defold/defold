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

#if !(defined(__linux__) || defined(__MACH__))
#error "Unsupported platform"
#endif

#include <assert.h>
#include "condition_variable.h"
#include "mutex.h"

#include <pthread.h>
#include <errno.h>
#include <time.h>

namespace dmConditionVariable
{

    struct ConditionVariable
    {
        pthread_cond_t m_NativeHandle;
    };

    HConditionVariable New()
    {
        ConditionVariable* condition = new ConditionVariable();
#if defined(__linux__)
        pthread_condattr_t attributes;
        int ret = pthread_condattr_init(&attributes);
        assert(ret == 0);
        ret = pthread_condattr_setclock(&attributes, CLOCK_MONOTONIC);
        assert(ret == 0);
        ret = pthread_cond_init(&condition->m_NativeHandle, &attributes);
        pthread_condattr_destroy(&attributes);
#else
        int ret = pthread_cond_init(&condition->m_NativeHandle, 0);
#endif
        assert(ret == 0);
        return condition;
    }

    void Delete(HConditionVariable condition)
    {
        assert(condition);
        int ret = pthread_cond_destroy(&condition->m_NativeHandle);
        delete condition;
        assert(ret == 0);
    }

    void Wait(HConditionVariable condition, dmMutex::HMutex mutex)
    {
        assert(condition);
        int ret = pthread_cond_wait(&condition->m_NativeHandle, (pthread_mutex_t*)dmMutex::GetNativeHandle(mutex));
        assert(ret == 0);
    }

    bool WaitTimeout(HConditionVariable condition, dmMutex::HMutex mutex, uint32_t timeout_ms)
    {
        struct timespec deadline;
#if defined(__MACH__)
        deadline.tv_sec = timeout_ms / 1000;
        deadline.tv_nsec = (timeout_ms % 1000) * 1000000L;
        int ret = pthread_cond_timedwait_relative_np(&condition->m_NativeHandle,
                                                     (pthread_mutex_t*)dmMutex::GetNativeHandle(mutex), &deadline);
#else
        clock_gettime(CLOCK_MONOTONIC, &deadline);
        deadline.tv_sec += timeout_ms / 1000;
        deadline.tv_nsec += (timeout_ms % 1000) * 1000000L;
        deadline.tv_sec += deadline.tv_nsec / 1000000000L;
        deadline.tv_nsec %= 1000000000L;
        int ret = pthread_cond_timedwait(&condition->m_NativeHandle,
                                         (pthread_mutex_t*)dmMutex::GetNativeHandle(mutex), &deadline);
#endif
        assert(ret == 0 || ret == ETIMEDOUT);
        return ret == 0;
    }

    void Signal(HConditionVariable condition)
    {
        assert(condition);
        int ret = pthread_cond_signal(&condition->m_NativeHandle);
        assert(ret == 0);
    }

    void Broadcast(HConditionVariable condition)
    {
        assert(condition);
        int ret = pthread_cond_broadcast(&condition->m_NativeHandle);
        assert(ret == 0);
    }
} // namespace dmConditionVariable
