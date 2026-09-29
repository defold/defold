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

// Read both fields without a spatial filter; tag optionally restricts the scan to enemies.
Query CreateHealthPositionQuery(Backend* store, const Fixture* input, uint64_t tag)
{
    Query out = {};
    out.m_Tag = tag;
    if (!store->m_Kind)
    {
        DataQueryField fields[] = {
            { .m_Field = g_Fields[HEALTH], .m_Type = DATA_VALUE_TYPE_NUMBER },
            { .m_Field = g_Fields[POSITION], .m_Type = DATA_VALUE_TYPE_VECTOR3 }
        };
        DataQueryDesc desc = { .m_AllTags = tag ? &tag : 0, .m_AllTagCount = tag ? 1u : 0u, .m_Fields = fields, .m_FieldCount = 2 };
        Check(DataCreateQuery(store->m_Data, &desc, &out.m_Data) == DATA_RESULT_OK, "create health position query");
        out.m_HealthField = DataQueryFindField(out.m_Data, &fields[0]);
        out.m_PositionField = DataQueryFindField(out.m_Data, &fields[1]);
    }
    else
    {
        for (uint32_t ti = 0; ti < TYPE_COUNT; ++ti)
        {
            const TypeInput* t = &input->m_Types[ti];
            if (store->m_Kind == 1 && (!HasTag(t, tag) || FindField(t, HEALTH) == UINT32_MAX || FindField(t, POSITION) == UINT32_MAX))
                continue;
            ecs_query_desc_t desc = {
                .terms = { { .id = store->m_Kind == 1 ? store->m_Types[ti] : store->m_Fields[HEALTH], .inout = EcsIn } },
                .cache_kind = EcsQueryCacheAuto,
            };
            uint32_t terms = 1;
            if (store->m_Kind == 2)
            {
                desc.terms[terms].id = store->m_Fields[POSITION];
                desc.terms[terms++].inout = EcsIn;
            }
            if (tag)
            {
                desc.terms[terms].id = Tag(store->m_World, tag);
                desc.terms[terms].inout = EcsIn;
            }
            out.m_Types[out.m_Count] = ti;
            out.m_Flecs[out.m_Count] = ecs_query_init(store->m_World, &desc);
            Check(out.m_Flecs[out.m_Count++] != 0, "create Flecs health position query");
            if (store->m_Kind == 2)
                break;
        }
    }
    return out;
}

// Scan position and health.
Stats HealthPosition_Defold(Backend* store, const Fixture* input, Query* query)
{
    (void)input;
    Stats    stats = {};
    uint32_t position_field = query->m_PositionField;
    uint32_t health_field = query->m_HealthField;
    DataStoreLock(store->m_Data);
    DataIterator it = DataQueryIter(query->m_Data);
    DataResult   result;
    while ((result = DataIterNext(&it)) == DATA_RESULT_OK)
    {
        ++stats.m_Batches;
        DataRowIterator rows = DataIterRows(&it);
        DataResult      row_result;
        while ((row_result = DataRowIterNext(&rows)) == DATA_RESULT_OK)
        {
            const DataVector3* position = DataRowIterGetVector3(&rows, position_field);
            double             health = *DataRowIterGetNumber(&rows, health_field);
            stats.m_Sum += health + SumVector(position->m_Values);
            ++stats.m_Rows;
        }
        stats.m_Error |= row_result == DATA_RESULT_END ? 0 : 1;
    }
    stats.m_Error |= result == DATA_RESULT_END ? 0 : 1;
    DataStoreUnlock(store->m_Data);
    return stats;
}

Stats HealthPosition_Flecs(Backend* store, const Fixture* input, Query* query)
{
    Stats stats = {};
    for (uint32_t q = 0; q < query->m_Count; ++q)
    {
        const TypeInput* t = &input->m_Types[query->m_Types[q]];
        uint32_t         first_offset = 0, second_offset = 0;
        uint32_t         first_stride = sizeof(double), second_stride = sizeof(Vector3);
        if (store->m_Kind == 1)
        {
            first_stride = second_stride = t->m_NativeStride;
            first_offset = t->m_Fields[FindField(t, HEALTH)].m_NativeOffset;
            second_offset = t->m_Fields[FindField(t, POSITION)].m_NativeOffset;
        }
        ecs_iter_t it = ecs_query_iter(store->m_World, query->m_Flecs[q]);
        while (ecs_query_next(&it))
        {
            ++stats.m_Batches;
            uint8_t*       first = (uint8_t*)ecs_field_w_size(&it, first_stride, 0);
            const uint8_t* second = store->m_Kind == 1 ? first : (const uint8_t*)ecs_field_w_size(&it, sizeof(Vector3), 1);
            for (int32_t r = 0; r < it.count; ++r)
            {
                double*      health = (double*)(first + (size_t)r * first_stride + first_offset);
                const float* position = (const float*)(second + (size_t)r * second_stride + second_offset);
                stats.m_Sum += *health + SumVector(position);
            }
            stats.m_Rows += it.count;
        }
    }
    return stats;
}

static Stats ExpectedHealthPosition(const Fixture* input, const Query* query, bool extra)
{
    Stats stats = {};
    for (uint32_t ti = 0; ti < TYPE_COUNT; ++ti)
    {
        const TypeInput* t = &input->m_Types[ti];
        uint32_t         f = FindField(t, HEALTH);
        if (f == UINT32_MAX || !HasTag(t, query->m_Tag))
            continue;
        uint32_t position = FindField(t, POSITION);
        uint32_t count = t->m_Count + (extra ? t->m_Extra : 0);
        for (uint32_t r = 0; r < count; ++r)
        {
            const DataValueData* values = &t->m_Values[(size_t)r * t->m_FieldCount];
            stats.m_Sum += values[f].m_Number + SumVector(values[position].m_Vector3);
            ++stats.m_Rows;
        }
    }
    return stats;
}

void MeasureHealthPosition(Backend* store, const Fixture* input, Query* query, uint32_t sample, const char* name, bool extra, double extra_sum)
{
    Stats expected = ExpectedHealthPosition(input, query, extra);
    expected.m_Sum += extra_sum;
    QueryBenchmark benchmark = store->m_Kind ? HealthPosition_Flecs : HealthPosition_Defold;
    MeasureQuery(store, input, query, sample, name, benchmark, expected, READ_PASSES);
}
