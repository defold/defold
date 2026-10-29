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
    float     m_Health;
    DSVector3 m_Position;
};

struct ScheduledContext
{
    DSFieldId m_Field;
    uint32_t m_Calls;
    float    m_Amount;
    float    m_Total;
};

struct ScheduledRanges
{
    DSFieldId m_Field;
    uint32_t m_Calls;
    uint32_t m_Counts[74];
    uint32_t m_Visits[74];
};

extern "C" int TestDataSchedulerFromC(void);

static void CountAndHealRanges(HDSQuery query, uint32_t first, uint32_t num_rows, void* context)
{
    ScheduledRanges* state = (ScheduledRanges*)context;
    ++state->m_Calls;
    state->m_Counts[first] = num_rows;
    uint32_t   index = first;
    DSIterator it = DSQueryIterRange(query, first, num_rows);
    while (DSIterNext(&it))
    {
        DSRowIterator rows = DSIterRows(&it);
        while (DSRowIterNext(&rows))
        {
            ++state->m_Visits[index++];
            *DSRowIterGetFloatMut(&rows, state->m_Field) += 1;
        }
    }
    EXPECT_EQ(first + num_rows, index);
}

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
    HDSContext  store = DSStoreCreate(0);
    DSFieldDesc fields[] = {
        { dmHashString64("health"), DS_TYPE_FLOAT, offsetof(ScheduledEntity, m_Health) },
        { dmHashString64("position"), DS_TYPE_VECTOR3, offsetof(ScheduledEntity, m_Position) }
    };
    DSTableDesc table = { 1, 0, 0, fields, 2, sizeof(ScheduledEntity) };
    EXPECT_EQ(DS_RESULT_OK, DSTableAdd(store, &table));
    ScheduledEntity input[] = { { 50 }, { 50 }, { 50 } };
    EXPECT_EQ(DS_RESULT_OK, DSRowCreate(store, 1, 0, 3, input, out_ids));
    return store;
}

static HDSQuery CreateScheduledQuery(HDSContext store, const DSQueryDesc& desc)
{
    HDSQuery query = 0;
    EXPECT_EQ(DS_RESULT_OK, DSQueryCreate(store, &desc, &query));
    return query;
}

static HJobContext CreateScheduledJobs()
{
    // No workers: JobSystemUpdate(jobs, 0) runs at most one job per call,
    // independent of elapsed time. Tests can inspect each scheduling step.
    JobSystemCreateParams params = { "ds-schedule", 0 };
    return JobSystemCreate(&params);
}

static void FinishSchedule(HDSContext store, uint32_t num_jobs)
{
    // Count deterministic job steps, not retries while waiting for workers.
    for (uint32_t i = 0; i < num_jobs; ++i)
        ASSERT_EQ(i + 1 == num_jobs ? DS_RESULT_OK : DS_RESULT_BUSY, DSSchedulerUpdate(store));
    ASSERT_EQ(DS_RESULT_OK, DSSchedulerEnd(store));
}

// Opt-in ranges must cover every row exactly once, including partial jobs that
// cross table/chunk boundaries. Zero keeps one job, and large limits cannot overflow.
TEST(DataScheduler, SplitRangesCoverRowsAcrossTables)
{
    DSCreateParams  params = { 1024, 256 };
    HDSContext      store = DSStoreCreate(&params);
    DSFieldDesc     field = { dmHashString64("health"), DS_TYPE_FLOAT };
    DSTableDesc     table = { 0, 0, 0, &field, 1, sizeof(ScheduledEntity), DS_RESET_NONE };
    ScheduledEntity input[37] = {};
    DSId            ids[74];
    for (uint32_t t = 0; t < 2; ++t)
    {
        table.m_Type = t + 1;
        ASSERT_EQ(DS_RESULT_OK, DSTableAdd(store, &table));
        ASSERT_EQ(DS_RESULT_OK, DSRowCreate(store, table.m_Type, 0, 37, input, ids + t * 37));
    }
    DSQueryDesc desc = { 0, 0, 0, { DSField("health", DS_TYPE_FLOAT, DS_FIELD_MUTABLE) } };
    HDSQuery    query = CreateScheduledQuery(store, desc);
    DSFieldId   field_index = DSQueryFindField(query, &desc.m_Items[0]);
    HJobContext jobs = CreateScheduledJobs();
    uint32_t    sizes[] = { 0, 1, 16, 17, 74, 75, UINT32_MAX };
    for (uint32_t s = 0; s < sizeof(sizes) / sizeof(sizes[0]); ++s)
    {
        ScheduledRanges ranges = { field_index };
        DSSystemDesc    function = { dmHashString64("heal"), {}, query, CountAndHealRanges, &ranges, sizes[s] };
        ASSERT_EQ(DS_RESULT_OK, DSFunctionAdd(store, &function));
        ASSERT_EQ(DS_RESULT_OK, DSSchedulerBegin(store, jobs));
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
        ASSERT_EQ(DS_RESULT_OK, DSFieldGetFloatBatch(store, 74, ids, field.m_Field, values));
        for (uint32_t r = 0; r < 74; ++r)
            ASSERT_EQ((float)(s + 1), values[r]);
        ASSERT_EQ(DS_RESULT_OK, DSFunctionRemove(store, function.m_Name));
    }
    ASSERT_EQ(DS_RESULT_OK, DSQueryDestroy(query));
    ASSERT_EQ(DS_RESULT_OK, DSStoreDestroy(store));
    JobSystemDestroy(jobs);
}

// Repeated schedules recompute ranges after population changes. Empty queries
// still call the callback once so dependents can run, with and without splitting.
TEST(DataScheduler, SplitRangesRefreshAfterPopulationChanges)
{
    DSId            ids[3];
    HDSContext      store = CreateScheduledStore(ids);
    DSQueryDesc     desc = { 0, 0, 0, { DSField("health", DS_TYPE_FLOAT, DS_FIELD_MUTABLE) } };
    HDSQuery        query = CreateScheduledQuery(store, desc);
    ScheduledRanges ranges = { DSQueryFindField(query, &desc.m_Items[0]) };
    HJobContext     jobs = CreateScheduledJobs();
    for (uint32_t rows_per_job = 0; rows_per_job <= 2; rows_per_job += 2)
    {
        DSSystemDesc function = { dmHashString64("heal"), {}, query, CountAndHealRanges, &ranges, rows_per_job };
        ASSERT_EQ(DS_RESULT_OK, DSFunctionAdd(store, &function));
        for (uint32_t num_rows = 3;; --num_rows)
        {
            ranges = { ranges.m_Field };
            ASSERT_EQ(DS_RESULT_OK, DSSchedulerBegin(store, jobs));
            uint32_t num_jobs = rows_per_job && num_rows ? 1 + (num_rows - 1) / rows_per_job : 1;
            FinishSchedule(store, num_jobs);
            ASSERT_EQ(num_jobs, ranges.m_Calls);
            for (uint32_t r = 0; r < 3; ++r)
                ASSERT_EQ(r < num_rows ? 1u : 0u, ranges.m_Visits[r]);
            if (!num_rows)
            {
                ASSERT_EQ(0u, ranges.m_Counts[0]);
                break;
            }
            ASSERT_EQ(DS_RESULT_OK, DSRowRemove(store, ids[num_rows - 1]));
        }
        ScheduledEntity input[3] = {};
        ASSERT_EQ(DS_RESULT_OK, DSRowCreate(store, 1, 0, 3, input, ids));
        ASSERT_EQ(DS_RESULT_OK, DSSchedulerBegin(store, jobs));
        FinishSchedule(store, rows_per_job ? 2 : 1);
        ASSERT_EQ(DS_RESULT_OK, DSFunctionRemove(store, function.m_Name));
    }
    ASSERT_EQ(DS_RESULT_OK, DSQueryDestroy(query));
    ASSERT_EQ(DS_RESULT_OK, DSStoreDestroy(store));
    JobSystemDestroy(jobs);
}

// A split function remains reserved and incomplete until its final job finishes.
// Explicit read-only dependencies need the same completion barrier as conflicting writes.
TEST(DataScheduler, SplitRangesHoldAccessAndDependencies)
{
    HJobContext jobs = CreateScheduledJobs();
    for (uint32_t writable = 0; writable < 2; ++writable)
    {
        DSId             ids[3];
        HDSContext       store = CreateScheduledStore(ids);
        DSQueryDesc      desc = { 0, 0, 0, { DSField("health", DS_TYPE_FLOAT, writable ? DS_FIELD_MUTABLE : 0) } };
        HDSQuery         producer = CreateScheduledQuery(store, desc);
        ScheduledContext produced = { DSQueryFindField(producer, &desc.m_Items[0]), 0, 1 };
        desc.m_Items[0].m_Access = 0;
        HDSQuery         consumer = CreateScheduledQuery(store, desc);
        ScheduledContext consumed = { DSQueryFindField(consumer, &desc.m_Items[0]) };
        DSSystemDesc     read = { dmHashString64("consume"), { dmHashString64("produce") }, consumer, ReadHealthRows, &consumed };
        DSSystemDesc     write = { dmHashString64("produce"), {}, producer, writable ? HealRows : ReadHealthRows, &produced, 1 };
        DSEventDesc      done = { dmHashString64("done"), { write.m_Name } };
        ASSERT_EQ(DS_RESULT_OK, DSFunctionAdd(store, &read));
        ASSERT_EQ(DS_RESULT_OK, DSFunctionAdd(store, &write));
        ASSERT_EQ(DS_RESULT_OK, DSEventAdd(store, &done));
        ASSERT_EQ(DS_RESULT_OK, DSSchedulerBegin(store, jobs));
        for (uint32_t job = 0; job < 3; ++job)
        {
            ASSERT_EQ(DS_RESULT_BUSY, DSSchedulerEnd(store));
            ASSERT_EQ(DS_RESULT_BUSY, DSSchedulerEvent(store, done.m_Name));
            ASSERT_EQ(DS_RESULT_BUSY, DSQueryTryBegin(producer));
            ASSERT_EQ(DS_RESULT_LOCKED, DSRowRemove(store, ids[0]));
            ASSERT_EQ(DS_RESULT_BUSY, DSSchedulerUpdate(store));
            ASSERT_EQ(job + 1, produced.m_Calls);
            ASSERT_EQ(0u, consumed.m_Calls);
        }
        ASSERT_EQ(DS_RESULT_OK, DSSchedulerEvent(store, done.m_Name));
        FinishSchedule(store, 1);
        ASSERT_EQ(1u, consumed.m_Calls);
        ASSERT_EQ(3.0f * (50 + writable), consumed.m_Total);
        ASSERT_EQ(DS_RESULT_OK, DSQueryDestroy(producer));
        ASSERT_EQ(DS_RESULT_OK, DSQueryDestroy(consumer));
        ASSERT_EQ(DS_RESULT_OK, DSStoreDestroy(store));
    }
    JobSystemDestroy(jobs);
}

// Registration copies hashes while preserving the callback's
// borrowed query/context and the row range supplied by its dispatcher.
TEST(DataScheduler, CopiesHashesAndBorrowsQueryAndContext)
{
    HDSContext  store = DSStoreCreate(0);
    DSFieldDesc field = { dmHashString64("health"), DS_TYPE_FLOAT, offsetof(ScheduledRow, m_Health) };
    DSTableDesc table = { 1, 0, 0, &field, 1, sizeof(ScheduledRow) };
    ASSERT_EQ(DS_RESULT_OK, DSTableAdd(store, &table));
    ScheduledRow input[] = { { 100 }, { 50 }, { 25 } };
    DSId         ids[3];
    ASSERT_EQ(DS_RESULT_OK, DSRowCreate(store, 1, 0, 3, input, ids));

    DSQueryDesc query_desc = { 0, 0, 0, { DSField("health", DS_TYPE_FLOAT, DS_FIELD_MUTABLE) } };
    HDSQuery    query;
    ASSERT_EQ(DS_RESULT_OK, DSQueryCreate(store, &query_desc, &query));
    ScheduledContext context = { DSQueryFindField(query, &query_desc.m_Items[0]), 0, 5 };
    DSSystemDesc     desc = { dmHashString64("game.heal"), { dmHashString64("engine.frame_begin") }, query, HealRows, &context };
    ASSERT_EQ(DS_RESULT_OK, DSFunctionAdd(store, &desc));
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
    DSEventDesc begin = { dmHashString64("engine.frame_begin") };
    ASSERT_EQ(DS_RESULT_OK, DSEventAdd(store, &begin));
    context.m_Amount = 10;
    ASSERT_EQ(DS_RESULT_OK, DSSchedulerBegin(store, jobs));
    ASSERT_EQ(DS_RESULT_OK, DSSchedulerEvent(store, begin.m_Name));
    FinishSchedule(store, 1);
    JobSystemDestroy(jobs);
    ASSERT_EQ(1u, context.m_Calls);
    float values[3];
    ASSERT_EQ(DS_RESULT_OK, DSFieldGetFloatBatch(store, 3, ids, field.m_Field, values));
    ASSERT_EQ(110.0f, values[0]);
    ASSERT_EQ(60.0f, values[1]);
    ASSERT_EQ(35.0f, values[2]);

    ASSERT_EQ(DS_RESULT_OK, DSFunctionRemove(store, dmHashString64("game.heal")));
    ASSERT_EQ(DS_RESULT_OK, DSQueryTryBegin(query));
    DSQueryEnd(query);
    ASSERT_EQ(DS_RESULT_OK, DSQueryDestroy(query));
    ASSERT_EQ(DS_RESULT_OK, DSStoreDestroy(store));
}

// Growth and swap removal must preserve other registrations, including a full
// dependency array without a terminator and names registered out of order.
TEST(DataScheduler, GrowthRemovalAndFullDependencyArray)
{
    HDSContext  store = DSStoreCreate(0);
    DSQueryDesc query_desc = { 0 };
    HDSQuery    query;
    ASSERT_EQ(DS_RESULT_OK, DSQueryCreate(store, &query_desc, &query));
    char         name[32];
    DSSystemDesc desc = { 0, {}, query, HealRows };
    for (uint32_t i = 0; i < DS_SYSTEM_MAX_DEPENDENCIES; ++i)
    {
        snprintf(name, sizeof(name), "prerequisite_%u", i);
        desc.m_DependsOn[i] = dmHashString64(name);
    }
    for (uint32_t i = 0; i < 24; ++i)
    {
        snprintf(name, sizeof(name), "system_%u", i);
        desc.m_Name = dmHashString64(name);
        ASSERT_EQ(DS_RESULT_OK, DSFunctionAdd(store, &desc));
    }
    ASSERT_EQ(DS_RESULT_ALREADY_EXISTS, DSFunctionAdd(store, &desc));
    ASSERT_EQ(24u, store->m_Systems.Size());
    ASSERT_EQ(dmHashString64("prerequisite_15"), store->m_Systems[0].m_Desc.m_DependsOn[15]);
    for (uint32_t i = 1; i < 24; i += 2)
    {
        snprintf(name, sizeof(name), "system_%u", i);
        ASSERT_EQ(DS_RESULT_OK, DSFunctionRemove(store, dmHashString64(name)));
        ASSERT_EQ(DS_RESULT_NOT_FOUND, DSFunctionRemove(store, dmHashString64(name)));
    }
    for (uint32_t i = 0; i < 24; i += 2)
    {
        snprintf(name, sizeof(name), "system_%u", i);
        ASSERT_EQ(DS_RESULT_OK, DSFunctionRemove(store, dmHashString64(name)));
    }
    ASSERT_EQ(0u, store->m_Systems.Size());
    ASSERT_EQ(DS_RESULT_OK, DSFunctionAdd(store, &desc));
    ASSERT_EQ(DS_RESULT_OK, DSQueryDestroy(query));
    ASSERT_EQ(DS_RESULT_OK, DSStoreDestroy(store));
}

// Invalid declarations must not leave partial registrations or accept another
// store's query; a null user context remains valid.
TEST(DataScheduler, RejectsInvalidDeclarations)
{
    HDSContext  store = DSStoreCreate(0);
    HDSContext  other_store = DSStoreCreate(0);
    DSQueryDesc query_desc = { 0 };
    HDSQuery    query;
    HDSQuery    other_query;
    ASSERT_EQ(DS_RESULT_OK, DSQueryCreate(store, &query_desc, &query));
    ASSERT_EQ(DS_RESULT_OK, DSQueryCreate(other_store, &query_desc, &other_query));
    DSSystemDesc desc = { dmHashString64("game.heal"), {}, other_query, HealRows };
    ASSERT_EQ(DS_RESULT_INVALID_ARGUMENT, DSFunctionAdd(store, &desc));
    desc.m_Query = query;
    desc.m_Name = 0;
    ASSERT_EQ(DS_RESULT_INVALID_ARGUMENT, DSFunctionAdd(store, &desc));
    desc.m_Name = dmHashString64("game.heal");
    desc.m_Process = 0;
    ASSERT_EQ(DS_RESULT_INVALID_ARGUMENT, DSFunctionAdd(store, &desc));
    desc.m_Process = HealRows;
    ASSERT_EQ(0u, store->m_Systems.Size());
    desc.m_DependsOn[0] = 0;
    ASSERT_EQ(DS_RESULT_OK, DSFunctionAdd(store, &desc));
    ASSERT_EQ(DS_RESULT_OK, DSQueryDestroy(other_query));
    ASSERT_EQ(DS_RESULT_OK, DSQueryDestroy(query));
    ASSERT_EQ(DS_RESULT_OK, DSStoreDestroy(other_store));
    ASSERT_EQ(DS_RESULT_OK, DSStoreDestroy(store));
}

// Hashes are scoped to the owning context. Rejected destruction must preserve
// registrations; completed teardown must not invoke callbacks or touch another store.
TEST(DataScheduler, ContextOwnsRegistrations)
{
    HDSContext       store = DSStoreCreate(0);
    HDSContext       other_store = DSStoreCreate(0);
    DSQueryDesc      query_desc = { 0 };
    HDSQuery         query;
    HDSQuery         other_query;
    ScheduledContext context = { 0 };
    ASSERT_EQ(DS_RESULT_OK, DSQueryCreate(store, &query_desc, &query));
    ASSERT_EQ(DS_RESULT_OK, DSQueryCreate(other_store, &query_desc, &other_query));
    DSSystemDesc desc = { dmHashString64("game.heal"), {}, query, HealRows, &context };
    ASSERT_EQ(DS_RESULT_OK, DSFunctionAdd(store, &desc));
    desc.m_Query = other_query;
    ASSERT_EQ(DS_RESULT_OK, DSFunctionAdd(other_store, &desc));

    ASSERT_EQ(DS_RESULT_OK, DSQueryTryBegin(query));
    ASSERT_EQ(DS_RESULT_LOCKED, DSStoreDestroy(store));
    ASSERT_EQ(1u, store->m_Systems.Size());
    ASSERT_EQ(dmHashString64("game.heal"), store->m_Systems[0].m_Desc.m_Name);
    DSQueryEnd(query);
    ASSERT_EQ(DS_RESULT_OK, DSFunctionRemove(store, desc.m_Name));
    ASSERT_EQ(0u, store->m_Systems.Size());
    ASSERT_EQ(1u, other_store->m_Systems.Size());
    ASSERT_EQ(dmHashString64("game.heal"), other_store->m_Systems[0].m_Desc.m_Name);

    // Teardown leaves the copied registration for the context to release.
    ASSERT_EQ(DS_RESULT_OK, DSQueryDestroy(other_query));
    ASSERT_EQ(DS_RESULT_OK, DSStoreDestroy(other_store));
    ASSERT_EQ(0u, context.m_Calls);
    ASSERT_EQ(DS_RESULT_OK, DSQueryTryBegin(query));
    DSQueryEnd(query);
    ASSERT_EQ(DS_RESULT_OK, DSQueryDestroy(query));
    ASSERT_EQ(DS_RESULT_OK, DSStoreDestroy(store));
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
            DSQueryDesc      desc = { 0, 0, 0, { DSField("health", DS_TYPE_FLOAT, a ? DS_FIELD_MUTABLE : 0) } };
            HDSQuery         first = CreateScheduledQuery(store, desc);
            ScheduledContext first_context = { DSQueryFindField(first, &desc.m_Items[0]), 0, 1 };
            desc.m_Items[0].m_Access = b ? DS_FIELD_MUTABLE : 0;
            HDSQuery         second = CreateScheduledQuery(store, desc);
            ScheduledContext second_context = { DSQueryFindField(second, &desc.m_Items[0]), 0, 1 };
            DSSystemDesc     first_desc = { dmHashString64("first"), { dmHashString64("engine.update.begin") }, first, a ? HealRows : ReadHealthRows, &first_context };
            DSSystemDesc     second_desc = { dmHashString64("second"), { dmHashString64("engine.update.begin") }, second, b ? HealRows : ReadHealthRows, &second_context };
            DSEventDesc      begin = { dmHashString64("engine.update.begin") };
            ASSERT_EQ(DS_RESULT_OK, DSFunctionAdd(store, &first_desc));
            ASSERT_EQ(DS_RESULT_OK, DSFunctionAdd(store, &second_desc));
            ASSERT_EQ(DS_RESULT_OK, DSEventAdd(store, &begin));
            for (uint32_t frame = 0; frame < 2; ++frame)
            {
                first_context.m_Total = second_context.m_Total = 0;
                ASSERT_EQ(DS_RESULT_OK, DSSchedulerBegin(store, jobs));
                ASSERT_EQ(DS_RESULT_BUSY, DSSchedulerUpdate(store));
                ASSERT_EQ(0u, store->m_ActiveQueries.Size());
                ASSERT_EQ(DS_RESULT_OK, DSSchedulerEvent(store, begin.m_Name));
                ASSERT_EQ(a || b ? 1u : 2u, store->m_ActiveQueries.Size());
                ASSERT_EQ(frame, first_context.m_Calls);
                ASSERT_EQ(frame, second_context.m_Calls);
                ASSERT_EQ(DS_RESULT_BUSY, DSSchedulerEnd(store));
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
            ASSERT_EQ(DS_RESULT_OK, DSQueryDestroy(first));
            ASSERT_EQ(DS_RESULT_OK, DSQueryDestroy(second));
            ASSERT_EQ(DS_RESULT_OK, DSStoreDestroy(store));
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
        DSQueryDesc      desc = { 0, 0, 0, { DSField("health", DS_TYPE_FLOAT, writable ? DS_FIELD_MUTABLE : 0) } };
        HDSQuery         producer = CreateScheduledQuery(store, desc);
        ScheduledContext producer_context = { DSQueryFindField(producer, &desc.m_Items[0]), 0, 1 };
        desc.m_Items[0].m_Access = 0;
        HDSQuery         consumer = CreateScheduledQuery(store, desc);
        ScheduledContext consumer_context = { DSQueryFindField(consumer, &desc.m_Items[0]) };
        DSSystemDesc     consume = { dmHashString64("consume"), { dmHashString64("produce") }, consumer, ReadHealthRows, &consumer_context };
        DSSystemDesc     produce = { dmHashString64("produce"), {}, producer, writable ? HealRows : ReadHealthRows, &producer_context };
        ASSERT_EQ(DS_RESULT_OK, DSFunctionAdd(store, &consume));
        ASSERT_EQ(DS_RESULT_OK, DSFunctionAdd(store, &produce));
        for (uint32_t frame = 0; frame < 2; ++frame)
        {
            consumer_context.m_Total = 0;
            ASSERT_EQ(DS_RESULT_OK, DSSchedulerBegin(store, jobs));
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
        ASSERT_EQ(DS_RESULT_OK, DSQueryDestroy(producer));
        ASSERT_EQ(DS_RESULT_OK, DSQueryDestroy(consumer));
        ASSERT_EQ(DS_RESULT_OK, DSStoreDestroy(store));
    }
    JobSystemDestroy(jobs);
}

// A blocked reader must not stop independent writes. Group filters and separate
// fields permit three jobs to reserve access before any work has executed.
TEST(DataScheduler, DisjointFieldsAndGroups)
{
    DSId            ids[3];
    HDSContext      store = CreateScheduledStore(ids);
    DSId            other_group = NextTestGroupId();
    ScheduledEntity input = { 75 };
    DSId            other_id;
    ASSERT_EQ(DS_RESULT_OK, DSRowCreate(store, 1, other_group, 1, &input, &other_id));
    DSId             groups[] = { 0, other_group };
    DSQueryDesc      desc = { groups, 1, 0, { DSField("health", DS_TYPE_FLOAT, DS_FIELD_MUTABLE) } };
    HDSQuery         first = CreateScheduledQuery(store, desc);
    ScheduledContext first_context = { DSQueryFindField(first, &desc.m_Items[0]), 0, 1 };
    desc.m_GroupIds = groups + 1;
    HDSQuery         second = CreateScheduledQuery(store, desc);
    ScheduledContext second_context = { DSQueryFindField(second, &desc.m_Items[0]), 0, 2 };
    desc = { 0, 0, 0, { DSField("health", DS_TYPE_FLOAT, 0) } };
    HDSQuery         reader = CreateScheduledQuery(store, desc);
    ScheduledContext read_context = { DSQueryFindField(reader, &desc.m_Items[0]) };
    desc = { 0, 0, 0, { DSField("position", DS_TYPE_VECTOR3, DS_FIELD_MUTABLE) } };
    HDSQuery         movement = CreateScheduledQuery(store, desc);
    ScheduledContext move_context = { DSQueryFindField(movement, &desc.m_Items[0]), 0, 10 };
    DSSystemDesc     systems[] = {
        { dmHashString64("health_7"), {}, first, HealRows, &first_context },
        { dmHashString64("read_health"), {}, reader, ReadHealthRows, &read_context },
        { dmHashString64("movement"), {}, movement, MoveRows, &move_context },
        { dmHashString64("health_8"), {}, second, HealRows, &second_context }
    };
    for (uint32_t i = 0; i < 4; ++i)
        ASSERT_EQ(DS_RESULT_OK, DSFunctionAdd(store, &systems[i]));
    HJobContext jobs = CreateScheduledJobs();
    ASSERT_EQ(DS_RESULT_OK, DSSchedulerBegin(store, jobs));
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
        ASSERT_EQ(DS_RESULT_OK, DSQueryDestroy(systems[i].m_Query));
    ASSERT_EQ(DS_RESULT_OK, DSStoreDestroy(store));
    JobSystemDestroy(jobs);
}

// Missing names and mixed event/function cycles fail before jobs are submitted.
// Removing and replacing registrations must rebuild dependency indices.
TEST(DataScheduler, GraphValidationAndRegistrationChanges)
{
    HJobContext      jobs = CreateScheduledJobs();
    HDSContext       store = DSStoreCreate(0);
    DSQueryDesc      desc = { 0 };
    HDSQuery         query = CreateScheduledQuery(store, desc);
    ScheduledContext context = { 0 };
    DSSystemDesc     function = { dmHashString64("heal"), { dmHashString64("engine.begin") }, query, HealRows, &context };
    DSEventDesc      begin = { dmHashString64("engine.begin"), { dmHashString64("heal") } };
    DSEventDesc      end = { dmHashString64("engine.end"), { dmHashString64("heal") } };
    ASSERT_EQ(DS_RESULT_OK, DSFunctionAdd(store, &function));
    ASSERT_EQ(DS_RESULT_NOT_FOUND, DSSchedulerBegin(store, jobs));
    ASSERT_EQ(DS_RESULT_OK, DSEventAdd(store, &begin));
    ASSERT_EQ(DS_RESULT_OK, DSEventAdd(store, &end));
    ASSERT_EQ(DS_RESULT_INVALID_ARGUMENT, DSSchedulerBegin(store, jobs));
    ASSERT_EQ(0u, context.m_Calls);
    ASSERT_EQ(DS_RESULT_OK, DSEventRemove(store, begin.m_Name));
    begin.m_DependsOn[0] = begin.m_Name;
    ASSERT_EQ(DS_RESULT_OK, DSEventAdd(store, &begin));
    ASSERT_EQ(DS_RESULT_INVALID_ARGUMENT, DSSchedulerBegin(store, jobs));
    ASSERT_EQ(DS_RESULT_OK, DSEventRemove(store, begin.m_Name));
    begin.m_DependsOn[0] = 0;
    ASSERT_EQ(DS_RESULT_OK, DSEventAdd(store, &begin));
    ASSERT_EQ(DS_RESULT_OK, DSSchedulerBegin(store, jobs));
    ASSERT_EQ(DS_RESULT_OK, DSSchedulerEvent(store, begin.m_Name));
    ASSERT_EQ(DS_RESULT_BUSY, DSSchedulerEvent(store, end.m_Name));
    ASSERT_EQ(DS_RESULT_OK, DSSchedulerUpdate(store));
    ASSERT_EQ(1u, context.m_Calls);
    ASSERT_EQ(DS_RESULT_OK, DSSchedulerEvent(store, end.m_Name));
    ASSERT_EQ(DS_RESULT_OK, DSSchedulerEnd(store));
    ASSERT_EQ(DS_RESULT_OK, DSFunctionRemove(store, function.m_Name));
    ASSERT_EQ(DS_RESULT_NOT_FOUND, DSSchedulerBegin(store, jobs));
    ASSERT_EQ(DS_RESULT_OK, DSEventRemove(store, end.m_Name));
    ASSERT_EQ(DS_RESULT_OK, DSSchedulerBegin(store, jobs));
    ASSERT_EQ(DS_RESULT_OK, DSSchedulerUpdate(store));
    ASSERT_EQ(DS_RESULT_OK, DSSchedulerEnd(store));
    ASSERT_EQ(DS_RESULT_OK, DSQueryDestroy(query));
    ASSERT_EQ(DS_RESULT_OK, DSStoreDestroy(store));
    JobSystemDestroy(jobs);
}

// All sixteen prerequisites must finish before the consumer job runs. Reusing one
// query across functions must serialize its reservations without caller dispatch.
TEST(DataScheduler, FullDependencyArray)
{
    HJobContext      jobs = CreateScheduledJobs();
    HDSContext       store = DSStoreCreate(0);
    DSQueryDesc      desc = { 0 };
    HDSQuery         query = CreateScheduledQuery(store, desc);
    ScheduledContext producer_context = { 0 }, consumer_context = { 0 };
    dmhash_t         names[DS_SYSTEM_MAX_DEPENDENCIES];
    char             name[32];
    DSSystemDesc     consumer = { dmHashString64("consumer"), {}, query, ReadHealthRows, &consumer_context };
    for (uint32_t i = 0; i < DS_SYSTEM_MAX_DEPENDENCIES; ++i)
    {
        snprintf(name, sizeof(name), "producer_%u", i);
        names[i] = dmHashString64(name);
        consumer.m_DependsOn[i] = names[i];
    }
    ASSERT_EQ(DS_RESULT_OK, DSFunctionAdd(store, &consumer));
    for (uint32_t i = 0; i < DS_SYSTEM_MAX_DEPENDENCIES; ++i)
    {
        DSSystemDesc producer = { names[i], {}, query, ReadHealthRows, &producer_context };
        ASSERT_EQ(DS_RESULT_OK, DSFunctionAdd(store, &producer));
    }
    ASSERT_EQ(DS_RESULT_OK, DSSchedulerBegin(store, jobs));
    for (uint32_t i = 0; i < DS_SYSTEM_MAX_DEPENDENCIES; ++i)
    {
        ASSERT_EQ(1u, store->m_ActiveQueries.Size());
        ASSERT_EQ(DS_RESULT_BUSY, DSSchedulerUpdate(store));
        ASSERT_EQ(i + 1, producer_context.m_Calls);
        ASSERT_EQ(0u, consumer_context.m_Calls);
    }
    FinishSchedule(store, 1);
    ASSERT_EQ(1u, consumer_context.m_Calls);
    ASSERT_EQ(DS_RESULT_OK, DSQueryDestroy(query));
    ASSERT_EQ(DS_RESULT_OK, DSStoreDestroy(store));
    JobSystemDestroy(jobs);
}

// Empty queries still run one job. Teardown/registration cannot invalidate its
// borrowed inputs; ending before an event discards only work not yet submitted.
TEST(DataScheduler, ScheduleLifetimeAndEmptyQueries)
{
    HJobContext jobs = CreateScheduledJobs();
    HDSContext  store = DSStoreCreate(0);
    ASSERT_EQ(DS_RESULT_INVALID_ARGUMENT, DSSchedulerBegin(store, 0));
    ASSERT_EQ(DS_RESULT_INVALID_ARGUMENT, DSSchedulerUpdate(store));
    ASSERT_EQ(DS_RESULT_INVALID_ARGUMENT, DSSchedulerEnd(store));
    ASSERT_EQ(DS_RESULT_OK, DSSchedulerBegin(store, jobs));
    ASSERT_EQ(DS_RESULT_OK, DSSchedulerUpdate(store));
    ASSERT_EQ(DS_RESULT_LOCKED, DSStoreDestroy(store));
    ASSERT_EQ(DS_RESULT_OK, DSSchedulerEnd(store));
    DSQueryDesc      desc = { 0 };
    HDSQuery         query = CreateScheduledQuery(store, desc);
    HDSQuery         unrelated = CreateScheduledQuery(store, desc);
    ScheduledContext context = { 0 };
    DSEventDesc      begin = { dmHashString64("engine.begin") };
    DSSystemDesc     system = { dmHashString64("empty"), { dmHashString64("engine.begin") }, query, ReadHealthRows, &context };
    ASSERT_EQ(DS_RESULT_OK, DSEventAdd(store, &begin));
    ASSERT_EQ(DS_RESULT_OK, DSFunctionAdd(store, &system));
    ASSERT_EQ(DS_RESULT_OK, DSSchedulerBegin(store, jobs));
    ASSERT_EQ(DS_RESULT_LOCKED, DSSchedulerBegin(store, jobs));
    ASSERT_EQ(DS_RESULT_LOCKED, DSFunctionAdd(store, &system));
    ASSERT_EQ(DS_RESULT_LOCKED, DSFunctionRemove(store, dmHashString64("empty")));
    ASSERT_EQ(DS_RESULT_LOCKED, DSQueryDestroy(query));
    ASSERT_EQ(DS_RESULT_OK, DSQueryDestroy(unrelated));
    ASSERT_EQ(DS_RESULT_OK, DSSchedulerEnd(store));
    ASSERT_EQ(0u, context.m_Calls);
    ASSERT_EQ(DS_RESULT_OK, DSSchedulerBegin(store, jobs));
    ASSERT_EQ(DS_RESULT_OK, DSSchedulerEvent(store, begin.m_Name));
    ASSERT_EQ(0u, DSQueryGetRowCount(query));
    ASSERT_EQ(DS_RESULT_BUSY, DSQueryTryBegin(query));
    ASSERT_EQ(DS_RESULT_BUSY, DSSchedulerEnd(store));
    ASSERT_EQ(DS_RESULT_LOCKED, DSStoreDestroy(store));
    ASSERT_EQ(DS_RESULT_OK, DSSchedulerUpdate(store));
    ASSERT_EQ(1u, context.m_Calls);
    ASSERT_EQ(DS_RESULT_OK, DSSchedulerUpdate(store));
    ASSERT_EQ(1u, context.m_Calls);
    ASSERT_EQ(DS_RESULT_OK, DSSchedulerEnd(store));
    ASSERT_EQ(DS_RESULT_OK, DSFunctionRemove(store, dmHashString64("empty")));
    ASSERT_EQ(DS_RESULT_OK, DSQueryDestroy(query));
    ASSERT_EQ(DS_RESULT_OK, DSStoreDestroy(store));
    JobSystemDestroy(jobs);
}

// An external reservation delays automatic dispatch. Update retries after release,
// and new table matches must affect conflict checks in the next invocation.
TEST(DataScheduler, TableChangesAndExternalReservations)
{
    HJobContext jobs = CreateScheduledJobs();
    HDSContext  store = DSStoreCreate(0);
    uint64_t    tags[] = { dmHashString64("first"), dmHashString64("second") };
    DSFieldDesc field = { dmHashString64("health"), DS_TYPE_FLOAT };
    DSTableDesc table = { 1, tags, 1, &field, 1, sizeof(ScheduledRow) };
    ASSERT_EQ(DS_RESULT_OK, DSTableAdd(store, &table));
    ScheduledRow input = { 50 };
    ASSERT_EQ(DS_RESULT_OK, DSRowCreate(store, 1, 0, 1, &input, 0));
    DSQueryDesc      desc = { 0, 0, 0, { DSTag("first"), DSField("health", DS_TYPE_FLOAT, DS_FIELD_MUTABLE) } };
    HDSQuery         writer = CreateScheduledQuery(store, desc);
    ScheduledContext write_context = { DSQueryFindField(writer, &desc.m_Items[1]), 0, 1 };
    desc = { 0, 0, 0, { DSTag("second"), DSField("health", DS_TYPE_FLOAT, 0) } };
    HDSQuery         reader = CreateScheduledQuery(store, desc);
    ScheduledContext read_context = { DSQueryFindField(reader, &desc.m_Items[1]) };
    DSSystemDesc     write = { dmHashString64("write"), {}, writer, HealRows, &write_context };
    DSSystemDesc     read = { dmHashString64("read"), {}, reader, ReadHealthRows, &read_context };
    ASSERT_EQ(DS_RESULT_OK, DSFunctionAdd(store, &write));
    ASSERT_EQ(DS_RESULT_OK, DSFunctionAdd(store, &read));
    for (uint32_t pass = 0; pass < 3; ++pass)
    {
        if (pass == 1)
        {
            table.m_Type = 2;
            table.m_TagCount = 2;
            ASSERT_EQ(DS_RESULT_OK, DSTableAdd(store, &table));
            ASSERT_EQ(DS_RESULT_OK, DSRowCreate(store, 2, 0, 1, &input, 0));
        }
        if (pass == 2)
            ASSERT_EQ(DS_RESULT_OK, DSTableRemove(store, 2));
        ASSERT_EQ(DS_RESULT_OK, DSSchedulerBegin(store, jobs));
        ASSERT_EQ(pass == 1 ? 1u : 2u, store->m_ActiveQueries.Size());
        FinishSchedule(store, 2);
    }
    desc = { 0, 0, 0, { DSField("health", DS_TYPE_FLOAT, 0) } };
    HDSQuery external = CreateScheduledQuery(store, desc);
    ASSERT_EQ(DS_RESULT_OK, DSQueryTryBegin(external));
    ASSERT_EQ(DS_RESULT_OK, DSSchedulerBegin(store, jobs));
    ASSERT_EQ(DS_RESULT_BUSY, DSSchedulerUpdate(store));
    ASSERT_EQ(3u, write_context.m_Calls);
    ASSERT_EQ(4u, read_context.m_Calls);
    DSQueryEnd(external);
    ASSERT_EQ(DS_RESULT_BUSY, DSSchedulerUpdate(store));
    ASSERT_EQ(DS_RESULT_BUSY, DSQueryTryBegin(writer));
    FinishSchedule(store, 1);
    ASSERT_EQ(4u, write_context.m_Calls);
    ASSERT_EQ(DS_RESULT_OK, DSQueryDestroy(external));
    ASSERT_EQ(DS_RESULT_OK, DSQueryDestroy(reader));
    ASSERT_EQ(DS_RESULT_OK, DSQueryDestroy(writer));
    ASSERT_EQ(DS_RESULT_OK, DSStoreDestroy(store));
    JobSystemDestroy(jobs);
}

// Ending must retry ready work blocked on query access, while ignoring a chain
// behind an unsignaled event. The engine uses this end/update loop each frame.
TEST(DataScheduler, EndWaitsForReadyFunctions)
{
    HJobContext      jobs = CreateScheduledJobs();
    DSId             ids[3];
    HDSContext       store = CreateScheduledStore(ids);
    DSQueryDesc      desc = { 0, 0, 0, { DSField("health", DS_TYPE_FLOAT, DS_FIELD_MUTABLE) } };
    HDSQuery         query = CreateScheduledQuery(store, desc);
    ScheduledContext writer = { DSQueryFindField(query, &desc.m_Items[0]), 0, 1 };
    ScheduledContext reader = { writer.m_Field };
    ScheduledContext pending = { writer.m_Field };
    DSEventDesc      events[] = { { dmHashString64("begin") }, { dmHashString64("manual") } };
    DSSystemDesc     systems[] = {
        { dmHashString64("write"), { events[0].m_Name }, query, HealRows, &writer },
        { dmHashString64("read"), { dmHashString64("write") }, query, ReadHealthRows, &reader },
        { dmHashString64("pending"), { events[1].m_Name }, query, ReadHealthRows, &pending },
        { dmHashString64("pending_child"), { dmHashString64("pending") }, query, ReadHealthRows, &pending }
    };
    for (uint32_t i = 0; i < DM_ARRAY_SIZE(events); ++i)
        ASSERT_EQ(DS_RESULT_OK, DSEventAdd(store, &events[i]));
    for (uint32_t i = 0; i < DM_ARRAY_SIZE(systems); ++i)
        ASSERT_EQ(DS_RESULT_OK, DSFunctionAdd(store, &systems[i]));

    for (uint32_t frame = 0; frame < 2; ++frame)
    {
        ASSERT_EQ(DS_RESULT_OK, DSQueryTryBegin(query));
        ASSERT_EQ(DS_RESULT_OK, DSSchedulerBegin(store, jobs));
        ASSERT_EQ(DS_RESULT_OK, DSSchedulerEvent(store, events[0].m_Name));
        ASSERT_EQ(DS_RESULT_BUSY, DSSchedulerEnd(store));
        ASSERT_EQ(frame, writer.m_Calls);
        DSQueryEnd(query);
        ASSERT_EQ(DS_RESULT_BUSY, DSSchedulerEnd(store));
        while (DSSchedulerEnd(store) == DS_RESULT_BUSY)
            DSSchedulerUpdate(store);
        ASSERT_EQ(frame + 1, writer.m_Calls);
        ASSERT_EQ(frame + 1, reader.m_Calls);
        ASSERT_EQ(0u, pending.m_Calls);
    }

    ASSERT_EQ(DS_RESULT_OK, DSQueryDestroy(query));
    ASSERT_EQ(DS_RESULT_OK, DSStoreDestroy(store));
    JobSystemDestroy(jobs);
}

// Signaling an entry queues its jobs without executing them on the caller. A later
// sync waits for ready work without discarding later events or rerunning completed
// functions. The next schedule starts each function afresh.
TEST(DataScheduler, EventsLaunchFunctions)
{
    HJobContext      jobs = CreateScheduledJobs();
    DSId             ids[3];
    HDSContext       store = CreateScheduledStore(ids);
    DSQueryDesc      desc = { 0, 0, 0, { DSField("health", DS_TYPE_FLOAT, DS_FIELD_MUTABLE) } };
    HDSQuery         writer = CreateScheduledQuery(store, desc);
    ScheduledContext write_context = { DSQueryFindField(writer, &desc.m_Items[0]), 0, 1 };
    desc.m_Items[0].m_Access = 0;
    HDSQuery         reader = CreateScheduledQuery(store, desc);
    ScheduledContext read_context = { DSQueryFindField(reader, &desc.m_Items[0]) };
    DSSystemDesc     write = { dmHashString64("heal"), { dmHashString64("engine.begin") }, writer, HealRows, &write_context };
    DSSystemDesc     read = { dmHashString64("observe"), { dmHashString64("engine.end") }, reader, ReadHealthRows, &read_context };
    DSEventDesc      begin = { dmHashString64("engine.begin") };
    DSEventDesc      end = { dmHashString64("engine.end"), { dmHashString64("heal") } };
    ASSERT_EQ(DS_RESULT_OK, DSFunctionAdd(store, &read));
    ASSERT_EQ(DS_RESULT_OK, DSEventAdd(store, &end));
    ASSERT_EQ(DS_RESULT_OK, DSFunctionAdd(store, &write));
    ASSERT_EQ(DS_RESULT_OK, DSEventAdd(store, &begin));
    ASSERT_EQ(DS_RESULT_INVALID_ARGUMENT, DSSchedulerSync(store));
    for (uint32_t frame = 0; frame < 3; ++frame)
    {
        read_context.m_Total = 0;
        ASSERT_EQ(DS_RESULT_OK, DSSchedulerBegin(store, jobs));
        ASSERT_EQ(DS_RESULT_BUSY, DSSchedulerUpdate(store));
        ASSERT_EQ(DS_RESULT_OK, DSSchedulerSync(store));
        ASSERT_EQ(frame, write_context.m_Calls);
        ASSERT_EQ(frame, read_context.m_Calls);
        ASSERT_EQ(DS_RESULT_BUSY, DSSchedulerEvent(store, end.m_Name));
        ASSERT_EQ(DS_RESULT_OK, DSSchedulerEvent(store, begin.m_Name));
        ASSERT_EQ(frame, write_context.m_Calls);
        ASSERT_EQ(DS_RESULT_OK, DSSchedulerSync(store));
        ASSERT_EQ(DS_RESULT_OK, DSSchedulerSync(store));
        ASSERT_EQ(frame + 1, write_context.m_Calls);
        ASSERT_EQ(frame, read_context.m_Calls);
        ASSERT_EQ(DS_RESULT_OK, DSQueryTryBegin(writer));
        DSQueryEnd(writer);
        ASSERT_EQ(DS_RESULT_OK, DSSchedulerEvent(store, end.m_Name));
        ASSERT_EQ(DS_RESULT_BUSY, DSSchedulerEnd(store));
        ASSERT_EQ(DS_RESULT_OK, DSSchedulerSync(store));
        ASSERT_EQ(frame + 1, write_context.m_Calls);
        ASSERT_EQ(frame + 1, read_context.m_Calls);
        ASSERT_EQ(DS_RESULT_OK, DSSchedulerEnd(store));
        ASSERT_EQ(3.0f * (51 + frame), read_context.m_Total);
    }
    ASSERT_EQ(DS_RESULT_INVALID_ARGUMENT, DSSchedulerSync(store));
    ASSERT_EQ(DS_RESULT_OK, DSQueryDestroy(reader));
    ASSERT_EQ(DS_RESULT_OK, DSQueryDestroy(writer));
    ASSERT_EQ(DS_RESULT_OK, DSStoreDestroy(store));
    JobSystemDestroy(jobs);
}

// Events copy their hashes and share the function namespace. Wrong-kind operations,
// duplicates and mid-invocation registration changes must leave the graph intact.
TEST(DataScheduler, EventRegistrationAndLifetime)
{
    HJobContext jobs = CreateScheduledJobs();
    HDSContext  store = DSStoreCreate(0);
    DSQueryDesc query_desc = { 0 };
    HDSQuery    query = CreateScheduledQuery(store, query_desc);
    DSEventDesc invalid = { 0 };
    ASSERT_EQ(DS_RESULT_INVALID_ARGUMENT, DSEventAdd(store, &invalid));

    DSEventDesc end = { dmHashString64("engine.update.end"), { dmHashString64("engine.update.begin") } };
    DSEventDesc begin = { dmHashString64("engine.update.begin") };
    ASSERT_EQ(DS_RESULT_OK, DSEventAdd(store, &end));
    ASSERT_EQ(DS_RESULT_ALREADY_EXISTS, DSEventAdd(store, &end));
    ASSERT_EQ(DS_RESULT_OK, DSEventAdd(store, &begin));
    end.m_Name = 0;
    end.m_DependsOn[0] = 0;
    DSSystemDesc function = { dmHashString64("engine.update.end"), { dmHashString64("never") }, query, HealRows };
    ASSERT_EQ(DS_RESULT_ALREADY_EXISTS, DSFunctionAdd(store, &function));
    ASSERT_EQ(DS_RESULT_INVALID_ARGUMENT, DSFunctionRemove(store, dmHashString64("engine.update.end")));
    DSEventDesc never = { dmHashString64("never") };
    ASSERT_EQ(DS_RESULT_OK, DSEventAdd(store, &never));
    function.m_Name = dmHashString64("heal");
    ASSERT_EQ(DS_RESULT_OK, DSFunctionAdd(store, &function));
    DSEventDesc duplicate = { dmHashString64("heal") };
    ASSERT_EQ(DS_RESULT_ALREADY_EXISTS, DSEventAdd(store, &duplicate));
    ASSERT_EQ(DS_RESULT_INVALID_ARGUMENT, DSEventRemove(store, dmHashString64("heal")));
    ASSERT_EQ(DS_RESULT_INVALID_ARGUMENT, DSSchedulerEvent(store, begin.m_Name));
    ASSERT_EQ(DS_RESULT_OK, DSSchedulerBegin(store, jobs));
    ASSERT_EQ(DS_RESULT_LOCKED, DSEventAdd(store, &begin));
    ASSERT_EQ(DS_RESULT_LOCKED, DSEventRemove(store, begin.m_Name));
    ASSERT_EQ(DS_RESULT_NOT_FOUND, DSSchedulerEvent(store, dmHashString64("missing")));
    ASSERT_EQ(DS_RESULT_INVALID_ARGUMENT, DSSchedulerEvent(store, dmHashString64("heal")));
    ASSERT_EQ(DS_RESULT_BUSY, DSSchedulerEvent(store, dmHashString64("engine.update.end")));
    ASSERT_EQ(DS_RESULT_OK, DSSchedulerEvent(store, begin.m_Name));
    ASSERT_EQ(DS_RESULT_INVALID_ARGUMENT, DSSchedulerEvent(store, begin.m_Name));
    ASSERT_EQ(DS_RESULT_OK, DSSchedulerEvent(store, dmHashString64("engine.update.end")));
    ASSERT_EQ(DS_RESULT_OK, DSSchedulerEnd(store));

    ASSERT_EQ(DS_RESULT_OK, DSSchedulerBegin(store, jobs));
    ASSERT_EQ(DS_RESULT_BUSY, DSSchedulerEvent(store, dmHashString64("engine.update.end")));
    ASSERT_EQ(DS_RESULT_OK, DSSchedulerEnd(store));
    ASSERT_EQ(DS_RESULT_OK, DSEventRemove(store, begin.m_Name));
    ASSERT_EQ(DS_RESULT_NOT_FOUND, DSEventRemove(store, begin.m_Name));
    ASSERT_EQ(DS_RESULT_NOT_FOUND, DSSchedulerBegin(store, jobs));
    ASSERT_EQ(DS_RESULT_OK, DSEventAdd(store, &begin));
    ASSERT_EQ(DS_RESULT_OK, DSSchedulerBegin(store, jobs));
    ASSERT_EQ(DS_RESULT_OK, DSSchedulerEvent(store, begin.m_Name));
    ASSERT_EQ(DS_RESULT_OK, DSSchedulerEvent(store, dmHashString64("engine.update.end")));
    ASSERT_EQ(DS_RESULT_OK, DSSchedulerEnd(store));
    ASSERT_EQ(DS_RESULT_OK, DSQueryDestroy(query));
    ASSERT_EQ(DS_RESULT_OK, DSStoreDestroy(store));
    JobSystemDestroy(jobs);
}

// An event with all dependency slots populated must wait for the last signal.
// Events never appear as callbacks or keep an otherwise finished function list busy.
TEST(DataScheduler, EventFullDependencyArray)
{
    HJobContext jobs = CreateScheduledJobs();
    HDSContext  store = DSStoreCreate(0);
    dmhash_t    names[DS_SYSTEM_MAX_DEPENDENCIES];
    char        name[32];
    DSEventDesc end = { dmHashString64("engine.update.end") };
    for (uint32_t i = 0; i < DS_SYSTEM_MAX_DEPENDENCIES; ++i)
    {
        snprintf(name, sizeof(name), "engine.update.%u", i);
        names[i] = dmHashString64(name);
        end.m_DependsOn[i] = names[i];
    }
    ASSERT_EQ(DS_RESULT_OK, DSEventAdd(store, &end));
    for (uint32_t i = 0; i < DS_SYSTEM_MAX_DEPENDENCIES; ++i)
    {
        DSEventDesc event = { names[i] };
        ASSERT_EQ(DS_RESULT_OK, DSEventAdd(store, &event));
    }
    ASSERT_EQ(DS_RESULT_OK, DSSchedulerBegin(store, jobs));
    ASSERT_EQ(DS_RESULT_OK, DSSchedulerUpdate(store));
    for (uint32_t i = 0; i < DS_SYSTEM_MAX_DEPENDENCIES; ++i)
    {
        ASSERT_EQ(DS_RESULT_BUSY, DSSchedulerEvent(store, end.m_Name));
        ASSERT_EQ(DS_RESULT_OK, DSSchedulerEvent(store, names[i]));
    }
    ASSERT_EQ(DS_RESULT_OK, DSSchedulerEvent(store, end.m_Name));
    ASSERT_EQ(DS_RESULT_OK, DSSchedulerEnd(store));
    ASSERT_EQ(DS_RESULT_OK, DSStoreDestroy(store));
    JobSystemDestroy(jobs);
}

// Hashes differing only in their upper bits must identify distinct events and
// dependencies, even without reverse-hash entries for their names.
TEST(DataScheduler, FullWidthHashIdentity)
{
    HDSContext       store = DSStoreCreate(0);
    HJobContext      jobs = CreateScheduledJobs();
    DSQueryDesc      query_desc = { 0 };
    HDSQuery         query = CreateScheduledQuery(store, query_desc);
    ScheduledContext context = { 0 };
    DSEventDesc      first = { UINT64_C(0x100000007) };
    DSEventDesc      second = { UINT64_C(0x200000007) };
    DSSystemDesc     function = {
        UINT64_C(0x300000007),
        { second.m_Name },
        query,
        ReadHealthRows,
        &context
    };
    ASSERT_EQ(DS_RESULT_OK, DSFunctionAdd(store, &function));
    ASSERT_EQ(DS_RESULT_OK, DSEventAdd(store, &first));
    ASSERT_EQ(DS_RESULT_OK, DSEventAdd(store, &second));
    ASSERT_EQ(DS_RESULT_ALREADY_EXISTS, DSEventAdd(store, &second));
    ASSERT_EQ(DS_RESULT_OK, DSSchedulerBegin(store, jobs));
    ASSERT_EQ(DS_RESULT_OK, DSSchedulerEvent(store, first.m_Name));
    ASSERT_EQ(DS_RESULT_BUSY, DSSchedulerUpdate(store));
    ASSERT_EQ(0U, context.m_Calls);
    ASSERT_EQ(DS_RESULT_OK, DSSchedulerEvent(store, second.m_Name));
    FinishSchedule(store, 1);
    ASSERT_EQ(1U, context.m_Calls);
    ASSERT_EQ(DS_RESULT_OK, DSEventRemove(store, first.m_Name));
    ASSERT_EQ(DS_RESULT_OK, DSEventRemove(store, second.m_Name));
    ASSERT_EQ(DS_RESULT_OK, DSFunctionRemove(store, function.m_Name));
    ASSERT_EQ(DS_RESULT_OK, DSQueryDestroy(query));
    ASSERT_EQ(DS_RESULT_OK, DSStoreDestroy(store));
    JobSystemDestroy(jobs);
}

// The public scheduler declarations and functions must compile and link from C.
TEST(DataScheduler, PublicRegistrationFromC)
{
    ASSERT_EQ(0, TestDataSchedulerFromC());
}
