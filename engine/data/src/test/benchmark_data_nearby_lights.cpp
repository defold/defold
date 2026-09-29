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
#include "benchmark_data_common.h"

Query CreateNearbyLightsQuery(Backend* store)
{
    Query out = {};
    if (!store->m_Kind)
    {
        DataQueryField fields[] = {
            { .m_Field = g_Fields[POSITION], .m_Type = DATA_VALUE_TYPE_VECTOR3 },
            { .m_Field = g_Fields[LIGHT], .m_Type = DATA_VALUE_TYPE_VECTOR3, .m_Path = &g_Fields[COLOR], .m_PathCount = 1 },
            { .m_Field = g_Fields[LIGHT], .m_Type = DATA_VALUE_TYPE_NUMBER, .m_Path = &g_Fields[INTENSITY], .m_PathCount = 1 }
        };
        DataQueryDesc desc = { .m_Fields = fields, .m_FieldCount = 3 };
        Check(DataCreateQuery(store->m_Data, &desc, &out.m_Data) == DATA_RESULT_OK, "nearby lights query");
        out.m_PositionField = DataQueryFindField(out.m_Data, &fields[0]);
        out.m_ColorField = DataQueryFindField(out.m_Data, &fields[1]);
        out.m_IntensityField = DataQueryFindField(out.m_Data, &fields[2]);
    }
    else
    {
        ecs_query_desc_t desc = {
            .terms = { { .id = store->m_Fields[POSITION], .inout = EcsIn }, { .id = store->m_Fields[LIGHT], .inout = EcsIn } },
            .cache_kind = EcsQueryCacheAuto
        };
        out.m_Flecs[0] = ecs_query_init(store->m_World, &desc);
        out.m_Count = 1;
        Check(out.m_Flecs[0] != 0, "Flecs nearby lights query");
    }
    return out;
}

static double Contribution(const float* position, const float* color, double intensity)
{
    float distance = position[0] * position[0] + position[1] * position[1] + position[2] * position[2];
    return SumVector(color) * intensity * (1.0 - distance / 2500.0);
}

static Stats NearbyLights_Defold(Backend* store, Query* query)
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
            const DataVector3* position = DataRowIterGetVector3(&rows, query->m_PositionField);
            if (Hit(position->m_Values, 50))
            {
                const DataVector3* color = DataRowIterGetVector3(&rows, query->m_ColorField);
                stats.m_Sum += Contribution(position->m_Values, color->m_Values, *DataRowIterGetNumber(&rows, query->m_IntensityField));
                ++stats.m_Hits;
            }
            ++stats.m_Rows;
        }
    }
    DataStoreUnlock(store->m_Data);
    return stats;
}

static Stats NearbyLights_Flecs(Backend* store, Query* query)
{
    Stats      stats = {};
    ecs_iter_t it = ecs_query_iter(store->m_World, query->m_Flecs[0]);
    while (ecs_query_next(&it))
    {
        ++stats.m_Batches;
        const Vector3* positions = (const Vector3*)ecs_field_w_size(&it, sizeof(Vector3), 0);
        const Light*   lights = (const Light*)ecs_field_w_size(&it, sizeof(Light), 1);
        for (int32_t r = 0; r < it.count; ++r)
        {
            if (Hit(positions[r].m_Values, 50))
            {
                stats.m_Sum += Contribution(positions[r].m_Values, lights[r].color.m_Values, lights[r].intensity);
                ++stats.m_Hits;
            }
            ++stats.m_Rows;
        }
    }
    return stats;
}

void MeasureNearbyLights(Backend* store, const Fixture* input, Query* query, uint32_t sample)
{
    Stats expected = {};
    for (uint32_t t = 0; t < 2; ++t)
    {
        const TypeInput* type = &input->m_Types[t];
        for (uint32_t r = 0; r < type->m_Count; ++r)
        {
            const float* position = ((const Vector3*)type->m_Columns[FindField(type, POSITION)])[r].m_Values;
            const Light& light = ((const Light*)type->m_Columns[FindField(type, LIGHT)])[r];
            if (Hit(position, 50))
            {
                expected.m_Sum += Contribution(position, light.color.m_Values, light.intensity);
                ++expected.m_Hits;
            }
            ++expected.m_Rows;
        }
    }
    uint64_t start = BeginOperation();
    Stats    actual = store->m_Kind ? NearbyLights_Flecs(store, query) : NearbyLights_Defold(store, query);
    uint64_t end = EndOperation();
    Check(actual.m_Rows == expected.m_Rows && actual.m_Hits == expected.m_Hits && fabs(actual.m_Sum - expected.m_Sum) <= 1e-7 * (1 + fabs(expected.m_Sum)), "nearby light contribution");
    Record(store, input, sample, "nearby_lights", start, end, actual.m_Rows, actual);
}

#ifdef DATA_BENCHMARK_ENTT
#include "benchmark_data_entt.h"

CoreEnttLights CreateNearbyLightsQuery_EnTT(CoreEnttStore* store)
{
    return store->m_Registry.view<const CoreEnttPosition, const CoreEnttLight>();
}

Stats NearbyLights_EnTT(CoreEnttLights* query)
{
    Stats stats = {};
    query->each([&stats](const CoreEnttPosition& position, const CoreEnttLight& light) {
        if (Hit(position.m_Value.m_Values, 50))
        {
            stats.m_Sum += Contribution(position.m_Value.m_Values, light.m_Value.color.m_Values, light.m_Value.intensity);
            ++stats.m_Hits;
        }
        ++stats.m_Rows;
    });
    return stats;
}

#endif
