#ifndef DM_GAMESYS_CONVEX_SHAPE_H
#define DM_GAMESYS_CONVEX_SHAPE_H

#include <resource/resource.h>

namespace dmGameSystem
{
    dmResource::CreateResult ResConvexShapeCreate(dmResource::HFactory factory,
                                               void* context,
                                               const void* buffer, uint32_t buffer_size,
                                               dmResource::SResourceDescriptor* resource,
                                               const char* filename);

    dmResource::CreateResult ResConvexShapeDestroy(dmResource::HFactory factory,
                                                void* context,
                                                dmResource::SResourceDescriptor* resource);
}

#endif // DM_GAMESYS_CONVEX_SHAPE_H
