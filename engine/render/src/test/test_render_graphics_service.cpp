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
#include <string.h>

#include <dlib/condition_variable.h>
#include <dlib/mutex.h>
#include <dlib/thread.h>
#include <resource/resource.h>
#include <testmain/testmain.h>

#include "render/render.h"
#include "render/render_graphics_service.h"

using namespace dmRender;

static const uint32_t MAX_TEST_TASKS = 32;

struct GraphicsServiceProbe;

// The only pointer is test instrumentation, which outlives the service and executor.
// Actual operation inputs and outputs live in the copied request bytes.
struct TestTaskPayload
{
    GraphicsServiceProbe* m_Probe;
    uint32_t              m_Value;
    uint32_t              m_Output;
    int32_t               m_Result;
};

// Observations shared with the executor are protected by m_Mutex. Completion
// callbacks also use it so ownership and ordering checks share one clear boundary.
struct GraphicsServiceProbe
{
    HGraphicsService                         m_Service;
    dmMutex::HMutex                          m_Mutex;
    dmConditionVariable::HConditionVariable  m_Changed;
    uint32_t                                 m_ExecutedValues[MAX_TEST_TASKS];
    uint32_t                                 m_CompletedValues[MAX_TEST_TASKS];
    uint32_t                                 m_CompletedOutputs[MAX_TEST_TASKS];
    int32_t                                  m_CompletedResults[MAX_TEST_TASKS];
    uint32_t                                 m_Entered;
    uint32_t                                 m_Executed;
    uint32_t                                 m_Completed;
    uint32_t                                 m_Wakes;
    uint32_t                                 m_BlockValue;
    uint32_t                                 m_NestedDispatchCount;
    GraphicsServicePumpResult                m_NestedPumpResult;
    GraphicsServiceResult                    m_ChildEnqueueResult;
    uint8_t                                  m_WrongOwner : 1;
    uint8_t                                  m_InvalidPayload : 1;
    uint8_t                                  m_ReleaseExecution : 1;
    uint8_t                                  m_EnqueueFromExecute : 1;
    uint8_t                                  m_EnqueueFromComplete : 1;
};

struct GraphicsServiceFixture
{
    GraphicsServiceProbe m_Probe;
    HGraphicsService     m_Service;
};

static GraphicsServiceResult EnqueueTestTask(GraphicsServiceFixture* fixture, uint32_t value, int32_t result);
static int32_t               ExecuteTestTask(dmGraphics::HContext graphics_context, void* payload, uint32_t payload_size);
static void                  CompleteTestTask(void* context, int32_t result, const void* payload, uint32_t payload_size);

static GraphicsTask MakeTask(GraphicsServiceProbe* probe, TestTaskPayload* payload, uint32_t value, int32_t result)
{
    payload->m_Probe = probe;
    payload->m_Value = value;
    payload->m_Output = 0;
    payload->m_Result = result;

    GraphicsTask task = {};
    task.m_Execute = ExecuteTestTask;
    task.m_Complete = CompleteTestTask;
    task.m_CompleteContext = probe;
    task.m_Payload = payload;
    task.m_PayloadSize = sizeof(*payload);
    return task;
}

static void WakeExecutor(void* context)
{
    GraphicsServiceProbe* probe = (GraphicsServiceProbe*)context;
    dmMutex::Lock(probe->m_Mutex);
    ++probe->m_Wakes;
    dmConditionVariable::Broadcast(probe->m_Changed);
    dmMutex::Unlock(probe->m_Mutex);
}

static int32_t ExecuteTestTask(dmGraphics::HContext graphics_context, void* payload, uint32_t payload_size)
{
    TestTaskPayload* input = (TestTaskPayload*)payload;
    GraphicsServiceProbe* probe = input->m_Probe;
    dmMutex::Lock(probe->m_Mutex);
    probe->m_WrongOwner |= !IsRenderExecutionOwner(GetGraphicsServiceExecution(probe->m_Service), EXECUTION_OWNER_RENDER);
    probe->m_InvalidPayload |= graphics_context != 0 || payload_size != sizeof(*input) || ((uintptr_t)payload & 15) != 0;
    ++probe->m_Entered;
    dmConditionVariable::Broadcast(probe->m_Changed);
    while (probe->m_BlockValue == input->m_Value && !probe->m_ReleaseExecution)
    {
        dmConditionVariable::Wait(probe->m_Changed, probe->m_Mutex);
    }
    assert(probe->m_Executed < MAX_TEST_TASKS);
    probe->m_ExecutedValues[probe->m_Executed++] = input->m_Value;
    bool enqueue_child = probe->m_EnqueueFromExecute && input->m_Value == 1;
    dmMutex::Unlock(probe->m_Mutex);

    input->m_Output = input->m_Value * 3;
    if (enqueue_child)
    {
        TestTaskPayload child;
        GraphicsTask task = MakeTask(probe, &child, 2, 0);
        GraphicsServiceResult enqueue_result = EnqueueGraphicsTask(probe->m_Service, task);
        GraphicsServicePumpResult pump_result = PumpGraphicsService(probe->m_Service, MAX_TEST_TASKS);
        dmMutex::Lock(probe->m_Mutex);
        probe->m_ChildEnqueueResult = enqueue_result;
        probe->m_NestedPumpResult = pump_result;
        dmMutex::Unlock(probe->m_Mutex);
    }
    return input->m_Result;
}

static void CompleteTestTask(void* context, int32_t result, const void* payload, uint32_t payload_size)
{
    GraphicsServiceProbe* probe = (GraphicsServiceProbe*)context;
    const TestTaskPayload* input = (const TestTaskPayload*)payload;
    dmMutex::Lock(probe->m_Mutex);
    probe->m_WrongOwner |= !IsRenderExecutionOwner(GetGraphicsServiceExecution(probe->m_Service), EXECUTION_OWNER_GAME);
    probe->m_InvalidPayload |= payload_size != sizeof(*input) || input->m_Probe != probe;
    assert(probe->m_Completed < MAX_TEST_TASKS);
    uint32_t index = probe->m_Completed++;
    probe->m_CompletedValues[index] = input->m_Value;
    probe->m_CompletedOutputs[index] = input->m_Output;
    probe->m_CompletedResults[index] = result;
    bool enqueue_child = probe->m_EnqueueFromComplete && input->m_Value == 1;
    dmMutex::Unlock(probe->m_Mutex);

    if (enqueue_child)
    {
        TestTaskPayload child;
        GraphicsTask task = MakeTask(probe, &child, 2, 0);
        GraphicsServiceResult enqueue_result = EnqueueGraphicsTask(probe->m_Service, task);
        PumpGraphicsService(probe->m_Service, MAX_TEST_TASKS);
        uint32_t nested_count = DispatchGraphicsCompletions(probe->m_Service);
        dmMutex::Lock(probe->m_Mutex);
        probe->m_ChildEnqueueResult = enqueue_result;
        probe->m_NestedDispatchCount = nested_count;
        probe->m_InvalidPayload |= input->m_Value != 1 || input->m_Output != 3;
        dmMutex::Unlock(probe->m_Mutex);
    }
}

static void CreateFixture(GraphicsServiceFixture* fixture, ExecutionMode mode, uint32_t max_requests, uint32_t max_bytes)
{
    memset(fixture, 0, sizeof(*fixture));
    fixture->m_Probe.m_Mutex = dmMutex::New();
    fixture->m_Probe.m_Changed = dmConditionVariable::New();

    GraphicsServiceParams params = {};
    params.m_Mode = mode;
    params.m_MaxRequests = max_requests;
    params.m_MaxPayloadBytes = max_bytes;
    params.m_ExecutorAvailable = true;
    params.m_Wake = WakeExecutor;
    params.m_WakeContext = &fixture->m_Probe;

    ExecutionSelection selection;
    fixture->m_Service = NewGraphicsService(params, &selection);
    assert(fixture->m_Service && selection.m_ExecutionMode == mode);
    fixture->m_Probe.m_Service = fixture->m_Service;
}

static void DeleteFixture(GraphicsServiceFixture* fixture)
{
    StopGraphicsService(fixture->m_Service);
    if (GetRenderExecutionMode(GetGraphicsServiceExecution(fixture->m_Service)) == EXECUTION_MODE_INLINE)
    {
        PumpGraphicsService(fixture->m_Service, MAX_TEST_TASKS);
    }
    DispatchGraphicsCompletions(fixture->m_Service);
    DeleteGraphicsService(fixture->m_Service);
    dmConditionVariable::Delete(fixture->m_Probe.m_Changed);
    dmMutex::Delete(fixture->m_Probe.m_Mutex);
}

static GraphicsServiceResult EnqueueTestTask(GraphicsServiceFixture* fixture, uint32_t value, int32_t result)
{
    TestTaskPayload payload;
    GraphicsTask task = MakeTask(&fixture->m_Probe, &payload, value, result);
    return EnqueueGraphicsTask(fixture->m_Service, task);
}

static bool HasThreadedExecution()
{
    ExecutionCapabilities capabilities = GetExecutionCapabilities(true);
    return capabilities.m_BuildEnabled && capabilities.m_ThreadsSupported;
}

// This executor only services graphics requests: it never creates or submits a
// render frame. Service wake/progress signals are the only scheduling mechanism.
struct GraphicsServiceExecutor
{
    GraphicsServiceFixture* m_Fixture;
    dmThread::Thread        m_Thread;
    // Full types: startup flags use the probe mutex, while release is read after
    // joining. They must not share a bitfield read/modify/write storage unit.
    uint8_t                 m_Started;
    uint8_t                 m_Run;
    uint8_t                 m_Bound;
    uint8_t                 m_Released;
};

static void RunExecutor(void* context)
{
    GraphicsServiceExecutor* executor = (GraphicsServiceExecutor*)context;
    GraphicsServiceProbe* probe = &executor->m_Fixture->m_Probe;
    HGraphicsService service = executor->m_Fixture->m_Service;
    HRenderExecution execution = GetGraphicsServiceExecution(service);
    executor->m_Bound = BindRenderExecutionOwner(execution);

    dmMutex::Lock(probe->m_Mutex);
    executor->m_Started = true;
    dmConditionVariable::Broadcast(probe->m_Changed);
    while (!executor->m_Run)
    {
        dmConditionVariable::Wait(probe->m_Changed, probe->m_Mutex);
    }
    dmMutex::Unlock(probe->m_Mutex);

    for (;;)
    {
        GraphicsServicePumpResult result = PumpGraphicsService(service, 2);
        if (result == GRAPHICS_SERVICE_PUMP_DRAINED)
        {
            break;
        }
        if (result == GRAPHICS_SERVICE_PUMP_IDLE || result == GRAPHICS_SERVICE_PUMP_BUSY)
        {
            WaitForGraphicsServiceWork(service);
        }
    }
    ReleaseRenderExecutionOwner(execution);
    executor->m_Released = !IsRenderExecutionOwner(execution, EXECUTION_OWNER_RENDER);
}

static void StartExecutor(GraphicsServiceExecutor* executor, GraphicsServiceFixture* fixture, bool run)
{
    memset(executor, 0, sizeof(*executor));
    executor->m_Fixture = fixture;
    executor->m_Run = run;
    executor->m_Thread = dmThread::New(RunExecutor, 0x80000, executor, "graphics-owner");

    dmMutex::Lock(fixture->m_Probe.m_Mutex);
    while (!executor->m_Started)
    {
        dmConditionVariable::Wait(fixture->m_Probe.m_Changed, fixture->m_Probe.m_Mutex);
    }
    dmMutex::Unlock(fixture->m_Probe.m_Mutex);
    assert(executor->m_Bound);
}

static void ResumeExecutor(GraphicsServiceExecutor* executor)
{
    GraphicsServiceProbe* probe = &executor->m_Fixture->m_Probe;
    dmMutex::Lock(probe->m_Mutex);
    executor->m_Run = true;
    dmConditionVariable::Broadcast(probe->m_Changed);
    dmMutex::Unlock(probe->m_Mutex);
}

static void WaitForExecuted(HGraphicsService service, uint32_t count)
{
    GraphicsServiceStatus status = GetGraphicsServiceStatus(service);
    while (status.m_Executed < count)
    {
        WaitForGraphicsServiceProgress(service, status.m_Revision);
        status = GetGraphicsServiceStatus(service);
    }
}

// Requests own their input bytes, and even inline completion waits for the game
// service boundary. No frame or graphics context is required to make progress.
TEST(GraphicsService, InlineCopiesInputsAndDefersCompletion)
{
    GraphicsServiceFixture fixture;
    CreateFixture(&fixture, EXECUTION_MODE_INLINE, 4, 1024);
    TestTaskPayload payload;
    GraphicsTask task = MakeTask(&fixture.m_Probe, &payload, 7, 0);

    ASSERT_EQ(GRAPHICS_SERVICE_OK, EnqueueGraphicsTask(fixture.m_Service, task));
    payload.m_Value = 99;
    ASSERT_EQ(0u, fixture.m_Probe.m_Executed);
    ASSERT_EQ(0u, DispatchGraphicsCompletions(fixture.m_Service));

    ASSERT_EQ(GRAPHICS_SERVICE_PUMP_PROGRESSED, PumpGraphicsService(fixture.m_Service, 1));
    ASSERT_EQ(1u, fixture.m_Probe.m_Executed);
    ASSERT_EQ(7u, fixture.m_Probe.m_ExecutedValues[0]);
    ASSERT_EQ(0u, fixture.m_Probe.m_Completed);

    ASSERT_EQ(1u, DispatchGraphicsCompletions(fixture.m_Service));
    ASSERT_EQ(7u, fixture.m_Probe.m_CompletedValues[0]);
    ASSERT_EQ(21u, fixture.m_Probe.m_CompletedOutputs[0]);
    ASSERT_FALSE(fixture.m_Probe.m_WrongOwner);
    ASSERT_FALSE(fixture.m_Probe.m_InvalidPayload);
    ASSERT_EQ(0u, GetGraphicsServiceStatus(fixture.m_Service).m_PendingRequests);
    DeleteFixture(&fixture);
}

// Undelivered completions still occupy request slots. Releasing a slot at execute
// time would allow an unbounded backlog of result payloads on a slow game owner.
TEST(GraphicsService, RequestLimitIncludesUndeliveredResults)
{
    GraphicsServiceFixture fixture;
    CreateFixture(&fixture, EXECUTION_MODE_INLINE, 2, 1024);
    ASSERT_EQ(GRAPHICS_SERVICE_OK, EnqueueTestTask(&fixture, 1, 0));
    ASSERT_EQ(GRAPHICS_SERVICE_OK, EnqueueTestTask(&fixture, 2, 0));
    ASSERT_EQ(GRAPHICS_SERVICE_WOULD_BLOCK, EnqueueTestTask(&fixture, 3, 0));

    ASSERT_EQ(GRAPHICS_SERVICE_PUMP_PROGRESSED, PumpGraphicsService(fixture.m_Service, 2));
    ASSERT_EQ(GRAPHICS_SERVICE_WOULD_BLOCK, EnqueueTestTask(&fixture, 3, 0));
    GraphicsServiceStatus status = GetGraphicsServiceStatus(fixture.m_Service);
    ASSERT_EQ(2u, status.m_PendingRequests);
    ASSERT_EQ(0u, status.m_QueuedRequests);
    ASSERT_EQ((uint64_t)2, status.m_Executed);

    ASSERT_EQ(2u, DispatchGraphicsCompletions(fixture.m_Service));
    ASSERT_EQ(GRAPHICS_SERVICE_OK, EnqueueTestTask(&fixture, 3, 0));
    DeleteFixture(&fixture);
    ASSERT_EQ(3u, fixture.m_Probe.m_Completed);
}

// Payload bytes have a separate bound from request count, and remain charged
// until the game owner receives the output. An impossible request is not retryable.
TEST(GraphicsService, ByteLimitIncludesUndeliveredResults)
{
    GraphicsServiceFixture fixture;
    CreateFixture(&fixture, EXECUTION_MODE_INLINE, 4, sizeof(TestTaskPayload));
    ASSERT_EQ(GRAPHICS_SERVICE_OK, EnqueueTestTask(&fixture, 1, 0));
    ASSERT_EQ(GRAPHICS_SERVICE_WOULD_BLOCK, EnqueueTestTask(&fixture, 2, 0));
    PumpGraphicsService(fixture.m_Service, 1);
    ASSERT_EQ(GRAPHICS_SERVICE_WOULD_BLOCK, EnqueueTestTask(&fixture, 2, 0));
    ASSERT_EQ((uint32_t)sizeof(TestTaskPayload), GetGraphicsServiceStatus(fixture.m_Service).m_PayloadBytes);

    TestTaskPayload payload;
    GraphicsTask oversized = MakeTask(&fixture.m_Probe, &payload, 3, 0);
    oversized.m_PayloadSize = sizeof(TestTaskPayload) + 1;
    ASSERT_EQ(GRAPHICS_SERVICE_INVALID_ARGUMENT, EnqueueGraphicsTask(fixture.m_Service, oversized));
    ASSERT_EQ(1u, DispatchGraphicsCompletions(fixture.m_Service));
    ASSERT_EQ(0u, GetGraphicsServiceStatus(fixture.m_Service).m_PayloadBytes);
    ASSERT_EQ(GRAPHICS_SERVICE_OK, EnqueueTestTask(&fixture, 2, 0));
    DeleteFixture(&fixture);
}

struct SplitTaskPayload
{
    uint32_t m_Count;
    uint32_t m_Sum;
    uint32_t m_Padding[2];
};

struct SplitTaskResult
{
    uint32_t m_Count;
    uint32_t m_Sum;
    uint32_t m_Size;
};

static int32_t ExecuteSplitTask(dmGraphics::HContext context, void* payload, uint32_t size)
{
    SplitTaskPayload* header = (SplitTaskPayload*)payload;
    const uint32_t* values = (const uint32_t*)(header + 1);
    header->m_Sum = 0;
    for (uint32_t i = 0; i < header->m_Count; ++i)
    {
        header->m_Sum += values[i];
    }
    return 0;
}

static void CompleteSplitTask(void* context, int32_t result, const void* payload, uint32_t size)
{
    const SplitTaskPayload* header = (const SplitTaskPayload*)payload;
    SplitTaskResult* output = (SplitTaskResult*)context;
    output->m_Count = header->m_Count;
    output->m_Sum = header->m_Sum;
    output->m_Size = size;
}

// Header-plus-data admission copies both spans into one owned block. Caller
// mutation after acceptance cannot change upload bytes or output delivered later.
TEST(GraphicsService, SplitPayloadOwnsHeaderAndTrailingBytes)
{
    GraphicsServiceFixture fixture;
    CreateFixture(&fixture, EXECUTION_MODE_INLINE, 4, 1024);
    SplitTaskPayload header = {};
    header.m_Count = 4;
    uint32_t values[] = { 2, 3, 5, 7 };
    SplitTaskResult output = {};
    GraphicsTask task = {};
    task.m_Execute = ExecuteSplitTask;
    task.m_Complete = CompleteSplitTask;
    task.m_CompleteContext = &output;
    task.m_Payload = &header;
    task.m_PayloadSize = sizeof(header);

    ASSERT_EQ(GRAPHICS_SERVICE_OK, EnqueueGraphicsTask(fixture.m_Service, task, values, sizeof(values)));
    memset(&header, 0, sizeof(header));
    memset(values, 0, sizeof(values));
    PumpGraphicsService(fixture.m_Service, 1);
    ASSERT_EQ(0u, output.m_Sum);
    ASSERT_EQ(1u, DispatchGraphicsCompletions(fixture.m_Service));
    ASSERT_EQ(4u, output.m_Count);
    ASSERT_EQ(17u, output.m_Sum);
    ASSERT_EQ((uint32_t)(sizeof(header) + sizeof(values)), output.m_Size);
    DeleteFixture(&fixture);
}

static int32_t ExecuteEmptyTask(dmGraphics::HContext context, void* payload, uint32_t size)
{
    assert(payload == 0 && size == 0);
    return 17;
}

// Requests without payload or completion callbacks still retire at the game
// boundary. Zero-byte tasks must not depend on platform malloc(0) behavior.
TEST(GraphicsService, EmptyTaskWithoutCompletionStillRetires)
{
    GraphicsServiceFixture fixture;
    CreateFixture(&fixture, EXECUTION_MODE_INLINE, 1, 0);
    GraphicsTask task = {};
    task.m_Execute = ExecuteEmptyTask;
    ASSERT_EQ(GRAPHICS_SERVICE_OK, EnqueueGraphicsTask(fixture.m_Service, task));
    ASSERT_EQ(GRAPHICS_SERVICE_PUMP_PROGRESSED, PumpGraphicsService(fixture.m_Service, 1));
    ASSERT_EQ(GRAPHICS_SERVICE_WOULD_BLOCK, EnqueueGraphicsTask(fixture.m_Service, task));
    ASSERT_EQ(1u, DispatchGraphicsCompletions(fixture.m_Service));
    ASSERT_EQ(0u, GetGraphicsServiceStatus(fixture.m_Service).m_PendingRequests);
    ASSERT_EQ(GRAPHICS_SERVICE_OK, EnqueueGraphicsTask(fixture.m_Service, task));
    DeleteFixture(&fixture);
}

// A failed operation still owns its output until the ordinary completion boundary
// and reports that exact result once, without silently dropping later requests.
TEST(GraphicsService, ExecutionFailureIsDeliveredOnce)
{
    GraphicsServiceFixture fixture;
    CreateFixture(&fixture, EXECUTION_MODE_INLINE, 4, 1024);
    ASSERT_EQ(GRAPHICS_SERVICE_OK, EnqueueTestTask(&fixture, 1, -17));
    ASSERT_EQ(GRAPHICS_SERVICE_OK, EnqueueTestTask(&fixture, 2, 0));
    PumpGraphicsService(fixture.m_Service, 4);
    ASSERT_EQ(2u, DispatchGraphicsCompletions(fixture.m_Service));
    ASSERT_EQ(-17, fixture.m_Probe.m_CompletedResults[0]);
    ASSERT_EQ(0, fixture.m_Probe.m_CompletedResults[1]);
    ASSERT_EQ(3u, fixture.m_Probe.m_CompletedOutputs[0]);
    ASSERT_EQ(6u, fixture.m_Probe.m_CompletedOutputs[1]);
    ASSERT_EQ(0u, DispatchGraphicsCompletions(fixture.m_Service));
    DeleteFixture(&fixture);
}

// A browser-style external pump performs only the requested amount of work and
// returns idle immediately when no work exists; it never needs a render frame.
TEST(GraphicsService, ExternalPumpHonorsWorkBudget)
{
    GraphicsServiceFixture fixture;
    CreateFixture(&fixture, EXECUTION_MODE_INLINE, 4, 1024);
    ASSERT_EQ(GRAPHICS_SERVICE_PUMP_IDLE, PumpGraphicsService(fixture.m_Service, 1));
    ASSERT_EQ(GRAPHICS_SERVICE_OK, EnqueueTestTask(&fixture, 1, 0));
    ASSERT_EQ(GRAPHICS_SERVICE_OK, EnqueueTestTask(&fixture, 2, 0));
    ASSERT_EQ(GRAPHICS_SERVICE_OK, EnqueueTestTask(&fixture, 3, 0));

    ASSERT_EQ(GRAPHICS_SERVICE_PUMP_PROGRESSED, PumpGraphicsService(fixture.m_Service, 1));
    ASSERT_EQ(1u, fixture.m_Probe.m_Executed);
    ASSERT_EQ(2u, GetGraphicsServiceStatus(fixture.m_Service).m_QueuedRequests);
    ASSERT_EQ(GRAPHICS_SERVICE_PUMP_PROGRESSED, PumpGraphicsService(fixture.m_Service, 1));
    ASSERT_EQ(2u, fixture.m_Probe.m_Executed);
    ASSERT_EQ(GRAPHICS_SERVICE_PUMP_PROGRESSED, PumpGraphicsService(fixture.m_Service, 1));
    ASSERT_EQ(GRAPHICS_SERVICE_PUMP_IDLE, PumpGraphicsService(fixture.m_Service, 1));
    ASSERT_EQ(3u, DispatchGraphicsCompletions(fixture.m_Service));
    DeleteFixture(&fixture);
}

// Execution callbacks may submit follow-up work, but cannot recursively pump the
// same owner service or bypass the outer pump's finite work budget.
TEST(GraphicsService, ExecuteReentryDefersNestedWork)
{
    GraphicsServiceFixture fixture;
    CreateFixture(&fixture, EXECUTION_MODE_INLINE, 4, 1024);
    fixture.m_Probe.m_EnqueueFromExecute = true;
    ASSERT_EQ(GRAPHICS_SERVICE_OK, EnqueueTestTask(&fixture, 1, 0));

    ASSERT_EQ(GRAPHICS_SERVICE_PUMP_PROGRESSED, PumpGraphicsService(fixture.m_Service, 1));
    ASSERT_EQ(GRAPHICS_SERVICE_OK, fixture.m_Probe.m_ChildEnqueueResult);
    ASSERT_EQ(GRAPHICS_SERVICE_PUMP_BUSY, fixture.m_Probe.m_NestedPumpResult);
    ASSERT_EQ(1u, fixture.m_Probe.m_Executed);
    ASSERT_EQ(1u, GetGraphicsServiceStatus(fixture.m_Service).m_QueuedRequests);
    ASSERT_EQ(1u, DispatchGraphicsCompletions(fixture.m_Service));

    PumpGraphicsService(fixture.m_Service, 4);
    ASSERT_EQ(1u, DispatchGraphicsCompletions(fixture.m_Service));
    ASSERT_EQ(2u, fixture.m_Probe.m_CompletedValues[1]);
    DeleteFixture(&fixture);
}

// A completion can enqueue and execute a child inline, but its completion belongs
// to the next game boundary rather than recursively extending the current batch.
TEST(GraphicsService, CompletionReentryDefersNewCompletions)
{
    GraphicsServiceFixture fixture;
    CreateFixture(&fixture, EXECUTION_MODE_INLINE, 4, 1024);
    fixture.m_Probe.m_EnqueueFromComplete = true;
    ASSERT_EQ(GRAPHICS_SERVICE_OK, EnqueueTestTask(&fixture, 1, 0));
    PumpGraphicsService(fixture.m_Service, 4);

    ASSERT_EQ(1u, DispatchGraphicsCompletions(fixture.m_Service));
    ASSERT_EQ(GRAPHICS_SERVICE_OK, fixture.m_Probe.m_ChildEnqueueResult);
    ASSERT_EQ(0u, fixture.m_Probe.m_NestedDispatchCount);
    ASSERT_EQ(2u, fixture.m_Probe.m_Executed);
    ASSERT_EQ(1u, fixture.m_Probe.m_Completed);
    ASSERT_EQ(1u, GetGraphicsServiceStatus(fixture.m_Service).m_PendingRequests);

    ASSERT_EQ(1u, DispatchGraphicsCompletions(fixture.m_Service));
    ASSERT_EQ(2u, fixture.m_Probe.m_Completed);
    ASSERT_EQ(0u, DispatchGraphicsCompletions(fixture.m_Service));
    ASSERT_FALSE(fixture.m_Probe.m_InvalidPayload);
    DeleteFixture(&fixture);
}

// Stop closes admission without cancelling accepted operations. Execution drains
// separately from game completion, and repeated stop/dispatch does not duplicate it.
TEST(GraphicsService, StopPreservesAcceptedWorkAndRejectsNewRequests)
{
    GraphicsServiceFixture fixture;
    CreateFixture(&fixture, EXECUTION_MODE_INLINE, 4, 1024);
    ASSERT_EQ(GRAPHICS_SERVICE_OK, EnqueueTestTask(&fixture, 1, 0));
    ASSERT_EQ(GRAPHICS_SERVICE_OK, EnqueueTestTask(&fixture, 2, -9));
    StopGraphicsService(fixture.m_Service);
    StopGraphicsService(fixture.m_Service);
    ASSERT_EQ(GRAPHICS_SERVICE_STOPPED, EnqueueTestTask(&fixture, 3, 0));

    PumpGraphicsService(fixture.m_Service, 4);
    ASSERT_EQ(GRAPHICS_SERVICE_PUMP_DRAINED, PumpGraphicsService(fixture.m_Service, 4));
    ASSERT_EQ(0u, fixture.m_Probe.m_Completed);
    ASSERT_EQ(2u, DispatchGraphicsCompletions(fixture.m_Service));
    ASSERT_EQ(0u, DispatchGraphicsCompletions(fixture.m_Service));
    ASSERT_EQ(1u, fixture.m_Probe.m_CompletedValues[0]);
    ASSERT_EQ(2u, fixture.m_Probe.m_CompletedValues[1]);
    ASSERT_EQ(-9, fixture.m_Probe.m_CompletedResults[1]);
    GraphicsServiceStatus status = GetGraphicsServiceStatus(fixture.m_Service);
    ASSERT_TRUE(status.m_Stopping);
    ASSERT_EQ((uint64_t)2, status.m_Accepted);
    ASSERT_EQ((uint64_t)2, status.m_Executed);
    ASSERT_EQ((uint64_t)2, status.m_Completed);
    ASSERT_EQ(0u, status.m_PendingRequests);
    DeleteFixture(&fixture);
}

// Explicit shutdown cancellation skips queued execution but still reports every
// accepted request once at the game completion boundary, with its owned payload.
TEST(GraphicsService, ShutdownCancellationDefersQueuedCompletions)
{
    GraphicsServiceFixture fixture;
    CreateFixture(&fixture, EXECUTION_MODE_INLINE, 4, 1024);
    ASSERT_EQ(GRAPHICS_SERVICE_OK, EnqueueTestTask(&fixture, 1, 0));
    ASSERT_EQ(GRAPHICS_SERVICE_OK, EnqueueTestTask(&fixture, 2, 0));
    CancelGraphicsService(fixture.m_Service, -99);
    CancelGraphicsService(fixture.m_Service, -100);

    ASSERT_EQ(0u, fixture.m_Probe.m_Executed);
    ASSERT_EQ(0u, fixture.m_Probe.m_Completed);
    ASSERT_EQ(GRAPHICS_SERVICE_STOPPED, EnqueueTestTask(&fixture, 3, 0));
    ASSERT_EQ(GRAPHICS_SERVICE_PUMP_DRAINED, PumpGraphicsService(fixture.m_Service, 4));
    ASSERT_EQ(2u, DispatchGraphicsCompletions(fixture.m_Service));
    ASSERT_EQ(-99, fixture.m_Probe.m_CompletedResults[0]);
    ASSERT_EQ(-99, fixture.m_Probe.m_CompletedResults[1]);
    ASSERT_EQ(0u, fixture.m_Probe.m_CompletedOutputs[0]);
    ASSERT_EQ(0u, fixture.m_Probe.m_CompletedOutputs[1]);
    ASSERT_EQ(0u, DispatchGraphicsCompletions(fixture.m_Service));
    ASSERT_EQ(0u, GetGraphicsServiceStatus(fixture.m_Service).m_PendingRequests);
    DeleteFixture(&fixture);
}

// A native owner processes copied startup requests before any frame exists, keeps
// outputs until game delivery, and wakes again after becoming idle.
TEST(GraphicsService, NativeOwnerProgressesWithoutFrames)
{
    if (!HasThreadedExecution())
    {
        return;
    }
    GraphicsServiceFixture fixture;
    CreateFixture(&fixture, EXECUTION_MODE_THREADED, 4, 1024);
    TestTaskPayload payload;
    GraphicsTask task = MakeTask(&fixture.m_Probe, &payload, 7, 0);
    ASSERT_EQ(GRAPHICS_SERVICE_OK, EnqueueGraphicsTask(fixture.m_Service, task));
    payload.m_Value = 99;

    GraphicsServiceExecutor executor;
    StartExecutor(&executor, &fixture, true);
    WaitForExecuted(fixture.m_Service, 1);
    ASSERT_EQ(0u, fixture.m_Probe.m_Completed);
    ASSERT_EQ(1u, DispatchGraphicsCompletions(fixture.m_Service));
    ASSERT_EQ(7u, fixture.m_Probe.m_CompletedValues[0]);
    ASSERT_EQ(21u, fixture.m_Probe.m_CompletedOutputs[0]);

    ASSERT_EQ(GRAPHICS_SERVICE_OK, EnqueueTestTask(&fixture, 11, 0));
    WaitForExecuted(fixture.m_Service, 2);
    ASSERT_EQ(1u, DispatchGraphicsCompletions(fixture.m_Service));
    StopGraphicsService(fixture.m_Service);
    dmThread::Join(executor.m_Thread);
    ASSERT_TRUE(executor.m_Released);
    ASSERT_EQ(33u, fixture.m_Probe.m_CompletedOutputs[1]);
    ASSERT_TRUE(fixture.m_Probe.m_Wakes >= 2);
    ASSERT_FALSE(fixture.m_Probe.m_WrongOwner);
    ASSERT_FALSE(fixture.m_Probe.m_InvalidPayload);
    DeleteFixture(&fixture);
}

// Shutdown while an operation is executing must finish that operation and every
// accepted successor. Explicit events hold the owner; no timing assumptions apply.
TEST(GraphicsService, NativeStopFinishesActiveAndQueuedWork)
{
    if (!HasThreadedExecution())
    {
        return;
    }
    GraphicsServiceFixture fixture;
    CreateFixture(&fixture, EXECUTION_MODE_THREADED, 4, 1024);
    fixture.m_Probe.m_BlockValue = 1;
    ASSERT_EQ(GRAPHICS_SERVICE_OK, EnqueueTestTask(&fixture, 1, 0));
    ASSERT_EQ(GRAPHICS_SERVICE_OK, EnqueueTestTask(&fixture, 2, 0));
    GraphicsServiceExecutor executor;
    StartExecutor(&executor, &fixture, true);

    dmMutex::Lock(fixture.m_Probe.m_Mutex);
    while (fixture.m_Probe.m_Entered == 0)
    {
        dmConditionVariable::Wait(fixture.m_Probe.m_Changed, fixture.m_Probe.m_Mutex);
    }
    dmMutex::Unlock(fixture.m_Probe.m_Mutex);

    StopGraphicsService(fixture.m_Service);
    GraphicsServiceStatus status = GetGraphicsServiceStatus(fixture.m_Service);
    ASSERT_EQ(1u, status.m_ExecutingRequests);
    ASSERT_EQ(1u, status.m_QueuedRequests);
    ASSERT_EQ(GRAPHICS_SERVICE_STOPPED, EnqueueTestTask(&fixture, 3, 0));
    ASSERT_EQ(0u, DispatchGraphicsCompletions(fixture.m_Service));

    dmMutex::Lock(fixture.m_Probe.m_Mutex);
    fixture.m_Probe.m_ReleaseExecution = true;
    dmConditionVariable::Broadcast(fixture.m_Probe.m_Changed);
    dmMutex::Unlock(fixture.m_Probe.m_Mutex);
    dmThread::Join(executor.m_Thread);

    ASSERT_TRUE(executor.m_Released);
    ASSERT_EQ(2u, DispatchGraphicsCompletions(fixture.m_Service));
    ASSERT_EQ(1u, fixture.m_Probe.m_CompletedValues[0]);
    ASSERT_EQ(2u, fixture.m_Probe.m_CompletedValues[1]);
    ASSERT_EQ(0u, DispatchGraphicsCompletions(fixture.m_Service));
    ASSERT_FALSE(fixture.m_Probe.m_WrongOwner);
    DeleteFixture(&fixture);
}

// Cancelling queued work cannot interrupt a callback already using its payload.
// The active operation finishes normally; only its queued successor is cancelled.
TEST(GraphicsService, NativeShutdownCancellationPreservesActiveOperation)
{
    if (!HasThreadedExecution())
    {
        return;
    }
    GraphicsServiceFixture fixture;
    CreateFixture(&fixture, EXECUTION_MODE_THREADED, 4, 1024);
    fixture.m_Probe.m_BlockValue = 1;
    ASSERT_EQ(GRAPHICS_SERVICE_OK, EnqueueTestTask(&fixture, 1, 0));
    ASSERT_EQ(GRAPHICS_SERVICE_OK, EnqueueTestTask(&fixture, 2, 0));
    GraphicsServiceExecutor executor;
    StartExecutor(&executor, &fixture, true);

    dmMutex::Lock(fixture.m_Probe.m_Mutex);
    while (fixture.m_Probe.m_Entered == 0)
    {
        dmConditionVariable::Wait(fixture.m_Probe.m_Changed, fixture.m_Probe.m_Mutex);
    }
    dmMutex::Unlock(fixture.m_Probe.m_Mutex);

    CancelGraphicsService(fixture.m_Service, -99);
    GraphicsServiceStatus status = GetGraphicsServiceStatus(fixture.m_Service);
    ASSERT_EQ(1u, status.m_ExecutingRequests);
    ASSERT_EQ(0u, status.m_QueuedRequests);
    ASSERT_EQ(0u, fixture.m_Probe.m_Completed);

    dmMutex::Lock(fixture.m_Probe.m_Mutex);
    fixture.m_Probe.m_ReleaseExecution = true;
    dmConditionVariable::Broadcast(fixture.m_Probe.m_Changed);
    dmMutex::Unlock(fixture.m_Probe.m_Mutex);
    dmThread::Join(executor.m_Thread);

    ASSERT_TRUE(executor.m_Released);
    ASSERT_EQ(1u, fixture.m_Probe.m_Executed);
    ASSERT_EQ(2u, DispatchGraphicsCompletions(fixture.m_Service));
    uint32_t seen = 0;
    for (uint32_t i = 0; i < 2; ++i)
    {
        uint32_t value = fixture.m_Probe.m_CompletedValues[i];
        ASSERT_TRUE(value == 1 || value == 2);
        seen |= 1u << (value - 1);
        ASSERT_EQ(value == 1 ? 0 : -99, fixture.m_Probe.m_CompletedResults[i]);
        ASSERT_EQ(value == 1 ? 3u : 0u, fixture.m_Probe.m_CompletedOutputs[i]);
    }
    ASSERT_EQ(3u, seen);
    ASSERT_EQ(0u, DispatchGraphicsCompletions(fixture.m_Service));
    ASSERT_FALSE(fixture.m_Probe.m_WrongOwner);
    DeleteFixture(&fixture);
}

struct TaskProducer
{
    GraphicsServiceFixture* m_Fixture;
    dmThread::Thread        m_Thread;
    uint32_t                m_FirstValue;
    uint32_t                m_Count;
    uint32_t                m_Accepted;
};

static void ProduceTasks(void* context)
{
    TaskProducer* producer = (TaskProducer*)context;
    for (uint32_t i = 0; i < producer->m_Count; ++i)
    {
        GraphicsServiceResult result = EnqueueTestTask(producer->m_Fixture, producer->m_FirstValue + i, 0);
        producer->m_Accepted += result == GRAPHICS_SERVICE_OK;
    }
}

// Worker threads can enqueue owned requests without becoming graphics owners.
// The owner is held until both finish so the test also checks copied stack data.
TEST(GraphicsService, WorkerProducersSubmitToOneGraphicsOwner)
{
    if (!HasThreadedExecution())
    {
        return;
    }
    GraphicsServiceFixture fixture;
    CreateFixture(&fixture, EXECUTION_MODE_THREADED, 16, 4096);
    GraphicsServiceExecutor executor;
    StartExecutor(&executor, &fixture, false);

    TaskProducer producers[2] = {};
    for (uint32_t i = 0; i < 2; ++i)
    {
        producers[i].m_Fixture = &fixture;
        producers[i].m_FirstValue = i * 8 + 1;
        producers[i].m_Count = 8;
        producers[i].m_Thread = dmThread::New(ProduceTasks, 0x80000, &producers[i], "graphics-producer");
    }
    for (uint32_t i = 0; i < 2; ++i)
    {
        dmThread::Join(producers[i].m_Thread);
        ASSERT_EQ(8u, producers[i].m_Accepted);
    }
    ASSERT_EQ(16u, GetGraphicsServiceStatus(fixture.m_Service).m_QueuedRequests);

    ResumeExecutor(&executor);
    WaitForExecuted(fixture.m_Service, 16);
    StopGraphicsService(fixture.m_Service);
    dmThread::Join(executor.m_Thread);
    ASSERT_EQ(16u, DispatchGraphicsCompletions(fixture.m_Service));

    uint32_t observed = 0;
    for (uint32_t i = 0; i < 16; ++i)
    {
        uint32_t value = fixture.m_Probe.m_CompletedValues[i];
        ASSERT_TRUE(value >= 1 && value <= 16);
        ASSERT_EQ(0u, observed & (1u << (value - 1)));
        observed |= 1u << (value - 1);
        ASSERT_EQ(value * 3, fixture.m_Probe.m_CompletedOutputs[i]);
    }
    ASSERT_EQ(0xffffu, observed);
    ASSERT_FALSE(fixture.m_Probe.m_WrongOwner);
    ASSERT_FALSE(fixture.m_Probe.m_InvalidPayload);
    DeleteFixture(&fixture);
}

struct ResourceServiceProbe
{
    GraphicsServiceFixture* m_Fixture;
    uint32_t                m_ServiceCount;
    uint32_t                m_CreateCount;
    uint32_t                m_PendingCount;
    uint32_t                m_DestroyCount;
};

static void ServiceResourceGraphics(void* context)
{
    ResourceServiceProbe* probe = (ResourceServiceProbe*)context;
    ++probe->m_ServiceCount;
    PumpGraphicsService(probe->m_Fixture->m_Service, 1);
    DispatchGraphicsCompletions(probe->m_Fixture->m_Service);
}

static dmResource::Result CreateGraphicsResource(const dmResource::ResourceCreateParams* params)
{
    ResourceServiceProbe* probe = (ResourceServiceProbe*)params->m_Context;
    ++probe->m_CreateCount;
    GraphicsServiceResult result = EnqueueTestTask(probe->m_Fixture, 13, 0);
    if (result != GRAPHICS_SERVICE_OK)
    {
        return dmResource::RESULT_OUT_OF_RESOURCES;
    }
    ResourceDescriptorSetResource(params->m_Resource, probe);
    return dmResource::RESULT_OK;
}

static dmResource::Result FinishGraphicsResource(const dmResource::ResourcePostCreateParams* params)
{
    ResourceServiceProbe* probe = (ResourceServiceProbe*)params->m_Context;
    if (probe->m_Fixture->m_Probe.m_Completed == 0)
    {
        ++probe->m_PendingCount;
        return dmResource::RESULT_PENDING;
    }
    return dmResource::RESULT_OK;
}

static dmResource::Result DestroyGraphicsResource(const dmResource::ResourceDestroyParams* params)
{
    ResourceServiceProbe* probe = (ResourceServiceProbe*)ResourceDescriptorGetResource(params->m_Resource);
    ++probe->m_DestroyCount;
    return dmResource::RESULT_OK;
}

static void LoadGraphicsResource(bool from_file)
{
    GraphicsServiceFixture fixture;
    CreateFixture(&fixture, EXECUTION_MODE_INLINE, 4, 1024);
    ResourceServiceProbe probe = {};
    probe.m_Fixture = &fixture;
    dmResource::NewFactoryParams params;
    params.m_MaxResources = 4;
    params.m_Service = ServiceResourceGraphics;
    params.m_ServiceContext = &probe;
#if defined(DM_PLATFORM_VENDOR)
    const char* mount_dir = "file:";
#else
    const char* mount_dir = ".";
#endif
    dmResource::HFactory factory = dmResource::NewFactory(&params, mount_dir);
    ASSERT_NE((dmResource::HFactory)0, factory);
    ASSERT_EQ(dmResource::RESULT_OK, dmResource::RegisterType(factory, "gst", &probe, 0,
                                                              CreateGraphicsResource, FinishGraphicsResource, DestroyGraphicsResource, 0));

    uint32_t source = 13;
    void* resource = 0;
    if (from_file)
    {
        ASSERT_EQ(dmResource::RESULT_OK, dmResource::AddFile(factory, "/bootstrap.gst", sizeof(source), &source));
        ASSERT_EQ(dmResource::RESULT_OK, dmResource::Get(factory, "/bootstrap.gst", &resource));
    }
    else
    {
        ASSERT_EQ(dmResource::RESULT_OK, dmResource::CreateResource(factory, "/bootstrap.gst", &source, sizeof(source), &resource));
    }

    ASSERT_EQ((void*)&probe, resource);
    ASSERT_EQ(1u, probe.m_CreateCount);
    ASSERT_TRUE(probe.m_PendingCount > 0);
    ASSERT_TRUE(probe.m_ServiceCount > 1);
    ASSERT_EQ(1u, fixture.m_Probe.m_Completed);
    ASSERT_EQ(39u, fixture.m_Probe.m_CompletedOutputs[0]);
    ASSERT_FALSE(fixture.m_Probe.m_WrongOwner);

    dmResource::Release(factory, resource);
    ASSERT_EQ(1u, probe.m_DestroyCount);
    dmResource::DeleteFactory(factory);
    DeleteFixture(&fixture);
}

// Initial resource creation may wait in PostCreate for graphics-owner work.
// The resource-loader service hook must finish it without an engine frame loop.
TEST(GraphicsService, SynchronousResourceCreationServicesPendingGraphics)
{
    LoadGraphicsResource(false);
}

// Synchronous Get follows the same service boundary as creation from supplied
// bytes. Loading a mounted resource cannot deadlock waiting for the first frame.
TEST(GraphicsService, SynchronousResourceGetServicesPendingGraphics)
{
    LoadGraphicsResource(true);
}

static int32_t TryCreateThreadedRenderer(dmGraphics::HContext graphics_context, void* payload, uint32_t size)
{
    TestTaskPayload* input = (TestTaskPayload*)payload;
    HGraphicsService service = input->m_Probe->m_Service;
    input->m_Output = IsRenderExecutionOwner(GetGraphicsServiceExecution(service), EXECUTION_OWNER_RENDER);
    RenderContextParams params;
    params.m_GraphicsService = service;
    return NewRenderContext(graphics_context, params) == 0 ? 0 : 1;
}

// Borrowing a threaded service must not bypass the renderer startup gate, even
// from its bound render owner: renderer initialization still mixes Lua and graphics.
TEST(GraphicsService, ThreadedServiceCannotBypassRendererStartupGate)
{
    if (!HasThreadedExecution())
    {
        return;
    }
    GraphicsServiceFixture fixture;
    CreateFixture(&fixture, EXECUTION_MODE_THREADED, 4, 1024);
    TestTaskPayload payload;
    GraphicsTask task = MakeTask(&fixture.m_Probe, &payload, 1, 0);
    task.m_Execute = TryCreateThreadedRenderer;
    ASSERT_EQ(GRAPHICS_SERVICE_OK, EnqueueGraphicsTask(fixture.m_Service, task));

    GraphicsServiceExecutor executor;
    StartExecutor(&executor, &fixture, true);
    WaitForExecuted(fixture.m_Service, 1);
    StopGraphicsService(fixture.m_Service);
    dmThread::Join(executor.m_Thread);

    ASSERT_TRUE(executor.m_Released);
    ASSERT_EQ(1u, DispatchGraphicsCompletions(fixture.m_Service));
    ASSERT_EQ(0, fixture.m_Probe.m_CompletedResults[0]);
    ASSERT_EQ(1u, fixture.m_Probe.m_CompletedOutputs[0]);
    ASSERT_TRUE(IsRenderExecutionOwner(GetGraphicsServiceExecution(fixture.m_Service), EXECUTION_OWNER_GAME));
    DeleteFixture(&fixture);
}

// Reject a mismatched borrowed service before touching the supplied graphics
// context. Startup failure must leave the service and its ownership usable.
TEST(GraphicsService, RendererRejectsGraphicsServiceContextMismatch)
{
    GraphicsServiceFixture fixture;
    CreateFixture(&fixture, EXECUTION_MODE_INLINE, 4, 1024);
    RenderContextParams params;
    params.m_GraphicsService = fixture.m_Service;
    dmGraphics::HContext other_context = (dmGraphics::HContext)(uintptr_t)1;
    ASSERT_EQ((HRenderContext)0, NewRenderContext(other_context, params));

    ASSERT_EQ(GRAPHICS_SERVICE_OK, EnqueueTestTask(&fixture, 7, 0));
    ASSERT_EQ(GRAPHICS_SERVICE_PUMP_PROGRESSED, PumpGraphicsService(fixture.m_Service, 1));
    ASSERT_EQ(1u, DispatchGraphicsCompletions(fixture.m_Service));
    ASSERT_EQ(21u, fixture.m_Probe.m_CompletedOutputs[0]);
    ASSERT_FALSE(fixture.m_Probe.m_WrongOwner);
    DeleteFixture(&fixture);
}

struct RendererStartupProbe
{
    HGraphicsService m_Service;
    HRenderContext   m_RenderContext;
    uint8_t          m_WasGameOwner : 1;
};

static void CreateRendererOnWorker(void* context)
{
    RendererStartupProbe* probe = (RendererStartupProbe*)context;
    probe->m_WasGameOwner = IsRenderExecutionOwner(GetGraphicsServiceExecution(probe->m_Service), EXECUTION_OWNER_GAME);
    RenderContextParams params;
    params.m_GraphicsService = probe->m_Service;
    probe->m_RenderContext = NewRenderContext(0, params);
}

// An inline service still belongs to its creating thread. Borrowing it from a
// worker must fail before renderer initialization, even in a non-threaded build.
TEST(GraphicsService, RendererRejectsInlineServiceFromAnotherThread)
{
    if (!GetExecutionCapabilities(true).m_ThreadsSupported)
    {
        return;
    }
    GraphicsServiceFixture fixture;
    CreateFixture(&fixture, EXECUTION_MODE_INLINE, 4, 1024);
    RendererStartupProbe probe = {};
    probe.m_Service = fixture.m_Service;
    probe.m_RenderContext = (HRenderContext)(uintptr_t)1;
    dmThread::Thread worker = dmThread::New(CreateRendererOnWorker, 0x80000, &probe, "renderer-startup");
    dmThread::Join(worker);

    ASSERT_FALSE(probe.m_WasGameOwner);
    ASSERT_EQ((HRenderContext)0, probe.m_RenderContext);
    ASSERT_TRUE(IsRenderExecutionOwner(GetGraphicsServiceExecution(fixture.m_Service), EXECUTION_OWNER_GAME));
    ASSERT_EQ(GRAPHICS_SERVICE_OK, EnqueueTestTask(&fixture, 7, 0));
    ASSERT_EQ(GRAPHICS_SERVICE_PUMP_PROGRESSED, PumpGraphicsService(fixture.m_Service, 1));
    ASSERT_EQ(1u, DispatchGraphicsCompletions(fixture.m_Service));
    ASSERT_EQ(21u, fixture.m_Probe.m_CompletedOutputs[0]);
    DeleteFixture(&fixture);
}

extern "C" void dmExportedSymbols();
extern "C" void ResourceProviderFile();

int main(int argc, char** argv)
{
    dmExportedSymbols();
    ResourceProviderFile();
    TestMainPlatformInit();
    jc_test_init(&argc, argv);
    return jc_test_run_all();
}
