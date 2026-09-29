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

FieldId ScalarField(uint32_t type)
{
    static const FieldId fields[] = { RANGE, RANGE, HEALTH, HEALTH, AMOUNT, HEALTH };
    return fields[type];
}

static Stats PackedRead_Defold(Backend* store, const Fixture* input, uint32_t count)
{
    Stats stats = {};
    for (uint32_t i = 0; i < count; ++i)
    {
        RowKey           key = input->m_Order[i];
        const TypeInput* t = &input->m_Types[key.m_Type];
        FieldId          field = ScalarField(key.m_Type);
        uint64_t         id = store->m_Ids[t->m_Offset + key.m_Row];
        double           value = 0;
        stats.m_Error |= DataFieldGetNumber(store->m_Data, id, g_Fields[field], &value);
        stats.m_Sum += value;
    }
    return stats;
}

static Stats PackedRead_Flecs(Backend* store, const Fixture* input, uint32_t count)
{
    Stats stats = {};
    for (uint32_t i = 0; i < count; ++i)
    {
        RowKey           key = input->m_Order[i];
        const TypeInput* t = &input->m_Types[key.m_Type];
        FieldId          field = ScalarField(key.m_Type);
        double*          value = (double*)FlecsField(store, t, key.m_Type, key.m_Row, field);
        if (!value)
        {
            stats.m_Error |= 1;
            continue;
        }
        stats.m_Sum += *value;
    }
    return stats;
}

static Stats PackedUpdate_Defold(Backend* store, const Fixture* input, uint32_t count)
{
    Stats stats = {};
    for (uint32_t i = 0; i < count; ++i)
    {
        RowKey           key = input->m_Order[i];
        const TypeInput* t = &input->m_Types[key.m_Type];
        FieldId          field = ScalarField(key.m_Type);
        uint64_t         id = store->m_Ids[t->m_Offset + key.m_Row];
        double           value = 0;
        stats.m_Error |= DataFieldGetNumber(store->m_Data, id, g_Fields[field], &value);
        value += 1;
        stats.m_Error |= DataSetFieldNumber(store->m_Data, id, g_Fields[field], value);
        stats.m_Sum += value;
    }
    return stats;
}

static Stats PackedUpdate_Flecs(Backend* store, const Fixture* input, uint32_t count)
{
    Stats stats = {};
    for (uint32_t i = 0; i < count; ++i)
    {
        RowKey           key = input->m_Order[i];
        const TypeInput* t = &input->m_Types[key.m_Type];
        FieldId          field = ScalarField(key.m_Type);
        double*          value = (double*)FlecsField(store, t, key.m_Type, key.m_Row, field);
        if (!value)
        {
            stats.m_Error |= 1;
            continue;
        }
        *value += 1;
        ecs_modified_id(store->m_World, store->m_Ids[t->m_Offset + key.m_Row], store->m_Kind == 1 ? store->m_Types[key.m_Type] : store->m_Fields[field]);
        stats.m_Sum += *value;
    }
    return stats;
}

void MeasurePackedAccess(Backend* store, const Fixture* input, uint32_t sample, uint32_t percent, uint32_t write)
{
    uint32_t changed = input->m_Count / 100 * percent;
    uint32_t count = write ? changed : input->m_Count;
    Stats    stats = {};
    double   expected = 0;
    for (uint32_t i = 0; i < count; ++i)
    {
        RowKey           key = input->m_Order[i];
        const TypeInput* t = &input->m_Types[key.m_Type];
        uint32_t         f = FindField(t, ScalarField(key.m_Type));
        expected += t->m_Values[(size_t)key.m_Row * t->m_FieldCount + f].m_Number + (i < changed ? (write ? write : 2) : 0);
    }
    uint64_t start = BeginOperation();
    if (write)
        stats = store->m_Kind ? PackedUpdate_Flecs(store, input, count) : PackedUpdate_Defold(store, input, count);
    else
        stats = store->m_Kind ? PackedRead_Flecs(store, input, count) : PackedRead_Defold(store, input, count);
    uint64_t end = EndOperation();
    Check(stats.m_Sum == expected, "packed scalar values");
    char name[64];
    snprintf(name, sizeof(name), "packed_%upct_%s", percent, write == 0 ? "random_get" : write == 1 ? "update_first" :
                                                                                                      "update_repeat");
    Record(store, input, sample, name, start, end, count, stats);
}
