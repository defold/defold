#ifndef DM_GAMESYS_COMP_TILEGRID_H
#define DM_GAMESYS_COMP_TILEGRID_H

#include <gameobject/gameobject.h>

namespace dmGameSystem
{
    struct TileGridComponent;

    // Script support
    uint32_t CalculateCellIndex(uint32_t layer, int32_t cell_x, int32_t cell_y, uint32_t column_count, uint32_t row_count);

    uint32_t GetLayerIndex(const TileGridComponent* component, dmhash_t layer_id);

    void GetTileGridBounds(const TileGridComponent* component, int32_t* x, int32_t* y, int32_t* w, int32_t* h);

    void GetTileGridCellCoord(const TileGridComponent* component, int32_t x, int32_t y, int32_t& cell_x, int32_t& cell_y);

    uint16_t GetTileGridTile(const TileGridComponent* component, uint32_t layer, int32_t cell_x, int32_t cell_y);

    void SetTileGridTile(TileGridComponent* component, uint32_t layer, int32_t cell_x, int32_t cell_y, uint32_t tile, bool flip_h, bool flip_v);

    uint16_t GetTileCount(const TileGridComponent* component);

    void SetLayerVisible(TileGridComponent* component, uint32_t layer, bool visible);

    // Component api functions
    dmGameObject::CreateResult CompTileGridNewWorld(const dmGameObject::ComponentNewWorldParams& params);

    dmGameObject::CreateResult CompTileGridDeleteWorld(const dmGameObject::ComponentDeleteWorldParams& params);

    dmGameObject::CreateResult CompTileGridCreate(const dmGameObject::ComponentCreateParams& params);

    dmGameObject::CreateResult CompTileGridDestroy(const dmGameObject::ComponentDestroyParams& params);

    dmGameObject::CreateResult CompTileGridAddToUpdate(const dmGameObject::ComponentAddToUpdateParams& params);

    dmGameObject::UpdateResult CompTileGridUpdate(const dmGameObject::ComponentsUpdateParams& params, dmGameObject::ComponentsUpdateResult& update_result);

    dmGameObject::UpdateResult CompTileGridRender(const dmGameObject::ComponentsRenderParams& params);

    dmGameObject::UpdateResult CompTileGridOnMessage(const dmGameObject::ComponentOnMessageParams& params);

    void CompTileGridOnReload(const dmGameObject::ComponentOnReloadParams& params);

    dmGameObject::PropertyResult CompTileGridGetProperty(const dmGameObject::ComponentGetPropertyParams& params, dmGameObject::PropertyDesc& out_value);

    dmGameObject::PropertyResult CompTileGridSetProperty(const dmGameObject::ComponentSetPropertyParams& params);
}

#endif
