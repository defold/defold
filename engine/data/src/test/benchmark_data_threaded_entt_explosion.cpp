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

#include "benchmark_data_threaded_entt.h"

// Partition the public driver-pool iterator in O(1). The view tests membership
// and retrieves the requested components through EnTT's public API.
static int32_t Explosion_EnTT(HJobContext, HJob, void* context, void* data)
{
    ThreadedRange*            range = (ThreadedRange*)data;
    const EnttThreadedUpdate* update = (const EnttThreadedUpdate*)context;
    const auto&               view = update->m_Explosion;
    auto                      first = update->m_Driver->begin() + range->m_First;
    auto                      last = first + range->m_Count;
    for (auto it = first; it != last; ++it)
    {
        EnttEntity entity = *it;
        if (!view.contains(entity))
            continue;
        const DataVector3& position = view.get<const EnttPosition>(entity).m_Value;
        double&            health = view.get<EnttHealth>(entity).m_Value;
        if (ThreadedInRadius(&position))
        {
            health = health > 25.0 ? health - 25.0 : 0.0;
            ++range->m_Stats.m_Hits;
        }
        range->m_Stats.m_Sum += health;
        ++range->m_Stats.m_Rows;
    }
    return 0;
}

void CreateEnttExplosion(EnttRegistry* registry, EnttThreadedUpdate* update)
{
    update->m_Explosion = registry->view<const EnttPosition, EnttHealth>();
    update->m_Driver = &registry->storage<EnttHealth>();
    update->m_Read = ENTT_POSITION;
    update->m_Write = ENTT_HEALTH;
    update->m_Common.m_Process = Explosion_EnTT;
    update->m_Common.m_Reference = Explosion_Reference;
}
