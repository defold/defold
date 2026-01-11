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

#include "dlib/job_thread.h"
#include "dlib/array.h"
#include "dlib/atomic.h"
#include "dlib/dalloca.h"
#include "dlib/time.h"
#include "dlib/log.h"

#include <dlib/thread.h> // We want the defines DM_HAS_THREADS

#define JC_TEST_IMPLEMENTATION
#include <jc_test/jc_test.h>


static int32_t ProcessSimple(dmJobThread::HContext ctx, dmJobThread::HJob job, void* user_context, void* user_data)
{
    return 1;
}

static void CallbackSimple(dmJobThread::HContext ctx, dmJobThread::HJob job, dmJobThread::JobStatus status, void* user_context, void* user_data, int32_t user_result)
{
    uint8_t* d = (uint8_t*) user_data;
    if (d)
        *d = user_result; // If the task was cancelled, the process function will not have
}

struct TestParams
{
    int m_NumThreads;

    TestParams(int num_threads) : m_NumThreads(num_threads) {};
};

class dmJobThreadTest : public jc_test_params_class<TestParams>
{
public:
    virtual void SetUp() override
    {
        m_NumThreads = GetParam().m_NumThreads;

        dmJobThread::JobThreadCreationParams job_thread_create_param = {0};
        job_thread_create_param.m_ThreadCount = m_NumThreads;

        m_JobThread = dmJobThread::Create(job_thread_create_param);
        ASSERT_NE((dmJobThread::HContext)0, m_JobThread);

        if (m_NumThreads == 0)
            m_NumThreads = 1;
    }

    virtual void TearDown() override
    {
        if (m_JobThread)
            dmJobThread::Destroy(m_JobThread);
    }

    dmJobThread::HJob PushJob(dmJobThread::FProcess process, dmJobThread::FCallback callback, void* context, void* data)
    {
        dmJobThread::Job job = {0};
        job.m_Process = process;
        job.m_Callback = callback;
        job.m_Context = context;
        job.m_Data = data;

        dmJobThread::HJob hjob = dmJobThread::CreateJob(m_JobThread, &job);
        dmJobThread::PushJob(m_JobThread, hjob);
        return hjob;
    }

    dmJobThread::HContext m_JobThread;
    int m_NumThreads;
};

TEST_P(dmJobThreadTest, PushJobs)
{
    PushJob(ProcessSimple, CallbackSimple, 0, 0);
}

TEST_P(dmJobThreadTest, PushJobsMultipleThreads)
{
    int num_jobs = m_NumThreads * 3;

    uint8_t* contexts = (uint8_t*)dmAlloca(sizeof(uint8_t)*num_jobs);
    uint8_t* datas = (uint8_t*)dmAlloca(sizeof(uint8_t)*num_jobs);

    for (int i = 0; i < num_jobs; ++i)
    {
        contexts[i] = i;
        datas[i] = 0;

        PushJob(ProcessSimple, CallbackSimple, (void*) &contexts[i], (void*) &datas[i]);
    }

    uint64_t stop_time = dmTime::GetMonotonicTime() + 2 * 1*1e6; // 1 second
    bool tests_done = false;
    while (dmTime::GetMonotonicTime() < stop_time && !tests_done)
    {
        dmJobThread::Update(m_JobThread, 0);

        tests_done = true;
        for (int i = 0; i < num_jobs; ++i)
        {
            tests_done &= datas[i];
        }

        dmTime::Sleep(20*1000);
    }

    if (!tests_done)
    {
        printf("datas[%d] = ", num_jobs);
        for (int i = 0; i < num_jobs; ++i)
        {
            printf("%2d", datas[i]);
        }
        printf("\n");
    }

    ASSERT_TRUE(tests_done);
}

struct ContextCancel
{
    int32_atomic_t m_NumProcessed;
    int32_atomic_t m_NumFinished;
    int32_atomic_t m_NumFinishedOk;

    dmArray<dmJobThread::HJob> m_JobsToCancel;
};

// When the first finished job arrives, we'll cancel the rest of the tasks
static int32_t ProcessCancel(dmJobThread::HContext ctx, dmJobThread::HJob job, void* user_context, void* user_data)
{
    ContextCancel* context = (ContextCancel*)user_context;
    dmAtomicIncrement32(&context->m_NumProcessed);

    uint64_t t = (uint64_t)(uintptr_t)user_data;

    for (uint32_t i = 0; i < context->m_JobsToCancel.Size(); ++i)
    {
        dmJobThread::CancelJob(ctx, context->m_JobsToCancel[i]);
    }

    dmTime::Sleep(t);
    return 1;
}

static void CallbackCancel(dmJobThread::HContext ctx, dmJobThread::HJob job, dmJobThread::JobStatus status, void* user_context, void* user_data, int32_t user_result)
{
    ContextCancel* context = (ContextCancel*)user_context;
    dmAtomicIncrement32(&context->m_NumFinished);
    dmAtomicAdd32(&context->m_NumFinishedOk, status == dmJobThread::JOB_STATUS_FINISHED ? 1 : 0);
}

TEST_P(dmJobThreadTest, CancelJobs)
{
    if (m_NumThreads > 1)
    {
        return; // the test is written for a one worker thread
    }
    ContextCancel cancelctx = {0};

    dmJobThread::Job job = {0};
    job.m_Process = ProcessCancel;
    job.m_Callback = CallbackCancel;
    job.m_Context = &cancelctx;

    job.m_Data = (void*)(uintptr_t)1000;
    dmJobThread::HJob job1 = dmJobThread::CreateJob(m_JobThread, &job);

    job.m_Data = 0;
    dmJobThread::HJob job2 = dmJobThread::CreateJob(m_JobThread, &job);
    dmJobThread::HJob job3 = dmJobThread::CreateJob(m_JobThread, &job);
    dmJobThread::HJob job4 = dmJobThread::CreateJob(m_JobThread, &job);

    cancelctx.m_JobsToCancel.SetCapacity(2);
    cancelctx.m_JobsToCancel.Push(job2);
    cancelctx.m_JobsToCancel.Push(job3);

    // specifically test trying to push a cancelled job
    dmJobThread::CancelJob(m_JobThread, job4);
    ASSERT_EQ(dmJobThread::JOB_RESULT_CANCELED, dmJobThread::PushJob(m_JobThread, job4));

    dmJobThread::PushJob(m_JobThread, job1);
    dmJobThread::PushJob(m_JobThread, job2);
    dmJobThread::PushJob(m_JobThread, job3);

    bool tests_done = false;
    uint64_t stop_time = dmTime::GetMonotonicTime() + 500000;
    while (dmTime::GetMonotonicTime() < stop_time && !tests_done)
    {
        dmJobThread::Update(m_JobThread, 500);

        tests_done = cancelctx.m_NumFinished == 3;

        dmTime::Sleep(20*1000);
    }

    ASSERT_EQ(1, cancelctx.m_NumProcessed);
    ASSERT_EQ(3, cancelctx.m_NumFinished);
    ASSERT_EQ(1, cancelctx.m_NumFinishedOk);
}

struct JobWithDependency
{
    uint64_t m_Sleep;
    int      m_Index;

    // Track which order the items are processed/finished (instead of time)
    int      m_ProcessingOrder;
    int      m_FinishingOrder;

    int32_atomic_t* m_Order;
};

static int32_t ProcessSortedDependencyJobs(dmJobThread::HContext ctx, dmJobThread::HJob hjob, void* user_context, void* user_data)
{
    JobWithDependency* data = (JobWithDependency*)user_data;
    dmTime::Sleep(data->m_Sleep);
    data->m_ProcessingOrder = dmAtomicIncrement32(data->m_Order);
    printf("job%d: process: order %d  %p\n", data->m_Index, data->m_ProcessingOrder, (void*)(uintptr_t)hjob);
    return 1;
}

static void CallbackSortedDependencyJobs(dmJobThread::HContext ctx, dmJobThread::HJob hjob, dmJobThread::JobStatus status, void* user_context, void* user_data, int32_t user_result)
{
    uint32_t* count_finished = (uint32_t*)user_context;
    JobWithDependency* data = (JobWithDependency*)user_data;
    data->m_FinishingOrder = dmAtomicIncrement32(data->m_Order);
    printf("job%d: finish: order %d\n", data->m_Index, data->m_FinishingOrder);
    (*count_finished)++;
}

// Make sure they all children are processed before their parents
TEST_P(dmJobThreadTest, SortedDependencyJobs)
{
    const uint32_t      job_count = 7;
    JobWithDependency   items[job_count];
    dmJobThread::HJob   hjobs[job_count];
    memset(items, 0, sizeof(items));

    int32_atomic_t order = 0;
    uint32_t count_finished = 0;

    for (uint32_t i = 0; i < job_count; ++i)
    {
        items[i].m_Index = (int)i;
        items[i].m_Sleep = rand() % 1000;
        items[i].m_Order = &order;
        printf("job%d: init: sleep: %d\n", (int)i, (int)items[i].m_Sleep);

        dmJobThread::Job job = {0};
        job.m_Process = ProcessSortedDependencyJobs;
        job.m_Callback = CallbackSortedDependencyJobs;
        job.m_Context = (void*)&count_finished;
        job.m_Data    = (void*)&items[i];

        hjobs[i] = dmJobThread::CreateJob(m_JobThread, &job);
    }

    // Setup the dependencies
    // [      3      ]
    // [  1       5  ]
    // [0   2   4   6]
    dmJobThread::SetParent(m_JobThread, hjobs[0], hjobs[1]);
    dmJobThread::SetParent(m_JobThread, hjobs[2], hjobs[1]);
    dmJobThread::SetParent(m_JobThread, hjobs[4], hjobs[5]);
    dmJobThread::SetParent(m_JobThread, hjobs[6], hjobs[5]);

    dmJobThread::SetParent(m_JobThread, hjobs[5], hjobs[3]);
    dmJobThread::SetParent(m_JobThread, hjobs[1], hjobs[3]);

    dmJobThread::JobResult result;

    for (uint32_t i = 0; i < job_count; ++i)
    {
        result = dmJobThread::PushJob(m_JobThread, hjobs[i]);
        ASSERT_EQ(dmJobThread::JOB_RESULT_OK, result);
    }

    bool tests_done = false;
    bool timeout = false;
    uint64_t max_time = 1000000;
    uint64_t stop_time = dmTime::GetMonotonicTime() + max_time;
    while (!tests_done)
    {
        if (dmTime::GetMonotonicTime() >= stop_time)
        {
            timeout = true;
            break;
        }

        dmJobThread::Update(m_JobThread, 500);

        tests_done = count_finished == job_count;

        dmTime::Sleep(20*1000);
    }

    if (timeout)
    {
        dmLogError("Test timed out after %u ms", (uint32_t)max_time / 1000);
    }

    ASSERT_FALSE(timeout);

    // Make sure all children are processed before their parents

    ASSERT_LT(items[0].m_ProcessingOrder, items[1].m_ProcessingOrder);
    ASSERT_LT(items[2].m_ProcessingOrder, items[1].m_ProcessingOrder);

    ASSERT_LT(items[4].m_ProcessingOrder, items[5].m_ProcessingOrder);
    ASSERT_LT(items[6].m_ProcessingOrder, items[5].m_ProcessingOrder);

    ASSERT_LT(items[1].m_ProcessingOrder, items[3].m_ProcessingOrder);
    ASSERT_LT(items[5].m_ProcessingOrder, items[3].m_ProcessingOrder);

    ASSERT_LT(items[0].m_FinishingOrder, items[1].m_FinishingOrder);
    ASSERT_LT(items[2].m_FinishingOrder, items[1].m_FinishingOrder);

    ASSERT_LT(items[4].m_FinishingOrder, items[5].m_FinishingOrder);
    ASSERT_LT(items[6].m_FinishingOrder, items[5].m_FinishingOrder);

    ASSERT_LT(items[1].m_FinishingOrder, items[3].m_FinishingOrder);
    ASSERT_LT(items[5].m_FinishingOrder, items[3].m_FinishingOrder);


    ASSERT_EQ(job_count, count_finished);
}


struct CancelChildTrack
{
    uint32_t                        m_Index;
    bool                            m_Delay;
    int32_atomic_t                  m_ProcessCalled;
    int32_atomic_t                  m_CallbackCalled;
    dmJobThread::JobStatus          m_CallbackStatus;
    dmJobThread::HJob               m_Job;
};

struct CancelParentTrack
{
    int32_atomic_t                  m_ProcessCalled;
    int32_atomic_t                  m_CallbackCalled;
    dmJobThread::JobStatus          m_CallbackStatus;
};

int32_atomic_t g_CancelParentGlobalLock = 0;

static int32_t ProcessCancelChild(dmJobThread::HContext ctx, dmJobThread::HJob job, void* user_context, void* user_data)
{
    CancelChildTrack* child = (CancelChildTrack*)user_data;
    dmAtomicIncrement32(&child->m_ProcessCalled);
    if (child->m_Delay)
    {
        // Wait until the test says it's ok to continue
        while (dmAtomicGet32(&g_CancelParentGlobalLock)) // only set if threads > 1
        {
            dmTime::Sleep(20*1000);
        }
    }
    return 1;
}

static void CallbackCancelChild(dmJobThread::HContext ctx, dmJobThread::HJob job, dmJobThread::JobStatus status, void* user_context, void* user_data, int32_t user_result)
{
    CancelChildTrack* child = (CancelChildTrack*)user_context;
    child->m_CallbackStatus = status;
    child->m_Job = job;
    dmAtomicIncrement32(&child->m_CallbackCalled);
}

static int32_t ProcessCancelParent(dmJobThread::HContext ctx, dmJobThread::HJob job, void* user_context, void* user_data)
{
    CancelParentTrack* parent = (CancelParentTrack*)user_data;
    dmAtomicIncrement32(&parent->m_ProcessCalled);
    return 1;
}

static void CallbackCancelParent(dmJobThread::HContext ctx, dmJobThread::HJob job, dmJobThread::JobStatus status, void* user_context, void* user_data, int32_t user_result)
{
    CancelParentTrack* parent = (CancelParentTrack*)user_context;
    parent->m_CallbackStatus = status;
    dmAtomicIncrement32(&parent->m_CallbackCalled);
}

// This tests that cancelling the parent after one of the children (not the last one) has finished.
// doesn't mess up the internal list of children.
TEST_P(dmJobThreadTest, CancelParentAfterChild)
{
    bool use_threads = GetParam().m_NumThreads != 0;

    g_CancelParentGlobalLock = 0;

    if (use_threads)
        dmAtomicAdd32(&g_CancelParentGlobalLock, 1);

    static const uint32_t CHILD_COUNT = 10;
    // For multi threaded test, we want the mid index such that we keep (num_threads - 1) occupied forever,
    // and one thread gets to finish the task.
    // This will test that cancelling the list of children is done correctly
    uint32_t MID_INDEX = use_threads ? (m_NumThreads-1) : (CHILD_COUNT / 2);

    CancelChildTrack children[CHILD_COUNT];
    memset(children, 0, sizeof(children));
    for (uint32_t i = 0; i < CHILD_COUNT; ++i)
    {
        children[i].m_Index = i;
        children[i].m_CallbackStatus = dmJobThread::JOB_STATUS_FREE;

        // Making sure some doesn't finish, to test the cancelling of the parent
        if (m_NumThreads == 1)
            children[i].m_Delay = i < MID_INDEX;
        else
            children[i].m_Delay = i != MID_INDEX;
    }

    CancelParentTrack parent = {0};
    parent.m_CallbackStatus = dmJobThread::JOB_STATUS_FREE;

    dmJobThread::Job parent_job = {0};
    parent_job.m_Process = ProcessCancelParent;
    parent_job.m_Callback = CallbackCancelParent;
    parent_job.m_Context = &parent;
    parent_job.m_Data = &parent;

    dmJobThread::HJob parent_hjob = dmJobThread::CreateJob(m_JobThread, &parent_job);
    ASSERT_NE((dmJobThread::HJob)0, parent_hjob);

    dmJobThread::HJob child_hjobs[CHILD_COUNT];
    for (uint32_t i = 0; i < CHILD_COUNT; ++i)
    {
        dmJobThread::Job child_job = {0};
        child_job.m_Process = ProcessCancelChild;
        child_job.m_Callback = CallbackCancelChild;
        child_job.m_Context = &children[i];
        child_job.m_Data = &children[i];

        child_hjobs[i] = dmJobThread::CreateJob(m_JobThread, &child_job);
        ASSERT_NE((dmJobThread::HJob)0, child_hjobs[i]);
        ASSERT_EQ(dmJobThread::JOB_RESULT_OK, dmJobThread::SetParent(m_JobThread, child_hjobs[i], parent_hjob));
        ASSERT_EQ(dmJobThread::JOB_RESULT_OK, dmJobThread::PushJob(m_JobThread, child_hjobs[i]));
    }

    ASSERT_EQ(dmJobThread::JOB_RESULT_OK, dmJobThread::PushJob(m_JobThread, parent_hjob));

    bool mid_finished = false;
    uint64_t finish_limit = dmTime::GetMonotonicTime() + 500000;
    while (!mid_finished)
    {
        dmJobThread::Update(m_JobThread, 0);
        mid_finished = dmAtomicGet32(&children[MID_INDEX].m_CallbackCalled) == 1;
        if (!mid_finished)
        {
            if (dmTime::GetMonotonicTime() >= finish_limit)
            {
                dmLogError("CancelParentAfterChild still waiting for child M after 500 ms");
                finish_limit = dmTime::GetMonotonicTime() + 500000;
                break;
            }
            if (use_threads)
                dmTime::Sleep(20*1000);
        }
    }

    ASSERT_EQ(1, dmAtomicGet32(&children[MID_INDEX].m_ProcessCalled));
    ASSERT_EQ(1, dmAtomicGet32(&children[MID_INDEX].m_CallbackCalled));
    ASSERT_EQ(dmJobThread::JOB_STATUS_FINISHED, children[MID_INDEX].m_CallbackStatus);

    for (uint32_t i = 0; i < CHILD_COUNT; ++i)
    {
        int process_called = dmAtomicGet32(&children[i].m_ProcessCalled);
        int callback_called = dmAtomicGet32(&children[i].m_CallbackCalled);
        if (m_NumThreads == 1) // i.e. no threads
        {
            if (i <= MID_INDEX)
            {
                ASSERT_EQ(1, process_called);
                ASSERT_EQ(1, callback_called);
            }
            else
            {
                ASSERT_EQ(0, process_called);
                ASSERT_EQ(0, callback_called);
            }
        }
        else
        {
            if (i == MID_INDEX)
            {
                ASSERT_EQ(1, process_called);
                ASSERT_EQ(1, callback_called);
            }
            else
            {
                // As it is multi threaded, we cannot be certain that the tasks have either started
                // and/or finished.
            }
        }
    }

    if (dmAtomicGet32(&g_CancelParentGlobalLock))
        dmAtomicSub32(&g_CancelParentGlobalLock, 1); // set it to 0 so that the tasks may finish

    uint64_t stop_time = dmTime::GetMonotonicTime() + 500000;

    dmJobThread::JobResult result;
    do
    {
        result = dmJobThread::CancelJob(m_JobThread, parent_hjob);

        dmJobThread::Update(m_JobThread, 1000);
        dmTime::Sleep(1);
    } while (dmJobThread::JOB_RESULT_PENDING == result);

    bool all_callbacks_received = false;
    stop_time = dmTime::GetMonotonicTime() + 500000;
    while (dmTime::GetMonotonicTime() < stop_time && !all_callbacks_received)
    {
        dmJobThread::Update(m_JobThread, 1000);
        dmTime::Sleep(1);

        if (dmAtomicGet32(&children[MID_INDEX].m_CallbackCalled) && dmAtomicGet32(&parent.m_CallbackCalled))
        {
            all_callbacks_received = true;
            break;
        }
    }

    ASSERT_TRUE(all_callbacks_received);
    ASSERT_EQ(0, dmAtomicGet32(&parent.m_ProcessCalled));
    ASSERT_EQ(dmJobThread::JOB_STATUS_CANCELED, parent.m_CallbackStatus);

    for (uint32_t i = 0; i < CHILD_COUNT; ++i)
    {
        if (m_NumThreads == 1)
        {
            if (i <= MID_INDEX)
            {
                ASSERT_EQ(dmJobThread::JOB_STATUS_FINISHED, children[i].m_CallbackStatus);
            }
            else
            {
                ASSERT_EQ(dmJobThread::JOB_STATUS_CANCELED, children[i].m_CallbackStatus);
                ASSERT_EQ(0, dmAtomicGet32(&children[i].m_ProcessCalled));
            }
        }
        else
        {
            if (i == MID_INDEX)
            {
                ASSERT_EQ(dmJobThread::JOB_STATUS_FINISHED, children[i].m_CallbackStatus);
            }
            else if (children[i].m_Job)
            {
                // As it is multi threaded, we cannot be certain that the tasks have either started
                // and/or finished, or if they've been cancelled.
            }
        }
    }
}


const TestParams test_setups[] = {
    TestParams(0), // single threaded test
#if defined(DM_HAS_THREADS)
    TestParams(4),
#endif
};

INSTANTIATE_TEST_CASE_P(dmJobThreadTest, dmJobThreadTest, jc_test_values_in(test_setups));

int main(int argc, char **argv)
{
    dmLog::LogParams params;
    dmLog::LogInitialize(&params);

    jc_test_init(&argc, argv);
    int result = jc_test_run_all();

    dmLog::LogFinalize();
    return result;
}
