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

static void ValidateQueryRows(Backend* store, const Fixture* input, Query* query, uint32_t types, uint32_t removed)
{
    uint32_t expected = 0, actual = 0;
    for (uint32_t t = 0; t < TYPE_COUNT; ++t)
        if (types & (1u << t))
            expected += input->m_Types[t].m_Count + input->m_Types[t].m_Extra;
    for (uint32_t i = 0; i < removed; ++i)
        expected -= (types >> input->m_Order[i].m_Type) & 1;
    if (!store->m_Kind)
    {
        DataStoreLock(store->m_Data);
        DataIterator it = DataQueryIter(query->m_Data);
        while (DataIterNext(&it) == DATA_RESULT_OK)
        {
            DataRowIterator rows = DataIterRows(&it);
            while (DataRowIterNext(&rows) == DATA_RESULT_OK)
                ++actual;
        }
        DataStoreUnlock(store->m_Data);
    }
    else
    {
        ecs_iter_t it = ecs_query_iter(store->m_World, query->m_Flecs[0]);
        while (ecs_query_next(&it))
            actual += it.count;
    }
    Check(actual == expected, "live query membership after spawn/despawn");
}

// Seven standalone operations; the eighth is the separately TSAN-validated
// threaded update. Each sample owns a fresh store and three live case queries.
void RunCore(const Fixture* input, uint32_t kind, uint32_t sample)
{
    // Keep backend allocation ownership active until DestroyBackend. Operation
    // boundaries snapshot counters; they do not change the allocation domain.
    Backend  store = CreateBackend(input, kind, 0, "setup");
    Stats    stats = {};
    uint64_t start = BeginOperation();
    for (uint32_t t = 0; t < TYPE_COUNT; ++t)
        stats.m_Error |= CreateBulk(&store, input, t, 0, input->m_Types[t].m_Count);
    Record(&store, input, sample, "create_population", start, EndOperation(), input->m_Count, stats);

    Query movement = CreateMovementQuery(&store);
    Query explosion = CreateExplosionQuery(&store, input);
    Query lights = CreateNearbyLightsQuery(&store);
    MeasureMovement(&store, input, &movement, sample);
    ResetValues(&store, input);
    MeasureExplosion(&store, input, &explosion, sample);
    ValidateHealth(&store, input, 50, 1);
    ResetValues(&store, input);
    MeasureNearbyLights(&store, input, &lights, sample);
    MeasurePositionLookup(&store, input, sample);

    start = BeginOperation();
    stats = kind ? AddInstances_Flecs(&store, input) : AddInstances_Defold(&store, input);
    Record(&store, input, sample, "spawn_wave", start, EndOperation(), input->m_Total - input->m_Count, stats);
    // Every created ID must resolve to its original value; validates both initial
    // population and the new wave without adding another measured workload.
    for (uint32_t t = 0; t < TYPE_COUNT; ++t)
    {
        const TypeInput* type = &input->m_Types[t];
        for (uint32_t r = 0; r < type->m_Count + type->m_Extra; ++r)
        {
            DataVector3 actual;
            uint64_t    id = store.m_Ids[type->m_Offset + r];
            if (!kind)
                Check(DataGetFieldVector3(store.m_Data, id, g_Fields[POSITION], &actual) == DATA_RESULT_OK, "spawn position");
            else
                memcpy(&actual, ecs_get_id(store.m_World, id, store.m_Fields[POSITION]), sizeof(actual));
            const DataValueData* expected = &type->m_Values[(size_t)r * type->m_FieldCount + FindField(type, POSITION)];
            Check(!memcmp(&actual, expected->m_Vector3, sizeof(actual)), "created position matches input");
        }
    }
    ValidateQueryRows(&store, input, &movement, (1u << 2) | (1u << 3), 0);
    ValidateQueryRows(&store, input, &explosion, (1u << 2) | (1u << 3) | (1u << 5), 0);
    ValidateQueryRows(&store, input, &lights, (1u << 0) | (1u << 1), 0);
    start = BeginOperation();
    stats = kind ? RemoveInstances_Flecs(&store, input) : RemoveInstances_Defold(&store, input);
    Record(&store, input, sample, "despawn_wave", start, EndOperation(), input->m_Count / 100, stats);
    for (uint32_t i = 0; i < input->m_Count / 100; ++i)
    {
        RowKey      key = input->m_Order[i];
        uint64_t    id = store.m_Ids[input->m_Types[key.m_Type].m_Offset + key.m_Row];
        DataVector3 value;
        Check(kind ? !ecs_is_alive(store.m_World, id) : DataGetFieldVector3(store.m_Data, id, g_Fields[POSITION], &value) == DATA_RESULT_NOT_FOUND, "despawned ID is stale");
    }
    ValidateQueryRows(&store, input, &movement, (1u << 2) | (1u << 3), input->m_Count / 100);
    ValidateQueryRows(&store, input, &explosion, (1u << 2) | (1u << 3) | (1u << 5), input->m_Count / 100);
    ValidateQueryRows(&store, input, &lights, (1u << 0) | (1u << 1), input->m_Count / 100);
    DestroyQuery(&movement);
    DestroyQuery(&explosion);
    DestroyQuery(&lights);
    DestroyBackend(&store, input, 0, "destroy");
}
