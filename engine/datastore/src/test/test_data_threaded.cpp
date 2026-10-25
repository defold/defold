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
    HDSContext m_Store;
    DSId       m_Ids[12];
    DSId       m_Group;
};

struct StructReadTask
{
    HDSQuery m_Query;
    uint32_t m_First;
    uint32_t m_Count;
    DSFieldId         m_Field;
    double   m_Total;
    DSResult m_Result;
};

struct BlobLifetimeTask
{
    HDSBlob                                 m_Blob;
    DSResult                                m_Result;
    dmMutex::HMutex                         m_Mutex;
    dmConditionVariable::HConditionVariable m_Condition;
    uint32_t*                               m_Ready;
    bool*                                   m_Start;
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
    ScheduledJobGate* m_Gate;
    DSFieldId         m_Field;
    int32_atomic_t    m_Calls;
    double            m_Values[12];
};

struct ScheduledRangeState
{
    ScheduledJobState m_Reader;
    dmMutex::HMutex m_Mutex;
    dmConditionVariable::HConditionVariable m_Condition;
    uint32_t m_Finished;
    bool m_Release;
};

struct SchedulerAdmissionGate
{
    HDSContext       m_Store;
    ScheduledJobGate m_Gate;
};

static ThreadFixture CreateFixture()
{
    ThreadFixture  fixture = {};
    DSCreateParams params = { .m_PageSize = 1024, .m_RowChunkSize = 256 };
    fixture.m_Store = DSStoreCreate(&params);
    fixture.m_Group = NextTestGroupId();
    DSFieldDesc       light_fields[] = { { .m_Field = COLOR, .m_Type = DS_TYPE_VECTOR3, .m_Name = "color" }, { .m_Field = INTENSITY, .m_Type = DS_TYPE_NUMBER, .m_Offset = 16, .m_Name = "intensity" } };
    DSStructDesc      light = { light_fields, 2, 24 };
    DSFieldDesc       fields[] = { { HEALTH, DS_TYPE_NUMBER, 0, 0 }, { POSITION, DS_TYPE_VECTOR3, 8, 0 }, { .m_Field = LIGHT, .m_Type = DS_TYPE_STRUCT, .m_Offset = 24, .m_Struct = &light, .m_Name = "light" } };
    const DSValueType child_types[] = { DS_TYPE_VECTOR3, DS_TYPE_NUMBER };
    DSValueData       children[2] = {};
    uint64_t          names[] = { COLOR, INTENSITY };
    const DSValueType types[] = { DS_TYPE_NUMBER, DS_TYPE_VECTOR3, DS_TYPE_STRUCT };
    DSValueData       values[3] = {};
    values[0].m_Number = 50;
    values[2].m_Struct.m_Array.m_Names = names;
    values[2].m_Struct.m_Array.m_Types = child_types;
    values[2].m_Struct.m_Array.m_Values = children;
    values[2].m_Struct.m_Count = 2;
    for (uint32_t t = 0; t < 2; ++t)
    {
        fields[0].m_Offset = t ? 16 : 0;
        fields[1].m_Offset = t ? 0 : 8;
        DSTableDesc table = { 100 + t, 0, 0, fields, 3, 48 };
        EXPECT_EQ(DS_RESULT_OK, DSTableAdd(fixture.m_Store, &table));
        DSRowDesc rows[6] = {};
        for (uint32_t r = 0; r < 6; ++r)
        {
            rows[r].m_Group = r % 2 ? fixture.m_Group : 0;
            rows[r].m_Types = types;
            rows[r].m_Values = values;
            rows[r].m_ValueCount = 3;
        }
        EXPECT_EQ(DS_RESULT_OK, DSRowAddBatch(fixture.m_Store, table.m_Type, rows, 6, fixture.m_Ids + t * 6));
    }
    return fixture;
}

static HDSQuery Query(HDSContext store, uint64_t field_hash, DSValueType type, uint32_t access, uint32_t flags = 0)
{
    DSQueryItem field = { .m_Hash = field_hash, .m_Type = type, .m_Access = access, .m_Kind = DS_QUERY_ITEM_FIELD };
    DSQueryDesc desc = { .m_Flags = flags, .m_Items = { field } };
    HDSQuery    query = 0;
    EXPECT_EQ(DS_RESULT_OK, DSQueryCreate(store, &desc, &query));
    return query;
}

struct Admission
{
    HDSQuery m_Query;
    DSResult m_Result;
};

static void TryOnThread(void* arg)
{
    Admission* attempt = (Admission*)arg;
    attempt->m_Result = DSQueryTryBegin(attempt->m_Query);
    if (attempt->m_Result == DS_RESULT_OK)
        DSQueryEnd(attempt->m_Query);
}

static DSResult TryFromWorker(HDSQuery query)
{
    Admission        attempt = { query, DS_RESULT_INVALID_ARGUMENT };
    dmThread::Thread thread = dmThread::New(TryOnThread, 0x80000, &attempt, "data-admission");
    dmThread::Join(thread);
    return attempt.m_Result;
}

TEST(DataThreaded, ConflictsIndependentFieldsAndRelease)
{
    ThreadFixture fixture = CreateFixture();
    HDSQuery      explosion = Query(fixture.m_Store, HEALTH, DS_TYPE_NUMBER, DS_FIELD_MUTABLE);
    HDSQuery      regenerate = Query(fixture.m_Store, HEALTH, DS_TYPE_NUMBER, DS_FIELD_MUTABLE);
    HDSQuery      reader = Query(fixture.m_Store, HEALTH, DS_TYPE_NUMBER, 0);
    HDSQuery      movement = Query(fixture.m_Store, POSITION, DS_TYPE_VECTOR3, DS_FIELD_MUTABLE);
    HDSQuery      queries[] = { explosion, regenerate, reader, movement };
    for (uint32_t first = 0; first < 2; ++first)
    {
        ASSERT_EQ(DS_RESULT_OK, DSQueryTryBegin(queries[first]));
        ASSERT_EQ(DS_RESULT_BUSY, TryFromWorker(queries[1 - first]));
        ASSERT_EQ(DS_RESULT_BUSY, TryFromWorker(reader));
        ASSERT_EQ(DS_RESULT_OK, TryFromWorker(movement));
        ASSERT_EQ(DS_RESULT_LOCKED, DSRowRemove(fixture.m_Store, fixture.m_Ids[0]));
        ASSERT_EQ(DS_RESULT_LOCKED, DSRowReset(fixture.m_Store, fixture.m_Ids[0]));
        ASSERT_EQ(DS_RESULT_LOCKED, DSFieldSetNumber(fixture.m_Store, fixture.m_Ids[0], HEALTH, 1));
        ASSERT_EQ(DS_RESULT_LOCKED, DSQueryDestroy(regenerate));
        ASSERT_EQ(DS_RESULT_BUSY, DSQueryTryBegin(queries[first]));
        DSQueryEnd(queries[first]);
        ASSERT_EQ(DS_RESULT_OK, TryFromWorker(queries[1 - first]));
    }
    ASSERT_EQ(DS_RESULT_OK, DSQueryTryBegin(reader));
    ASSERT_EQ(DS_RESULT_BUSY, TryFromWorker(reader));
    DSQueryItem writable = { .m_Hash = HEALTH, .m_Type = DS_TYPE_NUMBER, .m_Access = DS_FIELD_MUTABLE, .m_Kind = DS_QUERY_ITEM_FIELD };
    ASSERT_EQ(UINT32_MAX, DSQueryFindField(reader, &writable));
    DSIterator it = DSQueryIterRange(reader, 0, 1);
    ASSERT_TRUE(DSIterNext(&it));
    DSRowIterator row = DSIterRows(&it);
    ASSERT_TRUE(DSRowIterNext(&row));
    DSQueryItem readable = { .m_Hash = HEALTH, .m_Type = DS_TYPE_NUMBER, .m_Kind = DS_QUERY_ITEM_FIELD };
    DSFieldId   health = DSQueryFindField(reader, &readable);
    ASSERT_EQ(50.0, *DSRowIterGetNumber(&row, health));
    DSQueryEnd(reader);
    for (uint32_t q = 0; q < 4; ++q)
        ASSERT_EQ(DS_RESULT_OK, DSQueryDestroy(queries[q]));
    ASSERT_EQ(DS_RESULT_OK, DSStoreDestroy(fixture.m_Store));
}

// A worker must observe whole-row reservations even when its field is omitted
// from the other query's filter. Disjoint fields remain admissible across threads.
TEST(DataThreaded, WholeRowReservations)
{
    ThreadFixture fixture = CreateFixture();
    HDSQuery      position_read = Query(fixture.m_Store, POSITION, DS_TYPE_VECTOR3, 0);
    HDSQuery      row_write = Query(fixture.m_Store, POSITION, DS_TYPE_VECTOR3, 0, DS_ROW_MUTABLE);
    HDSQuery      health_read = Query(fixture.m_Store, HEALTH, DS_TYPE_NUMBER, 0);
    HDSQuery      health_write = Query(fixture.m_Store, HEALTH, DS_TYPE_NUMBER, DS_FIELD_MUTABLE);
    ASSERT_EQ(DS_RESULT_OK, DSQueryTryBegin(position_read));
    ASSERT_EQ(DS_RESULT_OK, TryFromWorker(health_read));
    ASSERT_EQ(DS_RESULT_OK, TryFromWorker(health_write));
    ASSERT_EQ(DS_RESULT_BUSY, TryFromWorker(row_write));
    DSQueryEnd(position_read);
    ASSERT_EQ(DS_RESULT_OK, DSQueryTryBegin(row_write));
    ASSERT_EQ(DS_RESULT_BUSY, TryFromWorker(health_read));
    ASSERT_EQ(DS_RESULT_BUSY, TryFromWorker(health_write));
    ASSERT_EQ(DS_RESULT_BUSY, TryFromWorker(position_read));
    DSQueryEnd(row_write);
    ASSERT_EQ(DS_RESULT_OK, DSQueryTryBegin(health_write));
    ASSERT_EQ(DS_RESULT_OK, TryFromWorker(position_read));
    ASSERT_EQ(DS_RESULT_BUSY, TryFromWorker(row_write));
    DSQueryEnd(health_write);
    ASSERT_EQ(DS_RESULT_OK, TryFromWorker(row_write));
    ASSERT_EQ(DS_RESULT_OK, DSQueryDestroy(position_read));
    ASSERT_EQ(DS_RESULT_OK, DSQueryDestroy(row_write));
    ASSERT_EQ(DS_RESULT_OK, DSQueryDestroy(health_read));
    ASSERT_EQ(DS_RESULT_OK, DSQueryDestroy(health_write));
    ASSERT_EQ(DS_RESULT_OK, DSStoreDestroy(fixture.m_Store));
}

TEST(DataThreaded, ReadOnlyAndEmptyQueryReservations)
{
    ThreadFixture fixture = CreateFixture();
    HDSQuery      queries[] = {
        Query(fixture.m_Store, HEALTH, DS_TYPE_NUMBER, 0),
        Query(fixture.m_Store, HEALTH, DS_TYPE_NUMBER, 0),
        Query(fixture.m_Store, 999, DS_TYPE_NUMBER, 0)
    };
    for (uint32_t i = 0; i < 3; ++i)
    {
        ASSERT_EQ(DS_RESULT_OK, DSQueryTryBegin(queries[i]));
        ASSERT_EQ(DS_RESULT_BUSY, DSQueryTryBegin(queries[i]));
        ASSERT_EQ(DS_RESULT_BUSY, TryFromWorker(queries[i]));
        ASSERT_EQ(i == 2 ? 0u : 12u, DSQueryGetRowCount(queries[i]));
    }
    DSIterator empty = DSQueryIterRange(queries[2], 0, 0);
    ASSERT_FALSE(DSIterNext(&empty));

    // Release out of order, retaining other queries' reservations as the pool compacts.
    uint32_t order[] = { 1, 0, 2 };
    for (uint32_t i = 0; i < 3; ++i)
    {
        ASSERT_EQ(DS_RESULT_LOCKED, DSRowRemove(fixture.m_Store, fixture.m_Ids[0]));
        DSQueryEnd(queries[order[i]]);
        ASSERT_EQ(DS_RESULT_OK, TryFromWorker(queries[order[i]]));
    }
    ASSERT_EQ(DS_RESULT_OK, DSRowRemove(fixture.m_Store, fixture.m_Ids[0]));
    for (uint32_t i = 0; i < 3; ++i)
        ASSERT_EQ(DS_RESULT_OK, DSQueryDestroy(queries[i]));
    ASSERT_EQ(DS_RESULT_OK, DSStoreDestroy(fixture.m_Store));
}

TEST(DataThreaded, ActiveQueryPoolGrowthAndReuse)
{
    ThreadFixture fixture = CreateFixture();
    HDSQuery      readers[40];
    for (uint32_t i = 0; i < 40; ++i)
        readers[i] = Query(fixture.m_Store, HEALTH, DS_TYPE_NUMBER, 0);
    HDSQuery writer = Query(fixture.m_Store, HEALTH, DS_TYPE_NUMBER, DS_FIELD_MUTABLE);

    for (uint32_t pass = 0; pass < 2; ++pass)
    {
        for (uint32_t i = 0; i < 40; ++i)
        {
            ASSERT_EQ(DS_RESULT_OK, DSQueryTryBegin(readers[i]));
            ASSERT_EQ(12u, DSQueryGetRowCount(readers[i]));
        }
        // End in a different order from admission, moving live reservations in
        // the dense pool. Reuse each freed slot while the others remain active.
        for (uint32_t i = 0; i < 40; ++i)
        {
            HDSQuery reader = readers[(i * 17) % 40];
            ASSERT_EQ(DS_RESULT_BUSY, DSQueryTryBegin(writer));
            ASSERT_EQ(DS_RESULT_BUSY, DSQueryTryBegin(reader));
            DSQueryEnd(reader);
            ASSERT_EQ(DS_RESULT_OK, DSQueryTryBegin(reader));
            DSQueryEnd(reader);
        }
        ASSERT_EQ(DS_RESULT_OK, TryFromWorker(writer));
    }
    for (uint32_t i = 0; i < 40; ++i)
        ASSERT_EQ(DS_RESULT_OK, DSQueryDestroy(readers[i]));
    ASSERT_EQ(DS_RESULT_OK, DSQueryDestroy(writer));
    ASSERT_EQ(DS_RESULT_OK, DSStoreDestroy(fixture.m_Store));
}

TEST(DataThreaded, NestedRangesAndWholeRows)
{
    ThreadFixture fixture = CreateFixture();
    HDSQuery      parent = Query(fixture.m_Store, LIGHT, DS_TYPE_STRUCT, 0);
    HDSQuery      child = Query(fixture.m_Store, dmHashString64("light.color"), DS_TYPE_VECTOR3, DS_FIELD_MUTABLE);
    HDSQuery      sibling = Query(fixture.m_Store, dmHashString64("light.intensity"), DS_TYPE_NUMBER, DS_FIELD_MUTABLE);
    HDSQuery      whole;
    DSQueryDesc   desc = { .m_Flags = DS_ROW_MUTABLE };
    ASSERT_EQ(DS_RESULT_OK, DSQueryCreate(fixture.m_Store, &desc, &whole));
    ASSERT_EQ(DS_RESULT_OK, DSQueryTryBegin(child));
    ASSERT_EQ(DS_RESULT_BUSY, TryFromWorker(parent));
    ASSERT_EQ(DS_RESULT_BUSY, TryFromWorker(whole));
    ASSERT_EQ(DS_RESULT_OK, TryFromWorker(sibling));
    DSQueryEnd(child);
    HDSQuery queries[] = { parent, child, sibling, whole };
    for (uint32_t q = 0; q < 4; ++q)
        ASSERT_EQ(DS_RESULT_OK, DSQueryDestroy(queries[q]));
    ASSERT_EQ(DS_RESULT_OK, DSStoreDestroy(fixture.m_Store));
}

TEST(DataThreaded, RangesCoverOwnerFilteredRowsAndRefreshAfterMutation)
{
    ThreadFixture fixture = CreateFixture();
    DSId          group = fixture.m_Group;
    DSQueryItem   field = { .m_Hash = HEALTH, .m_Type = DS_TYPE_NUMBER, .m_Kind = DS_QUERY_ITEM_FIELD };
    DSQueryDesc   desc = { .m_GroupIds = &group, .m_GroupIdCount = 1, .m_Items = { field } };
    HDSQuery      query;
    ASSERT_EQ(DS_RESULT_OK, DSQueryCreate(fixture.m_Store, &desc, &query));
    for (uint32_t phase = 0; phase < 2; ++phase)
    {
        ASSERT_EQ(DS_RESULT_OK, DSQueryTryBegin(query));
        uint32_t count = DSQueryGetRowCount(query);
        ASSERT_EQ(6u - phase, count);
        bool seen[12] = {};
        for (uint32_t first = 0; first < count; first += 2)
        {
            DSIterator it = DSQueryIterRange(query, first, count - first < 2 ? count - first : 2);
            while (DSIterNext(&it))
            {
                DSRowIterator rows = DSIterRows(&it);
                while (DSRowIterNext(&rows))
                {
                    ASSERT_EQ(group, DSRowIterGetGroupId(&rows));
                    DSId     id = DSRowIterGetId(&rows);
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
        DSIterator empty = DSQueryIterRange(query, count, 0);
        ASSERT_FALSE(DSIterNext(&empty));
        DSQueryEnd(query);
        if (!phase)
            ASSERT_EQ(DS_RESULT_OK, DSRowRemove(fixture.m_Store, fixture.m_Ids[1]));
    }
    ASSERT_EQ(DS_RESULT_OK, DSQueryDestroy(query));
    ASSERT_EQ(DS_RESULT_OK, DSStoreDestroy(fixture.m_Store));
}

struct MutationAttempt
{
    HDSContext m_Store;
    DSId       m_Id;
    DSId       m_Group;
    DSResult   m_Remove, m_RemoveGroup, m_CreateRows, m_CreateQuery, m_DestroyStore;
};

static void MutateOnThread(void* context)
{
    MutationAttempt* attempt = (MutationAttempt*)context;
    attempt->m_Remove = DSRowRemove(attempt->m_Store, attempt->m_Id);
    attempt->m_RemoveGroup = DSGroupRemove(attempt->m_Store, attempt->m_Group);
    uint64_t values[6] = {};
    DSId     id;
    attempt->m_CreateRows = DSRowCreate(attempt->m_Store, 100, attempt->m_Group, 1, values, &id);
    HDSQuery    query = 0;
    DSQueryDesc desc = {};
    attempt->m_CreateQuery = DSQueryCreate(attempt->m_Store, &desc, &query);
    attempt->m_DestroyStore = DSStoreDestroy(attempt->m_Store);
}

// Disjoint groups may reserve the same field, while structural mutation remains excluded.
TEST(DataThreaded, MutationExclusionAndDisjointGroups)
{
    ThreadFixture fixture = CreateFixture();
    DSQueryItem   field = { .m_Hash = HEALTH, .m_Type = DS_TYPE_NUMBER, .m_Access = DS_FIELD_MUTABLE, .m_Kind = DS_QUERY_ITEM_FIELD };
    DSId          groups[] = { 0, fixture.m_Group };
    HDSQuery      queries[2];
    for (uint32_t i = 0; i < 2; ++i)
    {
        DSQueryDesc desc = { .m_GroupIds = &groups[i], .m_GroupIdCount = 1, .m_Items = { field } };
        ASSERT_EQ(DS_RESULT_OK, DSQueryCreate(fixture.m_Store, &desc, &queries[i]));
    }
    ASSERT_EQ(DS_RESULT_OK, DSQueryTryBegin(queries[0]));
    ASSERT_EQ(DS_RESULT_OK, TryFromWorker(queries[1]));
    MutationAttempt  attempt = { .m_Store = fixture.m_Store, .m_Id = fixture.m_Ids[0], .m_Group = fixture.m_Group };
    dmThread::Thread worker = dmThread::New(MutateOnThread, 0x80000, &attempt, "data-mutate");
    dmThread::Join(worker);
    ASSERT_EQ(DS_RESULT_LOCKED, attempt.m_Remove);
    ASSERT_EQ(DS_RESULT_LOCKED, attempt.m_RemoveGroup);
    ASSERT_EQ(DS_RESULT_LOCKED, attempt.m_CreateRows);
    ASSERT_EQ(DS_RESULT_LOCKED, attempt.m_CreateQuery);
    ASSERT_EQ(DS_RESULT_LOCKED, attempt.m_DestroyStore);
    DSQueryEnd(queries[0]);
    ASSERT_EQ(DS_RESULT_OK, DSRowRemove(fixture.m_Store, fixture.m_Ids[0]));
    for (uint32_t i = 0; i < 2; ++i)
        ASSERT_EQ(DS_RESULT_OK, DSQueryDestroy(queries[i]));
    DSQueryItem invalid = { .m_Hash = LIGHT, .m_Type = DS_TYPE_STRUCT, .m_Access = DS_FIELD_MUTABLE, .m_Kind = DS_QUERY_ITEM_FIELD };
    DSQueryDesc desc = { .m_Items = { invalid } };
    HDSQuery    query = 0;
    ASSERT_EQ(DS_RESULT_INVALID_ARGUMENT, DSQueryCreate(fixture.m_Store, &desc, &query));
    ASSERT_EQ(DS_RESULT_OK, DSStoreDestroy(fixture.m_Store));
}

struct AdmissionRace
{
    dmMutex::HMutex                         m_Mutex;
    dmConditionVariable::HConditionVariable m_Condition;
    HDSQuery                                m_Query;
    uint32_t                                m_Ready, m_Attempted;
    bool                                    m_Start;
};

struct AdmissionWorker
{
    AdmissionRace* m_Race;
    DSResult       m_Result;
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
    worker->m_Result = DSQueryTryBegin(race->m_Query);
    {
        DM_MUTEX_SCOPED_LOCK(race->m_Mutex);
        ++race->m_Attempted;
        dmConditionVariable::Broadcast(race->m_Condition);
        // The successful begin retains access until both callers have attempted.
        while (race->m_Attempted != 2)
            dmConditionVariable::Wait(race->m_Condition, race->m_Mutex);
    }
    if (worker->m_Result == DS_RESULT_OK)
        DSQueryEnd(race->m_Query);
}

TEST(DataThreaded, SimultaneousBeginsAdmitExactlyOne)
{
    ThreadFixture fixture = CreateFixture();
    HDSQuery      queries[] = {
        Query(fixture.m_Store, HEALTH, DS_TYPE_NUMBER, DS_FIELD_MUTABLE),
        Query(fixture.m_Store, HEALTH, DS_TYPE_NUMBER, 0)
    };
    for (uint32_t pass = 0; pass < 32; ++pass)
    {
        AdmissionRace race = {};
        race.m_Mutex = dmMutex::New();
        race.m_Condition = dmConditionVariable::New();
        race.m_Query = queries[pass % 2];
        AdmissionWorker  workers[] = { { &race, DS_RESULT_INVALID_ARGUMENT }, { &race, DS_RESULT_INVALID_ARGUMENT } };
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
        ASSERT_TRUE((workers[0].m_Result == DS_RESULT_OK && workers[1].m_Result == DS_RESULT_BUSY) ||
                    (workers[1].m_Result == DS_RESULT_OK && workers[0].m_Result == DS_RESULT_BUSY));
        dmConditionVariable::Delete(race.m_Condition);
        dmMutex::Delete(race.m_Mutex);
    }
    for (uint32_t i = 0; i < 2; ++i)
    {
        ASSERT_EQ(DS_RESULT_OK, TryFromWorker(queries[i]));
        ASSERT_EQ(DS_RESULT_OK, DSQueryDestroy(queries[i]));
    }
    ASSERT_EQ(DS_RESULT_OK, DSStoreDestroy(fixture.m_Store));
}

struct RangeWork
{
    HDSQuery m_Query;
    uint32_t  m_First, m_Count;
    DSFieldId         m_Field;
    uint32_t m_Visited;
};

static int32_t WriteRange(HJobContext, HJob, void*, void* data)
{
    RangeWork* work = (RangeWork*)data;
    DSIterator it = DSQueryIterRange(work->m_Query, work->m_First, work->m_Count);
    while (DSIterNext(&it))
    {
        DSRowIterator rows = DSIterRows(&it);
        while (DSRowIterNext(&rows))
        {
            *DSRowIterGetNumberMut(&rows, work->m_Field) += 1;
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

static void ReadScheduledHealth(HDSQuery query, uint32_t first, uint32_t count, void* context)
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
            dmConditionVariable::Wait(gate->m_Condition, gate->m_Mutex);
    }
    DSIterator it = DSQueryIterRange(query, first, count);
    uint32_t   index = first;
    while (DSIterNext(&it))
    {
        DSRowIterator rows = DSIterRows(&it);
        while (DSRowIterNext(&rows))
            state->m_Values[index++] = *DSRowIterGetNumber(&rows, state->m_Field);
    }
}

static void RegenerateScheduledHealth(HDSQuery query, uint32_t first, uint32_t count, void* context)
{
    ScheduledJobState* state = (ScheduledJobState*)context;
    dmAtomicIncrement32(&state->m_Calls);
    DSIterator it = DSQueryIterRange(query, first, count);
    while (DSIterNext(&it))
    {
        DSRowIterator rows = DSIterRows(&it);
        while (DSRowIterNext(&rows))
            *DSRowIterGetNumberMut(&rows, state->m_Field) += 1;
    }
}

static void ReadScheduledRange(HDSQuery query, uint32_t first, uint32_t count, void* context)
{
    ScheduledRangeState* state = (ScheduledRangeState*)context;
    if (!first)
    {
        DM_MUTEX_SCOPED_LOCK(state->m_Mutex);
        while (!state->m_Release)
            dmConditionVariable::Wait(state->m_Condition, state->m_Mutex);
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
    ScheduledRangeState ranges = { .m_Mutex = dmMutex::New(), .m_Condition = dmConditionVariable::New() };
    ScheduledJobState   observer = {}, writer = {};
    HDSQuery            queries[] = {
        Query(fixture.m_Store, HEALTH, DS_TYPE_NUMBER, 0),
        Query(fixture.m_Store, HEALTH, DS_TYPE_NUMBER, 0),
        Query(fixture.m_Store, HEALTH, DS_TYPE_NUMBER, DS_FIELD_MUTABLE)
    };
    DSQueryItem field = { .m_Hash = HEALTH, .m_Type = DS_TYPE_NUMBER, .m_Kind = DS_QUERY_ITEM_FIELD };
    ranges.m_Reader.m_Field = DSQueryFindField(queries[0], &field);
    observer.m_Field = DSQueryFindField(queries[1], &field);
    field.m_Access = DS_FIELD_MUTABLE;
    writer.m_Field = DSQueryFindField(queries[2], &field);
    DSSystemDesc systems[] = {
        { .m_Name = dmHashString64("observe"), .m_DependsOn = { dmHashString64("read_ranges") }, .m_Query = queries[1], .m_Process = ReadScheduledHealth, .m_Context = &observer },
        { .m_Name = dmHashString64("read_ranges"), .m_Query = queries[0], .m_Process = ReadScheduledRange, .m_Context = &ranges, .m_RowsPerJob = 5 },
        { .m_Name = dmHashString64("regenerate"), .m_Query = queries[2], .m_Process = RegenerateScheduledHealth, .m_Context = &writer, .m_RowsPerJob = 5 }
    };
    for (uint32_t i = 0; i < 3; ++i)
        ASSERT_EQ(DS_RESULT_OK, DSFunctionAdd(fixture.m_Store, &systems[i]));
    JobSystemCreateParams params = { .m_ThreadNamePrefix = "ds-ranges", .m_ThreadCount = 2 };
    HJobContext           jobs = JobSystemCreate(&params);
    for (uint32_t frame = 0; frame < 20; ++frame)
    {
        ranges.m_Finished = 0;
        ranges.m_Release = false;
        ASSERT_EQ(DS_RESULT_OK, DSSchedulerBegin(fixture.m_Store, jobs));
        {
            DM_MUTEX_SCOPED_LOCK(ranges.m_Mutex);
            while (ranges.m_Finished != 2)
                dmConditionVariable::Wait(ranges.m_Condition, ranges.m_Mutex);
        }
        EXPECT_EQ(DS_RESULT_BUSY, DSSchedulerUpdate(fixture.m_Store));
        EXPECT_EQ((int32_t)(3 * frame + 2), dmAtomicGet32(&ranges.m_Reader.m_Calls));
        EXPECT_EQ((int32_t)frame, dmAtomicGet32(&observer.m_Calls));
        EXPECT_EQ((int32_t)(3 * frame), dmAtomicGet32(&writer.m_Calls));
        EXPECT_EQ(DS_RESULT_BUSY, DSQueryTryBegin(queries[2]));
        EXPECT_EQ(DS_RESULT_BUSY, DSSchedulerEnd(fixture.m_Store));
        EXPECT_EQ(DS_RESULT_LOCKED, DSRowRemove(fixture.m_Store, fixture.m_Ids[0]));
        {
            DM_MUTEX_SCOPED_LOCK(ranges.m_Mutex);
            ranges.m_Release = true;
            dmConditionVariable::Signal(ranges.m_Condition);
        }
        DSResult result;
        do
        {
            result = DSSchedulerUpdate(fixture.m_Store);
        } while (result == DS_RESULT_BUSY);
        ASSERT_EQ(DS_RESULT_OK, result);
        ASSERT_EQ(DS_RESULT_OK, DSSchedulerEnd(fixture.m_Store));
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
        ASSERT_EQ(DS_RESULT_OK, DSQueryDestroy(queries[i]));
    ASSERT_EQ(DS_RESULT_OK, DSStoreDestroy(fixture.m_Store));
}

// One event launches both readers on workers. Their reservations exclude the writer;
// completion automatically launches it and its observer without per-job caller dispatch.
TEST(DataThreaded, SchedulerAutomaticJobs)
{
    ThreadFixture     fixture = CreateFixture();
    ScheduledJobGate  gate = { .m_Mutex = dmMutex::New(), .m_Condition = dmConditionVariable::New() };
    ScheduledJobState states[4] = { { .m_Gate = &gate }, { .m_Gate = &gate } };
    DSSystemDesc      systems[] = {
        { .m_Name = dmHashString64("read_first"), .m_DependsOn = { dmHashString64("engine.update.begin") }, .m_Process = ReadScheduledHealth, .m_Context = &states[0] },
        { .m_Name = dmHashString64("read_second"), .m_DependsOn = { dmHashString64("engine.update.begin") }, .m_Process = ReadScheduledHealth, .m_Context = &states[1] },
        { .m_Name = dmHashString64("regenerate"), .m_DependsOn = { dmHashString64("engine.update.begin") }, .m_Process = RegenerateScheduledHealth, .m_Context = &states[2] },
        { .m_Name = dmHashString64("observe"), .m_DependsOn = { dmHashString64("regenerate") }, .m_Process = ReadScheduledHealth, .m_Context = &states[3] }
    };
    for (uint32_t i = 0; i < 4; ++i)
    {
        uint32_t access = i == 2 ? DS_FIELD_MUTABLE : 0;
        systems[i].m_Query = Query(fixture.m_Store, HEALTH, DS_TYPE_NUMBER, access);
        DSQueryItem field = { .m_Hash = HEALTH, .m_Type = DS_TYPE_NUMBER, .m_Access = access, .m_Kind = DS_QUERY_ITEM_FIELD };
        states[i].m_Field = DSQueryFindField(systems[i].m_Query, &field);
        ASSERT_EQ(DS_RESULT_OK, DSFunctionAdd(fixture.m_Store, &systems[i]));
    }
    DSEventDesc begin = { .m_Name = dmHashString64("engine.update.begin") };
    DSEventDesc end = { .m_Name = dmHashString64("engine.update.end"), .m_DependsOn = { dmHashString64("observe") } };
    ASSERT_EQ(DS_RESULT_OK, DSEventAdd(fixture.m_Store, &begin));
    ASSERT_EQ(DS_RESULT_OK, DSEventAdd(fixture.m_Store, &end));
    JobSystemCreateParams params = { .m_ThreadNamePrefix = "ds-schedule", .m_ThreadCount = 4 };
    HJobContext           jobs = JobSystemCreate(&params);
    for (uint32_t frame = 0; frame < 20; ++frame)
    {
        gate.m_Started = 0;
        gate.m_Release = false;
        ASSERT_EQ(DS_RESULT_OK, DSSchedulerBegin(fixture.m_Store, jobs));
        ASSERT_EQ(DS_RESULT_BUSY, DSSchedulerUpdate(fixture.m_Store));
        ASSERT_EQ(DS_RESULT_BUSY, DSSchedulerEvent(fixture.m_Store, end.m_Name));
        ASSERT_EQ(DS_RESULT_OK, DSSchedulerEvent(fixture.m_Store, begin.m_Name));
        {
            // Both readers must reach the gate before checking exclusion;
            // slow worker startup must not change the result.
            DM_MUTEX_SCOPED_LOCK(gate.m_Mutex);
            while (gate.m_Started != 2)
                dmConditionVariable::Wait(gate.m_Condition, gate.m_Mutex);
        }
        ASSERT_EQ((int32_t)frame, dmAtomicGet32(&states[2].m_Calls));
        ASSERT_EQ((int32_t)frame, dmAtomicGet32(&states[3].m_Calls));
        ASSERT_EQ(DS_RESULT_BUSY, DSSchedulerUpdate(fixture.m_Store));
        ASSERT_EQ(DS_RESULT_BUSY, DSSchedulerEnd(fixture.m_Store));
        ASSERT_EQ(DS_RESULT_LOCKED, DSRowRemove(fixture.m_Store, fixture.m_Ids[0]));
        {
            DM_MUTEX_SCOPED_LOCK(gate.m_Mutex);
            gate.m_Release = true;
            dmConditionVariable::Broadcast(gate.m_Condition);
        }
        DSResult result;
        do
        {
            result = DSSchedulerUpdate(fixture.m_Store);
        } while (result == DS_RESULT_BUSY);
        ASSERT_EQ(DS_RESULT_OK, result);
        ASSERT_EQ(DS_RESULT_OK, DSSchedulerEvent(fixture.m_Store, end.m_Name));
        ASSERT_EQ(DS_RESULT_OK, DSSchedulerEnd(fixture.m_Store));
        for (uint32_t i = 0; i < 4; ++i)
            ASSERT_EQ((int32_t)frame + 1, dmAtomicGet32(&states[i].m_Calls));
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
        ASSERT_EQ(DS_RESULT_OK, DSQueryDestroy(systems[i].m_Query));
    ASSERT_EQ(DS_RESULT_OK, DSStoreDestroy(fixture.m_Store));
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
        dmConditionVariable::Wait(gate->m_Condition, gate->m_Mutex);
}

// Temporary store-mutex contention must not let the engine end a schedule before
// a ready callback runs. Condition signals control contention without time limits.
TEST(DataThreaded, SchedulerRetriesAdmissionBeforeEnd)
{
    ThreadFixture     fixture = CreateFixture();
    HDSQuery          query = Query(fixture.m_Store, HEALTH, DS_TYPE_NUMBER, 0);
    DSQueryItem       field = { .m_Hash = HEALTH, .m_Type = DS_TYPE_NUMBER, .m_Kind = DS_QUERY_ITEM_FIELD };
    ScheduledJobState state = { .m_Field = DSQueryFindField(query, &field) };
    DSEventDesc       event = { .m_Name = DS_EVENT_GAMEOBJECT_PRE_UPDATE };
    DSSystemDesc      function = { .m_Name = dmHashString64("read"), .m_DependsOn = { event.m_Name }, .m_Query = query, .m_Process = ReadScheduledHealth, .m_Context = &state };
    ASSERT_EQ(DS_RESULT_OK, DSEventAdd(fixture.m_Store, &event));
    ASSERT_EQ(DS_RESULT_OK, DSFunctionAdd(fixture.m_Store, &function));
    JobSystemCreateParams  params = { .m_ThreadNamePrefix = "ds-admit", .m_ThreadCount = 0 };
    HJobContext            jobs = JobSystemCreate(&params);
    SchedulerAdmissionGate gate = { .m_Store = fixture.m_Store, .m_Gate = { .m_Mutex = dmMutex::New(), .m_Condition = dmConditionVariable::New() } };

    ASSERT_EQ(DS_RESULT_OK, DSSchedulerBegin(fixture.m_Store, jobs));
    dmThread::Thread worker = dmThread::New(HoldSchedulerAdmission, 0x80000, &gate, "ds-admit");
    {
        DM_MUTEX_SCOPED_LOCK(gate.m_Gate.m_Mutex);
        while (!gate.m_Gate.m_Started)
            dmConditionVariable::Wait(gate.m_Gate.m_Condition, gate.m_Gate.m_Mutex);
    }
    DSResult result = DSSchedulerEvent(fixture.m_Store, event.m_Name);
    {
        DM_MUTEX_SCOPED_LOCK(gate.m_Gate.m_Mutex);
        gate.m_Gate.m_Release = true;
        dmConditionVariable::Signal(gate.m_Gate.m_Condition);
    }
    dmThread::Join(worker);
    ASSERT_EQ(DS_RESULT_OK, result);
    ASSERT_EQ(0, dmAtomicGet32(&state.m_Calls));
    ASSERT_EQ(DS_RESULT_BUSY, DSSchedulerEnd(fixture.m_Store));
    while (DSSchedulerEnd(fixture.m_Store) == DS_RESULT_BUSY)
        DSSchedulerUpdate(fixture.m_Store);
    ASSERT_EQ(1, dmAtomicGet32(&state.m_Calls));
    for (uint32_t i = 0; i < DM_ARRAY_SIZE(state.m_Values); ++i)
        ASSERT_EQ(50.0, state.m_Values[i]);

    dmConditionVariable::Delete(gate.m_Gate.m_Condition);
    dmMutex::Delete(gate.m_Gate.m_Mutex);
    JobSystemDestroy(jobs);
    ASSERT_EQ(DS_RESULT_OK, DSQueryDestroy(query));
    ASSERT_EQ(DS_RESULT_OK, DSStoreDestroy(fixture.m_Store));
}

TEST(DataThreaded, JobRangesWithOneTwoFourEightWorkers)
{
    for (uint32_t workers = 1; workers <= 8; workers *= 2)
    {
        ThreadFixture         fixture = CreateFixture();
        HDSQuery              query = Query(fixture.m_Store, HEALTH, DS_TYPE_NUMBER, DS_FIELD_MUTABLE);
        DSQueryItem           requested_field = { .m_Hash = HEALTH, .m_Type = DS_TYPE_NUMBER, .m_Access = DS_FIELD_MUTABLE, .m_Kind = DS_QUERY_ITEM_FIELD };
        DSFieldId             field = DSQueryFindField(query, &requested_field);
        JobSystemCreateParams params = { "data-test", (uint8_t)workers };
        HJobContext           jobs = JobSystemCreate(&params);
        for (uint32_t pass = 0; pass < 20; ++pass)
        {
            ASSERT_EQ(DS_RESULT_OK, DSQueryTryBegin(query));
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
            DSQueryEnd(query);
        }
        JobSystemDestroy(jobs);
        for (uint32_t i = 0; i < 12; ++i)
        {
            double value;
            ASSERT_EQ(DS_RESULT_OK, DSFieldGetNumber(fixture.m_Store, fixture.m_Ids[i], HEALTH, &value));
            ASSERT_EQ(70.0, value);
        }
        ASSERT_EQ(DS_RESULT_OK, DSQueryDestroy(query));
        ASSERT_EQ(DS_RESULT_OK, DSStoreDestroy(fixture.m_Store));
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
    DSId         ids[73];
    for (uint32_t i = 0; i < 73; ++i)
        ids[i] = reader->m_Fixture->m_Ids[i % 12];
    for (uint32_t pass = 0; pass < 100; ++pass)
    {
        double    health[73];
        DSVector3 positions[73];
        if (DSFieldGetNumberBatch(reader->m_Fixture->m_Store, 73, ids, HEALTH, health) != DS_RESULT_OK ||
            DSFieldGetVector3Batch(reader->m_Fixture->m_Store, 73, ids, POSITION, positions) != DS_RESULT_OK)
        {
            ++reader->m_Errors;
            return;
        }
        for (uint32_t i = 0; i < 73; ++i)
            reader->m_Errors += health[i] != 50 || positions[i].m_Values[0] != 0 || positions[i].m_Values[1] != 0 || positions[i].m_Values[2] != 0;
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
    ASSERT_EQ(DS_RESULT_OK, DSStoreDestroy(fixture.m_Store));
}

static void ReadStructRows(void* context)
{
    StructReadTask* task = (StructReadTask*)context;
    for (uint32_t repeat = 0; repeat < 100; ++repeat)
    {
        DSIterator iterator = DSQueryIterRange(task->m_Query, task->m_First, task->m_Count);
        while (DSIterNext(&iterator))
        {
            DSRowIterator rows = DSIterRows(&iterator);
            while (DSRowIterNext(&rows))
            {
                DSValueView object, value;
                DSRowIterGetView(&rows, task->m_Field, &object);
                task->m_Result = DSViewGetField(&object, HEALTH, &value);
                if (task->m_Result != DS_RESULT_OK)
                    return;
                double health;
                task->m_Result = DSViewGetNumber(&value, &health);
                if (task->m_Result != DS_RESULT_OK)
                    return;
                task->m_Total += health;
            }
        }
    }
}

// Public container views must remain stable while separate jobs read child rows.
TEST(DataThreaded, OwnedStructRowsRemainStableDuringQueryJobs)
{
    HDSContext  store = DSStoreCreate(0);
    DSFieldDesc field = { .m_Field = 10, .m_Type = DS_TYPE_STRUCT };
    DSTableDesc desc = { .m_Type = 1, .m_Fields = &field, .m_FieldCount = 1, .m_RowStride = 8 };
    ASSERT_EQ(DS_RESULT_OK, DSTableAdd(store, &desc));
    DSFieldDesc   health = { .m_Field = HEALTH, .m_Type = DS_TYPE_NUMBER };
    DSStructDesc  layout = { .m_Fields = &health, .m_FieldCount = 1, .m_Size = 8 };
    double        value = 10;
    DSStructInput input = { .m_Layout = &layout, .m_Values = &value };
    DSReference   rows[128];
    DSId          ids[128];
    for (uint32_t i = 0; i < 128; ++i)
        rows[i].m_Struct = &input;
    ASSERT_EQ(DS_RESULT_OK, DSRowCreate(store, 1, 0, 128, rows, ids));
    HDSQuery    query = Query(store, 10, DS_TYPE_STRUCT, 0);
    DSQueryItem requested = { .m_Hash = 10, .m_Type = DS_TYPE_STRUCT, .m_Kind = DS_QUERY_ITEM_FIELD };
    DSFieldId   handle = DSQueryFindField(query, &requested);
    ASSERT_EQ(DS_RESULT_OK, DSQueryTryBegin(query));
    StructReadTask   tasks[4];
    dmThread::Thread threads[4];
    for (uint32_t i = 0; i < 4; ++i)
    {
        tasks[i] = { .m_Query = query, .m_First = i * 32, .m_Count = 32, .m_Field = handle };
        threads[i] = dmThread::New(ReadStructRows, 0x80000, &tasks[i], "data-struct-read");
    }
    ASSERT_EQ(DS_RESULT_LOCKED, DSRowRemove(store, ids[0]));
    ASSERT_EQ(DS_RESULT_LOCKED, DSRowReset(store, ids[0]));
    ASSERT_EQ(DS_RESULT_LOCKED, DSRowCreate(store, 1, 0, 128, rows, 0));
    for (uint32_t i = 0; i < 4; ++i)
    {
        dmThread::Join(threads[i]);
        ASSERT_EQ(DS_RESULT_OK, tasks[i].m_Result);
        ASSERT_EQ(32000.0, tasks[i].m_Total);
    }
    DSQueryEnd(query);
    ASSERT_EQ(DS_RESULT_OK, DSQueryDestroy(query));
    for (uint32_t i = 0; i < 128; ++i)
        ASSERT_EQ(DS_RESULT_OK, DSRowRemove(store, ids[i]));
    ASSERT_EQ(0u, FindTable(store, 1)->m_Structs->m_Tables[0]->m_RowCount);
    ASSERT_EQ(DS_RESULT_OK, DSStoreDestroy(store));
}

static void ExerciseBlobLifetime(void* context)
{
    BlobLifetimeTask* task = (BlobLifetimeTask*)context;
    HDSContext        store = DSStoreCreate(0);
    {
        DM_MUTEX_SCOPED_LOCK(task->m_Mutex);
        ++*task->m_Ready;
        dmConditionVariable::Broadcast(task->m_Condition);
        while (!*task->m_Start)
            dmConditionVariable::Wait(task->m_Condition, task->m_Mutex);
    }
    for (uint32_t i = 0; i < 1000; ++i)
    {
        HDSBlobInstance instance;
        task->m_Result = DSBlobAdd(store, task->m_Blob, 0, &instance);
        if (task->m_Result != DS_RESULT_OK)
            break;
        task->m_Result = DSBlobRemove(instance);
        if (task->m_Result != DS_RESULT_OK)
            break;
    }
    DSResult result = DSStoreDestroy(store);
    if (task->m_Result == DS_RESULT_OK)
        task->m_Result = result;
}

// Each store has a different mutex. Retaining/releasing one immutable resource
// from all of them must synchronize at the blob handle, including pool teardown.
TEST(DataThreaded, SharedBlobLifetimeAcrossStores)
{
    ThreadFixture source = CreateFixture();
    uint32_t      size;
    ASSERT_EQ(DS_RESULT_OK, DSBlobWrite(source.m_Store, 0, 0, &size));
    uint8_t* bytes = new uint8_t[size];
    ASSERT_EQ(DS_RESULT_OK, DSBlobWrite(source.m_Store, bytes, size, &size));
    HDSBlob blob;
    ASSERT_EQ(DS_RESULT_OK, DSBlobLoad(bytes, size, &blob));
    dmMutex::HMutex                         mutex = dmMutex::New();
    dmConditionVariable::HConditionVariable condition = dmConditionVariable::New();
    uint32_t                                ready = 0;
    bool                                    start = false;
    BlobLifetimeTask                        tasks[8];
    dmThread::Thread                        threads[8];
    for (uint32_t i = 0; i < 8; ++i)
    {
        tasks[i] = { .m_Blob = blob, .m_Result = DS_RESULT_OK, .m_Mutex = mutex, .m_Condition = condition, .m_Ready = &ready, .m_Start = &start };
        threads[i] = dmThread::New(ExerciseBlobLifetime, 0x80000, &tasks[i], "data-blob-lifetime");
    }
    {
        DM_MUTEX_SCOPED_LOCK(mutex);
        while (ready != 8)
            dmConditionVariable::Wait(condition, mutex);
        start = true;
        dmConditionVariable::Broadcast(condition);
    }
    for (uint32_t i = 0; i < 8; ++i)
    {
        dmThread::Join(threads[i]);
        ASSERT_EQ(DS_RESULT_OK, tasks[i].m_Result);
    }
    ASSERT_EQ(1, dmAtomicGet32(&blob->m_RefCount));
    DSBlobDestroy(blob);
    delete[] bytes;
    dmConditionVariable::Delete(condition);
    dmMutex::Delete(mutex);
    ASSERT_EQ(DS_RESULT_OK, DSStoreDestroy(source.m_Store));
}

// Independent group writers run simultaneously, each split into separate jobs.
TEST(DataThreaded, DisjointGroupJobs)
{
    ThreadFixture fixture = CreateFixture();
    DSId          groups[] = { 0, fixture.m_Group };
    HDSQuery      queries[2];
    uint32_t      handles[2];
    DSQueryItem   field = { .m_Hash = HEALTH, .m_Type = DS_TYPE_NUMBER, .m_Access = DS_FIELD_MUTABLE, .m_Kind = DS_QUERY_ITEM_FIELD };
    for (uint32_t i = 0; i < 2; ++i)
    {
        DSQueryDesc desc = { .m_GroupIds = groups + i, .m_GroupIdCount = 1, .m_Items = { field } };
        ASSERT_EQ(DS_RESULT_OK, DSQueryCreate(fixture.m_Store, &desc, queries + i));
        handles[i] = DSQueryFindField(queries[i], &field);
    }
    JobSystemCreateParams params = { "data-groups", 4 };
    HJobContext           jobs = JobSystemCreate(&params);
    for (uint32_t pass = 0; pass < 20; ++pass)
    {
        ASSERT_EQ(DS_RESULT_OK, DSQueryTryBegin(queries[0]));
        ASSERT_EQ(DS_RESULT_OK, DSQueryTryBegin(queries[1]));
        uint32_t  completed = 0;
        RangeWork work[4];
        for (uint32_t i = 0; i < 4; ++i)
        {
            uint32_t group = i / 2;
            work[i] = { .m_Query = queries[group], .m_First = (i % 2) * 3, .m_Count = 3, .m_Field = handles[group] };
            Job  job = { WriteRange, RangeDone, &completed, work + i };
            HJob handle = JobSystemCreateJob(jobs, &job);
            ASSERT_EQ(JOBSYSTEM_RESULT_OK, JobSystemPushJob(jobs, handle));
        }
        while (completed != 4)
            JobSystemUpdate(jobs, 0);
        DSQueryEnd(queries[0]);
        DSQueryEnd(queries[1]);
    }
    JobSystemDestroy(jobs);
    for (uint32_t i = 0; i < 12; ++i)
    {
        double health;
        ASSERT_EQ(DS_RESULT_OK, DSFieldGetNumber(fixture.m_Store, fixture.m_Ids[i], HEALTH, &health));
        ASSERT_EQ(70.0, health);
    }
    ASSERT_EQ(DS_RESULT_OK, DSQueryDestroy(queries[0]));
    ASSERT_EQ(DS_RESULT_OK, DSQueryDestroy(queries[1]));
    ASSERT_EQ(DS_RESULT_OK, DSStoreDestroy(fixture.m_Store));
}
