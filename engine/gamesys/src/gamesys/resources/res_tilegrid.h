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

    dmResource::Result ResTileGridPreload(dmResource::HFactory factory, dmResource::HPreloadHintInfo hint_info,
                                            void* context,
                                            const void* buffer, uint32_t buffer_size,
                                            void **preload_data,
                                            const char* filename);

    dmResource::Result ResTileGridCreate(dmResource::HFactory factory,
                                            void* context,
                                            const void* buffer, uint32_t buffer_size,
                                            void *preload_data,
                                            dmResource::SResourceDescriptor* resource,
                                            const char* filename);

    dmResource::Result ResTileGridDestroy(dmResource::HFactory factory,
                                            void* context,
                                            dmResource::SResourceDescriptor* resource);

    dmResource::Result ResTileGridRecreate(dmResource::HFactory factory,
                                              void* context,
                                              const void* buffer, uint32_t buffer_size,
                                              dmResource::SResourceDescriptor* resource,
                                              const char* filename);
}

#endif // DM_GAMESYS_RES_TILEGRID_H
