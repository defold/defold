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
#include "condition_variable.h"
#include "mutex.h"

#if defined(__linux__) || defined(__MACH__) || defined(__EMSCRIPTEN__)
#include <pthread.h>
#elif defined(_WIN32)
#include "safe_windows.h"
#else
#error "Unsupported platform"
#endif

namespace dmConditionVariable
{

    struct ConditionVariable
    {
#if defined(_WIN32)
        CONDITION_VARIABLE m_NativeHandle;
#else
        pthread_cond_t     m_NativeHandle;
#endif
    };

#if defined(__linux__) || defined(__MACH__)

    HConditionVariable New()
    {
        ConditionVariable* condition = new ConditionVariable();
        int ret = pthread_cond_init(&condition->m_NativeHandle, 0);
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
        int ret = pthread_cond_wait(&condition->m_NativeHandle, &mutex->m_NativeHandle);
        assert(ret == 0);
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

#elif defined(_WIN32)

    HConditionVariable New()
    {
        ConditionVariable* condition = new ConditionVariable();
        InitializeConditionVariable(&condition->m_NativeHandle);
        return condition;
    }

    void Delete(HConditionVariable condition)
    {
        assert(condition);
        delete condition;
    }

    void Wait(HConditionVariable condition, dmMutex::HMutex mutex)
    {
        assert(condition);
        BOOL ret = SleepConditionVariableCS(&condition->m_NativeHandle, &mutex->m_NativeHandle, INFINITE);
        assert(ret);
    }

    void Signal(HConditionVariable condition)
    {
        assert(condition);
        WakeConditionVariable(&condition->m_NativeHandle);
    }

    void Broadcast(HConditionVariable condition)
    {
        assert(condition);
        WakeAllConditionVariable(&condition->m_NativeHandle);
    }

#elif defined(__EMSCRIPTEN__)

    HConditionVariable New()
    {
        ConditionVariable* condition = new ConditionVariable();
        int ret = pthread_cond_init(&condition->m_NativeHandle, 0);
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
        // dmLog uses dmMessage, which in turn uses dmConditionVariable (Wait & Signal).
        // We cannot place assertions here.
    #if !defined(DM_NO_THREAD_SUPPORT)
        pthread_cond_wait(&condition->m_NativeHandle, &mutex->m_NativeHandle);
    #endif
    }

    void Signal(HConditionVariable condition)
    {
        assert(condition);
    #if !defined(DM_NO_THREAD_SUPPORT)
        int ret = pthread_cond_signal(&condition->m_NativeHandle);
        assert(ret == 0);
    #endif
    }

    void Broadcast(HConditionVariable condition)
    {
        assert(false);
    }
#else
#error "Unsupported platform"
#endif

}

