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

#include <dlib/thread.h>

#include "box2d_physics.h"

#if defined(DM_HAS_THREADS)
#include <box2d/constants.h>
#include <dlib/atomic.h>
#include <dlib/condition_variable.h>
#include <dlib/dstrings.h>
#include <dlib/mutex.h>
#include <dlib/time.h>

namespace dmPhysics
{
    static const uint64_t WORKER_SPIN_MICROSECONDS = 50;

    enum TaskStatus
    {
        TASK_COMPLETE,
        TASK_PENDING,
        TASK_CLAIMED,
    };

    struct Box2DTask
    {
        b2TaskCallbackFcn* m_Callback;
        void*              m_Context;
        int32_atomic_t     m_Status;
    };

    struct Box2DTaskContext
    {
        dmThread::Thread                        m_Workers[B2_MAX_WORKERS - 1];
        Box2DTask                               m_Tasks[B2_MAX_TASKS];
        dmMutex::HMutex                         m_Mutex;
        dmConditionVariable::HConditionVariable m_Work;
        dmConditionVariable::HConditionVariable m_Completion;
        int32_atomic_t                          m_TaskCount;
        int32_atomic_t                          m_Sleepers;
        int32_atomic_t                          m_Waiting;
        int32_atomic_t                          m_Stop;
        uint32_t                                m_WorkerCount;
    };

    static bool HasPending(Box2DTaskContext* context)
    {
        int32_t count = dmAtomicLoadAcquire32(&context->m_TaskCount);
        for (int32_t i = 0; i < count; ++i)
        {
            if (dmAtomicLoadAcquire32(&context->m_Tasks[i].m_Status) == TASK_PENDING)
            {
                return true;
            }
        }
        return false;
    }

    static bool ExecuteOne(Box2DTaskContext* context)
    {
        int32_t count = dmAtomicLoadAcquire32(&context->m_TaskCount);
        for (int32_t i = 0; i < count; ++i)
        {
            Box2DTask* task = &context->m_Tasks[i];
            if (dmAtomicLoadAcquire32(&task->m_Status) != TASK_PENDING ||
                dmAtomicCompareStore32(&task->m_Status, TASK_CLAIMED, TASK_PENDING) != TASK_PENDING)
            {
                continue;
            }
            task->m_Callback(task->m_Context);
            // Publish the task's writes before Finish can return.
            dmAtomicStoreRelease32(&task->m_Status, TASK_COMPLETE);
            // The full barrier pairs with waiter registration to prevent a missed completion.
            if (dmAtomicGet32(&context->m_Waiting))
            {
                dmMutex::Lock(context->m_Mutex);
                dmConditionVariable::Signal(context->m_Completion);
                dmMutex::Unlock(context->m_Mutex);
            }
            return true;
        }
        return false;
    }

    static void Worker(void* user_context)
    {
        Box2DTaskContext* context = (Box2DTaskContext*)user_context;
        while (!dmAtomicLoadAcquire32(&context->m_Stop))
        {
            while (ExecuteOne(context))
            {
            }
            uint64_t start = dmTime::GetMonotonicTime();
            while (!HasPending(context) && !dmAtomicLoadAcquire32(&context->m_Stop) &&
                   dmTime::GetMonotonicTime() - start < WORKER_SPIN_MICROSECONDS)
            {
            }
            if (HasPending(context))
            {
                continue;
            }
            // Checking the predicate under this mutex prevents a missed wakeup.
            dmMutex::Lock(context->m_Mutex);
            dmAtomicIncrement32(&context->m_Sleepers);
            while (!HasPending(context) && !dmAtomicLoadAcquire32(&context->m_Stop))
            {
                dmConditionVariable::Wait(context->m_Work, context->m_Mutex);
            }
            dmAtomicDecrement32(&context->m_Sleepers);
            dmMutex::Unlock(context->m_Mutex);
        }
    }

    Box2DTaskContext* NewBox2DTaskContext(uint32_t worker_count)
    {
        assert(worker_count > 0 && worker_count < B2_MAX_WORKERS);
        // Box2D profiling uses a lazy timer even with an external task system.
        b2GetMilliseconds(b2GetTicks());
        Box2DTaskContext* context = new Box2DTaskContext();
        context->m_Mutex = dmMutex::New();
        context->m_Work = dmConditionVariable::New();
        context->m_Completion = dmConditionVariable::New();
        context->m_WorkerCount = worker_count;
        for (uint32_t i = 0; i < worker_count; ++i)
        {
            char name[16];
            dmSnPrintf(name, sizeof(name), "box2d_%u", i);
            context->m_Workers[i] = dmThread::New(Worker, 0x80000, context, name);
        }
        return context;
    }

    void DeleteBox2DTaskContext(Box2DTaskContext* context)
    {
        dmMutex::Lock(context->m_Mutex);
        dmAtomicIncrement32(&context->m_Stop);
        dmConditionVariable::Broadcast(context->m_Work);
        dmMutex::Unlock(context->m_Mutex);
        for (uint32_t i = 0; i < context->m_WorkerCount; ++i)
        {
            dmThread::Join(context->m_Workers[i]);
#if defined(_WIN32)
            // Win32 Join waits for termination; Detach closes the thread handle.
            dmThread::Detach(context->m_Workers[i]);
#endif
        }
        dmConditionVariable::Delete(context->m_Work);
        dmConditionVariable::Delete(context->m_Completion);
        dmMutex::Delete(context->m_Mutex);
        delete context;
    }

    void ResetBox2DTaskContext(Box2DTaskContext* context)
    {
        dmAtomicStoreRelease32(&context->m_TaskCount, 0);
    }

    void* EnqueueBox2DTask(b2TaskCallbackFcn* callback, void* task_context, void* user_context)
    {
        Box2DTaskContext* context = (Box2DTaskContext*)user_context;
        int32_t slot = dmAtomicIncrement32(&context->m_TaskCount);
        assert(slot < B2_MAX_TASKS);
        Box2DTask* task = &context->m_Tasks[slot];
        task->m_Callback = callback;
        task->m_Context = task_context;
        // Publish callback and context together; workers only read claimed tasks.
        dmAtomicStoreRelease32(&task->m_Status, TASK_PENDING);
        // Order task publication before checking the registered sleepers.
        if (dmAtomicGet32(&context->m_Sleepers))
        {
            dmMutex::Lock(context->m_Mutex);
            dmConditionVariable::Signal(context->m_Work);
            dmMutex::Unlock(context->m_Mutex);
        }
        return task;
    }

    void FinishBox2DTask(void* user_task, void* user_context)
    {
        if (!user_task)
        {
            return;
        }
        Box2DTaskContext* context = (Box2DTaskContext*)user_context;
        Box2DTask* task = (Box2DTask*)user_task;
        uint64_t start = dmTime::GetMonotonicTime();
        while (dmAtomicLoadAcquire32(&task->m_Status) != TASK_COMPLETE)
        {
            if (ExecuteOne(context))
            {
                start = dmTime::GetMonotonicTime();
            }
            else if (dmTime::GetMonotonicTime() - start >= WORKER_SPIN_MICROSECONDS)
            {
                dmMutex::Lock(context->m_Mutex);
                dmAtomicIncrement32(&context->m_Waiting);
                while (dmAtomicLoadAcquire32(&task->m_Status) != TASK_COMPLETE)
                {
                    dmConditionVariable::Wait(context->m_Completion, context->m_Mutex);
                }
                dmAtomicDecrement32(&context->m_Waiting);
                dmMutex::Unlock(context->m_Mutex);
            }
        }
    }
} // namespace dmPhysics
#else
namespace dmPhysics
{
    Box2DTaskContext* NewBox2DTaskContext(uint32_t worker_count)
    {
        (void)worker_count;
        return 0;
    }

    void DeleteBox2DTaskContext(Box2DTaskContext* context)
    {
        (void)context;
    }

    void ResetBox2DTaskContext(Box2DTaskContext* context)
    {
        (void)context;
    }

    void* EnqueueBox2DTask(b2TaskCallbackFcn* callback, void* task_context, void* user_context)
    {
        (void)user_context;
        callback(task_context);
        return 0;
    }

    void FinishBox2DTask(void* task, void* user_context)
    {
        (void)task;
        (void)user_context;
    }
} // namespace dmPhysics
#endif
