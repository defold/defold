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

#include "benchmark_data_common.h"

Stats AddInstances_Defold(Backend* store, const Fixture* input)
{
    Stats stats = {};
    for (uint32_t t = 0; t < TYPE_COUNT; ++t)
    {
        const TypeInput* type = &input->m_Types[t];
        for (uint32_t r = 0; r < type->m_Extra; r += 100)
        {
            uint32_t count = type->m_Extra - r < 100 ? type->m_Extra - r : 100;
            stats.m_Error |= CreateBulk_Defold(store, input, t, type->m_Count + r, count);
        }
    }
    return stats;
}

Stats AddInstances_Flecs(Backend* store, const Fixture* input)
{
    Stats stats = {};
    for (uint32_t t = 0; t < TYPE_COUNT; ++t)
    {
        const TypeInput* type = &input->m_Types[t];
        for (uint32_t r = 0; r < type->m_Extra; r += 100)
        {
            uint32_t count = type->m_Extra - r < 100 ? type->m_Extra - r : 100;
            stats.m_Error |= CreateBulk_Flecs(store, input, t, type->m_Count + r, count);
        }
    }
    return stats;
}

#ifdef DATA_BENCHMARK_ENTT
#include "benchmark_data_entt.h"

Stats SpawnWave_EnTT(CoreEnttStore* store, const Fixture* input)
{
    for (uint32_t t = 0; t < TYPE_COUNT; ++t)
    {
        const TypeInput* type = &input->m_Types[t];
        for (uint32_t r = 0; r < type->m_Extra; r += 100)
            CreateBulk_EnTT(store, input, t, type->m_Count + r, type->m_Extra - r < 100 ? type->m_Extra - r : 100);
    }
    return Stats();
}

#endif

// Repeat the core wave with a fresh population and three live queries each time.
// Only insertion is timed; filter sampled stacks to AddInstances/DataCreateRows.
void ProfileSpawnWave(const Fixture* input, uint32_t kind, uint32_t samples, uint32_t passes)
{
    fprintf(stderr, "Profile ready: %s, Spawn wave, %u passes per sample\n", BACKENDS[kind], passes);
    for (uint32_t sample = 0; sample <= samples; ++sample)
    {
        for (uint32_t pass = 0; pass < passes; ++pass)
        {
            Backend store = CreateBackend(input, kind, 0, "setup");
            for (uint32_t t = 0; t < TYPE_COUNT; ++t)
                Check(CreateBulk(&store, input, t, 0, input->m_Types[t].m_Count) == 0, "profile population");
            Query    movement = CreateMovementQuery(&store);
            Query    explosion = CreateExplosionQuery(&store, input);
            Query    lights = CreateNearbyLightsQuery(&store);
            uint64_t start = BeginOperation();
            Stats    stats = kind ? AddInstances_Flecs(&store, input) : AddInstances_Defold(&store, input);
            Record(&store, input, sample, "spawn_wave", start, EndOperation(), input->m_Total - input->m_Count, stats);
            DestroyQuery(&movement);
            DestroyQuery(&explosion);
            DestroyQuery(&lights);
            DestroyBackend(&store, input, 0, "destroy");
        }
    }
}
