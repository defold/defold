#ifndef DM_GAMESYS_RES_IMAGE_FONT_H
#define DM_GAMESYS_RES_IMAGE_FONT_H

#include <stdint.h>

#include <resource/resource.h>

#include <render/font_renderer.h>

namespace dmGameSystem
{
    struct ImageFontResource
    {
        dmRender::HImageFont m_ImageFont;
    };

    dmResource::CreateResult ResImageFontCreate(dmResource::HFactory factory,
                                          void* context,
                                          const void* buffer, uint32_t buffer_size,
                                          dmResource::SResourceDescriptor* resource,
                                          const char* filename);

    dmResource::CreateResult ResImageFontDestroy(dmResource::HFactory factory,
                                           void* context,
                                           dmResource::SResourceDescriptor* resource);
    dmResource::CreateResult ResImageFontRecreate(dmResource::HFactory factory,
                                                void* context,
                                                const void* buffer, uint32_t buffer_size,
                                                dmResource::SResourceDescriptor* resource,
                                                const char* filename);
}

#endif // DM_GAMESYS_RES_IMAGE_FONT_H
