#ifndef RESOURCE_CREATION_H
#define RESOURCE_CREATION_H

#include <physics/physics.h>
#include <resource/resource.h>

struct CollisionObjectPrototype
{
    dmPhysics::HCollisionShape m_CollisionShape;
    float m_Mass;
    dmPhysics::CollisionObjectType m_Type;
    uint16_t m_Group;
    uint16_t m_Mask;
};

namespace dmGameSystem
{
    dmResource::FactoryResult RegisterResources(dmResource::HFactory factory);
}

#endif // RESOURCE_CREATION_H
