
#if !defined(__EMSCRIPTEN__)
    #define DM_HAS_THREADS
#endif

#include <dlib/atomic.h>
#include <dmsdk/dlib/array.h>
#include <dmsdk/dlib/profile.h>
#include <dmsdk/dlib/log.h>

#if defined(DM_HAS_THREADS)
    #include <dmsdk/dlib/condition_variable.h>
    #include <dmsdk/dlib/mutex.h>
    #include <dmsdk/dlib/thread.h>
#endif

#include "job_thread.h"


namespace dmJobThread
{
    struct JobItem
    {
        FJobItemProcess     m_Process;
        FJobItemCallback    m_Callback;
        void*               m_Context;
        void*               m_Data;
        int                 m_Result;
    };

    struct JobThreadContext
    {
        dmMutex::HMutex                         m_Mutex;
        dmConditionVariable::HConditionVariable m_WakeupCond;
        dmArray<JobItem>                        m_Work;        // On the thread
        dmArray<JobItem>                        m_Finished;    // Moved from the thread
        int32_atomic_t                          m_Run;
    };

    struct JobContext
    {
        dmThread::Thread m_Thread;
        JobThreadContext m_ThreadContext;
    };

#if defined(DM_HAS_THREADS)
    static void JobThread(void* _ctx);
#else
    static void UpdateSingleThread(JobThreadContext* ctx);
#endif

    HJobThread New(const char* name)
    {
        JobContext* ctx = new JobContext;

#if defined(DM_HAS_THREADS)
        ctx->m_ThreadContext.m_Mutex = dmMutex::New();
        ctx->m_ThreadContext.m_WakeupCond = dmConditionVariable::New();
        ctx->m_ThreadContext.m_Run = 1;
        ctx->m_Thread = dmThread::New(JobThread, 0x80000, (void*)&ctx->m_ThreadContext, name);
#endif
        return ctx;
    }

    void Join(HJobThread _ctx)
    {
        JobContext* ctx = (JobContext*)_ctx;

#if defined(DM_HAS_THREADS)
        {
            DM_MUTEX_SCOPED_LOCK(ctx->m_ThreadContext.m_Mutex);
            dmAtomicStore32(&ctx->m_ThreadContext.m_Run, 0);
            // Wake up the worker so it can exit and allow us to join
            dmConditionVariable::Signal(ctx->m_ThreadContext.m_WakeupCond);
        }

        dmThread::Join(ctx->m_Thread);
        dmConditionVariable::Delete(ctx->m_ThreadContext.m_WakeupCond);
        dmMutex::Delete(ctx->m_ThreadContext.m_Mutex);
#endif
        delete ctx;
    }

    static void FlushFinished(JobThreadContext* ctx)
    {
        dmArray<JobItem> items;
        {
#if defined(DM_HAS_THREADS)
            // Lock for as little as possible, by swapping the arrays
            DM_MUTEX_SCOPED_LOCK(ctx->m_Mutex);
#endif
            items.Swap(ctx->m_Finished);
        }

        uint32_t size = items.Size();
        for(uint32_t i = 0; i < size; ++i)
        {
            JobItem& item = items[i];
            item.m_Callback(item.m_Context, item.m_Data, item.m_Result);
        }
    }

#if !defined(DM_HAS_THREADS)
    static void UpdateSingleThread(JobThreadContext* ctx)
    {
        JobItem item = ctx->m_Work.Back();
        ctx->m_Work.Pop();

        item.m_Result = item.m_Process(item.m_Context, item.m_Data);

        if (ctx->m_Finished.Full())
            ctx->m_Finished.OffsetCapacity(8);
        ctx->m_Finished.Push(item);
    }
#endif

    // Flushes any finished job items
    void Update(HJobThread _ctx)
    {
        JobContext* ctx = (JobContext*)_ctx;
        JobThreadContext* threadctx = &ctx->m_ThreadContext;
#if !defined(DM_HAS_THREADS)
        UpdateSingleThread(threadctx);
#endif
        FlushFinished(threadctx);
    }

    bool PushJob(HJobThread _ctx, FJobItemProcess process, FJobItemCallback callback, void* jobctx, void* jobdata)
    {
        JobContext* ctx = (JobContext*)_ctx;

        JobItem item;
        item.m_Context = jobctx;
        item.m_Data = jobdata;
        item.m_Process = process;
        item.m_Callback = callback;
        item.m_Result = 0;

#if defined(DM_HAS_THREADS)
        DM_MUTEX_SCOPED_LOCK(ctx->m_ThreadContext.m_Mutex);
        if (!dmAtomicGet32(&ctx->m_ThreadContext.m_Run))
            return false;
#endif

        if (ctx->m_ThreadContext.m_Work.Full())
            ctx->m_ThreadContext.m_Work.OffsetCapacity(8);
        ctx->m_ThreadContext.m_Work.Push(item);

        return true;
    }

#if defined(DM_HAS_THREADS)
    static void JobThread(void* _ctx)
    {
        JobThreadContext* ctx = (JobThreadContext*)_ctx;
        while (dmAtomicGet32(&ctx->m_Run))
        {
            DM_PROFILE("Update");

            JobItem item;

            {
                DM_MUTEX_SCOPED_LOCK(ctx->m_Mutex);

                if(ctx->m_Work.Empty())
                {
                    dmConditionVariable::Wait(ctx->m_WakeupCond, ctx->m_Mutex);
                    continue;
                }

                item = ctx->m_Work.Back();
                ctx->m_Work.Pop();
            }

            item.m_Result = item.m_Process(item.m_Context, item.m_Data);

            {
                DM_MUTEX_SCOPED_LOCK(ctx->m_Mutex);
                if (ctx->m_Finished.Full())
                    ctx->m_Finished.OffsetCapacity(8);
                ctx->m_Finished.Push(item);
            }
        }
    }
#endif
}
