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

Query CreateMovementQuery(Backend* store)
{
    Query out = {};
    if (!store->m_Kind)
    {
        DataQueryField fields[] = {
            { .m_Field = g_Fields[POSITION], .m_Type = DATA_VALUE_TYPE_VECTOR3, .m_Access = DATA_ACCESS_READ_WRITE },
            { .m_Field = g_Fields[VELOCITY], .m_Type = DATA_VALUE_TYPE_VECTOR3 }
        };
        DataQueryDesc desc = { .m_Fields = fields, .m_FieldCount = 2 };
        Check(DataCreateQuery(store->m_Data, &desc, &out.m_Data) == DATA_RESULT_OK, "movement query");
        out.m_PositionField = DataQueryFindField(out.m_Data, &fields[0]);
        out.m_VelocityField = DataQueryFindField(out.m_Data, &fields[1]);
    }
    else
    {
        ecs_query_desc_t desc = {
            .terms = { { .id = store->m_Fields[POSITION], .inout = EcsInOut }, { .id = store->m_Fields[VELOCITY], .inout = EcsIn } },
            .cache_kind = EcsQueryCacheAuto
        };
        out.m_Flecs[0] = ecs_query_init(store->m_World, &desc);
        out.m_Count = 1;
        Check(out.m_Flecs[0] != 0, "Flecs movement query");
    }
    return out;
}

static Stats Movement_Defold(Backend* store, const Fixture*, Query* query)
{
    Stats stats = {};
    DataStoreLock(store->m_Data);
    DataIterator it = DataQueryIter(query->m_Data);
    while (DataIterNext(&it) == DATA_RESULT_OK)
    {
        ++stats.m_Batches;
        DataRowIterator rows = DataIterRows(&it);
        while (DataRowIterNext(&rows) == DATA_RESULT_OK)
        {
            DataVector3*       position = DataRowIterGetVector3Mut(&rows, query->m_PositionField);
            const DataVector3* velocity = DataRowIterGetVector3(&rows, query->m_VelocityField);
            for (uint32_t axis = 0; axis < 3; ++axis)
                position->m_Values[axis] += velocity->m_Values[axis] * 0.015625f;
            stats.m_Sum += SumVector(position->m_Values);
            ++stats.m_Rows;
        }
    }
    DataStoreUnlock(store->m_Data);
    return stats;
}

static Stats Movement_Flecs(Backend* store, const Fixture*, Query* query)
{
    Stats      stats = {};
    ecs_iter_t it = ecs_query_iter(store->m_World, query->m_Flecs[0]);
    while (ecs_query_next(&it))
    {
        ++stats.m_Batches;
        Vector3*       position = (Vector3*)ecs_field_w_size(&it, sizeof(Vector3), 0);
        const Vector3* velocity = (const Vector3*)ecs_field_w_size(&it, sizeof(Vector3), 1);
        for (int32_t r = 0; r < it.count; ++r)
        {
            for (uint32_t axis = 0; axis < 3; ++axis)
                position[r].m_Values[axis] += velocity[r].m_Values[axis] * 0.015625f;
            stats.m_Sum += SumVector(position[r].m_Values);
            ++stats.m_Rows;
        }
    }
    return stats;
}

void MeasureMovement(Backend* store, const Fixture* input, Query* query, uint32_t sample)
{
    Stats expected = {};
    for (uint32_t t = 2; t <= 3; ++t)
    {
        const TypeInput* type = &input->m_Types[t];
        for (uint32_t r = 0; r < type->m_Count; ++r)
        {
            Vector3        position = ((const Vector3*)type->m_Columns[FindField(type, POSITION)])[r];
            const Vector3& velocity = ((const Vector3*)type->m_Columns[FindField(type, VELOCITY)])[r];
            for (uint32_t axis = 0; axis < 3; ++axis)
                position.m_Values[axis] += velocity.m_Values[axis] * 0.015625f;
            expected.m_Sum += SumVector(position.m_Values);
            ++expected.m_Rows;
        }
    }
    MeasureQuery(store, input, query, sample, "movement", store->m_Kind ? Movement_Flecs : Movement_Defold, expected, 1);
    for (uint32_t t = 2; t <= 3; ++t)
    {
        const TypeInput* type = &input->m_Types[t];
        for (uint32_t r = 0; r < type->m_Count; ++r)
        {
            DataVector3 actual;
            uint64_t    id = store->m_Ids[type->m_Offset + r];
            if (!store->m_Kind)
                Check(DataFieldGetVector3(store->m_Data, id, g_Fields[POSITION], &actual) == DATA_RESULT_OK, "movement position");
            else
                memcpy(&actual, ecs_get_id(store->m_World, id, store->m_Fields[POSITION]), sizeof(actual));
            const Vector3& position = ((const Vector3*)type->m_Columns[FindField(type, POSITION)])[r];
            const Vector3& velocity = ((const Vector3*)type->m_Columns[FindField(type, VELOCITY)])[r];
            for (uint32_t axis = 0; axis < 3; ++axis)
                Check(actual.m_Values[axis] == position.m_Values[axis] + velocity.m_Values[axis] * 0.015625f, "persisted movement");
        }
    }
}

// Repeat the benchmark's exact public-API loop. Restore and validate outside the
// measured interval; the repeated scan is a profiling workload, not a report case.
void ProfileMovement(const Fixture* input, uint32_t kind, uint32_t samples, uint32_t passes)
{
    Backend store = CreateBackend(input, kind, 0, "setup");
    for (uint32_t t = 0; t < TYPE_COUNT; ++t)
        Check(CreateBulk(&store, input, t, 0, input->m_Types[t].m_Count) == 0, "profile bulk insertion");
    Query          query = CreateMovementQuery(&store);
    QueryBenchmark benchmark = kind ? Movement_Flecs : Movement_Defold;
    uint64_t       rows = input->m_Types[2].m_Count + input->m_Types[3].m_Count;
    fprintf(stderr, "Profile ready: %s, Movement, %u passes per sample\n", BACKENDS[kind], passes);
    for (uint32_t sample = 0; sample <= samples; ++sample)
    {
        ResetValues(&store, input);
        Stats    total = {};
        uint64_t start = BeginOperation();
        for (uint32_t pass = 0; pass < passes; ++pass)
        {
            Stats stats = benchmark(&store, input, &query);
            total.m_Sum += stats.m_Sum;
            total.m_Rows += stats.m_Rows;
            total.m_Batches += stats.m_Batches;
        }
        uint64_t end = EndOperation();
        Check(total.m_Rows == rows * passes, "profile movement rows");
        Record(&store, input, sample, "movement", start, end, total.m_Rows, total);
        for (uint32_t t = 2; t <= 3; ++t)
        {
            const TypeInput* type = &input->m_Types[t];
            const Vector3*   positions = (const Vector3*)type->m_Columns[FindField(type, POSITION)];
            const Vector3*   velocities = (const Vector3*)type->m_Columns[FindField(type, VELOCITY)];
            for (uint32_t r = 0; r < type->m_Count; ++r)
            {
                DataVector3 actual;
                if (!kind)
                    Check(DataFieldGetVector3(store.m_Data, store.m_Ids[type->m_Offset + r], g_Fields[POSITION], &actual) == DATA_RESULT_OK, "profile movement position");
                else
                    memcpy(&actual, FlecsField(&store, type, t, r, POSITION), sizeof(actual));
                for (uint32_t axis = 0; axis < 3; ++axis)
                    Check(actual.m_Values[axis] == positions[r].m_Values[axis] + velocities[r].m_Values[axis] * (0.015625f * passes), "profile persisted movement");
            }
        }
    }
    DestroyQuery(&query);
    DestroyBackend(&store, input, 0, "destroy");
}

#ifdef DATA_BENCHMARK_ENTT
#include "benchmark_data_entt.h"

CoreEnttMovement CreateMovementQuery_EnTT(CoreEnttStore* store)
{
    return store->m_Registry.view<CoreEnttPosition, const CoreEnttVelocity>();
}

Stats Movement_EnTT(CoreEnttMovement* query)
{
    Stats stats = {};
    query->each([&stats](CoreEnttPosition& position, const CoreEnttVelocity& velocity) {
        for (uint32_t axis = 0; axis < 3; ++axis)
            position.m_Value.m_Values[axis] += velocity.m_Value.m_Values[axis] * 0.015625f;
        stats.m_Sum += SumVector(position.m_Value.m_Values);
        ++stats.m_Rows;
    });
    return stats;
}

#endif
