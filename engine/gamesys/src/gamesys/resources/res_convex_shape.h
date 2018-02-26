#ifndef DM_GAMESYS_CONVEX_SHAPE_H
#define DM_GAMESYS_CONVEX_SHAPE_H

#include <resource/resource.h>

#include <physics/physics.h>

namespace dmGameSystem
{
    struct ConvexShapeResource
    {
        union
        {
            dmPhysics::HCollisionShape3D m_Shape3D;
            dmPhysics::HCollisionShape2D m_Shape2D;
        };
        bool m_3D;
    };

    dmResource::Result ResConvexShapeCreate(const dmResource::ResourceCreateParams& params);

    dmResource::Result ResConvexShapeDestroy(const dmResource::ResourceDestroyParams& params);

    dmResource::Result ResConvexShapeRecreate(const dmResource::ResourceRecreateParams& params);

    dmResource::Result ResConvexShapeGetInfo(dmResource::ResourceGetInfoParams& params);
}

#endif // DM_GAMESYS_CONVEX_SHAPE_H
