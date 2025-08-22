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
#include "dlib/time.h"

#define JC_TEST_IMPLEMENTATION
#include <jc_test/jc_test.h>

static int ProcessSimple(dmJobThread::HJob job, uint64_t tag, void* context, void* data)
{
    return 1;
}

static void CallbackSimple(dmJobThread::HJob job, uint64_t tag, void* context, void* data, int result)
{
    uint8_t* d = (uint8_t*) data;
    if (d)
        *d = result;
}

TEST(dmJobThread, PushJobs)
{
    dmJobThread::JobThreadCreationParams job_thread_create_param;
    job_thread_create_param.m_ThreadNames[0] = "DefoldTestJobThread1";
    job_thread_create_param.m_ThreadCount    = 1;

    dmJobThread::HContext ctx = dmJobThread::Create(job_thread_create_param);
    dmJobThread::PushJob(ctx, ProcessSimple, CallbackSimple, 0, 0);
    dmJobThread::Destroy(ctx);
}

TEST(dmJobThread, PushJobsMultipleThreads)
{
    dmJobThread::JobThreadCreationParams job_thread_create_params;
    job_thread_create_params.m_ThreadNames[0] = "DefoldTestJobThread1";
    job_thread_create_params.m_ThreadNames[1] = "DefoldTestJobThread2";
    job_thread_create_params.m_ThreadNames[2] = "DefoldTestJobThread3";
    job_thread_create_params.m_ThreadNames[3] = "DefoldTestJobThread4";
    job_thread_create_params.m_ThreadCount    = 4;

    dmJobThread::HContext ctx = dmJobThread::Create(job_thread_create_params);

    uint8_t contexts[8];
    uint8_t datas[8];
    for (int i = 0; i < DM_ARRAY_SIZE(contexts); ++i)
    {
        contexts[i] = i;
        datas[i] = 0;
        dmJobThread::PushJob(ctx, ProcessSimple, CallbackSimple, (void*) &contexts[i], (void*) &datas[i]);
    }

    uint64_t stop_time = dmTime::GetMonotonicTime() + 1*1e6; // 1 second
    bool tests_done = false;
    while (dmTime::GetMonotonicTime() < stop_time && !tests_done)
    {
        dmJobThread::Update(ctx, 0);

        tests_done = true;
        for (int i = 0; i < DM_ARRAY_SIZE(contexts); ++i)
        {
            tests_done &= datas[i];
        }

        dmTime::Sleep(20*1000);
    }

    dmJobThread::Destroy(ctx);

    ASSERT_TRUE(tests_done);
}

struct ContextCancel
{
    dmJobThread::HContext m_Context;
    int m_NumProcessed;
    int m_NumFinished;
    int m_NumFinishedOk;

    dmArray<HJob> m_JobsToCancel;
};

static int ProcessCancel(dmJobThread::HJob job, uint64_t tag, void* context, void* data)
{
    ContextCancel* ctx = (ContextCancel*)context;
    ctx->m_NumProcessed++;
    uint64_t t = (uint64_t)(uintptr_t)data;
    printf("  process: %d  t: %llu\n", ctx->m_NumProcessed, t);
    dmJobThread::CancelAllJobs(ctx->m_Context);

    for (uint32_t i = 0; i < ctx->m_JobsToCancel.Size(); ++i)
    {
        dmJobThread::CancelAllJobs(ctx->m_Context, ctx->m_JobsToCancel[i]);
    }

    dmTime::Sleep(t);
    return 1;
}

static void CallbackCancel(dmJobThread::HJob job, uint64_t tag, void* context, void* data, int result)
{
    ContextCancel* ctx = (ContextCancel*)context;
    ctx->m_NumFinished++;
    ctx->m_NumFinishedOk += result?1:0;
}

TEST(dmJobThread, CancelJobs)
{
    dmJobThread::JobThreadCreationParams job_thread_create_param;
    job_thread_create_param.m_ThreadNames[0] = "DefoldTestJobThread1";
    job_thread_create_param.m_ThreadCount    = 1;

    dmJobThread::HContext ctx = dmJobThread::Create(job_thread_create_param);

    ContextCancel cancelctx = {0};
    cancelctx.m_Context = ctx;

    dmJobThread::Job job = {0};
    job.m_Process = ProcessCancel;
    job.m_Callback = CallbackCancel;
    job.m_Context = &cancelctx;

    job.m_Data = (void*)(uintptr_t)1000;
    HJob job1 = dmJobThread::CreateJob(ctx, &job);

    job.m_Data = 0;
    HJob job2 = dmJobThread::CreateJob(ctx, &job);
    HJob job3 = dmJobThread::CreateJob(ctx, &job);

    cancelctx.m_JobsToCancel.SetCapacity(2);
    cancelctx.m_JobsToCancel.Push(job2);
    cancelctx.m_JobsToCancel.Push(job3);

    dmJobThread::PushJob(ctx, job1);
    dmJobThread::PushJob(ctx, job2);
    dmJobThread::PushJob(ctx, job3);

    bool tests_done = false;
    uint64_t stop_time = dmTime::GetMonotonicTime() + 500000;
    while (dmTime::GetMonotonicTime() < stop_time && !tests_done)
    {
        dmJobThread::Update(ctx, 500);

        tests_done = cancelctx.m_NumFinished == 3;

        dmTime::Sleep(20*1000);
    }

    dmJobThread::Destroy(ctx);

    ASSERT_EQ(1, cancelctx.m_NumProcessed);
    ASSERT_EQ(3, cancelctx.m_NumFinished);
    ASSERT_EQ(1, cancelctx.m_NumFinishedOk);
}


struct ContextCancelWithTag
{
    dmJobThread::HContext m_Context;
    uint64_t              m_Tag;
    int m_NumProcessed;
    int m_NumFinished;
    int m_NumFinishedOk;
};

static int ProcessCancelWithTag(dmJobThread::HJob job, uint64_t tag, void* context, void* data)
{
    ContextCancelWithTag* ctx = (ContextCancelWithTag*)context;
    ctx->m_NumProcessed++;
    if (ctx->m_Tag)
    {
        dmJobThread::CancelJobsWithTag(ctx->m_Context, ctx->m_Tag);
        ctx->m_Tag = 0;
    }
    return 1;
}

static void CallbackCancelWithTag(dmJobThread::HJob job, uint64_t tag, void* context, void* data, int result)
{
    ContextCancelWithTag* ctx = (ContextCancelWithTag*)context;
    ctx->m_NumFinished++;
    ctx->m_NumFinishedOk += result?1:0;
}

TEST(dmJobThread, CancelJobsWithTag)
{
    dmJobThread::JobThreadCreationParams job_thread_create_param;
    job_thread_create_param.m_ThreadNames[0] = "DefoldTestJobThread1";
    job_thread_create_param.m_ThreadCount    = 1;

    dmJobThread::HContext ctx = dmJobThread::Create(job_thread_create_param);

    ContextCancelWithTag cancelctx = {0};
    cancelctx.m_Context = ctx;
    cancelctx.m_Tag = 1;

    dmJobThread::Job job = {0};
    job.m_Process = ProcessCancelWithTag;
    job.m_Callback = CallbackCancelWithTag;
    job.m_Context = &cancelctx;

    job.m_Tag = 0;
    dmJobThread::PushJobs(ctx, &job, 1);
    dmJobThread::PushJobs(ctx, &job, 1);
    dmJobThread::PushJobs(ctx, &job, 1);

    job.m_Tag = 1;
    dmJobThread::PushJobs(ctx, &job, 1);
    dmJobThread::PushJobs(ctx, &job, 1);

    job.m_Tag = 3;
    dmJobThread::PushJobs(ctx, &job, 1);

    bool tests_done = false;
    uint64_t stop_time = dmTime::GetMonotonicTime() + 250000;
    while (dmTime::GetMonotonicTime() < stop_time && !tests_done)
    {
        dmJobThread::Update(ctx, 500);

        tests_done = cancelctx.m_NumFinished >= 4;

        dmTime::Sleep(20*1000);
    }

    dmJobThread::Destroy(ctx);

    ASSERT_EQ(4, cancelctx.m_NumProcessed);
    ASSERT_EQ(6, cancelctx.m_NumFinished);
    ASSERT_EQ(4, cancelctx.m_NumFinishedOk);
}

int main(int argc, char **argv)
{
    jc_test_init(&argc, argv);
    return jc_test_run_all();
}
