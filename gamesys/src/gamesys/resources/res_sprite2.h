#ifndef DM_GAMESYS_RES_SPRITE2_H
#define DM_GAMESYS_RES_SPRITE2_H

#include <stdint.h>

#include <dlib/hash.h>
#include <resource/resource.h>

#include "sprite2_ddf.h"

#include "res_tileset.h"

namespace dmGameSystem
{
    struct Sprite2Resource
    {
        dmhash_t m_DefaultAnimation;
        TileSetResource* m_TileSet;
        dmGameSystemDDF::Sprite2Desc* m_DDF;
    };

    dmResource::CreateResult ResSprite2Create(dmResource::HFactory factory,
                                            void* context,
                                            const void* buffer, uint32_t buffer_size,
                                            dmResource::SResourceDescriptor* resource,
                                            const char* filename);

    dmResource::CreateResult ResSprite2Destroy(dmResource::HFactory factory,
                                            void* context,
                                            dmResource::SResourceDescriptor* resource);

    dmResource::CreateResult ResSprite2Recreate(dmResource::HFactory factory,
                                              void* context,
                                              const void* buffer, uint32_t buffer_size,
                                              dmResource::SResourceDescriptor* resource,
                                              const char* filename);
}

#endif // DM_GAMESYS_RES_SPRITE2_H
