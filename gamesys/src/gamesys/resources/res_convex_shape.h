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

    dmResource::CreateResult ResConvexShapeCreate(dmResource::HFactory factory,
                                               void* context,
                                               const void* buffer, uint32_t buffer_size,
                                               dmResource::SResourceDescriptor* resource,
                                               const char* filename);

    dmResource::CreateResult ResConvexShapeDestroy(dmResource::HFactory factory,
                                                void* context,
                                                dmResource::SResourceDescriptor* resource);

    dmResource::CreateResult ResConvexShapeRecreate(dmResource::HFactory factory,
                                                void* context,
                                                const void* buffer, uint32_t buffer_size,
                                                dmResource::SResourceDescriptor* resource,
                                                const char* filename);
}

#endif // DM_GAMESYS_CONVEX_SHAPE_H
