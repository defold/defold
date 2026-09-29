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
static int32_t NearbyLights_EnTT(HJobContext, HJob, void* context, void* data)
{
    ThreadedRange*            range = (ThreadedRange*)data;
    const EnttThreadedUpdate* update = (const EnttThreadedUpdate*)context;
    const auto&               view = update->m_Lights;
    auto                      first = update->m_Driver->begin() + range->m_First;
    auto                      last = first + range->m_Count;
    for (auto it = first; it != last; ++it)
    {
        EnttEntity entity = *it;
        if (!view.contains(entity))
            continue;
        const DataVector3& position = view.get<const EnttPosition>(entity).m_Value;
        const EnttLight&   light = view.get<const EnttLight>(entity);
        if (ThreadedInRadius(&position))
        {
            const float* p = position.m_Values;
            const float* c = light.m_Color.m_Values;
            float        distance = p[0] * p[0] + p[1] * p[1] + p[2] * p[2];
            range->m_Stats.m_Sum += ((double)c[0] + c[1] + c[2]) * light.m_Intensity * (1.0 - distance / 2500.0);
            ++range->m_Stats.m_Hits;
        }
        ++range->m_Stats.m_Rows;
    }
    return 0;
}

void CreateEnttLights(EnttRegistry* registry, EnttThreadedUpdate* update)
{
    update->m_Lights = registry->view<const EnttPosition, const EnttLight>();
    update->m_Driver = &registry->storage<EnttLight>();
    update->m_Read = ENTT_POSITION | ENTT_LIGHT;
    update->m_Write = 0;
    update->m_Common.m_Process = NearbyLights_EnTT;
    update->m_Common.m_Reference = NearbyLights_Reference;
}
