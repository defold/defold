#include <stdint.h>

#include <dlib/array.h>
#include <dlib/log.h>
#include <dlib/profile.h>

#include "physics.h"

namespace dmPhysics
{
    using namespace Vectormath::Aos;

    NewContextParams::NewContextParams()
    : m_WorldCount(4)
    {

    }

    static const float WORLD_EXTENT = 1000.0f;

    NewWorldParams::NewWorldParams()
    : m_Gravity(0.0f, -10.0f, 0.0f)
    , m_WorldMin(-WORLD_EXTENT, -WORLD_EXTENT, -WORLD_EXTENT)
    , m_WorldMax(WORLD_EXTENT, WORLD_EXTENT, WORLD_EXTENT)
    , m_GetWorldTransformCallback(0x0)
    , m_SetWorldTransformCallback(0x0)
    , m_CollisionCallback(0x0)
    , m_CollisionCallbackUserData(0x0)
    , m_ContactPointCallback(0x0)
    , m_ContactPointCallbackUserData(0x0)
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

    RayCastRequest::RayCastRequest()
    : m_From(0.0f, 0.0f, 0.0f)
    , m_To(0.0f, 0.0f, 0.0f)
    , m_UserId(0)
    , m_Mask(~0)
    , m_IgnoredUserData((void*)~0) // unlikely user data to ignore
    , m_UserData(0x0)
    , m_Callback(0x0)
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
    {

    }
}
