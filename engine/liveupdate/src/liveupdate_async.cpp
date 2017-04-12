#include "liveupdate.h"
#include "liveupdate_private.h"

#include <resource/resource.h>
#include <resource/resource_archive.h>
#include <dlib/log.h>
#include <dlib/thread.h>
#include <dlib/mutex.h>
#include <dlib/condition_variable.h>
#include <axtls/crypto/crypto.h>
#include <dlib/array.h>


namespace dmLiveUpdate
{
    /// Resource system factory, used for async locking of load mutex
    static dmResource::HFactory m_ResourceFactory = 0x0;
    /// How many elements to offset queue capacity with when full
    static const uint32_t m_JobQueueSizeIncrement = 32;

    /// The liveupdate thread and synchronization objects, used for sequentially processing async liveupdate resource requests
    static dmThread::Thread m_AsyncThread = 0x0;
    static dmMutex::Mutex  m_ConsumerThreadMutex;
    static dmConditionVariable::ConditionVariable m_ConsumerThreadCondition;
    static volatile bool m_ThreadJobComplete = false;
    static volatile bool m_Active = false;

    /// job input and output queues
    static dmArray<AsyncResourceRequest> m_JobQueue;
    static dmArray<AsyncResourceRequest> m_ThreadJobQueue;
    static AsyncResourceRequest m_TreadJob;
    static ResourceRequestCallbackData m_JobCompleteData;

    void ProcessRequest(AsyncResourceRequest &request)
    {
        Result res = dmLiveUpdate::NewArchiveIndexWithResource(request.m_Manifest, request.m_ExpectedResourceDigest, request.m_ExpectedResourceDigestLength, &request.m_Resource, m_JobCompleteData.m_NewArchiveIndex);
        m_JobCompleteData.m_CallbackData = request.m_CallbackData;
        m_JobCompleteData.m_Callback = request.m_Callback;
        m_JobCompleteData.m_ArchiveIndexContainer = request.m_Manifest->m_ArchiveIndex;
        m_JobCompleteData.m_CallbackData.m_Status = res == dmLiveUpdate::RESULT_OK ? true : false;
    }

    void ProcessRequestComplete()
    {
        if(m_JobCompleteData.m_CallbackData.m_Status)
        {
            dmLiveUpdate::SetNewArchiveIndex(m_JobCompleteData.m_ArchiveIndexContainer, m_JobCompleteData.m_NewArchiveIndex, true);
        }
        m_JobCompleteData.m_Callback(&m_JobCompleteData.m_CallbackData);
    }


#if !(defined(__EMSCRIPTEN__))
    static void AsyncThread(void* args)
    {
        // Liveupdate async thread batch processing requested liveupdate tasks
        AsyncResourceRequest request;
        while (m_Active)
        {
            // Lock and sleep until signaled there is requests queued up
            {
                dmMutex::ScopedLock lk(m_ConsumerThreadMutex);
                while(m_ThreadJobQueue.Empty())
                    dmConditionVariable::Wait(m_ConsumerThreadCondition, m_ConsumerThreadMutex);
                if((m_ThreadJobComplete) || (!m_Active))
                    continue;
                request = m_ThreadJobQueue.Back();
                m_ThreadJobQueue.Pop();
            }
            ProcessRequest(request);
            m_ThreadJobComplete = true;
        }
    }

    void AsyncUpdate()
    {
        if(m_Active && ((m_ThreadJobComplete) || (!m_JobQueue.Empty())))
        {
            // Process any completed jobs, lock resource loadmutex as we will swap (update) archive containers archiveindex data
            dmMutex::ScopedLock lk(m_ConsumerThreadMutex);
            if(m_ThreadJobComplete)
            {
                dmMutex::Mutex mutex = dmResource::GetLoadMutex(m_ResourceFactory);
                if(!dmMutex::TryLock(mutex))
                    return;
                ProcessRequestComplete();
                dmMutex::Unlock(mutex);
                m_ThreadJobComplete = false;
            }
            // Push any accumulated request queue batch to job queue
            if(!m_JobQueue.Empty())
            {
                if(m_ThreadJobQueue.Remaining() < m_JobQueue.Size())
                {
                    m_ThreadJobQueue.SetCapacity(m_ThreadJobQueue.Size() + m_JobQueue.Size() + m_JobQueueSizeIncrement);
                }
                m_ThreadJobQueue.PushArray(m_JobQueue.Begin(), m_JobQueue.Size());
                m_JobQueue.SetSize(0);
            }
            // Either conditions should signal worker thread to check for new jobs
            dmConditionVariable::Signal(m_ConsumerThreadCondition);
        }
    }

    bool AddAsyncResourceRequest(AsyncResourceRequest& request)
    {
        // Add single job to job queue that will be batch pushed to thread worker in AsyncUpdate
        if(!m_Active)
            return false;
        if(m_JobQueue.Full())
        {
            m_JobQueue.OffsetCapacity(m_JobQueueSizeIncrement);
        }
        m_JobQueue.Push(request);
        return true;
    }

    void AsyncInitialize(const dmResource::HFactory factory)
    {
        m_ResourceFactory = factory;
        m_JobQueue.SetCapacity(m_JobQueueSizeIncrement);
        m_JobQueue.SetSize(0);
        m_ThreadJobQueue.SetCapacity(m_JobQueueSizeIncrement);
        m_ThreadJobQueue.SetSize(0);
        m_ConsumerThreadMutex = dmMutex::New();
        m_ConsumerThreadCondition = dmConditionVariable::New();
        m_ThreadJobComplete = false;
        m_Active = true;
        m_AsyncThread = dmThread::New(AsyncThread, 0x80000, 0, "liveupdate");
    }

    void AsyncFinalize()
    {
        if(m_AsyncThread)
        {
            // When shutting down, discard any complete jobs as they are going to be mounted at boot time in any case
            m_Active = false;
            dmMutex::Lock(m_ConsumerThreadMutex);
            m_JobQueue.SetSize(0);
            m_ThreadJobQueue.SetSize(1);
            dmConditionVariable::Signal(m_ConsumerThreadCondition);
            dmMutex::Unlock(m_ConsumerThreadMutex);
            dmThread::Join(m_AsyncThread);
            dmConditionVariable::Delete(m_ConsumerThreadCondition);
            dmMutex::Delete(m_ConsumerThreadMutex);
            m_AsyncThread = 0;
        }
    }

#else

    void AsyncUpdate()
    {
        if(!m_JobQueue.Empty())
        {
            ProcessRequest(m_JobQueue.Back());
            m_JobQueue.Pop();
            ProcessRequestComplete();
        }
    }

    bool AddAsyncResourceRequest(AsyncResourceRequest& request)
    {
        // Add single job to job queue that will be batch pushed to thread worker in AsyncUpdate
        if(!m_Active)
            return false;
        if(m_JobQueue.Full())
        {
            m_JobQueue.OffsetCapacity(m_JobQueueSizeIncrement);
        }
        m_JobQueue.Push(request);
        return true;
    }

    void AsyncInitialize(const dmResource::HFactory factory)
    {
        m_ResourceFactory = factory;
        m_Active = true;
    }

    void AsyncFinalize()
    {
        m_Active = false;
        m_JobQueue.SetSize(0);
    }

#endif

};
