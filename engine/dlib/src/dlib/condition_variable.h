#ifndef DM_CONDITION_VARIABLE_H
#define DM_CONDITION_VARIABLE_H

#include "mutex.h"

#if defined(__linux__) || defined(__MACH__) || defined(__EMSCRIPTEN__)
#include <pthread.h>
namespace dmConditionVariable
{
    typedef pthread_cond_t* ConditionVariable;
}

#elif defined(_WIN32)
#include "safe_windows.h"
namespace dmConditionVariable
{
    typedef CONDITION_VARIABLE* ConditionVariable;
}

#else
#error "Unsupported platform"
#endif

namespace dmConditionVariable
{
    /**
     * Create a new condition variable
     * @return
     */
    ConditionVariable New();

    /**
     * Delete condition variable
     * @param condition
     */
    void Delete(ConditionVariable condition);

    /**
     * Wait for condition variable
     * @param condition
     * @param mutex
     */
    void Wait(ConditionVariable condition, dmMutex::Mutex mutex);

    /**
     * Signal condition variable
     * @param condition
     */
    void Signal(ConditionVariable condition);

    /**
     * Broadcast condition variable
     * @param condition
     */
    void Broadcast(ConditionVariable condition);
}

#endif // #ifndef DM_CONDITION_VARIABLE_H
