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

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dlib/time.h>
#include "../data.h"
#include "benchmark_data_threaded.h"
#include "benchmark_data_threaded_memory.h"

struct ContentGroup
{
    HDataBlobInstance m_Instance;
};

struct ThreadedFixture
{
    HDataStore            m_Store;
    uint8_t*              m_Bytes[2];
    uint32_t              m_ByteCount[2];
    HDataBlob             m_Prototype[2];
    HDataQuery            m_Validation;
    uint32_t              m_PositionField;
    ThreadedReferenceRow  m_Defaults[2][THREAD_BUNDLE_ROWS];
    ThreadedReferenceRow* m_Reference;
    ContentGroup*         m_Groups;
    uint32_t*             m_Live[2];
    uint32_t              m_LiveCount[2], m_Issued, m_Capacity;
};

static DataValue Number(double value)
{
    DataValue out = { .m_Type = DATA_VALUE_TYPE_NUMBER, .m_Value = { .m_Number = value } };
    return out;
}

static DataValue Vector(const DataVector3& value)
{
    DataValue out = { .m_Type = DATA_VALUE_TYPE_VECTOR3 };
    memcpy(out.m_Value.m_Vector3, value.m_Values, sizeof(value));
    return out;
}

// Each serialized bundle contains 100 rows: either six tables in the fixture's
// 10/15/1/24/25/25 proportions, or one Enemy table for the additional waves.
static void BuildPrototype(ThreadedFixture* fixture, bool enemies)
{
    HDataStore    source = DataCreateStore();
    DataFieldDesc light_fields[] = {
        { .m_Field = THREAD_COLOR, .m_Type = DATA_VALUE_TYPE_VECTOR3, .m_Offset = 0 },
        { .m_Field = THREAD_INTENSITY, .m_Type = DATA_VALUE_TYPE_NUMBER, .m_Offset = 16 }
    };
    DataStructDesc light = { .m_Fields = light_fields, .m_FieldCount = 2, .m_Size = 24 };
    DataFieldDesc  fields[THREAD_TYPE_COUNT][6] = {
        { { .m_Field = THREAD_POSITION, .m_Type = DATA_VALUE_TYPE_VECTOR3, .m_Offset = 0 },
           { .m_Field = THREAD_LIGHT, .m_Type = DATA_VALUE_TYPE_STRUCT, .m_Offset = 16, .m_Struct = &light },
           { .m_Field = 7, .m_Type = DATA_VALUE_TYPE_NUMBER, .m_Offset = 40 },
           { .m_Field = 8, .m_Type = DATA_VALUE_TYPE_NUMBER, .m_Offset = 48 },
           { .m_Field = 9, .m_Type = DATA_VALUE_TYPE_NUMBER, .m_Offset = 56 } },
        { { .m_Field = THREAD_LIGHT, .m_Type = DATA_VALUE_TYPE_STRUCT, .m_Offset = 0, .m_Struct = &light },
           { .m_Field = THREAD_POSITION, .m_Type = DATA_VALUE_TYPE_VECTOR3, .m_Offset = 24 },
           { .m_Field = 7, .m_Type = DATA_VALUE_TYPE_NUMBER, .m_Offset = 40 } },
        { { .m_Field = THREAD_POSITION, .m_Type = DATA_VALUE_TYPE_VECTOR3, .m_Offset = 0 },
           { .m_Field = THREAD_HEALTH, .m_Type = DATA_VALUE_TYPE_NUMBER, .m_Offset = 16 },
           { .m_Field = THREAD_VELOCITY, .m_Type = DATA_VALUE_TYPE_VECTOR3, .m_Offset = 24 } },
        { { .m_Field = THREAD_HEALTH, .m_Type = DATA_VALUE_TYPE_NUMBER, .m_Offset = 0 },
           { .m_Field = THREAD_VELOCITY, .m_Type = DATA_VALUE_TYPE_VECTOR3, .m_Offset = 8 },
           { .m_Field = THREAD_POSITION, .m_Type = DATA_VALUE_TYPE_VECTOR3, .m_Offset = 20 } },
        { { .m_Field = THREAD_POSITION, .m_Type = DATA_VALUE_TYPE_VECTOR3, .m_Offset = 0 },
           { .m_Field = 10, .m_Type = DATA_VALUE_TYPE_NUMBER, .m_Offset = 16 } },
        { { .m_Field = THREAD_HEALTH, .m_Type = DATA_VALUE_TYPE_NUMBER, .m_Offset = 0 },
           { .m_Field = THREAD_POSITION, .m_Type = DATA_VALUE_TYPE_VECTOR3, .m_Offset = 8 } }
    };
    const uint32_t field_counts[] = { 5, 3, 3, 3, 2, 2 };
    const uint32_t strides[] = { 64, 48, 40, 32, 24, 24 };
    uint64_t       names[] = { THREAD_COLOR, THREAD_INTENSITY };
    uint32_t       component = 0;
    ThreadedDefaults(fixture->m_Defaults[enemies], enemies);
    for (uint32_t t = 0; t < THREAD_TYPE_COUNT; ++t)
    {
        uint32_t count = ThreadedTypeRows(t, enemies);
        if (!count)
            continue;
        uint64_t      tag = 100 + t;
        DataTableDesc table = {
            .m_Type = 1000 + t,
            .m_Tags = &tag,
            .m_TagCount = 1,
            .m_Fields = fields[t],
            .m_FieldCount = field_counts[t],
            .m_RowStride = strides[t]
        };
        ThreadedCheck(DataRegisterTable(source, &table) == DATA_RESULT_OK, "prototype table");
        DataRowDesc         rows[THREAD_BUNDLE_ROWS] = {};
        DataValueType       types[6];
        const DataValueType child_types[] = { DATA_VALUE_TYPE_VECTOR3, DATA_VALUE_TYPE_NUMBER };
        for (uint32_t f = 0; f < field_counts[t]; ++f)
            types[f] = fields[t][f].m_Type;
        DataValueData values[THREAD_BUNDLE_ROWS][6] = {}, children[THREAD_BUNDLE_ROWS][2] = {};
        DataId        ids[THREAD_BUNDLE_ROWS];
        for (uint32_t r = 0; r < count; ++r, ++component)
        {
            ThreadedReferenceRow* row = &fixture->m_Defaults[enemies][component];
            children[r][0] = Vector(row->m_Color).m_Value;
            children[r][1] = Number(row->m_Intensity).m_Value;
            for (uint32_t f = 0; f < field_counts[t]; ++f)
            {
                uint64_t  name = fields[t][f].m_Field;
                DataValue value = Number(10);
                if (name == THREAD_POSITION)
                    value = Vector(row->m_Position);
                else if (name == THREAD_VELOCITY)
                    value = Vector(row->m_Velocity);
                else if (name == THREAD_HEALTH)
                    value = Number(row->m_Health);
                else if (name == THREAD_LIGHT)
                {
                    value.m_Type = DATA_VALUE_TYPE_STRUCT;
                    value.m_Value.m_Struct.m_Names = names;
                    value.m_Value.m_Struct.m_Types = child_types;
                    value.m_Value.m_Struct.m_Values = children[r];
                    value.m_Value.m_Struct.m_Count = 2;
                }
                values[r][f] = value.m_Value;
            }
            rows[r].m_Types = types;
            rows[r].m_Values = values[r];
            rows[r].m_ValueCount = field_counts[t];
            rows[r].m_ComponentId = component + 1;
        }
        ThreadedCheck(DataAddRows(source, table.m_Type, rows, count, ids) == DATA_RESULT_OK, "prototype rows");
    }
    ThreadedCheck(DataWriteBlob(source, 0, 0, &fixture->m_ByteCount[enemies]) == DATA_RESULT_OK, "prototype size");
    {
        ThreadMemoryScope resource(THREAD_MEMORY_RESOURCE);
        fixture->m_Bytes[enemies] = new uint8_t[fixture->m_ByteCount[enemies]];
    }
    ThreadedCheck(DataWriteBlob(source, fixture->m_Bytes[enemies], fixture->m_ByteCount[enemies], &fixture->m_ByteCount[enemies]) == DATA_RESULT_OK, "serialize prototype");
    ThreadedCheck(DataLoadBlob(fixture->m_Bytes[enemies], fixture->m_ByteCount[enemies], &fixture->m_Prototype[enemies]) == DATA_RESULT_OK, "load prototype");
    ThreadedCheck(DataDestroyStore(source) == DATA_RESULT_OK, "destroy prototype builder");
}

static uint32_t AddGroup(ThreadedFixture* fixture, HDataBlob blob, bool enemies)
{
    uint32_t index = fixture->m_Issued++;
    ThreadedCheck(index < fixture->m_Capacity, "group capacity");
    ThreadedCheck(DataAddBlob(fixture->m_Store, blob, index + 1, &fixture->m_Groups[index].m_Instance) == DATA_RESULT_OK, "instantiate content");
    fixture->m_Live[enemies][fixture->m_LiveCount[enemies]++] = index;
    return index;
}

static void ValidateRows(ThreadedFixture* fixture)
{
    uint32_t visited = 0;
    DataStoreLock(fixture->m_Store);
    DataIterator it = DataQueryIter(fixture->m_Validation);
    while (DataIterNext(&it) == DATA_RESULT_OK)
    {
        DataRowIterator rows = DataIterRows(&it);
        while (DataRowIterNext(&rows) == DATA_RESULT_OK)
        {
            DataId   id = DataRowIterGetId(&rows);
            uint64_t owner = DataRowIterGetOwnerId(&rows);
            uint64_t component = DataGetComponentId(fixture->m_Store, id);
            ThreadedCheck(owner && owner <= fixture->m_Issued && component && component <= THREAD_BUNDLE_ROWS, "row identity");
            ThreadedReferenceRow* expected = &fixture->m_Reference[(owner - 1) * THREAD_BUNDLE_ROWS + component - 1];
            ThreadedCheck(expected->m_Live, "removed owner absent");
            ThreadedCheck(!expected->m_Id || expected->m_Id == id, "stable identity");
            expected->m_Id = id;
            const DataVector3* position = DataRowIterGetVector3(&rows, fixture->m_PositionField);
            ThreadedCheck(!memcmp(position, &expected->m_Position, sizeof(*position)), "position matches sequential reference");
            if (ThreadedHasHealth(expected->m_Type))
            {
                double health;
                ThreadedCheck(DataFieldGetNumber(fixture->m_Store, id, THREAD_HEALTH, &health) == DATA_RESULT_OK && health == expected->m_Health, "health matches sequential reference");
            }
            ++visited;
        }
    }
    DataStoreUnlock(fixture->m_Store);
    ThreadedCheck(visited == (fixture->m_LiveCount[0] + fixture->m_LiveCount[1]) * THREAD_BUNDLE_ROWS, "live count");
}

static void FinishUpdate(ThreadedUpdate* update)
{
    for (uint32_t j = 0; j < update->m_RangeCount; ++j)
    {
        const ThreadedStats& stats = update->m_Ranges[j].m_Stats;
        update->m_Stats.m_Rows += stats.m_Rows;
        update->m_Stats.m_Hits += stats.m_Hits;
        update->m_Stats.m_Sum += stats.m_Sum;
    }
    DataQueryEnd(update->m_Query);
    update->m_State = THREAD_DONE;
}

static void RangeFinished(HJobContext, HJob, JobSystemStatus status, void* context, void*, int32_t result)
{
    ThreadedCheck(status == JOBSYSTEM_STATUS_FINISHED && !result, "job completion");
    ThreadedUpdate* update = (ThreadedUpdate*)context;
    if (!--update->m_Remaining)
        FinishUpdate(update);
}

static void SubmitRanges(HJobContext jobs, ThreadedUpdate* update)
{
    ThreadMemoryScope memory(THREAD_MEMORY_JOBS);
    const uint32_t    chunk = 4096;
    uint32_t          count = DataQueryGetRowCount(update->m_Query);
    uint32_t          job_count = count / chunk + (count % chunk != 0);
    ThreadedCheck(job_count <= update->m_RangeCapacity, "range capacity");
    update->m_RangeCount = update->m_Remaining = job_count;
    update->m_State = THREAD_RUNNING;
    for (uint32_t j = 0; j < job_count; ++j)
    {
        ThreadedRange* range = &update->m_Ranges[j];
        memset(range, 0, sizeof(*range));
        range->m_Update = update;
        range->m_First = j * chunk;
        range->m_Count = count - range->m_First < chunk ? count - range->m_First : chunk;
        Job  job = { .m_Process = update->m_Process, .m_Callback = RangeFinished, .m_Context = update, .m_Data = range };
        HJob handle = JobSystemCreateJob(jobs, &job);
        ThreadedCheck(handle && JobSystemPushJob(jobs, handle) == JOBSYSTEM_RESULT_OK, "submit range job");
    }
    if (!job_count)
        FinishUpdate(update);
}

static bool UpdatesDone(const ThreadedUpdate* updates)
{
    for (uint32_t u = 0; u < THREAD_TASK_COUNT; ++u)
        if (updates[u].m_State != THREAD_DONE)
            return false;
    return true;
}

static void ValidateUpdates(ThreadedFixture* fixture, ThreadedUpdate* updates, const uint32_t* accepted)
{
    for (uint32_t a = 0; a < THREAD_TASK_COUNT; ++a)
    {
        ThreadedUpdate* update = &updates[accepted[a]];
        ThreadedStats   expected = {};
        for (uint32_t r = 0; r < fixture->m_Issued * THREAD_BUNDLE_ROWS; ++r)
            if (fixture->m_Reference[r].m_Live)
                update->m_Reference(&fixture->m_Reference[r], &expected);
        ThreadedCheck(expected.m_Rows == update->m_Stats.m_Rows && expected.m_Hits == update->m_Stats.m_Hits, "update counts");
        ThreadedCheck(fabs(expected.m_Sum - update->m_Stats.m_Sum) <= 1e-7 * (1 + fabs(expected.m_Sum)), "update checksum");
    }
}

static void Run(uint32_t population, uint32_t frames, uint32_t workers)
{
    ThreadMemoryScope memory(THREAD_MEMORY_STORE);
    ThreadedFixture   fixture = {};
    BuildPrototype(&fixture, false);
    BuildPrototype(&fixture, true);
    fixture.m_Store = DataCreateStore();
    uint32_t initial_groups = population / THREAD_BUNDLE_ROWS;
    fixture.m_Capacity = ThreadedGroupCapacity(initial_groups, frames);
    ThreadMemorySetDomain(THREAD_MEMORY_CALLER);
    fixture.m_Groups = new ContentGroup[fixture.m_Capacity]();
    fixture.m_Live[0] = new uint32_t[fixture.m_Capacity];
    fixture.m_Live[1] = new uint32_t[fixture.m_Capacity];
    fixture.m_Reference = new ThreadedReferenceRow[(size_t)fixture.m_Capacity * THREAD_BUNDLE_ROWS]();
    ThreadMemorySetDomain(THREAD_MEMORY_STORE);
    for (uint32_t i = 0; i < initial_groups; ++i)
    {
        uint32_t group = AddGroup(&fixture, fixture.m_Prototype[0], false);
        memcpy(fixture.m_Reference + (size_t)group * THREAD_BUNDLE_ROWS, fixture.m_Defaults[0], sizeof(fixture.m_Defaults[0]));
    }
    DataQueryField position = { .m_Field = THREAD_POSITION, .m_Type = DATA_VALUE_TYPE_VECTOR3 };
    DataQueryDesc  desc = { .m_Fields = &position, .m_FieldCount = 1 };
    ThreadedCheck(DataCreateQuery(fixture.m_Store, &desc, &fixture.m_Validation) == DATA_RESULT_OK, "validation query");
    fixture.m_PositionField = DataQueryFindField(fixture.m_Validation, &position);
    ValidateRows(&fixture);
    ThreadedUpdate updates[THREAD_TASK_COUNT] = {};
    CreateThreadedMovement(fixture.m_Store, &updates[THREAD_MOVEMENT]);
    CreateThreadedExplosion(fixture.m_Store, &updates[THREAD_EXPLOSION]);
    CreateThreadedRegenerate(fixture.m_Store, &updates[THREAD_REGENERATE]);
    CreateThreadedLights(fixture.m_Store, &updates[THREAD_LIGHTS]);
    for (uint32_t u = 0; u < THREAD_TASK_COUNT; ++u)
    {
        updates[u].m_RangeCapacity = fixture.m_Capacity * THREAD_BUNDLE_ROWS / 4096 + 1;
        ThreadMemoryScope caller(THREAD_MEMORY_CALLER);
        updates[u].m_Ranges = new ThreadedRange[updates[u].m_RangeCapacity];
    }
    JobSystemCreateParams params = { .m_ThreadNamePrefix = "data-bench", .m_ThreadCount = (uint8_t)workers };
    HJobContext           jobs;
    {
        ThreadMemoryScope job_memory(THREAD_MEMORY_JOBS);
        jobs = JobSystemCreate(&params);
    }
    ThreadedCheck(JobSystemGetWorkerCount(jobs) == workers, "worker count");
    uint32_t* removed;
    {
        ThreadMemoryScope caller(THREAD_MEMORY_CALLER);
        removed = new uint32_t[fixture.m_Capacity];
    }
    uint32_t random = 0x5678;
    for (uint32_t frame = 0; frame < frames; ++frame)
    {
        ThreadedWave wave = ThreadedPlanWave(initial_groups, frame, fixture.m_LiveCount[1]);
        uint32_t     order[] = { 0, 1, 2, 3 }, accepted[THREAD_TASK_COUNT], admitted = 0;
        for (uint32_t u = THREAD_TASK_COUNT - 1; u; --u)
        {
            uint32_t j = (ThreadedRandom(&random) >> 16) % (u + 1);
            uint32_t swap = order[u];
            order[u] = order[j];
            order[j] = swap;
        }
        for (uint32_t u = 0; u < THREAD_TASK_COUNT; ++u)
        {
            updates[u].m_State = THREAD_WAITING;
            updates[u].m_Stats = ThreadedStats();
            updates[u].m_Busy = 0;
            updates[u].m_FirstAttempt = updates[u].m_WaitMicros = 0;
        }
        HDataBlob pending[2] = {};
        ThreadMemoryBegin();
        uint64_t start = dmTime::GetMonotonicTime();
        while (!UpdatesDone(updates))
        {
            for (uint32_t u = 0; u < THREAD_TASK_COUNT; ++u)
            {
                ThreadedUpdate* update = &updates[order[u]];
                if (update->m_State != THREAD_WAITING)
                    continue;
                if (!update->m_FirstAttempt)
                    update->m_FirstAttempt = dmTime::GetMonotonicTime();
                DataResult result = DataQueryTryBegin(update->m_Query);
                if (result == DATA_RESULT_BUSY)
                {
                    ++update->m_Busy;
                    continue;
                }
                ThreadedCheck(result == DATA_RESULT_OK, "reserve update");
                update->m_WaitMicros = dmTime::GetMonotonicTime() - update->m_FirstAttempt;
                accepted[admitted++] = order[u];
                SubmitRanges(jobs, update);
            }
            // Resource preparation overlaps active executions. Store mutation waits.
            if (!pending[0])
            {
                ThreadedCheck(DataLoadBlob(fixture.m_Bytes[0], fixture.m_ByteCount[0], &pending[0]) == DATA_RESULT_OK, "prepare incoming resource");
                if (wave.m_EnemyAdd)
                    ThreadedCheck(DataLoadBlob(fixture.m_Bytes[1], fixture.m_ByteCount[1], &pending[1]) == DATA_RESULT_OK, "prepare enemy wave");
                ThreadedCheck(DataRemoveBlob(fixture.m_Groups[fixture.m_Live[0][0]].m_Instance) == DATA_RESULT_LOCKED, "unload blocked during execution");
            }
            uint32_t remaining_before = 0;
            for (uint32_t u = 0; u < THREAD_TASK_COUNT; ++u)
                remaining_before += updates[u].m_Remaining;
            {
                ThreadMemoryScope job_memory(THREAD_MEMORY_JOBS);
                JobSystemUpdate(jobs, 0);
            }
            uint32_t remaining_after = 0;
            for (uint32_t u = 0; u < THREAD_TASK_COUNT; ++u)
                remaining_after += updates[u].m_Remaining;
            // Caller-side idle policy: avoid spinning on the job-system mutex
            // while workers run. The requested sleep may be rounded by the OS.
            if (remaining_after && remaining_after == remaining_before)
                dmTime::Sleep(1);
        }
        ThreadMemorySample update_allocation = ThreadMemoryEnd();
        uint64_t           mutation_start = dmTime::GetMonotonicTime();
        uint32_t           removed_count = 0;
        for (uint32_t kind = 0; kind < 2; ++kind)
        {
            uint32_t count = kind ? wave.m_EnemyRemove : wave.m_Content;
            for (uint32_t i = 0; i < count; ++i)
            {
                uint32_t slot = (ThreadedRandom(&random) >> 8) % fixture.m_LiveCount[kind];
                uint32_t group = fixture.m_Live[kind][slot];
                removed[removed_count++] = group;
                ThreadedCheck(DataRemoveBlob(fixture.m_Groups[group].m_Instance) == DATA_RESULT_OK, "stream unload");
                fixture.m_Groups[group].m_Instance = 0;
                fixture.m_Live[kind][slot] = fixture.m_Live[kind][--fixture.m_LiveCount[kind]];
            }
        }
        uint32_t first_added = fixture.m_Issued;
        for (uint32_t i = 0; i < wave.m_Content; ++i)
            AddGroup(&fixture, pending[0], false);
        uint32_t first_enemy = fixture.m_Issued;
        for (uint32_t i = 0; i < wave.m_EnemyAdd; ++i)
            AddGroup(&fixture, pending[1], true);
        DataDestroyBlob(pending[0]);
        if (pending[1])
            DataDestroyBlob(pending[1]);
        uint64_t           end = dmTime::GetMonotonicTime();
        ThreadMemorySample allocation = ThreadMemoryEnd();
        // Everything below is outside frame timing, including sequential replay.
        ThreadedCheck(admitted == THREAD_TASK_COUNT, "all updates admitted once");
        ValidateUpdates(&fixture, updates, accepted);
        for (uint32_t i = 0; i < removed_count; ++i)
            for (uint32_t r = 0; r < THREAD_BUNDLE_ROWS; ++r)
            {
                ThreadedReferenceRow* row = &fixture.m_Reference[(size_t)removed[i] * THREAD_BUNDLE_ROWS + r];
                ThreadedCheck(!DataGetComponentId(fixture.m_Store, row->m_Id), "removed ID stays stale after reuse");
                row->m_Live = false;
            }
        for (uint32_t i = first_added; i < fixture.m_Issued; ++i)
            memcpy(fixture.m_Reference + (size_t)i * THREAD_BUNDLE_ROWS, fixture.m_Defaults[i >= first_enemy], sizeof(fixture.m_Defaults[0]));
        ValidateRows(&fixture);
        uint32_t busy = 0;
        uint64_t wait = 0;
        for (uint32_t u = 0; u < THREAD_TASK_COUNT; ++u)
        {
            busy += updates[u].m_Busy;
            wait += updates[u].m_WaitMicros;
        }
        ThreadedCheck(busy != 0, "conflicting tasks exercised admission");
        printf("Defold,%u,%u,%u,%llu,%llu,%u,%llu,%u%u%u%u,%.9f", workers, frame, (fixture.m_LiveCount[0] + fixture.m_LiveCount[1]) * THREAD_BUNDLE_ROWS, (unsigned long long)(end - start), (unsigned long long)(end - mutation_start), busy, (unsigned long long)wait, accepted[0], accepted[1], accepted[2], accepted[3], updates[THREAD_LIGHTS].m_Stats.m_Sum);
#ifdef DATA_THREADED_MEMORY
        for (uint32_t domain = 1; domain < THREAD_MEMORY_DOMAIN_COUNT; ++domain)
            printf(",%llu,%lld", (unsigned long long)(allocation.m_After[domain].m_Requests - allocation.m_Before[domain].m_Requests), (long long)allocation.m_After[domain].m_Bytes - (long long)allocation.m_Before[domain].m_Bytes);
        printf(",%llu,%llu,%llu", (unsigned long long)allocation.m_PeakAdditional, (unsigned long long)(update_allocation.m_After[THREAD_MEMORY_STORE].m_Requests - update_allocation.m_Before[THREAD_MEMORY_STORE].m_Requests), (unsigned long long)(allocation.m_After[THREAD_MEMORY_STORE].m_Requests - update_allocation.m_After[THREAD_MEMORY_STORE].m_Requests));
#else
        (void)allocation;
        (void)update_allocation;
#endif
        for (uint32_t u = 0; u < THREAD_TASK_COUNT; ++u)
            printf(",%llu", (unsigned long long)updates[u].m_WaitMicros);
        printf(",%u,%u,%u,%u,%u\n", wave.m_Content * THREAD_BUNDLE_ROWS, wave.m_Content * THREAD_BUNDLE_ROWS, wave.m_EnemyAdd * THREAD_BUNDLE_ROWS, wave.m_EnemyRemove * THREAD_BUNDLE_ROWS, fixture.m_LiveCount[0] * THREAD_TYPE_ROWS[3] + fixture.m_LiveCount[1] * THREAD_BUNDLE_ROWS);
    }
    JobSystemDestroy(jobs);
    for (uint32_t u = 0; u < THREAD_TASK_COUNT; ++u)
    {
        ThreadedCheck(DataDestroyQuery(updates[u].m_Query) == DATA_RESULT_OK, "destroy update query");
        delete[] updates[u].m_Ranges;
    }
    for (uint32_t kind = 0; kind < 2; ++kind)
        for (uint32_t i = 0; i < fixture.m_LiveCount[kind]; ++i)
            ThreadedCheck(DataRemoveBlob(fixture.m_Groups[fixture.m_Live[kind][i]].m_Instance) == DATA_RESULT_OK, "final content unload");
    ThreadedCheck(DataQueryTryBegin(fixture.m_Validation) == DATA_RESULT_OK && !DataQueryGetRowCount(fixture.m_Validation), "queries empty after final unload");
    DataQueryEnd(fixture.m_Validation);
    ThreadedCheck(DataDestroyQuery(fixture.m_Validation) == DATA_RESULT_OK, "destroy validation query");
    ThreadedCheck(DataDestroyStore(fixture.m_Store) == DATA_RESULT_OK, "destroy benchmark store");
    DataDestroyBlob(fixture.m_Prototype[0]);
    DataDestroyBlob(fixture.m_Prototype[1]);
    delete[] fixture.m_Bytes[0];
    delete[] fixture.m_Bytes[1];
    delete[] fixture.m_Groups;
    delete[] fixture.m_Live[0];
    delete[] fixture.m_Live[1];
    delete[] fixture.m_Reference;
    delete[] removed;
    ThreadMemoryCheckReleased();
}

int main(int argc, char** argv)
{
    uint32_t population = argc > 1 ? (uint32_t)strtoul(argv[1], 0, 10) : 1000000;
    uint32_t frames = argc > 2 ? (uint32_t)strtoul(argv[2], 0, 10) : 32;
    uint32_t workers = argc > 3 ? (uint32_t)strtoul(argv[3], 0, 10) : 0;
    ThreadedCheck(population >= 1000 && population <= 10000000 && population % 100 == 0 && frames > 0 && frames <= 1000, "population/frames out of range");
    ThreadedCheck(!workers || workers == 1 || workers == 2 || workers == 4 || workers == 8, "workers must be 1, 2, 4 or 8");
#ifdef DM_SANITIZE_THREAD
    printf("# sanitizer=thread; validation only; do not compare these timings\n");
#else
    printf("# sanitizer=none; frame_us includes dispatch, completion, preparation and content changes; validation excluded\n");
#endif
    printf("# tasks: 0=movement,1=explosion,2=regenerate,3=nearby_lights; 4096 rows/job; seeded candidate order; caller sleeps 1 us when completion polling makes no progress\n");
#ifdef DATA_THREADED_MEMORY
    printf("# memory=C++ new/delete including dlib arrays; excludes malloc in libc/thread runtime, OS thread stacks, allocator headers and TSAN runtime; timings not comparable\n");
#endif
    printf("backend,workers,frame,live_rows,frame_us,mutation_us,busy_attempts,access_wait_us,admission_order,light_sum");
#ifdef DATA_THREADED_MEMORY
    printf(",store_requests,store_delta_bytes,job_requests,job_delta_bytes,caller_requests,caller_delta_bytes,resource_requests,resource_delta_bytes,peak_additional_bytes,update_store_requests,stream_store_requests");
#endif
    printf(",movement_wait_us,explosion_wait_us,regenerate_wait_us,lights_wait_us,content_spawned,content_despawned,enemies_spawned,enemies_despawned,live_enemies\n");
    ThreadMemorySelfTest();
    if (workers)
        Run(population, frames, workers);
    else
        for (uint32_t n = 1; n <= 8; n *= 2)
            Run(population, frames, n);
    return 0;
}
