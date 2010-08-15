#ifndef RESOURCE_MESH_H
#define RESOURCE_MESH_H

#include <gamesys/gameobject.h>
#include <gamesys/resource.h>

namespace dmMesh
{
    dmResource::CreateResult CreateResource(dmResource::HFactory factory,
            void* context,
            const void* buffer, uint32_t buffer_size,
            dmResource::SResourceDescriptor* resource,
            const char* filename);

    dmResource::CreateResult DestroyResource(dmResource::HFactory factory,
            void* context,
            dmResource::SResourceDescriptor* resource);

    dmResource::CreateResult RecreateResource(dmResource::HFactory factory,
            void* context,
            const void* buffer, uint32_t buffer_size,
            dmResource::SResourceDescriptor* resource,
            const char* filename);
}

#endif
