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

#ifndef DM_BENCHMARK_DATA_THREADED_FLECS_H
#define DM_BENCHMARK_DATA_THREADED_FLECS_H
#include <flecs.h>
#include "benchmark_data_threaded.h"

struct FlecsThreadedLight
{
    DataVector3 m_Color;
    double      m_Intensity;
};
struct FlecsThreadedIds
{
    ecs_entity_t m_Position, m_Health, m_Velocity, m_Light, m_LightTag, m_Extra[6], m_Tags[6];
};
// Common is first so the shared job completion/reduction code can borrow it.
struct FlecsThreadedUpdate
{
    ThreadedUpdate m_Common;
    ecs_world_t*   m_World;
    ecs_query_t*   m_Query;
    ecs_query_t*   m_WorkerQueries[8];
    ecs_table_t*   m_Tables[THREAD_TYPE_COUNT];
    uint32_t       m_TableCount, m_RowCount;
};
ecs_iter_t ThreadedFlecsIter(const FlecsThreadedUpdate* update);
void       CreateFlecsMovement(ecs_world_t*, const FlecsThreadedIds*, FlecsThreadedUpdate*);
void       CreateFlecsExplosion(ecs_world_t*, const FlecsThreadedIds*, FlecsThreadedUpdate*);
void       CreateFlecsRegenerate(ecs_world_t*, const FlecsThreadedIds*, FlecsThreadedUpdate*);
void       CreateFlecsLights(ecs_world_t*, const FlecsThreadedIds*, FlecsThreadedUpdate*);
#endif
