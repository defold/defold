#ifndef DM_GAMESYS_RES_COLLISION_OBJECT_H
#define DM_GAMESYS_RES_COLLISION_OBJECT_H

#include <string.h>

#include <resource/resource.h>
#include <physics/physics.h>

#include "res_convex_shape.h"
#include "res_tilegrid.h"

#include "../proto/physics_ddf.h"

namespace dmGameSystem
{
    struct CollisionObjectResource
    {
        inline CollisionObjectResource()
        {
            memset(this, 0, sizeof(CollisionObjectResource));
        }

        uint64_t m_Mask[16];
        uint64_t m_Group;
        union
        {
            ConvexShapeResource* m_ConvexShapeResource;
            TileGridResource* m_TileGridResource;
        };
        dmPhysicsDDF::CollisionObjectDesc* m_DDF;
        uint32_t m_TileGrid : 1;
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
