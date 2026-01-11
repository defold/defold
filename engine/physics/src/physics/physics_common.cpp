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

#include "physics.h"
#include "physics_private.h"

#include <string.h>

namespace dmPhysics
{
    const char* PHYSICS_SOCKET_NAME = "@physics";

    NewContextParams::NewContextParams()
    : m_Gravity(0.0f, -10.0f, 0.0f)
    , m_WorldCount(4)
    , m_Scale(1.0f)
    , m_ContactImpulseLimit(0.0f)
    , m_TriggerEnterLimit(0.0f)
    , m_VelocityThreshold(1.0f)
    , m_RayCastLimit2D(0)
    , m_RayCastLimit3D(0)
    , m_TriggerOverlapCapacity(0)
    , m_AllowDynamicTransforms(0)
    {

    }

    static const float WORLD_EXTENT = 1000.0f;

    NewWorldParams::NewWorldParams()
    : m_WorldMin(-WORLD_EXTENT, -WORLD_EXTENT, -WORLD_EXTENT)
    , m_WorldMax(WORLD_EXTENT, WORLD_EXTENT, WORLD_EXTENT)
    , m_GetWorldTransformCallback(0x0)
    , m_SetWorldTransformCallback(0x0)
    {

    }

    CollisionObjectData::CollisionObjectData()
    : m_UserData(0x0)
    , m_Type(COLLISION_OBJECT_TYPE_DYNAMIC)
    , m_Mass(1.0f)
    , m_Friction(0.5f)
    , m_Restitution(0.0f)
    , m_LinearDamping(0.0f)
    , m_AngularDamping(0.0f)
    , m_Group(1)
    , m_Mask(1)
    , m_LockedRotation(0)
    , m_Bullet(0)
    , m_Enabled(1)
    {

    }

    StepWorldContext::StepWorldContext()
    {
        memset(this, 0, sizeof(*this));
    }

    RayCastRequest::RayCastRequest()
    : m_From(0.0f, 0.0f, 0.0f)
    , m_To(0.0f, 0.0f, 0.0f)
    , m_IgnoredUserData((void*)~0) // unlikely user data to ignore
    , m_UserData(0x0)
    , m_Mask(~0)
    , m_UserId(0)
    {

    }

    RayCastResponse::RayCastResponse()
    : m_Fraction(1.0f)
    , m_Position(0.0f, 0.0f, 0.0f)
    , m_Normal(0.0f, 0.0f, 0.0f)
    , m_CollisionObjectUserData(0x0)
    , m_CollisionObjectGroup(0)
    , m_Hit(0)
    {

    }

    DebugCallbacks::DebugCallbacks()
    : m_DrawLines(0x0)
    , m_DrawTriangles(0x0)
    , m_UserData(0x0)
    , m_Alpha(1.0f)
    , m_Scale(1.0f)
    , m_DebugScale(1.0f)
    {

    }

    OverlapCache::OverlapCache(uint32_t trigger_overlap_capacity)
    : m_OverlapCache()
    , m_TriggerOverlapCapacity(trigger_overlap_capacity)
    {

    }

    OverlapCacheAddData::OverlapCacheAddData()
    {
        memset(this, 0, sizeof(*this));
    }

    OverlapCachePruneData::OverlapCachePruneData()
    {
        memset(this, 0, sizeof(*this));
    }

}
