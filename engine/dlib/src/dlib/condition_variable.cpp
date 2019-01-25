#include <assert.h>
#include "condition_variable.h"

#if defined(__linux__) || defined(__MACH__) || defined(__EMSCRIPTEN__)
#include <pthread.h>
#elif defined(_WIN32)
#include "safe_windows.h"
#else
#error "Unsupported platform"
#endif

namespace dmConditionVariable
{

    struct OpaqueConditionalVariable
    {
#if defined(__linux__) || defined(__MACH__) || defined(__EMSCRIPTEN__)
        pthread_cond_t*     m_NativeHandle;
#elif defined(_WIN32)
        CONDITION_VARIABLE* m_NativeHandle;
#endif
    };

#if defined(__linux__) || defined(__MACH__)

    ConditionVariable New()
    {
        OpaqueConditionalVariable* condition = new OpaqueConditionalVariable();
        condition->m_NativeHandle = new pthread_cond_t;
        int ret = pthread_cond_init(condition->m_NativeHandle, 0);
        assert(ret == 0);
        return condition;
    }

    void Delete(ConditionVariable condition)
    {
        assert(condition);
        int ret = pthread_cond_destroy(condition->m_NativeHandle);
        delete condition->m_NativeHandle;
        delete condition;
        assert(ret == 0);
    }

    void Wait(ConditionVariable condition, dmMutex::Mutex mutex)
    {
        assert(condition);
        int ret = pthread_cond_wait(condition->m_NativeHandle, mutex->m_NativeHandle);
        assert(ret == 0);
    }

    void Signal(ConditionVariable condition)
    {
        assert(condition);
        int ret = pthread_cond_signal(condition->m_NativeHandle);
        assert(ret == 0);
    }

    void Broadcast(ConditionVariable condition)
    {
        assert(condition);
        int ret = pthread_cond_broadcast(condition->m_NativeHandle);
        assert(ret == 0);
    }

#elif defined(_WIN32)

    ConditionVariable New()
    {
        OpaqueConditionalVariable* condition = new OpaqueConditionalVariable();
        condition->m_NativeHandle = new CONDITION_VARIABLE;
        InitializeConditionVariable(condition->m_NativeHandle);
        return condition;
    }

    void Delete(ConditionVariable condition)
    {
        assert(condition);
        delete condition->m_NativeHandle;
        delete condition;
    }

    void Wait(ConditionVariable condition, dmMutex::Mutex mutex)
    {
        assert(condition);
        BOOL ret = SleepConditionVariableCS(condition->m_NativeHandle, mutex->m_NativeHandle, INFINITE);
        assert(ret);
    }

    void Signal(ConditionVariable condition)
    {
        assert(condition);
        WakeConditionVariable(condition->m_NativeHandle);
    }

    void Broadcast(ConditionVariable condition)
    {
        assert(condition);
        WakeAllConditionVariable(condition->m_NativeHandle);
    }

#elif defined(__EMSCRIPTEN__)

    ConditionVariable New()
    {
        OpaqueConditionalVariable* condition = new OpaqueConditionalVariable();
        condition->m_NativeHandle = new pthread_cond_t;
        int ret = pthread_cond_init(condition->m_NativeHandle, 0);
        assert(ret == 0);
        return condition;
    }

    void Delete(ConditionVariable condition)
    {
        assert(condition);
        int ret = pthread_cond_destroy(condition->m_NativeHandle);
        delete condition->m_NativeHandle;
        delete condition;
        assert(ret == 0);
    }

    void Wait(ConditionVariable condition, dmMutex::Mutex mutex)
    {
        // dmLog uses dmMessage, which in turn uses dmConditionVariable (Wait & Signal).
        // We cannot place assertions here.
    }

    void Signal(ConditionVariable condition)
    {

    }

    void Broadcast(ConditionVariable condition)
    {
        assert(false);
    }
#else
#error "Unsupported platform"
#endif

}
