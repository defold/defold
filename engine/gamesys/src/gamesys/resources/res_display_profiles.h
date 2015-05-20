#ifndef DM_GAMESYS_RES_DISPLAY_PROFILES_H
#define DM_GAMESYS_RES_DISPLAY_PROFILES_H

#include <stdint.h>

#include <resource/resource.h>

namespace dmGameSystem
{
    dmResource::Result ResDisplayProfilesCreate(dmResource::HFactory factory,
                                           void* context,
                                           const void* buffer, uint32_t buffer_size,
                                           dmResource::SResourceDescriptor* resource,
                                           const char* filename);

    dmResource::Result ResDisplayProfilesDestroy(dmResource::HFactory factory,
                                            void* context,
                                            dmResource::SResourceDescriptor* resource);

    dmResource::Result ResDisplayProfilesRecreate(dmResource::HFactory factory,
            void* context,
            const void* buffer, uint32_t buffer_size,
            dmResource::SResourceDescriptor* resource,
            const char* filename);
}

#endif
