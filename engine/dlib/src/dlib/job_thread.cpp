// Copyright 2020-2025 The Defold Foundation
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


#include <stdio.h> // printf

#include <dlib/thread.h> // We want the defines DM_HAS_THREADS

#include <dmsdk/dlib/dstrings.h>
#include <dmsdk/dlib/math.h>
#include <dmsdk/dlib/array.h>
#include <dmsdk/dlib/atomic.h>
#include <dmsdk/dlib/log.h>
#include <dmsdk/dlib/profile.h>
#include <dmsdk/dlib/time.h>
#include <dmsdk/dlib/object_pool.h>

#if defined(DM_HAS_THREADS)
    #include <dmsdk/dlib/condition_variable.h>
    #include <dmsdk/dlib/mutex.h>
#endif

#include "jc/ringbuffer.h"
#include "job_thread.h"

namespace dmJobThread
{

static const uint32_t   INVALID_INDEX   = 0xFFFFFFFF;
static const HJob       INVALID_JOB     = 0;    // we always start at generation=1, so we can never have a 0 handle
static const uint32_t   MAX_JOB_DEPTH   = 32;   // Only really used when traversing the structure recursively

struct JobItem
{
    Job         m_Job;
    uint64_t    m_TimeCreated;  // to help sorting, and avoid starvation
    int32_t     m_Result;       // The result after processing
    uint32_t    m_Generation;   // Used to detect old handles
    HJob        m_Parent;       // We can only have one parent task (no parent == INVALID_JOB)
    HJob        m_Sibling;      // Index to the next sibling (or INVALID_JOB)
    HJob        m_FirstChild;   // Index to the first child (or INVALID_JOB)

    // Run this task only after the children have all completed
    int32_atomic_t  m_NumChildren;
    int32_atomic_t  m_NumChildrenCompleted;

    JobStatus m_Status;

    // TODO: Implement a scheduler
    // int8_t   m_Prio;
};

struct JobThreadContext
{
    dmObjectPool<JobItem>   m_Items;
    jc::RingBuffer<HJob>    m_Work;         // Workload queue (currently fifo)
    jc::RingBuffer<HJob>    m_Done;         // Processed tasks, ready for callbacks
    uint32_t                m_Generation;

#if defined(DM_HAS_THREADS)
    dmMutex::HMutex                         m_Mutex;
    dmConditionVariable::HConditionVariable m_WakeupCond;
    bool                                    m_Run;
#endif
};

struct JobContext
{
#if defined(DM_HAS_THREADS)
    dmArray<dmThread::Thread> m_Threads;
#endif
    JobThreadContext    m_ThreadContext;
    int32_atomic_t      m_Initialized;
    bool                m_UseThreads;
};

// ***********************************************************************************
static void PutWork(JobThreadContext* ctx, HJob job);
static void PutDone(JobThreadContext* ctx, HJob job, JobStatus status, int result);

// ***********************************************************************************
// MISC

static inline uint64_t MakeHandle(uint32_t generation, uint32_t index)
{
    uint64_t handle = ((uint64_t)generation << 32) | index;
    return handle;
}

static inline uint32_t ToGeneration(HJob job)
{
    return (job >> 32) & 0xFFFFFFFF;
}

static inline uint32_t ToIndex(HJob job)
{
    return job & 0xFFFFFFFF;
}

// ***********************************************************************************
// Jobs

static void FreeJob(JobThreadContext* ctx, HJob hjob)
{
    uint32_t generation = ToGeneration(hjob);
    uint32_t index = ToIndex(hjob);
    JobItem& item = ctx->m_Items.Get(index);
    assert(item.m_Generation == generation);
    item.m_Generation = INVALID_INDEX;
    item.m_Status = JOB_STATUS_FREE;

    ctx->m_Items.Free(index, false);
}

static JobItem* ToItem(JobThreadContext* ctx, HJob hjob)
{
    uint32_t generation = ToGeneration(hjob);
    uint32_t index      = ToIndex(hjob);
    JobItem* item = &ctx->m_Items.Get(index);
    if (item->m_Generation != generation)
    {
        return 0;
    }
    return item;
}

static JobItem* CheckItem(JobThreadContext* ctx, HJob hjob)
{
    JobItem* item = ToItem(ctx, hjob);
    assert(item != 0); // Generation differed!
    return item;
}

JobResult SetParent(HContext context, HJob hchild, HJob hparent)
{
    JobThreadContext* ctx = &context->m_ThreadContext;
    DM_MUTEX_OPTIONAL_SCOPED_LOCK(ctx->m_Mutex);

    JobItem* item = CheckItem(ctx, hchild);
    JobItem* parent = CheckItem(ctx, hparent);

    // For now, let's only support it _before_ pushing the jobs
    assert(item->m_Status == JOB_STATUS_CREATED);
    assert(parent->m_Status == JOB_STATUS_CREATED);
    // You should only call setparent once per task
    assert(item->m_Parent == INVALID_JOB);
    assert(item->m_Sibling == INVALID_JOB);

    item->m_Parent = hparent;
    item->m_Sibling = parent->m_FirstChild;
    parent->m_FirstChild = hchild;
    dmAtomicIncrement32(&parent->m_NumChildren);

    // TODO: Make sure all the children inherit the priority of the parent

    return JOB_RESULT_OK;
}

HJob CreateJob(HContext context, Job* job)
{
    JobThreadContext* ctx = &context->m_ThreadContext;

    uint32_t index;
    uint64_t generation;
    {
        DM_MUTEX_OPTIONAL_SCOPED_LOCK(ctx->m_Mutex);

        if (ctx->m_Items.Full())
            ctx->m_Items.OffsetCapacity(16);

        index = ctx->m_Items.Alloc();
        generation = ctx->m_Generation++;
    }

    JobItem& item = ctx->m_Items.Get(index);
    memset(&item, 0, sizeof(item));

    item.m_TimeCreated  = dmTime::GetMonotonicTime();
    item.m_Job          = *job;
    item.m_Generation   = generation;
    item.m_Status       = JOB_STATUS_CREATED;
    item.m_Parent       = INVALID_JOB;
    item.m_Sibling      = INVALID_JOB;
    item.m_FirstChild   = INVALID_JOB;

    return MakeHandle(generation, index);
}

JobResult PushJob(HContext context, HJob job)
{
    if (dmAtomicGet32(&context->m_Initialized) == 0)
    {
        printf("Not m_Initialized!\n");
        return JOB_RESULT_ERROR;
    }

    JobThreadContext* ctx = &context->m_ThreadContext;

    PutWork(ctx, job);

#if defined(DM_HAS_THREADS)
    if (ctx->m_WakeupCond)
    {
        dmConditionVariable::Signal(ctx->m_WakeupCond);
    }
#endif

    return JOB_RESULT_OK;
}

// Deprecated
bool PushJob(HContext context, FProcess process, FCallback callback, void* user_context, void* data)
{
    Job job = {0};
    job.m_Process   = process;
    job.m_Callback  = callback;
    job.m_Context   = user_context;
    job.m_Data      = data;
    job.m_Tag       = 0;

    HJob hjob = CreateJob(context, &job);
    if (!hjob)
    {
        printf("Failed to create job!\n");
        return false;
    }
    return PushJob(context, hjob);
}

static JobItem* GetJobItem(JobThreadContext* ctx, HJob hjob)
{
    uint32_t generation = ToGeneration(hjob);
    uint32_t index      = ToIndex(hjob);
    JobItem* item = &ctx->m_Items.Get(index);
    if (item->m_Generation != generation)
        return 0;
    return item;
}

static JobResult CancelJobInternal(JobThreadContext* ctx, HJob hjob)
{
    JobItem* item = GetJobItem(ctx, hjob);
    if (!item)
        return JOB_RESULT_INVALID_HANDLE;

    if (item->m_Status == JOB_STATUS_PROCESSING)
        return JOB_RESULT_PENDING;

    PutDone(ctx, hjob, JOB_STATUS_CANCELED, 0);

    JobResult result = JOB_RESULT_OK;

    HJob hchild = item->m_FirstChild;
    while (hchild != INVALID_JOB)
    {
        JobResult childresult = CancelJobInternal(ctx, hchild);
        JobItem* child = GetJobItem(ctx, hchild);
        if (childresult == JOB_RESULT_PENDING)
            result = JOB_RESULT_PENDING;

        hchild = child->m_Sibling;
    }
    return result;
}

JobResult CancelJob(HContext context, HJob hjob)
{
    JobThreadContext* ctx = &context->m_ThreadContext;
    DM_MUTEX_OPTIONAL_SCOPED_LOCK(ctx->m_Mutex);

    JobResult result = CancelJobInternal(ctx, hjob);

    // Prune all the canceled jobs
    {
        uint32_t size = ctx->m_Work.Size();

        // find it in the work list and remove it
        jc::RingBuffer<HJob> items;
        items.SetCapacity(size);

        for (uint32_t i = 0; i < size; ++i)
        {
            JobItem* item = GetJobItem(ctx, ctx->m_Work[i]);
            if (item->m_Status != JOB_STATUS_CANCELED)
            {
                items.Push(ctx->m_Work[i]);
            }
        }
        // restore the work queue
        ctx->m_Work.Swap(items);
    }

    return JOB_RESULT_OK;
}

// ***********************************************************************************
// Job Thread


static void PutWork(JobThreadContext* ctx, HJob job)
{
    DM_MUTEX_OPTIONAL_SCOPED_LOCK(ctx->m_Mutex);

    uint32_t generation = ToGeneration(job);
    uint32_t index      = ToIndex(job);
    JobItem& item = ctx->m_Items.Get(index);
    assert(item.m_Generation == generation);
    assert(item.m_Status == JOB_STATUS_CREATED);

    item.m_Status = JOB_STATUS_QUEUED;

    if (item.m_Parent != INVALID_JOB)
    {
        JobItem* parent = GetJobItem(ctx, item.m_Parent);
        // Current limitation, the caller must push the parent jobs last
        assert(parent->m_Status == JOB_STATUS_CREATED);
    }

    if (ctx->m_Work.Full())
        ctx->m_Work.OffsetCapacity(16);
    ctx->m_Work.Push(job);
}

static void PutDone(JobThreadContext* ctx, HJob job, JobStatus status, int result)
{
    DM_MUTEX_OPTIONAL_SCOPED_LOCK(ctx->m_Mutex);

    if (ctx->m_Done.Full())
        ctx->m_Done.OffsetCapacity(16);
    ctx->m_Done.Push(job);

    uint32_t generation = ToGeneration(job);
    uint32_t index      = ToIndex(job);
    JobItem& item = ctx->m_Items.Get(index);
    assert(item.m_Generation == generation);

    item.m_Status = status;
    item.m_Result = result;

    if (item.m_Parent != INVALID_JOB)
    {
        JobItem& parent = ctx->m_Items.Get(item.m_Parent);
        dmAtomicIncrement32(&parent.m_NumChildrenCompleted);
    }
}

static void CancelAllJobs(HContext context)
{
    JobThreadContext* ctx = &context->m_ThreadContext;
    DM_MUTEX_OPTIONAL_SCOPED_LOCK(ctx->m_Mutex);

    uint32_t size = ctx->m_Work.Size();
    for (uint32_t i = 0; i < size; ++i)
    {
        HJob hjob = ctx->m_Work[i];
        PutDone(ctx, hjob, JOB_STATUS_CANCELED, 0);
    }

    ctx->m_Work.Clear();
}

static void ProcessOneJob(JobThreadContext* ctx, HJob hjob)
{
    uint32_t generation = ToGeneration(hjob);
    uint32_t index      = ToIndex(hjob);

    Job job;
    {
        DM_MUTEX_OPTIONAL_SCOPED_LOCK(ctx->m_Mutex);

        JobItem& item = ctx->m_Items.Get(index);
        assert(item.m_Generation == generation);
        assert(item.m_Status == JOB_STATUS_QUEUED);
        item.m_Status = JOB_STATUS_PROCESSING;

        job = item.m_Job;

    }

    // Don't keep the lock here, as the jobs may use their own locks, and it may easily lead to a dead lock
    int result = job.m_Process(hjob, job.m_Tag, job.m_Context, job.m_Data);

    PutDone(ctx, hjob, JOB_STATUS_FINISHED, result);
}

// Good for unit testing with/without threads enabled
static void UpdateSingleThread(JobThreadContext* ctx, uint64_t max_time)
{
    uint64_t tstart = dmTime::GetMonotonicTime();
    while (!ctx->m_Work.Empty())
    {
        HJob hjob = ctx->m_Work.Pop();
        ProcessOneJob(ctx, hjob);

        uint64_t tend = dmTime::GetMonotonicTime();
        if ((tend-tstart) > max_time)
        {
            break;
        }
    }
}

#if defined(DM_HAS_THREADS)
static void JobThread(void* _ctx)
{
    JobThreadContext* ctx = (JobThreadContext*)_ctx;
    while (true)
    {
        HJob hjob = 0;
        {
            DM_MUTEX_SCOPED_LOCK(ctx->m_Mutex);

            if (!ctx->m_Run)
                break;

            while(ctx->m_Work.Empty())
            {
                dmConditionVariable::Wait(ctx->m_WakeupCond, ctx->m_Mutex);
                if (!ctx->m_Run)
                    return;
            }
            hjob = ctx->m_Work.Pop();
        }

        {
            DM_PROFILE("JobThreadProcess");
            ProcessOneJob(ctx, hjob);
        }
    }
}
#endif

static void ProcessFinishedJobs(HContext context, jc::RingBuffer<HJob>& items)
{
    // Lock should already be acquired
    JobThreadContext* ctx = &context->m_ThreadContext;

    uint32_t size = items.Size();
    for(uint32_t i = 0; i < size; ++i)
    {
        HJob hjob = items[i];
        JobItem* item = GetJobItem(ctx, hjob);

        if (!item)
        {
            continue;
        }

        Job& job = item->m_Job;
        if (job.m_Callback)
        {
            // Don't keep the lock here, as the jobs may use their own locks, and it may easily lead to a dead lock
            // (this is generally on the main thread which is less problematic, but still)
            job.m_Callback(hjob, job.m_Tag, job.m_Context, job.m_Data, item->m_Result);
        }

        DM_MUTEX_OPTIONAL_SCOPED_LOCK(context->m_ThreadContext.m_Mutex);
        FreeJob(ctx, hjob);
    }
}

HContext Create(const JobThreadCreationParams& create_params)
{
    JobContext* context = new JobContext;
    context->m_Initialized = 1;

    context->m_ThreadContext.m_Generation = 1;

#if defined(DM_HAS_THREADS)
    uint32_t thread_count = dmMath::Min(create_params.m_ThreadCount, DM_MAX_JOB_THREAD_COUNT);
    context->m_UseThreads = thread_count > 0;
    if (context->m_UseThreads)
    {
        context->m_ThreadContext.m_Mutex = dmMutex::New();
        context->m_ThreadContext.m_WakeupCond = dmConditionVariable::New();
        context->m_ThreadContext.m_Run = true;

        context->m_Threads.SetCapacity(thread_count);
        context->m_Threads.SetSize(thread_count);

        for (int i = 0; i < thread_count; ++i)
        {
            char name_buf[128];
            dmSnPrintf(name_buf, sizeof(name_buf), "%s_%d", create_params.m_ThreadNames[i], i);
            context->m_Threads[i] = dmThread::New(JobThread, 0x80000, (void*)&context->m_ThreadContext, name_buf);
        }
    }
    else
    {
        context->m_ThreadContext.m_Mutex = 0;
        context->m_ThreadContext.m_WakeupCond = 0;
        context->m_ThreadContext.m_Run = false;
    }
#else
    context->m_UseThreads = false;
#endif
    return context;
}

void Destroy(HContext context)
{
    if (!context)
        return;

    dmAtomicDecrement32(&context->m_Initialized); // accept no more jobs

    CancelAllJobs(context);

#if defined(DM_HAS_THREADS)
    if (context->m_UseThreads)
    {
        {
            DM_MUTEX_SCOPED_LOCK(context->m_ThreadContext.m_Mutex);

            context->m_ThreadContext.m_Run = false;

            dmConditionVariable::Broadcast(context->m_ThreadContext.m_WakeupCond);
        }

        for (int i = 0; i < context->m_Threads.Size(); ++i)
        {
            dmThread::Join(context->m_Threads[i]);
        }

        dmConditionVariable::Delete(context->m_ThreadContext.m_WakeupCond);
        dmMutex::Delete(context->m_ThreadContext.m_Mutex);
    }
#endif // DM_HAS_THREADS

    delete context;
}


uint32_t GetWorkerCount(HContext context)
{
#if defined(DM_HAS_THREADS)
    return context->m_Threads.Size();
#else
    return 0;
#endif
}

void Update(HContext context, uint64_t time_limit)
{
    DM_PROFILE("JobThreadUpdate");

    if (!context->m_UseThreads)
    {
        UpdateSingleThread(&context->m_ThreadContext, time_limit);
    }

    // Lock for as little as possible, by copying the items to an array owned by this thread
    jc::RingBuffer<HJob> items;

    {
        DM_MUTEX_OPTIONAL_SCOPED_LOCK(context->m_ThreadContext.m_Mutex);
        items.Swap(context->m_ThreadContext.m_Done);
    }

    // Now do the callbacks
    ProcessFinishedJobs(context, items);
}

static void DebugPrintJob(JobThreadContext* ctx, HJob hjob)
{
    uint32_t generation = ToGeneration(hjob);
    uint32_t index      = ToIndex(hjob);
    JobItem& item = ctx->m_Items.Get(index);
}

void DebugPrintJobs(HContext context)
{
    DM_MUTEX_OPTIONAL_SCOPED_LOCK(context->m_ThreadContext.m_Mutex);

    JobThreadContext* ctx = &context->m_ThreadContext;

    printf("JOBTHREAD: %p\n", context);
    printf("  DONE:\n");
    for (uint32_t i = 0; i < context->m_ThreadContext.m_Done.Size(); ++i)
    {
        HJob hjob = context->m_ThreadContext.m_Done[i];
        DebugPrintJob(ctx, hjob);
    }
    printf("  QUEUE:\n");
    for (uint32_t i = 0; i < context->m_ThreadContext.m_Work.Size(); ++i)
    {
        HJob hjob = context->m_ThreadContext.m_Work[i];
        DebugPrintJob(ctx, hjob);
    }
}


} // namespace dmJobThread
