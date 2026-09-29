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
        stats.m_Error |= DataFieldGetVector3(store->m_Data, id, g_Fields[POSITION], &value);
        value.m_Values[0] += 1;
        stats.m_Error |= DataSetFieldVector3(store->m_Data, id, g_Fields[POSITION], &value);
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
        Vector3*         value = (Vector3*)FlecsField(store, t, key.m_Type, key.m_Row, POSITION);
        if (!value)
        {
            stats.m_Error |= 1;
            continue;
        }
        value->m_Values[0] += 1;
        ecs_modified_id(store->m_World, store->m_Ids[t->m_Offset + key.m_Row], store->m_Kind == 1 ? store->m_Types[key.m_Type] : store->m_Fields[POSITION]);
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
        expected += SumVector(t->m_Values[(size_t)key.m_Row * t->m_FieldCount + FindField(t, POSITION)].m_Vector3) + 1;
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
                Check(DataFieldGetVector3(store->m_Data, store->m_Ids[t->m_Offset + r], g_Fields[POSITION], &actual) == DATA_RESULT_OK, "validate shuffled position read");
            else
                memcpy(actual.m_Values, FlecsField(store, t, ti, r, POSITION), sizeof(actual.m_Values));
            const float* base = t->m_Values[(size_t)r * t->m_FieldCount + position].m_Vector3;
            Check(actual.m_Values[0] == base[0] + 1 && actual.m_Values[1] == base[1] && actual.m_Values[2] == base[2], "per-instance position update");
        }
    }
    Record(store, input, sample, "shuffled_position", start, end, input->m_Count, stats);
}

// Read-only shuffled ID access; no position update or reset is measured.
static Stats PositionLookup_Defold(Backend* store, const Fixture* input)
{
    Stats stats = {};
    for (uint32_t first = 0; first < input->m_Count; first += 256)
    {
        uint32_t    count = input->m_Count - first < 256 ? input->m_Count - first : 256;
        DataId      ids[256];
        DataVector3 positions[256];
        for (uint32_t i = 0; i < count; ++i)
        {
            RowKey key = input->m_Order[first + i];
            ids[i] = store->m_Ids[input->m_Types[key.m_Type].m_Offset + key.m_Row];
        }
        DataResult result = DataFieldGetVector3Batch(store->m_Data, count, ids, g_Fields[POSITION], positions);
        stats.m_Error |= result;
        if (result != DATA_RESULT_OK)
            return stats;
        for (uint32_t i = 0; i < count; ++i)
        {
            stats.m_Sum += SumVector(positions[i].m_Values);
        }
        stats.m_Rows += count;
    }
    return stats;
}

static Stats PositionLookup_Flecs(Backend* store, const Fixture* input)
{
    Stats stats = {};
    for (uint32_t i = 0; i < input->m_Count; ++i)
    {
        RowKey         key = input->m_Order[i];
        uint64_t       id = store->m_Ids[input->m_Types[key.m_Type].m_Offset + key.m_Row];
        const Vector3* position = (const Vector3*)ecs_get_id(store->m_World, id, store->m_Fields[POSITION]);
        stats.m_Sum += SumVector(position->m_Values);
        ++stats.m_Rows;
    }
    return stats;
}

static Stats ExpectedPositionLookup(const Fixture* input)
{
    Stats expected = {};
    for (uint32_t i = 0; i < input->m_Count; ++i)
    {
        RowKey           key = input->m_Order[i];
        const TypeInput* type = &input->m_Types[key.m_Type];
        expected.m_Sum += SumVector(type->m_Values[(size_t)key.m_Row * type->m_FieldCount + FindField(type, POSITION)].m_Vector3);
        ++expected.m_Rows;
    }
    return expected;
}

void MeasurePositionLookup(Backend* store, const Fixture* input, uint32_t sample)
{
    Stats    expected = ExpectedPositionLookup(input);
    uint64_t start = BeginOperation();
    Stats    stats = store->m_Kind ? PositionLookup_Flecs(store, input) : PositionLookup_Defold(store, input);
    uint64_t end = EndOperation();
    Validate(stats, expected);
    Record(store, input, sample, "position_lookup", start, end, input->m_Count, stats);
}

// Repeat the same public-API lookup loop; setup and validation stay outside profiling.
void ProfilePositionLookup(const Fixture* input, uint32_t kind, uint32_t samples, uint32_t passes)
{
    Backend store = CreateBackend(input, kind, 0, "setup");
    for (uint32_t t = 0; t < TYPE_COUNT; ++t)
        Check(CreateBulk(&store, input, t, 0, input->m_Types[t].m_Count) == 0, "profile bulk insertion");
    Stats expected = ExpectedPositionLookup(input);
    expected.m_Sum *= passes;
    expected.m_Rows *= passes;
    fprintf(stderr, "Profile ready: %s, Shuffled position lookup, %u passes per sample\n", BACKENDS[kind], passes);
    for (uint32_t sample = 0; sample <= samples; ++sample)
    {
        Stats    total = {};
        uint64_t start = BeginOperation();
        for (uint32_t pass = 0; pass < passes; ++pass)
        {
            Stats stats = kind ? PositionLookup_Flecs(&store, input) : PositionLookup_Defold(&store, input);
            total.m_Sum += stats.m_Sum;
            total.m_Rows += stats.m_Rows;
            total.m_Error |= stats.m_Error;
        }
        uint64_t end = EndOperation();
        Validate(total, expected);
        Record(&store, input, sample, "position_lookup", start, end, total.m_Rows, total);
    }
    DestroyBackend(&store, input, 0, "destroy");
}

#ifdef DATA_BENCHMARK_ENTT
#include "benchmark_data_entt.h"

Stats PositionLookup_EnTT(CoreEnttStore* store, const Fixture* input)
{
    Stats stats = {};
    for (uint32_t i = 0; i < input->m_Count; ++i)
    {
        RowKey                  key = input->m_Order[i];
        CoreEnttEntity          id = store->m_Ids[input->m_Types[key.m_Type].m_Offset + key.m_Row];
        const CoreEnttPosition& position = store->m_Registry.get<CoreEnttPosition>(id);
        stats.m_Sum += SumVector(position.m_Value.m_Values);
        ++stats.m_Rows;
    }
    return stats;
}

#endif
