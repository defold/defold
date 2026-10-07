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

#include "benchmark_data_threaded_flecs.h"

// Each job gets a worker iterator over its non-overlapping share of every table.
static int32_t NearbyLights_Flecs(HJobContext, HJob, void*, void* data)
{
    ThreadedRange*             range = (ThreadedRange*)data;
    const FlecsThreadedUpdate* update = (const FlecsThreadedUpdate*)range->m_Update;
    ecs_iter_t                 query = ThreadedFlecsIter(update);
    ecs_iter_t                 it = ecs_worker_iter(&query, range->m_First, range->m_Count);
    while (ecs_worker_next(&it))
    {
        const DataVector3*        position = (const DataVector3*)ecs_field_w_size(&it, sizeof(DataVector3), 0);
        const FlecsThreadedLight* lights = (const FlecsThreadedLight*)ecs_field_w_size(&it, sizeof(FlecsThreadedLight), 1);
        for (int32_t r = 0; r < it.count; ++r)
        {
            if (ThreadedInRadius(&position[r]))
            {
                const float* p = position[r].m_Values;
                const float* c = lights[r].m_Color.m_Values;
                float        distance = p[0] * p[0] + p[1] * p[1] + p[2] * p[2];
                range->m_Stats.m_Sum += ((double)c[0] + c[1] + c[2]) * lights[r].m_Intensity * (1.0 - distance / 2500.0);
                ++range->m_Stats.m_Hits;
            }
            ++range->m_Stats.m_Rows;
        }
    }
    return 0;
}

void CreateFlecsLights(ecs_world_t* world, const FlecsThreadedIds* ids, FlecsThreadedUpdate* update)
{
    ecs_query_desc_t desc = {
        .terms = { { .id = ids->m_Position, .inout = EcsIn }, { .id = ids->m_Light, .inout = EcsIn }, { .id = ids->m_LightTag, .inout = EcsIn } },
        .cache_kind = EcsQueryCacheAuto,
    };
    update->m_World = world;
    // Flecs writes cached-query bookkeeping during iterator creation. Each
    // worker owns its cache; all queries still traverse the same component rows.
    for (int32_t w = 0; w < ecs_get_stage_count(world); ++w)
        update->m_WorkerQueries[w] = ecs_query_init(world, &desc);
    update->m_Query = update->m_WorkerQueries[0];
    ThreadedCheck(update->m_Query != 0, "create Flecs query");
    update->m_Common.m_Process = NearbyLights_Flecs;
    update->m_Common.m_Reference = NearbyLights_Reference;
}
