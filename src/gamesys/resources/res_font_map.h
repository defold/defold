#ifndef DM_GAMESYS_RES_FONT_MAP_H
#define DM_GAMESYS_RES_FONT_MAP_H

#include <stdint.h>

#include <resource/resource.h>

namespace dmGameSystem
{
    dmResource::CreateResult ResFontMapCreate(dmResource::HFactory factory,
                                     void* context,
                                     const void* buffer, uint32_t buffer_size,
                                     dmResource::SResourceDescriptor* resource,
                                     const char* filename);

    dmResource::CreateResult ResFontMapDestroy(dmResource::HFactory factory,
                                      void* context,
                                      dmResource::SResourceDescriptor* resource);

    dmResource::CreateResult ResFontMapRecreate(dmResource::HFactory factory,
                                                void* context,
                                                const void* buffer, uint32_t buffer_size,
                                                dmResource::SResourceDescriptor* resource,
                                                const char* filename);
}

#endif // DM_GAMESYS_RES_FONT_MAP_H
