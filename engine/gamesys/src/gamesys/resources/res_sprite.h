#ifndef DM_GAMESYS_RES_SPRITE_H
#define DM_GAMESYS_RES_SPRITE_H

#include <stdint.h>

#include <dlib/hash.h>
#include <resource/resource.h>

#include "sprite_ddf.h"

#include "res_tileset.h"

namespace dmGameSystem
{
    struct SpriteResource
    {
        dmhash_t m_DefaultAnimation;
        TileSetResource* m_TileSet;
        dmGameSystemDDF::SpriteDesc* m_DDF;
        dmRender::HMaterial m_Material;
    };

    dmResource::Result ResSpriteCreate(dmResource::HFactory factory,
                                            void* context,
                                            const void* buffer, uint32_t buffer_size,
                                            dmResource::SResourceDescriptor* resource,
                                            const char* filename);

    dmResource::Result ResSpriteDestroy(dmResource::HFactory factory,
                                            void* context,
                                            dmResource::SResourceDescriptor* resource);

    dmResource::Result ResSpriteRecreate(dmResource::HFactory factory,
                                              void* context,
                                              const void* buffer, uint32_t buffer_size,
                                              dmResource::SResourceDescriptor* resource,
                                              const char* filename);
}

#endif // DM_GAMESYS_RES_SPRITE_H
