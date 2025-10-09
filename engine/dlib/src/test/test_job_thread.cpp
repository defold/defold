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

#include "dlib/job_thread.h"
#include "dlib/array.h"
#include "dlib/atomic.h"
#include "dlib/dalloca.h"
#include "dlib/time.h"

#include <dlib/thread.h> // We want the defines DM_HAS_THREADS

#define JC_TEST_IMPLEMENTATION
#include <jc_test/jc_test.h>


static int32_t ProcessSimple(dmJobThread::HContext ctx, dmJobThread::HJob job, void* user_context, void* user_data)
{
    printf("ProcessSimple: %p  result: 1\n", (void*)(uintptr_t)job);
    return 1;
}

static void CallbackSimple(dmJobThread::HContext ctx, dmJobThread::HJob job, dmJobThread::JobStatus status, void* user_context, void* user_data, int32_t user_result)
{
    printf("CallbackSimple: %p  result: %d\n", (void*)(uintptr_t)job, user_result);

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

        dmJobThread::JobThreadCreationParams job_thread_create_param;
        job_thread_create_param.m_ThreadNamePrefix  = "DefoldTestJobThread1";
        job_thread_create_param.m_ThreadCount       = m_NumThreads;

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

printf("ProcessCancel ->\n");
    for (uint32_t i = 0; i < context->m_JobsToCancel.Size(); ++i)
    {
        dmJobThread::CancelJob(ctx, context->m_JobsToCancel[i]);
    }
printf("<- ProcessCancel\n");

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
    uint64_t stop_time = dmTime::GetMonotonicTime() + 250000;
    while (dmTime::GetMonotonicTime() < stop_time && !tests_done)
    {
        dmJobThread::Update(m_JobThread, 500);

        tests_done = count_finished == job_count;

        dmTime::Sleep(20*1000);
    }

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


const TestParams test_setups[] = {
    TestParams(0), // single threaded test
#if defined(DM_HAS_THREADS)
    TestParams(4),
#endif
};

INSTANTIATE_TEST_CASE_P(dmJobThreadTest, dmJobThreadTest, jc_test_values_in(test_setups));

int main(int argc, char **argv)
{
    jc_test_init(&argc, argv);
    return jc_test_run_all();
}
