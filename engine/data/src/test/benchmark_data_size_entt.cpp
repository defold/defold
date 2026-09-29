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

#include <entt/entity/registry.hpp>
struct Position
{
    float m_Values[3];
};
struct Health
{
    double m_Value;
};
enum class Entity : uint64_t
{
};

int main(int argc, char** argv)
{
    uint32_t                     count = argc > 1 ? (uint32_t)strtoul(argv[1], 0, 10) : 100;
    entt::basic_registry<Entity> registry;
    Entity*                      entities = new Entity[count];
    for (uint32_t row = 0; row < count; ++row)
    {
        entities[row] = registry.create();
        registry.emplace<Position>(entities[row], Position {});
        registry.emplace<Health>(entities[row], Health { .m_Value = 100 });
    }
    auto view = registry.view<const Position, Health>();
    for (Entity entity : view)
        if (view.get<const Position>(entity).m_Values[0] < 50)
            view.get<Health>(entity).m_Value -= 25;
    double sum = 0;
    for (uint32_t row = 0; row < count; ++row)
    {
        sum += registry.get<Health>(entities[row]).m_Value;
        registry.destroy(entities[row]);
    }
    delete[] entities;
    printf("%.0f\n", sum);
    return sum != (double)count * 75;
}
