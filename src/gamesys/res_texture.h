#ifndef DM_GAMESYS_RES_TEXTURE_H
#define DM_GAMESYS_RES_TEXTURE_H

#include <stdint.h>

#include <resource/resource.h>

namespace dmGameSystem
{
    dmResource::CreateResult ResTextureCreate(dmResource::HFactory factory,
                                           void* context,
                                           const void* buffer, uint32_t buffer_size,
                                           dmResource::SResourceDescriptor* resource,
                                           const char* filename);

    dmResource::CreateResult ResTextureDestroy(dmResource::HFactory factory,
                                            void* context,
                                            dmResource::SResourceDescriptor* resource);
}

#endif
