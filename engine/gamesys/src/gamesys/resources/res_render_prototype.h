#ifndef DMGAMESYSTEM_RES_RENDER_PROTOTYPE_H
#define DMGAMESYSTEM_RES_RENDER_PROTOTYPE_H

#include <stdint.h>

#include <resource/resource.h>

#include <render/render.h>

namespace dmGameSystem
{
    dmResource::Result ResRenderPrototypeCreate(dmResource::HFactory factory,
            void* context,
            const void* buffer, uint32_t buffer_size,
            void *preload_data,
            dmResource::SResourceDescriptor* resource,
            const char* filename);

    dmResource::Result ResRenderPrototypeDestroy(dmResource::HFactory factory,
            void* context,
            dmResource::SResourceDescriptor* resource);

    dmResource::Result ResRenderPrototypeRecreate(dmResource::HFactory factory,
            void* context,
            const void* buffer, uint32_t buffer_size,
            dmResource::SResourceDescriptor* resource,
            const char* filename);
}

#endif // DMGAMESYSTEM_RES_RENDER_PROTOTYPE_H
