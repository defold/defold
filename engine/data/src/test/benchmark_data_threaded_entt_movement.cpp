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
static int32_t Movement_EnTT(HJobContext, HJob, void* context, void* data)
{
    ThreadedRange*            range = (ThreadedRange*)data;
    const EnttThreadedUpdate* update = (const EnttThreadedUpdate*)context;
    const auto&               view = update->m_Movement;
    auto                      first = update->m_Driver->begin() + range->m_First;
    auto                      last = first + range->m_Count;
    for (auto it = first; it != last; ++it)
    {
        EnttEntity entity = *it;
        if (!view.contains(entity))
            continue;
        EnttPosition&       position = view.get<EnttPosition>(entity);
        const EnttVelocity& velocity = view.get<const EnttVelocity>(entity);
        for (uint32_t axis = 0; axis < 3; ++axis)
            position.m_Value.m_Values[axis] += velocity.m_Value.m_Values[axis] * 0.015625f;
        range->m_Stats.m_Sum += position.m_Value.m_Values[0];
        ++range->m_Stats.m_Rows;
    }
    return 0;
}

void CreateEnttMovement(EnttRegistry* registry, EnttThreadedUpdate* update)
{
    update->m_Movement = registry->view<EnttPosition, const EnttVelocity>();
    update->m_Driver = &registry->storage<EnttVelocity>();
    update->m_Read = ENTT_VELOCITY;
    update->m_Write = ENTT_POSITION;
    update->m_Common.m_Process = Movement_EnTT;
    update->m_Common.m_Reference = Movement_Reference;
}
