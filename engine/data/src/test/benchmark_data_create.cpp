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

static uint32_t FlecsColumns(Backend* store, const TypeInput* t, uint32_t ti, uint32_t start, ecs_id_t* ids, void** data)
{
    uint32_t count = 0;
    if (store->m_Kind == 1)
    {
        ids[count] = store->m_Types[ti];
        data[count++] = t->m_Native + (size_t)start * t->m_NativeStride;
    }
    else
    {
        ids[count] = store->m_Types[ti];
        data[count++] = 0;
        for (uint32_t f = 0; f < t->m_FieldCount; ++f)
        {
            ids[count] = store->m_Properties[t->m_Fields[f].m_Property];
            data[count++] = t->m_Columns[f] + (size_t)start * t->m_Fields[f].m_NativeSize;
        }
    }
    ids[count] = store->m_Owner;
    data[count++] = t->m_Owners + start;
    ids[count] = store->m_Component;
    data[count++] = t->m_ComponentIds + start;
    for (uint32_t i = 0; i < t->m_TagCount; ++i)
    {
        ids[count] = store->m_Tags[ti][i];
        data[count++] = 0;
    }
    return count;
}

int CreateBulk_Defold(Backend* store, const Fixture* input, uint32_t ti, uint32_t start, uint32_t count)
{
    const TypeInput* t = &input->m_Types[ti];
    return DataAddRows(store->m_Data, t->m_Type, t->m_Rows + start, count, store->m_Ids + t->m_Offset + start);
}

int CreateIndividual_Defold(Backend* store, const Fixture* input, uint32_t ti, uint32_t start, uint32_t count)
{
    const TypeInput* t = &input->m_Types[ti];
    uint64_t*        output = store->m_Ids + t->m_Offset + start;
    int              error = 0;
    for (uint32_t r = 0; r < count; ++r)
    {
        const DataRowDesc* row = &t->m_Rows[start + r];
        error |= DataAddRow(store->m_Data, t->m_Type, row->m_Owner, row->m_Values, row->m_ValueCount, &output[r], row->m_ComponentId);
    }
    return error;
}

int CreateBulk_Flecs(Backend* store, const Fixture* input, uint32_t ti, uint32_t start, uint32_t count)
{
    const TypeInput* t = &input->m_Types[ti];
    uint64_t*        output = store->m_Ids + t->m_Offset + start;
    ecs_id_t         ids[16] = {};
    void*            data[16] = {};
    uint32_t         fields = FlecsColumns(store, t, ti, start, ids, data);
    ecs_bulk_desc_t  desc = {};
    desc.count = count;
    memcpy(desc.ids, ids, fields * sizeof(ecs_id_t));
    desc.data = data;
    const ecs_entity_t* added = ecs_bulk_init(store->m_World, &desc);
    if (!added)
        return 1;
    memcpy(output, added, count * sizeof(ecs_entity_t));
    return 0;
}

int CreateBulk(Backend* store, const Fixture* input, uint32_t ti, uint32_t start, uint32_t count)
{
    return store->m_Kind ? CreateBulk_Flecs(store, input, ti, start, count) : CreateBulk_Defold(store, input, ti, start, count);
}

int CreateIndividual_Flecs(Backend* store, const Fixture* input, uint32_t ti, uint32_t start, uint32_t count)
{
    const TypeInput* t = &input->m_Types[ti];
    uint64_t*        output = store->m_Ids + t->m_Offset + start;
    ecs_id_t         ids[16] = {};
    void*            data[16] = {};
    uint32_t         fields = FlecsColumns(store, t, ti, start, ids, data);
    uint32_t         sizes[16] = {};
    for (uint32_t f = 0; f < fields; ++f)
    {
        const ecs_type_info_t* info = ecs_get_type_info(store->m_World, ids[f]);
        sizes[f] = info ? info->size : 0;
    }
    for (uint32_t r = 0; r < count; ++r)
    {
        ecs_value_t values[17] = {};
        for (uint32_t f = 0; f < fields; ++f)
        {
            values[f].type = ids[f];
            values[f].ptr = data[f] ? (uint8_t*)data[f] + (size_t)r * sizes[f] : 0;
        }
        output[r] = ecs_insert_w_values(store->m_World, values);
        if (!output[r])
            return 1;
    }
    return 0;
}

int CreateIndividual(Backend* store, const Fixture* input, uint32_t ti, uint32_t start, uint32_t count)
{
    return store->m_Kind ? CreateIndividual_Flecs(store, input, ti, start, count) : CreateIndividual_Defold(store, input, ti, start, count);
}
