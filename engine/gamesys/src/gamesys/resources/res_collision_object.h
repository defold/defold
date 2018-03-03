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
    const uint32_t COLLISION_OBJECT_MAX_SHAPES = 16; // Number of shapes including shape from convex shape resource

    struct CollisionObjectResource
    {
        inline CollisionObjectResource()
        {
            memset(this, 0, sizeof(CollisionObjectResource));
        }

        uint64_t m_Mask[16];
        uint64_t m_Group;
        TileGridResource* m_TileGridResource;

        union
        {
            dmPhysics::HCollisionShape3D m_Shapes3D[COLLISION_OBJECT_MAX_SHAPES];
            dmPhysics::HCollisionShape2D m_Shapes2D[COLLISION_OBJECT_MAX_SHAPES];
        };
        Vectormath::Aos::Vector3 m_ShapeTranslation[COLLISION_OBJECT_MAX_SHAPES];
        Vectormath::Aos::Quat m_ShapeRotation[COLLISION_OBJECT_MAX_SHAPES];
        uint32_t m_ShapeCount;

        dmPhysicsDDF::CollisionObjectDesc* m_DDF;
        uint32_t m_TileGrid : 1;
    };

    dmResource::Result ResCollisionObjectCreate(const dmResource::ResourceCreateParams& params);

    dmResource::Result ResCollisionObjectDestroy(const dmResource::ResourceDestroyParams& params);

    dmResource::Result ResCollisionObjectRecreate(const dmResource::ResourceRecreateParams& params);

    dmResource::Result ResCollisionObjectRecreate(const dmResource::ResourceRecreateParams& params);

    dmResource::Result ResCollisionObjectGetInfo(dmResource::ResourceGetInfoParams& params);
}

#endif
