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
#include <dlib/atomic.h>
#include "benchmark_data_threaded_flecs.h"
#include "benchmark_data_threaded_memory.h"

// One stage per actual job thread, reused by successive jobs on that thread.
// Jobs never issue structural commands; the main thread owns streaming changes.
static int32_atomic_t g_NextStage;
ecs_iter_t            ThreadedFlecsIter(const FlecsThreadedUpdate* update)
{
    static thread_local int32_t stage = -1;
    if (stage < 0)
        stage = dmAtomicIncrement32(&g_NextStage);
    return ecs_query_iter(ecs_get_stage(update->m_World, stage), update->m_WorkerQueries[stage]);
}

struct FlecsFixture
{
    ecs_world_t*          m_World;
    FlecsThreadedIds      m_Ids;
    ThreadedReferenceRow  m_Defaults[2][THREAD_BUNDLE_ROWS];
    ThreadedReferenceRow* m_Reference;
    ecs_entity_t*         m_Entities;
    uint32_t*             m_Live[2];
    uint32_t              m_LiveCount[2], m_Issued, m_Capacity;
};

static ecs_entity_t Component(ecs_world_t* world, size_t size, size_t alignment)
{
    ecs_component_desc_t desc = { .type = { .size = (ecs_size_t)size, .alignment = (ecs_size_t)alignment } };
    return ecs_component_init(world, &desc);
}

static void AddGroup(FlecsFixture* fixture, const ThreadedReferenceRow* defaults, bool enemies)
{
    uint32_t group = fixture->m_Issued++, first = 0;
    ThreadedCheck(group < fixture->m_Capacity, "Flecs group capacity");
    const FlecsThreadedIds* ids = &fixture->m_Ids;
    for (uint32_t t = 0; t < THREAD_TYPE_COUNT; ++t)
    {
        uint32_t count = ThreadedTypeRows(t, enemies);
        if (!count)
            continue;
        DataVector3        positions[THREAD_BUNDLE_ROWS], velocities[THREAD_BUNDLE_ROWS];
        FlecsThreadedLight lights[THREAD_BUNDLE_ROWS];
        double             health[THREAD_BUNDLE_ROWS], extra[THREAD_BUNDLE_ROWS][3];
        for (uint32_t r = 0; r < count; ++r)
        {
            const ThreadedReferenceRow& row = defaults[first + r];
            positions[r] = row.m_Position;
            velocities[r] = row.m_Velocity;
            health[r] = row.m_Health;
            lights[r].m_Color = row.m_Color;
            lights[r].m_Intensity = row.m_Intensity;
            for (uint32_t e = 0; e < 3; ++e)
                extra[r][e] = 10;
        }
        ecs_bulk_desc_t desc = {};
        void*           data[8] = {};
        uint32_t        f = 0;
        desc.ids[f] = ids->m_Tags[t];
        data[f++] = 0;
        desc.ids[f] = ids->m_Position;
        data[f++] = positions;
        if (ThreadedHasHealth(t))
        {
            desc.ids[f] = ids->m_Health;
            data[f++] = health;
        }
        if (t == 2 || t == 3)
        {
            desc.ids[f] = ids->m_Velocity;
            data[f++] = velocities;
        }
        if (t < 2)
        {
            desc.ids[f] = ids->m_Light;
            data[f++] = lights;
        }
        if (ids->m_Extra[t])
        {
            desc.ids[f] = ids->m_Extra[t];
            data[f++] = extra;
        }
        desc.data = data;
        desc.count = count;
        const ecs_entity_t* entities = ecs_bulk_init(fixture->m_World, &desc);
        ThreadedCheck(entities != 0, "Flecs bulk creation");
        memcpy(fixture->m_Entities + (size_t)group * THREAD_BUNDLE_ROWS + first, entities, sizeof(ecs_entity_t) * desc.count);
        first += desc.count;
    }
    fixture->m_Live[enemies][fixture->m_LiveCount[enemies]++] = group;
}

static void RemoveGroup(FlecsFixture* fixture, uint32_t group)
{
    for (uint32_t r = 0; r < THREAD_BUNDLE_ROWS; ++r)
        ecs_delete(fixture->m_World, fixture->m_Entities[(size_t)group * THREAD_BUNDLE_ROWS + r]);
}

static void ValidateRows(FlecsFixture* fixture)
{
    for (uint32_t g = 0; g < fixture->m_Issued; ++g)
        for (uint32_t r = 0; r < THREAD_BUNDLE_ROWS; ++r)
        {
            size_t                      index = (size_t)g * THREAD_BUNDLE_ROWS + r;
            const ThreadedReferenceRow& row = fixture->m_Reference[index];
            ecs_entity_t                entity = fixture->m_Entities[index];
            ThreadedCheck(ecs_is_alive(fixture->m_World, entity) == row.m_Live, "Flecs identity remains live/stale after reuse");
            if (!row.m_Live)
                continue;
            const void* position = ecs_get_id(fixture->m_World, entity, fixture->m_Ids.m_Position);
            ThreadedCheck(position && !memcmp(position, &row.m_Position, sizeof(DataVector3)), "Flecs position matches reference");
            if (ThreadedHasHealth(row.m_Type))
            {
                const double* health = (const double*)ecs_get_id(fixture->m_World, entity, fixture->m_Ids.m_Health);
                ThreadedCheck(health && *health == row.m_Health, "Flecs health matches reference");
            }
        }
}

// This is caller scheduling, not an automatic lock in ecs_query_iter. For these
// simple owned-component queries, shared tables plus shared writable IDs conflict.
static bool Conflicts(const FlecsThreadedUpdate* a, const FlecsThreadedUpdate* b)
{
    bool shared = false;
    for (uint32_t i = 0; i < a->m_TableCount; ++i)
        for (uint32_t j = 0; j < b->m_TableCount; ++j)
            shared |= a->m_Tables[i] == b->m_Tables[j];
    if (!shared)
        return false;
    for (int32_t i = 0; i < a->m_Query->term_count; ++i)
        for (int32_t j = 0; j < b->m_Query->term_count; ++j)
            if (a->m_Query->terms[i].id == b->m_Query->terms[j].id &&
                (a->m_Query->terms[i].inout != EcsIn || b->m_Query->terms[j].inout != EcsIn))
                return true;
    return false;
}

static void Refresh(FlecsThreadedUpdate* update)
{
    update->m_RowCount = update->m_TableCount = 0;
    ecs_iter_t it = ecs_query_iter(update->m_World, update->m_Query);
    while (ecs_query_next(&it))
    {
        ThreadedCheck(update->m_TableCount < THREAD_TYPE_COUNT, "Flecs fixture table capacity");
        update->m_Tables[update->m_TableCount++] = it.table;
        update->m_RowCount += it.count;
    }
}

static void RangeFinished(HJobContext, HJob, JobSystemStatus status, void* context, void*, int32_t result)
{
    ThreadedCheck(status == JOBSYSTEM_STATUS_FINISHED && !result, "Flecs job completion");
    ThreadedUpdate* update = (ThreadedUpdate*)context;
    if (--update->m_Remaining)
        return;
    for (uint32_t j = 0; j < update->m_RangeCount; ++j)
    {
        update->m_Stats.m_Rows += update->m_Ranges[j].m_Stats.m_Rows;
        update->m_Stats.m_Hits += update->m_Ranges[j].m_Stats.m_Hits;
        update->m_Stats.m_Sum += update->m_Ranges[j].m_Stats.m_Sum;
    }
    update->m_State = THREAD_DONE;
}

static void SubmitRanges(HJobContext jobs, FlecsThreadedUpdate* flecs)
{
    ThreadMemoryScope memory(THREAD_MEMORY_JOBS);
    ThreadedUpdate*   update = &flecs->m_Common;
    uint32_t          count = (flecs->m_RowCount + 4095) / 4096;
    ThreadedCheck(count && count <= update->m_RangeCapacity, "Flecs partition count");
    update->m_RangeCount = update->m_Remaining = count;
    update->m_State = THREAD_RUNNING;
    for (uint32_t j = 0; j < count; ++j)
    {
        ThreadedRange* range = &update->m_Ranges[j];
        memset(range, 0, sizeof(*range));
        range->m_Update = update;
        range->m_First = j; // ecs_worker_iter partition index, not a row offset.
        range->m_Count = count;
        Job  job = { .m_Process = update->m_Process, .m_Callback = RangeFinished, .m_Context = update, .m_Data = range };
        HJob handle = JobSystemCreateJob(jobs, &job);
        ThreadedCheck(handle && JobSystemPushJob(jobs, handle) == JOBSYSTEM_RESULT_OK, "Flecs submit job");
    }
}

static void Run(uint32_t population, uint32_t frames, uint32_t workers)
{
    ThreadMemoryScope memory(THREAD_MEMORY_STORE);
    FlecsFixture      fixture = {};
    ThreadedDefaults(fixture.m_Defaults[0]);
    ThreadedDefaults(fixture.m_Defaults[1], true);
    fixture.m_World = ecs_mini();
    FlecsThreadedIds* ids = &fixture.m_Ids;
    ids->m_Position = Component(fixture.m_World, sizeof(DataVector3), alignof(DataVector3));
    ids->m_Velocity = Component(fixture.m_World, sizeof(DataVector3), alignof(DataVector3));
    ids->m_Health = Component(fixture.m_World, sizeof(double), alignof(double));
    ids->m_Light = Component(fixture.m_World, sizeof(FlecsThreadedLight), alignof(FlecsThreadedLight));
    for (uint32_t t = 0; t < THREAD_TYPE_COUNT; ++t)
        ids->m_Tags[t] = ecs_new(fixture.m_World);
    ids->m_Extra[0] = Component(fixture.m_World, 24, 8);
    ids->m_Extra[1] = ids->m_Extra[4] = Component(fixture.m_World, 8, 8);
    uint32_t initial = population / THREAD_BUNDLE_ROWS;
    fixture.m_Capacity = ThreadedGroupCapacity(initial, frames);
    ThreadMemorySetDomain(THREAD_MEMORY_CALLER);
    fixture.m_Entities = new ecs_entity_t[(size_t)fixture.m_Capacity * THREAD_BUNDLE_ROWS];
    fixture.m_Reference = new ThreadedReferenceRow[(size_t)fixture.m_Capacity * THREAD_BUNDLE_ROWS]();
    fixture.m_Live[0] = new uint32_t[fixture.m_Capacity];
    fixture.m_Live[1] = new uint32_t[fixture.m_Capacity];
    uint32_t* removed = new uint32_t[fixture.m_Capacity];
    ThreadMemorySetDomain(THREAD_MEMORY_STORE);
    for (uint32_t g = 0; g < initial; ++g)
    {
        AddGroup(&fixture, fixture.m_Defaults[0], false);
        memcpy(fixture.m_Reference + (size_t)g * THREAD_BUNDLE_ROWS, fixture.m_Defaults[0], sizeof(fixture.m_Defaults[0]));
    }
    ecs_set_stage_count(fixture.m_World, workers);
    FlecsThreadedUpdate updates[THREAD_TASK_COUNT] = {};
    CreateFlecsMovement(fixture.m_World, ids, &updates[0]);
    CreateFlecsExplosion(fixture.m_World, ids, &updates[1]);
    CreateFlecsRegenerate(fixture.m_World, ids, &updates[2]);
    CreateFlecsLights(fixture.m_World, ids, &updates[3]);
    for (uint32_t u = 0; u < THREAD_TASK_COUNT; ++u)
    {
        ThreadMemoryScope caller(THREAD_MEMORY_CALLER);
        updates[u].m_Common.m_RangeCapacity = fixture.m_Capacity * THREAD_BUNDLE_ROWS / 4096 + 1;
        updates[u].m_Common.m_Ranges = new ThreadedRange[updates[u].m_Common.m_RangeCapacity];
        Refresh(&updates[u]);
    }
    ThreadedCheck(Conflicts(&updates[0], &updates[1]) && Conflicts(&updates[1], &updates[2]), "Flecs read/write and write/write conflicts");
    ThreadedCheck(!Conflicts(&updates[0], &updates[2]) && !Conflicts(&updates[0], &updates[3]), "Flecs independent fields/tables");
    g_NextStage = 0;
    JobSystemCreateParams params = { .m_ThreadNamePrefix = "flecs-bench", .m_ThreadCount = (uint8_t)workers };
    HJobContext           jobs;
    {
        ThreadMemoryScope job_memory(THREAD_MEMORY_JOBS);
        jobs = JobSystemCreate(&params);
    }
    uint32_t random = 0x5678;
    for (uint32_t frame = 0; frame < frames; ++frame)
    {
        ThreadedWave wave = ThreadedPlanWave(initial, frame, fixture.m_LiveCount[1]);
        uint32_t     order[] = { 0, 1, 2, 3 }, accepted[4], admitted = 0, done = 0;
        for (uint32_t u = 3; u; --u)
        {
            uint32_t j = (ThreadedRandom(&random) >> 16) % (u + 1), swap = order[u];
            order[u] = order[j];
            order[j] = swap;
        }
        for (uint32_t u = 0; u < 4; ++u)
        {
            ThreadedUpdate* update = &updates[u].m_Common;
            update->m_State = THREAD_WAITING;
            update->m_Stats = ThreadedStats();
            update->m_FirstAttempt = update->m_WaitMicros = update->m_Busy = 0;
        }
        ThreadMemoryBegin();
        uint64_t start = dmTime::GetMonotonicTime();
        for (uint32_t u = 0; u < 4; ++u)
            Refresh(&updates[u]);
        ecs_readonly_begin(fixture.m_World, true);
        // Decoded incoming content is independent of world storage and can be
        // prepared while workers run. Flecs has no Defold packed-blob format.
        ThreadedReferenceRow* pending[2] = {};
        while (done != 4)
        {
            for (uint32_t u = 0; u < 4; ++u)
            {
                FlecsThreadedUpdate* candidate = &updates[order[u]];
                ThreadedUpdate*      update = &candidate->m_Common;
                if (update->m_State != THREAD_WAITING)
                    continue;
                if (!update->m_FirstAttempt)
                    update->m_FirstAttempt = dmTime::GetMonotonicTime();
                bool busy = false;
                for (uint32_t v = 0; v < 4; ++v)
                    busy |= updates[v].m_Common.m_State == THREAD_RUNNING && Conflicts(candidate, &updates[v]);
                if (busy)
                {
                    ++update->m_Busy;
                    continue;
                }
                accepted[admitted++] = order[u];
                update->m_WaitMicros = dmTime::GetMonotonicTime() - update->m_FirstAttempt;
                SubmitRanges(jobs, candidate);
            }
            if (!pending[0])
            {
                ThreadMemoryScope resource(THREAD_MEMORY_RESOURCE);
                pending[0] = new ThreadedReferenceRow[THREAD_BUNDLE_ROWS];
                memcpy(pending[0], fixture.m_Defaults[0], sizeof(fixture.m_Defaults[0]));
                if (wave.m_EnemyAdd)
                {
                    pending[1] = new ThreadedReferenceRow[THREAD_BUNDLE_ROWS];
                    memcpy(pending[1], fixture.m_Defaults[1], sizeof(fixture.m_Defaults[1]));
                }
            }
            uint32_t before = 0, after = 0;
            for (uint32_t u = 0; u < 4; ++u)
                before += updates[u].m_Common.m_Remaining;
            {
                ThreadMemoryScope job_memory(THREAD_MEMORY_JOBS);
                JobSystemUpdate(jobs, 0);
            }
            done = 0;
            for (uint32_t u = 0; u < 4; ++u)
            {
                after += updates[u].m_Common.m_Remaining;
                done += updates[u].m_Common.m_State == THREAD_DONE;
            }
            if (after && after == before)
                dmTime::Sleep(1);
        }
        ecs_readonly_end(fixture.m_World);
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
                RemoveGroup(&fixture, group);
                fixture.m_Live[kind][slot] = fixture.m_Live[kind][--fixture.m_LiveCount[kind]];
            }
        }
        uint32_t first_added = fixture.m_Issued;
        for (uint32_t i = 0; i < wave.m_Content; ++i)
            AddGroup(&fixture, pending[0], false);
        uint32_t first_enemy = fixture.m_Issued;
        for (uint32_t i = 0; i < wave.m_EnemyAdd; ++i)
            AddGroup(&fixture, pending[1], true);
        delete[] pending[0];
        delete[] pending[1];
        uint64_t           end = dmTime::GetMonotonicTime();
        ThreadMemorySample allocation = ThreadMemoryEnd();
        ThreadedCheck(admitted == 4, "Flecs all tasks ran");
        for (uint32_t a = 0; a < 4; ++a)
        {
            ThreadedStats   expected = {};
            ThreadedUpdate* update = &updates[accepted[a]].m_Common;
            for (uint32_t r = 0; r < first_added * THREAD_BUNDLE_ROWS; ++r)
                if (fixture.m_Reference[r].m_Live)
                    update->m_Reference(&fixture.m_Reference[r], &expected);
            ThreadedCheck(expected.m_Rows == update->m_Stats.m_Rows && expected.m_Hits == update->m_Stats.m_Hits && fabs(expected.m_Sum - update->m_Stats.m_Sum) <= 1e-7 * (1 + fabs(expected.m_Sum)), "Flecs update matches sequential replay");
        }
        for (uint32_t i = 0; i < removed_count; ++i)
            for (uint32_t r = 0; r < THREAD_BUNDLE_ROWS; ++r)
                fixture.m_Reference[(size_t)removed[i] * THREAD_BUNDLE_ROWS + r].m_Live = false;
        for (uint32_t g = first_added; g < fixture.m_Issued; ++g)
            memcpy(fixture.m_Reference + (size_t)g * THREAD_BUNDLE_ROWS, fixture.m_Defaults[g >= first_enemy], sizeof(fixture.m_Defaults[0]));
        ValidateRows(&fixture);
        uint32_t busy = 0;
        uint64_t wait = 0;
        for (uint32_t u = 0; u < 4; ++u)
        {
            busy += updates[u].m_Common.m_Busy;
            wait += updates[u].m_Common.m_WaitMicros;
        }
        ThreadedCheck(busy, "Flecs conflicting tasks retried");
        printf("Flecs,%u,%u,%u,%llu,%llu,%u,%llu,%u%u%u%u,%.9f", workers, frame, (fixture.m_LiveCount[0] + fixture.m_LiveCount[1]) * THREAD_BUNDLE_ROWS, (unsigned long long)(end - start), (unsigned long long)(end - mutation_start), busy, (unsigned long long)wait, accepted[0], accepted[1], accepted[2], accepted[3], updates[3].m_Common.m_Stats.m_Sum);
#ifdef DATA_THREADED_MEMORY
        for (uint32_t d = 1; d < THREAD_MEMORY_DOMAIN_COUNT; ++d)
            printf(",%llu,%lld", (unsigned long long)(allocation.m_After[d].m_Requests - allocation.m_Before[d].m_Requests), (long long)allocation.m_After[d].m_Bytes - (long long)allocation.m_Before[d].m_Bytes);
        printf(",%llu,%llu,%llu", (unsigned long long)allocation.m_PeakAdditional, (unsigned long long)(update_allocation.m_After[THREAD_MEMORY_STORE].m_Requests - update_allocation.m_Before[THREAD_MEMORY_STORE].m_Requests), (unsigned long long)(allocation.m_After[THREAD_MEMORY_STORE].m_Requests - update_allocation.m_After[THREAD_MEMORY_STORE].m_Requests));
#else
        (void)allocation;
        (void)update_allocation;
#endif
        for (uint32_t u = 0; u < 4; ++u)
            printf(",%llu", (unsigned long long)updates[u].m_Common.m_WaitMicros);
        printf(",%u,%u,%u,%u,%u\n", wave.m_Content * THREAD_BUNDLE_ROWS, wave.m_Content * THREAD_BUNDLE_ROWS, wave.m_EnemyAdd * THREAD_BUNDLE_ROWS, wave.m_EnemyRemove * THREAD_BUNDLE_ROWS, fixture.m_LiveCount[0] * THREAD_TYPE_ROWS[3] + fixture.m_LiveCount[1] * THREAD_BUNDLE_ROWS);
    }
    JobSystemDestroy(jobs);
    for (uint32_t kind = 0; kind < 2; ++kind)
        for (uint32_t g = 0; g < fixture.m_LiveCount[kind]; ++g)
            RemoveGroup(&fixture, fixture.m_Live[kind][g]);
    for (uint32_t u = 0; u < 4; ++u)
    {
        ThreadedCheck(!ecs_query_count(updates[u].m_Query).results, "Flecs empty after final unload");
        for (uint32_t w = 0; w < workers; ++w)
            ecs_query_fini(updates[u].m_WorkerQueries[w]);
        delete[] updates[u].m_Common.m_Ranges;
    }
    ecs_fini(fixture.m_World);
    delete[] fixture.m_Entities;
    delete[] fixture.m_Reference;
    delete[] fixture.m_Live[0];
    delete[] fixture.m_Live[1];
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
    printf("# tasks: 0=movement,1=explosion,2=regenerate,3=nearby_lights; approximately 4096 rows/worker-iterator partition; seeded candidate order; caller sleeps 1 us when completion polling makes no progress\n");
#ifdef DATA_THREADED_MEMORY
    printf("# memory=Flecs OS allocation hooks plus C++ new/delete including dlib arrays; excludes malloc in libc/thread runtime, OS thread stacks, allocator headers and TSAN runtime; timings not comparable\n");
#endif
    printf("backend,workers,frame,live_rows,frame_us,mutation_us,busy_attempts,access_wait_us,admission_order,light_sum");
#ifdef DATA_THREADED_MEMORY
    printf(",store_requests,store_delta_bytes,job_requests,job_delta_bytes,caller_requests,caller_delta_bytes,resource_requests,resource_delta_bytes,peak_additional_bytes,update_store_requests,stream_store_requests");
#endif
    printf(",movement_wait_us,explosion_wait_us,regenerate_wait_us,lights_wait_us,content_spawned,content_despawned,enemies_spawned,enemies_despawned,live_enemies\n");
    ThreadMemorySelfTest();
    ThreadMemoryInstallFlecs();
    if (workers)
        Run(population, frames, workers);
    else
        for (uint32_t n = 1; n <= 8; n *= 2)
            Run(population, frames, n);
    return 0;
}
