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
#include <dmsdk/dlib/math.h>
#include "test_data_memory.h"

struct ScheduledRow
{
    float m_Health;
};

struct ScheduledEntity
{
    float            m_Health;
    DataStoreVector3 m_Position;
};

struct ScheduledContext
{
    DataStoreFieldId m_Field;
    uint32_t         m_Calls;
    float            m_Amount;
    float            m_Total;
};

struct ScheduledRanges
{
    DataStoreFieldId m_Field;
    uint32_t         m_Calls;
    uint32_t         m_Counts[74];
    uint32_t         m_Visits[74];
};

extern "C" int TestDataSchedulerFromC(void);

static void CountAndHealRanges(HDataStoreQuery query, uint32_t first, uint32_t num_rows, void* context)
{
    ScheduledRanges* state = (ScheduledRanges*)context;
    ++state->m_Calls;
    state->m_Counts[first] = num_rows;
    uint32_t          index = first;
    DataStoreIterator it    = DataStoreQueryIterRange(query, first, num_rows);
    while (DataStoreIterNext(&it))
    {
        DataStoreRowIterator rows = DataStoreIterRows(&it);
        while (DataStoreRowIterNext(&rows))
        {
            ++state->m_Visits[index++];
            *DataStoreRowIterGetFloatMut(&rows, state->m_Field) += 1;
        }
    }
    EXPECT_EQ(first + num_rows, index);
}

static void HealRows(HDataStoreQuery query, uint32_t first, uint32_t num_rows, void* context)
{
    ScheduledContext* state = (ScheduledContext*)context;
    ++state->m_Calls;
    DataStoreIterator it = DataStoreQueryIterRange(query, first, num_rows);
    while (DataStoreIterNext(&it))
    {
        DataStoreRowIterator rows = DataStoreIterRows(&it);
        while (DataStoreRowIterNext(&rows))
        {
            *DataStoreRowIterGetFloatMut(&rows, state->m_Field) += state->m_Amount;
        }
    }
}

static void ReadHealthRows(HDataStoreQuery query, uint32_t first, uint32_t num_rows, void* context)
{
    ScheduledContext* state = (ScheduledContext*)context;
    ++state->m_Calls;
    DataStoreIterator it = DataStoreQueryIterRange(query, first, num_rows);
    while (DataStoreIterNext(&it))
    {
        DataStoreRowIterator rows = DataStoreIterRows(&it);
        while (DataStoreRowIterNext(&rows))
        {
            state->m_Total += *DataStoreRowIterGetFloat(&rows, state->m_Field);
        }
    }
}

static void MoveRows(HDataStoreQuery query, uint32_t first, uint32_t num_rows, void* context)
{
    ScheduledContext* state = (ScheduledContext*)context;
    ++state->m_Calls;
    DataStoreIterator it = DataStoreQueryIterRange(query, first, num_rows);
    while (DataStoreIterNext(&it))
    {
        DataStoreRowIterator rows = DataStoreIterRows(&it);
        while (DataStoreRowIterNext(&rows))
        {
            DataStoreRowIterGetVector3Mut(&rows, state->m_Field)->m_Values[0] += state->m_Amount;
        }
    }
}

static HDataStoreContext CreateScheduledStore(DataStoreId* out_ids)
{
    HDataStoreContext  store = DataStoreCreate(0);
    DataStoreFieldDesc fields[] = {
        { dmHashString64("health"), DATASTORE_TYPE_FLOAT, offsetof(ScheduledEntity, m_Health) },
        { dmHashString64("position"), DATASTORE_TYPE_VECTOR3, offsetof(ScheduledEntity, m_Position) }
    };
    DataStoreTableDesc table = { 1, 0, 0, fields, 2, sizeof(ScheduledEntity) };
    EXPECT_EQ(DATASTORE_RESULT_OK, DataStoreTableAdd(store, &table));
    ScheduledEntity input[] = { { 50 }, { 50 }, { 50 } };
    EXPECT_EQ(DATASTORE_RESULT_OK, DataStoreRowCreate(store, 1, 0, 3, input, out_ids));
    return store;
}

static HDataStoreQuery CreateScheduledQuery(HDataStoreContext store, const DataStoreQueryDesc& desc)
{
    HDataStoreQuery query = 0;
    EXPECT_EQ(DATASTORE_RESULT_OK, DataStoreQueryCreate(store, &desc, &query));
    return query;
}

static HJobContext CreateScheduledJobs()
{
    // No workers: JobSystemUpdate(jobs, 0) runs at most one job per call,
    // independent of elapsed time. Tests can inspect each scheduling step.
    JobSystemCreateParams params = { "ds-schedule", 0 };
    return JobSystemCreate(&params);
}

static void FinishSchedule(HDataStoreContext store, uint32_t num_jobs)
{
    // Count deterministic job steps, not retries while waiting for workers.
    for (uint32_t i = 0; i < num_jobs; ++i)
    {
        ASSERT_EQ(i + 1 == num_jobs ? DATASTORE_RESULT_OK : DATASTORE_RESULT_BUSY, DataStoreSchedulerUpdate(store));
    }
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreSchedulerEnd(store));
}

// Opt-in ranges must cover every row exactly once, including partial jobs that
// cross table/chunk boundaries. Zero keeps one job, and large limits cannot overflow.
TEST(DataScheduler, SplitRangesCoverRowsAcrossTables)
{
    DataStoreCreateParams params = { 1024, 256 };
    HDataStoreContext     store = DataStoreCreate(&params);
    DataStoreFieldDesc    field = { dmHashString64("health"), DATASTORE_TYPE_FLOAT };
    DataStoreTableDesc    table = { 0, 0, 0, &field, 1, sizeof(ScheduledEntity), DATASTORE_RESET_NONE };
    ScheduledEntity       input[37] = {};
    DataStoreId           ids[74];
    for (uint32_t t = 0; t < 2; ++t)
    {
        table.m_Type = t + 1;
        ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreTableAdd(store, &table));
        ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreRowCreate(store, table.m_Type, 0, 37, input, ids + t * 37));
    }
    DataStoreQueryDesc desc = { { DataStoreField("health", DATASTORE_TYPE_FLOAT, DATASTORE_FIELD_MUTABLE) }, 0, 0, 0 };
    HDataStoreQuery    query = CreateScheduledQuery(store, desc);
    DataStoreFieldId   field_index = DataStoreQueryFindField(query, &desc.m_Items[0]);
    HJobContext        jobs = CreateScheduledJobs();
    uint32_t           sizes[] = { 0, 1, 16, 17, 74, 75, UINT32_MAX };
    for (uint32_t s = 0; s < sizeof(sizes) / sizeof(sizes[0]); ++s)
    {
        ScheduledRanges     ranges   = { field_index };
        DataStoreSystemDesc function = { dmHashString64("heal"), {}, query, CountAndHealRanges, &ranges, sizes[s] };
        ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreFunctionAdd(store, &function));
        ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreSchedulerBegin(store, jobs));
        uint32_t stride = sizes[s] ? sizes[s] : 74;
        uint32_t num_jobs = 1 + 73 / stride;
        FinishSchedule(store, num_jobs);
        ASSERT_EQ(num_jobs, ranges.m_Calls);
        for (uint32_t r = 0; r < 74; ++r)
        {
            uint32_t expected = r % stride == 0 ? dmMath::Min(stride, 74 - r) : 0;
            ASSERT_EQ(expected, ranges.m_Counts[r]);
            ASSERT_EQ(1u, ranges.m_Visits[r]);
        }
        float values[74];
        ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreFieldGetFloatBatch(store, 74, ids, field.m_Field, values));
        for (uint32_t r = 0; r < 74; ++r)
        {
            ASSERT_EQ((float)(s + 1), values[r]);
        }
        ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreFunctionRemove(store, function.m_Name));
    }
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreQueryDestroy(query));
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreDestroy(store));
    JobSystemDestroy(jobs);
}

// Repeated schedules recompute ranges after population changes. Empty queries
// still call the callback once so dependents can run, with and without splitting.
TEST(DataScheduler, SplitRangesRefreshAfterPopulationChanges)
{
    DataStoreId        ids[3];
    HDataStoreContext  store = CreateScheduledStore(ids);
    DataStoreQueryDesc desc = { { DataStoreField("health", DATASTORE_TYPE_FLOAT, DATASTORE_FIELD_MUTABLE) }, 0, 0, 0 };
    HDataStoreQuery    query = CreateScheduledQuery(store, desc);
    ScheduledRanges    ranges = { DataStoreQueryFindField(query, &desc.m_Items[0]) };
    HJobContext        jobs = CreateScheduledJobs();
    for (uint32_t rows_per_job = 0; rows_per_job <= 2; rows_per_job += 2)
    {
        DataStoreSystemDesc function = { dmHashString64("heal"), {}, query, CountAndHealRanges, &ranges, rows_per_job };
        ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreFunctionAdd(store, &function));
        for (uint32_t num_rows = 3;; --num_rows)
        {
            ranges = { ranges.m_Field };
            ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreSchedulerBegin(store, jobs));
            uint32_t num_jobs = rows_per_job && num_rows ? 1 + (num_rows - 1) / rows_per_job : 1;
            FinishSchedule(store, num_jobs);
            ASSERT_EQ(num_jobs, ranges.m_Calls);
            for (uint32_t r = 0; r < 3; ++r)
            {
                ASSERT_EQ(r < num_rows ? 1u : 0u, ranges.m_Visits[r]);
            }
            if (!num_rows)
            {
                ASSERT_EQ(0u, ranges.m_Counts[0]);
                break;
            }
            ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreRowRemove(store, ids[num_rows - 1]));
        }
        ScheduledEntity input[3] = {};
        ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreRowCreate(store, 1, 0, 3, input, ids));
        ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreSchedulerBegin(store, jobs));
        FinishSchedule(store, rows_per_job ? 2 : 1);
        ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreFunctionRemove(store, function.m_Name));
    }
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreQueryDestroy(query));
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreDestroy(store));
    JobSystemDestroy(jobs);
}

// A split function remains reserved and incomplete until its final job finishes.
// Explicit read-only dependencies need the same completion barrier as conflicting writes.
TEST(DataScheduler, SplitRangesHoldAccessAndDependencies)
{
    HJobContext jobs = CreateScheduledJobs();
    for (uint32_t writable = 0; writable < 2; ++writable)
    {
        DataStoreId        ids[3];
        HDataStoreContext  store = CreateScheduledStore(ids);
        DataStoreQueryDesc desc = { { DataStoreField("health", DATASTORE_TYPE_FLOAT, writable ? DATASTORE_FIELD_MUTABLE : 0) }, 0, 0, 0 };
        HDataStoreQuery    producer = CreateScheduledQuery(store, desc);
        ScheduledContext   produced = { DataStoreQueryFindField(producer, &desc.m_Items[0]), 0, 1 };
        desc.m_Items[0].m_Access = 0;
        HDataStoreQuery     consumer = CreateScheduledQuery(store, desc);
        ScheduledContext    consumed = { DataStoreQueryFindField(consumer, &desc.m_Items[0]) };
        DataStoreSystemDesc read     = { dmHashString64("consume"), { dmHashString64("produce") }, consumer, ReadHealthRows, &consumed };
        DataStoreSystemDesc write    = { dmHashString64("produce"), {}, producer, writable ? HealRows : ReadHealthRows, &produced, 1 };
        DataStoreEventDesc  done     = { dmHashString64("done"), { write.m_Name } };
        ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreFunctionAdd(store, &read));
        ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreFunctionAdd(store, &write));
        ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreEventAdd(store, &done));
        ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreSchedulerBegin(store, jobs));
        for (uint32_t job = 0; job < 3; ++job)
        {
            ASSERT_EQ(DATASTORE_RESULT_BUSY, DataStoreSchedulerEnd(store));
            ASSERT_EQ(DATASTORE_RESULT_BUSY, DataStoreSchedulerEvent(store, done.m_Name));
            ASSERT_EQ(DATASTORE_RESULT_BUSY, DataStoreQueryTryBegin(producer));
            ASSERT_EQ(DATASTORE_RESULT_LOCKED, DataStoreRowRemove(store, ids[0]));
            ASSERT_EQ(DATASTORE_RESULT_BUSY, DataStoreSchedulerUpdate(store));
            ASSERT_EQ(job + 1, produced.m_Calls);
            ASSERT_EQ(0u, consumed.m_Calls);
        }
        ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreSchedulerEvent(store, done.m_Name));
        FinishSchedule(store, 1);
        ASSERT_EQ(1u, consumed.m_Calls);
        ASSERT_EQ(3.0f * (50 + writable), consumed.m_Total);
        ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreQueryDestroy(producer));
        ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreQueryDestroy(consumer));
        ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreDestroy(store));
    }
    JobSystemDestroy(jobs);
}

// Registration copies hashes while preserving the callback's
// borrowed query/context and the row range supplied by its dispatcher.
TEST(DataScheduler, CopiesHashesAndBorrowsQueryAndContext)
{
    HDataStoreContext  store = DataStoreCreate(0);
    DataStoreFieldDesc field = { dmHashString64("health"), DATASTORE_TYPE_FLOAT, offsetof(ScheduledRow, m_Health) };
    DataStoreTableDesc table = { 1, 0, 0, &field, 1, sizeof(ScheduledRow) };
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreTableAdd(store, &table));
    ScheduledRow input[] = { { 100 }, { 50 }, { 25 } };
    DataStoreId  ids[3];
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreRowCreate(store, 1, 0, 3, input, ids));

    DataStoreQueryDesc query_desc = { { DataStoreField("health", DATASTORE_TYPE_FLOAT, DATASTORE_FIELD_MUTABLE) }, 0, 0, 0 };
    HDataStoreQuery    query;
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreQueryCreate(store, &query_desc, &query));
    ScheduledContext    context = { DataStoreQueryFindField(query, &query_desc.m_Items[0]), 0, 5 };
    DataStoreSystemDesc desc    = { dmHashString64("game.heal"), { dmHashString64("engine.frame_begin") }, query, HealRows, &context };
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreFunctionAdd(store, &desc));
    ASSERT_EQ(0u, context.m_Calls);

    desc.m_Name = 0;
    desc.m_DependsOn[0] = 0;
    desc.m_Query = 0;
    desc.m_Process = 0;
    desc.m_Context = 0;
    const DataStoreSystemDesc& registered = store->m_Systems[0].m_Desc;
    ASSERT_EQ(dmHashString64("game.heal"), registered.m_Name);
    ASSERT_EQ(dmHashString64("engine.frame_begin"), registered.m_DependsOn[0]);
    ASSERT_EQ(query, registered.m_Query);
    ASSERT_TRUE(HealRows == registered.m_Process);
    ASSERT_EQ((void*)&context, registered.m_Context);

    HJobContext jobs = CreateScheduledJobs();
    DataStoreEventDesc begin = { dmHashString64("engine.frame_begin") };
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreEventAdd(store, &begin));
    context.m_Amount = 10;
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreSchedulerBegin(store, jobs));
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreSchedulerEvent(store, begin.m_Name));
    FinishSchedule(store, 1);
    JobSystemDestroy(jobs);
    ASSERT_EQ(1u, context.m_Calls);
    float values[3];
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreFieldGetFloatBatch(store, 3, ids, field.m_Field, values));
    ASSERT_EQ(110.0f, values[0]);
    ASSERT_EQ(60.0f, values[1]);
    ASSERT_EQ(35.0f, values[2]);

    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreFunctionRemove(store, dmHashString64("game.heal")));
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreQueryTryBegin(query));
    DataStoreQueryEnd(query);
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreQueryDestroy(query));
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreDestroy(store));
}

// Growth and swap removal must preserve other registrations, including a full
// dependency array without a terminator and names registered out of order.
TEST(DataScheduler, GrowthRemovalAndFullDependencyArray)
{
    HDataStoreContext  store = DataStoreCreate(0);
    DataStoreQueryDesc query_desc = { 0 };
    HDataStoreQuery    query;
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreQueryCreate(store, &query_desc, &query));
    char                name[32];
    DataStoreSystemDesc desc = { 0, {}, query, HealRows };
    for (uint32_t i = 0; i < DATASTORE_SYSTEM_MAX_DEPENDENCIES; ++i)
    {
        snprintf(name, sizeof(name), "prerequisite_%u", i);
        desc.m_DependsOn[i] = dmHashString64(name);
    }
    for (uint32_t i = 0; i < 24; ++i)
    {
        snprintf(name, sizeof(name), "system_%u", i);
        desc.m_Name = dmHashString64(name);
        ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreFunctionAdd(store, &desc));
    }
    ASSERT_EQ(DATASTORE_RESULT_ALREADY_EXISTS, DataStoreFunctionAdd(store, &desc));
    ASSERT_EQ(24u, store->m_Systems.Size());
    ASSERT_EQ(dmHashString64("prerequisite_15"), store->m_Systems[0].m_Desc.m_DependsOn[15]);
    for (uint32_t i = 1; i < 24; i += 2)
    {
        snprintf(name, sizeof(name), "system_%u", i);
        ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreFunctionRemove(store, dmHashString64(name)));
        ASSERT_EQ(DATASTORE_RESULT_NOT_FOUND, DataStoreFunctionRemove(store, dmHashString64(name)));
    }
    for (uint32_t i = 0; i < 24; i += 2)
    {
        snprintf(name, sizeof(name), "system_%u", i);
        ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreFunctionRemove(store, dmHashString64(name)));
    }
    ASSERT_EQ(0u, store->m_Systems.Size());
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreFunctionAdd(store, &desc));
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreQueryDestroy(query));
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreDestroy(store));
}

// Invalid declarations must not leave partial registrations or accept another
// store's query; a null user context remains valid.
TEST(DataScheduler, RejectsInvalidDeclarations)
{
    HDataStoreContext  store = DataStoreCreate(0);
    HDataStoreContext  other_store = DataStoreCreate(0);
    DataStoreQueryDesc query_desc = { 0 };
    HDataStoreQuery    query;
    HDataStoreQuery    other_query;
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreQueryCreate(store, &query_desc, &query));
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreQueryCreate(other_store, &query_desc, &other_query));
    DataStoreSystemDesc desc = { dmHashString64("game.heal"), {}, other_query, HealRows };
    ASSERT_EQ(DATASTORE_RESULT_INVALID_ARGUMENT, DataStoreFunctionAdd(store, &desc));
    desc.m_Query = query;
    desc.m_Name = 0;
    ASSERT_EQ(DATASTORE_RESULT_INVALID_ARGUMENT, DataStoreFunctionAdd(store, &desc));
    desc.m_Name = dmHashString64("game.heal");
    desc.m_Process = 0;
    ASSERT_EQ(DATASTORE_RESULT_INVALID_ARGUMENT, DataStoreFunctionAdd(store, &desc));
    desc.m_Process = HealRows;
    ASSERT_EQ(0u, store->m_Systems.Size());
    desc.m_DependsOn[0] = 0;
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreFunctionAdd(store, &desc));
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreQueryDestroy(other_query));
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreQueryDestroy(query));
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreDestroy(other_store));
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreDestroy(store));
}

// Hashes are scoped to the owning context. Rejected destruction must preserve
// registrations; completed teardown must not invoke callbacks or touch another store.
TEST(DataScheduler, ContextOwnsRegistrations)
{
    HDataStoreContext  store = DataStoreCreate(0);
    HDataStoreContext  other_store = DataStoreCreate(0);
    DataStoreQueryDesc query_desc = { 0 };
    HDataStoreQuery    query;
    HDataStoreQuery    other_query;
    ScheduledContext   context = { 0 };
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreQueryCreate(store, &query_desc, &query));
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreQueryCreate(other_store, &query_desc, &other_query));
    DataStoreSystemDesc desc = { dmHashString64("game.heal"), {}, query, HealRows, &context };
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreFunctionAdd(store, &desc));
    desc.m_Query = other_query;
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreFunctionAdd(other_store, &desc));

    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreQueryTryBegin(query));
    ASSERT_EQ(DATASTORE_RESULT_LOCKED, DataStoreDestroy(store));
    ASSERT_EQ(1u, store->m_Systems.Size());
    ASSERT_EQ(dmHashString64("game.heal"), store->m_Systems[0].m_Desc.m_Name);
    DataStoreQueryEnd(query);
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreFunctionRemove(store, desc.m_Name));
    ASSERT_EQ(0u, store->m_Systems.Size());
    ASSERT_EQ(1u, other_store->m_Systems.Size());
    ASSERT_EQ(dmHashString64("game.heal"), other_store->m_Systems[0].m_Desc.m_Name);

    // Teardown leaves the copied registration for the context to release.
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreQueryDestroy(other_query));
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreDestroy(other_store));
    ASSERT_EQ(0u, context.m_Calls);
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreQueryTryBegin(query));
    DataStoreQueryEnd(query);
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreQueryDestroy(query));
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreDestroy(store));
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
            DataStoreId        ids[3];
            HDataStoreContext  store = CreateScheduledStore(ids);
            DataStoreQueryDesc desc = { { DataStoreField("health", DATASTORE_TYPE_FLOAT, a ? DATASTORE_FIELD_MUTABLE : 0) }, 0, 0, 0 };
            HDataStoreQuery    first = CreateScheduledQuery(store, desc);
            ScheduledContext   first_context = { DataStoreQueryFindField(first, &desc.m_Items[0]), 0, 1 };
            desc.m_Items[0].m_Access = b ? DATASTORE_FIELD_MUTABLE : 0;
            HDataStoreQuery     second         = CreateScheduledQuery(store, desc);
            ScheduledContext    second_context = { DataStoreQueryFindField(second, &desc.m_Items[0]), 0, 1 };
            DataStoreSystemDesc first_desc     = { dmHashString64("first"), { dmHashString64("engine.update.begin") }, first, a ? HealRows : ReadHealthRows, &first_context };
            DataStoreSystemDesc second_desc    = { dmHashString64("second"), { dmHashString64("engine.update.begin") }, second, b ? HealRows : ReadHealthRows, &second_context };
            DataStoreEventDesc  begin          = { dmHashString64("engine.update.begin") };
            ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreFunctionAdd(store, &first_desc));
            ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreFunctionAdd(store, &second_desc));
            ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreEventAdd(store, &begin));
            for (uint32_t frame = 0; frame < 2; ++frame)
            {
                first_context.m_Total = second_context.m_Total = 0;
                ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreSchedulerBegin(store, jobs));
                ASSERT_EQ(DATASTORE_RESULT_BUSY, DataStoreSchedulerUpdate(store));
                ASSERT_EQ(0u, store->m_ActiveQueries.Size());
                ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreSchedulerEvent(store, begin.m_Name));
                ASSERT_EQ(a || b ? 1u : 2u, store->m_ActiveQueries.Size());
                ASSERT_EQ(frame, first_context.m_Calls);
                ASSERT_EQ(frame, second_context.m_Calls);
                ASSERT_EQ(DATASTORE_RESULT_BUSY, DataStoreSchedulerEnd(store));
                FinishSchedule(store, 2);
                ASSERT_EQ(frame + 1, first_context.m_Calls);
                ASSERT_EQ(frame + 1, second_context.m_Calls);
                if (!a)
                {
                    ASSERT_EQ(3.0f * (50 + frame * (a + b)), first_context.m_Total);
                }
                if (!b)
                {
                    ASSERT_EQ(3.0f * (50 + frame * (a + b) + a), second_context.m_Total);
                }
                float health[3];
                ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreFieldGetFloatBatch(store, 3, ids, dmHashString64("health"), health));
                for (uint32_t i = 0; i < 3; ++i)
                {
                    ASSERT_EQ(50.0f + (frame + 1) * (a + b), health[i]);
                }
            }
            ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreQueryDestroy(first));
            ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreQueryDestroy(second));
            ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreDestroy(store));
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
        DataStoreId        ids[3];
        HDataStoreContext  store = CreateScheduledStore(ids);
        DataStoreQueryDesc desc = { { DataStoreField("health", DATASTORE_TYPE_FLOAT, writable ? DATASTORE_FIELD_MUTABLE : 0) }, 0, 0, 0 };
        HDataStoreQuery    producer = CreateScheduledQuery(store, desc);
        ScheduledContext   producer_context = { DataStoreQueryFindField(producer, &desc.m_Items[0]), 0, 1 };
        desc.m_Items[0].m_Access = 0;
        HDataStoreQuery     consumer         = CreateScheduledQuery(store, desc);
        ScheduledContext    consumer_context = { DataStoreQueryFindField(consumer, &desc.m_Items[0]) };
        DataStoreSystemDesc consume          = { dmHashString64("consume"), { dmHashString64("produce") }, consumer, ReadHealthRows, &consumer_context };
        DataStoreSystemDesc produce          = { dmHashString64("produce"), {}, producer, writable ? HealRows : ReadHealthRows, &producer_context };
        ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreFunctionAdd(store, &consume));
        ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreFunctionAdd(store, &produce));
        for (uint32_t frame = 0; frame < 2; ++frame)
        {
            consumer_context.m_Total = 0;
            ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreSchedulerBegin(store, jobs));
            ASSERT_EQ(1u, store->m_ActiveQueries.Size());
            // The normal engine job pump is sufficient: completion schedules dependents.
            JobSystemUpdate(jobs, 0);
            ASSERT_EQ(frame + 1, producer_context.m_Calls);
            ASSERT_EQ(frame, consumer_context.m_Calls);
            ASSERT_EQ(DATASTORE_RESULT_BUSY, DataStoreQueryTryBegin(consumer));
            FinishSchedule(store, 1);
            ASSERT_EQ(frame + 1, consumer_context.m_Calls);
            ASSERT_EQ(3.0f * (50 + (frame + 1) * writable), consumer_context.m_Total);
        }
        ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreQueryDestroy(producer));
        ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreQueryDestroy(consumer));
        ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreDestroy(store));
    }
    JobSystemDestroy(jobs);
}

// A blocked reader must not stop independent writes. Group filters and separate
// fields permit three jobs to reserve access before any work has executed.
TEST(DataScheduler, DisjointFieldsAndGroups)
{
    DataStoreId       ids[3];
    HDataStoreContext store = CreateScheduledStore(ids);
    DataStoreId       other_group = NextTestGroupId();
    ScheduledEntity   input = { 75 };
    DataStoreId       other_id;
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreRowCreate(store, 1, other_group, 1, &input, &other_id));
    DataStoreId        groups[] = { 0, other_group };
    DataStoreQueryDesc desc = { { DataStoreField("health", DATASTORE_TYPE_FLOAT, DATASTORE_FIELD_MUTABLE) }, groups, 1, 0 };
    HDataStoreQuery    first = CreateScheduledQuery(store, desc);
    ScheduledContext   first_context = { DataStoreQueryFindField(first, &desc.m_Items[0]), 0, 1 };
    desc.m_GroupIds = groups + 1;
    HDataStoreQuery  second         = CreateScheduledQuery(store, desc);
    ScheduledContext second_context = { DataStoreQueryFindField(second, &desc.m_Items[0]), 0, 2 };
    desc = { { DataStoreField("health", DATASTORE_TYPE_FLOAT, 0) }, 0, 0, 0 };
    HDataStoreQuery  reader       = CreateScheduledQuery(store, desc);
    ScheduledContext read_context = { DataStoreQueryFindField(reader, &desc.m_Items[0]) };
    desc = { { DataStoreField("position", DATASTORE_TYPE_VECTOR3, DATASTORE_FIELD_MUTABLE) }, 0, 0, 0 };
    HDataStoreQuery     movement = CreateScheduledQuery(store, desc);
    ScheduledContext    move_context = { DataStoreQueryFindField(movement, &desc.m_Items[0]), 0, 10 };
    DataStoreSystemDesc systems[] = {
        { dmHashString64("health_7"), {}, first, HealRows, &first_context },
        { dmHashString64("read_health"), {}, reader, ReadHealthRows, &read_context },
        { dmHashString64("movement"), {}, movement, MoveRows, &move_context },
        { dmHashString64("health_8"), {}, second, HealRows, &second_context }
    };
    for (uint32_t i = 0; i < 4; ++i)
    {
        ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreFunctionAdd(store, &systems[i]));
    }
    HJobContext jobs = CreateScheduledJobs();
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreSchedulerBegin(store, jobs));
    ASSERT_EQ(3u, store->m_ActiveQueries.Size());
    FinishSchedule(store, 4);
    ASSERT_EQ(230.0f, read_context.m_Total);
    ASSERT_EQ(1u, first_context.m_Calls);
    ASSERT_EQ(1u, second_context.m_Calls);
    ASSERT_EQ(1u, move_context.m_Calls);
    ASSERT_EQ(1u, read_context.m_Calls);
    DataStoreVector3 position;
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreFieldGetVector3(store, other_id, dmHashString64("position"), &position));
    ASSERT_EQ(10.0f, position.m_Values[0]);
    for (uint32_t i = 0; i < 4; ++i)
    {
        ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreQueryDestroy(systems[i].m_Query));
    }
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreDestroy(store));
    JobSystemDestroy(jobs);
}

// Missing names and mixed event/function cycles fail before jobs are submitted.
// Removing and replacing registrations must rebuild dependency indices.
TEST(DataScheduler, GraphValidationAndRegistrationChanges)
{
    HJobContext         jobs     = CreateScheduledJobs();
    HDataStoreContext   store    = DataStoreCreate(0);
    DataStoreQueryDesc  desc     = { 0 };
    HDataStoreQuery     query    = CreateScheduledQuery(store, desc);
    ScheduledContext    context  = { 0 };
    DataStoreSystemDesc function = { dmHashString64("heal"), { dmHashString64("engine.begin") }, query, HealRows, &context };
    DataStoreEventDesc  begin    = { dmHashString64("engine.begin"), { dmHashString64("heal") } };
    DataStoreEventDesc  end      = { dmHashString64("engine.end"), { dmHashString64("heal") } };
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreFunctionAdd(store, &function));
    ASSERT_EQ(DATASTORE_RESULT_NOT_FOUND, DataStoreSchedulerBegin(store, jobs));
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreEventAdd(store, &begin));
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreEventAdd(store, &end));
    ASSERT_EQ(DATASTORE_RESULT_INVALID_ARGUMENT, DataStoreSchedulerBegin(store, jobs));
    ASSERT_EQ(0u, context.m_Calls);
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreEventRemove(store, begin.m_Name));
    begin.m_DependsOn[0] = begin.m_Name;
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreEventAdd(store, &begin));
    ASSERT_EQ(DATASTORE_RESULT_INVALID_ARGUMENT, DataStoreSchedulerBegin(store, jobs));
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreEventRemove(store, begin.m_Name));
    begin.m_DependsOn[0] = 0;
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreEventAdd(store, &begin));
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreSchedulerBegin(store, jobs));
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreSchedulerEvent(store, begin.m_Name));
    ASSERT_EQ(DATASTORE_RESULT_BUSY, DataStoreSchedulerEvent(store, end.m_Name));
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreSchedulerUpdate(store));
    ASSERT_EQ(1u, context.m_Calls);
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreSchedulerEvent(store, end.m_Name));
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreSchedulerEnd(store));
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreFunctionRemove(store, function.m_Name));
    ASSERT_EQ(DATASTORE_RESULT_NOT_FOUND, DataStoreSchedulerBegin(store, jobs));
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreEventRemove(store, end.m_Name));
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreSchedulerBegin(store, jobs));
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreSchedulerUpdate(store));
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreSchedulerEnd(store));
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreQueryDestroy(query));
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreDestroy(store));
    JobSystemDestroy(jobs);
}

// All sixteen prerequisites must finish before the consumer job runs. Reusing one
// query across functions must serialize its reservations without caller dispatch.
TEST(DataScheduler, FullDependencyArray)
{
    HJobContext         jobs = CreateScheduledJobs();
    HDataStoreContext   store = DataStoreCreate(0);
    DataStoreQueryDesc  desc = { 0 };
    HDataStoreQuery     query = CreateScheduledQuery(store, desc);
    ScheduledContext    producer_context = { 0 }, consumer_context = { 0 };
    dmhash_t            names[DATASTORE_SYSTEM_MAX_DEPENDENCIES];
    char                name[32];
    DataStoreSystemDesc consumer = { dmHashString64("consumer"), {}, query, ReadHealthRows, &consumer_context };
    for (uint32_t i = 0; i < DATASTORE_SYSTEM_MAX_DEPENDENCIES; ++i)
    {
        snprintf(name, sizeof(name), "producer_%u", i);
        names[i] = dmHashString64(name);
        consumer.m_DependsOn[i] = names[i];
    }
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreFunctionAdd(store, &consumer));
    for (uint32_t i = 0; i < DATASTORE_SYSTEM_MAX_DEPENDENCIES; ++i)
    {
        DataStoreSystemDesc producer = { names[i], {}, query, ReadHealthRows, &producer_context };
        ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreFunctionAdd(store, &producer));
    }
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreSchedulerBegin(store, jobs));
    for (uint32_t i = 0; i < DATASTORE_SYSTEM_MAX_DEPENDENCIES; ++i)
    {
        ASSERT_EQ(1u, store->m_ActiveQueries.Size());
        ASSERT_EQ(DATASTORE_RESULT_BUSY, DataStoreSchedulerUpdate(store));
        ASSERT_EQ(i + 1, producer_context.m_Calls);
        ASSERT_EQ(0u, consumer_context.m_Calls);
    }
    FinishSchedule(store, 1);
    ASSERT_EQ(1u, consumer_context.m_Calls);
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreQueryDestroy(query));
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreDestroy(store));
    JobSystemDestroy(jobs);
}

// Empty queries still run one job. Teardown/registration cannot invalidate its
// borrowed inputs; ending before an event discards only work not yet submitted.
TEST(DataScheduler, ScheduleLifetimeAndEmptyQueries)
{
    HJobContext       jobs  = CreateScheduledJobs();
    HDataStoreContext store = DataStoreCreate(0);
    ASSERT_EQ(DATASTORE_RESULT_INVALID_ARGUMENT, DataStoreSchedulerBegin(store, 0));
    ASSERT_EQ(DATASTORE_RESULT_INVALID_ARGUMENT, DataStoreSchedulerUpdate(store));
    ASSERT_EQ(DATASTORE_RESULT_INVALID_ARGUMENT, DataStoreSchedulerEnd(store));
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreSchedulerBegin(store, jobs));
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreSchedulerUpdate(store));
    ASSERT_EQ(DATASTORE_RESULT_LOCKED, DataStoreDestroy(store));
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreSchedulerEnd(store));
    DataStoreQueryDesc  desc      = { 0 };
    HDataStoreQuery     query     = CreateScheduledQuery(store, desc);
    HDataStoreQuery     unrelated = CreateScheduledQuery(store, desc);
    ScheduledContext    context   = { 0 };
    DataStoreEventDesc  begin     = { dmHashString64("engine.begin") };
    DataStoreSystemDesc system    = { dmHashString64("empty"), { dmHashString64("engine.begin") }, query, ReadHealthRows, &context };
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreEventAdd(store, &begin));
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreFunctionAdd(store, &system));
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreSchedulerBegin(store, jobs));
    ASSERT_EQ(DATASTORE_RESULT_LOCKED, DataStoreSchedulerBegin(store, jobs));
    ASSERT_EQ(DATASTORE_RESULT_LOCKED, DataStoreFunctionAdd(store, &system));
    ASSERT_EQ(DATASTORE_RESULT_LOCKED, DataStoreFunctionRemove(store, dmHashString64("empty")));
    ASSERT_EQ(DATASTORE_RESULT_LOCKED, DataStoreQueryDestroy(query));
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreQueryDestroy(unrelated));
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreSchedulerEnd(store));
    ASSERT_EQ(0u, context.m_Calls);
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreSchedulerBegin(store, jobs));
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreSchedulerEvent(store, begin.m_Name));
    ASSERT_EQ(0u, DataStoreQueryGetRowCount(query));
    ASSERT_EQ(DATASTORE_RESULT_BUSY, DataStoreQueryTryBegin(query));
    ASSERT_EQ(DATASTORE_RESULT_BUSY, DataStoreSchedulerEnd(store));
    ASSERT_EQ(DATASTORE_RESULT_LOCKED, DataStoreDestroy(store));
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreSchedulerUpdate(store));
    ASSERT_EQ(1u, context.m_Calls);
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreSchedulerUpdate(store));
    ASSERT_EQ(1u, context.m_Calls);
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreSchedulerEnd(store));
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreFunctionRemove(store, dmHashString64("empty")));
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreQueryDestroy(query));
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreDestroy(store));
    JobSystemDestroy(jobs);
}

// An external reservation delays automatic dispatch. Update retries after release,
// and new table matches must affect conflict checks in the next invocation.
TEST(DataScheduler, TableChangesAndExternalReservations)
{
    HJobContext        jobs = CreateScheduledJobs();
    HDataStoreContext  store = DataStoreCreate(0);
    uint64_t           tags[] = { dmHashString64("first"), dmHashString64("second") };
    DataStoreFieldDesc field = { dmHashString64("health"), DATASTORE_TYPE_FLOAT };
    DataStoreTableDesc table = { 1, tags, 1, &field, 1, sizeof(ScheduledRow) };
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreTableAdd(store, &table));
    ScheduledRow input = { 50 };
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreRowCreate(store, 1, 0, 1, &input, 0));
    DataStoreQueryDesc desc          = { { DataStoreTag("first"), DataStoreField("health", DATASTORE_TYPE_FLOAT, DATASTORE_FIELD_MUTABLE) }, 0, 0, 0 };
    HDataStoreQuery    writer        = CreateScheduledQuery(store, desc);
    ScheduledContext   write_context = { DataStoreQueryFindField(writer, &desc.m_Items[1]), 0, 1 };
    desc = { { DataStoreTag("second"), DataStoreField("health", DATASTORE_TYPE_FLOAT, 0) }, 0, 0, 0 };
    HDataStoreQuery     reader       = CreateScheduledQuery(store, desc);
    ScheduledContext    read_context = { DataStoreQueryFindField(reader, &desc.m_Items[1]) };
    DataStoreSystemDesc write        = { dmHashString64("write"), {}, writer, HealRows, &write_context };
    DataStoreSystemDesc read         = { dmHashString64("read"), {}, reader, ReadHealthRows, &read_context };
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreFunctionAdd(store, &write));
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreFunctionAdd(store, &read));
    for (uint32_t pass = 0; pass < 3; ++pass)
    {
        if (pass == 1)
        {
            table.m_Type = 2;
            table.m_TagCount = 2;
            ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreTableAdd(store, &table));
            ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreRowCreate(store, 2, 0, 1, &input, 0));
        }
        if (pass == 2)
        {
            ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreTableRemove(store, 2));
        }
        ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreSchedulerBegin(store, jobs));
        ASSERT_EQ(pass == 1 ? 1u : 2u, store->m_ActiveQueries.Size());
        FinishSchedule(store, 2);
    }
    desc = { { DataStoreField("health", DATASTORE_TYPE_FLOAT, 0) }, 0, 0, 0 };
    HDataStoreQuery external = CreateScheduledQuery(store, desc);
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreQueryTryBegin(external));
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreSchedulerBegin(store, jobs));
    ASSERT_EQ(DATASTORE_RESULT_BUSY, DataStoreSchedulerUpdate(store));
    ASSERT_EQ(3u, write_context.m_Calls);
    ASSERT_EQ(4u, read_context.m_Calls);
    DataStoreQueryEnd(external);
    ASSERT_EQ(DATASTORE_RESULT_BUSY, DataStoreSchedulerUpdate(store));
    ASSERT_EQ(DATASTORE_RESULT_BUSY, DataStoreQueryTryBegin(writer));
    FinishSchedule(store, 1);
    ASSERT_EQ(4u, write_context.m_Calls);
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreQueryDestroy(external));
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreQueryDestroy(reader));
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreQueryDestroy(writer));
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreDestroy(store));
    JobSystemDestroy(jobs);
}

// Ending must retry ready work blocked on query access, while ignoring a chain
// behind an unsignaled event. The engine uses this end/update loop each frame.
TEST(DataScheduler, EndWaitsForReadyFunctions)
{
    HJobContext         jobs = CreateScheduledJobs();
    DataStoreId         ids[3];
    HDataStoreContext   store = CreateScheduledStore(ids);
    DataStoreQueryDesc  desc = { { DataStoreField("health", DATASTORE_TYPE_FLOAT, DATASTORE_FIELD_MUTABLE) }, 0, 0, 0 };
    HDataStoreQuery     query = CreateScheduledQuery(store, desc);
    ScheduledContext    writer = { DataStoreQueryFindField(query, &desc.m_Items[0]), 0, 1 };
    ScheduledContext    reader = { writer.m_Field };
    ScheduledContext    pending = { writer.m_Field };
    DataStoreEventDesc  events[] = { { dmHashString64("begin") }, { dmHashString64("manual") } };
    DataStoreSystemDesc systems[] = {
        { dmHashString64("write"), { events[0].m_Name }, query, HealRows, &writer },
        { dmHashString64("read"), { dmHashString64("write") }, query, ReadHealthRows, &reader },
        { dmHashString64("pending"), { events[1].m_Name }, query, ReadHealthRows, &pending },
        { dmHashString64("pending_child"), { dmHashString64("pending") }, query, ReadHealthRows, &pending }
    };
    for (uint32_t i = 0; i < DM_ARRAY_SIZE(events); ++i)
    {
        ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreEventAdd(store, &events[i]));
    }
    for (uint32_t i = 0; i < DM_ARRAY_SIZE(systems); ++i)
    {
        ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreFunctionAdd(store, &systems[i]));
    }

    for (uint32_t frame = 0; frame < 2; ++frame)
    {
        ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreQueryTryBegin(query));
        ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreSchedulerBegin(store, jobs));
        ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreSchedulerEvent(store, events[0].m_Name));
        ASSERT_EQ(DATASTORE_RESULT_BUSY, DataStoreSchedulerEnd(store));
        ASSERT_EQ(frame, writer.m_Calls);
        DataStoreQueryEnd(query);
        ASSERT_EQ(DATASTORE_RESULT_BUSY, DataStoreSchedulerEnd(store));
        while (DataStoreSchedulerEnd(store) == DATASTORE_RESULT_BUSY)
        {
            DataStoreSchedulerUpdate(store);
        }
        ASSERT_EQ(frame + 1, writer.m_Calls);
        ASSERT_EQ(frame + 1, reader.m_Calls);
        ASSERT_EQ(0u, pending.m_Calls);
    }

    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreQueryDestroy(query));
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreDestroy(store));
    JobSystemDestroy(jobs);
}

// Signaling an entry queues its jobs without executing them on the caller. A later
// sync waits for ready work without discarding later events or rerunning completed
// functions. The next schedule starts each function afresh.
TEST(DataScheduler, EventsLaunchFunctions)
{
    HJobContext        jobs = CreateScheduledJobs();
    DataStoreId        ids[3];
    HDataStoreContext  store = CreateScheduledStore(ids);
    DataStoreQueryDesc desc = { { DataStoreField("health", DATASTORE_TYPE_FLOAT, DATASTORE_FIELD_MUTABLE) }, 0, 0, 0 };
    HDataStoreQuery    writer = CreateScheduledQuery(store, desc);
    ScheduledContext   write_context = { DataStoreQueryFindField(writer, &desc.m_Items[0]), 0, 1 };
    desc.m_Items[0].m_Access = 0;
    HDataStoreQuery     reader       = CreateScheduledQuery(store, desc);
    ScheduledContext    read_context = { DataStoreQueryFindField(reader, &desc.m_Items[0]) };
    DataStoreSystemDesc write        = { dmHashString64("heal"), { dmHashString64("engine.begin") }, writer, HealRows, &write_context };
    DataStoreSystemDesc read         = { dmHashString64("observe"), { dmHashString64("engine.end") }, reader, ReadHealthRows, &read_context };
    DataStoreEventDesc  begin        = { dmHashString64("engine.begin") };
    DataStoreEventDesc  end          = { dmHashString64("engine.end"), { dmHashString64("heal") } };
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreFunctionAdd(store, &read));
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreEventAdd(store, &end));
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreFunctionAdd(store, &write));
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreEventAdd(store, &begin));
    ASSERT_EQ(DATASTORE_RESULT_INVALID_ARGUMENT, DataStoreSchedulerSync(store));
    for (uint32_t frame = 0; frame < 3; ++frame)
    {
        read_context.m_Total = 0;
        ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreSchedulerBegin(store, jobs));
        ASSERT_EQ(DATASTORE_RESULT_BUSY, DataStoreSchedulerUpdate(store));
        ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreSchedulerSync(store));
        ASSERT_EQ(frame, write_context.m_Calls);
        ASSERT_EQ(frame, read_context.m_Calls);
        ASSERT_EQ(DATASTORE_RESULT_BUSY, DataStoreSchedulerEvent(store, end.m_Name));
        ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreSchedulerEvent(store, begin.m_Name));
        ASSERT_EQ(frame, write_context.m_Calls);
        ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreSchedulerSync(store));
        ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreSchedulerSync(store));
        ASSERT_EQ(frame + 1, write_context.m_Calls);
        ASSERT_EQ(frame, read_context.m_Calls);
        ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreQueryTryBegin(writer));
        DataStoreQueryEnd(writer);
        ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreSchedulerEvent(store, end.m_Name));
        ASSERT_EQ(DATASTORE_RESULT_BUSY, DataStoreSchedulerEnd(store));
        ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreSchedulerSync(store));
        ASSERT_EQ(frame + 1, write_context.m_Calls);
        ASSERT_EQ(frame + 1, read_context.m_Calls);
        ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreSchedulerEnd(store));
        ASSERT_EQ(3.0f * (51 + frame), read_context.m_Total);
    }
    ASSERT_EQ(DATASTORE_RESULT_INVALID_ARGUMENT, DataStoreSchedulerSync(store));
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreQueryDestroy(reader));
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreQueryDestroy(writer));
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreDestroy(store));
    JobSystemDestroy(jobs);
}

// Events copy their hashes and share the function namespace. Wrong-kind operations,
// duplicates and mid-invocation registration changes must leave the graph intact.
TEST(DataScheduler, EventRegistrationAndLifetime)
{
    HJobContext        jobs       = CreateScheduledJobs();
    HDataStoreContext  store      = DataStoreCreate(0);
    DataStoreQueryDesc query_desc = { 0 };
    HDataStoreQuery    query      = CreateScheduledQuery(store, query_desc);
    DataStoreEventDesc invalid    = { 0 };
    ASSERT_EQ(DATASTORE_RESULT_INVALID_ARGUMENT, DataStoreEventAdd(store, &invalid));

    DataStoreEventDesc end = { dmHashString64("engine.update.end"), { dmHashString64("engine.update.begin") } };
    DataStoreEventDesc begin = { dmHashString64("engine.update.begin") };
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreEventAdd(store, &end));
    ASSERT_EQ(DATASTORE_RESULT_ALREADY_EXISTS, DataStoreEventAdd(store, &end));
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreEventAdd(store, &begin));
    end.m_Name = 0;
    end.m_DependsOn[0] = 0;
    DataStoreSystemDesc function = { dmHashString64("engine.update.end"), { dmHashString64("never") }, query, HealRows };
    ASSERT_EQ(DATASTORE_RESULT_ALREADY_EXISTS, DataStoreFunctionAdd(store, &function));
    ASSERT_EQ(DATASTORE_RESULT_INVALID_ARGUMENT, DataStoreFunctionRemove(store, dmHashString64("engine.update.end")));
    DataStoreEventDesc never = { dmHashString64("never") };
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreEventAdd(store, &never));
    function.m_Name = dmHashString64("heal");
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreFunctionAdd(store, &function));
    DataStoreEventDesc duplicate = { dmHashString64("heal") };
    ASSERT_EQ(DATASTORE_RESULT_ALREADY_EXISTS, DataStoreEventAdd(store, &duplicate));
    ASSERT_EQ(DATASTORE_RESULT_INVALID_ARGUMENT, DataStoreEventRemove(store, dmHashString64("heal")));
    ASSERT_EQ(DATASTORE_RESULT_INVALID_ARGUMENT, DataStoreSchedulerEvent(store, begin.m_Name));
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreSchedulerBegin(store, jobs));
    ASSERT_EQ(DATASTORE_RESULT_LOCKED, DataStoreEventAdd(store, &begin));
    ASSERT_EQ(DATASTORE_RESULT_LOCKED, DataStoreEventRemove(store, begin.m_Name));
    ASSERT_EQ(DATASTORE_RESULT_NOT_FOUND, DataStoreSchedulerEvent(store, dmHashString64("missing")));
    ASSERT_EQ(DATASTORE_RESULT_INVALID_ARGUMENT, DataStoreSchedulerEvent(store, dmHashString64("heal")));
    ASSERT_EQ(DATASTORE_RESULT_BUSY, DataStoreSchedulerEvent(store, dmHashString64("engine.update.end")));
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreSchedulerEvent(store, begin.m_Name));
    ASSERT_EQ(DATASTORE_RESULT_INVALID_ARGUMENT, DataStoreSchedulerEvent(store, begin.m_Name));
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreSchedulerEvent(store, dmHashString64("engine.update.end")));
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreSchedulerEnd(store));

    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreSchedulerBegin(store, jobs));
    ASSERT_EQ(DATASTORE_RESULT_BUSY, DataStoreSchedulerEvent(store, dmHashString64("engine.update.end")));
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreSchedulerEnd(store));
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreEventRemove(store, begin.m_Name));
    ASSERT_EQ(DATASTORE_RESULT_NOT_FOUND, DataStoreEventRemove(store, begin.m_Name));
    ASSERT_EQ(DATASTORE_RESULT_NOT_FOUND, DataStoreSchedulerBegin(store, jobs));
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreEventAdd(store, &begin));
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreSchedulerBegin(store, jobs));
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreSchedulerEvent(store, begin.m_Name));
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreSchedulerEvent(store, dmHashString64("engine.update.end")));
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreSchedulerEnd(store));
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreQueryDestroy(query));
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreDestroy(store));
    JobSystemDestroy(jobs);
}

// An event with all dependency slots populated must wait for the last signal.
// Events never appear as callbacks or keep an otherwise finished function list busy.
TEST(DataScheduler, EventFullDependencyArray)
{
    HJobContext        jobs = CreateScheduledJobs();
    HDataStoreContext  store = DataStoreCreate(0);
    dmhash_t           names[DATASTORE_SYSTEM_MAX_DEPENDENCIES];
    char               name[32];
    DataStoreEventDesc end = { dmHashString64("engine.update.end") };
    for (uint32_t i = 0; i < DATASTORE_SYSTEM_MAX_DEPENDENCIES; ++i)
    {
        snprintf(name, sizeof(name), "engine.update.%u", i);
        names[i] = dmHashString64(name);
        end.m_DependsOn[i] = names[i];
    }
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreEventAdd(store, &end));
    for (uint32_t i = 0; i < DATASTORE_SYSTEM_MAX_DEPENDENCIES; ++i)
    {
        DataStoreEventDesc event = { names[i] };
        ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreEventAdd(store, &event));
    }
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreSchedulerBegin(store, jobs));
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreSchedulerUpdate(store));
    for (uint32_t i = 0; i < DATASTORE_SYSTEM_MAX_DEPENDENCIES; ++i)
    {
        ASSERT_EQ(DATASTORE_RESULT_BUSY, DataStoreSchedulerEvent(store, end.m_Name));
        ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreSchedulerEvent(store, names[i]));
    }
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreSchedulerEvent(store, end.m_Name));
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreSchedulerEnd(store));
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreDestroy(store));
    JobSystemDestroy(jobs);
}

// Hashes differing only in their upper bits must identify distinct events and
// dependencies, even without reverse-hash entries for their names.
TEST(DataScheduler, FullWidthHashIdentity)
{
    HDataStoreContext   store      = DataStoreCreate(0);
    HJobContext         jobs       = CreateScheduledJobs();
    DataStoreQueryDesc  query_desc = { 0 };
    HDataStoreQuery     query      = CreateScheduledQuery(store, query_desc);
    ScheduledContext    context    = { 0 };
    DataStoreEventDesc  first      = { UINT64_C(0x100000007) };
    DataStoreEventDesc  second     = { UINT64_C(0x200000007) };
    DataStoreSystemDesc function   = {
        UINT64_C(0x300000007),
        { second.m_Name },
        query,
        ReadHealthRows,
        &context
    };
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreFunctionAdd(store, &function));
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreEventAdd(store, &first));
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreEventAdd(store, &second));
    ASSERT_EQ(DATASTORE_RESULT_ALREADY_EXISTS, DataStoreEventAdd(store, &second));
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreSchedulerBegin(store, jobs));
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreSchedulerEvent(store, first.m_Name));
    ASSERT_EQ(DATASTORE_RESULT_BUSY, DataStoreSchedulerUpdate(store));
    ASSERT_EQ(0U, context.m_Calls);
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreSchedulerEvent(store, second.m_Name));
    FinishSchedule(store, 1);
    ASSERT_EQ(1U, context.m_Calls);
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreEventRemove(store, first.m_Name));
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreEventRemove(store, second.m_Name));
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreFunctionRemove(store, function.m_Name));
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreQueryDestroy(query));
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreDestroy(store));
    JobSystemDestroy(jobs);
}

// The public scheduler declarations and functions must compile and link from C.
TEST(DataScheduler, PublicRegistrationFromC)
{
    ASSERT_EQ(0, TestDataSchedulerFromC());
}
