#ifndef DMSDK_CONDITION_VARIABLE_H
#define DMSDK_CONDITION_VARIABLE_H

#include <dmsdk/dlib/mutex.h>

namespace dmConditionVariable
{
    typedef struct OpaqueConditionalVariable* ConditionVariable;

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

#endif // #ifndef DMSDK_CONDITION_VARIABLE_H
