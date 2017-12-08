#ifndef DM_GAMESYS_COMP_TILEGRID_H
#define DM_GAMESYS_COMP_TILEGRID_H

#include <dlib/array.h>
#include <gameobject/gameobject.h>
#include "../resources/res_tilegrid.h"
#include "tile_ddf.h"

namespace dmGameSystem
{
    typedef struct TileGridWorld* HTileGridWorld;
    typedef struct TileGridComponent* HTileGridComponent;

    dmGameObject::CreateResult CompTileGridNewWorld(const dmGameObject::ComponentNewWorldParams& params);

    dmGameObject::CreateResult CompTileGridDeleteWorld(const dmGameObject::ComponentDeleteWorldParams& params);

    dmGameObject::CreateResult CompTileGridCreate(const dmGameObject::ComponentCreateParams& params);

    dmGameObject::CreateResult CompTileGridDestroy(const dmGameObject::ComponentDestroyParams& params);

    dmGameObject::CreateResult CompTileGridAddToUpdate(const dmGameObject::ComponentAddToUpdateParams& params);

    dmGameObject::UpdateResult CompTileGridUpdate(const dmGameObject::ComponentsUpdateParams& params);

    dmGameObject::UpdateResult CompTileGridRender(const dmGameObject::ComponentsRenderParams& params);

    dmGameObject::UpdateResult CompTileGridOnMessage(const dmGameObject::ComponentOnMessageParams& params);

    void CompTileGridOnReload(const dmGameObject::ComponentOnReloadParams& params);

    dmGameObject::PropertyResult CompTileGridGetProperty(const dmGameObject::ComponentGetPropertyParams& params, dmGameObject::PropertyDesc& out_value);

    dmGameObject::PropertyResult CompTileGridSetProperty(const dmGameObject::ComponentSetPropertyParams& params);

    void CompTileGridSetTile(HTileGridComponent component, uint32_t layer_index, int32_t cell_x, int32_t cell_y, uint32_t tile, bool flip_x, bool flip_y);

    uint16_t CompTileGridGetTile(HTileGridComponent component, uint32_t layer_index, int32_t cell_x, int32_t cell_y);

    uint32_t CompTileGridGetLayerIndex(const HTileGridComponent component, dmhash_t layer_id);

    TileGridResource* CompTileGridGetResource(const HTileGridComponent component);

}

#endif
