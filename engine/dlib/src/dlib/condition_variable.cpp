#include "condition_variable.h"
#include <assert.h>

namespace dmConditionVariable
{
#if defined(__linux__) || defined(__MACH__)

    ConditionVariable New()
    {
        ConditionVariable condition = new pthread_cond_t;
        int ret = pthread_cond_init(condition, 0);
        assert(ret == 0);
        return condition;
    }

    void Delete(ConditionVariable condition)
    {
        int ret = pthread_cond_destroy(condition);
        delete condition;
        assert(ret == 0);
    }

    void Wait(ConditionVariable condition, dmMutex::Mutex mutex)
    {
        int ret = pthread_cond_wait(condition, mutex);
        assert(ret == 0);
    }

    void Signal(ConditionVariable condition)
    {
        int ret = pthread_cond_signal(condition);
        assert(ret == 0);
    }

    void Broadcast(ConditionVariable condition)
    {
        int ret = pthread_cond_broadcast(condition);
        assert(ret == 0);
    }

#elif defined(_WIN32)

    ConditionVariable New()
    {
        ConditionVariable condition = new CONDITION_VARIABLE;
        InitializeConditionVariable(condition);
        return condition;
    }

    void Delete(ConditionVariable condition)
    {
        delete condition;
    }

    void Wait(ConditionVariable condition, dmMutex::Mutex mutex)
    {
        BOOL ret = SleepConditionVariableCS(condition, mutex, INFINITE);
        assert(ret);
    }

    void Signal(ConditionVariable condition)
    {
        WakeConditionVariable(condition);
    }

    void Broadcast(ConditionVariable condition)
    {
        WakeAllConditionVariable(condition);
    }

#elif defined(__EMSCRIPTEN__)

    ConditionVariable New()
    {
        ConditionVariable condition = new pthread_cond_t;
        int ret = pthread_cond_init(condition, 0);
        assert(ret == 0);
        return condition;
    }

    void Delete(ConditionVariable condition)
    {
        int ret = pthread_cond_destroy(condition);
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
