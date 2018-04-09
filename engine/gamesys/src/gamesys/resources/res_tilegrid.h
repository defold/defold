#ifndef DM_GAMESYS_RES_TILEGRID_H
#define DM_GAMESYS_RES_TILEGRID_H

#include <stdint.h>

#include <resource/resource.h>
#include "tile_ddf.h"
#include "res_textureset.h"

namespace dmGameSystem
{
    struct TileGridResource
    {
        inline TileGridResource()
        {
            memset(this, 0, sizeof(TileGridResource));
        }

        TextureSetResource*                     m_TextureSet;
        dmGameSystemDDF::TileGrid*              m_TileGrid;
        dmArray<dmPhysics::HCollisionShape2D>   m_GridShapes;
        dmRender::HMaterial                     m_Material;
        uint32_t                                m_ColumnCount;
        uint32_t                                m_RowCount;
        int32_t                                 m_MinCellX;
        int32_t                                 m_MinCellY;
    };

    dmResource::Result ResTileGridPreload(const dmResource::ResourcePreloadParams& params);

    dmResource::Result ResTileGridCreate(const dmResource::ResourceCreateParams& params);

    dmResource::Result ResTileGridDestroy(const dmResource::ResourceDestroyParams& params);

    dmResource::Result ResTileGridRecreate(const dmResource::ResourceRecreateParams& params);
}

#endif // DM_GAMESYS_RES_TILEGRID_H
