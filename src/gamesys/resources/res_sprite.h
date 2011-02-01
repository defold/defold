#ifndef DM_GAMESYS_RES_SPRITE_H
#define DM_GAMESYS_RES_SPRITE_H

#include <stdint.h>

#include <resource/resource.h>

#include <graphics/graphics.h>

#include "sprite_ddf.h"

namespace dmGameSystem
{
    struct SpriteResource
    {
        dmGraphics::HTexture m_Texture;
        dmGameSystemDDF::SpriteDesc* m_DDF;
    };

    dmResource::CreateResult ResSpriteCreate(dmResource::HFactory factory,
                                            void* context,
                                            const void* buffer, uint32_t buffer_size,
                                            dmResource::SResourceDescriptor* resource,
                                            const char* filename);

    dmResource::CreateResult ResSpriteDestroy(dmResource::HFactory factory,
                                            void* context,
                                            dmResource::SResourceDescriptor* resource);

    dmResource::CreateResult ResSpriteRecreate(dmResource::HFactory factory,
                                              void* context,
                                              const void* buffer, uint32_t buffer_size,
                                              dmResource::SResourceDescriptor* resource,
                                              const char* filename);
}

#endif // DM_GAMESYS_RES_SPRITE_H
