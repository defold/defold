// Copyright 2020-2022 The Defold Foundation
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

#ifndef DMSDK_GAMESYS_RES_TILEGRID_H
#define DMSDK_GAMESYS_RES_TILEGRID_H

#include <stdint.h>

#include <gamesys/tile_ddf.h>
#include <dmsdk/render/render.h>
#include <dmsdk/gamesys/resources/res_textureset.h>

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
        uint32_t                                m_RowCount : 31;
        uint32_t                                m_Dirty : 1;
        int32_t                                 m_MinCellX;
        int32_t                                 m_MinCellY;
    };
}

#endif // DMSDK_GAMESYS_RES_TILEGRID_H
