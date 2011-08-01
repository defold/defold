#ifndef DM_GAMESYS_RES_COLLISION_OBJECT_H
#define DM_GAMESYS_RES_COLLISION_OBJECT_H

#include <resource/resource.h>
#include <physics/physics.h>

#include "res_convex_shape.h"

#include "../proto/physics_ddf.h"

namespace dmGameSystem
{
    struct CollisionObjectResource
    {
        uint64_t m_Mask[16];
        uint64_t m_Group;
        ConvexShapeResource* m_ConvexShape;
        dmPhysicsDDF::CollisionObjectDesc* m_DDF;
    };

    dmResource::CreateResult ResCollisionObjectCreate(dmResource::HFactory factory,
                                             void* context,
                                             const void* buffer, uint32_t buffer_size,
                                             dmResource::SResourceDescriptor* resource,
                                             const char* filename);

    dmResource::CreateResult ResCollisionObjectDestroy(dmResource::HFactory factory,
                                              void* context,
                                              dmResource::SResourceDescriptor* resource);

    dmResource::CreateResult ResCollisionObjectRecreate(dmResource::HFactory factory,
                                                void* context,
                                                const void* buffer, uint32_t buffer_size,
                                                dmResource::SResourceDescriptor* resource,
                                                const char* filename);

    dmResource::CreateResult ResCollisionObjectRecreate(dmResource::HFactory factory,
                                                void* context,
                                                const void* buffer, uint32_t buffer_size,
                                                dmResource::SResourceDescriptor* resource,
                                                const char* filename);
}

#endif
