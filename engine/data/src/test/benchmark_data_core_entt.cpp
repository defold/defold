// Copyright 2020-2026 The Defold Foundation
// Copyright 2014-2020 King
// Copyright 2009-2014 Ragnar Svensson, Christian Murray
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

#include "benchmark_data_entt.h"

// Validation and restoration are outside operation timing and allocation snapshots.
static void ValidateValues(CoreEnttStore* store, const Fixture* input, bool extra, bool moved, bool damaged)
{
    for (uint32_t t = 0; t < TYPE_COUNT; ++t)
    {
        const TypeInput* type = &input->m_Types[t];
        const Vector3*   positions = (const Vector3*)type->m_Columns[FindField(type, POSITION)];
        uint32_t         health_field = FindField(type, HEALTH);
        for (uint32_t r = 0; r < type->m_Count + (extra ? type->m_Extra : 0); ++r)
        {
            CoreEnttEntity id = store->m_Ids[type->m_Offset + r];
            Check(store->m_Registry.valid(id), "EnTT live identity");
            Check(store->m_Registry.get<CoreEnttOwner>(id).m_Value == type->m_Groups[r], "EnTT owner");
            Check(store->m_Registry.get<CoreEnttIdentity>(id).m_Value == type->m_ComponentIds[r], "EnTT component identity");
            const Vector3& position = store->m_Registry.get<CoreEnttPosition>(id).m_Value;
            for (uint32_t axis = 0; axis < 3; ++axis)
            {
                float expected = positions[r].m_Values[axis];
                if (moved && (t == 2 || t == 3))
                    expected += ((const Vector3*)type->m_Columns[FindField(type, VELOCITY)])[r].m_Values[axis] * 0.015625f;
                Check(position.m_Values[axis] == expected, "EnTT persisted position");
            }
            if (health_field != UINT32_MAX)
            {
                double expected = ((const double*)type->m_Columns[health_field])[r];
                if (damaged && Hit(positions[r].m_Values, 50))
                    expected = Damaged(expected, 1);
                Check(store->m_Registry.get<CoreEnttHealth>(id).m_Value == expected, "EnTT persisted health");
            }
            if (t < 2)
            {
                const Light& expected = ((const Light*)type->m_Columns[FindField(type, LIGHT)])[r];
                const Light& actual = store->m_Registry.get<CoreEnttLight>(id).m_Value;
                Check(!memcmp(&expected.color, &actual.color, sizeof(Vector3)) && expected.intensity == actual.intensity, "EnTT nested light");
            }
            Check(store->m_Registry.storage<CoreEnttTag>((entt::id_type)type->m_Type).contains(id), "EnTT type tag");
            for (uint32_t tag = 0; tag < type->m_TagCount; ++tag)
                Check(store->m_Registry.storage<CoreEnttTag>((entt::id_type)type->m_Tags[tag]).contains(id), "EnTT fixture tag");
        }
    }
}

static void ResetValues(CoreEnttStore* store, const Fixture* input)
{
    for (uint32_t t = 0; t < TYPE_COUNT; ++t)
    {
        const TypeInput* type = &input->m_Types[t];
        const Vector3*   positions = (const Vector3*)type->m_Columns[FindField(type, POSITION)];
        uint32_t         health = FindField(type, HEALTH);
        for (uint32_t r = 0; r < type->m_Count; ++r)
        {
            CoreEnttEntity id = store->m_Ids[type->m_Offset + r];
            store->m_Registry.get<CoreEnttPosition>(id).m_Value = positions[r];
            if (health != UINT32_MAX)
                store->m_Registry.get<CoreEnttHealth>(id).m_Value = ((const double*)type->m_Columns[health])[r];
        }
    }
}

template <typename View>
static void ValidateQueryRows(const Fixture* input, const View& view, uint32_t types, uint32_t removed)
{
    uint32_t expected = 0, actual = 0;
    for (uint32_t t = 0; t < TYPE_COUNT; ++t)
        if (types & (1u << t))
            expected += input->m_Types[t].m_Count + input->m_Types[t].m_Extra;
    for (uint32_t i = 0; i < removed; ++i)
        expected -= (types >> input->m_Order[i].m_Type) & 1;
    for (CoreEnttEntity id : view)
    {
        (void)id;
        ++actual;
    }
    Check(actual == expected, "EnTT live view membership after spawn/despawn");
}

void RunCoreEnTT(const Fixture* input, uint32_t sample)
{
    SetBenchmarkMemoryDomain(BENCHMARK_MEMORY_FIXTURE);
    CoreEnttEntity* ids = new CoreEnttEntity[input->m_Total];
    BeginBenchmarkMemoryBackend();
    {
        CoreEnttStore store;
        store.m_Ids = ids;
        Setup_EnTT(&store, input);
        Backend  report = { .m_Kind = 3 };
        uint64_t start = BeginOperation();
        Stats    stats = CreatePopulation_EnTT(&store, input);
        Record(&report, input, sample, "create_population", start, EndOperation(), input->m_Count, stats);
        ValidateValues(&store, input, false, false, false);

        CoreEnttMovement  movement = CreateMovementQuery_EnTT(&store);
        CoreEnttExplosion explosion = CreateExplosionQuery_EnTT(&store);
        CoreEnttLights    lights = CreateNearbyLightsQuery_EnTT(&store);
        start = BeginOperation();
        stats = Movement_EnTT(&movement);
        Record(&report, input, sample, "movement", start, EndOperation(), stats.m_Rows, stats);
        ValidateValues(&store, input, false, true, false);
        ResetValues(&store, input);
        start = BeginOperation();
        stats = Explosion_EnTT(&explosion);
        Record(&report, input, sample, "explosion_r50_first", start, EndOperation(), stats.m_Rows, stats);
        ValidateValues(&store, input, false, false, true);
        ResetValues(&store, input);
        start = BeginOperation();
        stats = NearbyLights_EnTT(&lights);
        Record(&report, input, sample, "nearby_lights", start, EndOperation(), stats.m_Rows, stats);
        start = BeginOperation();
        stats = PositionLookup_EnTT(&store, input);
        Record(&report, input, sample, "position_lookup", start, EndOperation(), stats.m_Rows, stats);

        start = BeginOperation();
        stats = SpawnWave_EnTT(&store, input);
        Record(&report, input, sample, "spawn_wave", start, EndOperation(), input->m_Total - input->m_Count, stats);
        ValidateValues(&store, input, true, false, false);
        ValidateQueryRows(input, movement, (1u << 2) | (1u << 3), 0);
        ValidateQueryRows(input, explosion, (1u << 2) | (1u << 3) | (1u << 5), 0);
        ValidateQueryRows(input, lights, (1u << 0) | (1u << 1), 0);
        start = BeginOperation();
        stats = DespawnWave_EnTT(&store, input);
        Record(&report, input, sample, "despawn_wave", start, EndOperation(), input->m_Count / 100, stats);
        for (uint32_t i = 0; i < input->m_Count / 100; ++i)
        {
            RowKey key = input->m_Order[i];
            Check(!store.m_Registry.valid(ids[input->m_Types[key.m_Type].m_Offset + key.m_Row]), "EnTT despawned ID is stale");
        }
        ValidateQueryRows(input, movement, (1u << 2) | (1u << 3), input->m_Count / 100);
        ValidateQueryRows(input, explosion, (1u << 2) | (1u << 3) | (1u << 5), input->m_Count / 100);
        ValidateQueryRows(input, lights, (1u << 0) | (1u << 1), input->m_Count / 100);
    }
    EndBenchmarkMemoryBackend();
    delete[] ids;
}
