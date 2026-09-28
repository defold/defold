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

#include "benchmark_data_common.h"

static Stats PackedPopulate_Defold(Backend* store, const Fixture* input, HDataBlobInstance* instances)
{
    Stats    stats = {};
    uint32_t group = 0;
    for (uint32_t ti = 0; ti < TYPE_COUNT; ++ti)
    {
        const TypeInput* t = &input->m_Types[ti];
        for (uint32_t r = 0; r < t->m_Count; r += input->m_GroupSize)
            stats.m_Error |= DataAddBlob(store->m_Data, input->m_Blobs[ti], t->m_Owners[r], &instances[group++]);
    }
    // Include returning/mapping every runtime row ID, as for Flecs population.
    DataQueryDesc desc = {};
    HDataQuery    ids;
    stats.m_Error |= DataCreateQuery(store->m_Data, &desc, &ids);
    DataStoreLock(store->m_Data);
    DataIterator it = DataQueryIter(ids);
    while (DataIterNext(&it) == DATA_RESULT_OK)
    {
        DataRowIterator rows = DataIterRows(&it);
        uint64_t        first = 0;
        uint32_t        row = 0;
        while (DataRowIterNext(&rows) == DATA_RESULT_OK)
        {
            if (!row)
                first = DataRowIterGetOwnerId(&rows) - 1;
            store->m_Ids[first + row++] = DataRowIterGetId(&rows);
        }
    }
    DataStoreUnlock(store->m_Data);
    DataDestroyQuery(ids);
    return stats;
}

static Stats PackedPopulate_Flecs(Backend* store, const Fixture* input)
{
    Stats stats = {};
    for (uint32_t ti = 0; ti < TYPE_COUNT; ++ti)
        stats.m_Error |= CreateBulk_Flecs(store, input, ti, 0, input->m_Types[ti].m_Count);
    return stats;
}

static void PackedReset_Defold(HDataBlobInstance* instances, uint32_t groups)
{
    for (uint32_t i = 0; i < groups; ++i)
        DataResetBlob(instances[i]);
}

static void PackedUnload_Defold(HDataBlobInstance* instances, uint32_t groups)
{
    // Release registrations in reverse creation order.
    for (uint32_t i = groups; i; --i)
        DataRemoveBlob(instances[i - 1]);
}

void RunPacked(const Fixture* input, uint32_t kind, uint32_t sample)
{
    Backend  store = CreateBackend(input, kind, sample, "setup_packed");
    uint32_t groups = input->m_Count / input->m_GroupSize;
    SetBenchmarkMemoryDomain(BENCHMARK_MEMORY_FIXTURE);
    HDataBlobInstance* instances = kind ? 0 : new HDataBlobInstance[groups];
    SetBenchmarkMemoryDomain(BENCHMARK_MEMORY_BACKEND);
    Stats    stats = {};
    uint64_t start = BeginOperation();
    stats = kind ? PackedPopulate_Flecs(&store, input) : PackedPopulate_Defold(&store, input, instances);
    Record(&store, input, sample, "packed_populate_and_ids", start, EndOperation(), input->m_Count, stats);
    start = BeginOperation();
    Query health = CreateHealthPositionQuery(&store, input, 0);
    Query light = CreateLightColorQuery(&store, input, g_LightTag);
    Record(&store, input, sample, "packed_create_queries", start, EndOperation(), 2, stats);
    MeasureLightColor(&store, input, &light, sample, "packed_light_color");
    const uint32_t densities[] = { 0, 1, 10 };
    for (uint32_t d = 0; d < 3; ++d)
    {
        uint32_t percent = densities[d];
        if (percent)
        {
            MeasurePackedAccess(&store, input, sample, percent, 1);
            MeasurePackedAccess(&store, input, sample, percent, 2);
        }
        MeasurePackedAccess(&store, input, sample, percent, 0);
        uint32_t changed = input->m_Count / 100 * percent;
        uint32_t health_changes = 0;
        for (uint32_t i = 0; i < changed; ++i)
            health_changes += ScalarProperty(input->m_Order[i].m_Type) == HEALTH;
        char name[64];
        snprintf(name, sizeof(name), "packed_%upct_health_query", percent);
        MeasureHealthPosition(&store, input, &health, sample, name, false, health_changes * 2);
        if (!kind)
        {
            DataMemoryStats memory;
            GetDataMemoryStats(store.m_Data, &memory);
            Check(memory.m_Rows == input->m_Count && memory.m_Tables == TYPE_COUNT && memory.m_Instances == groups && !memory.m_PayloadBlocks, "packed numeric updates allocate no payloads");
            if (sample)
                printf("# arena,sample=%u,density=%u,tables=%llu,rows=%llu,mutable_bytes=%llu,blocks=%llu,used=%llu,capacity=%llu\n", sample, percent, (unsigned long long)memory.m_Tables, (unsigned long long)memory.m_Rows, (unsigned long long)memory.m_ValueBytes, (unsigned long long)memory.m_PayloadBlocks, (unsigned long long)memory.m_PayloadUsed, (unsigned long long)memory.m_PayloadCapacity);
            start = BeginOperation();
            PackedReset_Defold(instances, groups);
            snprintf(name, sizeof(name), "packed_%upct_reset", percent);
            Record(&store, input, sample, name, start, EndOperation(), input->m_Count, stats);
            GetDataMemoryStats(store.m_Data, &memory);
            Check(!memory.m_PayloadBlocks && !memory.m_PayloadCapacity, "reset releases replacement payloads");
        }
        else
            ResetValues(&store, input);
        ValidateHealth(&store, input, -1, 0);
    }
    BeginBenchmarkMemoryOperation();
    DestroyQuery(&health);
    DestroyQuery(&light);
    EndBenchmarkMemoryOperation();
    RecordMemory(&store, input, sample, "destroy_queries", 2, Stats());
    start = BeginOperation();
    if (!kind)
    {
        PackedUnload_Defold(instances, groups);
        Record(&store, input, sample, "packed_unload_no_queries", start, EndOperation(), input->m_Count, stats);
    }
    delete[] instances;
    DestroyBackend(&store, input, sample, "destroy_packed");
}
