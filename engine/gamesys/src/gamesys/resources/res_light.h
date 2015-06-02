#ifndef DM_GAMESYS_RES_LIGHT_H
#define DM_GAMESYS_RES_LIGHT_H

#include <stdint.h>

#include <resource/resource.h>

namespace dmGameSystem
{
    dmResource::Result ResLightCreate(dmResource::HFactory factory,
                                            void* context,
                                            const void* buffer, uint32_t buffer_size,
                                            void *preload_data,
                                            dmResource::SResourceDescriptor* resource,
                                            const char* filename);

    dmResource::Result ResLightDestroy(dmResource::HFactory factory,
                                            void* context,
                                            dmResource::SResourceDescriptor* resource);

    dmResource::Result ResLightRecreate(dmResource::HFactory factory,
                                              void* context,
                                              const void* buffer, uint32_t buffer_size,
                                              dmResource::SResourceDescriptor* resource,
                                              const char* filename);
}

#endif // DM_GAMESYS_RES_LIGHT_H
