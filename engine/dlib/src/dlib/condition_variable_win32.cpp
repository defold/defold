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

#if !defined(_WIN32)
#error "Unsupported platform"
#endif

#include <assert.h>
#include "condition_variable.h"
#include "mutex.h"
#include "safe_windows.h"

namespace dmConditionVariable
{

    struct ConditionVariable
    {
        CONDITION_VARIABLE m_NativeHandle;
    };

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
        BOOL ret = SleepConditionVariableCS(&condition->m_NativeHandle, (CRITICAL_SECTION*)dmMutex::GetNativeHandle(mutex), INFINITE);
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
} // namespace dmConditionVariable
