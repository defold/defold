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

namespace dmMutex
{
    HMutex New()
    {
        Mutex* mutex = new Mutex();
        InitializeCriticalSection(&mutex->m_NativeHandle);
        return mutex;
    }

    void Delete(HMutex mutex)
    {
        assert(mutex);
        DeleteCriticalSection(&mutex->m_NativeHandle);
        delete mutex;
    }

    void Lock(HMutex mutex)
    {
        assert(mutex);
        EnterCriticalSection(&mutex->m_NativeHandle);
    }

    bool TryLock(HMutex mutex)
    {
        assert(mutex);
        return (TryEnterCriticalSection(&mutex->m_NativeHandle)) == 0 ? false : true;
    }

    void Unlock(HMutex mutex)
    {
        assert(mutex);
        LeaveCriticalSection(&mutex->m_NativeHandle);
    }
}
