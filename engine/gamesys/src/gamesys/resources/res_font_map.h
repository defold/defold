#ifndef DM_GAMESYS_RES_FONT_MAP_H
#define DM_GAMESYS_RES_FONT_MAP_H

#include <stdint.h>

#include <resource/resource.h>

namespace dmGameSystem
{
    dmResource::Result ResFontMapPreload(dmResource::HFactory factory, dmResource::HPreloadHintInfo hint_info,
                                     void* context,
                                     const void* buffer, uint32_t buffer_size,
                                     void **preload_data,
                                     const char* filename);

    dmResource::Result ResFontMapCreate(dmResource::HFactory factory,
                                     void* context,
                                     const void* buffer, uint32_t buffer_size,
                                     void *preload_data,
                                     dmResource::SResourceDescriptor* resource,
                                     const char* filename);

    dmResource::Result ResFontMapDestroy(dmResource::HFactory factory,
                                      void* context,
                                      dmResource::SResourceDescriptor* resource);

    dmResource::Result ResFontMapRecreate(dmResource::HFactory factory,
                                                void* context,
                                                const void* buffer, uint32_t buffer_size,
                                                dmResource::SResourceDescriptor* resource,
                                                const char* filename);
}

#endif // DM_GAMESYS_RES_FONT_MAP_H
