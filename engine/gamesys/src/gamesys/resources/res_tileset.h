#ifndef DMGAMESYSTEM_RES_TILESET_H
#define DMGAMESYSTEM_RES_TILESET_H

#include <stdint.h>

#include <resource/resource.h>

#include <render/render.h>

#include <physics/physics.h>

#include "tile_ddf.h"

namespace dmGameSystem
{
    struct TileSetResource
    {
        inline TileSetResource()
        {
            memset(this, 0, sizeof(*this));
        }

        dmArray<float>              m_TexCoords;
        dmArray<dmhash_t>           m_HullCollisionGroups;
        dmArray<dmhash_t>           m_AnimationIds;
        dmGraphics::HTexture        m_Texture;
        dmGameSystemDDF::TileSet*   m_TileSet;
        dmPhysics::HHullSet2D       m_HullSet;
    };

    dmResource::Result ResTileSetCreate(dmResource::HFactory factory,
            void* context,
            const void* buffer, uint32_t buffer_size,
            dmResource::SResourceDescriptor* resource,
            const char* filename);

    dmResource::Result ResTileSetDestroy(dmResource::HFactory factory,
            void* context,
            dmResource::SResourceDescriptor* resource);

    dmResource::Result ResTileSetRecreate(dmResource::HFactory factory,
            void* context,
            const void* buffer, uint32_t buffer_size,
            dmResource::SResourceDescriptor* resource,
            const char* filename);
}

#endif // DMGAMESYSTEM_RES_TILESET_H
