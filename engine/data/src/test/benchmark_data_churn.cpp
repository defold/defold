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

Stats RemoveInstances_Defold(Backend* store, const Fixture* input)
{
    Stats    stats = {};
    uint32_t count = input->m_Count / 100;
    for (uint32_t i = 0; i < count; ++i)
    {
        RowKey   key = input->m_Order[i];
        uint64_t id = store->m_Ids[input->m_Types[key.m_Type].m_Offset + key.m_Row];
        stats.m_Error |= DataRemoveRow(store->m_Data, id);
    }
    return stats;
}

Stats ReplaceInstances_Defold(Backend* store, const Fixture* input)
{
    Stats    stats = {};
    uint32_t count = input->m_Count / 100;
    for (uint32_t i = 0; i < count; ++i)
    {
        RowKey key = input->m_Order[i];
        stats.m_Error |= CreateIndividual_Defold(store, input, key.m_Type, key.m_Row, 1);
    }
    return stats;
}

Stats RemoveInstances_Flecs(Backend* store, const Fixture* input)
{
    Stats    stats = {};
    uint32_t count = input->m_Count / 100;
    for (uint32_t i = 0; i < count; ++i)
    {
        RowKey   key = input->m_Order[i];
        uint64_t id = store->m_Ids[input->m_Types[key.m_Type].m_Offset + key.m_Row];
        ecs_delete(store->m_World, id);
    }
    return stats;
}

Stats ReplaceInstances_Flecs(Backend* store, const Fixture* input)
{
    Stats    stats = {};
    uint32_t count = input->m_Count / 100;
    for (uint32_t i = 0; i < count; ++i)
    {
        RowKey key = input->m_Order[i];
        stats.m_Error |= CreateIndividual_Flecs(store, input, key.m_Type, key.m_Row, 1);
    }
    return stats;
}

#ifdef DATA_BENCHMARK_ENTT
#include "benchmark_data_entt.h"

Stats DespawnWave_EnTT(CoreEnttStore* store, const Fixture* input)
{
    for (uint32_t i = 0; i < input->m_Count / 100; ++i)
    {
        RowKey key = input->m_Order[i];
        store->m_Registry.destroy(store->m_Ids[input->m_Types[key.m_Type].m_Offset + key.m_Row]);
    }
    return Stats();
}

#endif
