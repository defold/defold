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

#include "dlib/array.h"
#include "dlib/atomic.h"
#include "dlib/dalloca.h"
#include "dlib/log.h"
#include "dlib/time.h"

#include <dmsdk/dlib/jobsystem.h>
#include <dlib/thread.h> // We want the defines DM_HAS_THREADS

#define JC_TEST_IMPLEMENTATION
#include <jc_test/jc_test.h>


static int32_t ProcessSimple(HJobContext ctx, HJob job, void* user_context, void* user_data)
{
    return 1;
}

static void CallbackSimple(HJobContext ctx, HJob job, JobSystemStatus status, void* user_context, void* user_data, int32_t user_result)
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

class dmJobSystemTest : public jc_test_params_class<TestParams>
{
public:
    virtual void SetUp() override
    {
        m_NumThreads = GetParam().m_NumThreads;

        JobSystemCreateParams job_system_create_params = {0};
        job_system_create_params.m_ThreadCount = m_NumThreads;

        m_JobSystem = JobSystemCreate(&job_system_create_params);
        ASSERT_NE((HJobContext)0, m_JobSystem);

        if (m_NumThreads == 0)
            m_NumThreads = 1;
    }

    virtual void TearDown() override
    {
        if (m_JobSystem)
            JobSystemDestroy(m_JobSystem);
    }

    HJob PushJob(FJobProcess process, FJobCallback callback, void* context, void* data)
    {
        Job job = {0};
        job.m_Process = process;
        job.m_Callback = callback;
        job.m_Context = context;
        job.m_Data = data;

        HJob hjob = JobSystemCreateJob(m_JobSystem, &job);
        JobSystemPushJob(m_JobSystem, hjob);
        return hjob;
    }

    HJobContext m_JobSystem;
    int m_NumThreads;
};

static const uint64_t TEST_TIMEOUT_US = 500000;
static const uint64_t TEST_SLEEP_US = 1000;

static bool WaitForAtomicValue(int32_atomic_t* value, int expected, const char* name = 0)
{
    uint64_t stop_time = dmTime::GetMonotonicTime() + TEST_TIMEOUT_US;
    while (dmAtomicGet32(value) != expected && dmTime::GetMonotonicTime() < stop_time)
    {
        dmTime::Sleep(TEST_SLEEP_US);
    }

    int actual = dmAtomicGet32(value);
    if (actual != expected)
    {
        dmLogError("Timed out waiting for %s == %d, got %d", name ? name : "atomic value", expected, actual);
    }
    return actual == expected;
}

static JobSystemResult PollCancelUntilTerminal(HJobContext job_context, HJob hjob, bool no_callback)
{
    JobSystemResult result = JOBSYSTEM_RESULT_PENDING;
    uint64_t stop_time = dmTime::GetMonotonicTime() + TEST_TIMEOUT_US;
    while (result == JOBSYSTEM_RESULT_PENDING && dmTime::GetMonotonicTime() < stop_time)
    {
        dmTime::Sleep(TEST_SLEEP_US);
        result = no_callback ? JobSystemCancelJobNoCallback(job_context, hjob) : JobSystemCancelJob(job_context, hjob);
    }

    if (result == JOBSYSTEM_RESULT_PENDING)
    {
        dmLogError("Timed out waiting for cancel to finish for job %p, no_callback=%d", (void*)(uintptr_t)hjob, no_callback);
    }
    return result;
}

struct ControlledJobState
{
    int32_atomic_t  m_ProcessStarted;
    int32_atomic_t  m_AllowProcessFinish;
    int32_atomic_t  m_ProcessFinished;
    int32_atomic_t  m_CallbackStarted;
    int32_atomic_t  m_AllowCallbackFinish;
    int32_atomic_t  m_CallbackFinished;
    JobSystemStatus m_CallbackStatus;
    int32_t         m_ProcessResult;
};

static void InitControlledJobState(ControlledJobState* state, bool allow_process_finish, bool allow_callback_finish)
{
    memset(state, 0, sizeof(*state));
    state->m_CallbackStatus = JOBSYSTEM_STATUS_FREE;
    state->m_ProcessResult = 1;
    dmAtomicStore32(&state->m_AllowProcessFinish, allow_process_finish ? 1 : 0);
    dmAtomicStore32(&state->m_AllowCallbackFinish, allow_callback_finish ? 1 : 0);
}

static void AllowProcessFinish(ControlledJobState* state)
{
    dmAtomicStore32(&state->m_AllowProcessFinish, 1);
}

static void AllowCallbackFinish(ControlledJobState* state)
{
    dmAtomicStore32(&state->m_AllowCallbackFinish, 1);
}

static int32_t ProcessControlledJob(HJobContext ctx, HJob job, void* user_context, void* user_data)
{
    (void)ctx;
    (void)job;

    ControlledJobState* state = (ControlledJobState*)(user_data ? user_data : user_context);
    dmAtomicIncrement32(&state->m_ProcessStarted);

    while (!dmAtomicGet32(&state->m_AllowProcessFinish))
    {
        dmTime::Sleep(TEST_SLEEP_US);
    }

    dmAtomicIncrement32(&state->m_ProcessFinished);
    return state->m_ProcessResult;
}

static void CallbackControlledJob(HJobContext ctx, HJob job, JobSystemStatus status, void* user_context, void* user_data, int32_t user_result)
{
    (void)ctx;
    (void)job;
    (void)user_data;
    (void)user_result;

    ControlledJobState* state = (ControlledJobState*)user_context;
    state->m_CallbackStatus = status;
    dmAtomicIncrement32(&state->m_CallbackStarted);

    while (!dmAtomicGet32(&state->m_AllowCallbackFinish))
    {
        dmTime::Sleep(TEST_SLEEP_US);
    }

    dmAtomicIncrement32(&state->m_CallbackFinished);
}

static HJob CreateControlledJob(HJobContext job_context, ControlledJobState* state)
{
    Job job = {0};
    job.m_Process = ProcessControlledJob;
    job.m_Callback = CallbackControlledJob;
    job.m_Context = state;
    job.m_Data = state;
    return JobSystemCreateJob(job_context, &job);
}

TEST_P(dmJobSystemTest, PushJobs)
{
    PushJob(ProcessSimple, CallbackSimple, 0, 0);
}

TEST_P(dmJobSystemTest, PushJobsMultipleThreads)
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
        JobSystemUpdate(m_JobSystem, 0);

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

    dmArray<HJob> m_JobsToCancel;
};

// When the first finished job arrives, we'll cancel the rest of the tasks
static int32_t ProcessCancel(HJobContext ctx, HJob job, void* user_context, void* user_data)
{
    ContextCancel* context = (ContextCancel*)user_context;
    dmAtomicIncrement32(&context->m_NumProcessed);

    uint64_t t = (uint64_t)(uintptr_t)user_data;

    for (uint32_t i = 0; i < context->m_JobsToCancel.Size(); ++i)
    {
        JobSystemCancelJob(ctx, context->m_JobsToCancel[i]);
    }

    dmTime::Sleep(t);
    return 1;
}

static void CallbackCancel(HJobContext ctx, HJob job, JobSystemStatus status, void* user_context, void* user_data, int32_t user_result)
{
    ContextCancel* context = (ContextCancel*)user_context;
    dmAtomicIncrement32(&context->m_NumFinished);
    dmAtomicAdd32(&context->m_NumFinishedOk, status == JOBSYSTEM_STATUS_FINISHED ? 1 : 0);
}

TEST_P(dmJobSystemTest, CancelJobs)
{
    if (m_NumThreads > 1)
    {
        return; // the test is written for a one worker thread
    }
    ContextCancel cancelctx = {0};

    Job job = {0};
    job.m_Process = ProcessCancel;
    job.m_Callback = CallbackCancel;
    job.m_Context = &cancelctx;

    job.m_Data = (void*)(uintptr_t)1000;
    HJob job1 = JobSystemCreateJob(m_JobSystem, &job);

    job.m_Data = 0;
    HJob job2 = JobSystemCreateJob(m_JobSystem, &job);
    HJob job3 = JobSystemCreateJob(m_JobSystem, &job);
    HJob job4 = JobSystemCreateJob(m_JobSystem, &job);

    cancelctx.m_JobsToCancel.SetCapacity(2);
    cancelctx.m_JobsToCancel.Push(job2);
    cancelctx.m_JobsToCancel.Push(job3);

    // specifically test trying to push a cancelled job
    JobSystemCancelJob(m_JobSystem, job4);
    ASSERT_EQ(JOBSYSTEM_RESULT_CANCELED, JobSystemPushJob(m_JobSystem, job4));

    JobSystemPushJob(m_JobSystem, job1);
    JobSystemPushJob(m_JobSystem, job2);
    JobSystemPushJob(m_JobSystem, job3);

    bool tests_done = false;
    uint64_t stop_time = dmTime::GetMonotonicTime() + 500000;
    while (dmTime::GetMonotonicTime() < stop_time && !tests_done)
    {
        JobSystemUpdate(m_JobSystem, 500);

        tests_done = cancelctx.m_NumFinished == 3;

        dmTime::Sleep(20*1000);
    }

    ASSERT_EQ(1, cancelctx.m_NumProcessed);
    ASSERT_EQ(3, cancelctx.m_NumFinished);
    ASSERT_EQ(1, cancelctx.m_NumFinishedOk);
}

// Normal cancellation keeps callback cleanup consistent. A finished job that has
// not yet been flushed should still invoke its callback during JobSystemUpdate().
TEST_P(dmJobSystemTest, CancelFinishedJobBeforeUpdateRunsCallback)
{
    if (GetParam().m_NumThreads == 0)
    {
        return;
    }

    ControlledJobState job_state;
    InitControlledJobState(&job_state, false, true);

    HJob hjob = CreateControlledJob(m_JobSystem, &job_state);
    ASSERT_NE((HJob)0, hjob);
    ASSERT_EQ(JOBSYSTEM_RESULT_OK, JobSystemPushJob(m_JobSystem, hjob));

    ASSERT_TRUE(WaitForAtomicValue(&job_state.m_ProcessStarted, 1, "job process start"));

    ASSERT_EQ(JOBSYSTEM_RESULT_PENDING, JobSystemCancelJob(m_JobSystem, hjob));

    AllowProcessFinish(&job_state);

    JobSystemResult result = PollCancelUntilTerminal(m_JobSystem, hjob, false);

    ASSERT_EQ(JOBSYSTEM_RESULT_OK, result);
    ASSERT_EQ(0, dmAtomicGet32(&job_state.m_CallbackFinished));

    JobSystemUpdate(m_JobSystem, 0);
    ASSERT_EQ(1, dmAtomicGet32(&job_state.m_CallbackFinished));
    ASSERT_EQ(JOBSYSTEM_STATUS_FINISHED, job_state.m_CallbackStatus);
}

// No-callback cancellation lets owners release callback-owned state as soon as
// cancellation stops reporting PENDING.
TEST_P(dmJobSystemTest, CancelFinishedJobNoCallbackBeforeUpdateSkipsCallback)
{
    if (GetParam().m_NumThreads == 0)
    {
        return;
    }

    ControlledJobState job_state;
    InitControlledJobState(&job_state, false, true);

    HJob hjob = CreateControlledJob(m_JobSystem, &job_state);
    ASSERT_NE((HJob)0, hjob);
    ASSERT_EQ(JOBSYSTEM_RESULT_OK, JobSystemPushJob(m_JobSystem, hjob));

    ASSERT_TRUE(WaitForAtomicValue(&job_state.m_ProcessStarted, 1, "job process start"));

    ASSERT_EQ(JOBSYSTEM_RESULT_PENDING, JobSystemCancelJobNoCallback(m_JobSystem, hjob));

    AllowProcessFinish(&job_state);

    JobSystemResult result = PollCancelUntilTerminal(m_JobSystem, hjob, true);

    ASSERT_EQ(JOBSYSTEM_RESULT_OK, result);
    ASSERT_EQ(0, dmAtomicGet32(&job_state.m_CallbackFinished));

    JobSystemUpdate(m_JobSystem, 0);
    ASSERT_EQ(0, dmAtomicGet32(&job_state.m_CallbackFinished));
}

struct UpdateUntilCallbackStartedContext
{
    HJobContext    m_JobSystem;
    ControlledJobState* m_JobState;
    int32_atomic_t m_StopUpdateThread;
};

static void UpdateUntilCallbackStarted(void* user_data)
{
    UpdateUntilCallbackStartedContext* context = (UpdateUntilCallbackStartedContext*)user_data;

    while (!dmAtomicGet32(&context->m_JobState->m_CallbackStarted) && !dmAtomicGet32(&context->m_StopUpdateThread))
    {
        JobSystemUpdate(context->m_JobSystem, 0);
        dmTime::Sleep(TEST_SLEEP_US);
    }
}

// If a concurrent JobSystemUpdate() has already entered the callback,
// no-callback cancellation must still report PENDING until the callback returns.
TEST_P(dmJobSystemTest, CancelFinishedJobNoCallbackDuringCallbackReportsPending)
{
    if (GetParam().m_NumThreads == 0)
    {
        return;
    }

    ControlledJobState job_state;
    InitControlledJobState(&job_state, true, false);

    HJob hjob = CreateControlledJob(m_JobSystem, &job_state);
    ASSERT_NE((HJob)0, hjob);
    ASSERT_EQ(JOBSYSTEM_RESULT_OK, JobSystemPushJob(m_JobSystem, hjob));

    UpdateUntilCallbackStartedContext update_context = {0};
    update_context.m_JobSystem = m_JobSystem;
    update_context.m_JobState = &job_state;

    dmThread::Thread update_thread = dmThread::New(UpdateUntilCallbackStarted, 0x80000, &update_context, "jobupd");
    ASSERT_NE((dmThread::Thread)0, update_thread);

    if (!WaitForAtomicValue(&job_state.m_CallbackStarted, 1, "job callback start"))
    {
        dmAtomicStore32(&update_context.m_StopUpdateThread, 1);
        AllowCallbackFinish(&job_state);
        dmThread::Join(update_thread);
        ASSERT_EQ(1, dmAtomicGet32(&job_state.m_CallbackStarted));
    }

    JobSystemResult cancel_result = JobSystemCancelJobNoCallback(m_JobSystem, hjob);
    int callback_finished_before_release = dmAtomicGet32(&job_state.m_CallbackFinished);

    AllowCallbackFinish(&job_state);
    dmThread::Join(update_thread);

    ASSERT_EQ(JOBSYSTEM_RESULT_PENDING, cancel_result);
    ASSERT_EQ(0, callback_finished_before_release);
    ASSERT_EQ(1, dmAtomicGet32(&job_state.m_CallbackFinished));
    ASSERT_EQ(JOBSYSTEM_STATUS_FINISHED, job_state.m_CallbackStatus);
}

struct ParentWaitForChildCallbackContext
{
    HJobContext    m_JobSystem;
    ControlledJobState m_Parent;
    ControlledJobState m_Child;
    int32_atomic_t m_StopUpdateThread;
};

static int32_t ProcessParentAfterChildCallbackStarted(HJobContext ctx, HJob job, void* user_context, void* user_data)
{
    (void)ctx;
    (void)job;
    (void)user_data;

    ParentWaitForChildCallbackContext* context = (ParentWaitForChildCallbackContext*)user_context;
    dmAtomicIncrement32(&context->m_Parent.m_ProcessStarted);

    while (!dmAtomicGet32(&context->m_Child.m_CallbackStarted) && !dmAtomicGet32(&context->m_StopUpdateThread))
    {
        dmTime::Sleep(TEST_SLEEP_US);
    }

    while (!dmAtomicGet32(&context->m_Parent.m_AllowProcessFinish))
    {
        dmTime::Sleep(TEST_SLEEP_US);
    }

    dmAtomicIncrement32(&context->m_Parent.m_ProcessFinished);
    return context->m_Parent.m_ProcessResult;
}

static void UpdateUntilChildCallbackStarted(void* user_data)
{
    ParentWaitForChildCallbackContext* context = (ParentWaitForChildCallbackContext*)user_data;

    while (!dmAtomicGet32(&context->m_Child.m_CallbackStarted) && !dmAtomicGet32(&context->m_StopUpdateThread))
    {
        JobSystemUpdate(context->m_JobSystem, 0);
        dmTime::Sleep(TEST_SLEEP_US);
    }
}

// Normal cancellation of a finished parent does not wait for child callbacks.
// Callback cleanup remains owned by the callbacks themselves.
TEST_P(dmJobSystemTest, CancelFinishedParentDuringChildCallbackReportsOk)
{
    if (GetParam().m_NumThreads == 0)
    {
        return;
    }

    ParentWaitForChildCallbackContext context = {0};
    context.m_JobSystem = m_JobSystem;
    InitControlledJobState(&context.m_Parent, false, true);
    InitControlledJobState(&context.m_Child, true, false);

    Job parent_job = {0};
    parent_job.m_Process = ProcessParentAfterChildCallbackStarted;
    parent_job.m_Context = &context;

    HJob parent_hjob = JobSystemCreateJob(m_JobSystem, &parent_job);
    ASSERT_NE((HJob)0, parent_hjob);

    HJob child_hjob = CreateControlledJob(m_JobSystem, &context.m_Child);
    ASSERT_NE((HJob)0, child_hjob);
    ASSERT_EQ(JOBSYSTEM_RESULT_OK, JobSystemSetParent(m_JobSystem, child_hjob, parent_hjob));

    ASSERT_EQ(JOBSYSTEM_RESULT_OK, JobSystemPushJob(m_JobSystem, child_hjob));
    ASSERT_EQ(JOBSYSTEM_RESULT_OK, JobSystemPushJob(m_JobSystem, parent_hjob));

    dmThread::Thread update_thread = dmThread::New(UpdateUntilChildCallbackStarted, 0x80000, &context, "jobupd");
    ASSERT_NE((dmThread::Thread)0, update_thread);

    if (!WaitForAtomicValue(&context.m_Child.m_CallbackStarted, 1, "child callback start"))
    {
        dmAtomicStore32(&context.m_StopUpdateThread, 1);
        AllowProcessFinish(&context.m_Parent);
        AllowCallbackFinish(&context.m_Child);
        dmThread::Join(update_thread);
        ASSERT_EQ(1, dmAtomicGet32(&context.m_Child.m_CallbackStarted));
    }

    if (!WaitForAtomicValue(&context.m_Parent.m_ProcessStarted, 1, "parent process start"))
    {
        AllowProcessFinish(&context.m_Parent);
        AllowCallbackFinish(&context.m_Child);
        dmThread::Join(update_thread);
        ASSERT_EQ(1, dmAtomicGet32(&context.m_Parent.m_ProcessStarted));
    }

    AllowProcessFinish(&context.m_Parent);

    if (!WaitForAtomicValue(&context.m_Parent.m_ProcessFinished, 1, "parent process finish"))
    {
        AllowCallbackFinish(&context.m_Child);
        dmThread::Join(update_thread);
        ASSERT_EQ(1, dmAtomicGet32(&context.m_Parent.m_ProcessFinished));
    }

    JobSystemResult cancel_result = JobSystemCancelJob(m_JobSystem, parent_hjob);
    int callback_finished_before_release = dmAtomicGet32(&context.m_Child.m_CallbackFinished);

    AllowCallbackFinish(&context.m_Child);
    dmThread::Join(update_thread);

    ASSERT_EQ(JOBSYSTEM_RESULT_OK, cancel_result);
    ASSERT_EQ(0, callback_finished_before_release);
    ASSERT_EQ(1, dmAtomicGet32(&context.m_Child.m_CallbackFinished));
    ASSERT_EQ(JOBSYSTEM_STATUS_FINISHED, context.m_Child.m_CallbackStatus);
}

// No-callback cancellation of a finished parent must also suppress finished
// child callbacks that are still waiting in the done queue.
TEST_P(dmJobSystemTest, CancelFinishedParentNoCallbackBeforeUpdateSkipsChildCallbacks)
{
    if (GetParam().m_NumThreads == 0)
    {
        return;
    }

    ControlledJobState parent_state;
    ControlledJobState child_state;
    InitControlledJobState(&parent_state, true, true);
    InitControlledJobState(&child_state, false, true);

    HJob parent_hjob = CreateControlledJob(m_JobSystem, &parent_state);
    ASSERT_NE((HJob)0, parent_hjob);

    HJob child_hjob = CreateControlledJob(m_JobSystem, &child_state);
    ASSERT_NE((HJob)0, child_hjob);
    ASSERT_EQ(JOBSYSTEM_RESULT_OK, JobSystemSetParent(m_JobSystem, child_hjob, parent_hjob));

    ASSERT_EQ(JOBSYSTEM_RESULT_OK, JobSystemPushJob(m_JobSystem, child_hjob));
    ASSERT_EQ(JOBSYSTEM_RESULT_OK, JobSystemPushJob(m_JobSystem, parent_hjob));

    ASSERT_TRUE(WaitForAtomicValue(&child_state.m_ProcessStarted, 1, "child process start"));

    AllowProcessFinish(&child_state);

    ASSERT_TRUE(WaitForAtomicValue(&parent_state.m_ProcessStarted, 1, "parent process start"));

    JobSystemResult result = PollCancelUntilTerminal(m_JobSystem, parent_hjob, true);

    ASSERT_EQ(JOBSYSTEM_RESULT_OK, result);
    ASSERT_EQ(0, dmAtomicGet32(&child_state.m_CallbackFinished));
    ASSERT_EQ(0, dmAtomicGet32(&parent_state.m_CallbackFinished));

    JobSystemUpdate(m_JobSystem, 0);
    ASSERT_EQ(0, dmAtomicGet32(&child_state.m_CallbackFinished));
    ASSERT_EQ(0, dmAtomicGet32(&parent_state.m_CallbackFinished));
}

// No-callback cancellation must traverse children even when the parent itself is
// already in flight, otherwise finished child callbacks may still run.
TEST_P(dmJobSystemTest, CancelProcessingParentNoCallbackSkipsFinishedChildCallback)
{
    if (GetParam().m_NumThreads == 0)
    {
        return;
    }

    ControlledJobState parent_state;
    ControlledJobState child_state;
    InitControlledJobState(&parent_state, false, true);
    InitControlledJobState(&child_state, false, true);

    HJob parent_hjob = CreateControlledJob(m_JobSystem, &parent_state);
    ASSERT_NE((HJob)0, parent_hjob);

    HJob child_hjob = CreateControlledJob(m_JobSystem, &child_state);
    ASSERT_NE((HJob)0, child_hjob);
    ASSERT_EQ(JOBSYSTEM_RESULT_OK, JobSystemSetParent(m_JobSystem, child_hjob, parent_hjob));

    ASSERT_EQ(JOBSYSTEM_RESULT_OK, JobSystemPushJob(m_JobSystem, child_hjob));
    ASSERT_EQ(JOBSYSTEM_RESULT_OK, JobSystemPushJob(m_JobSystem, parent_hjob));

    ASSERT_TRUE(WaitForAtomicValue(&child_state.m_ProcessStarted, 1, "child process start"));

    AllowProcessFinish(&child_state);

    if (!WaitForAtomicValue(&parent_state.m_ProcessStarted, 1, "parent process start"))
    {
        AllowProcessFinish(&parent_state);
        ASSERT_EQ(1, dmAtomicGet32(&parent_state.m_ProcessStarted));
    }

    JobSystemResult first_cancel_result = JobSystemCancelJobNoCallback(m_JobSystem, parent_hjob);

    JobSystemUpdate(m_JobSystem, 0);
    int child_callback_after_cancel = dmAtomicGet32(&child_state.m_CallbackFinished);

    AllowProcessFinish(&parent_state);

    JobSystemResult result = PollCancelUntilTerminal(m_JobSystem, parent_hjob, true);

    JobSystemUpdate(m_JobSystem, 0);

    ASSERT_EQ(JOBSYSTEM_RESULT_PENDING, first_cancel_result);
    ASSERT_EQ(JOBSYSTEM_RESULT_OK, result);
    ASSERT_EQ(0, child_callback_after_cancel);
    ASSERT_EQ(0, dmAtomicGet32(&child_state.m_CallbackFinished));
    ASSERT_EQ(0, dmAtomicGet32(&parent_state.m_CallbackFinished));
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

static int32_t ProcessSortedDependencyJobs(HJobContext ctx, HJob hjob, void* user_context, void* user_data)
{
    JobWithDependency* data = (JobWithDependency*)user_data;
    dmTime::Sleep(data->m_Sleep);
    data->m_ProcessingOrder = dmAtomicIncrement32(data->m_Order);
    printf("job%d: process: order %d  %p\n", data->m_Index, data->m_ProcessingOrder, (void*)(uintptr_t)hjob);
    return 1;
}

static void CallbackSortedDependencyJobs(HJobContext ctx, HJob hjob, JobSystemStatus status, void* user_context, void* user_data, int32_t user_result)
{
    uint32_t* count_finished = (uint32_t*)user_context;
    JobWithDependency* data = (JobWithDependency*)user_data;
    data->m_FinishingOrder = dmAtomicIncrement32(data->m_Order);
    printf("job%d: finish: order %d\n", data->m_Index, data->m_FinishingOrder);
    (*count_finished)++;
}

// Make sure they all children are processed before their parents
TEST_P(dmJobSystemTest, SortedDependencyJobs)
{
    const uint32_t      job_count = 7;
    JobWithDependency   items[job_count];
    HJob   hjobs[job_count];
    memset(items, 0, sizeof(items));

    int32_atomic_t order = 0;
    uint32_t count_finished = 0;

    for (uint32_t i = 0; i < job_count; ++i)
    {
        items[i].m_Index = (int)i;
        items[i].m_Sleep = rand() % 1000;
        items[i].m_Order = &order;
        printf("job%d: init: sleep: %d\n", (int)i, (int)items[i].m_Sleep);

        Job job = {0};
        job.m_Process = ProcessSortedDependencyJobs;
        job.m_Callback = CallbackSortedDependencyJobs;
        job.m_Context = (void*)&count_finished;
        job.m_Data    = (void*)&items[i];

        hjobs[i] = JobSystemCreateJob(m_JobSystem, &job);
    }

    // Setup the dependencies
    // [      3      ]
    // [  1       5  ]
    // [0   2   4   6]
    JobSystemSetParent(m_JobSystem, hjobs[0], hjobs[1]);
    JobSystemSetParent(m_JobSystem, hjobs[2], hjobs[1]);
    JobSystemSetParent(m_JobSystem, hjobs[4], hjobs[5]);
    JobSystemSetParent(m_JobSystem, hjobs[6], hjobs[5]);

    JobSystemSetParent(m_JobSystem, hjobs[5], hjobs[3]);
    JobSystemSetParent(m_JobSystem, hjobs[1], hjobs[3]);

    JobSystemResult result;

    for (uint32_t i = 0; i < job_count; ++i)
    {
        result = JobSystemPushJob(m_JobSystem, hjobs[i]);
        ASSERT_EQ(JOBSYSTEM_RESULT_OK, result);
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

        JobSystemUpdate(m_JobSystem, 500);

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
    JobSystemStatus                 m_CallbackStatus;
    HJob                            m_Job;
};

struct CancelParentTrack
{
    int32_atomic_t                  m_ProcessCalled;
    int32_atomic_t                  m_CallbackCalled;
    JobSystemStatus                 m_CallbackStatus;
};

int32_atomic_t g_CancelParentGlobalLock = 0;

static int32_t ProcessCancelChild(HJobContext ctx, HJob job, void* user_context, void* user_data)
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

static void CallbackCancelChild(HJobContext ctx, HJob job, JobSystemStatus status, void* user_context, void* user_data, int32_t user_result)
{
    CancelChildTrack* child = (CancelChildTrack*)user_context;
    child->m_CallbackStatus = status;
    child->m_Job = job;
    dmAtomicIncrement32(&child->m_CallbackCalled);
}

static int32_t ProcessCancelParent(HJobContext ctx, HJob job, void* user_context, void* user_data)
{
    CancelParentTrack* parent = (CancelParentTrack*)user_data;
    dmAtomicIncrement32(&parent->m_ProcessCalled);
    return 1;
}

static void CallbackCancelParent(HJobContext ctx, HJob job, JobSystemStatus status, void* user_context, void* user_data, int32_t user_result)
{
    CancelParentTrack* parent = (CancelParentTrack*)user_context;
    parent->m_CallbackStatus = status;
    dmAtomicIncrement32(&parent->m_CallbackCalled);
}

// This tests that cancelling the parent after one of the children (not the last one) has finished.
// doesn't mess up the internal list of children.
TEST_P(dmJobSystemTest, CancelParentAfterChild)
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
        children[i].m_CallbackStatus = JOBSYSTEM_STATUS_FREE;

        // Making sure some doesn't finish, to test the cancelling of the parent
        if (m_NumThreads == 1)
            children[i].m_Delay = i < MID_INDEX;
        else
            children[i].m_Delay = i != MID_INDEX;
    }

    CancelParentTrack parent = {0};
    parent.m_CallbackStatus = JOBSYSTEM_STATUS_FREE;

    Job parent_job = {0};
    parent_job.m_Process = ProcessCancelParent;
    parent_job.m_Callback = CallbackCancelParent;
    parent_job.m_Context = &parent;
    parent_job.m_Data = &parent;

    HJob parent_hjob = JobSystemCreateJob(m_JobSystem, &parent_job);
    ASSERT_NE((HJob)0, parent_hjob);

    HJob child_hjobs[CHILD_COUNT];
    for (uint32_t i = 0; i < CHILD_COUNT; ++i)
    {
        Job child_job = {0};
        child_job.m_Process = ProcessCancelChild;
        child_job.m_Callback = CallbackCancelChild;
        child_job.m_Context = &children[i];
        child_job.m_Data = &children[i];

        child_hjobs[i] = JobSystemCreateJob(m_JobSystem, &child_job);
        ASSERT_NE((HJob)0, child_hjobs[i]);
        ASSERT_EQ(JOBSYSTEM_RESULT_OK, JobSystemSetParent(m_JobSystem, child_hjobs[i], parent_hjob));
        ASSERT_EQ(JOBSYSTEM_RESULT_OK, JobSystemPushJob(m_JobSystem, child_hjobs[i]));
    }

    ASSERT_EQ(JOBSYSTEM_RESULT_OK, JobSystemPushJob(m_JobSystem, parent_hjob));

    bool mid_finished = false;
    uint64_t finish_limit = dmTime::GetMonotonicTime() + 500000;
    while (!mid_finished)
    {
        JobSystemUpdate(m_JobSystem, 0);
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
    ASSERT_EQ(JOBSYSTEM_STATUS_FINISHED, children[MID_INDEX].m_CallbackStatus);

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

    JobSystemResult result;
    do
    {
        result = JobSystemCancelJob(m_JobSystem, parent_hjob);

        JobSystemUpdate(m_JobSystem, 1000);
        dmTime::Sleep(1);
    } while (JOBSYSTEM_RESULT_PENDING == result);

    bool all_callbacks_received = false;
    stop_time = dmTime::GetMonotonicTime() + 500000;
    while (dmTime::GetMonotonicTime() < stop_time && !all_callbacks_received)
    {
        JobSystemUpdate(m_JobSystem, 1000);
        dmTime::Sleep(1);

        if (dmAtomicGet32(&children[MID_INDEX].m_CallbackCalled) && dmAtomicGet32(&parent.m_CallbackCalled))
        {
            all_callbacks_received = true;
            break;
        }
    }

    ASSERT_TRUE(all_callbacks_received);
    ASSERT_EQ(0, dmAtomicGet32(&parent.m_ProcessCalled));
    ASSERT_EQ(JOBSYSTEM_STATUS_CANCELED, parent.m_CallbackStatus);

    for (uint32_t i = 0; i < CHILD_COUNT; ++i)
    {
        if (m_NumThreads == 1)
        {
            if (i <= MID_INDEX)
            {
                ASSERT_EQ(JOBSYSTEM_STATUS_FINISHED, children[i].m_CallbackStatus);
            }
            else
            {
                ASSERT_EQ(JOBSYSTEM_STATUS_CANCELED, children[i].m_CallbackStatus);
                ASSERT_EQ(0, dmAtomicGet32(&children[i].m_ProcessCalled));
            }
        }
        else
        {
            if (i == MID_INDEX)
            {
                ASSERT_EQ(JOBSYSTEM_STATUS_FINISHED, children[i].m_CallbackStatus);
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

INSTANTIATE_TEST_CASE_P(dmJobSystemTest, dmJobSystemTest, jc_test_values_in(test_setups));

int main(int argc, char **argv)
{
    dmLog::LogParams params;
    dmLog::LogInitialize(&params);

    jc_test_init(&argc, argv);
    int result = jc_test_run_all();

    dmLog::LogFinalize();
    return result;
}
