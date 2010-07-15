#ifndef RESOURCE_CREATION_H
#define RESOURCE_CREATION_H

#include <physics/physics.h>
#include "resource.h"

struct RigidBodyPrototype
{
    dmPhysics::HCollisionShape m_CollisionShape;
    float m_Mass;
    dmPhysics::RigidBodyType m_Type;
};

namespace dmGameSystem
{
    dmResource::FactoryResult RegisterResources(dmResource::HFactory factory);
}

#endif // RESOURCE_CREATION_H
