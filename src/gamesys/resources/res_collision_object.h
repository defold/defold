#ifndef DM_GAMESYS_RES_COLLISION_OBJECT_H
#define DM_GAMESYS_RES_COLLISION_OBJECT_H

#include <resource/resource.h>
#include <physics/physics.h>

namespace dmGameSystem
{
    struct CollisionObjectPrototype
    {
        dmPhysics::HCollisionShape m_CollisionShape;
        float m_Mass;
        dmPhysics::CollisionObjectType m_Type;
        uint16_t m_Group;
        uint16_t m_Mask;
    };

    dmResource::CreateResult ResCollisionObjectCreate(dmResource::HFactory factory,
                                             void* context,
                                             const void* buffer, uint32_t buffer_size,
                                             dmResource::SResourceDescriptor* resource,
                                             const char* filename);

    dmResource::CreateResult ResCollisionObjectDestroy(dmResource::HFactory factory,
                                              void* context,
                                              dmResource::SResourceDescriptor* resource);
}

#endif
