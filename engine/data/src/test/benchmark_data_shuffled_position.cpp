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

Stats ShuffledPosition_Defold(Backend* store, const Fixture* input)
{
    Stats stats = {};
    for (uint32_t i = 0; i < input->m_Count; ++i)
    {
        RowKey           key = input->m_Order[i];
        const TypeInput* t = &input->m_Types[key.m_Type];
        DataVector3      value = {};
        uint64_t         id = store->m_Ids[t->m_Offset + key.m_Row];
        stats.m_Error |= DataGetPropertyVector3(store->m_Data, id, g_Properties[POSITION], &value);
        value.m_Values[0] += 1;
        stats.m_Error |= DataSetPropertyVector3(store->m_Data, id, g_Properties[POSITION], &value);
        stats.m_Sum += SumVector(value.m_Values);
    }
    return stats;
}

Stats ShuffledPosition_Flecs(Backend* store, const Fixture* input)
{
    Stats stats = {};
    for (uint32_t i = 0; i < input->m_Count; ++i)
    {
        RowKey           key = input->m_Order[i];
        const TypeInput* t = &input->m_Types[key.m_Type];
        Vector3*         value = (Vector3*)FlecsProperty(store, t, key.m_Type, key.m_Row, POSITION);
        if (!value)
        {
            stats.m_Error |= 1;
            continue;
        }
        value->m_Values[0] += 1;
        ecs_modified_id(store->m_World, store->m_Ids[t->m_Offset + key.m_Row], store->m_Kind == 1 ? store->m_Types[key.m_Type] : store->m_Properties[POSITION]);
        stats.m_Sum += SumVector(value->m_Values);
    }
    return stats;
}

void MeasureShuffledPosition(Backend* store, const Fixture* input, uint32_t sample)
{
    Stats  stats = {};
    double expected = 0;
    for (uint32_t i = 0; i < input->m_Count; ++i)
    {
        RowKey           key = input->m_Order[i];
        const TypeInput* t = &input->m_Types[key.m_Type];
        expected += SumVector(t->m_Values[(size_t)key.m_Row * t->m_FieldCount + FindField(t, POSITION)].m_Value.m_Vector3) + 1;
    }
    uint64_t start = BeginOperation();
    stats = store->m_Kind ? ShuffledPosition_Flecs(store, input) : ShuffledPosition_Defold(store, input);
    uint64_t end = EndOperation();
    Check(stats.m_Sum == expected, "random access checksum");
    // Verify persisted writes outside the measured loop.
    for (uint32_t ti = 0; ti < TYPE_COUNT; ++ti)
    {
        const TypeInput* t = &input->m_Types[ti];
        uint32_t         position = FindField(t, POSITION);
        for (uint32_t r = 0; r < t->m_Count; ++r)
        {
            DataVector3 actual;
            if (!store->m_Kind)
                Check(DataGetPropertyVector3(store->m_Data, store->m_Ids[t->m_Offset + r], g_Properties[POSITION], &actual) == DATA_RESULT_OK, "validate shuffled position read");
            else
                memcpy(actual.m_Values, FlecsProperty(store, t, ti, r, POSITION), sizeof(actual.m_Values));
            const float* base = t->m_Values[(size_t)r * t->m_FieldCount + position].m_Value.m_Vector3;
            Check(actual.m_Values[0] == base[0] + 1 && actual.m_Values[1] == base[1] && actual.m_Values[2] == base[2], "per-instance position update");
        }
    }
    Record(store, input, sample, "shuffled_position", start, end, input->m_Count, stats);
}
