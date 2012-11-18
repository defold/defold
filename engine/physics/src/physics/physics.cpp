#include "physics.h"

#include <stdint.h>
#include <string.h>

#include <dlib/array.h>
#include <dlib/log.h>
#include <dlib/profile.h>

namespace dmPhysics
{
    using namespace Vectormath::Aos;

    const char* PHYSICS_SOCKET_NAME = "@physics";

    NewContextParams::NewContextParams()
    : m_Gravity(0.0f, -10.0f, 0.0f)
    , m_WorldCount(4)
    , m_Scale(1.0f)
    , m_ContactImpulseLimit(0.0f)
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
    , m_Group(1)
    , m_Mask(1)
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
}
