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

#ifndef DM_BENCHMARK_DATA_ENTT_H
#define DM_BENCHMARK_DATA_ENTT_H

#include <entt/entity/registry.hpp>
#include "benchmark_data_boids.h"

// Generation-bearing 64-bit IDs match the other backends' handle width.
enum class CoreEnttEntity : uint64_t
{
};
using CoreEnttRegistry = entt::basic_registry<CoreEnttEntity>;

template <uint32_t Kind, typename Value>
struct CoreEnttComponent
{
    Value m_Value;
};
using CoreEnttPosition = CoreEnttComponent<POSITION, Vector3>;
using CoreEnttVelocity = CoreEnttComponent<VELOCITY, Vector3>;
using CoreEnttHealth = CoreEnttComponent<HEALTH, double>;
using CoreEnttLight = CoreEnttComponent<LIGHT, Light>;
using CoreEnttRange = CoreEnttComponent<RANGE, double>;
using CoreEnttInnerAngle = CoreEnttComponent<INNER_ANGLE, double>;
using CoreEnttOuterAngle = CoreEnttComponent<OUTER_ANGLE, double>;
using CoreEnttAmount = CoreEnttComponent<AMOUNT, double>;
using CoreEnttOwner = CoreEnttComponent<FIELD_COUNT, uint64_t>;
using CoreEnttIdentity = CoreEnttComponent<FIELD_COUNT + 1, uint64_t>;
struct CoreEnttTag
{
};

// Views own only pool bindings. Keep them alive across insertion and removal,
// as with the other backends' queries. The registry owns every component value.
using CoreEnttBoids = decltype(std::declval<CoreEnttRegistry&>().view<CoreEnttPosition, CoreEnttVelocity, const CoreEnttTag>());
using CoreEnttExplosion = decltype(std::declval<CoreEnttRegistry&>().view<const CoreEnttPosition, CoreEnttHealth>());
using CoreEnttLights = decltype(std::declval<CoreEnttRegistry&>().view<const CoreEnttPosition, const CoreEnttLight, const CoreEnttTag>());

struct CoreEnttStore
{
    CoreEnttRegistry m_Registry;
    CoreEnttEntity*  m_Ids; // Caller-owned fixture allocation, released after the registry.
};

void              Setup_EnTT(CoreEnttStore*, const Fixture*);
void              CreateBulk_EnTT(CoreEnttStore*, const Fixture*, uint32_t type, uint32_t start, uint32_t count);
Stats             CreatePopulation_EnTT(CoreEnttStore*, const Fixture*);
Stats             SpawnWave_EnTT(CoreEnttStore*, const Fixture*);
Stats             DespawnWave_EnTT(CoreEnttStore*, const Fixture*);
CoreEnttBoids     CreateBoidsQuery_EnTT(CoreEnttStore*, uint32_t flock);
CoreEnttExplosion CreateExplosionQuery_EnTT(CoreEnttStore*);
CoreEnttLights    CreateNearbyLightsQuery_EnTT(CoreEnttStore*);
Stats             Boids_EnTT(CoreEnttBoids*, BoidsScratch*);
Stats             Explosion_EnTT(CoreEnttExplosion*);
Stats             NearbyLights_EnTT(CoreEnttLights*);
Stats             PositionLookup_EnTT(CoreEnttStore*, const Fixture*);
void              RunCoreEnTT(const Fixture*, uint32_t sample);

#endif
