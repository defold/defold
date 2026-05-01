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

#ifndef DM_GAMESYS_COMP_COLLISION_OBJECT_PRIVATE_H
#define DM_GAMESYS_COMP_COLLISION_OBJECT_PRIVATE_H

#include <gamesys.h>
#include <gamesys/physics_ddf.h>

namespace dmGameSystem
{
    static const dmhash_t PROP_LINEAR_DAMPING   = dmHashString64("linear_damping");
    static const dmhash_t PROP_ANGULAR_DAMPING  = dmHashString64("angular_damping");
    static const dmhash_t PROP_LINEAR_VELOCITY  = dmHashString64("linear_velocity");
    static const dmhash_t PROP_ANGULAR_VELOCITY = dmHashString64("angular_velocity");
    static const dmhash_t PROP_MASS             = dmHashString64("mass");
    static const dmhash_t PROP_BULLET           = dmHashString64("bullet");

    enum EventMask
    {
        EVENT_MASK_COLLISION = 1 << 0,
        EVENT_MASK_CONTACT   = 1 << 1,
        EVENT_MASK_TRIGGER   = 1 << 2,
    };

    struct CollisionUserData
    {
        CollisionWorld* m_World;
        PhysicsContext* m_Context;
        uint32_t        m_Count;
    };

    uint16_t GetGroupBitIndex(CollisionWorld* world, uint64_t group_hash, bool readonly);
    bool ContactPointCallback(const dmPhysics::ContactPoint& contact_point, void* user_data);
    bool CollisionCallback(void* user_data_a, uint16_t group_a, void* user_data_b, uint16_t group_b, void* user_data);
    void TriggerEnteredCallback(const dmPhysics::TriggerEnter& trigger_enter, void* user_data);
    void TriggerExitedCallback(const dmPhysics::TriggerExit& trigger_exit, void* user_data);
    void RayCastCallback(const dmPhysics::RayCastResponse& response, const dmPhysics::RayCastRequest& request, void* user_data);
    bool GetShapeIndexShared(CollisionWorld* world, CollisionComponent* _component, dmhash_t shape_name_hash, uint32_t* index_out);
}

#endif // DM_GAMESYS_COMP_COLLISION_OBJECT_PRIVATE_H
