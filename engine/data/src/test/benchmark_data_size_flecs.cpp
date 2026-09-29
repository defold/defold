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

#include <stdio.h>
#include <stdlib.h>

#include <stdint.h>

#include <flecs.h>
struct Position
{
    float m_Values[3];
};
struct Health
{
    double m_Value;
};

int main(int argc, char** argv)
{
    uint32_t             count = argc > 1 ? (uint32_t)strtoul(argv[1], 0, 10) : 100;
    ecs_world_t*         world = ecs_mini();
    ecs_component_desc_t position_desc = { .type = { .size = sizeof(Position), .alignment = alignof(Position) } };
    ecs_component_desc_t health_desc = { .type = { .size = sizeof(Health), .alignment = alignof(Health) } };
    ecs_entity_t         position_id = ecs_component_init(world, &position_desc);
    ecs_entity_t         health_id = ecs_component_init(world, &health_desc);
    ecs_entity_t*        entities = new ecs_entity_t[count];
    for (uint32_t row = 0; row < count; ++row)
    {
        entities[row] = ecs_new(world);
        Position position = {};
        Health   health = { .m_Value = 100 };
        ecs_set_id(world, entities[row], position_id, sizeof(position), &position);
        ecs_set_id(world, entities[row], health_id, sizeof(health), &health);
    }
    ecs_query_desc_t desc = {
        .terms = { { .id = position_id, .inout = EcsIn }, { .id = health_id, .inout = EcsInOut } },
        .cache_kind = EcsQueryCacheAuto,
    };
    ecs_query_t* query = ecs_query_init(world, &desc);
    ecs_iter_t   it = ecs_query_iter(world, query);
    while (ecs_query_next(&it))
    {
        const Position* position = (const Position*)ecs_field_w_size(&it, sizeof(Position), 0);
        Health*         health = (Health*)ecs_field_w_size(&it, sizeof(Health), 1);
        for (int32_t row = 0; row < it.count; ++row)
            if (position[row].m_Values[0] < 50)
                health[row].m_Value -= 25;
    }
    double sum = 0;
    for (uint32_t row = 0; row < count; ++row)
    {
        sum += ((const Health*)ecs_get_id(world, entities[row], health_id))->m_Value;
        ecs_delete(world, entities[row]);
    }
    ecs_query_fini(query);
    ecs_fini(world);
    delete[] entities;
    printf("%.0f\n", sum);
    return sum != (double)count * 75;
}
