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

#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <jc_test/jc_test.h>
#include <dmsdk/datastore/datastore.h>
#include "test_data_memory.h"

struct ScheduledRow
{
    float m_Health;
};

struct ScheduledEntity
{
    float     m_Health;
    DSVector3 m_Position;
};

struct ScheduledContext
{
    uint32_t m_Field;
    uint32_t m_Calls;
    float    m_Amount;
    float    m_Total;
};

extern "C" int TestDataSchedulerFromC(void);

static void    HealRows(HDSQuery query, uint32_t first, uint32_t num_rows, void* context)
{
    ScheduledContext* state = (ScheduledContext*)context;
    ++state->m_Calls;
    DSIterator it = DSQueryIterRange(query, first, num_rows);
    while (DSIterNext(&it))
    {
        DSRowIterator rows = DSIterRows(&it);
        while (DSRowIterNext(&rows))
            *DSRowIterGetFloatMut(&rows, state->m_Field) += state->m_Amount;
    }
}

static void ReadHealthRows(HDSQuery query, uint32_t first, uint32_t num_rows, void* context)
{
    ScheduledContext* state = (ScheduledContext*)context;
    ++state->m_Calls;
    DSIterator it = DSQueryIterRange(query, first, num_rows);
    while (DSIterNext(&it))
    {
        DSRowIterator rows = DSIterRows(&it);
        while (DSRowIterNext(&rows))
            state->m_Total += *DSRowIterGetFloat(&rows, state->m_Field);
    }
}

static void MoveRows(HDSQuery query, uint32_t first, uint32_t num_rows, void* context)
{
    ScheduledContext* state = (ScheduledContext*)context;
    ++state->m_Calls;
    DSIterator it = DSQueryIterRange(query, first, num_rows);
    while (DSIterNext(&it))
    {
        DSRowIterator rows = DSIterRows(&it);
        while (DSRowIterNext(&rows))
            DSRowIterGetVector3Mut(&rows, state->m_Field)->m_Values[0] += state->m_Amount;
    }
}

static HDSContext CreateScheduledStore(DSId* out_ids)
{
    HDSContext  store = DSCreateStore(0);
    DSFieldDesc fields[] = {
        { .m_Field = dmHashString64("health"), .m_Type = DS_TYPE_FLOAT, .m_Offset = offsetof(ScheduledEntity, m_Health) },
        { .m_Field = dmHashString64("position"), .m_Type = DS_TYPE_VECTOR3, .m_Offset = offsetof(ScheduledEntity, m_Position) }
    };
    DSTableDesc table = { .m_Type = 1, .m_Fields = fields, .m_FieldCount = 2, .m_RowStride = sizeof(ScheduledEntity) };
    EXPECT_EQ(DS_RESULT_OK, DSRegisterTable(store, &table));
    ScheduledEntity input[] = { { .m_Health = 50 }, { .m_Health = 50 }, { .m_Health = 50 } };
    EXPECT_EQ(DS_RESULT_OK, DSCreateRows(store, 1, 0, 3, input, out_ids));
    return store;
}

static HDSQuery CreateScheduledQuery(HDSContext store, const DSQueryDesc& desc)
{
    HDSQuery query = 0;
    EXPECT_EQ(DS_RESULT_OK, DSCreateQuery(store, &desc, &query));
    return query;
}

static HJobContext CreateScheduledJobs()
{
    // No workers: JobSystemUpdate(jobs, 0) runs at most one job per call,
    // independent of elapsed time. Tests can inspect each scheduling step.
    JobSystemCreateParams params = { .m_ThreadNamePrefix = "ds-schedule", .m_ThreadCount = 0 };
    return JobSystemCreate(&params);
}

static void FinishSchedule(HDSContext store, uint32_t num_functions)
{
    // Count deterministic job steps, not retries while waiting for workers.
    for (uint32_t i = 0; i < num_functions; ++i)
        ASSERT_EQ(i + 1 == num_functions ? DS_RESULT_OK : DS_RESULT_BUSY, DSSchedulerUpdate(store));
    ASSERT_EQ(DS_RESULT_OK, DSEndSchedule(store));
}

// Registration copies hashes while preserving the callback's
// borrowed query/context and the row range supplied by its dispatcher.
TEST(DataScheduler, CopiesHashesAndBorrowsQueryAndContext)
{
    HDSContext  store = DSCreateStore(0);
    DSFieldDesc field = { .m_Field = dmHashString64("health"), .m_Type = DS_TYPE_FLOAT, .m_Offset = offsetof(ScheduledRow, m_Health) };
    DSTableDesc table = { .m_Type = 1, .m_Fields = &field, .m_FieldCount = 1, .m_RowStride = sizeof(ScheduledRow) };
    ASSERT_EQ(DS_RESULT_OK, DSRegisterTable(store, &table));
    ScheduledRow input[] = { { .m_Health = 100 }, { .m_Health = 50 }, { .m_Health = 25 } };
    DSId         ids[3];
    ASSERT_EQ(DS_RESULT_OK, DSCreateRows(store, 1, 0, 3, input, ids));

    DSQueryDesc query_desc = { .m_Items = { DSField("health", DS_TYPE_FLOAT, DS_FIELD_MUTABLE) } };
    HDSQuery    query;
    ASSERT_EQ(DS_RESULT_OK, DSCreateQuery(store, &query_desc, &query));
    ScheduledContext context = { .m_Field = DSQueryFindField(query, &query_desc.m_Items[0]), .m_Amount = 5 };
    DSSystemDesc     desc = { .m_Name = dmHashString64("game.heal"), .m_DependsOn = { dmHashString64("engine.frame_begin") }, .m_Query = query, .m_Process = HealRows, .m_Context = &context };
    ASSERT_EQ(DS_RESULT_OK, DSRegisterFunction(store, &desc));
    ASSERT_EQ(0u, context.m_Calls);

    desc.m_Name = 0;
    desc.m_DependsOn[0] = 0;
    desc.m_Query = 0;
    desc.m_Process = 0;
    desc.m_Context = 0;
    const DSSystemDesc& registered = store->m_Systems[0].m_Desc;
    ASSERT_EQ(dmHashString64("game.heal"), registered.m_Name);
    ASSERT_EQ(dmHashString64("engine.frame_begin"), registered.m_DependsOn[0]);
    ASSERT_EQ(query, registered.m_Query);
    ASSERT_TRUE(HealRows == registered.m_Process);
    ASSERT_EQ((void*)&context, registered.m_Context);

    HJobContext jobs = CreateScheduledJobs();
    DSEventDesc begin = { .m_Name = dmHashString64("engine.frame_begin") };
    ASSERT_EQ(DS_RESULT_OK, DSRegisterEvent(store, &begin));
    context.m_Amount = 10;
    ASSERT_EQ(DS_RESULT_OK, DSBeginSchedule(store, jobs));
    ASSERT_EQ(DS_RESULT_OK, DSSchedulerEvent(store, begin.m_Name));
    FinishSchedule(store, 1);
    JobSystemDestroy(jobs);
    ASSERT_EQ(1u, context.m_Calls);
    float values[3];
    ASSERT_EQ(DS_RESULT_OK, DSFieldGetFloatBatch(store, 3, ids, field.m_Field, values));
    ASSERT_EQ(110.0f, values[0]);
    ASSERT_EQ(60.0f, values[1]);
    ASSERT_EQ(35.0f, values[2]);

    ASSERT_EQ(DS_RESULT_OK, DSUnregisterFunction(store, dmHashString64("game.heal")));
    ASSERT_EQ(DS_RESULT_OK, DSQueryTryBegin(query));
    DSQueryEnd(query);
    ASSERT_EQ(DS_RESULT_OK, DSDestroyQuery(query));
    ASSERT_EQ(DS_RESULT_OK, DSDestroyStore(store));
}

// Growth and swap removal must preserve other registrations, including a full
// dependency array without a terminator and names registered out of order.
TEST(DataScheduler, GrowthRemovalAndFullDependencyArray)
{
    HDSContext  store = DSCreateStore(0);
    DSQueryDesc query_desc = { 0 };
    HDSQuery    query;
    ASSERT_EQ(DS_RESULT_OK, DSCreateQuery(store, &query_desc, &query));
    char         name[32];
    DSSystemDesc desc = { .m_Query = query, .m_Process = HealRows };
    for (uint32_t i = 0; i < DS_SYSTEM_MAX_DEPENDENCIES; ++i)
    {
        snprintf(name, sizeof(name), "prerequisite_%u", i);
        desc.m_DependsOn[i] = dmHashString64(name);
    }
    for (uint32_t i = 0; i < 24; ++i)
    {
        snprintf(name, sizeof(name), "system_%u", i);
        desc.m_Name = dmHashString64(name);
        ASSERT_EQ(DS_RESULT_OK, DSRegisterFunction(store, &desc));
    }
    ASSERT_EQ(DS_RESULT_ALREADY_EXISTS, DSRegisterFunction(store, &desc));
    ASSERT_EQ(24u, store->m_Systems.Size());
    ASSERT_EQ(dmHashString64("prerequisite_15"), store->m_Systems[0].m_Desc.m_DependsOn[15]);
    for (uint32_t i = 1; i < 24; i += 2)
    {
        snprintf(name, sizeof(name), "system_%u", i);
        ASSERT_EQ(DS_RESULT_OK, DSUnregisterFunction(store, dmHashString64(name)));
        ASSERT_EQ(DS_RESULT_NOT_FOUND, DSUnregisterFunction(store, dmHashString64(name)));
    }
    for (uint32_t i = 0; i < 24; i += 2)
    {
        snprintf(name, sizeof(name), "system_%u", i);
        ASSERT_EQ(DS_RESULT_OK, DSUnregisterFunction(store, dmHashString64(name)));
    }
    ASSERT_EQ(0u, store->m_Systems.Size());
    ASSERT_EQ(DS_RESULT_OK, DSRegisterFunction(store, &desc));
    ASSERT_EQ(DS_RESULT_OK, DSDestroyQuery(query));
    ASSERT_EQ(DS_RESULT_OK, DSDestroyStore(store));
}

// Invalid declarations must not leave partial registrations or accept another
// store's query; a null user context remains valid.
TEST(DataScheduler, RejectsInvalidDeclarations)
{
    HDSContext  store = DSCreateStore(0);
    HDSContext  other_store = DSCreateStore(0);
    DSQueryDesc query_desc = { 0 };
    HDSQuery    query;
    HDSQuery    other_query;
    ASSERT_EQ(DS_RESULT_OK, DSCreateQuery(store, &query_desc, &query));
    ASSERT_EQ(DS_RESULT_OK, DSCreateQuery(other_store, &query_desc, &other_query));
    DSSystemDesc desc = { .m_Name = dmHashString64("game.heal"), .m_Query = other_query, .m_Process = HealRows };
    ASSERT_EQ(DS_RESULT_INVALID_ARGUMENT, DSRegisterFunction(store, &desc));
    desc.m_Query = query;
    desc.m_Name = 0;
    ASSERT_EQ(DS_RESULT_INVALID_ARGUMENT, DSRegisterFunction(store, &desc));
    desc.m_Name = dmHashString64("game.heal");
    desc.m_Process = 0;
    ASSERT_EQ(DS_RESULT_INVALID_ARGUMENT, DSRegisterFunction(store, &desc));
    desc.m_Process = HealRows;
    ASSERT_EQ(0u, store->m_Systems.Size());
    desc.m_DependsOn[0] = 0;
    ASSERT_EQ(DS_RESULT_OK, DSRegisterFunction(store, &desc));
    ASSERT_EQ(DS_RESULT_OK, DSDestroyQuery(other_query));
    ASSERT_EQ(DS_RESULT_OK, DSDestroyQuery(query));
    ASSERT_EQ(DS_RESULT_OK, DSDestroyStore(other_store));
    ASSERT_EQ(DS_RESULT_OK, DSDestroyStore(store));
}

// Hashes are scoped to the owning context. Rejected destruction must preserve
// registrations; completed teardown must not invoke callbacks or touch another store.
TEST(DataScheduler, ContextOwnsRegistrations)
{
    HDSContext       store = DSCreateStore(0);
    HDSContext       other_store = DSCreateStore(0);
    DSQueryDesc      query_desc = { 0 };
    HDSQuery         query;
    HDSQuery         other_query;
    ScheduledContext context = { 0 };
    ASSERT_EQ(DS_RESULT_OK, DSCreateQuery(store, &query_desc, &query));
    ASSERT_EQ(DS_RESULT_OK, DSCreateQuery(other_store, &query_desc, &other_query));
    DSSystemDesc desc = { .m_Name = dmHashString64("game.heal"), .m_Query = query, .m_Process = HealRows, .m_Context = &context };
    ASSERT_EQ(DS_RESULT_OK, DSRegisterFunction(store, &desc));
    desc.m_Query = other_query;
    ASSERT_EQ(DS_RESULT_OK, DSRegisterFunction(other_store, &desc));

    ASSERT_EQ(DS_RESULT_OK, DSQueryTryBegin(query));
    ASSERT_EQ(DS_RESULT_LOCKED, DSDestroyStore(store));
    ASSERT_EQ(1u, store->m_Systems.Size());
    ASSERT_EQ(dmHashString64("game.heal"), store->m_Systems[0].m_Desc.m_Name);
    DSQueryEnd(query);
    ASSERT_EQ(DS_RESULT_OK, DSUnregisterFunction(store, desc.m_Name));
    ASSERT_EQ(0u, store->m_Systems.Size());
    ASSERT_EQ(1u, other_store->m_Systems.Size());
    ASSERT_EQ(dmHashString64("game.heal"), other_store->m_Systems[0].m_Desc.m_Name);

    // Teardown leaves the copied registration for the context to release.
    ASSERT_EQ(DS_RESULT_OK, DSDestroyQuery(other_query));
    ASSERT_EQ(DS_RESULT_OK, DSDestroyStore(other_store));
    ASSERT_EQ(0u, context.m_Calls);
    ASSERT_EQ(DS_RESULT_OK, DSQueryTryBegin(query));
    DSQueryEnd(query);
    ASSERT_EQ(DS_RESULT_OK, DSDestroyQuery(query));
    ASSERT_EQ(DS_RESULT_OK, DSDestroyStore(store));
}

// An event submits every compatible function automatically; conflicting queries
// remain queued until completion releases access. Each invocation starts fresh.
TEST(DataScheduler, QueryAccessMatrix)
{
    HJobContext jobs = CreateScheduledJobs();
    for (uint32_t a = 0; a < 2; ++a)
    {
        for (uint32_t b = 0; b < 2; ++b)
        {
            DSId             ids[3];
            HDSContext       store = CreateScheduledStore(ids);
            DSQueryDesc      desc = { .m_Items = { DSField("health", DS_TYPE_FLOAT, a ? DS_FIELD_MUTABLE : 0) } };
            HDSQuery         first = CreateScheduledQuery(store, desc);
            ScheduledContext first_context = { .m_Field = DSQueryFindField(first, &desc.m_Items[0]), .m_Amount = 1 };
            desc.m_Items[0].m_Access = b ? DS_FIELD_MUTABLE : 0;
            HDSQuery         second = CreateScheduledQuery(store, desc);
            ScheduledContext second_context = { .m_Field = DSQueryFindField(second, &desc.m_Items[0]), .m_Amount = 1 };
            DSSystemDesc     first_desc = { .m_Name = dmHashString64("first"), .m_DependsOn = { dmHashString64("engine.update.begin") }, .m_Query = first, .m_Process = a ? HealRows : ReadHealthRows, .m_Context = &first_context };
            DSSystemDesc     second_desc = { .m_Name = dmHashString64("second"), .m_DependsOn = { dmHashString64("engine.update.begin") }, .m_Query = second, .m_Process = b ? HealRows : ReadHealthRows, .m_Context = &second_context };
            DSEventDesc      begin = { .m_Name = dmHashString64("engine.update.begin") };
            ASSERT_EQ(DS_RESULT_OK, DSRegisterFunction(store, &first_desc));
            ASSERT_EQ(DS_RESULT_OK, DSRegisterFunction(store, &second_desc));
            ASSERT_EQ(DS_RESULT_OK, DSRegisterEvent(store, &begin));
            for (uint32_t frame = 0; frame < 2; ++frame)
            {
                first_context.m_Total = second_context.m_Total = 0;
                ASSERT_EQ(DS_RESULT_OK, DSBeginSchedule(store, jobs));
                ASSERT_EQ(DS_RESULT_BUSY, DSSchedulerUpdate(store));
                ASSERT_EQ(0u, store->m_ActiveQueries.Size());
                ASSERT_EQ(DS_RESULT_OK, DSSchedulerEvent(store, begin.m_Name));
                ASSERT_EQ(a || b ? 1u : 2u, store->m_ActiveQueries.Size());
                ASSERT_EQ(frame, first_context.m_Calls);
                ASSERT_EQ(frame, second_context.m_Calls);
                ASSERT_EQ(DS_RESULT_BUSY, DSEndSchedule(store));
                FinishSchedule(store, 2);
                ASSERT_EQ(frame + 1, first_context.m_Calls);
                ASSERT_EQ(frame + 1, second_context.m_Calls);
                if (!a)
                    ASSERT_EQ(3.0f * (50 + frame * (a + b)), first_context.m_Total);
                if (!b)
                    ASSERT_EQ(3.0f * (50 + frame * (a + b) + a), second_context.m_Total);
                float health[3];
                ASSERT_EQ(DS_RESULT_OK, DSFieldGetFloatBatch(store, 3, ids, dmHashString64("health"), health));
                for (uint32_t i = 0; i < 3; ++i)
                    ASSERT_EQ(50.0f + (frame + 1) * (a + b), health[i]);
            }
            ASSERT_EQ(DS_RESULT_OK, DSDestroyQuery(first));
            ASSERT_EQ(DS_RESULT_OK, DSDestroyQuery(second));
            ASSERT_EQ(DS_RESULT_OK, DSDestroyStore(store));
        }
    }
    JobSystemDestroy(jobs);
}

// Explicit dependencies apply to read-only jobs too. Completing the producer
// automatically submits the consumer, even when it was registered first.
TEST(DataScheduler, DependenciesSubmitAfterCompletion)
{
    HJobContext jobs = CreateScheduledJobs();
    for (uint32_t writable = 0; writable < 2; ++writable)
    {
        DSId             ids[3];
        HDSContext       store = CreateScheduledStore(ids);
        DSQueryDesc      desc = { .m_Items = { DSField("health", DS_TYPE_FLOAT, writable ? DS_FIELD_MUTABLE : 0) } };
        HDSQuery         producer = CreateScheduledQuery(store, desc);
        ScheduledContext producer_context = { .m_Field = DSQueryFindField(producer, &desc.m_Items[0]), .m_Amount = 1 };
        desc.m_Items[0].m_Access = 0;
        HDSQuery         consumer = CreateScheduledQuery(store, desc);
        ScheduledContext consumer_context = { .m_Field = DSQueryFindField(consumer, &desc.m_Items[0]) };
        DSSystemDesc     consume = { .m_Name = dmHashString64("consume"), .m_DependsOn = { dmHashString64("produce") }, .m_Query = consumer, .m_Process = ReadHealthRows, .m_Context = &consumer_context };
        DSSystemDesc     produce = { .m_Name = dmHashString64("produce"), .m_Query = producer, .m_Process = writable ? HealRows : ReadHealthRows, .m_Context = &producer_context };
        ASSERT_EQ(DS_RESULT_OK, DSRegisterFunction(store, &consume));
        ASSERT_EQ(DS_RESULT_OK, DSRegisterFunction(store, &produce));
        for (uint32_t frame = 0; frame < 2; ++frame)
        {
            consumer_context.m_Total = 0;
            ASSERT_EQ(DS_RESULT_OK, DSBeginSchedule(store, jobs));
            ASSERT_EQ(1u, store->m_ActiveQueries.Size());
            // The normal engine job pump is sufficient: completion schedules dependents.
            JobSystemUpdate(jobs, 0);
            ASSERT_EQ(frame + 1, producer_context.m_Calls);
            ASSERT_EQ(frame, consumer_context.m_Calls);
            ASSERT_EQ(DS_RESULT_BUSY, DSQueryTryBegin(consumer));
            FinishSchedule(store, 1);
            ASSERT_EQ(frame + 1, consumer_context.m_Calls);
            ASSERT_EQ(3.0f * (50 + (frame + 1) * writable), consumer_context.m_Total);
        }
        ASSERT_EQ(DS_RESULT_OK, DSDestroyQuery(producer));
        ASSERT_EQ(DS_RESULT_OK, DSDestroyQuery(consumer));
        ASSERT_EQ(DS_RESULT_OK, DSDestroyStore(store));
    }
    JobSystemDestroy(jobs);
}

// A blocked reader must not stop independent writes. Group filters and separate
// fields permit three jobs to reserve access before any work has executed.
TEST(DataScheduler, DisjointFieldsAndGroups)
{
    DSId            ids[3];
    HDSContext      store = CreateScheduledStore(ids);
    DSGroupId       other_group = CreateTestGroup(store);
    ScheduledEntity input = { .m_Health = 75 };
    DSId            other_id;
    ASSERT_EQ(DS_RESULT_OK, DSCreateRows(store, 1, other_group, 1, &input, &other_id));
    DSGroupId        groups[] = { 0, other_group };
    DSQueryDesc      desc = { .m_GroupIds = groups, .m_GroupIdCount = 1, .m_Items = { DSField("health", DS_TYPE_FLOAT, DS_FIELD_MUTABLE) } };
    HDSQuery         first = CreateScheduledQuery(store, desc);
    ScheduledContext first_context = { .m_Field = DSQueryFindField(first, &desc.m_Items[0]), .m_Amount = 1 };
    desc.m_GroupIds = groups + 1;
    HDSQuery         second = CreateScheduledQuery(store, desc);
    ScheduledContext second_context = { .m_Field = DSQueryFindField(second, &desc.m_Items[0]), .m_Amount = 2 };
    desc = { .m_Items = { DSField("health", DS_TYPE_FLOAT, 0) } };
    HDSQuery         reader = CreateScheduledQuery(store, desc);
    ScheduledContext read_context = { .m_Field = DSQueryFindField(reader, &desc.m_Items[0]) };
    desc = { .m_Items = { DSField("position", DS_TYPE_VECTOR3, DS_FIELD_MUTABLE) } };
    HDSQuery         movement = CreateScheduledQuery(store, desc);
    ScheduledContext move_context = { .m_Field = DSQueryFindField(movement, &desc.m_Items[0]), .m_Amount = 10 };
    DSSystemDesc     systems[] = {
        { .m_Name = dmHashString64("health_7"), .m_Query = first, .m_Process = HealRows, .m_Context = &first_context },
        { .m_Name = dmHashString64("read_health"), .m_Query = reader, .m_Process = ReadHealthRows, .m_Context = &read_context },
        { .m_Name = dmHashString64("movement"), .m_Query = movement, .m_Process = MoveRows, .m_Context = &move_context },
        { .m_Name = dmHashString64("health_8"), .m_Query = second, .m_Process = HealRows, .m_Context = &second_context }
    };
    for (uint32_t i = 0; i < 4; ++i)
        ASSERT_EQ(DS_RESULT_OK, DSRegisterFunction(store, &systems[i]));
    HJobContext jobs = CreateScheduledJobs();
    ASSERT_EQ(DS_RESULT_OK, DSBeginSchedule(store, jobs));
    ASSERT_EQ(3u, store->m_ActiveQueries.Size());
    FinishSchedule(store, 4);
    ASSERT_EQ(230.0f, read_context.m_Total);
    ASSERT_EQ(1u, first_context.m_Calls);
    ASSERT_EQ(1u, second_context.m_Calls);
    ASSERT_EQ(1u, move_context.m_Calls);
    ASSERT_EQ(1u, read_context.m_Calls);
    DSVector3 position;
    ASSERT_EQ(DS_RESULT_OK, DSFieldGetVector3(store, other_id, dmHashString64("position"), &position));
    ASSERT_EQ(10.0f, position.m_Values[0]);
    for (uint32_t i = 0; i < 4; ++i)
        ASSERT_EQ(DS_RESULT_OK, DSDestroyQuery(systems[i].m_Query));
    ASSERT_EQ(DS_RESULT_OK, DSDestroyStore(store));
    JobSystemDestroy(jobs);
}

// Missing names and mixed event/function cycles fail before jobs are submitted.
// Removing and replacing registrations must rebuild dependency indices.
TEST(DataScheduler, GraphValidationAndRegistrationChanges)
{
    HJobContext      jobs = CreateScheduledJobs();
    HDSContext       store = DSCreateStore(0);
    DSQueryDesc      desc = { 0 };
    HDSQuery         query = CreateScheduledQuery(store, desc);
    ScheduledContext context = { 0 };
    DSSystemDesc     function = { .m_Name = dmHashString64("heal"), .m_DependsOn = { dmHashString64("engine.begin") }, .m_Query = query, .m_Process = HealRows, .m_Context = &context };
    DSEventDesc      begin = { .m_Name = dmHashString64("engine.begin"), .m_DependsOn = { dmHashString64("heal") } };
    DSEventDesc      end = { .m_Name = dmHashString64("engine.end"), .m_DependsOn = { dmHashString64("heal") } };
    ASSERT_EQ(DS_RESULT_OK, DSRegisterFunction(store, &function));
    ASSERT_EQ(DS_RESULT_NOT_FOUND, DSBeginSchedule(store, jobs));
    ASSERT_EQ(DS_RESULT_OK, DSRegisterEvent(store, &begin));
    ASSERT_EQ(DS_RESULT_OK, DSRegisterEvent(store, &end));
    ASSERT_EQ(DS_RESULT_INVALID_ARGUMENT, DSBeginSchedule(store, jobs));
    ASSERT_EQ(0u, context.m_Calls);
    ASSERT_EQ(DS_RESULT_OK, DSUnregisterEvent(store, begin.m_Name));
    begin.m_DependsOn[0] = begin.m_Name;
    ASSERT_EQ(DS_RESULT_OK, DSRegisterEvent(store, &begin));
    ASSERT_EQ(DS_RESULT_INVALID_ARGUMENT, DSBeginSchedule(store, jobs));
    ASSERT_EQ(DS_RESULT_OK, DSUnregisterEvent(store, begin.m_Name));
    begin.m_DependsOn[0] = 0;
    ASSERT_EQ(DS_RESULT_OK, DSRegisterEvent(store, &begin));
    ASSERT_EQ(DS_RESULT_OK, DSBeginSchedule(store, jobs));
    ASSERT_EQ(DS_RESULT_OK, DSSchedulerEvent(store, begin.m_Name));
    ASSERT_EQ(DS_RESULT_BUSY, DSSchedulerEvent(store, end.m_Name));
    ASSERT_EQ(DS_RESULT_OK, DSSchedulerUpdate(store));
    ASSERT_EQ(1u, context.m_Calls);
    ASSERT_EQ(DS_RESULT_OK, DSSchedulerEvent(store, end.m_Name));
    ASSERT_EQ(DS_RESULT_OK, DSEndSchedule(store));
    ASSERT_EQ(DS_RESULT_OK, DSUnregisterFunction(store, function.m_Name));
    ASSERT_EQ(DS_RESULT_NOT_FOUND, DSBeginSchedule(store, jobs));
    ASSERT_EQ(DS_RESULT_OK, DSUnregisterEvent(store, end.m_Name));
    ASSERT_EQ(DS_RESULT_OK, DSBeginSchedule(store, jobs));
    ASSERT_EQ(DS_RESULT_OK, DSSchedulerUpdate(store));
    ASSERT_EQ(DS_RESULT_OK, DSEndSchedule(store));
    ASSERT_EQ(DS_RESULT_OK, DSDestroyQuery(query));
    ASSERT_EQ(DS_RESULT_OK, DSDestroyStore(store));
    JobSystemDestroy(jobs);
}

// All sixteen prerequisites must finish before the consumer job runs. Reusing one
// query across functions must serialize its reservations without caller dispatch.
TEST(DataScheduler, FullDependencyArray)
{
    HJobContext      jobs = CreateScheduledJobs();
    HDSContext       store = DSCreateStore(0);
    DSQueryDesc      desc = { 0 };
    HDSQuery         query = CreateScheduledQuery(store, desc);
    ScheduledContext producer_context = { 0 }, consumer_context = { 0 };
    dmhash_t         names[DS_SYSTEM_MAX_DEPENDENCIES];
    char             name[32];
    DSSystemDesc     consumer = { .m_Name = dmHashString64("consumer"), .m_Query = query, .m_Process = ReadHealthRows, .m_Context = &consumer_context };
    for (uint32_t i = 0; i < DS_SYSTEM_MAX_DEPENDENCIES; ++i)
    {
        snprintf(name, sizeof(name), "producer_%u", i);
        names[i] = dmHashString64(name);
        consumer.m_DependsOn[i] = names[i];
    }
    ASSERT_EQ(DS_RESULT_OK, DSRegisterFunction(store, &consumer));
    for (uint32_t i = 0; i < DS_SYSTEM_MAX_DEPENDENCIES; ++i)
    {
        DSSystemDesc producer = { .m_Name = names[i], .m_Query = query, .m_Process = ReadHealthRows, .m_Context = &producer_context };
        ASSERT_EQ(DS_RESULT_OK, DSRegisterFunction(store, &producer));
    }
    ASSERT_EQ(DS_RESULT_OK, DSBeginSchedule(store, jobs));
    for (uint32_t i = 0; i < DS_SYSTEM_MAX_DEPENDENCIES; ++i)
    {
        ASSERT_EQ(1u, store->m_ActiveQueries.Size());
        ASSERT_EQ(DS_RESULT_BUSY, DSSchedulerUpdate(store));
        ASSERT_EQ(i + 1, producer_context.m_Calls);
        ASSERT_EQ(0u, consumer_context.m_Calls);
    }
    FinishSchedule(store, 1);
    ASSERT_EQ(1u, consumer_context.m_Calls);
    ASSERT_EQ(DS_RESULT_OK, DSDestroyQuery(query));
    ASSERT_EQ(DS_RESULT_OK, DSDestroyStore(store));
    JobSystemDestroy(jobs);
}

// Empty queries still run one job. Teardown/registration cannot invalidate its
// borrowed inputs; ending before an event discards only work not yet submitted.
TEST(DataScheduler, ScheduleLifetimeAndEmptyQueries)
{
    HJobContext jobs = CreateScheduledJobs();
    HDSContext  store = DSCreateStore(0);
    ASSERT_EQ(DS_RESULT_INVALID_ARGUMENT, DSBeginSchedule(store, 0));
    ASSERT_EQ(DS_RESULT_INVALID_ARGUMENT, DSSchedulerUpdate(store));
    ASSERT_EQ(DS_RESULT_INVALID_ARGUMENT, DSEndSchedule(store));
    ASSERT_EQ(DS_RESULT_OK, DSBeginSchedule(store, jobs));
    ASSERT_EQ(DS_RESULT_OK, DSSchedulerUpdate(store));
    ASSERT_EQ(DS_RESULT_LOCKED, DSDestroyStore(store));
    ASSERT_EQ(DS_RESULT_OK, DSEndSchedule(store));
    DSQueryDesc      desc = { 0 };
    HDSQuery         query = CreateScheduledQuery(store, desc);
    HDSQuery         unrelated = CreateScheduledQuery(store, desc);
    ScheduledContext context = { 0 };
    DSEventDesc      begin = { .m_Name = dmHashString64("engine.begin") };
    DSSystemDesc     system = { .m_Name = dmHashString64("empty"), .m_DependsOn = { dmHashString64("engine.begin") }, .m_Query = query, .m_Process = ReadHealthRows, .m_Context = &context };
    ASSERT_EQ(DS_RESULT_OK, DSRegisterEvent(store, &begin));
    ASSERT_EQ(DS_RESULT_OK, DSRegisterFunction(store, &system));
    ASSERT_EQ(DS_RESULT_OK, DSBeginSchedule(store, jobs));
    ASSERT_EQ(DS_RESULT_LOCKED, DSBeginSchedule(store, jobs));
    ASSERT_EQ(DS_RESULT_LOCKED, DSRegisterFunction(store, &system));
    ASSERT_EQ(DS_RESULT_LOCKED, DSUnregisterFunction(store, dmHashString64("empty")));
    ASSERT_EQ(DS_RESULT_LOCKED, DSDestroyQuery(query));
    ASSERT_EQ(DS_RESULT_OK, DSDestroyQuery(unrelated));
    ASSERT_EQ(DS_RESULT_OK, DSEndSchedule(store));
    ASSERT_EQ(0u, context.m_Calls);
    ASSERT_EQ(DS_RESULT_OK, DSBeginSchedule(store, jobs));
    ASSERT_EQ(DS_RESULT_OK, DSSchedulerEvent(store, begin.m_Name));
    ASSERT_EQ(0u, DSQueryGetRowCount(query));
    ASSERT_EQ(DS_RESULT_BUSY, DSQueryTryBegin(query));
    ASSERT_EQ(DS_RESULT_BUSY, DSEndSchedule(store));
    ASSERT_EQ(DS_RESULT_LOCKED, DSDestroyStore(store));
    ASSERT_EQ(DS_RESULT_OK, DSSchedulerUpdate(store));
    ASSERT_EQ(1u, context.m_Calls);
    ASSERT_EQ(DS_RESULT_OK, DSSchedulerUpdate(store));
    ASSERT_EQ(1u, context.m_Calls);
    ASSERT_EQ(DS_RESULT_OK, DSEndSchedule(store));
    ASSERT_EQ(DS_RESULT_OK, DSUnregisterFunction(store, dmHashString64("empty")));
    ASSERT_EQ(DS_RESULT_OK, DSDestroyQuery(query));
    ASSERT_EQ(DS_RESULT_OK, DSDestroyStore(store));
    JobSystemDestroy(jobs);
}

// An external reservation delays automatic dispatch. Update retries after release,
// and new table matches must affect conflict checks in the next invocation.
TEST(DataScheduler, TableChangesAndExternalReservations)
{
    HJobContext jobs = CreateScheduledJobs();
    HDSContext  store = DSCreateStore(0);
    uint64_t    tags[] = { dmHashString64("first"), dmHashString64("second") };
    DSFieldDesc field = { .m_Field = dmHashString64("health"), .m_Type = DS_TYPE_FLOAT };
    DSTableDesc table = { .m_Type = 1, .m_Tags = tags, .m_TagCount = 1, .m_Fields = &field, .m_FieldCount = 1, .m_RowStride = sizeof(ScheduledRow) };
    ASSERT_EQ(DS_RESULT_OK, DSRegisterTable(store, &table));
    ScheduledRow input = { .m_Health = 50 };
    ASSERT_EQ(DS_RESULT_OK, DSCreateRows(store, 1, 0, 1, &input, 0));
    DSQueryDesc      desc = { .m_Items = { DSTag("first"), DSField("health", DS_TYPE_FLOAT, DS_FIELD_MUTABLE) } };
    HDSQuery         writer = CreateScheduledQuery(store, desc);
    ScheduledContext write_context = { .m_Field = DSQueryFindField(writer, &desc.m_Items[1]), .m_Amount = 1 };
    desc = { .m_Items = { DSTag("second"), DSField("health", DS_TYPE_FLOAT, 0) } };
    HDSQuery         reader = CreateScheduledQuery(store, desc);
    ScheduledContext read_context = { .m_Field = DSQueryFindField(reader, &desc.m_Items[1]) };
    DSSystemDesc     write = { .m_Name = dmHashString64("write"), .m_Query = writer, .m_Process = HealRows, .m_Context = &write_context };
    DSSystemDesc     read = { .m_Name = dmHashString64("read"), .m_Query = reader, .m_Process = ReadHealthRows, .m_Context = &read_context };
    ASSERT_EQ(DS_RESULT_OK, DSRegisterFunction(store, &write));
    ASSERT_EQ(DS_RESULT_OK, DSRegisterFunction(store, &read));
    for (uint32_t pass = 0; pass < 3; ++pass)
    {
        if (pass == 1)
        {
            table.m_Type = 2;
            table.m_TagCount = 2;
            ASSERT_EQ(DS_RESULT_OK, DSRegisterTable(store, &table));
            ASSERT_EQ(DS_RESULT_OK, DSCreateRows(store, 2, 0, 1, &input, 0));
        }
        if (pass == 2)
            ASSERT_EQ(DS_RESULT_OK, DSUnregisterTable(store, 2));
        ASSERT_EQ(DS_RESULT_OK, DSBeginSchedule(store, jobs));
        ASSERT_EQ(pass == 1 ? 1u : 2u, store->m_ActiveQueries.Size());
        FinishSchedule(store, 2);
    }
    desc = { .m_Items = { DSField("health", DS_TYPE_FLOAT, 0) } };
    HDSQuery external = CreateScheduledQuery(store, desc);
    ASSERT_EQ(DS_RESULT_OK, DSQueryTryBegin(external));
    ASSERT_EQ(DS_RESULT_OK, DSBeginSchedule(store, jobs));
    ASSERT_EQ(DS_RESULT_BUSY, DSSchedulerUpdate(store));
    ASSERT_EQ(3u, write_context.m_Calls);
    ASSERT_EQ(4u, read_context.m_Calls);
    DSQueryEnd(external);
    ASSERT_EQ(DS_RESULT_BUSY, DSSchedulerUpdate(store));
    ASSERT_EQ(DS_RESULT_BUSY, DSQueryTryBegin(writer));
    FinishSchedule(store, 1);
    ASSERT_EQ(4u, write_context.m_Calls);
    ASSERT_EQ(DS_RESULT_OK, DSDestroyQuery(external));
    ASSERT_EQ(DS_RESULT_OK, DSDestroyQuery(reader));
    ASSERT_EQ(DS_RESULT_OK, DSDestroyQuery(writer));
    ASSERT_EQ(DS_RESULT_OK, DSDestroyStore(store));
    JobSystemDestroy(jobs);
}

// Ending must retry ready work blocked on query access, while ignoring a chain
// behind an unsignaled event. The engine uses this end/update loop each frame.
TEST(DataScheduler, EndWaitsForReadyFunctions)
{
    HJobContext      jobs = CreateScheduledJobs();
    DSId             ids[3];
    HDSContext       store = CreateScheduledStore(ids);
    DSQueryDesc      desc = { .m_Items = { DSField("health", DS_TYPE_FLOAT, DS_FIELD_MUTABLE) } };
    HDSQuery         query = CreateScheduledQuery(store, desc);
    ScheduledContext writer = { .m_Field = DSQueryFindField(query, &desc.m_Items[0]), .m_Amount = 1 };
    ScheduledContext reader = { .m_Field = writer.m_Field };
    ScheduledContext pending = { .m_Field = writer.m_Field };
    DSEventDesc      events[] = { { .m_Name = dmHashString64("begin") }, { .m_Name = dmHashString64("manual") } };
    DSSystemDesc     systems[] = {
        { .m_Name = dmHashString64("write"), .m_DependsOn = { events[0].m_Name }, .m_Query = query, .m_Process = HealRows, .m_Context = &writer },
        { .m_Name = dmHashString64("read"), .m_DependsOn = { dmHashString64("write") }, .m_Query = query, .m_Process = ReadHealthRows, .m_Context = &reader },
        { .m_Name = dmHashString64("pending"), .m_DependsOn = { events[1].m_Name }, .m_Query = query, .m_Process = ReadHealthRows, .m_Context = &pending },
        { .m_Name = dmHashString64("pending_child"), .m_DependsOn = { dmHashString64("pending") }, .m_Query = query, .m_Process = ReadHealthRows, .m_Context = &pending }
    };
    for (uint32_t i = 0; i < DM_ARRAY_SIZE(events); ++i)
        ASSERT_EQ(DS_RESULT_OK, DSRegisterEvent(store, &events[i]));
    for (uint32_t i = 0; i < DM_ARRAY_SIZE(systems); ++i)
        ASSERT_EQ(DS_RESULT_OK, DSRegisterFunction(store, &systems[i]));

    for (uint32_t frame = 0; frame < 2; ++frame)
    {
        ASSERT_EQ(DS_RESULT_OK, DSQueryTryBegin(query));
        ASSERT_EQ(DS_RESULT_OK, DSBeginSchedule(store, jobs));
        ASSERT_EQ(DS_RESULT_OK, DSSchedulerEvent(store, events[0].m_Name));
        ASSERT_EQ(DS_RESULT_BUSY, DSEndSchedule(store));
        ASSERT_EQ(frame, writer.m_Calls);
        DSQueryEnd(query);
        ASSERT_EQ(DS_RESULT_BUSY, DSEndSchedule(store));
        while (DSEndSchedule(store) == DS_RESULT_BUSY)
            DSSchedulerUpdate(store);
        ASSERT_EQ(frame + 1, writer.m_Calls);
        ASSERT_EQ(frame + 1, reader.m_Calls);
        ASSERT_EQ(0u, pending.m_Calls);
    }

    ASSERT_EQ(DS_RESULT_OK, DSDestroyQuery(query));
    ASSERT_EQ(DS_RESULT_OK, DSDestroyStore(store));
    JobSystemDestroy(jobs);
}

// Signaling an entry queues its jobs without executing them on the caller. A later
// engine boundary waits for those jobs; both states reset on the next invocation.
TEST(DataScheduler, EventsLaunchFunctions)
{
    HJobContext      jobs = CreateScheduledJobs();
    DSId             ids[3];
    HDSContext       store = CreateScheduledStore(ids);
    DSQueryDesc      desc = { .m_Items = { DSField("health", DS_TYPE_FLOAT, DS_FIELD_MUTABLE) } };
    HDSQuery         writer = CreateScheduledQuery(store, desc);
    ScheduledContext write_context = { .m_Field = DSQueryFindField(writer, &desc.m_Items[0]), .m_Amount = 1 };
    desc.m_Items[0].m_Access = 0;
    HDSQuery         reader = CreateScheduledQuery(store, desc);
    ScheduledContext read_context = { .m_Field = DSQueryFindField(reader, &desc.m_Items[0]) };
    DSSystemDesc     write = { .m_Name = dmHashString64("heal"), .m_DependsOn = { dmHashString64("engine.begin") }, .m_Query = writer, .m_Process = HealRows, .m_Context = &write_context };
    DSSystemDesc     read = { .m_Name = dmHashString64("observe"), .m_DependsOn = { dmHashString64("engine.end") }, .m_Query = reader, .m_Process = ReadHealthRows, .m_Context = &read_context };
    DSEventDesc      begin = { .m_Name = dmHashString64("engine.begin") };
    DSEventDesc      end = { .m_Name = dmHashString64("engine.end"), .m_DependsOn = { dmHashString64("heal") } };
    ASSERT_EQ(DS_RESULT_OK, DSRegisterFunction(store, &read));
    ASSERT_EQ(DS_RESULT_OK, DSRegisterEvent(store, &end));
    ASSERT_EQ(DS_RESULT_OK, DSRegisterFunction(store, &write));
    ASSERT_EQ(DS_RESULT_OK, DSRegisterEvent(store, &begin));
    for (uint32_t frame = 0; frame < 3; ++frame)
    {
        read_context.m_Total = 0;
        ASSERT_EQ(DS_RESULT_OK, DSBeginSchedule(store, jobs));
        ASSERT_EQ(DS_RESULT_BUSY, DSSchedulerUpdate(store));
        ASSERT_EQ(DS_RESULT_BUSY, DSSchedulerEvent(store, end.m_Name));
        ASSERT_EQ(DS_RESULT_OK, DSSchedulerEvent(store, begin.m_Name));
        ASSERT_EQ(frame, write_context.m_Calls);
        ASSERT_EQ(DS_RESULT_BUSY, DSSchedulerUpdate(store));
        ASSERT_EQ(frame + 1, write_context.m_Calls);
        ASSERT_EQ(frame, read_context.m_Calls);
        ASSERT_EQ(DS_RESULT_OK, DSSchedulerEvent(store, end.m_Name));
        ASSERT_EQ(DS_RESULT_BUSY, DSEndSchedule(store));
        FinishSchedule(store, 1);
        ASSERT_EQ(3.0f * (51 + frame), read_context.m_Total);
    }
    ASSERT_EQ(DS_RESULT_OK, DSDestroyQuery(reader));
    ASSERT_EQ(DS_RESULT_OK, DSDestroyQuery(writer));
    ASSERT_EQ(DS_RESULT_OK, DSDestroyStore(store));
    JobSystemDestroy(jobs);
}

// Events copy their hashes and share the function namespace. Wrong-kind operations,
// duplicates and mid-invocation registration changes must leave the graph intact.
TEST(DataScheduler, EventRegistrationAndLifetime)
{
    HJobContext jobs = CreateScheduledJobs();
    HDSContext  store = DSCreateStore(0);
    DSQueryDesc query_desc = { 0 };
    HDSQuery    query = CreateScheduledQuery(store, query_desc);
    DSEventDesc invalid = { 0 };
    ASSERT_EQ(DS_RESULT_INVALID_ARGUMENT, DSRegisterEvent(store, &invalid));

    DSEventDesc end = { .m_Name = dmHashString64("engine.update.end"), .m_DependsOn = { dmHashString64("engine.update.begin") } };
    DSEventDesc begin = { .m_Name = dmHashString64("engine.update.begin") };
    ASSERT_EQ(DS_RESULT_OK, DSRegisterEvent(store, &end));
    ASSERT_EQ(DS_RESULT_ALREADY_EXISTS, DSRegisterEvent(store, &end));
    ASSERT_EQ(DS_RESULT_OK, DSRegisterEvent(store, &begin));
    end.m_Name = 0;
    end.m_DependsOn[0] = 0;
    DSSystemDesc function = { .m_Name = dmHashString64("engine.update.end"), .m_DependsOn = { dmHashString64("never") }, .m_Query = query, .m_Process = HealRows };
    ASSERT_EQ(DS_RESULT_ALREADY_EXISTS, DSRegisterFunction(store, &function));
    ASSERT_EQ(DS_RESULT_INVALID_ARGUMENT, DSUnregisterFunction(store, dmHashString64("engine.update.end")));
    DSEventDesc never = { .m_Name = dmHashString64("never") };
    ASSERT_EQ(DS_RESULT_OK, DSRegisterEvent(store, &never));
    function.m_Name = dmHashString64("heal");
    ASSERT_EQ(DS_RESULT_OK, DSRegisterFunction(store, &function));
    DSEventDesc duplicate = { .m_Name = dmHashString64("heal") };
    ASSERT_EQ(DS_RESULT_ALREADY_EXISTS, DSRegisterEvent(store, &duplicate));
    ASSERT_EQ(DS_RESULT_INVALID_ARGUMENT, DSUnregisterEvent(store, dmHashString64("heal")));
    ASSERT_EQ(DS_RESULT_INVALID_ARGUMENT, DSSchedulerEvent(store, begin.m_Name));
    ASSERT_EQ(DS_RESULT_OK, DSBeginSchedule(store, jobs));
    ASSERT_EQ(DS_RESULT_LOCKED, DSRegisterEvent(store, &begin));
    ASSERT_EQ(DS_RESULT_LOCKED, DSUnregisterEvent(store, begin.m_Name));
    ASSERT_EQ(DS_RESULT_NOT_FOUND, DSSchedulerEvent(store, dmHashString64("missing")));
    ASSERT_EQ(DS_RESULT_INVALID_ARGUMENT, DSSchedulerEvent(store, dmHashString64("heal")));
    ASSERT_EQ(DS_RESULT_BUSY, DSSchedulerEvent(store, dmHashString64("engine.update.end")));
    ASSERT_EQ(DS_RESULT_OK, DSSchedulerEvent(store, begin.m_Name));
    ASSERT_EQ(DS_RESULT_INVALID_ARGUMENT, DSSchedulerEvent(store, begin.m_Name));
    ASSERT_EQ(DS_RESULT_OK, DSSchedulerEvent(store, dmHashString64("engine.update.end")));
    ASSERT_EQ(DS_RESULT_OK, DSEndSchedule(store));

    ASSERT_EQ(DS_RESULT_OK, DSBeginSchedule(store, jobs));
    ASSERT_EQ(DS_RESULT_BUSY, DSSchedulerEvent(store, dmHashString64("engine.update.end")));
    ASSERT_EQ(DS_RESULT_OK, DSEndSchedule(store));
    ASSERT_EQ(DS_RESULT_OK, DSUnregisterEvent(store, begin.m_Name));
    ASSERT_EQ(DS_RESULT_NOT_FOUND, DSUnregisterEvent(store, begin.m_Name));
    ASSERT_EQ(DS_RESULT_NOT_FOUND, DSBeginSchedule(store, jobs));
    ASSERT_EQ(DS_RESULT_OK, DSRegisterEvent(store, &begin));
    ASSERT_EQ(DS_RESULT_OK, DSBeginSchedule(store, jobs));
    ASSERT_EQ(DS_RESULT_OK, DSSchedulerEvent(store, begin.m_Name));
    ASSERT_EQ(DS_RESULT_OK, DSSchedulerEvent(store, dmHashString64("engine.update.end")));
    ASSERT_EQ(DS_RESULT_OK, DSEndSchedule(store));
    ASSERT_EQ(DS_RESULT_OK, DSDestroyQuery(query));
    ASSERT_EQ(DS_RESULT_OK, DSDestroyStore(store));
    JobSystemDestroy(jobs);
}

// An event with all dependency slots populated must wait for the last signal.
// Events never appear as callbacks or keep an otherwise finished function list busy.
TEST(DataScheduler, EventFullDependencyArray)
{
    HJobContext jobs = CreateScheduledJobs();
    HDSContext  store = DSCreateStore(0);
    dmhash_t    names[DS_SYSTEM_MAX_DEPENDENCIES];
    char        name[32];
    DSEventDesc end = { .m_Name = dmHashString64("engine.update.end") };
    for (uint32_t i = 0; i < DS_SYSTEM_MAX_DEPENDENCIES; ++i)
    {
        snprintf(name, sizeof(name), "engine.update.%u", i);
        names[i] = dmHashString64(name);
        end.m_DependsOn[i] = names[i];
    }
    ASSERT_EQ(DS_RESULT_OK, DSRegisterEvent(store, &end));
    for (uint32_t i = 0; i < DS_SYSTEM_MAX_DEPENDENCIES; ++i)
    {
        DSEventDesc event = { .m_Name = names[i] };
        ASSERT_EQ(DS_RESULT_OK, DSRegisterEvent(store, &event));
    }
    ASSERT_EQ(DS_RESULT_OK, DSBeginSchedule(store, jobs));
    ASSERT_EQ(DS_RESULT_OK, DSSchedulerUpdate(store));
    for (uint32_t i = 0; i < DS_SYSTEM_MAX_DEPENDENCIES; ++i)
    {
        ASSERT_EQ(DS_RESULT_BUSY, DSSchedulerEvent(store, end.m_Name));
        ASSERT_EQ(DS_RESULT_OK, DSSchedulerEvent(store, names[i]));
    }
    ASSERT_EQ(DS_RESULT_OK, DSSchedulerEvent(store, end.m_Name));
    ASSERT_EQ(DS_RESULT_OK, DSEndSchedule(store));
    ASSERT_EQ(DS_RESULT_OK, DSDestroyStore(store));
    JobSystemDestroy(jobs);
}

// Hashes differing only in their upper bits must identify distinct events and
// dependencies, even without reverse-hash entries for their names.
TEST(DataScheduler, FullWidthHashIdentity)
{
    HDSContext       store = DSCreateStore(0);
    HJobContext      jobs = CreateScheduledJobs();
    DSQueryDesc      query_desc = { 0 };
    HDSQuery         query = CreateScheduledQuery(store, query_desc);
    ScheduledContext context = { 0 };
    DSEventDesc      first = { .m_Name = UINT64_C(0x100000007) };
    DSEventDesc      second = { .m_Name = UINT64_C(0x200000007) };
    DSSystemDesc     function = {
            .m_Name = UINT64_C(0x300000007),
            .m_DependsOn = { second.m_Name },
            .m_Query = query,
            .m_Process = ReadHealthRows,
            .m_Context = &context
    };
    ASSERT_EQ(DS_RESULT_OK, DSRegisterFunction(store, &function));
    ASSERT_EQ(DS_RESULT_OK, DSRegisterEvent(store, &first));
    ASSERT_EQ(DS_RESULT_OK, DSRegisterEvent(store, &second));
    ASSERT_EQ(DS_RESULT_ALREADY_EXISTS, DSRegisterEvent(store, &second));
    ASSERT_EQ(DS_RESULT_OK, DSBeginSchedule(store, jobs));
    ASSERT_EQ(DS_RESULT_OK, DSSchedulerEvent(store, first.m_Name));
    ASSERT_EQ(DS_RESULT_BUSY, DSSchedulerUpdate(store));
    ASSERT_EQ(0U, context.m_Calls);
    ASSERT_EQ(DS_RESULT_OK, DSSchedulerEvent(store, second.m_Name));
    FinishSchedule(store, 1);
    ASSERT_EQ(1U, context.m_Calls);
    ASSERT_EQ(DS_RESULT_OK, DSUnregisterEvent(store, first.m_Name));
    ASSERT_EQ(DS_RESULT_OK, DSUnregisterEvent(store, second.m_Name));
    ASSERT_EQ(DS_RESULT_OK, DSUnregisterFunction(store, function.m_Name));
    ASSERT_EQ(DS_RESULT_OK, DSDestroyQuery(query));
    ASSERT_EQ(DS_RESULT_OK, DSDestroyStore(store));
    JobSystemDestroy(jobs);
}

// The public scheduler declarations and functions must compile and link from C.
TEST(DataScheduler, PublicRegistrationFromC)
{
    ASSERT_EQ(0, TestDataSchedulerFromC());
}
