#if !defined(__NX__)
#error "Unsupported platform"
#endif

#include <assert.h>
#include <stdlib.h> // malloc
#include <dlib/condition_variable.h>
#include "mutex.h"

#include <nn/os/os_ConditionVariable.h>
#include <nn/os/os_MutexTypes.h>

// https://developer.nintendo.com/html/online-docs/nx-en/g1kr9vj6-en/Packages/SDK/NintendoSDK/Documents/Package/contents/external.html?file=../../Api/HtmlNX/index.html

namespace dmConditionVariable
{
    struct ConditionVariable
    {
        nn::os::ConditionVariableType m_NativeHandle;
    };

    HConditionVariable New()
    {
        ConditionVariable* condition = (ConditionVariable*)malloc(sizeof(ConditionVariable));
        nn::os::InitializeConditionVariable(&condition->m_NativeHandle);
        return condition;
    }

    void Delete(HConditionVariable condition)
    {
        assert(condition);
        nn::os::FinalizeConditionVariable(&condition->m_NativeHandle);
        free(condition);
    }

    void Wait(HConditionVariable condition, dmMutex::HMutex mutex)
    {
        assert(condition);
        nn::os::WaitConditionVariable(&condition->m_NativeHandle, &mutex->m_NativeHandle);
    }

    void Signal(HConditionVariable condition)
    {
        assert(condition);
        nn::os::SignalConditionVariable(&condition->m_NativeHandle);
    }

    void Broadcast(HConditionVariable condition)
    {
        assert(condition);
        nn::os::BroadcastConditionVariable(&condition->m_NativeHandle);
    }
}
