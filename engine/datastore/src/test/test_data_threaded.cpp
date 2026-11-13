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
#include <dlib/thread.h>
#include <dlib/condition_variable.h>
#include <dlib/jobsystem.h>
#include "test_data_memory.h"
#include <dlib/hash.h>

#ifndef DM_SANITIZE_THREAD
#error Threading tests must be built and run with WITH_TSAN=ON
#endif

// Deliberately different offsets in the two types, with nested fields sharing a row.
static const uint64_t HEALTH = 1, POSITION = 2, LIGHT = 3, COLOR = 4, INTENSITY = 5;

struct ThreadFixture
{
    HDataStoreContext m_Store;
    DataStoreId       m_Ids[12];
    DataStoreId       m_Group;
};

struct StructReadTask
{
    HDataStoreQuery          m_Query;
    uint32_t                 m_First;
    uint32_t                 m_Count;
    DataStoreFieldId         m_Field;
    double                   m_Total;
    DataStoreResult          m_Result;
};

struct BlobLifetimeTask
{
    HDataStoreBlob                                 m_Blob;
    DataStoreResult                                m_Result;
    dmMutex::HMutex                                m_Mutex;
    dmConditionVariable::HConditionVariable        m_Condition;
    uint32_t*                                      m_Ready;
    bool*                                          m_Start;
};

struct ScheduledJobGate
{
    dmMutex::HMutex                         m_Mutex;
    dmConditionVariable::HConditionVariable m_Condition;
    uint32_t                                m_Started;
    bool                                    m_Release;
};

struct ScheduledJobState
{
    ScheduledJobGate*        m_Gate;
    DataStoreFieldId         m_Field;
    int32_atomic_t           m_Calls;
    double                   m_Values[12];
};

struct ScheduledRangeState
{
    ScheduledJobState                       m_Reader;
    dmMutex::HMutex                         m_Mutex;
    dmConditionVariable::HConditionVariable m_Condition;
    uint32_t                                m_Finished;
    bool                                    m_Release;
};

struct SchedulerAdmissionGate
{
    HDataStoreContext       m_Store;
    ScheduledJobGate        m_Gate;
};

static ThreadFixture CreateFixture()
{
    ThreadFixture         fixture = {};
    DataStoreCreateParams params  = { 1024, 256 };
    fixture.m_Store = DataStoreCreate(&params);
    fixture.m_Group = NextTestGroupId();
    DataStoreFieldDesc       light_fields[] = { { COLOR, DATASTORE_TYPE_VECTOR3, 0, 0, "color" }, { INTENSITY, DATASTORE_TYPE_NUMBER, 16, 0, "intensity" } };
    DataStoreStructDesc      light = { light_fields, 2, 24 };
    DataStoreFieldDesc       fields[] = { { HEALTH, DATASTORE_TYPE_NUMBER, 0, 0 }, { POSITION, DATASTORE_TYPE_VECTOR3, 8, 0 }, { LIGHT, DATASTORE_TYPE_STRUCT, 24, &light, "light" } };
    const DataStoreValueType child_types[] = { DATASTORE_TYPE_VECTOR3, DATASTORE_TYPE_NUMBER };
    DataStoreValueData       children[2] = {};
    uint64_t                 names[] = { COLOR, INTENSITY };
    const DataStoreValueType types[] = { DATASTORE_TYPE_NUMBER, DATASTORE_TYPE_VECTOR3, DATASTORE_TYPE_STRUCT };
    DataStoreValueData       values[3] = {};
    values[0].m_Number = 50;
    values[2].m_Struct.m_Array.m_Names = names;
    values[2].m_Struct.m_Array.m_Types = child_types;
    values[2].m_Struct.m_Array.m_Values = children;
    values[2].m_Struct.m_Count = 2;
    for (uint32_t t = 0; t < 2; ++t)
    {
        fields[0].m_Offset = t ? 16 : 0;
        fields[1].m_Offset = t ? 0 : 8;
        DataStoreTableDesc table = { 100 + t, 0, 0, fields, 3, 48 };
        EXPECT_EQ(DATASTORE_RESULT_OK, DataStoreTableAdd(fixture.m_Store, &table));
        DataStoreRowDesc rows[6] = {};
        for (uint32_t r = 0; r < 6; ++r)
        {
            rows[r].m_Group = r % 2 ? fixture.m_Group : 0;
            rows[r].m_Types = types;
            rows[r].m_Values = values;
            rows[r].m_ValueCount = 3;
        }
        EXPECT_EQ(DATASTORE_RESULT_OK, DataStoreRowAddBatch(fixture.m_Store, table.m_Type, rows, 6, fixture.m_Ids + t * 6));
    }
    return fixture;
}

static HDataStoreQuery Query(HDataStoreContext store, uint64_t field_hash, DataStoreValueType type, uint32_t access, uint32_t flags)
{
    DataStoreQueryItem field = { field_hash, type, access, DATASTORE_QUERY_ITEM_FIELD };
    DataStoreQueryDesc desc = { { field }, 0, 0, flags };
    HDataStoreQuery    query = 0;
    EXPECT_EQ(DATASTORE_RESULT_OK, DataStoreQueryCreate(store, &desc, &query));
    return query;
}

struct Admission
{
    HDataStoreQuery m_Query;
    DataStoreResult m_Result;
};

static void TryOnThread(void* arg)
{
    Admission* attempt = (Admission*)arg;
    attempt->m_Result = DataStoreQueryTryBegin(attempt->m_Query);
    if (attempt->m_Result == DATASTORE_RESULT_OK)
    {
        DataStoreQueryEnd(attempt->m_Query);
    }
}

static DataStoreResult TryFromWorker(HDataStoreQuery query)
{
    Admission        attempt = { query, DATASTORE_RESULT_INVALID_ARGUMENT };
    dmThread::Thread thread = dmThread::New(TryOnThread, 0x80000, &attempt, "data-admission");
    dmThread::Join(thread);
    return attempt.m_Result;
}

// Conflicting accesses wait while independent fields proceed and releases unblock work.
TEST(DataThreaded, ConflictsIndependentFieldsAndRelease)
{
    ThreadFixture   fixture = CreateFixture();
    HDataStoreQuery explosion = Query(fixture.m_Store, HEALTH, DATASTORE_TYPE_NUMBER, DATASTORE_FIELD_MUTABLE, 0);
    HDataStoreQuery regenerate = Query(fixture.m_Store, HEALTH, DATASTORE_TYPE_NUMBER, DATASTORE_FIELD_MUTABLE, 0);
    HDataStoreQuery reader = Query(fixture.m_Store, HEALTH, DATASTORE_TYPE_NUMBER, 0, 0);
    HDataStoreQuery movement = Query(fixture.m_Store, POSITION, DATASTORE_TYPE_VECTOR3, DATASTORE_FIELD_MUTABLE, 0);
    HDataStoreQuery queries[] = { explosion, regenerate, reader, movement };
    for (uint32_t first = 0; first < 2; ++first)
    {
        ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreQueryTryBegin(queries[first]));
        ASSERT_EQ(DATASTORE_RESULT_BUSY, TryFromWorker(queries[1 - first]));
        ASSERT_EQ(DATASTORE_RESULT_BUSY, TryFromWorker(reader));
        ASSERT_EQ(DATASTORE_RESULT_OK, TryFromWorker(movement));
        ASSERT_EQ(DATASTORE_RESULT_LOCKED, DataStoreRowRemove(fixture.m_Store, fixture.m_Ids[0]));
        ASSERT_EQ(DATASTORE_RESULT_LOCKED, DataStoreRowReset(fixture.m_Store, fixture.m_Ids[0]));
        ASSERT_EQ(DATASTORE_RESULT_LOCKED, DataStoreFieldSetNumber(fixture.m_Store, fixture.m_Ids[0], HEALTH, 1));
        ASSERT_EQ(DATASTORE_RESULT_LOCKED, DataStoreQueryDestroy(regenerate));
        ASSERT_EQ(DATASTORE_RESULT_BUSY, DataStoreQueryTryBegin(queries[first]));
        DataStoreQueryEnd(queries[first]);
        ASSERT_EQ(DATASTORE_RESULT_OK, TryFromWorker(queries[1 - first]));
    }
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreQueryTryBegin(reader));
    ASSERT_EQ(DATASTORE_RESULT_BUSY, TryFromWorker(reader));
    DataStoreQueryItem writable = { HEALTH, DATASTORE_TYPE_NUMBER, DATASTORE_FIELD_MUTABLE, DATASTORE_QUERY_ITEM_FIELD };
    ASSERT_EQ(UINT32_MAX, DataStoreQueryFindField(reader, &writable));
    DataStoreIterator it = DataStoreQueryIterRange(reader, 0, 1);
    ASSERT_TRUE(DataStoreIterNext(&it));
    DataStoreRowIterator row = DataStoreIterRows(&it);
    ASSERT_TRUE(DataStoreRowIterNext(&row));
    DataStoreQueryItem readable = { HEALTH, DATASTORE_TYPE_NUMBER, 0, DATASTORE_QUERY_ITEM_FIELD };
    DataStoreFieldId   health = DataStoreQueryFindField(reader, &readable);
    ASSERT_EQ(50.0, *DataStoreRowIterGetNumber(&row, health));
    DataStoreQueryEnd(reader);
    for (uint32_t q = 0; q < 4; ++q)
    {
        ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreQueryDestroy(queries[q]));
    }
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreDestroy(fixture.m_Store));
}

// A worker must observe whole-row reservations even when its field is omitted
// from the other query's filter. Disjoint fields remain admissible across threads.
TEST(DataThreaded, WholeRowReservations)
{
    ThreadFixture   fixture       = CreateFixture();
    HDataStoreQuery position_read = Query(fixture.m_Store, POSITION, DATASTORE_TYPE_VECTOR3, 0, 0);
    HDataStoreQuery row_write     = Query(fixture.m_Store, POSITION, DATASTORE_TYPE_VECTOR3, 0, DATASTORE_ROW_MUTABLE);
    HDataStoreQuery health_read   = Query(fixture.m_Store, HEALTH, DATASTORE_TYPE_NUMBER, 0, 0);
    HDataStoreQuery health_write  = Query(fixture.m_Store, HEALTH, DATASTORE_TYPE_NUMBER, DATASTORE_FIELD_MUTABLE, 0);
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreQueryTryBegin(position_read));
    ASSERT_EQ(DATASTORE_RESULT_OK, TryFromWorker(health_read));
    ASSERT_EQ(DATASTORE_RESULT_OK, TryFromWorker(health_write));
    ASSERT_EQ(DATASTORE_RESULT_BUSY, TryFromWorker(row_write));
    DataStoreQueryEnd(position_read);
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreQueryTryBegin(row_write));
    ASSERT_EQ(DATASTORE_RESULT_BUSY, TryFromWorker(health_read));
    ASSERT_EQ(DATASTORE_RESULT_BUSY, TryFromWorker(health_write));
    ASSERT_EQ(DATASTORE_RESULT_BUSY, TryFromWorker(position_read));
    DataStoreQueryEnd(row_write);
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreQueryTryBegin(health_write));
    ASSERT_EQ(DATASTORE_RESULT_OK, TryFromWorker(position_read));
    ASSERT_EQ(DATASTORE_RESULT_BUSY, TryFromWorker(row_write));
    DataStoreQueryEnd(health_write);
    ASSERT_EQ(DATASTORE_RESULT_OK, TryFromWorker(row_write));
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreQueryDestroy(position_read));
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreQueryDestroy(row_write));
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreQueryDestroy(health_read));
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreQueryDestroy(health_write));
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreDestroy(fixture.m_Store));
}

// Readers can overlap; even empty queries cannot reserve the same handle twice.
TEST(DataThreaded, ReadOnlyAndEmptyQueryReservations)
{
    ThreadFixture   fixture = CreateFixture();
    HDataStoreQuery queries[] = {
        Query(fixture.m_Store, HEALTH, DATASTORE_TYPE_NUMBER, 0, 0),
        Query(fixture.m_Store, HEALTH, DATASTORE_TYPE_NUMBER, 0, 0),
        Query(fixture.m_Store, 999, DATASTORE_TYPE_NUMBER, 0, 0)
    };
    for (uint32_t i = 0; i < 3; ++i)
    {
        ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreQueryTryBegin(queries[i]));
        ASSERT_EQ(DATASTORE_RESULT_BUSY, DataStoreQueryTryBegin(queries[i]));
        ASSERT_EQ(DATASTORE_RESULT_BUSY, TryFromWorker(queries[i]));
        ASSERT_EQ(i == 2 ? 0u : 12u, DataStoreQueryGetRowCount(queries[i]));
    }
    DataStoreIterator empty = DataStoreQueryIterRange(queries[2], 0, 0);
    ASSERT_FALSE(DataStoreIterNext(&empty));

    // Release out of order, retaining other queries' reservations as the pool compacts.
    uint32_t order[] = { 1, 0, 2 };
    for (uint32_t i = 0; i < 3; ++i)
    {
        ASSERT_EQ(DATASTORE_RESULT_LOCKED, DataStoreRowRemove(fixture.m_Store, fixture.m_Ids[0]));
        DataStoreQueryEnd(queries[order[i]]);
        ASSERT_EQ(DATASTORE_RESULT_OK, TryFromWorker(queries[order[i]]));
    }
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreRowRemove(fixture.m_Store, fixture.m_Ids[0]));
    for (uint32_t i = 0; i < 3; ++i)
    {
        ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreQueryDestroy(queries[i]));
    }
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreDestroy(fixture.m_Store));
}

// Active-query pool growth and compaction preserve conflicts while freed entries are reused.
TEST(DataThreaded, ActiveQueryPoolGrowthAndReuse)
{
    ThreadFixture   fixture = CreateFixture();
    HDataStoreQuery readers[40];
    for (uint32_t i = 0; i < 40; ++i)
    {
        readers[i] = Query(fixture.m_Store, HEALTH, DATASTORE_TYPE_NUMBER, 0, 0);
    }
    HDataStoreQuery writer = Query(fixture.m_Store, HEALTH, DATASTORE_TYPE_NUMBER, DATASTORE_FIELD_MUTABLE, 0);

    for (uint32_t pass = 0; pass < 2; ++pass)
    {
        for (uint32_t i = 0; i < 40; ++i)
        {
            ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreQueryTryBegin(readers[i]));
            ASSERT_EQ(12u, DataStoreQueryGetRowCount(readers[i]));
        }
        // End in a different order from admission, moving live reservations in
        // the dense pool. Reuse each freed slot while the others remain active.
        for (uint32_t i = 0; i < 40; ++i)
        {
            HDataStoreQuery reader = readers[(i * 17) % 40];
            ASSERT_EQ(DATASTORE_RESULT_BUSY, DataStoreQueryTryBegin(writer));
            ASSERT_EQ(DATASTORE_RESULT_BUSY, DataStoreQueryTryBegin(reader));
            DataStoreQueryEnd(reader);
            ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreQueryTryBegin(reader));
            DataStoreQueryEnd(reader);
        }
        ASSERT_EQ(DATASTORE_RESULT_OK, TryFromWorker(writer));
    }
    for (uint32_t i = 0; i < 40; ++i)
    {
        ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreQueryDestroy(readers[i]));
    }
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreQueryDestroy(writer));
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreDestroy(fixture.m_Store));
}

// Parent/whole-row access conflicts with child writes while sibling fields can run together.
TEST(DataThreaded, NestedRangesAndWholeRows)
{
    ThreadFixture      fixture = CreateFixture();
    HDataStoreQuery    parent = Query(fixture.m_Store, LIGHT, DATASTORE_TYPE_STRUCT, 0, 0);
    HDataStoreQuery    child = Query(fixture.m_Store, dmHashString64("light.color"), DATASTORE_TYPE_VECTOR3, DATASTORE_FIELD_MUTABLE, 0);
    HDataStoreQuery    sibling = Query(fixture.m_Store, dmHashString64("light.intensity"), DATASTORE_TYPE_NUMBER, DATASTORE_FIELD_MUTABLE, 0);
    HDataStoreQuery    whole;
    DataStoreQueryDesc desc = { { 0 }, 0, 0, DATASTORE_ROW_MUTABLE };
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreQueryCreate(fixture.m_Store, &desc, &whole));
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreQueryTryBegin(child));
    ASSERT_EQ(DATASTORE_RESULT_BUSY, TryFromWorker(parent));
    ASSERT_EQ(DATASTORE_RESULT_BUSY, TryFromWorker(whole));
    ASSERT_EQ(DATASTORE_RESULT_OK, TryFromWorker(sibling));
    DataStoreQueryEnd(child);
    HDataStoreQuery queries[] = { parent, child, sibling, whole };
    for (uint32_t q = 0; q < 4; ++q)
    {
        ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreQueryDestroy(queries[q]));
    }
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreDestroy(fixture.m_Store));
}

// Disjoint ranges visit group-filtered rows once and refresh their coverage after removal.
TEST(DataThreaded, RangesCoverOwnerFilteredRowsAndRefreshAfterMutation)
{
    ThreadFixture      fixture = CreateFixture();
    DataStoreId        group = fixture.m_Group;
    DataStoreQueryItem field = { HEALTH, DATASTORE_TYPE_NUMBER, 0, DATASTORE_QUERY_ITEM_FIELD };
    DataStoreQueryDesc desc = { { field }, &group, 1, 0 };
    HDataStoreQuery    query;
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreQueryCreate(fixture.m_Store, &desc, &query));
    for (uint32_t phase = 0; phase < 2; ++phase)
    {
        ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreQueryTryBegin(query));
        uint32_t count = DataStoreQueryGetRowCount(query);
        ASSERT_EQ(6u - phase, count);
        bool seen[12] = {};
        for (uint32_t first = 0; first < count; first += 2)
        {
            DataStoreIterator it = DataStoreQueryIterRange(query, first, count - first < 2 ? count - first : 2);
            while (DataStoreIterNext(&it))
            {
                DataStoreRowIterator rows = DataStoreIterRows(&it);
                while (DataStoreRowIterNext(&rows))
                {
                    ASSERT_EQ(group, DataStoreRowIterGetGroupId(&rows));
                    DataStoreId id    = DataStoreRowIterGetId(&rows);
                    uint32_t    index = 0;
                    while (index < 12 && fixture.m_Ids[index] != id)
                    {
                        ++index;
                    }
                    ASSERT_LT(index, 12u);
                    ASSERT_FALSE(seen[index]);
                    seen[index] = true;
                }
            }
        }
        uint32_t visited = 0;
        for (uint32_t i = 0; i < 12; ++i)
        {
            visited += seen[i];
        }
        ASSERT_EQ(count, visited);
        DataStoreIterator empty = DataStoreQueryIterRange(query, count, 0);
        ASSERT_FALSE(DataStoreIterNext(&empty));
        DataStoreQueryEnd(query);
        if (!phase)
        {
            ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreRowRemove(fixture.m_Store, fixture.m_Ids[1]));
        }
    }
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreQueryDestroy(query));
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreDestroy(fixture.m_Store));
}

struct MutationAttempt
{
    HDataStoreContext m_Store;
    DataStoreId       m_Id;
    DataStoreId       m_Group;
    DataStoreResult   m_Remove, m_RemoveGroup, m_CreateRows, m_CreateQuery, m_DestroyStore;
};

static void MutateOnThread(void* context)
{
    MutationAttempt* attempt = (MutationAttempt*)context;
    attempt->m_Remove = DataStoreRowRemove(attempt->m_Store, attempt->m_Id);
    attempt->m_RemoveGroup = DataStoreGroupRemove(attempt->m_Store, attempt->m_Group);
    uint64_t    values[6] = {};
    DataStoreId id;
    attempt->m_CreateRows = DataStoreRowCreate(attempt->m_Store, 100, attempt->m_Group, 1, values, &id);
    HDataStoreQuery    query = 0;
    DataStoreQueryDesc desc = {};
    attempt->m_CreateQuery = DataStoreQueryCreate(attempt->m_Store, &desc, &query);
    attempt->m_DestroyStore = DataStoreDestroy(attempt->m_Store);
}

// Disjoint groups may reserve the same field, while structural mutation remains excluded.
TEST(DataThreaded, MutationExclusionAndDisjointGroups)
{
    ThreadFixture      fixture = CreateFixture();
    DataStoreQueryItem field = { HEALTH, DATASTORE_TYPE_NUMBER, DATASTORE_FIELD_MUTABLE, DATASTORE_QUERY_ITEM_FIELD };
    DataStoreId        groups[] = { 0, fixture.m_Group };
    HDataStoreQuery    queries[2];
    for (uint32_t i = 0; i < 2; ++i)
    {
        DataStoreQueryDesc desc = { { field }, &groups[i], 1, 0 };
        ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreQueryCreate(fixture.m_Store, &desc, &queries[i]));
    }
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreQueryTryBegin(queries[0]));
    ASSERT_EQ(DATASTORE_RESULT_OK, TryFromWorker(queries[1]));
    MutationAttempt  attempt = { fixture.m_Store, fixture.m_Ids[0], fixture.m_Group };
    dmThread::Thread worker = dmThread::New(MutateOnThread, 0x80000, &attempt, "data-mutate");
    dmThread::Join(worker);
    ASSERT_EQ(DATASTORE_RESULT_LOCKED, attempt.m_Remove);
    ASSERT_EQ(DATASTORE_RESULT_LOCKED, attempt.m_RemoveGroup);
    ASSERT_EQ(DATASTORE_RESULT_LOCKED, attempt.m_CreateRows);
    ASSERT_EQ(DATASTORE_RESULT_LOCKED, attempt.m_CreateQuery);
    ASSERT_EQ(DATASTORE_RESULT_LOCKED, attempt.m_DestroyStore);
    DataStoreQueryEnd(queries[0]);
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreRowRemove(fixture.m_Store, fixture.m_Ids[0]));
    for (uint32_t i = 0; i < 2; ++i)
    {
        ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreQueryDestroy(queries[i]));
    }
    DataStoreQueryItem invalid = { LIGHT, DATASTORE_TYPE_STRUCT, DATASTORE_FIELD_MUTABLE, DATASTORE_QUERY_ITEM_FIELD };
    DataStoreQueryDesc desc = { { invalid }, 0, 0, 0 };
    HDataStoreQuery    query = 0;
    ASSERT_EQ(DATASTORE_RESULT_INVALID_ARGUMENT, DataStoreQueryCreate(fixture.m_Store, &desc, &query));
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreDestroy(fixture.m_Store));
}

struct AdmissionRace
{
    dmMutex::HMutex                                m_Mutex;
    dmConditionVariable::HConditionVariable        m_Condition;
    HDataStoreQuery                                m_Query;
    uint32_t                                       m_Ready, m_Attempted;
    bool                                           m_Start;
};

struct AdmissionWorker
{
    AdmissionRace*        m_Race;
    DataStoreResult       m_Result;
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
        {
            dmConditionVariable::Wait(race->m_Condition, race->m_Mutex);
        }
    }
    worker->m_Result = DataStoreQueryTryBegin(race->m_Query);
    {
        DM_MUTEX_SCOPED_LOCK(race->m_Mutex);
        ++race->m_Attempted;
        dmConditionVariable::Broadcast(race->m_Condition);
        // The successful begin retains access until both callers have attempted.
        while (race->m_Attempted != 2)
        {
            dmConditionVariable::Wait(race->m_Condition, race->m_Mutex);
        }
    }
    if (worker->m_Result == DATASTORE_RESULT_OK)
    {
        DataStoreQueryEnd(race->m_Query);
    }
}

// Simultaneous conflicting reservations admit exactly one worker until access is released.
TEST(DataThreaded, SimultaneousBeginsAdmitExactlyOne)
{
    ThreadFixture   fixture = CreateFixture();
    HDataStoreQuery queries[] = {
        Query(fixture.m_Store, HEALTH, DATASTORE_TYPE_NUMBER, DATASTORE_FIELD_MUTABLE, 0),
        Query(fixture.m_Store, HEALTH, DATASTORE_TYPE_NUMBER, 0, 0)
    };
    for (uint32_t pass = 0; pass < 32; ++pass)
    {
        AdmissionRace race = {};
        race.m_Mutex = dmMutex::New();
        race.m_Condition = dmConditionVariable::New();
        race.m_Query = queries[pass % 2];
        AdmissionWorker  workers[] = { { &race, DATASTORE_RESULT_INVALID_ARGUMENT }, { &race, DATASTORE_RESULT_INVALID_ARGUMENT } };
        dmThread::Thread threads[2];
        for (uint32_t i = 0; i < 2; ++i)
        {
            threads[i] = dmThread::New(RaceAdmission, 0x80000, &workers[i], "data-race");
        }
        {
            DM_MUTEX_SCOPED_LOCK(race.m_Mutex);
            while (race.m_Ready != 2)
            {
                dmConditionVariable::Wait(race.m_Condition, race.m_Mutex);
            }
            race.m_Start = true;
            dmConditionVariable::Broadcast(race.m_Condition);
        }
        for (uint32_t i = 0; i < 2; ++i)
        {
            dmThread::Join(threads[i]);
        }
        ASSERT_TRUE((workers[0].m_Result == DATASTORE_RESULT_OK && workers[1].m_Result == DATASTORE_RESULT_BUSY) ||
                    (workers[1].m_Result == DATASTORE_RESULT_OK && workers[0].m_Result == DATASTORE_RESULT_BUSY));
        dmConditionVariable::Delete(race.m_Condition);
        dmMutex::Delete(race.m_Mutex);
    }
    for (uint32_t i = 0; i < 2; ++i)
    {
        ASSERT_EQ(DATASTORE_RESULT_OK, TryFromWorker(queries[i]));
        ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreQueryDestroy(queries[i]));
    }
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreDestroy(fixture.m_Store));
}

struct RangeWork
{
    HDataStoreQuery          m_Query;
    uint32_t                 m_First, m_Count;
    DataStoreFieldId         m_Field;
    uint32_t                 m_Visited;
};

static int32_t WriteRange(HJobContext, HJob, void*, void* data)
{
    RangeWork* work = (RangeWork*)data;
    DataStoreIterator it = DataStoreQueryIterRange(work->m_Query, work->m_First, work->m_Count);
    while (DataStoreIterNext(&it))
    {
        DataStoreRowIterator rows = DataStoreIterRows(&it);
        while (DataStoreRowIterNext(&rows))
        {
            *DataStoreRowIterGetNumberMut(&rows, work->m_Field) += 1;
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

static void ReadScheduledHealth(HDataStoreQuery query, uint32_t first, uint32_t count, void* context)
{
    ScheduledJobState* state = (ScheduledJobState*)context;
    dmAtomicIncrement32(&state->m_Calls);
    if (state->m_Gate)
    {
        ScheduledJobGate* gate = state->m_Gate;
        DM_MUTEX_SCOPED_LOCK(gate->m_Mutex);
        ++gate->m_Started;
        dmConditionVariable::Broadcast(gate->m_Condition);
        while (!gate->m_Release)
        {
            dmConditionVariable::Wait(gate->m_Condition, gate->m_Mutex);
        }
    }
    DataStoreIterator it    = DataStoreQueryIterRange(query, first, count);
    uint32_t          index = first;
    while (DataStoreIterNext(&it))
    {
        DataStoreRowIterator rows = DataStoreIterRows(&it);
        while (DataStoreRowIterNext(&rows))
        {
            state->m_Values[index++] = *DataStoreRowIterGetNumber(&rows, state->m_Field);
        }
    }
}

static void RegenerateScheduledHealth(HDataStoreQuery query, uint32_t first, uint32_t count, void* context)
{
    ScheduledJobState* state = (ScheduledJobState*)context;
    dmAtomicIncrement32(&state->m_Calls);
    DataStoreIterator it = DataStoreQueryIterRange(query, first, count);
    while (DataStoreIterNext(&it))
    {
        DataStoreRowIterator rows = DataStoreIterRows(&it);
        while (DataStoreRowIterNext(&rows))
        {
            *DataStoreRowIterGetNumberMut(&rows, state->m_Field) += 1;
        }
    }
}

static void ReadScheduledRange(HDataStoreQuery query, uint32_t first, uint32_t count, void* context)
{
    ScheduledRangeState* state = (ScheduledRangeState*)context;
    if (!first)
    {
        DM_MUTEX_SCOPED_LOCK(state->m_Mutex);
        while (!state->m_Release)
        {
            dmConditionVariable::Wait(state->m_Condition, state->m_Mutex);
        }
    }
    ReadScheduledHealth(query, first, count, &state->m_Reader);
    if (first)
    {
        DM_MUTEX_SCOPED_LOCK(state->m_Mutex);
        ++state->m_Finished;
        dmConditionVariable::Broadcast(state->m_Condition);
    }
}

// Hold the first range while later ranges finish on another worker. Neither an
// explicit read-only dependent nor a conflicting writer may start before it finishes.
TEST(DataThreaded, SchedulerSplitRangesCompleteOutOfOrder)
{
    ThreadFixture       fixture = CreateFixture();
    ScheduledRangeState ranges = { {}, dmMutex::New(), dmConditionVariable::New() };
    ScheduledJobState   observer = {}, writer = {};
    HDataStoreQuery     queries[] = {
        Query(fixture.m_Store, HEALTH, DATASTORE_TYPE_NUMBER, 0, 0),
        Query(fixture.m_Store, HEALTH, DATASTORE_TYPE_NUMBER, 0, 0),
        Query(fixture.m_Store, HEALTH, DATASTORE_TYPE_NUMBER, DATASTORE_FIELD_MUTABLE, 0)
    };
    DataStoreQueryItem field = { HEALTH, DATASTORE_TYPE_NUMBER, 0, DATASTORE_QUERY_ITEM_FIELD };
    ranges.m_Reader.m_Field = DataStoreQueryFindField(queries[0], &field);
    observer.m_Field = DataStoreQueryFindField(queries[1], &field);
    field.m_Access = DATASTORE_FIELD_MUTABLE;
    writer.m_Field = DataStoreQueryFindField(queries[2], &field);
    DataStoreSystemDesc systems[] = {
        { dmHashString64("observe"), { dmHashString64("read_ranges") }, queries[1], ReadScheduledHealth, &observer },
        { dmHashString64("read_ranges"), {}, queries[0], ReadScheduledRange, &ranges, 5 },
        { dmHashString64("regenerate"), {}, queries[2], RegenerateScheduledHealth, &writer, 5 }
    };
    for (uint32_t i = 0; i < 3; ++i)
    {
        ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreFunctionAdd(fixture.m_Store, &systems[i]));
    }
    JobSystemCreateParams params = { "ds-ranges", 2 };
    HJobContext           jobs = JobSystemCreate(&params);
    for (uint32_t frame = 0; frame < 20; ++frame)
    {
        ranges.m_Finished = 0;
        ranges.m_Release = false;
        ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreSchedulerBegin(fixture.m_Store, jobs));
        {
            DM_MUTEX_SCOPED_LOCK(ranges.m_Mutex);
            while (ranges.m_Finished != 2)
            {
                dmConditionVariable::Wait(ranges.m_Condition, ranges.m_Mutex);
            }
        }
        EXPECT_EQ(DATASTORE_RESULT_BUSY, DataStoreSchedulerUpdate(fixture.m_Store));
        EXPECT_EQ((int32_t)(3 * frame + 2), dmAtomicGet32(&ranges.m_Reader.m_Calls));
        EXPECT_EQ((int32_t)frame, dmAtomicGet32(&observer.m_Calls));
        EXPECT_EQ((int32_t)(3 * frame), dmAtomicGet32(&writer.m_Calls));
        EXPECT_EQ(DATASTORE_RESULT_BUSY, DataStoreQueryTryBegin(queries[2]));
        EXPECT_EQ(DATASTORE_RESULT_BUSY, DataStoreSchedulerEnd(fixture.m_Store));
        EXPECT_EQ(DATASTORE_RESULT_LOCKED, DataStoreRowRemove(fixture.m_Store, fixture.m_Ids[0]));
        {
            DM_MUTEX_SCOPED_LOCK(ranges.m_Mutex);
            ranges.m_Release = true;
            dmConditionVariable::Signal(ranges.m_Condition);
        }
        DataStoreResult result;
        do
        {
            result = DataStoreSchedulerUpdate(fixture.m_Store);
        } while (result == DATASTORE_RESULT_BUSY);
        ASSERT_EQ(DATASTORE_RESULT_OK, result);
        ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreSchedulerEnd(fixture.m_Store));
        ASSERT_EQ((int32_t)(3 * (frame + 1)), dmAtomicGet32(&ranges.m_Reader.m_Calls));
        ASSERT_EQ((int32_t)frame + 1, dmAtomicGet32(&observer.m_Calls));
        ASSERT_EQ((int32_t)(3 * (frame + 1)), dmAtomicGet32(&writer.m_Calls));
        for (uint32_t row = 0; row < 12; ++row)
        {
            ASSERT_EQ(50.0 + frame, ranges.m_Reader.m_Values[row]);
            ASSERT_EQ(50.0 + frame, observer.m_Values[row]);
        }
    }
    JobSystemDestroy(jobs);
    dmConditionVariable::Delete(ranges.m_Condition);
    dmMutex::Delete(ranges.m_Mutex);
    for (uint32_t i = 0; i < 3; ++i)
    {
        ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreQueryDestroy(queries[i]));
    }
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreDestroy(fixture.m_Store));
}

// One event launches both readers on workers. Their reservations exclude the writer;
// completion automatically launches it and its observer without per-job caller dispatch.
TEST(DataThreaded, SchedulerAutomaticJobs)
{
    ThreadFixture       fixture = CreateFixture();
    ScheduledJobGate    gate = { dmMutex::New(), dmConditionVariable::New() };
    ScheduledJobState   states[4] = { { &gate }, { &gate } };
    DataStoreSystemDesc systems[] = {
        { dmHashString64("read_first"), { dmHashString64("engine.update.begin") }, 0, ReadScheduledHealth, &states[0] },
        { dmHashString64("read_second"), { dmHashString64("engine.update.begin") }, 0, ReadScheduledHealth, &states[1] },
        { dmHashString64("regenerate"), { dmHashString64("engine.update.begin") }, 0, RegenerateScheduledHealth, &states[2] },
        { dmHashString64("observe"), { dmHashString64("regenerate") }, 0, ReadScheduledHealth, &states[3] }
    };
    for (uint32_t i = 0; i < 4; ++i)
    {
        uint32_t access = i == 2 ? DATASTORE_FIELD_MUTABLE : 0;
        systems[i].m_Query = Query(fixture.m_Store, HEALTH, DATASTORE_TYPE_NUMBER, access, 0);
        DataStoreQueryItem field = { HEALTH, DATASTORE_TYPE_NUMBER, access, DATASTORE_QUERY_ITEM_FIELD };
        states[i].m_Field = DataStoreQueryFindField(systems[i].m_Query, &field);
        ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreFunctionAdd(fixture.m_Store, &systems[i]));
    }
    DataStoreEventDesc begin = { dmHashString64("engine.update.begin") };
    DataStoreEventDesc end = { dmHashString64("engine.update.end"), { dmHashString64("observe") } };
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreEventAdd(fixture.m_Store, &begin));
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreEventAdd(fixture.m_Store, &end));
    JobSystemCreateParams params = { "ds-schedule", 4 };
    HJobContext           jobs = JobSystemCreate(&params);
    for (uint32_t frame = 0; frame < 20; ++frame)
    {
        gate.m_Started = 0;
        gate.m_Release = false;
        ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreSchedulerBegin(fixture.m_Store, jobs));
        ASSERT_EQ(DATASTORE_RESULT_BUSY, DataStoreSchedulerUpdate(fixture.m_Store));
        ASSERT_EQ(DATASTORE_RESULT_BUSY, DataStoreSchedulerEvent(fixture.m_Store, end.m_Name));
        ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreSchedulerEvent(fixture.m_Store, begin.m_Name));
        {
            // Both readers must reach the gate before checking exclusion;
            // slow worker startup must not change the result.
            DM_MUTEX_SCOPED_LOCK(gate.m_Mutex);
            while (gate.m_Started != 2)
            {
                dmConditionVariable::Wait(gate.m_Condition, gate.m_Mutex);
            }
        }
        ASSERT_EQ((int32_t)frame, dmAtomicGet32(&states[2].m_Calls));
        ASSERT_EQ((int32_t)frame, dmAtomicGet32(&states[3].m_Calls));
        ASSERT_EQ(DATASTORE_RESULT_BUSY, DataStoreSchedulerUpdate(fixture.m_Store));
        ASSERT_EQ(DATASTORE_RESULT_BUSY, DataStoreSchedulerEnd(fixture.m_Store));
        ASSERT_EQ(DATASTORE_RESULT_LOCKED, DataStoreRowRemove(fixture.m_Store, fixture.m_Ids[0]));
        {
            DM_MUTEX_SCOPED_LOCK(gate.m_Mutex);
            gate.m_Release = true;
            dmConditionVariable::Broadcast(gate.m_Condition);
        }
        DataStoreResult result;
        do
        {
            result = DataStoreSchedulerUpdate(fixture.m_Store);
        } while (result == DATASTORE_RESULT_BUSY);
        ASSERT_EQ(DATASTORE_RESULT_OK, result);
        ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreSchedulerEvent(fixture.m_Store, end.m_Name));
        ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreSchedulerEnd(fixture.m_Store));
        for (uint32_t i = 0; i < 4; ++i)
        {
            ASSERT_EQ((int32_t)frame + 1, dmAtomicGet32(&states[i].m_Calls));
        }
        for (uint32_t r = 0; r < 12; ++r)
        {
            ASSERT_EQ(50.0 + frame, states[0].m_Values[r]);
            ASSERT_EQ(50.0 + frame, states[1].m_Values[r]);
            ASSERT_EQ(51.0 + frame, states[3].m_Values[r]);
        }
    }
    JobSystemDestroy(jobs);
    dmConditionVariable::Delete(gate.m_Condition);
    dmMutex::Delete(gate.m_Mutex);
    for (uint32_t i = 0; i < 4; ++i)
    {
        ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreQueryDestroy(systems[i].m_Query));
    }
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreDestroy(fixture.m_Store));
}

static void HoldSchedulerAdmission(void* context)
{
    SchedulerAdmissionGate* state = (SchedulerAdmissionGate*)context;
    ScheduledJobGate*       gate = &state->m_Gate;
    DM_MUTEX_SCOPED_LOCK(state->m_Store->m_Mutex);
    DM_MUTEX_SCOPED_LOCK(gate->m_Mutex);
    ++gate->m_Started;
    dmConditionVariable::Signal(gate->m_Condition);
    while (!gate->m_Release)
    {
        dmConditionVariable::Wait(gate->m_Condition, gate->m_Mutex);
    }
}

// Temporary store-mutex contention must not let the engine end a schedule before
// a ready callback runs. Condition signals control contention without time limits.
TEST(DataThreaded, SchedulerRetriesAdmissionBeforeEnd)
{
    ThreadFixture       fixture  = CreateFixture();
    HDataStoreQuery     query    = Query(fixture.m_Store, HEALTH, DATASTORE_TYPE_NUMBER, 0, 0);
    DataStoreQueryItem  field    = { HEALTH, DATASTORE_TYPE_NUMBER, 0, DATASTORE_QUERY_ITEM_FIELD };
    ScheduledJobState   state    = { 0, DataStoreQueryFindField(query, &field) };
    DataStoreEventDesc  event    = { DATASTORE_EVENT_GAMEOBJECT_PRE_UPDATE };
    DataStoreSystemDesc function = { dmHashString64("read"), { event.m_Name }, query, ReadScheduledHealth, &state };
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreEventAdd(fixture.m_Store, &event));
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreFunctionAdd(fixture.m_Store, &function));
    JobSystemCreateParams  params = { "ds-admit", 0 };
    HJobContext            jobs = JobSystemCreate(&params);
    SchedulerAdmissionGate gate = { fixture.m_Store, { dmMutex::New(), dmConditionVariable::New() } };

    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreSchedulerBegin(fixture.m_Store, jobs));
    dmThread::Thread worker = dmThread::New(HoldSchedulerAdmission, 0x80000, &gate, "ds-admit");
    {
        DM_MUTEX_SCOPED_LOCK(gate.m_Gate.m_Mutex);
        while (!gate.m_Gate.m_Started)
        {
            dmConditionVariable::Wait(gate.m_Gate.m_Condition, gate.m_Gate.m_Mutex);
        }
    }
    DataStoreResult result = DataStoreSchedulerEvent(fixture.m_Store, event.m_Name);
    {
        DM_MUTEX_SCOPED_LOCK(gate.m_Gate.m_Mutex);
        gate.m_Gate.m_Release = true;
        dmConditionVariable::Signal(gate.m_Gate.m_Condition);
    }
    dmThread::Join(worker);
    ASSERT_EQ(DATASTORE_RESULT_OK, result);
    ASSERT_EQ(0, dmAtomicGet32(&state.m_Calls));
    ASSERT_EQ(DATASTORE_RESULT_BUSY, DataStoreSchedulerEnd(fixture.m_Store));
    while (DataStoreSchedulerEnd(fixture.m_Store) == DATASTORE_RESULT_BUSY)
    {
        DataStoreSchedulerUpdate(fixture.m_Store);
    }
    ASSERT_EQ(1, dmAtomicGet32(&state.m_Calls));
    for (uint32_t i = 0; i < DM_ARRAY_SIZE(state.m_Values); ++i)
    {
        ASSERT_EQ(50.0, state.m_Values[i]);
    }

    dmConditionVariable::Delete(gate.m_Gate.m_Condition);
    dmMutex::Delete(gate.m_Gate.m_Mutex);
    JobSystemDestroy(jobs);
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreQueryDestroy(query));
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreDestroy(fixture.m_Store));
}

// Disjoint job ranges update each row once per pass with 1, 2, 4 or 8 workers.
TEST(DataThreaded, JobRangesWithOneTwoFourEightWorkers)
{
    for (uint32_t workers = 1; workers <= 8; workers *= 2)
    {
        ThreadFixture         fixture         = CreateFixture();
        HDataStoreQuery       query           = Query(fixture.m_Store, HEALTH, DATASTORE_TYPE_NUMBER, DATASTORE_FIELD_MUTABLE, 0);
        DataStoreQueryItem    requested_field = { HEALTH, DATASTORE_TYPE_NUMBER, DATASTORE_FIELD_MUTABLE, DATASTORE_QUERY_ITEM_FIELD };
        DataStoreFieldId      field           = DataStoreQueryFindField(query, &requested_field);
        JobSystemCreateParams params          = { "data-test", (uint8_t)workers };
        HJobContext           jobs            = JobSystemCreate(&params);
        for (uint32_t pass = 0; pass < 20; ++pass)
        {
            ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreQueryTryBegin(query));
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
            {
                JobSystemUpdate(jobs, 0);
            }
            for (uint32_t j = 0; j < 4; ++j)
            {
                ASSERT_EQ(3u, ranges[j].m_Visited);
            }
            DataStoreQueryEnd(query);
        }
        JobSystemDestroy(jobs);
        for (uint32_t i = 0; i < 12; ++i)
        {
            double value;
            ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreFieldGetNumber(fixture.m_Store, fixture.m_Ids[i], HEALTH, &value));
            ASSERT_EQ(70.0, value);
        }
        ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreQueryDestroy(query));
        ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreDestroy(fixture.m_Store));
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
    DataStoreId  ids[73];
    for (uint32_t i = 0; i < 73; ++i)
    {
        ids[i] = reader->m_Fixture->m_Ids[i % 12];
    }
    for (uint32_t pass = 0; pass < 100; ++pass)
    {
        double           health[73];
        DataStoreVector3 positions[73];
        if (DataStoreFieldGetNumberBatch(reader->m_Fixture->m_Store, 73, ids, HEALTH, health) != DATASTORE_RESULT_OK ||
            DataStoreFieldGetVector3Batch(reader->m_Fixture->m_Store, 73, ids, POSITION, positions) != DATASTORE_RESULT_OK)
        {
            ++reader->m_Errors;
            return;
        }
        for (uint32_t i = 0; i < 73; ++i)
        {
            reader->m_Errors += health[i] != 50 || positions[i].m_Values[0] != 0 || positions[i].m_Values[1] != 0 || positions[i].m_Values[2] != 0;
        }
    }
}

// The caller schedules only readers and joins them before changing or releasing storage.
TEST(DataThreaded, ConcurrentBatchReadersWithCallerSynchronization)
{
    ThreadFixture    fixture = CreateFixture();
    BatchReader      readers[4] = {};
    dmThread::Thread threads[4];
    // The caller excludes structural changes and writes until all readers finish.
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
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreDestroy(fixture.m_Store));
}

static void ReadStructRows(void* context)
{
    StructReadTask* task = (StructReadTask*)context;
    for (uint32_t repeat = 0; repeat < 100; ++repeat)
    {
        DataStoreIterator iterator = DataStoreQueryIterRange(task->m_Query, task->m_First, task->m_Count);
        while (DataStoreIterNext(&iterator))
        {
            DataStoreRowIterator rows = DataStoreIterRows(&iterator);
            while (DataStoreRowIterNext(&rows))
            {
                DataStoreValueView object, value;
                DataStoreRowIterGetView(&rows, task->m_Field, &object);
                task->m_Result = DataStoreViewGetField(&object, HEALTH, &value);
                if (task->m_Result != DATASTORE_RESULT_OK)
                {
                    return;
                }
                double health;
                task->m_Result = DataStoreViewGetNumber(&value, &health);
                if (task->m_Result != DATASTORE_RESULT_OK)
                {
                    return;
                }
                task->m_Total += health;
            }
        }
    }
}

// Public container views must remain stable while separate jobs read child rows.
TEST(DataThreaded, OwnedStructRowsRemainStableDuringQueryJobs)
{
    HDataStoreContext  store = DataStoreCreate(0);
    DataStoreFieldDesc field = { 10, DATASTORE_TYPE_STRUCT };
    DataStoreTableDesc desc = { 1, 0, 0, &field, 1, 8 };
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreTableAdd(store, &desc));
    DataStoreFieldDesc   health = { HEALTH, DATASTORE_TYPE_NUMBER };
    DataStoreStructDesc  layout = { &health, 1, 8 };
    double               value = 10;
    DataStoreStructInput input = { &layout, &value };
    DataStoreReference   rows[128];
    DataStoreId          ids[128];
    for (uint32_t i = 0; i < 128; ++i)
    {
        rows[i].m_Struct = &input;
    }
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreRowCreate(store, 1, 0, 128, rows, ids));
    HDataStoreQuery    query = Query(store, 10, DATASTORE_TYPE_STRUCT, 0, 0);
    DataStoreQueryItem requested = { 10, DATASTORE_TYPE_STRUCT, 0, DATASTORE_QUERY_ITEM_FIELD };
    DataStoreFieldId   handle = DataStoreQueryFindField(query, &requested);
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreQueryTryBegin(query));
    StructReadTask   tasks[4];
    dmThread::Thread threads[4];
    for (uint32_t i = 0; i < 4; ++i)
    {
        tasks[i] = { query, i* 32, 32, handle };
        threads[i] = dmThread::New(ReadStructRows, 0x80000, &tasks[i], "data-struct-read");
    }
    ASSERT_EQ(DATASTORE_RESULT_LOCKED, DataStoreRowRemove(store, ids[0]));
    ASSERT_EQ(DATASTORE_RESULT_LOCKED, DataStoreRowReset(store, ids[0]));
    ASSERT_EQ(DATASTORE_RESULT_LOCKED, DataStoreRowCreate(store, 1, 0, 128, rows, 0));
    for (uint32_t i = 0; i < 4; ++i)
    {
        dmThread::Join(threads[i]);
        ASSERT_EQ(DATASTORE_RESULT_OK, tasks[i].m_Result);
        ASSERT_EQ(32000.0, tasks[i].m_Total);
    }
    DataStoreQueryEnd(query);
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreQueryDestroy(query));
    for (uint32_t i = 0; i < 128; ++i)
    {
        ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreRowRemove(store, ids[i]));
    }
    ASSERT_EQ(0u, FindTable(store, 1)->m_Structs->m_Tables[0]->m_RowCount);
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreDestroy(store));
}

static void ExerciseBlobLifetime(void* context)
{
    BlobLifetimeTask* task  = (BlobLifetimeTask*)context;
    HDataStoreContext store = DataStoreCreate(0);
    {
        DM_MUTEX_SCOPED_LOCK(task->m_Mutex);
        ++*task->m_Ready;
        dmConditionVariable::Broadcast(task->m_Condition);
        while (!*task->m_Start)
        {
            dmConditionVariable::Wait(task->m_Condition, task->m_Mutex);
        }
    }
    for (uint32_t i = 0; i < 1000; ++i)
    {
        HDataStoreBlobInstance instance;
        task->m_Result = DataStoreBlobAdd(store, task->m_Blob, 0, &instance);
        if (task->m_Result != DATASTORE_RESULT_OK)
        {
            break;
        }
        task->m_Result = DataStoreBlobRemove(instance);
        if (task->m_Result != DATASTORE_RESULT_OK)
        {
            break;
        }
    }
    DataStoreResult result = DataStoreDestroy(store);
    if (task->m_Result == DATASTORE_RESULT_OK)
    {
        task->m_Result = result;
    }
}

// Each store has a different mutex. Retaining/releasing one immutable resource
// from all of them must synchronize at the blob handle, including pool teardown.
TEST(DataThreaded, SharedBlobLifetimeAcrossStores)
{
    ThreadFixture source = CreateFixture();
    uint32_t      size;
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreBlobWrite(source.m_Store, 0, 0, &size));
    uint8_t* bytes = new uint8_t[size];
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreBlobWrite(source.m_Store, bytes, size, &size));
    HDataStoreBlob blob;
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreBlobLoad(bytes, size, &blob));
    dmMutex::HMutex                         mutex = dmMutex::New();
    dmConditionVariable::HConditionVariable condition = dmConditionVariable::New();
    uint32_t                                ready = 0;
    bool                                    start = false;
    BlobLifetimeTask                        tasks[8];
    dmThread::Thread                        threads[8];
    for (uint32_t i = 0; i < 8; ++i)
    {
        tasks[i] = { blob, DATASTORE_RESULT_OK, mutex, condition, &ready, &start };
        threads[i] = dmThread::New(ExerciseBlobLifetime, 0x80000, &tasks[i], "data-blob-lifetime");
    }
    {
        DM_MUTEX_SCOPED_LOCK(mutex);
        while (ready != 8)
        {
            dmConditionVariable::Wait(condition, mutex);
        }
        start = true;
        dmConditionVariable::Broadcast(condition);
    }
    for (uint32_t i = 0; i < 8; ++i)
    {
        dmThread::Join(threads[i]);
        ASSERT_EQ(DATASTORE_RESULT_OK, tasks[i].m_Result);
    }
    ASSERT_EQ(1, dmAtomicGet32(&blob->m_RefCount));
    DataStoreBlobDestroy(blob);
    delete[] bytes;
    dmConditionVariable::Delete(condition);
    dmMutex::Delete(mutex);
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreDestroy(source.m_Store));
}

// Independent group writers run simultaneously, each split into separate jobs.
TEST(DataThreaded, DisjointGroupJobs)
{
    ThreadFixture      fixture = CreateFixture();
    DataStoreId        groups[] = { 0, fixture.m_Group };
    HDataStoreQuery    queries[2];
    uint32_t           handles[2];
    DataStoreQueryItem field = { HEALTH, DATASTORE_TYPE_NUMBER, DATASTORE_FIELD_MUTABLE, DATASTORE_QUERY_ITEM_FIELD };
    for (uint32_t i = 0; i < 2; ++i)
    {
        DataStoreQueryDesc desc = { { field }, groups + i, 1, 0 };
        ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreQueryCreate(fixture.m_Store, &desc, queries + i));
        handles[i] = DataStoreQueryFindField(queries[i], &field);
    }
    JobSystemCreateParams params = { "data-groups", 4 };
    HJobContext           jobs = JobSystemCreate(&params);
    for (uint32_t pass = 0; pass < 20; ++pass)
    {
        ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreQueryTryBegin(queries[0]));
        ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreQueryTryBegin(queries[1]));
        uint32_t  completed = 0;
        RangeWork work[4];
        for (uint32_t i = 0; i < 4; ++i)
        {
            uint32_t group = i / 2;
            work[i] = { queries[group], (i % 2) * 3, 3, handles[group] };
            Job  job = { WriteRange, RangeDone, &completed, work + i };
            HJob handle = JobSystemCreateJob(jobs, &job);
            ASSERT_EQ(JOBSYSTEM_RESULT_OK, JobSystemPushJob(jobs, handle));
        }
        while (completed != 4)
        {
            JobSystemUpdate(jobs, 0);
        }
        DataStoreQueryEnd(queries[0]);
        DataStoreQueryEnd(queries[1]);
    }
    JobSystemDestroy(jobs);
    for (uint32_t i = 0; i < 12; ++i)
    {
        double health;
        ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreFieldGetNumber(fixture.m_Store, fixture.m_Ids[i], HEALTH, &health));
        ASSERT_EQ(70.0, health);
    }
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreQueryDestroy(queries[0]));
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreQueryDestroy(queries[1]));
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreDestroy(fixture.m_Store));
}
