// Copyright 2026 The Defold Foundation
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
#include <dlib/thread.h>
#include <dlib/condition_variable.h>
#include <dlib/jobsystem.h>
#include <dlib/time.h>
#include "../data.h"

#ifndef DM_SANITIZE_THREAD
#error Threading tests must be built and run with WITH_TSAN=ON
#endif

// Deliberately different offsets in the two types, with nested fields sharing a row.
static const uint64_t HEALTH = 1, POSITION = 2, LIGHT = 3, COLOR = 4, INTENSITY = 5;

struct ThreadFixture
{
    HDataStore m_Store;
    DataId     m_Ids[12];
};

static ThreadFixture CreateFixture()
{
    ThreadFixture fixture = {};
    fixture.m_Store = DataCreateStore();
    DataFieldDesc       light_fields[] = { { COLOR, DATA_VALUE_TYPE_VECTOR3, 0, 0 }, { INTENSITY, DATA_VALUE_TYPE_NUMBER, 16, 0 } };
    DataStructDesc      light = { light_fields, 2, 24 };
    DataFieldDesc       fields[] = { { HEALTH, DATA_VALUE_TYPE_NUMBER, 0, 0 }, { POSITION, DATA_VALUE_TYPE_VECTOR3, 8, 0 }, { LIGHT, DATA_VALUE_TYPE_STRUCT, 24, &light } };
    const DataValueType child_types[] = { DATA_VALUE_TYPE_VECTOR3, DATA_VALUE_TYPE_NUMBER };
    DataValueData       children[2] = {};
    uint64_t            names[] = { COLOR, INTENSITY };
    const DataValueType types[] = { DATA_VALUE_TYPE_NUMBER, DATA_VALUE_TYPE_VECTOR3, DATA_VALUE_TYPE_STRUCT };
    DataValueData       values[3] = {};
    values[0].m_Number = 50;
    values[2].m_Struct.m_Names = names;
    values[2].m_Struct.m_Types = child_types;
    values[2].m_Struct.m_Values = children;
    values[2].m_Struct.m_Count = 2;
    for (uint32_t t = 0; t < 2; ++t)
    {
        fields[0].m_Offset = t ? 16 : 0;
        fields[1].m_Offset = t ? 0 : 8;
        DataTableDesc table = { 100 + t, 0, 0, fields, 3, 48 };
        EXPECT_EQ(DATA_RESULT_OK, DataRegisterTable(fixture.m_Store, &table));
        DataRowDesc rows[6] = {};
        for (uint32_t r = 0; r < 6; ++r)
        {
            rows[r].m_Owner = r % 2;
            rows[r].m_Types = types;
            rows[r].m_Values = values;
            rows[r].m_ValueCount = 3;
        }
        EXPECT_EQ(DATA_RESULT_OK, DataAddRows(fixture.m_Store, table.m_Type, rows, 6, fixture.m_Ids + t * 6));
    }
    return fixture;
}

static HDataQuery Query(HDataStore store, uint64_t field_hash, DataValueType type, DataAccess access, const uint64_t* path = 0)
{
    DataQueryField field = { field_hash, type, path, path ? 1u : 0u, access };
    DataQueryDesc  desc = { 0, 0, 0, 0, &field, 1 };
    HDataQuery     query = 0;
    EXPECT_EQ(DATA_RESULT_OK, DataCreateQuery(store, &desc, &query));
    return query;
}

struct Admission
{
    HDataQuery m_Query;
    DataResult m_Result;
};

static void TryOnThread(void* arg)
{
    Admission* attempt = (Admission*)arg;
    attempt->m_Result = DataQueryTryBegin(attempt->m_Query);
    if (attempt->m_Result == DATA_RESULT_OK)
        DataQueryEnd(attempt->m_Query);
}

static DataResult TryFromWorker(HDataQuery query)
{
    Admission        attempt = { query, DATA_RESULT_INVALID_ARGUMENT };
    dmThread::Thread thread = dmThread::New(TryOnThread, 0x80000, &attempt, "data-admission");
    dmThread::Join(thread);
    return attempt.m_Result;
}

TEST(DataThreaded, ConflictsIndependentFieldsAndRelease)
{
    ThreadFixture fixture = CreateFixture();
    HDataQuery    explosion = Query(fixture.m_Store, HEALTH, DATA_VALUE_TYPE_NUMBER, DATA_ACCESS_READ_WRITE);
    HDataQuery    regenerate = Query(fixture.m_Store, HEALTH, DATA_VALUE_TYPE_NUMBER, DATA_ACCESS_READ_WRITE);
    HDataQuery    reader = Query(fixture.m_Store, HEALTH, DATA_VALUE_TYPE_NUMBER, DATA_ACCESS_READ);
    HDataQuery    movement = Query(fixture.m_Store, POSITION, DATA_VALUE_TYPE_VECTOR3, DATA_ACCESS_READ_WRITE);
    HDataQuery    queries[] = { explosion, regenerate, reader, movement };
    for (uint32_t first = 0; first < 2; ++first)
    {
        ASSERT_EQ(DATA_RESULT_OK, DataQueryTryBegin(queries[first]));
        ASSERT_EQ(DATA_RESULT_BUSY, TryFromWorker(queries[1 - first]));
        ASSERT_EQ(DATA_RESULT_BUSY, TryFromWorker(reader));
        ASSERT_EQ(DATA_RESULT_OK, TryFromWorker(movement));
        ASSERT_EQ(DATA_RESULT_LOCKED, DataRemoveRow(fixture.m_Store, fixture.m_Ids[0]));
        ASSERT_EQ(DATA_RESULT_LOCKED, DataResetRow(fixture.m_Store, fixture.m_Ids[0]));
        ASSERT_EQ(DATA_RESULT_LOCKED, DataSetFieldNumber(fixture.m_Store, fixture.m_Ids[0], HEALTH, 1));
        ASSERT_EQ(DATA_RESULT_LOCKED, DataDestroyQuery(regenerate));
        ASSERT_EQ(DATA_RESULT_BUSY, DataQueryTryBegin(queries[first]));
        DataQueryEnd(queries[first]);
        ASSERT_EQ(DATA_RESULT_OK, TryFromWorker(queries[1 - first]));
    }
    ASSERT_EQ(DATA_RESULT_OK, DataQueryTryBegin(reader));
    ASSERT_EQ(DATA_RESULT_BUSY, TryFromWorker(reader));
    DataQueryField writable = { HEALTH, DATA_VALUE_TYPE_NUMBER, 0, 0, DATA_ACCESS_READ_WRITE };
    ASSERT_EQ(UINT32_MAX, DataQueryFindField(reader, &writable));
    DataIterator it = DataQueryIterRange(reader, 0, 1);
    ASSERT_EQ(DATA_RESULT_OK, DataIterNext(&it));
    DataRowIterator row = DataIterRows(&it);
    ASSERT_EQ(DATA_RESULT_OK, DataRowIterNext(&row));
    DataFieldIterator field = DataRowIterFields(&row);
    ASSERT_EQ(DATA_RESULT_OK, DataFieldIterNext(&field));
    ASSERT_EQ(DATA_RESULT_INVALID_ARGUMENT, DataFieldIterSetNumber(&field, 1));
    DataQueryEnd(reader);
    DataStoreLock(fixture.m_Store);
    ASSERT_EQ(DATA_RESULT_BUSY, TryFromWorker(reader));
    DataStoreUnlock(fixture.m_Store);
    for (uint32_t q = 0; q < 4; ++q)
        ASSERT_EQ(DATA_RESULT_OK, DataDestroyQuery(queries[q]));
    ASSERT_EQ(DATA_RESULT_OK, DataDestroyStore(fixture.m_Store));
}

TEST(DataThreaded, ReadOnlyAndEmptyQueryReservations)
{
    ThreadFixture fixture = CreateFixture();
    HDataQuery    queries[] = {
        Query(fixture.m_Store, HEALTH, DATA_VALUE_TYPE_NUMBER, DATA_ACCESS_READ),
        Query(fixture.m_Store, HEALTH, DATA_VALUE_TYPE_NUMBER, DATA_ACCESS_READ),
        Query(fixture.m_Store, 999, DATA_VALUE_TYPE_NUMBER, DATA_ACCESS_READ)
    };
    for (uint32_t i = 0; i < 3; ++i)
    {
        ASSERT_EQ(DATA_RESULT_OK, DataQueryTryBegin(queries[i]));
        ASSERT_EQ(DATA_RESULT_BUSY, DataQueryTryBegin(queries[i]));
        ASSERT_EQ(DATA_RESULT_BUSY, TryFromWorker(queries[i]));
        ASSERT_EQ(i == 2 ? 0u : 12u, DataQueryGetRowCount(queries[i]));
    }
    DataIterator empty = DataQueryIterRange(queries[2], 0, 0);
    ASSERT_EQ(DATA_RESULT_END, DataIterNext(&empty));

    // Release out of order, retaining other queries' reservations as the pool compacts.
    uint32_t order[] = { 1, 0, 2 };
    for (uint32_t i = 0; i < 3; ++i)
    {
        ASSERT_EQ(DATA_RESULT_LOCKED, DataRemoveRow(fixture.m_Store, fixture.m_Ids[0]));
        DataQueryEnd(queries[order[i]]);
        ASSERT_EQ(DATA_RESULT_OK, TryFromWorker(queries[order[i]]));
    }
    ASSERT_EQ(DATA_RESULT_OK, DataRemoveRow(fixture.m_Store, fixture.m_Ids[0]));
    for (uint32_t i = 0; i < 3; ++i)
        ASSERT_EQ(DATA_RESULT_OK, DataDestroyQuery(queries[i]));
    ASSERT_EQ(DATA_RESULT_OK, DataDestroyStore(fixture.m_Store));
}

TEST(DataThreaded, ActiveQueryPoolGrowthAndReuse)
{
    ThreadFixture fixture = CreateFixture();
    HDataQuery    readers[40];
    for (uint32_t i = 0; i < 40; ++i)
        readers[i] = Query(fixture.m_Store, HEALTH, DATA_VALUE_TYPE_NUMBER, DATA_ACCESS_READ);
    HDataQuery writer = Query(fixture.m_Store, HEALTH, DATA_VALUE_TYPE_NUMBER, DATA_ACCESS_READ_WRITE);

    for (uint32_t pass = 0; pass < 2; ++pass)
    {
        for (uint32_t i = 0; i < 40; ++i)
        {
            ASSERT_EQ(DATA_RESULT_OK, DataQueryTryBegin(readers[i]));
            ASSERT_EQ(12u, DataQueryGetRowCount(readers[i]));
        }
        // End in a different order from admission, moving live reservations in
        // the dense pool. Reuse each freed slot while the others remain active.
        for (uint32_t i = 0; i < 40; ++i)
        {
            HDataQuery reader = readers[(i * 17) % 40];
            ASSERT_EQ(DATA_RESULT_BUSY, DataQueryTryBegin(writer));
            ASSERT_EQ(DATA_RESULT_BUSY, DataQueryTryBegin(reader));
            DataQueryEnd(reader);
            ASSERT_EQ(DATA_RESULT_OK, DataQueryTryBegin(reader));
            DataQueryEnd(reader);
        }
        ASSERT_EQ(DATA_RESULT_OK, TryFromWorker(writer));
    }
    for (uint32_t i = 0; i < 40; ++i)
        ASSERT_EQ(DATA_RESULT_OK, DataDestroyQuery(readers[i]));
    ASSERT_EQ(DATA_RESULT_OK, DataDestroyQuery(writer));
    ASSERT_EQ(DATA_RESULT_OK, DataDestroyStore(fixture.m_Store));
}

TEST(DataThreaded, NestedRangesAndWholeRows)
{
    ThreadFixture fixture = CreateFixture();
    uint64_t      color = COLOR, intensity = INTENSITY;
    HDataQuery    parent = Query(fixture.m_Store, LIGHT, DATA_VALUE_TYPE_STRUCT, DATA_ACCESS_READ);
    HDataQuery    child = Query(fixture.m_Store, LIGHT, DATA_VALUE_TYPE_VECTOR3, DATA_ACCESS_READ_WRITE, &color);
    HDataQuery    sibling = Query(fixture.m_Store, LIGHT, DATA_VALUE_TYPE_NUMBER, DATA_ACCESS_READ_WRITE, &intensity);
    HDataQuery    whole;
    DataQueryDesc desc = {};
    ASSERT_EQ(DATA_RESULT_OK, DataCreateQuery(fixture.m_Store, &desc, &whole));
    ASSERT_EQ(DATA_RESULT_OK, DataQueryTryBegin(child));
    ASSERT_EQ(DATA_RESULT_BUSY, TryFromWorker(parent));
    ASSERT_EQ(DATA_RESULT_BUSY, TryFromWorker(whole));
    ASSERT_EQ(DATA_RESULT_OK, TryFromWorker(sibling));
    DataQueryEnd(child);
    HDataQuery queries[] = { parent, child, sibling, whole };
    for (uint32_t q = 0; q < 4; ++q)
        ASSERT_EQ(DATA_RESULT_OK, DataDestroyQuery(queries[q]));
    ASSERT_EQ(DATA_RESULT_OK, DataDestroyStore(fixture.m_Store));
}

TEST(DataThreaded, RangesCoverOwnerFilteredRowsAndRefreshAfterMutation)
{
    ThreadFixture  fixture = CreateFixture();
    DataOwnerId    owner = 1;
    DataQueryField field = { HEALTH, DATA_VALUE_TYPE_NUMBER };
    DataQueryDesc  desc = { &owner, 1, 0, 0, &field, 1 };
    HDataQuery     query;
    ASSERT_EQ(DATA_RESULT_OK, DataCreateQuery(fixture.m_Store, &desc, &query));
    for (uint32_t phase = 0; phase < 2; ++phase)
    {
        ASSERT_EQ(DATA_RESULT_OK, DataQueryTryBegin(query));
        uint32_t count = DataQueryGetRowCount(query);
        ASSERT_EQ(6u - phase, count);
        bool seen[12] = {};
        for (uint32_t first = 0; first < count; first += 2)
        {
            DataIterator it = DataQueryIterRange(query, first, count - first < 2 ? count - first : 2);
            while (DataIterNext(&it) == DATA_RESULT_OK)
            {
                DataRowIterator rows = DataIterRows(&it);
                while (DataRowIterNext(&rows) == DATA_RESULT_OK)
                {
                    ASSERT_EQ(owner, DataRowIterGetOwnerId(&rows));
                    DataId   id = DataRowIterGetId(&rows);
                    uint32_t index = 0;
                    while (index < 12 && fixture.m_Ids[index] != id)
                        ++index;
                    ASSERT_LT(index, 12u);
                    ASSERT_FALSE(seen[index]);
                    seen[index] = true;
                }
            }
        }
        uint32_t visited = 0;
        for (uint32_t i = 0; i < 12; ++i)
            visited += seen[i];
        ASSERT_EQ(count, visited);
        DataIterator empty = DataQueryIterRange(query, count, 0);
        ASSERT_EQ(DATA_RESULT_END, DataIterNext(&empty));
        DataQueryEnd(query);
        if (!phase)
            ASSERT_EQ(DATA_RESULT_OK, DataRemoveRow(fixture.m_Store, fixture.m_Ids[1]));
    }
    ASSERT_EQ(DATA_RESULT_OK, DataDestroyQuery(query));
    ASSERT_EQ(DATA_RESULT_OK, DataDestroyStore(fixture.m_Store));
}

struct MutationAttempt
{
    HDataStore m_Store;
    DataId     m_Id;
    DataResult m_Remove, m_CreateQuery, m_DestroyStore;
};

static void MutateOnThread(void* context)
{
    MutationAttempt* attempt = (MutationAttempt*)context;
    attempt->m_Remove = DataRemoveRow(attempt->m_Store, attempt->m_Id);
    HDataQuery    query = 0;
    DataQueryDesc desc = {};
    attempt->m_CreateQuery = DataCreateQuery(attempt->m_Store, &desc, &query);
    attempt->m_DestroyStore = DataDestroyStore(attempt->m_Store);
}

TEST(DataThreaded, MutationExclusionAndConservativeOwnerReservations)
{
    ThreadFixture  fixture = CreateFixture();
    DataQueryField field = { HEALTH, DATA_VALUE_TYPE_NUMBER, 0, 0, DATA_ACCESS_READ_WRITE };
    DataOwnerId    owners[] = { 0, 1 };
    HDataQuery     queries[2];
    for (uint32_t i = 0; i < 2; ++i)
    {
        DataQueryDesc desc = { &owners[i], 1, 0, 0, &field, 1 };
        ASSERT_EQ(DATA_RESULT_OK, DataCreateQuery(fixture.m_Store, &desc, &queries[i]));
    }
    ASSERT_EQ(DATA_RESULT_OK, DataQueryTryBegin(queries[0]));
    ASSERT_EQ(DATA_RESULT_BUSY, TryFromWorker(queries[1]));
    MutationAttempt  attempt = { fixture.m_Store, fixture.m_Ids[0] };
    dmThread::Thread worker = dmThread::New(MutateOnThread, 0x80000, &attempt, "data-mutate");
    dmThread::Join(worker);
    ASSERT_EQ(DATA_RESULT_LOCKED, attempt.m_Remove);
    ASSERT_EQ(DATA_RESULT_LOCKED, attempt.m_CreateQuery);
    ASSERT_EQ(DATA_RESULT_LOCKED, attempt.m_DestroyStore);
    DataQueryEnd(queries[0]);
    ASSERT_EQ(DATA_RESULT_OK, DataRemoveRow(fixture.m_Store, fixture.m_Ids[0]));
    for (uint32_t i = 0; i < 2; ++i)
        ASSERT_EQ(DATA_RESULT_OK, DataDestroyQuery(queries[i]));
    DataQueryField invalid = { LIGHT, DATA_VALUE_TYPE_STRUCT, 0, 0, DATA_ACCESS_READ_WRITE };
    DataQueryDesc  desc = { 0, 0, 0, 0, &invalid, 1 };
    HDataQuery     query = 0;
    ASSERT_EQ(DATA_RESULT_INVALID_ARGUMENT, DataCreateQuery(fixture.m_Store, &desc, &query));
    ASSERT_EQ(DATA_RESULT_OK, DataDestroyStore(fixture.m_Store));
}

struct AdmissionRace
{
    dmMutex::HMutex                         m_Mutex;
    dmConditionVariable::HConditionVariable m_Condition;
    HDataQuery                              m_Query;
    uint32_t                                m_Ready, m_Attempted;
    bool                                    m_Start;
};

struct AdmissionWorker
{
    AdmissionRace* m_Race;
    DataResult     m_Result;
};

static void RaceAdmission(void* context)
{
    AdmissionWorker* worker = (AdmissionWorker*)context;
    AdmissionRace*   race = worker->m_Race;
    {
        DM_MUTEX_SCOPED_LOCK(race->m_Mutex);
        ++race->m_Ready;
        dmConditionVariable::Broadcast(race->m_Condition);
        while (!race->m_Start)
            dmConditionVariable::Wait(race->m_Condition, race->m_Mutex);
    }
    worker->m_Result = DataQueryTryBegin(race->m_Query);
    {
        DM_MUTEX_SCOPED_LOCK(race->m_Mutex);
        ++race->m_Attempted;
        dmConditionVariable::Broadcast(race->m_Condition);
        // The successful begin retains access until both callers have attempted.
        while (race->m_Attempted != 2)
            dmConditionVariable::Wait(race->m_Condition, race->m_Mutex);
    }
    if (worker->m_Result == DATA_RESULT_OK)
        DataQueryEnd(race->m_Query);
}

TEST(DataThreaded, SimultaneousBeginsAdmitExactlyOne)
{
    ThreadFixture fixture = CreateFixture();
    HDataQuery    queries[] = {
        Query(fixture.m_Store, HEALTH, DATA_VALUE_TYPE_NUMBER, DATA_ACCESS_READ_WRITE),
        Query(fixture.m_Store, HEALTH, DATA_VALUE_TYPE_NUMBER, DATA_ACCESS_READ)
    };
    for (uint32_t pass = 0; pass < 32; ++pass)
    {
        AdmissionRace race = {};
        race.m_Mutex = dmMutex::New();
        race.m_Condition = dmConditionVariable::New();
        race.m_Query = queries[pass % 2];
        AdmissionWorker  workers[] = { { &race, DATA_RESULT_INVALID_ARGUMENT }, { &race, DATA_RESULT_INVALID_ARGUMENT } };
        dmThread::Thread threads[2];
        for (uint32_t i = 0; i < 2; ++i)
            threads[i] = dmThread::New(RaceAdmission, 0x80000, &workers[i], "data-race");
        {
            DM_MUTEX_SCOPED_LOCK(race.m_Mutex);
            while (race.m_Ready != 2)
                dmConditionVariable::Wait(race.m_Condition, race.m_Mutex);
            race.m_Start = true;
            dmConditionVariable::Broadcast(race.m_Condition);
        }
        for (uint32_t i = 0; i < 2; ++i)
            dmThread::Join(threads[i]);
        ASSERT_TRUE((workers[0].m_Result == DATA_RESULT_OK && workers[1].m_Result == DATA_RESULT_BUSY) ||
                    (workers[1].m_Result == DATA_RESULT_OK && workers[0].m_Result == DATA_RESULT_BUSY));
        dmConditionVariable::Delete(race.m_Condition);
        dmMutex::Delete(race.m_Mutex);
    }
    for (uint32_t i = 0; i < 2; ++i)
    {
        ASSERT_EQ(DATA_RESULT_OK, TryFromWorker(queries[i]));
        ASSERT_EQ(DATA_RESULT_OK, DataDestroyQuery(queries[i]));
    }
    ASSERT_EQ(DATA_RESULT_OK, DataDestroyStore(fixture.m_Store));
}

struct RangeWork
{
    HDataQuery m_Query;
    uint32_t   m_First, m_Count, m_Field;
    uint32_t   m_Visited;
};

static int32_t WriteRange(HJobContext, HJob, void*, void* data)
{
    RangeWork*   work = (RangeWork*)data;
    DataIterator it = DataQueryIterRange(work->m_Query, work->m_First, work->m_Count);
    while (DataIterNext(&it) == DATA_RESULT_OK)
    {
        DataRowIterator rows = DataIterRows(&it);
        while (DataRowIterNext(&rows) == DATA_RESULT_OK)
        {
            *DataRowIterGetNumberMut(&rows, work->m_Field) += 1;
            ++work->m_Visited;
        }
    }
    return 0;
}

static void RangeDone(HJobContext, HJob, JobSystemStatus status, void* context, void*, int32_t result)
{
    ASSERT_EQ(JOBSYSTEM_STATUS_FINISHED, status);
    ASSERT_EQ(0, result);
    ++*(uint32_t*)context;
}

TEST(DataThreaded, JobRangesWithOneTwoFourEightWorkers)
{
    for (uint32_t workers = 1; workers <= 8; workers *= 2)
    {
        ThreadFixture         fixture = CreateFixture();
        HDataQuery            query = Query(fixture.m_Store, HEALTH, DATA_VALUE_TYPE_NUMBER, DATA_ACCESS_READ_WRITE);
        DataQueryField        requested_field = { .m_Field = HEALTH, .m_Type = DATA_VALUE_TYPE_NUMBER, .m_Access = DATA_ACCESS_READ_WRITE };
        uint32_t              field = DataQueryFindField(query, &requested_field);
        JobSystemCreateParams params = { "data-test", (uint8_t)workers };
        HJobContext           jobs = JobSystemCreate(&params);
        for (uint32_t pass = 0; pass < 20; ++pass)
        {
            ASSERT_EQ(DATA_RESULT_OK, DataQueryTryBegin(query));
            RangeWork ranges[4] = {};
            uint32_t  completed = 0;
            for (uint32_t j = 0; j < 4; ++j)
            {
                ranges[j].m_Query = query;
                ranges[j].m_First = j * 3;
                ranges[j].m_Count = 3;
                ranges[j].m_Field = field;
                Job  job = { WriteRange, RangeDone, &completed, &ranges[j] };
                HJob handle = JobSystemCreateJob(jobs, &job);
                ASSERT_NE(0u, handle);
                ASSERT_EQ(JOBSYSTEM_RESULT_OK, JobSystemPushJob(jobs, handle));
            }
            while (completed != 4)
                JobSystemUpdate(jobs, 0);
            for (uint32_t j = 0; j < 4; ++j)
                ASSERT_EQ(3u, ranges[j].m_Visited);
            DataQueryEnd(query);
        }
        JobSystemDestroy(jobs);
        for (uint32_t i = 0; i < 12; ++i)
        {
            double value;
            ASSERT_EQ(DATA_RESULT_OK, DataFieldGetNumber(fixture.m_Store, fixture.m_Ids[i], HEALTH, &value));
            ASSERT_EQ(70.0, value);
        }
        ASSERT_EQ(DATA_RESULT_OK, DataDestroyQuery(query));
        ASSERT_EQ(DATA_RESULT_OK, DataDestroyStore(fixture.m_Store));
    }
}

int main(int argc, char** argv)
{
    jc_test_init(&argc, argv);
    return jc_test_run_all();
}

struct BatchReader
{
    ThreadFixture* m_Fixture;
    uint32_t       m_Errors;
};

static void ReadBatches(void* context)
{
    BatchReader* reader = (BatchReader*)context;
    DataId       ids[73];
    for (uint32_t i = 0; i < 73; ++i)
        ids[i] = reader->m_Fixture->m_Ids[i % 12];
    for (uint32_t pass = 0; pass < 100; ++pass)
    {
        double      health[73];
        DataVector3 positions[73];
        if (DataFieldGetNumberBatch(reader->m_Fixture->m_Store, 73, ids, HEALTH, health) != DATA_RESULT_OK ||
            DataFieldGetVector3Batch(reader->m_Fixture->m_Store, 73, ids, POSITION, positions) != DATA_RESULT_OK)
        {
            ++reader->m_Errors;
            return;
        }
        for (uint32_t i = 0; i < 73; ++i)
            reader->m_Errors += health[i] != 50 || positions[i].m_Values[0] != 0 || positions[i].m_Values[1] != 0 || positions[i].m_Values[2] != 0;
    }
}

TEST(DataThreaded, ConcurrentBatchReadersWithCallerSynchronization)
{
    ThreadFixture    fixture = CreateFixture();
    BatchReader      readers[4] = {};
    dmThread::Thread threads[4];
    // The caller excludes structural changes and writes until all readers finish.
    DataStoreLock(fixture.m_Store);
    for (uint32_t i = 0; i < 4; ++i)
    {
        readers[i].m_Fixture = &fixture;
        threads[i] = dmThread::New(ReadBatches, 0x80000, &readers[i], "data-batch-read");
    }
    for (uint32_t i = 0; i < 4; ++i)
    {
        dmThread::Join(threads[i]);
        ASSERT_EQ(0u, readers[i].m_Errors);
    }
    DataStoreUnlock(fixture.m_Store);
    ASSERT_EQ(DATA_RESULT_OK, DataDestroyStore(fixture.m_Store));
}
