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

// Bind light.color once for all matching light rows; the tag selects all lights or SpotLight.
Query CreateLightColorQuery(Backend* store, const Fixture* input, uint64_t tag)
{
    Query out = {};
    out.m_Tag = tag;
    if (!store->m_Kind)
    {
        DataQueryField color = { .m_Field = g_Fields[LIGHT], .m_Type = DATA_VALUE_TYPE_VECTOR3, .m_Path = &g_Fields[COLOR], .m_PathCount = 1 };
        DataQueryDesc  desc = { .m_AllTags = tag ? &tag : 0, .m_AllTagCount = tag ? 1u : 0u, .m_Fields = &color, .m_FieldCount = 1 };
        Check(DataCreateQuery(store->m_Data, &desc, &out.m_Data) == DATA_RESULT_OK, "create light color query");
        out.m_ColorField = DataQueryFindField(out.m_Data, &color);
    }
    else
    {
        for (uint32_t ti = 0; ti < TYPE_COUNT; ++ti)
        {
            const TypeInput* t = &input->m_Types[ti];
            if (store->m_Kind == 1 && (!HasTag(t, tag) || FindField(t, LIGHT) == UINT32_MAX))
                continue;
            ecs_query_desc_t desc = {
                .terms = { { .id = store->m_Kind == 1 ? store->m_Types[ti] : store->m_Fields[LIGHT], .inout = EcsIn } },
                .cache_kind = EcsQueryCacheAuto,
            };
            if (tag)
            {
                desc.terms[1].id = Tag(store->m_World, tag);
                desc.terms[1].inout = EcsIn;
            }
            out.m_Types[out.m_Count] = ti;
            out.m_Flecs[out.m_Count] = ecs_query_init(store->m_World, &desc);
            Check(out.m_Flecs[out.m_Count++] != 0, "create Flecs light color query");
            if (store->m_Kind == 2)
                break;
        }
    }
    return out;
}

// Sum light.color for the query's matching light rows.
Stats LightColor_Defold(Backend* store, const Fixture* input, Query* query)
{
    (void)input;
    Stats    stats = {};
    uint32_t color_field = query->m_ColorField;
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
            const DataVector3* color = DataFieldGetVector3(&rows, color_field);
            stats.m_Sum += SumVector(color->m_Values);
            ++stats.m_Rows;
        }
        stats.m_Error |= row_result == DATA_RESULT_END ? 0 : 1;
    }
    stats.m_Error |= result == DATA_RESULT_END ? 0 : 1;
    DataStoreUnlock(store->m_Data);
    return stats;
}

Stats LightColor_Flecs(Backend* store, const Fixture* input, Query* query)
{
    Stats stats = {};
    for (uint32_t q = 0; q < query->m_Count; ++q)
    {
        const TypeInput* t = &input->m_Types[query->m_Types[q]];
        uint32_t         first_offset = 0;
        uint32_t         first_stride = sizeof(Light), second_stride = sizeof(Vector3);
        if (store->m_Kind == 1)
        {
            first_stride = second_stride = t->m_NativeStride;
            first_offset = t->m_Fields[FindField(t, LIGHT)].m_NativeOffset;
        }
        ecs_iter_t it = ecs_query_iter(store->m_World, query->m_Flecs[q]);
        while (ecs_query_next(&it))
        {
            ++stats.m_Batches;
            uint8_t* first = (uint8_t*)ecs_field_w_size(&it, first_stride, 0);
            for (int32_t r = 0; r < it.count; ++r)
            {
                const Light* light = (const Light*)(first + (size_t)r * first_stride + first_offset);
                stats.m_Sum += SumVector(light->color.m_Values);
            }
            stats.m_Rows += it.count;
        }
    }
    return stats;
}

static Stats ExpectedLightColor(const Fixture* input, const Query* query, bool extra)
{
    Stats stats = {};
    for (uint32_t ti = 0; ti < TYPE_COUNT; ++ti)
    {
        const TypeInput* t = &input->m_Types[ti];
        uint32_t         f = FindField(t, LIGHT);
        if (f == UINT32_MAX || !HasTag(t, query->m_Tag))
            continue;
        uint32_t count = t->m_Count + (extra ? t->m_Extra : 0);
        for (uint32_t r = 0; r < count; ++r)
        {
            stats.m_Sum += SumVector(t->m_LightValues[r * 2].m_Vector3);
            ++stats.m_Rows;
        }
    }
    return stats;
}

void MeasureLightColor(Backend* store, const Fixture* input, Query* query, uint32_t sample, const char* name, bool extra, uint32_t passes)
{
    Stats          expected = ExpectedLightColor(input, query, extra);
    QueryBenchmark benchmark = store->m_Kind ? LightColor_Flecs : LightColor_Defold;
    MeasureQuery(store, input, query, sample, name, benchmark, expected, passes);
}
