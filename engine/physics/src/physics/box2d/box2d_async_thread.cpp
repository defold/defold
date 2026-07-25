// Copyright 2020-2026 The Defold Foundation
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

#include "box2d_async_thread.h"

#include <assert.h>
#include <stdlib.h>
#include <dlib/thread.h>
#include <dlib/mutex.h>
#include <dlib/condition_variable.h>

// Threads are available everywhere except an Emscripten build without pthreads.
#if defined(__EMSCRIPTEN__) && !defined(__EMSCRIPTEN_PTHREADS__)
    #define DM_BOX2D_ASYNC_NO_THREADS 1
#endif

namespace dmPhysics
{
    static const uint32_t ASYNC_WORKER_STACK_SIZE = 0x100000; // 1 MiB; the physics step runs here.

#if defined(DM_BOX2D_ASYNC_NO_THREADS)

    // Inline fallback: no background thread. Submit runs the job immediately and Wait is a no-op.
    struct AsyncWorker
    {
        uint8_t m_Unused;
    };

    AsyncWorker* NewAsyncWorker(const char* name)
    {
        (void)name;
        return new AsyncWorker();
    }

    void DeleteAsyncWorker(AsyncWorker* worker)
    {
        delete worker;
    }

    void AsyncWorkerSubmit(AsyncWorker* worker, AsyncWorkerJob job, void* context)
    {
        (void)worker;
        job(context);
    }

    void AsyncWorkerWait(AsyncWorker* worker)
    {
        (void)worker;
    }

    bool AsyncWorkerIsIdle(AsyncWorker* worker)
    {
        (void)worker;
        return true;
    }

#else

    struct AsyncWorker
    {
        dmThread::Thread                        m_Thread;
        dmMutex::HMutex                         m_Mutex;
        dmConditionVariable::HConditionVariable m_WorkCond; // signalled when a job is submitted or on shutdown
        dmConditionVariable::HConditionVariable m_DoneCond; // signalled when a job completes
        AsyncWorkerJob                          m_Job;
        void*                                   m_Context;
        bool                                    m_HasJob;   // a job is submitted and not finished
        bool                                    m_Shutdown;
    };

    static void AsyncWorkerThread(void* arg)
    {
        AsyncWorker* worker = (AsyncWorker*)arg;
        while (true)
        {
            AsyncWorkerJob job;
            void*          context;
            {
                dmMutex::ScopedLock lk(worker->m_Mutex);
                while (!worker->m_HasJob && !worker->m_Shutdown)
                {
                    dmConditionVariable::Wait(worker->m_WorkCond, worker->m_Mutex);
                }
                if (worker->m_Shutdown && !worker->m_HasJob)
                {
                    return;
                }
                job     = worker->m_Job;
                context = worker->m_Context;
            }

            // Run the job outside the lock so the completion state is not held while it works.
            job(context);

            {
                dmMutex::ScopedLock lk(worker->m_Mutex);
                worker->m_HasJob = false;
                dmConditionVariable::Broadcast(worker->m_DoneCond);
            }
        }
    }

    AsyncWorker* NewAsyncWorker(const char* name)
    {
        AsyncWorker* worker = new AsyncWorker();
        worker->m_Mutex     = dmMutex::New();
        worker->m_WorkCond  = dmConditionVariable::New();
        worker->m_DoneCond  = dmConditionVariable::New();
        worker->m_Job       = 0;
        worker->m_Context   = 0;
        worker->m_HasJob    = false;
        worker->m_Shutdown  = false;
        worker->m_Thread    = dmThread::New(AsyncWorkerThread, ASYNC_WORKER_STACK_SIZE, worker, name ? name : "physics_async");
        return worker;
    }

    void DeleteAsyncWorker(AsyncWorker* worker)
    {
        AsyncWorkerWait(worker);
        {
            dmMutex::ScopedLock lk(worker->m_Mutex);
            worker->m_Shutdown = true;
            dmConditionVariable::Signal(worker->m_WorkCond);
        }
        dmThread::Join(worker->m_Thread);
        dmConditionVariable::Delete(worker->m_DoneCond);
        dmConditionVariable::Delete(worker->m_WorkCond);
        dmMutex::Delete(worker->m_Mutex);
        delete worker;
    }

    void AsyncWorkerSubmit(AsyncWorker* worker, AsyncWorkerJob job, void* context)
    {
        dmMutex::ScopedLock lk(worker->m_Mutex);
        assert(!worker->m_HasJob); // one job in flight at a time; callers wait before submitting
        worker->m_Job     = job;
        worker->m_Context = context;
        worker->m_HasJob  = true;
        dmConditionVariable::Signal(worker->m_WorkCond);
    }

    void AsyncWorkerWait(AsyncWorker* worker)
    {
        dmMutex::ScopedLock lk(worker->m_Mutex);
        while (worker->m_HasJob)
        {
            dmConditionVariable::Wait(worker->m_DoneCond, worker->m_Mutex);
        }
    }

    bool AsyncWorkerIsIdle(AsyncWorker* worker)
    {
        dmMutex::ScopedLock lk(worker->m_Mutex);
        return !worker->m_HasJob;
    }

#endif // DM_BOX2D_ASYNC_NO_THREADS
}
