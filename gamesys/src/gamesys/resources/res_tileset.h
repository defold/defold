#ifndef DMGAMESYSTEM_RES_TILESET_H
#define DMGAMESYSTEM_RES_TILESET_H

#include <stdint.h>

#include <resource/resource.h>

#include <render/render.h>

namespace dmGameSystem
{
    struct TileSetResource
    {
        inline TileSetResource()
        {
            memset(this, 0, sizeof(*this));
        }
        struct ConvexHull
        {
            uint32_t m_Index;
            uint32_t m_Count;
            dmhash_t m_CollisionGroupHash;
        };

        dmGraphics::HTexture m_Texture;
        ConvexHull*          m_ConvexHulls;
        float*               m_ConvexHullPoints;
    };

    dmResource::CreateResult ResTileSetCreate(dmResource::HFactory factory,
            void* context,
            const void* buffer, uint32_t buffer_size,
            dmResource::SResourceDescriptor* resource,
            const char* filename);

    dmResource::CreateResult ResTileSetDestroy(dmResource::HFactory factory,
            void* context,
            dmResource::SResourceDescriptor* resource);

    dmResource::CreateResult ResTileSetRecreate(dmResource::HFactory factory,
            void* context,
            const void* buffer, uint32_t buffer_size,
            dmResource::SResourceDescriptor* resource,
            const char* filename);
}

#endif // DMGAMESYSTEM_RES_TILESET_H
