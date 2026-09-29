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
static int32_t Regenerate_EnTT(HJobContext, HJob, void* context, void* data)
{
    ThreadedRange*            range = (ThreadedRange*)data;
    const EnttThreadedUpdate* update = (const EnttThreadedUpdate*)context;
    const auto&               view = update->m_Regenerate;
    auto                      first = update->m_Driver->begin() + range->m_First;
    auto                      last = first + range->m_Count;
    for (auto it = first; it != last; ++it)
    {
        EnttEntity entity = *it;
        if (!view.contains(entity))
            continue;
        double& health = view.get<EnttHealth>(entity).m_Value;
        health = health < 99.75 ? health + 0.25 : 100.0;
        range->m_Stats.m_Sum += health;
        ++range->m_Stats.m_Rows;
    }
    return 0;
}

void CreateEnttRegenerate(EnttRegistry* registry, EnttThreadedUpdate* update)
{
    update->m_Regenerate = registry->view<EnttHealth>();
    update->m_Driver = &registry->storage<EnttHealth>();
    update->m_Read = 0;
    update->m_Write = ENTT_HEALTH;
    update->m_Common.m_Process = Regenerate_EnTT;
    update->m_Common.m_Reference = Regenerate_Reference;
}
