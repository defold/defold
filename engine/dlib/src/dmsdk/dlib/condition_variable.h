#ifndef DMSDK_CONDITION_VARIABLE_H
#define DMSDK_CONDITION_VARIABLE_H

#include <dmsdk/dlib/mutex.h>

namespace dmConditionVariable
{
    /*# SDK Condition Variable API documentation
     * [file:<dmsdk/dlib/condition_variable.h>]
     *
     * API for platform independent mutex synchronization primitive.
     *
     * @document
     * @name Condition Variable
     * @namespace dmConditionVariable
     */

    /*# HConditionVariable type definition
     *
     * ```cpp
     * typedef struct ConditionVariable* HConditionVariable;
     * ```
     *
     * @typedef
     * @name dmConditionVariable::HConditionVariable
     *
     */
    typedef struct ConditionVariable* HConditionVariable;

    /*# create condition variable
     *
     * Create a new HConditionVariable
     *
     * @name dmConditionVariable::New
     * @return condition_variable [type:dmConditionVariable::HConditionVariable] A new ConditionVariable handle.
     */
    HConditionVariable New();

    /*# delete condition variable
     *
     * Deletes a HConditionVariable.
     *
     * @name dmConditionVariable::Delete
     * @param mutex [type:dmConditionVariable::HConditionVariable] ConditionVariable handle to delete.
     */
    void Delete(HConditionVariable condition);

    /*# wait for condition variable
     *
     * Wait for condition variable. This is a blocking function, and should be called with
     * the mutex being locked.
     *
     * @name dmConditionVariable::Wait
     * @param condition [type:dmConditionVariable::HConditionVariable] ConditionVariable handle.
     * @param mutex [type:dmMutex::HMutex] Mutex handle.
     */
    void Wait(HConditionVariable condition, dmMutex::HMutex mutex);

    /*# signal condition variable
     *
     * Signal condition variable, effectively unblocks at least one of the waithing threads blocked
     * by the condition variable.
     *
     * @name dmConditionVariable::Signal
     * @param condition [type:dmConditionVariable::HConditionVariable] ConditionVariable handle.
     */
    void Signal(HConditionVariable condition);

    /**
     * Broadcast condition variable
     * @param condition
     */
    /*# broadcast condition variable
     *
     * Broadcast condition variable, effectively unblocks all of the waithing threads blocked
     * by the condition variable.
     *
     * @name dmConditionVariable::Broadcast
     * @param condition [type:dmConditionVariable::HConditionVariable] ConditionVariable handle.
     */
    void Broadcast(HConditionVariable condition);
}

#endif // #ifndef DMSDK_CONDITION_VARIABLE_H
