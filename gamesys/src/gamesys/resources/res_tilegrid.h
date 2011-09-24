#ifndef DM_GAMESYS_RES_TILEGRID_H
#define DM_GAMESYS_RES_TILEGRID_H

#include <stdint.h>

#include <resource/resource.h>
#include "tile_ddf.h"
#include "res_tileset.h"

namespace dmGameSystem
{
    struct TileGridResource
    {
        TileSetResource*           m_TileSet;
        dmGameSystemDDF::TileGrid* m_TileGrid;
    };

    dmResource::CreateResult ResTileGridCreate(dmResource::HFactory factory,
                                            void* context,
                                            const void* buffer, uint32_t buffer_size,
                                            dmResource::SResourceDescriptor* resource,
                                            const char* filename);

    dmResource::CreateResult ResTileGridDestroy(dmResource::HFactory factory,
                                            void* context,
                                            dmResource::SResourceDescriptor* resource);

    dmResource::CreateResult ResTileGridRecreate(dmResource::HFactory factory,
                                              void* context,
                                              const void* buffer, uint32_t buffer_size,
                                              dmResource::SResourceDescriptor* resource,
                                              const char* filename);
}

#endif // DM_GAMESYS_RES_TILEGRID_H
