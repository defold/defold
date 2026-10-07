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

#include "benchmark_data_common.h"

static volatile double g_Checksum;

void                   Check(bool ok, const char* message)
{
    if (!ok)
    {
        fprintf(stderr, "Mixed benchmark check failed: %s\n", message);
        exit(1);
    }
}

uint64_t BeginOperation()
{
    BeginBenchmarkMemoryOperation();
    return dmTime::GetMonotonicTime();
}

uint64_t EndOperation()
{
    uint64_t end = dmTime::GetMonotonicTime();
    EndBenchmarkMemoryOperation();
    return end;
}

void RecordMemory(const Backend* store, const Fixture* input, uint32_t sample, const char* operation, uint64_t operations, const Stats& stats)
{
#ifdef DATA_BENCHMARK_MEMORY
    if (!sample)
        return;
    BenchmarkMemorySample       memory = GetBenchmarkMemorySample();
    const BenchmarkMemoryStats& before = memory.m_Before;
    const BenchmarkMemoryStats& after = memory.m_After;
    if (!store->m_Kind && (!strcmp(operation, "explosion_r50_first") || !strcmp(operation, "shuffled_position") || (strstr(operation, "packed_") == operation && strstr(operation, "_update_"))))
    {
        Check(after.m_Allocations == before.m_Allocations && after.m_Reallocations == before.m_Reallocations &&
              after.m_Bytes == before.m_Bytes,
              "numeric writes must not allocate or grow storage");
    }
    printf("%s,%s,%u,%u,%llu,%llu,%llu,%.9f", BACKENDS[store->m_Kind], operation, input->m_Count, sample, (unsigned long long)operations, (unsigned long long)stats.m_Hits, (unsigned long long)stats.m_Batches, stats.m_Sum);
    printf(",%llu,%llu,%llu,%llu,%llu,%llu,%llu,%llu,%llu,%llu,%llu,%llu,%llu,%llu,%llu,%llu,%llu\n",
           (unsigned long long)before.m_Bytes,
           (unsigned long long)after.m_Bytes,
           (unsigned long long)after.m_PeakBytes,
           (unsigned long long)after.m_Blocks,
           (unsigned long long)after.m_PeakBlocks,
           (unsigned long long)(after.m_Allocations - before.m_Allocations),
           (unsigned long long)(after.m_Reallocations - before.m_Reallocations),
           (unsigned long long)(after.m_Frees - before.m_Frees),
           (unsigned long long)(after.m_AllocatedBytes - before.m_AllocatedBytes),
           (unsigned long long)after.m_Allocations,
           (unsigned long long)after.m_Reallocations,
           (unsigned long long)after.m_Frees,
           (unsigned long long)after.m_AllocatedBytes,
           (unsigned long long)memory.m_Fixture.m_Bytes,
           (unsigned long long)memory.m_Fixture.m_Blocks,
           (unsigned long long)memory.m_Resource.m_Bytes,
           (unsigned long long)memory.m_Resource.m_Blocks);
#else
    (void)store;
    (void)input;
    (void)sample;
    (void)operation;
    (void)operations;
    (void)stats;
#endif
}

uint32_t FindField(const TypeInput* type, FieldId field)
{
    for (uint32_t i = 0; i < type->m_FieldCount; ++i)
        if (type->m_Fields[i].m_Field == field)
            return i;
    return UINT32_MAX;
}

bool HasTag(const TypeInput* type, uint64_t tag)
{
    if (!tag)
        return true;
    for (uint32_t i = 0; i < type->m_TagCount; ++i)
        if (type->m_Tags[i] == tag)
            return true;
    return false;
}

void Validate(const Stats& actual, const Stats& expected)
{
    Check(!actual.m_Error && actual.m_Rows == expected.m_Rows && actual.m_Hits == expected.m_Hits && actual.m_Sum == expected.m_Sum, "query count/hits/checksum");
    g_Checksum = actual.m_Sum;
}

void Record(const Backend* store, const Fixture* input, uint32_t sample, const char* operation, uint64_t start, uint64_t end, uint64_t operations, const Stats& stats)
{
    Check(!stats.m_Error, operation);
    if (!sample)
        return;
#ifdef DATA_BENCHMARK_MEMORY
    (void)start;
    (void)end;
    RecordMemory(store, input, sample, operation, operations, stats);
#else
    double ns = (end - start) * 1000.0;
    printf("%s,%s,%u,%u,%llu,%llu,%llu,%.6f,%.3f,%.9f\n", BACKENDS[store->m_Kind], operation, input->m_Count, sample, (unsigned long long)operations, (unsigned long long)stats.m_Hits, (unsigned long long)stats.m_Batches, ns / 1000000, operations ? ns / operations : 0, stats.m_Sum);
#endif
}

void ResetValues(Backend* store, const Fixture* input)
{
    for (uint32_t ti = 0; ti < TYPE_COUNT; ++ti)
    {
        const TypeInput* t = &input->m_Types[ti];
        if (!store->m_Kind)
            for (uint32_t r = 0; r < t->m_Count; ++r)
                Check(DataResetRow(store->m_Data, store->m_Ids[t->m_Offset + r]) == DATA_RESULT_OK, "reset row");
        else
        {
            for (uint32_t r = 0; r < t->m_Count; ++r)
            {
                uint64_t id = store->m_Ids[t->m_Offset + r];
                if (store->m_Kind == 1)
                    memcpy(ecs_get_mut_id(store->m_World, id, store->m_Types[ti]), t->m_Native + (size_t)r * t->m_NativeStride, t->m_NativeStride);
                else
                {
                    for (uint32_t f = 0; f < t->m_FieldCount; ++f)
                        memcpy(FlecsField(store, t, ti, r, t->m_Fields[f].m_Field), t->m_Columns[f] + (size_t)r * t->m_Fields[f].m_NativeSize, t->m_Fields[f].m_NativeSize);
                }
            }
        }
    }
}

void ValidateHealth(Backend* store, const Fixture* input, int radius, uint32_t writes)
{
    for (uint32_t ti = 0; ti < TYPE_COUNT; ++ti)
    {
        const TypeInput* t = &input->m_Types[ti];
        uint32_t         hp = FindField(t, HEALTH), pos = FindField(t, POSITION);
        if (hp == UINT32_MAX)
            continue;
        for (uint32_t r = 0; r < t->m_Count; ++r)
        {
            double               actual;
            if (!store->m_Kind)
            {
                Check(DataFieldGetNumber(store->m_Data, store->m_Ids[t->m_Offset + r], g_Fields[HEALTH], &actual) == DATA_RESULT_OK, "validate health read");
            }
            else
                actual = *(double*)FlecsField(store, t, ti, r, HEALTH);
            Check(actual == Damaged(*(const double*)FixtureField(t, r, hp), Hit(((const Vector3*)FixtureField(t, r, pos))->m_Values, radius) ? writes : 0), "per-instance damage");
        }
    }
}

void MeasureQuery(Backend* store, const Fixture* input, Query* query, uint32_t sample, const char* name, QueryBenchmark benchmark, Stats expected, uint32_t passes)
{
    Stats    total = {};
    uint64_t start = BeginOperation();
    for (uint32_t i = 0; i < passes; ++i)
    {
        Stats stats = benchmark(store, input, query);
        total.m_Sum += stats.m_Sum;
        total.m_Rows += stats.m_Rows;
        total.m_Hits += stats.m_Hits;
        total.m_Batches += stats.m_Batches;
        total.m_Error |= stats.m_Error;
    }
    uint64_t end = EndOperation();
    expected.m_Sum *= passes;
    expected.m_Rows *= passes;
    expected.m_Hits *= passes;
    Validate(total, expected);
    Record(store, input, sample, name, start, end, total.m_Rows, total);
}

ecs_entity_t Tag(ecs_world_t* world, uint64_t hash)
{
    // Hashes are names in this adapter, never forced into Flecs' entity-ID space.
    char name[32];
    snprintf(name, sizeof(name), "tag_%016llx", (unsigned long long)hash);
    ecs_entity_desc_t desc = { .name = name };
    return ecs_entity_init(world, &desc);
}

void DestroyQuery(Query* query)
{
    if (query->m_Data)
        DataDestroyQuery(query->m_Data);
    for (uint32_t i = 0; i < query->m_Count; ++i)
        ecs_query_fini(query->m_Flecs[i]);
}
