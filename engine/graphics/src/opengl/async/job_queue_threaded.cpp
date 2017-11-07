
#include <dlib/log.h>
#include <dlib/thread.h>
#include <dlib/mutex.h>
#include <dlib/condition_variable.h>
#include <dlib/array.h>

#include <graphics/glfw/glfw.h>

#include "job_queue.h"

namespace dmGraphics
{
    /// How many elements to offset queue capacity with when full
    static const uint32_t m_JobQueueSizeIncrement = 32;

    /// The graphics worker thread and synchronization objects, used for sequentially processing async graphics jobs
    static dmThread::Thread m_JobThread = 0x0;
    static dmMutex::Mutex  m_ConsumerThreadMutex;
    static dmConditionVariable::ConditionVariable m_ConsumerThreadCondition;
    static volatile bool m_Active = false;

    /// Job input and output queues
    static dmArray<JobDesc> m_JobQueue;

    static void ProcessJob(const JobDesc &job)
    {
        assert(job.m_Func);
        job.m_Func(job.m_Context);
        if(job.m_FuncComplete)
        {
            job.m_FuncComplete(job.m_Context);
        }
    }

    static void AsyncThread(void* args)
    {
        void* aux_context = glfwAcquireAuxContext();
        JobDesc job;
        while (m_Active)
        {
            // Lock and sleep until signaled there is jobs queued up
            {
                dmMutex::ScopedLock lk(m_ConsumerThreadMutex);
                while(m_JobQueue.Empty())
                    dmConditionVariable::Wait(m_ConsumerThreadCondition, m_ConsumerThreadMutex);
                if(!m_Active)
                    break;
                job = m_JobQueue.Back();
                m_JobQueue.Pop();
            }
            ProcessJob(job);
        }
        glfwUnacquireAuxContext(aux_context);
    }

    void JobQueuePush(const JobDesc& job)
    {
        if(m_JobThread)
        {
            // Add single job to job queue that will be batch pushed to thread worker in AsyncUpdate
            dmMutex::ScopedLock lk(m_ConsumerThreadMutex);
            if(m_JobQueue.Full())
            {
                m_JobQueue.OffsetCapacity(m_JobQueueSizeIncrement);
            }
            m_JobQueue.Push(job);
            dmConditionVariable::Signal(m_ConsumerThreadCondition);
        }
        else
        {
            // If shared context / thread isn't enabled
            ProcessJob(job);
        }
    }

    void JobQueueInitialize()
    {
        dmLogDebug("AsyncInitialize: Initializing auxillary context..");
        assert(m_JobThread == 0x0);
        if(!glfwQueryAuxContext())
        {
            dmLogDebug("AsyncInitialize: Auxillary context unsupported");
            return;
        }
        m_JobQueue.SetCapacity(m_JobQueueSizeIncrement);
        m_JobQueue.SetSize(0);
        m_ConsumerThreadMutex = dmMutex::New();
        m_ConsumerThreadCondition = dmConditionVariable::New();
        m_Active = true;
        m_JobThread = dmThread::New(AsyncThread, 0x80000, 0, "graphicsworker");

        dmLogDebug("AsyncInitialize: Auxillary context enabled");
    }

    void JobQueueFinalize()
    {
        if(m_JobThread)
        {
            // When shutting down, discard any pending jobs
            // Set queue size to 1 for early bail (previous size does not matter, discarding jobs)
            m_Active = false;
            dmMutex::Lock(m_ConsumerThreadMutex);
            m_JobQueue.SetSize(1);
            dmConditionVariable::Signal(m_ConsumerThreadCondition);
            dmMutex::Unlock(m_ConsumerThreadMutex);
            dmThread::Join(m_JobThread);
            dmConditionVariable::Delete(m_ConsumerThreadCondition);
            dmMutex::Delete(m_ConsumerThreadMutex);
            m_JobThread = 0;
        }
    }

    bool JobQueueIsAsync()
    {
        return m_JobThread != 0;
    }

};
