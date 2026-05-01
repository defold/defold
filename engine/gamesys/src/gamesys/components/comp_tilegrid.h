// Copyright 2020-2026 The Defold Foundation
// Copyright 2014-2020 King
// Copyright 2009-2014 Ragnar Svensson, Christian Murray
// Licensed under the Defold License version 1.0 (the "License"); you may not use
// this file except in compliance with the License.
//
// You may obtain a copy of the License, together with FAQs at
// https://www.defold.com/license
//
// Unless required by applicable law or agreed to in writing, software distributed
// under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
// CONDITIONS OF ANY KIND, either express or implied. See the License for the
// specific language governing permissions and limitations under the License.

#ifndef DM_GAMESYS_COMP_TILEGRID_H
#define DM_GAMESYS_COMP_TILEGRID_H

#include <gameobject/component.h>
// for scripting
#include <stdint.h>

namespace dmGameSystem
{
    // Component api functions
    dmGameObject::CreateResult CompTileGridNewWorld(const dmGameObject::ComponentNewWorldParams& params);

    dmGameObject::CreateResult CompTileGridDeleteWorld(const dmGameObject::ComponentDeleteWorldParams& params);

    dmGameObject::CreateResult CompTileGridCreate(const dmGameObject::ComponentCreateParams& params);

    dmGameObject::CreateResult CompTileGridDestroy(const dmGameObject::ComponentDestroyParams& params);

    dmGameObject::CreateResult CompTileGridAddToUpdate(const dmGameObject::ComponentAddToUpdateParams& params);

    dmGameObject::UpdateResult CompTileGridLateUpdate(const dmGameObject::ComponentsUpdateParams& params, dmGameObject::ComponentsUpdateResult& update_result);

    dmGameObject::UpdateResult CompTileGridRender(const dmGameObject::ComponentsRenderParams& params);

    dmGameObject::UpdateResult CompTileGridOnMessage(const dmGameObject::ComponentOnMessageParams& params);

    void*                      CompTileGridGetComponent(const dmGameObject::ComponentGetParams& params);

    void CompTileGridOnReload(const dmGameObject::ComponentOnReloadParams& params);

    dmGameObject::PropertyResult CompTileGridGetProperty(const dmGameObject::ComponentGetPropertyParams& params, dmGameObject::PropertyDesc& out_value);

    dmGameObject::PropertyResult CompTileGridSetProperty(const dmGameObject::ComponentSetPropertyParams& params);

    void CompTileGridIterProperties(dmGameObject::SceneNodePropertyIterator* pit, dmGameObject::SceneNode* node);

    // Script support
    struct TileGridComponent;

    uint32_t CalculateCellIndex(uint32_t layer, int32_t cell_x, int32_t cell_y, uint32_t column_count, uint32_t row_count);

    uint32_t GetLayerIndex(const TileGridComponent* component, dmhash_t layer_id);

    void GetTileGridBounds(const TileGridComponent* component, int32_t* x, int32_t* y, int32_t* w, int32_t* h);

    void GetTileGridCellCoord(const TileGridComponent* component, int32_t x, int32_t y, int32_t& cell_x, int32_t& cell_y);

    uint16_t GetTileGridTile(const TileGridComponent* component, uint32_t layer, int32_t cell_x, int32_t cell_y);

    uint8_t GetTileTransformMask(const TileGridComponent* component, uint32_t layer, int32_t cell_x, int32_t cell_y);

    void SetTileGridTile(TileGridComponent* component, uint32_t layer, int32_t cell_x, int32_t cell_y, uint32_t tile, uint8_t transform_mask);

    uint16_t GetTileCount(const TileGridComponent* component);

    void SetLayerVisible(TileGridComponent* component, uint32_t layer, bool visible);

    enum TileTransformMask
    {
        FLIP_HORIZONTAL = 1,
        FLIP_VERTICAL = 2,
        ROTATE_90 = 4
    };
    static uint8_t MAX_TRANSFORM_FLAG = FLIP_HORIZONTAL + FLIP_VERTICAL + ROTATE_90;
}

#endif
