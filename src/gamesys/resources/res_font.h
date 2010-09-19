#ifndef DM_GAMESYS_RES_FONT_H
#define DM_GAMESYS_RES_FONT_H

#include <stdint.h>

#include <resource/resource.h>

namespace dmGameSystem
{
    dmResource::CreateResult ResFontCreate(dmResource::HFactory factory,
                                     void* context,
                                     const void* buffer, uint32_t buffer_size,
                                     dmResource::SResourceDescriptor* resource,
                                     const char* filename);

    dmResource::CreateResult ResFontDestroy(dmResource::HFactory factory,
                                      void* context,
                                      dmResource::SResourceDescriptor* resource);
}

#endif // DM_GAMESYS_RES_FONT_H
