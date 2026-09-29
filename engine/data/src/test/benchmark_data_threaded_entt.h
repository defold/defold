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

#ifndef DM_BENCHMARK_DATA_THREADED_ENTT_H
#define DM_BENCHMARK_DATA_THREADED_ENTT_H

#include <entt/entity/registry.hpp>
#include "benchmark_data_threaded.h"

// Use 64-bit generation-bearing handles, as in Flecs and Bevy. This also lets
// the benchmark's optional 10-million population fit without changing EnTT.
enum class EnttEntity : uint64_t
{
};
using EnttRegistry = entt::basic_registry<EnttEntity>;
struct EnttPosition
{
    DataVector3 m_Value;
};
struct EnttVelocity
{
    DataVector3 m_Value;
};
struct EnttHealth
{
    double m_Value;
};
struct EnttLight
{
    DataVector3 m_Color;
    double      m_Intensity;
};
struct EnttSpotParameters
{
    double m_Value[3];
};
struct EnttScalar
{
    double m_Value;
};
template <uint32_t Type>
struct EnttTag
{
};

using EnttMovementView = decltype(std::declval<EnttRegistry&>().view<EnttPosition, const EnttVelocity>());
using EnttExplosionView = decltype(std::declval<EnttRegistry&>().view<const EnttPosition, EnttHealth>());
using EnttRegenerateView = decltype(std::declval<EnttRegistry&>().view<EnttHealth>());
using EnttLightsView = decltype(std::declval<EnttRegistry&>().view<const EnttPosition, const EnttLight>());

enum EnttAccess
{
    ENTT_POSITION = 1,
    ENTT_VELOCITY = 2,
    ENTT_HEALTH = 4,
    ENTT_LIGHT = 8
};

// Views bind pools on the main thread. Workers only read these bindings and
// never ask a mutable registry to lazily create a pool. Only one view is active
// in each update; the other views remain empty.
struct EnttThreadedUpdate
{
    ThreadedUpdate                            m_Common;
    EnttMovementView                          m_Movement;
    EnttExplosionView                         m_Explosion;
    EnttRegenerateView                        m_Regenerate;
    EnttLightsView                            m_Lights;
    const entt::basic_sparse_set<EnttEntity>* m_Driver;
    uint32_t                                  m_Read, m_Write;
};
void CreateEnttMovement(EnttRegistry*, EnttThreadedUpdate*);
void CreateEnttExplosion(EnttRegistry*, EnttThreadedUpdate*);
void CreateEnttRegenerate(EnttRegistry*, EnttThreadedUpdate*);
void CreateEnttLights(EnttRegistry*, EnttThreadedUpdate*);
#endif
