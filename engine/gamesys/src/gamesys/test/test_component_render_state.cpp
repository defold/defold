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

#include <stdlib.h>
#include <string.h>

#include <testmain/testmain.h>

#include "gamesys/component_render_state_private.h"

using namespace dmRender;
using namespace dmGameSystem;

struct ComponentFixture
{
    HRenderExecution             m_Execution;
    HComponentRenderStateContext m_Components;
    HRenderFrameQueue             m_Queue;
};

struct ComponentProbe
{
    uint64_t          m_Sum;
    HRenderExecution  m_Execution;
    RenderFrameResult m_InitializeResult;
    RenderFrameResult m_ConsumeResult;
    uint32_t          m_Initialized;
    uint32_t          m_Consumed;
    uint32_t          m_Destroyed;
    uint32_t          m_InitValue;
    uint32_t          m_FirstTable;
    uint32_t          m_TableCount;
    // This fixture executes all callbacks inline on the test thread.
    uint8_t           m_PartialState : 1;
    uint8_t           m_NullState : 1;
    uint8_t           m_WrongOwner : 1;
    uint8_t           m_InvalidInput : 1;
};

struct ComponentState
{
    uint32_t m_Value;
};

struct ComponentProducer
{
    uint32_t          m_Values[4];
    uint32_t          m_Tables;
    uint32_t          m_Calls;
    RenderFrameResult m_Result;
};

static RenderFrameResult InitializeComponent(void* renderer_context, const void* init_data, uint32_t init_size, void** state)
{
    ComponentProbe* probe = (ComponentProbe*)renderer_context;
    probe->m_WrongOwner |= !IsRenderExecutionOwner(probe->m_Execution, EXECUTION_OWNER_RENDER);
    ++probe->m_Initialized;
    if (init_size != sizeof(uint32_t) || !init_data)
    {
        probe->m_InvalidInput = true;
        return RENDER_FRAME_INVALID_ARGUMENT;
    }
    probe->m_InitValue = *(const uint32_t*)init_data;
    if (!probe->m_NullState && (probe->m_InitializeResult == RENDER_FRAME_OK || probe->m_PartialState))
    {
        ComponentState* component_state = (ComponentState*)malloc(sizeof(ComponentState));
        if (!component_state)
            return RENDER_FRAME_OUT_OF_MEMORY;
        component_state->m_Value = probe->m_InitValue;
        *state = component_state;
    }
    return probe->m_InitializeResult;
}

static RenderFrameResult ConsumeComponent(void* renderer_context, void* state, const RenderFrameView& frame,
    uint32_t first_table, uint32_t table_count)
{
    ComponentProbe* probe = (ComponentProbe*)renderer_context;
    probe->m_WrongOwner |= !IsRenderExecutionOwner(probe->m_Execution, EXECUTION_OWNER_RENDER);
    ++probe->m_Consumed;
    probe->m_FirstTable = first_table;
    probe->m_TableCount = table_count;
    probe->m_InvalidInput |= (state == 0) != probe->m_NullState;
    if (state)
        probe->m_InvalidInput |= ((ComponentState*)state)->m_Value != probe->m_InitValue;
    for (uint32_t i = 0; i < table_count; ++i)
    {
        const RenderFrameTable& table = frame.m_Tables[first_table + i];
        if (table.m_Type != 123 || table.m_Count != 4 || table.m_Stride != sizeof(uint32_t))
        {
            probe->m_InvalidInput = true;
            continue;
        }
        const uint32_t* values = (const uint32_t*)GetRenderFrameTableData(frame, first_table + i);
        for (uint32_t j = 0; j < 4; ++j)
            probe->m_Sum += values[j];
    }
    return probe->m_ConsumeResult;
}

static void DestroyComponent(void* renderer_context, void* state)
{
    ComponentProbe* probe = (ComponentProbe*)renderer_context;
    probe->m_WrongOwner |= !IsRenderExecutionOwner(probe->m_Execution, EXECUTION_OWNER_RENDER);
    ++probe->m_Destroyed;
    if (state)
        probe->m_InvalidInput |= ((ComponentState*)state)->m_Value != probe->m_InitValue;
    free(state);
}

static RenderFrameResult CaptureComponentValues(HRenderFrame frame, void* producer_context)
{
    ComponentProducer* producer = (ComponentProducer*)producer_context;
    ++producer->m_Calls;
    for (uint32_t i = 0; i < producer->m_Tables; ++i)
    {
        uint32_t table;
        RenderFrameResult result = CopyRenderFrameTable(frame, 123, 4, sizeof(uint32_t), 4, producer->m_Values, &table);
        if (result != RENDER_FRAME_OK)
            return result;
    }
    return producer->m_Result;
}

static bool CreateComponentFixture(ComponentFixture* fixture, const RenderFrameLimits& limits)
{
    memset(fixture, 0, sizeof(*fixture));
    ExecutionSelection selection;
    fixture->m_Execution = NewRenderExecution(EXECUTION_MODE_INLINE, false, &selection);
    if (!fixture->m_Execution)
        return false;

    fixture->m_Components = NewComponentRenderStateContext(fixture->m_Execution);
    if (!fixture->m_Components)
        return false;

    RenderFrameQueueParams params = {};
    params.m_Execution = fixture->m_Execution;
    params.m_Limits = limits;
    params.m_Consume = ConsumeComponentRenderFrame;
    params.m_ConsumeContext = fixture->m_Components;
    params.m_Service = ServiceComponentRenderState;
    params.m_ServiceContext = fixture->m_Components;

    fixture->m_Queue = NewRenderFrameQueue(params);
    if (!fixture->m_Queue)
        return false;

    SetComponentRenderStateQueue(fixture->m_Components, fixture->m_Queue);
    return true;
}

static void DeleteComponentFixture(ComponentFixture* fixture)
{
    ASSERT_EQ(0u, GetComponentRenderStateWorldCount(fixture->m_Components));
    StopRenderFrameQueue(fixture->m_Queue);
    ASSERT_EQ(RENDER_FRAME_PUMP_DRAINED, PumpRenderFrameQueue(fixture->m_Queue, true));
    DeleteComponentRenderStateContext(fixture->m_Components);
    DeleteRenderFrameQueue(fixture->m_Queue);
    DeleteRenderExecution(fixture->m_Execution);
}

static HComponentRenderWorld CreateComponentWorld(ComponentFixture* fixture, ComponentProbe* probe, uint32_t init_value)
{
    probe->m_Execution = fixture->m_Execution;
    ComponentRenderCallbacks callbacks = { InitializeComponent, ConsumeComponent, DestroyComponent };
    return NewComponentRenderWorld(fixture->m_Components, callbacks, probe, &init_value, sizeof(init_value));
}

// Verify initialization bytes, callback descriptors and producer payloads remain valid after their original storage is changed/freed.
TEST(ComponentRenderState, CopiedInputsOutliveGameStorage)
{
    ComponentFixture fixture;
    RenderFrameLimits limits = { 1024, 16, 16 };
    ASSERT_TRUE(CreateComponentFixture(&fixture, limits));
    ComponentProbe probe = {};
    probe.m_Execution = fixture.m_Execution;
    ComponentRenderCallbacks callbacks = { InitializeComponent, ConsumeComponent, DestroyComponent };
    uint32_t* init_data = (uint32_t*)malloc(sizeof(uint32_t));
    ASSERT_NE((uint32_t*)0, init_data);
    *init_data = 71;
    HComponentRenderWorld world = NewComponentRenderWorld(fixture.m_Components, callbacks, &probe, init_data, sizeof(*init_data));
    ASSERT_NE((HComponentRenderWorld)0, world);
    *init_data = 999;
    free(init_data);
    memset(&callbacks, 0, sizeof(callbacks));
    ComponentProducer* producer = (ComponentProducer*)calloc(1, sizeof(ComponentProducer));
    ASSERT_NE((ComponentProducer*)0, producer);
    producer->m_Tables = 1;
    for (uint32_t i = 0; i < 4; ++i)
        producer->m_Values[i] = i + 10;
    RenderFrameTicket ticket;
    ASSERT_EQ(RENDER_FRAME_OK, BeginRenderFrameCapture(fixture.m_Queue, &ticket));
    ASSERT_EQ(RENDER_FRAME_OK, CaptureComponentRenderWorld(world, ticket.m_Frame, CaptureComponentValues, producer));
    ASSERT_EQ(1u, producer->m_Calls);
    memset(producer, 0xcd, sizeof(*producer));
    free(producer);
    DeleteComponentRenderWorld(world);
    ASSERT_EQ(1u, GetComponentRenderStateWorldCount(fixture.m_Components));
    ASSERT_EQ(0u, probe.m_Initialized);
    ASSERT_EQ(RENDER_FRAME_OK, PublishRenderFrame(fixture.m_Queue, ticket));
    ASSERT_EQ(1u, probe.m_Initialized);
    ASSERT_EQ(71u, probe.m_InitValue);
    ASSERT_EQ(1u, probe.m_Consumed);
    ASSERT_EQ((uint64_t)46, probe.m_Sum);
    ASSERT_EQ(0u, probe.m_Destroyed);
    ASSERT_EQ(1u, RetireRenderFrames(fixture.m_Queue));
    ASSERT_EQ(0u, probe.m_Destroyed);
    ASSERT_EQ(RENDER_FRAME_PUMP_IDLE, PumpRenderFrameQueue(fixture.m_Queue, false));
    ASSERT_EQ(1u, probe.m_Destroyed);
    ASSERT_EQ(0u, GetComponentRenderStateWorldCount(fixture.m_Components));
    ASSERT_EQ(RENDER_FRAME_OK, GetComponentRenderStateResult(fixture.m_Components));
    ASSERT_FALSE(probe.m_WrongOwner);
    ASSERT_FALSE(probe.m_InvalidInput);
    DeleteComponentFixture(&fixture);
}

// Verify persistent renderer state initializes once across frames and survives CPU retirement while its game owner still exists.
TEST(ComponentRenderState, PersistentWorldAcrossFrames)
{
    ComponentFixture fixture;
    RenderFrameLimits limits = { 1024, 16, 16 };
    ASSERT_TRUE(CreateComponentFixture(&fixture, limits));
    ComponentProbe probe = {};
    HComponentRenderWorld world = CreateComponentWorld(&fixture, &probe, 7);
    ASSERT_NE((HComponentRenderWorld)0, world);
    ComponentProducer producer = { { 1, 2, 3, 4 }, 1, 0, RENDER_FRAME_OK };
    for (uint32_t i = 0; i < 4; ++i)
    {
        RenderFrameTicket ticket;
        ASSERT_EQ(RENDER_FRAME_OK, BeginRenderFrameCapture(fixture.m_Queue, &ticket));
        ASSERT_EQ(RENDER_FRAME_OK, CaptureComponentRenderWorld(world, ticket.m_Frame, CaptureComponentValues, &producer));
        ASSERT_EQ(RENDER_FRAME_OK, PublishRenderFrame(fixture.m_Queue, ticket));
        ASSERT_EQ(1u, probe.m_Initialized);
        ASSERT_EQ(i + 1, probe.m_Consumed);
        ASSERT_EQ(1u, RetireRenderFrames(fixture.m_Queue));
        ASSERT_EQ(RENDER_FRAME_PUMP_IDLE, PumpRenderFrameQueue(fixture.m_Queue, false));
        ASSERT_EQ(0u, probe.m_Destroyed);
        ASSERT_EQ(1u, GetComponentRenderStateWorldCount(fixture.m_Components));
    }
    ASSERT_EQ((uint64_t)40, probe.m_Sum);
    DeleteComponentRenderWorld(world);
    ASSERT_EQ(RENDER_FRAME_PUMP_IDLE, PumpRenderFrameQueue(fixture.m_Queue, false));
    ASSERT_EQ(1u, probe.m_Destroyed);
    ASSERT_EQ(0u, GetComponentRenderStateWorldCount(fixture.m_Components));
    ASSERT_FALSE(probe.m_WrongOwner);
    ASSERT_FALSE(probe.m_InvalidInput);
    DeleteComponentFixture(&fixture);
}

// Verify a closed world with no accepted render work is reaped by no-frame service without creating/destroying GPU state.
TEST(ComponentRenderState, NeverRenderedWorld)
{
    ComponentFixture fixture;
    RenderFrameLimits limits = { 0, 0, 0 };
    ASSERT_TRUE(CreateComponentFixture(&fixture, limits));
    ComponentProbe probe = {};
    HComponentRenderWorld world = CreateComponentWorld(&fixture, &probe, 7);
    ASSERT_NE((HComponentRenderWorld)0, world);
    DeleteComponentRenderWorld(world);
    ASSERT_EQ(1u, GetComponentRenderStateWorldCount(fixture.m_Components));
    ASSERT_EQ(RENDER_FRAME_PUMP_IDLE, PumpRenderFrameQueue(fixture.m_Queue, false));
    ASSERT_EQ(0u, GetComponentRenderStateWorldCount(fixture.m_Components));
    ASSERT_EQ(0u, probe.m_Initialized);
    ASSERT_EQ(0u, probe.m_Consumed);
    ASSERT_EQ(0u, probe.m_Destroyed);
    ASSERT_EQ(RENDER_FRAME_OK, GetComponentRenderStateResult(fixture.m_Components));
    DeleteComponentFixture(&fixture);
}

// Verify failed initialization cleans returned partial state once, never retries, and does not suppress another accepted world's work.
TEST(ComponentRenderState, InitializationFailureCleanupAndContinuation)
{
    const bool partial_states[] = { false, true };
    for (uint32_t test = 0; test < 2; ++test)
    {
        ComponentFixture fixture;
        RenderFrameLimits limits = { 1024, 16, 16 };
        ASSERT_TRUE(CreateComponentFixture(&fixture, limits));
        ComponentProbe failed = {};
        failed.m_InitializeResult = RENDER_FRAME_OUT_OF_MEMORY;
        failed.m_PartialState = partial_states[test];
        ComponentProbe healthy = {};
        HComponentRenderWorld failed_world = CreateComponentWorld(&fixture, &failed, 11);
        HComponentRenderWorld healthy_world = CreateComponentWorld(&fixture, &healthy, 12);
        ASSERT_NE((HComponentRenderWorld)0, failed_world);
        ASSERT_NE((HComponentRenderWorld)0, healthy_world);
        ComponentProducer producer = { { 1, 2, 3, 4 }, 1, 0, RENDER_FRAME_OK };
        for (uint32_t i = 0; i < 2; ++i)
        {
            RenderFrameTicket ticket;
            ASSERT_EQ(RENDER_FRAME_OK, BeginRenderFrameCapture(fixture.m_Queue, &ticket));
            ASSERT_EQ(RENDER_FRAME_OK, CaptureComponentRenderWorld(failed_world, ticket.m_Frame, CaptureComponentValues, &producer));
            ASSERT_EQ(RENDER_FRAME_OK, CaptureComponentRenderWorld(healthy_world, ticket.m_Frame, CaptureComponentValues, &producer));
            ASSERT_EQ(RENDER_FRAME_OK, PublishRenderFrame(fixture.m_Queue, ticket));
            ASSERT_EQ(1u, failed.m_Initialized);
            ASSERT_EQ(0u, failed.m_Consumed);
            ASSERT_EQ(partial_states[test] ? 1u : 0u, failed.m_Destroyed);
            ASSERT_EQ(i + 1, healthy.m_Consumed);
            ASSERT_EQ(RENDER_FRAME_OUT_OF_MEMORY, GetComponentRenderStateResult(fixture.m_Components));
            ASSERT_EQ(1u, RetireRenderFrames(fixture.m_Queue));
        }
        DeleteComponentRenderWorld(failed_world);
        DeleteComponentRenderWorld(healthy_world);
        ASSERT_EQ(RENDER_FRAME_PUMP_IDLE, PumpRenderFrameQueue(fixture.m_Queue, false));
        ASSERT_EQ(0u, GetComponentRenderStateWorldCount(fixture.m_Components));
        ASSERT_EQ(partial_states[test] ? 1u : 0u, failed.m_Destroyed);
        ASSERT_EQ(1u, healthy.m_Destroyed);
        ASSERT_FALSE(failed.m_WrongOwner);
        ASSERT_FALSE(healthy.m_WrongOwner);
        ASSERT_FALSE(failed.m_InvalidInput);
        ASSERT_FALSE(healthy.m_InvalidInput);
        DeleteComponentFixture(&fixture);
    }
}

// Verify consume errors preserve other accepted entries and the first diagnostic, while dispatch spans exclude unrelated tables.
TEST(ComponentRenderState, ConsumeErrorPreservesAcceptedWork)
{
    ComponentFixture fixture;
    RenderFrameLimits limits = { 1024, 16, 16 };
    ASSERT_TRUE(CreateComponentFixture(&fixture, limits));
    ComponentProbe first = {};
    ComponentProbe second = {};
    first.m_ConsumeResult = RENDER_FRAME_INVALID_STATE;
    second.m_ConsumeResult = RENDER_FRAME_OUT_OF_MEMORY;
    HComponentRenderWorld first_world = CreateComponentWorld(&fixture, &first, 1);
    HComponentRenderWorld second_world = CreateComponentWorld(&fixture, &second, 2);
    ASSERT_NE((HComponentRenderWorld)0, first_world);
    ASSERT_NE((HComponentRenderWorld)0, second_world);
    RenderFrameTicket ticket;
    ASSERT_EQ(RENDER_FRAME_OK, BeginRenderFrameCapture(fixture.m_Queue, &ticket));
    uint32_t unrelated = 999;
    uint32_t index;
    ASSERT_EQ(RENDER_FRAME_OK, CopyRenderFrameTable(ticket.m_Frame, 456, 1, sizeof(unrelated), 4, &unrelated, &index));
    ComponentProducer one = { { 1, 2, 3, 4 }, 1, 0, RENDER_FRAME_OK };
    ComponentProducer two = { { 10, 20, 30, 40 }, 2, 0, RENDER_FRAME_OK };
    ASSERT_EQ(RENDER_FRAME_OK, CaptureComponentRenderWorld(first_world, ticket.m_Frame, CaptureComponentValues, &one));
    ASSERT_EQ(RENDER_FRAME_OK, CaptureComponentRenderWorld(second_world, ticket.m_Frame, CaptureComponentValues, &two));
    DeleteComponentRenderWorld(first_world);
    DeleteComponentRenderWorld(second_world);
    ASSERT_EQ(RENDER_FRAME_OK, PublishRenderFrame(fixture.m_Queue, ticket));
    ASSERT_EQ(1u, first.m_Consumed);
    ASSERT_EQ(1u, second.m_Consumed);
    ASSERT_EQ(2u, first.m_FirstTable);
    ASSERT_EQ(1u, first.m_TableCount);
    ASSERT_EQ(4u, second.m_FirstTable);
    ASSERT_EQ(2u, second.m_TableCount);
    ASSERT_EQ((uint64_t)10, first.m_Sum);
    ASSERT_EQ((uint64_t)200, second.m_Sum);
    ASSERT_EQ(RENDER_FRAME_INVALID_STATE, GetComponentRenderStateResult(fixture.m_Components));
    ASSERT_EQ(1u, RetireRenderFrames(fixture.m_Queue));
    ASSERT_EQ(RENDER_FRAME_PUMP_IDLE, PumpRenderFrameQueue(fixture.m_Queue, false));
    ASSERT_EQ(1u, first.m_Destroyed);
    ASSERT_EQ(1u, second.m_Destroyed);
    ASSERT_FALSE(first.m_InvalidInput);
    ASSERT_FALSE(second.m_InvalidInput);
    DeleteComponentFixture(&fixture);
}

// Verify a callback error with no payload poisons the complete capture, including earlier successfully captured components.
TEST(ComponentRenderState, EmptyCaptureFailureRejectsWholeFrame)
{
    ComponentFixture fixture;
    RenderFrameLimits limits = { 1024, 16, 16 };
    ASSERT_TRUE(CreateComponentFixture(&fixture, limits));
    ComponentProbe first = {};
    ComponentProbe second = {};
    HComponentRenderWorld first_world = CreateComponentWorld(&fixture, &first, 1);
    HComponentRenderWorld second_world = CreateComponentWorld(&fixture, &second, 2);
    ASSERT_NE((HComponentRenderWorld)0, first_world);
    ASSERT_NE((HComponentRenderWorld)0, second_world);
    RenderFrameTicket ticket;
    ASSERT_EQ(RENDER_FRAME_OK, BeginRenderFrameCapture(fixture.m_Queue, &ticket));
    ComponentProducer one = { { 1, 2, 3, 4 }, 1, 0, RENDER_FRAME_OK };
    ComponentProducer failed = { { 0, 0, 0, 0 }, 0, 0, RENDER_FRAME_INVALID_ARGUMENT };
    ASSERT_EQ(RENDER_FRAME_OK, CaptureComponentRenderWorld(first_world, ticket.m_Frame, CaptureComponentValues, &one));
    ASSERT_EQ(RENDER_FRAME_INVALID_ARGUMENT, CaptureComponentRenderWorld(second_world, ticket.m_Frame, CaptureComponentValues, &failed));
    ASSERT_EQ(1u, failed.m_Calls);
    ASSERT_EQ(2u, GetRenderFrameStatistics(ticket.m_Frame).m_ReferenceCount);
    ASSERT_EQ(RENDER_FRAME_INVALID_ARGUMENT, PublishRenderFrame(fixture.m_Queue, ticket));
    ASSERT_EQ(0u, first.m_Initialized);
    ASSERT_EQ(0u, second.m_Initialized);
    DeleteComponentRenderWorld(first_world);
    DeleteComponentRenderWorld(second_world);
    ASSERT_EQ(RENDER_FRAME_OK, CancelRenderFrameCapture(fixture.m_Queue, ticket));
    ASSERT_EQ(RENDER_FRAME_PUMP_IDLE, PumpRenderFrameQueue(fixture.m_Queue, false));
    ASSERT_EQ(0u, GetComponentRenderStateWorldCount(fixture.m_Components));
    ASSERT_EQ(0u, first.m_Destroyed);
    ASSERT_EQ(0u, second.m_Destroyed);
    ASSERT_EQ(RENDER_FRAME_OK, GetComponentRenderStateResult(fixture.m_Components));
    DeleteComponentFixture(&fixture);
}

// Verify arena, table and retained-reference overflow reject publication and cancellation never leaks a world record.
TEST(ComponentRenderState, CaptureCapacityFailures)
{
    struct CapacityCase
    {
        RenderFrameLimits m_Limits;
        uint32_t m_ProducerCalls;
        uint32_t m_References;
    };
    const CapacityCase cases[] = {
        { { 0, 4, 4 }, 0, 0 },
        { { 64, 0, 4 }, 0, 0 },
        { { 64, 4, 0 }, 0, 0 },
        { { 64, 1, 1 }, 1, 1 },
        { { 16, 4, 1 }, 1, 1 },
    };
    for (uint32_t i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i)
    {
        ComponentFixture fixture;
        ASSERT_TRUE(CreateComponentFixture(&fixture, cases[i].m_Limits));
        ComponentProbe probe = {};
        HComponentRenderWorld world = CreateComponentWorld(&fixture, &probe, 1);
        ASSERT_NE((HComponentRenderWorld)0, world);
        ComponentProducer producer = { { 1, 2, 3, 4 }, 1, 0, RENDER_FRAME_OK };
        RenderFrameTicket ticket;
        ASSERT_EQ(RENDER_FRAME_OK, BeginRenderFrameCapture(fixture.m_Queue, &ticket));
        ASSERT_EQ(RENDER_FRAME_CAPACITY_EXCEEDED, CaptureComponentRenderWorld(world, ticket.m_Frame, CaptureComponentValues, &producer));
        ASSERT_EQ(cases[i].m_ProducerCalls, producer.m_Calls);
        ASSERT_EQ(cases[i].m_References, GetRenderFrameStatistics(ticket.m_Frame).m_ReferenceCount);
        ASSERT_EQ(RENDER_FRAME_CAPACITY_EXCEEDED, PublishRenderFrame(fixture.m_Queue, ticket));
        DeleteComponentRenderWorld(world);
        ASSERT_EQ(RENDER_FRAME_OK, CancelRenderFrameCapture(fixture.m_Queue, ticket));
        ASSERT_EQ(RENDER_FRAME_PUMP_IDLE, PumpRenderFrameQueue(fixture.m_Queue, false));
        ASSERT_EQ(0u, GetComponentRenderStateWorldCount(fixture.m_Components));
        ASSERT_EQ(0u, probe.m_Initialized);
        ASSERT_EQ(0u, probe.m_Destroyed);
        DeleteComponentFixture(&fixture);
    }
}

struct ReferenceProducer
{
    uint32_t m_Resource;
    uint32_t m_Releases;
};

static void ReleaseProducerResource(void* context, void* resource)
{
    ReferenceProducer* producer = (ReferenceProducer*)context;
    producer->m_Releases += *(uint32_t*)resource;
}

static RenderFrameResult CaptureReferenceThenFail(HRenderFrame frame, void* context)
{
    ReferenceProducer* producer = (ReferenceProducer*)context;
    RenderFrameResult result = AddRenderFrameReference(frame, &producer->m_Resource, ReleaseProducerResource, producer);
    return result == RENDER_FRAME_OK ? RENDER_FRAME_OUT_OF_MEMORY : result;
}

// Verify partial capture cleanup releases successfully adopted producer dependencies once while preserving failed-adoption ownership.
TEST(ComponentRenderState, PartialCaptureReferenceCleanup)
{
    for (uint32_t capacity = 1; capacity <= 2; ++capacity)
    {
        ComponentFixture fixture;
        RenderFrameLimits limits = { 128, 4, capacity };
        ASSERT_TRUE(CreateComponentFixture(&fixture, limits));
        ComponentProbe probe = {};
        HComponentRenderWorld world = CreateComponentWorld(&fixture, &probe, 1);
        ASSERT_NE((HComponentRenderWorld)0, world);
        ReferenceProducer producer = { 1, 0 };
        RenderFrameTicket ticket;
        ASSERT_EQ(RENDER_FRAME_OK, BeginRenderFrameCapture(fixture.m_Queue, &ticket));
        RenderFrameResult error = capacity == 1 ? RENDER_FRAME_CAPACITY_EXCEEDED : RENDER_FRAME_OUT_OF_MEMORY;
        ASSERT_EQ(error, CaptureComponentRenderWorld(world, ticket.m_Frame, CaptureReferenceThenFail, &producer));
        ASSERT_EQ(error, PublishRenderFrame(fixture.m_Queue, ticket));
        DeleteComponentRenderWorld(world);
        ASSERT_EQ(RENDER_FRAME_OK, CancelRenderFrameCapture(fixture.m_Queue, ticket));
        ASSERT_EQ(capacity == 1 ? 0u : 1u, producer.m_Releases);
        ASSERT_EQ(0u, RetireRenderFrames(fixture.m_Queue));
        ASSERT_EQ(RENDER_FRAME_PUMP_IDLE, PumpRenderFrameQueue(fixture.m_Queue, false));
        ASSERT_EQ(0u, GetComponentRenderStateWorldCount(fixture.m_Components));
        if (capacity == 1)
            ReleaseProducerResource(&producer, &producer.m_Resource);
        ASSERT_EQ(1u, producer.m_Releases);
        DeleteComponentFixture(&fixture);
        ASSERT_EQ(1u, producer.m_Releases);
    }
}

// Verify successful zero-table capture and stateless initialization still execute one accepted callback and paired destruction.
TEST(ComponentRenderState, EmptyPayloadAndNullState)
{
    ComponentFixture fixture;
    RenderFrameLimits limits = { 64, 1, 1 };
    ASSERT_TRUE(CreateComponentFixture(&fixture, limits));
    ComponentProbe probe = {};
    probe.m_NullState = true;
    HComponentRenderWorld world = CreateComponentWorld(&fixture, &probe, 1);
    ASSERT_NE((HComponentRenderWorld)0, world);
    ComponentProducer producer = {};
    RenderFrameTicket ticket;
    ASSERT_EQ(RENDER_FRAME_OK, BeginRenderFrameCapture(fixture.m_Queue, &ticket));
    ASSERT_EQ(RENDER_FRAME_OK, CaptureComponentRenderWorld(world, ticket.m_Frame, CaptureComponentValues, &producer));
    DeleteComponentRenderWorld(world);
    ASSERT_EQ(RENDER_FRAME_OK, PublishRenderFrame(fixture.m_Queue, ticket));
    ASSERT_EQ(1u, probe.m_Initialized);
    ASSERT_EQ(1u, probe.m_Consumed);
    ASSERT_EQ(0u, probe.m_TableCount);
    ASSERT_EQ(1u, RetireRenderFrames(fixture.m_Queue));
    ASSERT_EQ(RENDER_FRAME_PUMP_IDLE, PumpRenderFrameQueue(fixture.m_Queue, false));
    ASSERT_EQ(1u, probe.m_Destroyed);
    ASSERT_FALSE(probe.m_InvalidInput);
    DeleteComponentFixture(&fixture);
}

// Verify invalid callback registrations/init inputs create no record, and a missing producer poisons capture before any reference is adopted.
TEST(ComponentRenderState, InvalidArguments)
{
    ComponentFixture fixture;
    RenderFrameLimits limits = { 128, 4, 4 };
    ASSERT_TRUE(CreateComponentFixture(&fixture, limits));
    ComponentProbe probe = {};
    probe.m_Execution = fixture.m_Execution;
    uint32_t init = 1;
    const ComponentRenderCallbacks invalid[] = {
        { 0, ConsumeComponent, DestroyComponent },
        { InitializeComponent, 0, DestroyComponent },
        { InitializeComponent, ConsumeComponent, 0 },
    };
    for (uint32_t i = 0; i < sizeof(invalid) / sizeof(invalid[0]); ++i)
        ASSERT_EQ((HComponentRenderWorld)0, NewComponentRenderWorld(fixture.m_Components, invalid[i], &probe, &init, sizeof(init)));
    ComponentRenderCallbacks callbacks = { InitializeComponent, ConsumeComponent, DestroyComponent };
    ASSERT_EQ((HComponentRenderWorld)0, NewComponentRenderWorld(fixture.m_Components, callbacks, &probe, 0, sizeof(init)));
    ASSERT_EQ(0u, GetComponentRenderStateWorldCount(fixture.m_Components));
    HComponentRenderWorld world = CreateComponentWorld(&fixture, &probe, 1);
    ASSERT_NE((HComponentRenderWorld)0, world);
    RenderFrameTicket ticket;
    ASSERT_EQ(RENDER_FRAME_OK, BeginRenderFrameCapture(fixture.m_Queue, &ticket));
    ASSERT_EQ(RENDER_FRAME_INVALID_ARGUMENT, CaptureComponentRenderWorld(world, ticket.m_Frame, 0, 0));
    ASSERT_EQ(0u, GetRenderFrameStatistics(ticket.m_Frame).m_ReferenceCount);
    ASSERT_EQ(RENDER_FRAME_INVALID_ARGUMENT, PublishRenderFrame(fixture.m_Queue, ticket));
    ASSERT_EQ(RENDER_FRAME_OK, CancelRenderFrameCapture(fixture.m_Queue, ticket));
    DeleteComponentRenderWorld(world);
    ASSERT_EQ(RENDER_FRAME_PUMP_IDLE, PumpRenderFrameQueue(fixture.m_Queue, false));
    DeleteComponentFixture(&fixture);
}

extern "C" void dmExportedSymbols();

int main(int argc, char** argv)
{
    dmExportedSymbols();
    TestMainPlatformInit();
    jc_test_init(&argc, argv);
    return jc_test_run_all();
}
