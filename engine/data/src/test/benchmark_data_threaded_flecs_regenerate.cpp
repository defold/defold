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

#include "benchmark_data_threaded_flecs.h"

// Each job gets a worker iterator over its non-overlapping share of every table.
static int32_t Regenerate_Flecs(HJobContext, HJob, void*, void* data)
{
    ThreadedRange*             range = (ThreadedRange*)data;
    const FlecsThreadedUpdate* update = (const FlecsThreadedUpdate*)range->m_Update;
    ecs_iter_t                 query = ThreadedFlecsIter(update);
    ecs_iter_t                 it = ecs_worker_iter(&query, range->m_First, range->m_Count);
    while (ecs_worker_next(&it))
    {
        double* health = (double*)ecs_field_w_size(&it, sizeof(double), 0);
        for (int32_t r = 0; r < it.count; ++r)
        {
            health[r] = health[r] < 99.75 ? health[r] + 0.25 : 100.0;
            range->m_Stats.m_Sum += health[r];
            ++range->m_Stats.m_Rows;
        }
    }
    return 0;
}

void CreateFlecsRegenerate(ecs_world_t* world, const FlecsThreadedIds* ids, FlecsThreadedUpdate* update)
{
    ecs_query_desc_t desc = {
        .terms = { { .id = ids->m_Health, .inout = EcsInOut } },
        .cache_kind = EcsQueryCacheAuto,
    };
    update->m_World = world;
    // Flecs writes cached-query bookkeeping during iterator creation. Each
    // worker owns its cache; all queries still traverse the same component rows.
    for (int32_t w = 0; w < ecs_get_stage_count(world); ++w)
        update->m_WorkerQueries[w] = ecs_query_init(world, &desc);
    update->m_Query = update->m_WorkerQueries[0];
    ThreadedCheck(update->m_Query != 0, "create Flecs query");
    update->m_Common.m_Process = Regenerate_Flecs;
    update->m_Common.m_Reference = Regenerate_Reference;
}
