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

#define JC_TEST_IMPLEMENTATION
#include <jc_test/jc_test.h>

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include <dlib/condition_variable.h>
#include <dlib/mutex.h>
#include <dlib/thread.h>
#include <testmain/testmain.h>

#include "render/render_frame_queue.h"
#include "render/render_frame_queue_private.h"
#include "render_frame_benchmark.h"

using namespace dmRender;

static const uint32_t MAX_TEST_FRAMES = 32;
static const uint32_t SAMPLE_TABLE_TYPE = 17;

struct SampleInput
{
    uint32_t m_Sequence;
    uint32_t m_Values[4];
};

// Lives until the executor is joined and all frames are retired. Only the consumer
// modifies observations, under this mutex; release callbacks run on the game owner.
struct QueueProbe
{
    HRenderExecution m_Execution;
    dmMutex::HMutex m_Mutex;
    dmConditionVariable::HConditionVariable m_Changed;
    SampleInput m_Inputs[MAX_TEST_FRAMES];
    RenderFrameId m_FrameIds[MAX_TEST_FRAMES];
    uint32_t m_ReleaseOrder[MAX_TEST_FRAMES];
    uint32_t m_Consumed;
    uint32_t m_Released;
    uint32_t m_ServiceCount;
    bool m_WrongOwner;
    bool m_InvalidInput;
    bool m_HoldFirstReader;
    bool m_FirstReaderEntered;
    bool m_ReleaseFirstReader;
    bool m_HoldFirstService;
    bool m_FirstServiceEntered;
    bool m_ReleaseFirstService;
    bool m_CopyUpload;
    SampleInput* m_Upload;
    const uint8_t* m_FirstPayload;
    bool m_ReusedPayload;
};

static void ConsumeFrame(void* context, const RenderFrameView& frame)
{
    QueueProbe* probe = (QueueProbe*)context;
    dmMutex::Lock(probe->m_Mutex);
    probe->m_WrongOwner |= !IsRenderExecutionOwner(probe->m_Execution, EXECUTION_OWNER_RENDER);
    if (probe->m_Consumed == 0 && probe->m_HoldFirstReader)
    {
        probe->m_FirstReaderEntered = true;
        dmConditionVariable::Broadcast(probe->m_Changed);
        while (!probe->m_ReleaseFirstReader)
            dmConditionVariable::Wait(probe->m_Changed, probe->m_Mutex);
    }

    uint32_t index = probe->m_Consumed++;
    bool valid = index < MAX_TEST_FRAMES && frame.m_Statistics.m_TableCount == 1 &&
        frame.m_Tables[0].m_Type == SAMPLE_TABLE_TYPE && frame.m_Tables[0].m_Count == 1 &&
        frame.m_Tables[0].m_Stride == sizeof(SampleInput);
    probe->m_InvalidInput |= !valid;
    if (valid)
    {
        const SampleInput* input = (const SampleInput*)GetRenderFrameTableData(frame, 0);
        probe->m_Inputs[index] = *input;
        probe->m_FrameIds[index] = frame.m_FrameId;
        if (index == 0)
        {
            probe->m_FirstPayload = frame.m_Payload;
            if (probe->m_CopyUpload)
            {
                // A simulated asynchronous upload explicitly owns its own copy. It
                // intentionally survives both CPU slot retirement and queue deletion.
                probe->m_Upload = (SampleInput*)malloc(sizeof(SampleInput));
                assert(probe->m_Upload);
                *probe->m_Upload = *input;
            }
        }
        else
        {
            probe->m_ReusedPayload |= probe->m_FirstPayload == frame.m_Payload;
        }
    }
    dmConditionVariable::Broadcast(probe->m_Changed);
    dmMutex::Unlock(probe->m_Mutex);
}

static void ServiceOwner(void* context)
{
    QueueProbe* probe = (QueueProbe*)context;
    dmMutex::Lock(probe->m_Mutex);
    probe->m_WrongOwner |= !IsRenderExecutionOwner(probe->m_Execution, EXECUTION_OWNER_RENDER);
    ++probe->m_ServiceCount;
    if (probe->m_ServiceCount == 1 && probe->m_HoldFirstService)
    {
        probe->m_FirstServiceEntered = true;
        dmConditionVariable::Broadcast(probe->m_Changed);
        while (!probe->m_ReleaseFirstService)
            dmConditionVariable::Wait(probe->m_Changed, probe->m_Mutex);
    }
    dmConditionVariable::Broadcast(probe->m_Changed);
    dmMutex::Unlock(probe->m_Mutex);
}

static void ReleaseReference(void* context, void* resource)
{
    QueueProbe* probe = (QueueProbe*)context;
    dmMutex::Lock(probe->m_Mutex);
    probe->m_WrongOwner |= !IsRenderExecutionOwner(probe->m_Execution, EXECUTION_OWNER_GAME);
    assert(probe->m_Released < MAX_TEST_FRAMES);
    probe->m_ReleaseOrder[probe->m_Released++] = *(uint32_t*)resource;
    dmMutex::Unlock(probe->m_Mutex);
}

struct QueueFixture
{
    HRenderExecution m_Execution;
    HRenderFrameQueue m_Queue;
    QueueProbe m_Probe;
};

static bool HasThreadedExecution()
{
    ExecutionCapabilities capabilities = GetExecutionCapabilities(true);
    return capabilities.m_BuildEnabled && capabilities.m_ThreadsSupported;
}

static void CreateFixture(QueueFixture* fixture, ExecutionMode mode)
{
    memset(fixture, 0, sizeof(*fixture));
    ExecutionSelection selection;
    fixture->m_Execution = NewRenderExecution(mode, true, &selection);
    assert(fixture->m_Execution && selection.m_ExecutionMode == mode);
    fixture->m_Probe.m_Execution = fixture->m_Execution;
    fixture->m_Probe.m_Mutex = dmMutex::New();
    fixture->m_Probe.m_Changed = dmConditionVariable::New();
    RenderFrameQueueParams params = {};
    params.m_Execution = fixture->m_Execution;
    params.m_Limits.m_PayloadBytes = 256;
    params.m_Limits.m_MaxTables = 4;
    params.m_Limits.m_MaxReferences = 4;
    params.m_Consume = ConsumeFrame;
    params.m_ConsumeContext = &fixture->m_Probe;
    params.m_Service = ServiceOwner;
    params.m_ServiceContext = &fixture->m_Probe;
    fixture->m_Queue = NewRenderFrameQueue(params);
    assert(fixture->m_Queue);
}

// Call only after Stop/drain and executor join. Upload ownership is intentionally
// separate, so each test that requested an upload copy explicitly frees it.
static void DeleteFixture(QueueFixture* fixture)
{
    RetireRenderFrames(fixture->m_Queue);
    DeleteRenderFrameQueue(fixture->m_Queue);
    DeleteRenderExecution(fixture->m_Execution);
    dmConditionVariable::Delete(fixture->m_Probe.m_Changed);
    dmMutex::Delete(fixture->m_Probe.m_Mutex);
}

// A deliberately small native executor; production platform executors are out of
// scope. The initial gate makes Ready/Reading state tests independent of timing.
struct TestExecutor
{
    QueueFixture* m_Fixture;
    dmThread::Thread m_Thread;
    dmMutex::HMutex m_Mutex;
    dmConditionVariable::HConditionVariable m_Changed;
    bool m_Started;
    bool m_Bound;
    bool m_Run;
    bool m_Released;
};

static void RunExecutor(void* context)
{
    TestExecutor* executor = (TestExecutor*)context;
    executor->m_Bound = BindRenderExecutionOwner(executor->m_Fixture->m_Execution);
    dmMutex::Lock(executor->m_Mutex);
    executor->m_Started = true;
    dmConditionVariable::Broadcast(executor->m_Changed);
    while (!executor->m_Run)
        dmConditionVariable::Wait(executor->m_Changed, executor->m_Mutex);
    dmMutex::Unlock(executor->m_Mutex);
    assert(executor->m_Bound);
    for (;;)
    {
        RenderFramePumpResult result = PumpRenderFrameQueue(executor->m_Fixture->m_Queue, true);
        if (result == RENDER_FRAME_PUMP_DRAINED)
            break;
        if (result == RENDER_FRAME_PUMP_IDLE || result == RENDER_FRAME_PUMP_BUSY)
            WaitForRenderFrameWork(executor->m_Fixture->m_Queue);
    }
    ReleaseRenderExecutionOwner(executor->m_Fixture->m_Execution);
    executor->m_Released = !IsRenderExecutionOwner(executor->m_Fixture->m_Execution, EXECUTION_OWNER_RENDER);
}

static void StartExecutor(TestExecutor* executor, QueueFixture* fixture, bool run)
{
    memset(executor, 0, sizeof(*executor));
    executor->m_Fixture = fixture;
    executor->m_Run = run;
    executor->m_Mutex = dmMutex::New();
    executor->m_Changed = dmConditionVariable::New();
    executor->m_Thread = dmThread::New(RunExecutor, 0x80000, executor, "frame-reader");
    dmMutex::Lock(executor->m_Mutex);
    while (!executor->m_Started)
        dmConditionVariable::Wait(executor->m_Changed, executor->m_Mutex);
    dmMutex::Unlock(executor->m_Mutex);
    assert(executor->m_Bound);
}

static void ResumeExecutor(TestExecutor* executor)
{
    dmMutex::Lock(executor->m_Mutex);
    executor->m_Run = true;
    dmConditionVariable::Broadcast(executor->m_Changed);
    dmMutex::Unlock(executor->m_Mutex);
}

static void JoinExecutor(TestExecutor* executor)
{
    dmThread::Join(executor->m_Thread);
    assert(executor->m_Released);
    dmConditionVariable::Delete(executor->m_Changed);
    dmMutex::Delete(executor->m_Mutex);
}

static SampleInput MakeSample(uint32_t sequence)
{
    SampleInput sample;
    sample.m_Sequence = sequence;
    for (uint32_t i = 0; i < 4; ++i)
        sample.m_Values[i] = sequence * 10 + i;
    return sample;
}

static RenderFrameResult CaptureSample(const RenderFrameTicket& ticket, uint32_t sequence)
{
    SampleInput source = MakeSample(sequence);
    uint32_t index;
    RenderFrameResult result = CopyRenderFrameTable(ticket.m_Frame, SAMPLE_TABLE_TYPE, 1,
        sizeof(source), 4, &source, &index);
    // The local source expires before publication; no borrowed pointer may escape.
    memset(&source, 0xee, sizeof(source));
    return result;
}

static void WaitForConsumed(QueueFixture* fixture, uint32_t count)
{
    for (;;)
    {
        RenderFrameQueueStatus status = GetRenderFrameQueueStatus(fixture->m_Queue);
        if (status.m_Consumed >= count)
            return;
        WaitForRenderFrameProgress(fixture->m_Queue, status.m_Revision);
    }
}

static RenderFrameResult BeginWhenAvailable(QueueFixture* fixture, RenderFrameTicket* ticket)
{
    for (;;)
    {
        RenderFrameQueueStatus status = GetRenderFrameQueueStatus(fixture->m_Queue);
        RetireRenderFrames(fixture->m_Queue);
        RenderFrameResult result = BeginRenderFrameCapture(fixture->m_Queue, ticket);
        if (result != RENDER_FRAME_WOULD_BLOCK)
            return result;
        WaitForRenderFrameProgress(fixture->m_Queue, status.m_Revision);
    }
}

static RenderFrameResult PublishWhenAvailable(QueueFixture* fixture, const RenderFrameTicket& ticket)
{
    for (;;)
    {
        RenderFrameQueueStatus status = GetRenderFrameQueueStatus(fixture->m_Queue);
        RenderFrameResult result = PublishRenderFrame(fixture->m_Queue, ticket);
        if (result != RENDER_FRAME_WOULD_BLOCK)
            return result;
        WaitForRenderFrameProgress(fixture->m_Queue, status.m_Revision);
    }
}

static void WaitForFirstReader(QueueProbe* probe)
{
    dmMutex::Lock(probe->m_Mutex);
    while (!probe->m_FirstReaderEntered)
        dmConditionVariable::Wait(probe->m_Changed, probe->m_Mutex);
    dmMutex::Unlock(probe->m_Mutex);
}

static void ReleaseFirstReader(QueueProbe* probe)
{
    dmMutex::Lock(probe->m_Mutex);
    probe->m_ReleaseFirstReader = true;
    dmConditionVariable::Broadcast(probe->m_Changed);
    dmMutex::Unlock(probe->m_Mutex);
}

// Verify inline publishes the immutable copy immediately but retires references later,
// exactly once, in reverse retention order on the game owner.
TEST(RenderFrameQueue, InlineCaptureAndRetirement)
{
    QueueFixture fixture;
    CreateFixture(&fixture, EXECUTION_MODE_INLINE);
    ASSERT_EQ((dmMutex::HMutex)0, fixture.m_Queue->m_Mutex);
    ASSERT_EQ((dmConditionVariable::HConditionVariable)0, fixture.m_Queue->m_Changed);
    RenderFrameTicket ticket;
    ASSERT_EQ(RENDER_FRAME_OK, BeginRenderFrameCapture(fixture.m_Queue, &ticket));
    ASSERT_EQ(RENDER_FRAME_OK, CaptureSample(ticket, 3));
    uint32_t resources[] = { 11, 12 };
    ASSERT_EQ(RENDER_FRAME_OK, AddRenderFrameReference(ticket.m_Frame, &resources[0], ReleaseReference, &fixture.m_Probe));
    ASSERT_EQ(RENDER_FRAME_OK, AddRenderFrameReference(ticket.m_Frame, &resources[1], ReleaseReference, &fixture.m_Probe));
    ASSERT_EQ(RENDER_FRAME_OK, PublishRenderFrame(fixture.m_Queue, ticket));
    ASSERT_EQ(1u, fixture.m_Probe.m_Consumed);
    ASSERT_EQ(3u, fixture.m_Probe.m_Inputs[0].m_Sequence);
    ASSERT_EQ(33u, fixture.m_Probe.m_Inputs[0].m_Values[3]);
    ASSERT_EQ(ticket.m_FrameId, fixture.m_Probe.m_FrameIds[0]);
    ASSERT_EQ(0u, fixture.m_Probe.m_Released);
    ASSERT_EQ(1u, RetireRenderFrames(fixture.m_Queue));
    ASSERT_EQ(0u, RetireRenderFrames(fixture.m_Queue));
    ASSERT_EQ(2u, fixture.m_Probe.m_Released);
    ASSERT_EQ(12u, fixture.m_Probe.m_ReleaseOrder[0]);
    ASSERT_EQ(11u, fixture.m_Probe.m_ReleaseOrder[1]);
    StopRenderFrameQueue(fixture.m_Queue);
    ASSERT_EQ(RENDER_FRAME_PUMP_DRAINED, PumpRenderFrameQueue(fixture.m_Queue, true));
    ASSERT_FALSE(fixture.m_Probe.m_WrongOwner);
    ASSERT_FALSE(fixture.m_Probe.m_InvalidInput);
    DeleteFixture(&fixture);
}

// Verify inline and a distinct native owner consume identical snapshots and retire the
// same references; frame source lifetime is independent of the chosen execution mode.
TEST(RenderFrameQueue, InlineAndThreadedEquivalent)
{
    if (!HasThreadedExecution())
        return;
    SampleInput inputs[2][3];
    uint32_t release_order[2][3];
    for (uint32_t mode = 0; mode < 2; ++mode)
    {
        QueueFixture fixture;
        CreateFixture(&fixture, mode == 0 ? EXECUTION_MODE_INLINE : EXECUTION_MODE_THREADED);
        TestExecutor executor;
        if (mode == 1)
            StartExecutor(&executor, &fixture, true);
        uint32_t resources[] = { 1, 2, 3 };
        for (uint32_t i = 0; i < 3; ++i)
        {
            RenderFrameTicket ticket;
            ASSERT_EQ(RENDER_FRAME_OK, BeginWhenAvailable(&fixture, &ticket));
            ASSERT_EQ(RENDER_FRAME_OK, CaptureSample(ticket, i + 1));
            ASSERT_EQ(RENDER_FRAME_OK, AddRenderFrameReference(ticket.m_Frame, &resources[i], ReleaseReference, &fixture.m_Probe));
            ASSERT_EQ(RENDER_FRAME_OK, PublishWhenAvailable(&fixture, ticket));
            WaitForConsumed(&fixture, i + 1);
            RetireRenderFrames(fixture.m_Queue);
        }
        StopRenderFrameQueue(fixture.m_Queue);
        if (mode == 1)
            JoinExecutor(&executor);
        else
            ASSERT_EQ(RENDER_FRAME_PUMP_DRAINED, PumpRenderFrameQueue(fixture.m_Queue, true));
        ASSERT_EQ(3u, fixture.m_Probe.m_Consumed);
        ASSERT_EQ(3u, fixture.m_Probe.m_Released);
        ASSERT_FALSE(fixture.m_Probe.m_WrongOwner);
        ASSERT_FALSE(fixture.m_Probe.m_InvalidInput);
        memcpy(inputs[mode], fixture.m_Probe.m_Inputs, sizeof(inputs[mode]));
        memcpy(release_order[mode], fixture.m_Probe.m_ReleaseOrder, sizeof(release_order[mode]));
        DeleteFixture(&fixture);
    }
    ASSERT_EQ(0, memcmp(inputs[0], inputs[1], sizeof(inputs[0])));
    ASSERT_EQ(0, memcmp(release_order[0], release_order[1], sizeof(release_order[0])));
}

// Verify a held reader permits exactly one next capture, but neither a second capture
// nor publication can overtake it; publication makes the completed copy visible.
TEST(RenderFrameQueue, SlowReaderBoundsProducerLead)
{
    if (!HasThreadedExecution())
        return;
    QueueFixture fixture;
    CreateFixture(&fixture, EXECUTION_MODE_THREADED);
    fixture.m_Probe.m_HoldFirstReader = true;
    TestExecutor executor;
    StartExecutor(&executor, &fixture, true);
    RenderFrameTicket first, second, extra;
    ASSERT_EQ(RENDER_FRAME_OK, BeginRenderFrameCapture(fixture.m_Queue, &first));
    ASSERT_EQ(RENDER_FRAME_OK, CaptureSample(first, 7));
    ASSERT_EQ(RENDER_FRAME_OK, PublishRenderFrame(fixture.m_Queue, first));
    ASSERT_EQ(RENDER_FRAME_INVALID_STATE, CancelRenderFrameCapture(fixture.m_Queue, first));
    WaitForFirstReader(&fixture.m_Probe);
    ASSERT_EQ(RENDER_FRAME_OK, BeginRenderFrameCapture(fixture.m_Queue, &second));
    ASSERT_NE(first.m_Frame, second.m_Frame);
    SampleInput source = MakeSample(8);
    uint32_t table_index;
    ASSERT_EQ(RENDER_FRAME_OK, CopyRenderFrameTable(second.m_Frame, SAMPLE_TABLE_TYPE, 1, sizeof(source), 4, &source, &table_index));
    memset(&source, 0xff, sizeof(source));
    ASSERT_EQ(RENDER_FRAME_WOULD_BLOCK, PublishRenderFrame(fixture.m_Queue, second));
    ASSERT_EQ(RENDER_FRAME_WOULD_BLOCK, BeginRenderFrameCapture(fixture.m_Queue, &extra));
    ASSERT_EQ(0u, RetireRenderFrames(fixture.m_Queue));
    RenderFrameQueueStatus held = GetRenderFrameQueueStatus(fixture.m_Queue);
    ASSERT_EQ((uint64_t)1, held.m_Published);
    ASSERT_EQ((uint64_t)0, held.m_Consumed);
    ReleaseFirstReader(&fixture.m_Probe);
    ASSERT_EQ(RENDER_FRAME_OK, PublishWhenAvailable(&fixture, second));
    StopRenderFrameQueue(fixture.m_Queue);
    JoinExecutor(&executor);
    ASSERT_EQ(2u, fixture.m_Probe.m_Consumed);
    ASSERT_EQ(7u, fixture.m_Probe.m_Inputs[0].m_Sequence);
    ASSERT_EQ(8u, fixture.m_Probe.m_Inputs[1].m_Sequence);
    ASSERT_EQ(83u, fixture.m_Probe.m_Inputs[1].m_Values[3]);
    ASSERT_FALSE(fixture.m_Probe.m_WrongOwner);
    ASSERT_FALSE(fixture.m_Probe.m_InvalidInput);
    DeleteFixture(&fixture);
}

// Verify cancelling a next-frame capture does not grant another simulation/update
// reservation while the same older reader is still active.
TEST(RenderFrameQueue, CancelDoesNotResetActiveReaderLead)
{
    if (!HasThreadedExecution())
        return;
    QueueFixture fixture;
    CreateFixture(&fixture, EXECUTION_MODE_THREADED);
    fixture.m_Probe.m_HoldFirstReader = true;
    TestExecutor executor;
    StartExecutor(&executor, &fixture, true);
    RenderFrameTicket accepted, cancelled, next;
    ASSERT_EQ(RENDER_FRAME_OK, BeginRenderFrameCapture(fixture.m_Queue, &accepted));
    ASSERT_EQ(RENDER_FRAME_OK, CaptureSample(accepted, 1));
    ASSERT_EQ(RENDER_FRAME_OK, PublishRenderFrame(fixture.m_Queue, accepted));
    WaitForFirstReader(&fixture.m_Probe);
    ASSERT_EQ(RENDER_FRAME_OK, BeginRenderFrameCapture(fixture.m_Queue, &cancelled));
    ASSERT_EQ(RENDER_FRAME_OK, CancelRenderFrameCapture(fixture.m_Queue, cancelled));
    ASSERT_EQ(RENDER_FRAME_WOULD_BLOCK, BeginRenderFrameCapture(fixture.m_Queue, &next));
    ReleaseFirstReader(&fixture.m_Probe);
    WaitForConsumed(&fixture, 1);
    RetireRenderFrames(fixture.m_Queue);
    ASSERT_EQ(RENDER_FRAME_OK, BeginRenderFrameCapture(fixture.m_Queue, &next));
    ASSERT_EQ(RENDER_FRAME_OK, CaptureSample(next, 2));
    ASSERT_EQ(RENDER_FRAME_OK, PublishRenderFrame(fixture.m_Queue, next));
    StopRenderFrameQueue(fixture.m_Queue);
    JoinExecutor(&executor);
    ASSERT_EQ(2u, fixture.m_Probe.m_Consumed);
    ASSERT_EQ(2u, fixture.m_Probe.m_Inputs[1].m_Sequence);
    ASSERT_FALSE(fixture.m_Probe.m_WrongOwner);
    DeleteFixture(&fixture);
}

// Verify repeated backpressure/reuse never drops or reorders accepted frames, and a
// stop immediately after the final acceptance still drains the complete FIFO.
TEST(RenderFrameQueue, AcceptedFramesDrainInOrder)
{
    if (!HasThreadedExecution())
        return;
    QueueFixture fixture;
    CreateFixture(&fixture, EXECUTION_MODE_THREADED);
    TestExecutor executor;
    StartExecutor(&executor, &fixture, true);
    RenderFrameId ids[12];
    uint32_t resources[12];
    for (uint32_t i = 0; i < 12; ++i)
    {
        RenderFrameTicket ticket;
        ASSERT_EQ(RENDER_FRAME_OK, BeginWhenAvailable(&fixture, &ticket));
        ids[i] = ticket.m_FrameId;
        resources[i] = i;
        ASSERT_EQ(RENDER_FRAME_OK, CaptureSample(ticket, i));
        ASSERT_EQ(RENDER_FRAME_OK, AddRenderFrameReference(ticket.m_Frame, &resources[i], ReleaseReference, &fixture.m_Probe));
        ASSERT_EQ(RENDER_FRAME_OK, PublishWhenAvailable(&fixture, ticket));
    }
    StopRenderFrameQueue(fixture.m_Queue);
    JoinExecutor(&executor);
    RetireRenderFrames(fixture.m_Queue);
    RenderFrameQueueStatus status = GetRenderFrameQueueStatus(fixture.m_Queue);
    ASSERT_EQ((uint64_t)12, status.m_Published);
    ASSERT_EQ((uint64_t)12, status.m_Consumed);
    ASSERT_EQ((uint64_t)12, status.m_Retired);
    ASSERT_EQ((uint64_t)0, status.m_Cancelled);
    ASSERT_EQ(12u, fixture.m_Probe.m_Consumed);
    ASSERT_EQ(12u, fixture.m_Probe.m_Released);
    uint32_t released_mask = 0;
    for (uint32_t i = 0; i < 12; ++i)
    {
        ASSERT_EQ(i, fixture.m_Probe.m_Inputs[i].m_Sequence);
        ASSERT_EQ(ids[i], fixture.m_Probe.m_FrameIds[i]);
        ASSERT_TRUE(fixture.m_Probe.m_ReleaseOrder[i] < 12);
        released_mask |= 1u << fixture.m_Probe.m_ReleaseOrder[i];
    }
    // Retirement may visit the two physical slots in either order, but must release
    // every reference once. FIFO is a publication/consumption requirement.
    ASSERT_EQ((1u << 12) - 1, released_mask);
    ASSERT_FALSE(fixture.m_Probe.m_WrongOwner);
    ASSERT_FALSE(fixture.m_Probe.m_InvalidInput);
    DeleteFixture(&fixture);
}

static void CheckStopWithAcceptedFrame(bool reading)
{
    QueueFixture fixture;
    CreateFixture(&fixture, EXECUTION_MODE_THREADED);
    fixture.m_Probe.m_HoldFirstReader = reading;
    TestExecutor executor;
    StartExecutor(&executor, &fixture, reading);
    uint32_t resources[] = { 1, 2 };
    RenderFrameTicket accepted, building, rejected;
    ASSERT_EQ(RENDER_FRAME_OK, BeginRenderFrameCapture(fixture.m_Queue, &accepted));
    ASSERT_EQ(RENDER_FRAME_OK, CaptureSample(accepted, 1));
    ASSERT_EQ(RENDER_FRAME_OK, AddRenderFrameReference(accepted.m_Frame, &resources[0], ReleaseReference, &fixture.m_Probe));
    ASSERT_EQ(RENDER_FRAME_OK, PublishRenderFrame(fixture.m_Queue, accepted));
    if (reading)
        WaitForFirstReader(&fixture.m_Probe);
    ASSERT_EQ(RENDER_FRAME_OK, BeginRenderFrameCapture(fixture.m_Queue, &building));
    ASSERT_EQ(RENDER_FRAME_OK, CaptureSample(building, 2));
    ASSERT_EQ(RENDER_FRAME_OK, AddRenderFrameReference(building.m_Frame, &resources[1], ReleaseReference, &fixture.m_Probe));
    StopRenderFrameQueue(fixture.m_Queue);
    StopRenderFrameQueue(fixture.m_Queue);
    ASSERT_EQ(1u, fixture.m_Probe.m_Released);
    ASSERT_EQ(2u, fixture.m_Probe.m_ReleaseOrder[0]);
    ASSERT_EQ(RENDER_FRAME_STOPPED, BeginRenderFrameCapture(fixture.m_Queue, &rejected));
    ASSERT_EQ(RENDER_FRAME_STOPPED, RequestRenderFrameService(fixture.m_Queue));
    ASSERT_NE(RENDER_FRAME_OK, PublishRenderFrame(fixture.m_Queue, building));
    if (reading)
        ReleaseFirstReader(&fixture.m_Probe);
    else
        ResumeExecutor(&executor);
    JoinExecutor(&executor);
    ASSERT_EQ(1u, RetireRenderFrames(fixture.m_Queue));
    ASSERT_EQ(0u, RetireRenderFrames(fixture.m_Queue));
    StopRenderFrameQueue(fixture.m_Queue);
    RenderFrameQueueStatus status = GetRenderFrameQueueStatus(fixture.m_Queue);
    ASSERT_TRUE(status.m_Stopping);
    ASSERT_EQ((uint64_t)1, status.m_Published);
    ASSERT_EQ((uint64_t)1, status.m_Consumed);
    ASSERT_EQ((uint64_t)1, status.m_Cancelled);
    ASSERT_EQ(1u, fixture.m_Probe.m_Consumed);
    ASSERT_EQ(1u, fixture.m_Probe.m_Inputs[0].m_Sequence);
    ASSERT_EQ(2u, fixture.m_Probe.m_Released);
    ASSERT_EQ(1u, fixture.m_Probe.m_ReleaseOrder[1]);
    ASSERT_FALSE(fixture.m_Probe.m_WrongOwner);
    DeleteFixture(&fixture);
}

// Verify Stop cancels only Building storage while preserving an accepted Ready frame,
// including repeated Stop calls before and after draining.
TEST(RenderFrameQueue, StopPreservesReadyFrame)
{
    if (HasThreadedExecution())
        CheckStopWithAcceptedFrame(false);
}

// Verify Stop never retires a still-active reader, and its completion releases exactly
// the accepted references after the partial next capture has been cancelled.
TEST(RenderFrameQueue, StopPreservesReadingFrame)
{
    if (HasThreadedExecution())
        CheckStopWithAcceptedFrame(true);
}

// Verify no-frame service requests wake a native executor before its first frame and
// accepted service requests are serviced when Stop follows immediately.
TEST(RenderFrameQueue, ServiceWakesWithoutFrames)
{
    if (!HasThreadedExecution())
        return;
    QueueFixture fixture;
    CreateFixture(&fixture, EXECUTION_MODE_THREADED);
    TestExecutor executor;
    StartExecutor(&executor, &fixture, true);
    dmMutex::Lock(fixture.m_Probe.m_Mutex);
    uint32_t before = fixture.m_Probe.m_ServiceCount;
    dmMutex::Unlock(fixture.m_Probe.m_Mutex);
    ASSERT_EQ(RENDER_FRAME_OK, RequestRenderFrameService(fixture.m_Queue));
    dmMutex::Lock(fixture.m_Probe.m_Mutex);
    while (fixture.m_Probe.m_ServiceCount == before)
        dmConditionVariable::Wait(fixture.m_Probe.m_Changed, fixture.m_Probe.m_Mutex);
    dmMutex::Unlock(fixture.m_Probe.m_Mutex);
    ASSERT_EQ(RENDER_FRAME_OK, RequestRenderFrameService(fixture.m_Queue));
    StopRenderFrameQueue(fixture.m_Queue);
    JoinExecutor(&executor);
    ASSERT_EQ(0u, fixture.m_Probe.m_Consumed);
    ASSERT_TRUE(fixture.m_Probe.m_ServiceCount > before);
    ASSERT_FALSE(fixture.m_Probe.m_WrongOwner);
    DeleteFixture(&fixture);
}

// Verify a service request arriving inside an active service callback is not cleared
// by that callback's completion or lost before the executor begins its next wait.
TEST(RenderFrameQueue, ServiceRequestDuringCallbackIsNotLost)
{
    if (!HasThreadedExecution())
        return;
    QueueFixture fixture;
    CreateFixture(&fixture, EXECUTION_MODE_THREADED);
    fixture.m_Probe.m_HoldFirstService = true;
    TestExecutor executor;
    StartExecutor(&executor, &fixture, true);
    dmMutex::Lock(fixture.m_Probe.m_Mutex);
    while (!fixture.m_Probe.m_FirstServiceEntered)
        dmConditionVariable::Wait(fixture.m_Probe.m_Changed, fixture.m_Probe.m_Mutex);
    dmMutex::Unlock(fixture.m_Probe.m_Mutex);
    ASSERT_EQ(RENDER_FRAME_OK, RequestRenderFrameService(fixture.m_Queue));
    dmMutex::Lock(fixture.m_Probe.m_Mutex);
    fixture.m_Probe.m_ReleaseFirstService = true;
    dmConditionVariable::Broadcast(fixture.m_Probe.m_Changed);
    while (fixture.m_Probe.m_ServiceCount < 2)
        dmConditionVariable::Wait(fixture.m_Probe.m_Changed, fixture.m_Probe.m_Mutex);
    dmMutex::Unlock(fixture.m_Probe.m_Mutex);
    StopRenderFrameQueue(fixture.m_Queue);
    JoinExecutor(&executor);
    ASSERT_EQ(0u, fixture.m_Probe.m_Consumed);
    ASSERT_TRUE(fixture.m_Probe.m_ServiceCount >= 2);
    ASSERT_FALSE(fixture.m_Probe.m_WrongOwner);
    DeleteFixture(&fixture);
}

struct ServiceRequestProbe
{
    HRenderFrameQueue m_Queue;
    RenderFrameResult m_Result;
};

static void RequestServiceFromWorker(void* context)
{
    ServiceRequestProbe* probe = (ServiceRequestProbe*)context;
    probe->m_Result = RequestRenderFrameService(probe->m_Queue);
}

// Verify a foreign worker cannot mutate an inline queue that intentionally owns no
// synchronization objects; the inline game/render owner still services it normally.
TEST(RenderFrameQueue, InlineRejectsWorkerServiceRequest)
{
    if (!GetExecutionCapabilities(false).m_ThreadsSupported)
        return;
    QueueFixture fixture;
    CreateFixture(&fixture, EXECUTION_MODE_INLINE);
    ServiceRequestProbe probe = { fixture.m_Queue, RENDER_FRAME_OK };
    dmThread::Thread worker = dmThread::New(RequestServiceFromWorker, 0x80000, &probe, "service-probe");
    dmThread::Join(worker);
    ASSERT_EQ(RENDER_FRAME_INVALID_STATE, probe.m_Result);
    ASSERT_EQ(0u, fixture.m_Probe.m_ServiceCount);
    ASSERT_EQ(RENDER_FRAME_OK, RequestRenderFrameService(fixture.m_Queue));
    ASSERT_EQ(RENDER_FRAME_PUMP_IDLE, PumpRenderFrameQueue(fixture.m_Queue, false));
    ASSERT_EQ(1u, fixture.m_Probe.m_ServiceCount);
    ASSERT_EQ(0u, fixture.m_Probe.m_Consumed);
    StopRenderFrameQueue(fixture.m_Queue);
    ASSERT_EQ(RENDER_FRAME_PUMP_DRAINED, PumpRenderFrameQueue(fixture.m_Queue, false));
    DeleteFixture(&fixture);
}

enum MainRenderPhase
{
    MAIN_RENDER_INITIAL,
    MAIN_RENDER_CREATED,
    MAIN_RENDER_BOUND,
    MAIN_RENDER_SERVICE_REQUESTED,
    MAIN_RENDER_SERVICED,
    MAIN_RENDER_PUBLISHED,
    MAIN_RENDER_CONSUMED,
    MAIN_RENDER_STOPPED,
    MAIN_RENDER_RELEASED,
};

struct MainRenderProbe
{
    QueueFixture m_Fixture;
    dmMutex::HMutex m_Mutex;
    dmConditionVariable::HConditionVariable m_Changed;
    MainRenderPhase m_Phase;
    bool m_GameOwner;
    bool m_ReferenceReleased;
};

static void SetMainRenderPhase(MainRenderProbe* probe, MainRenderPhase phase)
{
    dmMutex::Lock(probe->m_Mutex);
    probe->m_Phase = phase;
    dmConditionVariable::Broadcast(probe->m_Changed);
    dmMutex::Unlock(probe->m_Mutex);
}

static void WaitMainRenderPhase(MainRenderProbe* probe, MainRenderPhase phase)
{
    dmMutex::Lock(probe->m_Mutex);
    while (probe->m_Phase < phase)
        dmConditionVariable::Wait(probe->m_Changed, probe->m_Mutex);
    dmMutex::Unlock(probe->m_Mutex);
}

static void RunGameWithMainRenderOwner(void* context)
{
    MainRenderProbe* probe = (MainRenderProbe*)context;
    QueueFixture* fixture = &probe->m_Fixture;
    CreateFixture(fixture, EXECUTION_MODE_THREADED);
    probe->m_GameOwner = IsRenderExecutionOwner(fixture->m_Execution, EXECUTION_OWNER_GAME) &&
        !IsRenderExecutionOwner(fixture->m_Execution, EXECUTION_OWNER_RENDER);
    SetMainRenderPhase(probe, MAIN_RENDER_CREATED);
    WaitMainRenderPhase(probe, MAIN_RENDER_BOUND);
    RenderFrameResult result = RequestRenderFrameService(fixture->m_Queue);
    assert(result == RENDER_FRAME_OK);
    SetMainRenderPhase(probe, MAIN_RENDER_SERVICE_REQUESTED);
    WaitMainRenderPhase(probe, MAIN_RENDER_SERVICED);
    RenderFrameTicket ticket;
    result = BeginRenderFrameCapture(fixture->m_Queue, &ticket);
    assert(result == RENDER_FRAME_OK);
    result = CaptureSample(ticket, 42);
    assert(result == RENDER_FRAME_OK);
    uint32_t resource = 42;
    result = AddRenderFrameReference(ticket.m_Frame, &resource, ReleaseReference, &fixture->m_Probe);
    assert(result == RENDER_FRAME_OK);
    result = PublishRenderFrame(fixture->m_Queue, ticket);
    assert(result == RENDER_FRAME_OK);
    (void)result;
    SetMainRenderPhase(probe, MAIN_RENDER_PUBLISHED);
    WaitMainRenderPhase(probe, MAIN_RENDER_CONSUMED);
    StopRenderFrameQueue(fixture->m_Queue);
    SetMainRenderPhase(probe, MAIN_RENDER_STOPPED);
    WaitMainRenderPhase(probe, MAIN_RENDER_RELEASED);
    RetireRenderFrames(fixture->m_Queue);
    probe->m_ReferenceReleased = fixture->m_Probe.m_Released == 1 && fixture->m_Probe.m_ReleaseOrder[0] == 42;
    DeleteFixture(fixture);
}

// Verify web-style placement with a game worker and main-thread render owner, including
// no-frame service before startup and a service-only pump that must not draw Ready data.
TEST(RenderFrameQueue, MainRenderOwnerServiceOnlyDoesNotConsume)
{
    if (!HasThreadedExecution())
        return;
    MainRenderProbe probe = {};
    probe.m_Mutex = dmMutex::New();
    probe.m_Changed = dmConditionVariable::New();
    dmThread::Thread game = dmThread::New(RunGameWithMainRenderOwner, 0x80000, &probe, "frame-game");
    WaitMainRenderPhase(&probe, MAIN_RENDER_CREATED);
    HRenderExecution execution = probe.m_Fixture.m_Execution;
    HRenderFrameQueue queue = probe.m_Fixture.m_Queue;
    bool bound = BindRenderExecutionOwner(execution);
    assert(bound);
    bool render_only = IsRenderExecutionOwner(execution, EXECUTION_OWNER_RENDER) &&
        !IsRenderExecutionOwner(execution, EXECUTION_OWNER_GAME);
    SetMainRenderPhase(&probe, MAIN_RENDER_BOUND);
    WaitMainRenderPhase(&probe, MAIN_RENDER_SERVICE_REQUESTED);
    RenderFramePumpResult initial_service = PumpRenderFrameQueue(queue, false);
    uint32_t service_before_frame = probe.m_Fixture.m_Probe.m_ServiceCount;
    uint32_t consumed_before_frame = probe.m_Fixture.m_Probe.m_Consumed;
    RenderFrameResult render_request = RequestRenderFrameService(queue);
    SetMainRenderPhase(&probe, MAIN_RENDER_SERVICED);
    WaitMainRenderPhase(&probe, MAIN_RENDER_PUBLISHED);
    RenderFramePumpResult service_only = PumpRenderFrameQueue(queue, false);
    uint32_t consumed_after_service = probe.m_Fixture.m_Probe.m_Consumed;
    RenderFramePumpResult draw = PumpRenderFrameQueue(queue, true);
    uint32_t consumed_after_draw = probe.m_Fixture.m_Probe.m_Consumed;
    SetMainRenderPhase(&probe, MAIN_RENDER_CONSUMED);
    WaitMainRenderPhase(&probe, MAIN_RENDER_STOPPED);
    RenderFramePumpResult drain = PumpRenderFrameQueue(queue, true);
    bool correct_callbacks = !probe.m_Fixture.m_Probe.m_WrongOwner && !probe.m_Fixture.m_Probe.m_InvalidInput;
    ReleaseRenderExecutionOwner(execution);
    // The waiting game worker owns deletion; do not access queue/context after this signal.
    SetMainRenderPhase(&probe, MAIN_RENDER_RELEASED);
    dmThread::Join(game);
    dmConditionVariable::Delete(probe.m_Changed);
    dmMutex::Delete(probe.m_Mutex);
    ASSERT_TRUE(bound);
    ASSERT_TRUE(render_only);
    ASSERT_TRUE(probe.m_GameOwner);
    ASSERT_TRUE(probe.m_ReferenceReleased);
    ASSERT_TRUE(correct_callbacks);
    ASSERT_EQ(RENDER_FRAME_INVALID_STATE, render_request);
    ASSERT_EQ(RENDER_FRAME_PUMP_IDLE, initial_service);
    ASSERT_EQ(1u, service_before_frame);
    ASSERT_EQ(0u, consumed_before_frame);
    ASSERT_EQ(RENDER_FRAME_PUMP_IDLE, service_only);
    ASSERT_EQ(0u, consumed_after_service);
    ASSERT_EQ(RENDER_FRAME_PUMP_CONSUMED, draw);
    ASSERT_EQ(1u, consumed_after_draw);
    ASSERT_EQ(RENDER_FRAME_PUMP_DRAINED, drain);
}

// Verify stale tickets cannot publish or cancel the next capture reusing their slot.
TEST(RenderFrameQueue, StaleTicketsRejectReusedStorage)
{
    QueueFixture fixture;
    CreateFixture(&fixture, EXECUTION_MODE_INLINE);
    RenderFrameTicket stale, current;
    ASSERT_EQ(RENDER_FRAME_OK, BeginRenderFrameCapture(fixture.m_Queue, &stale));
    ASSERT_EQ(RENDER_FRAME_OK, CancelRenderFrameCapture(fixture.m_Queue, stale));
    ASSERT_EQ(RENDER_FRAME_OK, BeginRenderFrameCapture(fixture.m_Queue, &current));
    ASSERT_NE(stale.m_FrameId, current.m_FrameId);
    ASSERT_EQ(RENDER_FRAME_INVALID_STATE, PublishRenderFrame(fixture.m_Queue, stale));
    ASSERT_EQ(RENDER_FRAME_INVALID_STATE, CancelRenderFrameCapture(fixture.m_Queue, stale));
    ASSERT_EQ(RENDER_FRAME_OK, CaptureSample(current, 4));
    ASSERT_EQ(RENDER_FRAME_OK, PublishRenderFrame(fixture.m_Queue, current));
    ASSERT_EQ(RENDER_FRAME_INVALID_STATE, PublishRenderFrame(fixture.m_Queue, current));
    ASSERT_EQ(1u, fixture.m_Probe.m_Consumed);
    ASSERT_EQ(4u, fixture.m_Probe.m_Inputs[0].m_Sequence);
    StopRenderFrameQueue(fixture.m_Queue);
    ASSERT_EQ(RENDER_FRAME_PUMP_DRAINED, PumpRenderFrameQueue(fixture.m_Queue, true));
    DeleteFixture(&fixture);
}

// Verify capture overflow cannot publish partial data, releases already-transferred
// references on cancellation, and allows a clean capture in the same bounded storage.
TEST(RenderFrameQueue, OverflowCannotPublishAndCancellationCleansUp)
{
    QueueFixture fixture;
    CreateFixture(&fixture, EXECUTION_MODE_INLINE);
    RenderFrameTicket ticket;
    uint32_t resource = 6;
    ASSERT_EQ(RENDER_FRAME_OK, BeginRenderFrameCapture(fixture.m_Queue, &ticket));
    ASSERT_EQ(RENDER_FRAME_OK, AddRenderFrameReference(ticket.m_Frame, &resource, ReleaseReference, &fixture.m_Probe));
    uint32_t table_index;
    void* data;
    ASSERT_EQ(RENDER_FRAME_CAPACITY_EXCEEDED, AllocateRenderFrameTable(ticket.m_Frame, SAMPLE_TABLE_TYPE,
        1000, sizeof(SampleInput), 4, &table_index, &data));
    ASSERT_EQ(RENDER_FRAME_CAPACITY_EXCEEDED, PublishRenderFrame(fixture.m_Queue, ticket));
    ASSERT_EQ(0u, fixture.m_Probe.m_Consumed);
    ASSERT_EQ(0u, fixture.m_Probe.m_Released);
    ASSERT_EQ(RENDER_FRAME_OK, CancelRenderFrameCapture(fixture.m_Queue, ticket));
    ASSERT_EQ(1u, fixture.m_Probe.m_Released);
    ASSERT_EQ(RENDER_FRAME_OK, BeginRenderFrameCapture(fixture.m_Queue, &ticket));
    ASSERT_EQ(RENDER_FRAME_OK, CaptureSample(ticket, 9));
    ASSERT_EQ(RENDER_FRAME_OK, PublishRenderFrame(fixture.m_Queue, ticket));
    StopRenderFrameQueue(fixture.m_Queue);
    ASSERT_EQ(RENDER_FRAME_PUMP_DRAINED, PumpRenderFrameQueue(fixture.m_Queue, true));
    ASSERT_EQ(1u, fixture.m_Probe.m_Consumed);
    ASSERT_EQ(9u, fixture.m_Probe.m_Inputs[0].m_Sequence);
    ASSERT_FALSE(fixture.m_Probe.m_WrongOwner);
    DeleteFixture(&fixture);
}

// Verify an independently copied upload remains valid after CPU storage reuse and
// queue deletion; CPU retirement alone does not represent upload/GPU completion.
TEST(RenderFrameQueue, CopiedUploadOutlivesSlotReuse)
{
    QueueFixture fixture;
    CreateFixture(&fixture, EXECUTION_MODE_INLINE);
    fixture.m_Probe.m_CopyUpload = true;
    for (uint32_t i = 0; i < 3; ++i)
    {
        RenderFrameTicket ticket;
        ASSERT_EQ(RENDER_FRAME_OK, BeginRenderFrameCapture(fixture.m_Queue, &ticket));
        ASSERT_EQ(RENDER_FRAME_OK, CaptureSample(ticket, 10 + i));
        ASSERT_EQ(RENDER_FRAME_OK, PublishRenderFrame(fixture.m_Queue, ticket));
        ASSERT_EQ(1u, RetireRenderFrames(fixture.m_Queue));
    }
    ASSERT_TRUE(fixture.m_Probe.m_ReusedPayload);
    SampleInput* upload = fixture.m_Probe.m_Upload;
    StopRenderFrameQueue(fixture.m_Queue);
    ASSERT_EQ(RENDER_FRAME_PUMP_DRAINED, PumpRenderFrameQueue(fixture.m_Queue, true));
    DeleteFixture(&fixture);
    ASSERT_NE((SampleInput*)0, upload);
    ASSERT_EQ(10u, upload->m_Sequence);
    ASSERT_EQ(103u, upload->m_Values[3]);
    free(upload);
}


// Verify both CPU slots remain unavailable after consumption until explicit game-owner
// retirement, and frame IDs never wrap into identifiers used by earlier tickets.
TEST(RenderFrameQueue, SlotBudgetAndFrameIdExhaustion)
{
    QueueFixture fixture;
    CreateFixture(&fixture, EXECUTION_MODE_INLINE);
    RenderFrameTicket tickets[2], extra;
    for (uint32_t i = 0; i < 2; ++i)
    {
        ASSERT_EQ(RENDER_FRAME_OK, BeginRenderFrameCapture(fixture.m_Queue, &tickets[i]));
        ASSERT_EQ(RENDER_FRAME_OK, CaptureSample(tickets[i], i));
        ASSERT_EQ(RENDER_FRAME_OK, PublishRenderFrame(fixture.m_Queue, tickets[i]));
    }
    ASSERT_NE(tickets[0].m_Frame, tickets[1].m_Frame);
    ASSERT_EQ(RENDER_FRAME_WOULD_BLOCK, BeginRenderFrameCapture(fixture.m_Queue, &extra));
    RenderFrameQueueStatus status = GetRenderFrameQueueStatus(fixture.m_Queue);
    ASSERT_EQ(RENDER_FRAME_RETIRED, status.m_States[0]);
    ASSERT_EQ(RENDER_FRAME_RETIRED, status.m_States[1]);
    ASSERT_EQ(2u, RetireRenderFrames(fixture.m_Queue));
    ASSERT_EQ(sizeof(RenderFrameQueueContext) + 2 * GetRenderFrameStorageSize(fixture.m_Queue->m_Params.m_Limits),
        GetRenderFrameQueueStorageSize(fixture.m_Queue->m_Params.m_Limits));
    fixture.m_Queue->m_NextFrameId = UINT64_MAX;
    ASSERT_EQ(RENDER_FRAME_OK, BeginRenderFrameCapture(fixture.m_Queue, &extra));
    ASSERT_EQ((RenderFrameId)UINT64_MAX, extra.m_FrameId);
    ASSERT_EQ(RENDER_FRAME_OK, CancelRenderFrameCapture(fixture.m_Queue, extra));
    ASSERT_EQ(RENDER_FRAME_CAPACITY_EXCEEDED, BeginRenderFrameCapture(fixture.m_Queue, &extra));
    StopRenderFrameQueue(fixture.m_Queue);
    ASSERT_EQ(RENDER_FRAME_PUMP_DRAINED, PumpRenderFrameQueue(fixture.m_Queue, true));
    DeleteFixture(&fixture);
}

struct PumpStepExecutor
{
    QueueFixture* m_Fixture;
    dmThread::Thread m_Thread;
    dmMutex::HMutex m_Mutex;
    dmConditionVariable::HConditionVariable m_Changed;
    uint32_t m_Requested;
    uint32_t m_Finished;
    bool m_Started;
    bool m_Consume;
    bool m_Exit;
    RenderFramePumpResult m_Result;
};

static void RunPumpSteps(void* context)
{
    PumpStepExecutor* executor = (PumpStepExecutor*)context;
    bool bound = BindRenderExecutionOwner(executor->m_Fixture->m_Execution);
    assert(bound);
    (void)bound;
    dmMutex::Lock(executor->m_Mutex);
    executor->m_Started = true;
    dmConditionVariable::Broadcast(executor->m_Changed);
    for (;;)
    {
        while (!executor->m_Exit && executor->m_Requested == executor->m_Finished)
            dmConditionVariable::Wait(executor->m_Changed, executor->m_Mutex);
        if (executor->m_Exit)
            break;
        bool consume = executor->m_Consume;
        dmMutex::Unlock(executor->m_Mutex);
        RenderFramePumpResult result = PumpRenderFrameQueue(executor->m_Fixture->m_Queue, consume);
        dmMutex::Lock(executor->m_Mutex);
        executor->m_Result = result;
        ++executor->m_Finished;
        dmConditionVariable::Broadcast(executor->m_Changed);
    }
    dmMutex::Unlock(executor->m_Mutex);
    ReleaseRenderExecutionOwner(executor->m_Fixture->m_Execution);
}

static void StartPumpSteps(PumpStepExecutor* executor, QueueFixture* fixture)
{
    memset(executor, 0, sizeof(*executor));
    executor->m_Fixture = fixture;
    executor->m_Mutex = dmMutex::New();
    executor->m_Changed = dmConditionVariable::New();
    executor->m_Thread = dmThread::New(RunPumpSteps, 0x80000, executor, "pump-steps");
    dmMutex::Lock(executor->m_Mutex);
    while (!executor->m_Started)
        dmConditionVariable::Wait(executor->m_Changed, executor->m_Mutex);
    dmMutex::Unlock(executor->m_Mutex);
}

static void RequestPumpStep(PumpStepExecutor* executor, bool consume)
{
    DM_MUTEX_SCOPED_LOCK(executor->m_Mutex);
    assert(executor->m_Requested == executor->m_Finished);
    executor->m_Consume = consume;
    ++executor->m_Requested;
    dmConditionVariable::Broadcast(executor->m_Changed);
}

static RenderFramePumpResult WaitPumpStep(PumpStepExecutor* executor)
{
    DM_MUTEX_SCOPED_LOCK(executor->m_Mutex);
    while (executor->m_Finished != executor->m_Requested)
        dmConditionVariable::Wait(executor->m_Changed, executor->m_Mutex);
    return executor->m_Result;
}

static void JoinPumpSteps(PumpStepExecutor* executor)
{
    dmMutex::Lock(executor->m_Mutex);
    executor->m_Exit = true;
    dmConditionVariable::Broadcast(executor->m_Changed);
    dmMutex::Unlock(executor->m_Mutex);
    dmThread::Join(executor->m_Thread);
    dmConditionVariable::Delete(executor->m_Changed);
    dmMutex::Delete(executor->m_Mutex);
}

// Verify a contended external-owner pump returns while the producer still holds the
// handoff lock, without claiming or losing the accepted frame.
TEST(RenderFrameQueue, PumpNeverWaitsForProducerLock)
{
    if (!HasThreadedExecution())
        return;
    QueueFixture fixture;
    CreateFixture(&fixture, EXECUTION_MODE_THREADED);
    PumpStepExecutor executor;
    StartPumpSteps(&executor, &fixture);
    RenderFrameTicket ticket;
    ASSERT_EQ(RENDER_FRAME_OK, BeginRenderFrameCapture(fixture.m_Queue, &ticket));
    ASSERT_EQ(RENDER_FRAME_OK, CaptureSample(ticket, 5));
    ASSERT_EQ(RENDER_FRAME_OK, PublishRenderFrame(fixture.m_Queue, ticket));
    dmMutex::Lock(fixture.m_Queue->m_Mutex);
    RequestPumpStep(&executor, true);
    RenderFramePumpResult busy = WaitPumpStep(&executor);
    dmMutex::Unlock(fixture.m_Queue->m_Mutex);
    ASSERT_EQ(RENDER_FRAME_PUMP_BUSY, busy);
    ASSERT_EQ((uint64_t)0, GetRenderFrameQueueStatus(fixture.m_Queue).m_Consumed);
    RequestPumpStep(&executor, true);
    ASSERT_EQ(RENDER_FRAME_PUMP_CONSUMED, WaitPumpStep(&executor));
    StopRenderFrameQueue(fixture.m_Queue);
    RequestPumpStep(&executor, true);
    ASSERT_EQ(RENDER_FRAME_PUMP_DRAINED, WaitPumpStep(&executor));
    JoinPumpSteps(&executor);
    ASSERT_EQ(1u, fixture.m_Probe.m_Consumed);
    ASSERT_EQ(5u, fixture.m_Probe.m_Inputs[0].m_Sequence);
    DeleteFixture(&fixture);
}

// Verify completion contention defers acknowledgement without blocking the render owner;
// Reading storage cannot retire until another pump acknowledges it, and is not read twice.
TEST(RenderFrameQueue, CompletionAcknowledgementNeverWaits)
{
    if (!HasThreadedExecution())
        return;
    QueueFixture fixture;
    CreateFixture(&fixture, EXECUTION_MODE_THREADED);
    fixture.m_Probe.m_HoldFirstReader = true;
    PumpStepExecutor executor;
    StartPumpSteps(&executor, &fixture);
    RenderFrameTicket ticket;
    ASSERT_EQ(RENDER_FRAME_OK, BeginRenderFrameCapture(fixture.m_Queue, &ticket));
    ASSERT_EQ(RENDER_FRAME_OK, CaptureSample(ticket, 6));
    ASSERT_EQ(RENDER_FRAME_OK, PublishRenderFrame(fixture.m_Queue, ticket));
    RequestPumpStep(&executor, true);
    WaitForFirstReader(&fixture.m_Probe);
    dmMutex::Lock(fixture.m_Queue->m_Mutex);
    ReleaseFirstReader(&fixture.m_Probe);
    RenderFramePumpResult consumed = WaitPumpStep(&executor);
    bool reading = fixture.m_Queue->m_Active != -1 &&
        fixture.m_Queue->m_Slots[fixture.m_Queue->m_Active].m_State == RENDER_FRAME_READING;
    uint64_t acknowledged = fixture.m_Queue->m_Consumed;
    dmMutex::Unlock(fixture.m_Queue->m_Mutex);
    ASSERT_EQ(RENDER_FRAME_PUMP_CONSUMED, consumed);
    ASSERT_TRUE(reading);
    ASSERT_EQ((uint64_t)0, acknowledged);
    ASSERT_EQ(0u, RetireRenderFrames(fixture.m_Queue));
    RequestPumpStep(&executor, false);
    ASSERT_EQ(RENDER_FRAME_PUMP_IDLE, WaitPumpStep(&executor));
    ASSERT_EQ((uint64_t)1, GetRenderFrameQueueStatus(fixture.m_Queue).m_Consumed);
    ASSERT_EQ(1u, RetireRenderFrames(fixture.m_Queue));
    StopRenderFrameQueue(fixture.m_Queue);
    RequestPumpStep(&executor, true);
    ASSERT_EQ(RENDER_FRAME_PUMP_DRAINED, WaitPumpStep(&executor));
    JoinPumpSteps(&executor);
    ASSERT_EQ(1u, fixture.m_Probe.m_Consumed);
    DeleteFixture(&fixture);
}

extern "C" void dmExportedSymbols();

int main(int argc, char** argv)
{
    dmExportedSymbols();
    TestMainPlatformInit();
    for (int i = 1; i < argc; ++i)
        if (strcmp(argv[i], "--benchmark-render-frames") == 0)
            return RunRenderFrameBenchmark();
    jc_test_init(&argc, argv);
    return jc_test_run_all();
}
