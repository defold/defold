#ifndef DM_GAMESYS_RES_TEXTURE_H
#define DM_GAMESYS_RES_TEXTURE_H

#include <stdint.h>

#include <resource/resource.h>

namespace dmGameSystem
{
    dmResource::Result ResTexturePreload(dmResource::HFactory factory, dmResource::HPreloadHintInfo hint_info,
                                           void* context,
                                           const void* buffer, uint32_t buffer_size,
                                           void** preload_data,
                                           const char* filename);

    dmResource::Result ResTextureCreate(dmResource::HFactory factory,
                                           void* context,
                                           const void* buffer, uint32_t buffer_size,
                                           void* preload_data,
                                           dmResource::SResourceDescriptor* resource,
                                           const char* filename);

    dmResource::Result ResTextureDestroy(dmResource::HFactory factory,
                                            void* context,
                                            dmResource::SResourceDescriptor* resource);

    dmResource::Result ResTextureRecreate(dmResource::HFactory factory,
            void* context,
            const void* buffer, uint32_t buffer_size,
            dmResource::SResourceDescriptor* resource,
            const char* filename);
}

#endif
