#ifndef DM_GAMESYS_RES_MATERIAL_H
#define DM_GAMESYS_RES_MATERIAL_H

#include <stdint.h>

#include <resource/resource.h>

namespace dmGameSystem
{
    dmResource::CreateResult ResCreateMaterial(dmResource::HFactory factory,
            void* context,
            const void* buffer, uint32_t buffer_size,
            dmResource::SResourceDescriptor* resource,
            const char* filename);

    dmResource::CreateResult ResDestroyMaterial(dmResource::HFactory factory,
            void* context,
            dmResource::SResourceDescriptor* resource);

    dmResource::CreateResult ResRecreateMaterial(dmResource::HFactory factory,
            void* context,
            const void* buffer, uint32_t buffer_size,
            dmResource::SResourceDescriptor* resource,
            const char* filename);
}

#endif
