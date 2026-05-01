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


#include <dlib/configfile.h>
#include <dlib/log.h>
#include <ddf/ddf.h>
#include <gameobject/gameobject.h>
#include <render/render.h>
#include <script/script.h>
#include <gameobject/script.h>
#include "gamesys.h"
#include <gamesys/tile_ddf.h>
#include <gamesys/physics_ddf.h>
#include "../gamesys_private.h"
#include "../resources/res_tilegrid.h"
#include "../components/comp_tilegrid.h"
#include "script_tilemap.h"

extern "C"
{
#include <lua/lauxlib.h>
#include <lua/lualib.h>
}

namespace dmGameSystem
{
    /*# Tilemap API documentation
     *
     * Functions and messages used to manipulate tile map components.
     *
     * @document
     * @name Tilemap
     * @namespace tilemap
     * @language Lua
     */

    /*# [type:hash] tile source
     *
     * The tile source used when rendering the tile map. The type of the property is hash.
     *
     * @name tile_source
     * @property
     *
     * @examples
     *
     * How to set tile source using a script property (see [ref:resource.tile_source])
     *
     * ```lua
     * go.property("my_tile_source", resource.tile_source("/tilesource.tilesource"))
     * function init(self)
     *   go.set("#tilemap", "tile_source", self.my_tile_source)
     * end
     * ```
     */

    /*# [type:hash] tile map material
     *
     * The material used when rendering the tile map. The type of the property is hash.
     *
     * @name material
     * @property
     *
     * @examples
     *
     * How to set material using a script property (see [ref:resource.material])
     *
     * ```lua
     * go.property("my_material", resource.material("/material.material"))
     * function init(self)
     *   go.set("#tilemap", "material", self.my_material)
     * end
     * ```
     */

    /** DEPRECATED! set a shader constant for a tile map
     * Sets a shader constant for a tile map component.
     * The constant must be defined in the material assigned to the tile map.
     * Setting a constant through this function will override the value set for that constant in the material.
     * The value will be overridden until tilemap.reset_constant is called.
     * Which tile map to set a constant for is identified by the URL.
     *
     * @name tilemap.set_constant
     * @param url [type:string|hash|url] the tile map that should have a constant set
     * @param constant [type:string|hash] name of the constant
     * @param value [type:vector4] value of the constant
     * @examples
     *
     * The following examples assumes that the tile map has id "tile map" and that the default-material in builtins is used, which defines the constant "tint".
     * If you assign a custom material to the tile map, you can set the constants defined there in the same manner.
     *
     * How to tint a tile map to red:
     *
     * ```lua
     * function init(self)
     *     tilemap.set_constant("#tilemap", "tint", vmath.vector4(1, 0, 0, 1))
     * end
     * ```
     */
    static int TileMap_SetConstant(lua_State* L)
    {
        int top = lua_gettop(L);

        (void)CheckGoInstance(L); // left to check that it's not called from incorrect context.

        dmhash_t name_hash = dmScript::CheckHashOrString(L, 2);
        dmVMath::Vector4* value = dmScript::CheckVector4(L, 3);

        dmGameSystemDDF::SetConstantTileMap msg;
        msg.m_NameHash = name_hash;
        msg.m_Value = *value;

        dmMessage::URL receiver;
        dmMessage::URL sender;
        dmScript::ResolveURL(L, 1, &receiver, &sender);

        dmMessage::Post(&sender, &receiver, dmGameSystemDDF::SetConstantTileMap::m_DDFDescriptor->m_NameHash, 0, (uintptr_t)dmGameSystemDDF::SetConstantTileMap::m_DDFDescriptor, &msg, sizeof(msg), 0);
        assert(top == lua_gettop(L));
        return 0;
    }

    /** DEPRECATED! reset a shader constant for a tile map
     * Resets a shader constant for a tile map component.
     * The constant must be defined in the material assigned to the tile map.
     * Resetting a constant through this function implies that the value defined in the material will be used.
     * Which tile map to reset a constant for is identified by the URL.
     *
     * @name tilemap.reset_constant
     * @param url [type:string|hash|url] the tile map that should have a constant reset
     * @param constant [type:string|hash] name of the constant
     * @examples
     *
     * The following examples assumes that the tile map has id "tilemap" and that the default-material in builtins is used, which defines the constant "tint".
     * If you assign a custom material to the tile map, you can reset the constants defined there in the same manner.
     *
     * How to reset the tinting of a tile map:
     *
     * ```lua
     * function init(self)
     *     tilemap.reset_constant("#tilemap", "tint")
     * end
     * ```
     */
    static int TileMap_ResetConstant(lua_State* L)
    {
        int top = lua_gettop(L);

        (void)CheckGoInstance(L); // left to check that it's not called from incorrect context.
        dmhash_t name_hash = dmScript::CheckHashOrString(L, 2);

        dmGameSystemDDF::ResetConstantTileMap msg;
        msg.m_NameHash = name_hash;

        dmMessage::URL receiver;
        dmMessage::URL sender;
        dmScript::ResolveURL(L, 1, &receiver, &sender);

        dmMessage::Post(&sender, &receiver, dmGameSystemDDF::ResetConstantTileMap::m_DDFDescriptor->m_NameHash, 0, (uintptr_t)dmGameSystemDDF::ResetConstantTileMap::m_DDFDescriptor, &msg, sizeof(msg), 0);
        assert(top == lua_gettop(L));
        return 0;
    }

    /*# set a tile in a tile map
     * Replace a tile in a tile map with a new tile.
     * The coordinates of the tiles are indexed so that the "first" tile just
     * above and to the right of origin has coordinates 1,1.
     * Tiles to the left of and below origin are indexed 0, -1, -2 and so forth.
     *
     * <pre>
     * +-------+-------+------+------+
     * |  0,3  |  1,3  | 2,3  | 3,3  |
     * +-------+-------+------+------+
     * |  0,2  |  1,2  | 2,2  | 3,2  |
     * +-------+-------+------+------+
     * |  0,1  |  1,1  | 2,1  | 3,1  |
     * +-------O-------+------+------+
     * |  0,0  |  1,0  | 2,0  | 3,0  |
     * +-------+-------+------+------+
     * </pre>
     *
     * The coordinates must be within the bounds of the tile map as it were created.
     * That is, it is not possible to extend the size of a tile map by setting tiles outside the edges.
     * To clear a tile, set the tile to number 0. Which tile map and layer to manipulate is identified by the URL and the layer name parameters.
     *
     * Transform bitmask is arithmetic sum of one or both FLIP constants (`tilemap.H_FLIP`, `tilemap.V_FLIP`) and/or one of ROTATION constants
     * (`tilemap.ROTATE_90`, `tilemap.ROTATE_180`, `tilemap.ROTATE_270`).
     * Flip always applies before rotation (clockwise).
     *
     * @name tilemap.set_tile
     * @param url [type:string|hash|url] the tile map
     * @param layer [type:string|hash] name of the layer for the tile
     * @param x [type:number] x-coordinate of the tile
     * @param y [type:number] y-coordinate of the tile
     * @param tile [type:number] index of new tile to set. 0 resets the cell
     * @param [transform_bitmask] [type:number] optional flip and/or rotation should be applied to the tile
     * @examples
     *
     * ```lua
     * -- Clear the tile under the player.
     * tilemap.set_tile("/level#tilemap", "foreground", self.player_x, self.player_y, 0)
     *
     * -- Set tile with different combination of flip and rotation
     * tilemap.set_tile("#tilemap", "layer1", x, y, 0, tilemap.H_FLIP + tilemap.V_FLIP + tilemap.ROTATE_90)
     * tilemap.set_tile("#tilemap", "layer1", x, y, 0, tilemap.H_FLIP + tilemap.ROTATE_270)
     * tilemap.set_tile("#tilemap", "layer1", x, y, 0, tilemap.V_FLIP + tilemap.H_FLIP)
     * tilemap.set_tile("#tilemap", "layer1", x, y, 0, tilemap.ROTATE_180)
     * ```
     */
    static int TileMap_SetTile(lua_State* L)
    {
        int top = lua_gettop(L);

        dmGameObject::HInstance sender_instance = CheckGoInstance(L);
        dmGameObject::HCollection collection = dmGameObject::GetCollection(sender_instance);

        TileGridComponent* component;
        dmMessage::URL receiver;
        dmGameObject::GetComponentFromLua(L, 1, collection, TILE_MAP_EXT, (dmGameObject::HComponent*)&component, &receiver, 0);

        dmhash_t layer_id = dmScript::CheckHashOrString(L, 2);

        uint32_t layer_index = GetLayerIndex(component, layer_id);
        if (layer_index == ~0u)
        {
            dmLogError("Could not find layer '%s'.", dmHashReverseSafe64(layer_id));
            lua_pushboolean(L, 0);
            assert(top + 1 == lua_gettop(L));
            return 1;
        }

        int x = luaL_checkinteger(L, 3) - 1;
        int y = luaL_checkinteger(L, 4) - 1;

        int lua_tile  = luaL_checkinteger(L, 5);

        /* Range check: tile indices that fall outside of the valid limits will crash the engine.
         *
         * Note that 0 resets the tile cell, so the valid range is [0...N]
         * where N is equal to the amount of tiles availabel in the tile source.
         */
        if (lua_tile < 0 || lua_tile > (int)GetTileCount(component))
        {
            return luaL_error(L, "tilemap.set_tile called with out-of-range tile index (%d)", lua_tile);
        }

        /*
         * NOTE AND BEWARE: Empty tile is encoded as 0xffffffff
         * That's why tile-index is subtracted by 1
         * See B2GRIDSHAPE_EMPTY_CELL in b2GridShape.h
         */
        uint32_t tile = lua_tile - 1;

        int min_x, min_y, grid_w, grid_h;
        GetTileGridBounds(component, &min_x, &min_y, &grid_w, &grid_h);

        int32_t cell_x, cell_y;
        GetTileGridCellCoord(component, x, y, cell_x, cell_y);

        if (cell_x < 0 || cell_x >= grid_w || cell_y < 0 || cell_y >= grid_h)
        {
            dmLogError("Could not set the tile since the supplied tile was out of range.");
            lua_pushboolean(L, 0);
            assert(top + 1 == lua_gettop(L));
            return 1;
        }
        uint8_t bitmask = 0;
        if (lua_isnumber(L, 6) && top == 6)
        {
            // Read more info about bitmask and constants values in SETCONSTANT macros
            bitmask = dmMath::Abs(luaL_checkinteger(L, 6));
            if (bitmask > MAX_TRANSFORM_FLAG)
            {
                return luaL_error(L, "tilemap.set_tile called with wrong tranformation bitmask (tile: %d)", lua_tile);
            }
        }
        else
        {
            // deprecated API flow with boolean flags
            bool flip_h = lua_toboolean(L, 6);
            bool flip_v = lua_toboolean(L, 7);
            if (flip_h)
            {
                bitmask = FLIP_HORIZONTAL;
            }
            if (flip_v)
            {
                bitmask |= FLIP_VERTICAL;
            }
        }

        SetTileGridTile(component, layer_index, cell_x, cell_y, tile, bitmask);

        dmMessage::URL sender;
        if (dmScript::GetURL(L, &sender))
        {
            // Broadcast to any collision object components
            // TODO Filter broadcast to only collision objects
            dmPhysicsDDF::SetGridShapeHull set_hull_ddf;
            set_hull_ddf.m_Shape = layer_index;
            set_hull_ddf.m_Column = cell_x;
            set_hull_ddf.m_Row = cell_y;
            set_hull_ddf.m_Hull = tile;
            set_hull_ddf.m_FlipHorizontal = (bitmask & FLIP_HORIZONTAL) > 0 ? 1 : 0;
            set_hull_ddf.m_FlipVertical = (bitmask & FLIP_VERTICAL) > 0 ? 1 : 0;
            set_hull_ddf.m_Rotate90 = (bitmask & ROTATE_90) > 0 ? 1 : 0;
            dmhash_t message_id = dmPhysicsDDF::SetGridShapeHull::m_DDFDescriptor->m_NameHash;
            uintptr_t descriptor = (uintptr_t)dmPhysicsDDF::SetGridShapeHull::m_DDFDescriptor;
            uint32_t data_size = sizeof(dmPhysicsDDF::SetGridShapeHull);
            receiver.m_Fragment = 0;
            dmMessage::Result result = dmMessage::Post(&sender, &receiver, message_id, 0, descriptor, &set_hull_ddf, data_size, 0);
            if (result != dmMessage::RESULT_OK)
            {
                dmLogError("Could not send %s to components, result: %d.", dmPhysicsDDF::SetGridShapeHull::m_DDFDescriptor->m_Name, result);
            }
        }
        else
        {
            return luaL_error(L, "tilemap.set_tile is not available from this script-type.");
        }

        lua_pushboolean(L, 1);
        assert(top + 1 == lua_gettop(L));
        return 1;
    }

    static int TileMap_Get(lua_State* L, bool is_full_info)
    {
        int top = lua_gettop(L);

        dmGameObject::HInstance sender_instance = CheckGoInstance(L);
        dmGameObject::HCollection collection = dmGameObject::GetCollection(sender_instance);

        TileGridComponent* component;
        dmGameObject::GetComponentFromLua(L, 1, collection, TILE_MAP_EXT, (dmGameObject::HComponent*)&component, 0, 0);

        dmhash_t layer_id = dmScript::CheckHashOrString(L, 2);
        uint32_t layer_index = GetLayerIndex(component, layer_id);
        if (layer_index == ~0u)
        {
            dmLogError("Could not find layer '%s'.", dmHashReverseSafe64(layer_id));
            lua_pushnil(L);
            assert(top + 1 == lua_gettop(L));
            return 1;
        }

        int x = luaL_checkinteger(L, 3) - 1;
        int y = luaL_checkinteger(L, 4) - 1;

        int min_x, min_y, grid_w, grid_h;
        GetTileGridBounds(component, &min_x, &min_y, &grid_w, &grid_h);

        int32_t cell_x, cell_y;
        GetTileGridCellCoord(component, x, y, cell_x, cell_y);

        if (cell_x < 0 || cell_x >= grid_w || cell_y < 0 || cell_y >= grid_h)
        {
            dmLogError("Could not get the tile since the supplied tile was out of range.");
            lua_pushnil(L);
            assert(top + 1 == lua_gettop(L));
            return 1;
        }

        uint16_t cell = GetTileGridTile(component, layer_index, cell_x, cell_y);

        if (is_full_info)
        {
            lua_newtable(L);

            lua_pushliteral(L, "index");
            lua_pushinteger(L, cell);
            lua_rawset(L, -3);

            uint8_t transform_flags = GetTileTransformMask(component, layer_index, cell_x, cell_y);

            lua_pushliteral(L, "h_flip");
            lua_pushboolean(L, transform_flags & FLIP_HORIZONTAL);
            lua_rawset(L, -3);

            lua_pushliteral(L, "v_flip");
            lua_pushboolean(L, transform_flags & FLIP_VERTICAL);
            lua_rawset(L, -3);

            lua_pushliteral(L, "rotate_90");
            lua_pushboolean(L, transform_flags & ROTATE_90);
            lua_rawset(L, -3);
        }
        else
        {
            lua_pushinteger(L,  cell);
        }

        assert(top + 1 == lua_gettop(L));
        return 1;
    }

    /*# get a tile from a tile map
     * Get the tile set at the specified position in the tilemap.
     * The position is identified by the tile index starting at origin
     * with index 1, 1. (see [ref:tilemap.set_tile()])
     * Which tile map and layer to query is identified by the URL and the
     * layer name parameters.
     *
     * @name tilemap.get_tile
     * @param url [type:string|hash|url] the tile map
     * @param layer [type:string|hash] name of the layer for the tile
     * @param x [type:number] x-coordinate of the tile
     * @param y [type:number] y-coordinate of the tile
     * @return tile [type:number] index of the tile
     * @examples
     *
     * ```lua
     * -- get the tile under the player.
     * local tileno = tilemap.get_tile("/level#tilemap", "foreground", self.player_x, self.player_y)
     * ```
     */
    static int TileMap_GetTile(lua_State* L)
    {
        return TileMap_Get(L, false);
    }

    /*# get full information for a tile from a tile map
     * Get the tile information at the specified position in the tilemap.
     * The position is identified by the tile index starting at origin
     * with index 1, 1. (see [ref:tilemap.set_tile()])
     * Which tile map and layer to query is identified by the URL and the
     * layer name parameters.
     *
     * @name tilemap.get_tile_info
     * @param url [type:string|hash|url] the tile map
     * @param layer [type:string|hash] name of the layer for the tile
     * @param x [type:number] x-coordinate of the tile
     * @param y [type:number] y-coordinate of the tile
     * @return tile_info [type:table] index of the tile
     * @examples
     *
     * ```lua
     * -- get the tile under the player.
     * local tile_info = tilemap.get_tile_info("/level#tilemap", "foreground", self.player_x, self.player_y)
     * pprint(tile_info)
     * -- {
     * --    index = 0,
     * --    h_flip = false,
     * --    v_flip = true,
     * --    rotate_90 = false
     * -- }
     * ```
     */
    static int TileMap_GetTileInfo(lua_State* L)
    {
        return TileMap_Get(L, true);
    }

    /*# get all the tiles from a layer in a tilemap
     * Retrieves all the tiles for the specified layer in the tilemap.
     * It returns a table of rows where the keys are the
     * tile positions (see [ref:tilemap.get_bounds()]).
     * You can iterate it using `tiles[row_index][column_index]`.
     *
     * @name tilemap.get_tiles
     * @param url [type:string|hash|url] the tilemap
     * @param layer [type:string|hash] the name of the layer for the tiles
     * @return tiles [type:table] a table of rows representing the layer
     * @examples
     *
     * ```lua
     * local left, bottom, columns_count, rows_count = tilemap.get_bounds("#tilemap")
     * local tiles = tilemap.get_tiles("#tilemap", "layer")
     * local tile, count = 0, 0
     * for row_index = bottom, bottom + rows_count - 1 do
     *     for column_index = left, left + columns_count - 1 do
     *         tile = tiles[row_index][column_index]
     *         count = count + 1
     *     end
     * end
     * ```
     */
    static int TileMap_GetTiles(lua_State* L)
    {
        int top = lua_gettop(L);

        dmGameObject::HInstance sender_instance = CheckGoInstance(L);
        dmGameObject::HCollection collection = dmGameObject::GetCollection(sender_instance);

        TileGridComponent* component;
        dmGameObject::GetComponentFromLua(L, 1, collection, TILE_MAP_EXT, (dmGameObject::HComponent*)&component, 0, 0);

        dmhash_t layer_id = dmScript::CheckHashOrString(L, 2);
        uint32_t layer_index = GetLayerIndex(component, layer_id);
        if (layer_index == ~0u)
        {
            dmLogError("Could not find layer '%s'.", dmHashReverseSafe64(layer_id));
            lua_pushnil(L);
            assert(top + 1 == lua_gettop(L));
            return 1;
        }

        int min_x, min_y, grid_w, grid_h;
        GetTileGridBounds(component, &min_x, &min_y, &grid_w, &grid_h);

        int32_t cell_x, cell_y;
        GetTileGridCellCoord(component, min_x, min_y, cell_x, cell_y);

        lua_newtable(L);
        for (int iy = 0; iy < grid_h; iy++)
        {
            lua_pushinteger(L, min_y + iy + 1);
            lua_newtable(L);
            for (int ix = 0; ix < grid_w; ix++)
            {
                uint16_t cell = GetTileGridTile(component, layer_index, cell_x + ix, cell_y + iy);
                lua_pushinteger(L, min_x + ix + 1);
                lua_pushinteger(L, cell);
                lua_settable(L, -3);
            }

            lua_settable(L, -3);
        }
        assert(top + 1 == lua_gettop(L));
        return 1;
    }

    /*# get the bounds of a tile map
     * Get the bounds for a tile map. This function returns multiple values:
     * The lower left corner index x and y coordinates (1-indexed),
     * the tile map width and the tile map height.
     *
     * The resulting values take all tile map layers into account, meaning that
     * the bounds are calculated as if all layers were collapsed into one.
     *
     * @name tilemap.get_bounds
     * @param url [type:string|hash|url] the tile map
     * @return x [type:number] x coordinate of the bottom left corner
     * @return y [type:number] y coordinate of the bottom left corner
     * @return w [type:number] number of columns (width) in the tile map
     * @return h [type:number] number of rows (height) in the tile map
     * @examples
     *
     * ```lua
     * -- get the level bounds.
     * local x, y, w, h = tilemap.get_bounds("/level#tilemap")
     * ```
     */
    static int TileMap_GetBounds(lua_State* L)
    {
        int top = lua_gettop(L);

        dmGameObject::HInstance sender_instance = CheckGoInstance(L);
        dmGameObject::HCollection collection = dmGameObject::GetCollection(sender_instance);

        TileGridComponent* component;
        dmGameObject::GetComponentFromLua(L, 1, collection, TILE_MAP_EXT, (dmGameObject::HComponent*)&component, 0, 0);

        int x, y, w, h;
        GetTileGridBounds(component, &x, &y, &w, &h);

        lua_pushinteger(L, x + 1);
        lua_pushinteger(L, y + 1);
        lua_pushinteger(L, w);
        lua_pushinteger(L, h);

        assert(top + 4 == lua_gettop(L));
        return 4;
    }

    /*# set the visibility of a layer
     * Sets the visibility of the tilemap layer
     *
     * @name tilemap.set_visible
     * @param url [type:string|hash|url] the tile map
     * @param layer [type:string|hash] name of the layer for the tile
     * @param visible [type:boolean] should the layer be visible
     * @examples
     *
     * ```lua
     * -- Disable rendering of the layer
     * tilemap.set_visible("/level#tilemap", "foreground", false)
     * ```
     */
    static int TileMap_SetVisible(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 0);

        dmGameObject::HInstance sender_instance = CheckGoInstance(L);
        dmGameObject::HCollection collection = dmGameObject::GetCollection(sender_instance);

        TileGridComponent* component;
        dmMessage::URL receiver;
        dmGameObject::GetComponentFromLua(L, 1, collection, TILE_MAP_EXT, (dmGameObject::HComponent*)&component, &receiver, 0);

        dmhash_t layer_id = dmScript::CheckHashOrString(L, 2);
        uint32_t layer_index = GetLayerIndex(component, layer_id);
        if (layer_index == ~0u)
        {
            return DM_LUA_ERROR("Could not find layer '%s'.", dmHashReverseSafe64(layer_id));
        }

        bool visible = lua_toboolean(L, 3);
        SetLayerVisible(component, layer_index, visible);

        dmMessage::URL sender;
        if (dmScript::GetURL(L, &sender))
        {
            // Broadcast to any collision object components under this game object
            // TODO Filter broadcast to only collision objects
            dmPhysicsDDF::EnableGridShapeLayer ddf;
            ddf.m_Shape = layer_index;
            ddf.m_Enable = visible;
            dmhash_t message_id = dmPhysicsDDF::EnableGridShapeLayer::m_DDFDescriptor->m_NameHash;
            uintptr_t descriptor = (uintptr_t)dmPhysicsDDF::EnableGridShapeLayer::m_DDFDescriptor;
            uint32_t data_size = sizeof(dmPhysicsDDF::EnableGridShapeLayer);
            receiver.m_Fragment = 0;
            dmMessage::Result result = dmMessage::Post(&sender, &receiver, message_id, 0, descriptor, &ddf, data_size, 0);
            if (result != dmMessage::RESULT_OK)
            {
                dmLogError("Could not send %s to components, result: %d.", dmPhysicsDDF::EnableGridShapeLayer::m_DDFDescriptor->m_Name, result);
            }
        }
        else
        {
            return luaL_error(L, "tilemap.set_tile is not available from this script-type.");
        }

        return 0;
    }

    static const luaL_reg TILEMAP_FUNCTIONS[] =
    {
        {"set_constant",    TileMap_SetConstant},
        {"reset_constant",  TileMap_ResetConstant},
        {"set_tile",        TileMap_SetTile},
        {"get_tile",        TileMap_GetTile},
        {"get_tiles",       TileMap_GetTiles},
        {"get_tile_info",   TileMap_GetTileInfo},
        {"get_bounds",      TileMap_GetBounds},
        {"set_visible",     TileMap_SetVisible},
        {0, 0}
    };

    /*# flip tile horizontally
     *
     * @name tilemap.H_FLIP
     * @constant
     */
    /*# flip tile vertically
     *
     * @name tilemap.V_FLIP
     * @constant
     */
    /*# rotate tile 90 degrees clockwise
     *
     * @name tilemap.ROTATE_90
     * @constant
     */
    /*# rotate tile 180 degrees clockwise
     *
     * @name tilemap.ROTATE_180
     * @constant
     */
    /*# rotate tile 270 degrees clockwise
     *
     * @name tilemap.ROTATE_270
     * @constant
     */

    void ScriptTileMapRegister(const ScriptLibContext& context)
    {
        lua_State* L = context.m_LuaState;
        DM_LUA_STACK_CHECK(L, 0);
        luaL_register(L, "tilemap", TILEMAP_FUNCTIONS);

        #define SETCONSTANT(name, val) \
            lua_pushnumber(L, (lua_Number) val); \
            lua_setfield(L, -2, #name);\

        SETCONSTANT(H_FLIP, FLIP_HORIZONTAL);
        SETCONSTANT(V_FLIP, FLIP_VERTICAL);
        SETCONSTANT(ROTATE_90, ROTATE_90);
        SETCONSTANT(ROTATE_180, -(FLIP_HORIZONTAL + FLIP_VERTICAL));
        SETCONSTANT(ROTATE_270, -(FLIP_HORIZONTAL + FLIP_VERTICAL + ROTATE_90));

        /* It's enough to have 3 flags to specify a quad transform. 1st bit for h_flip, 2nd for v_flip and 3rd for 90 degree rotation.
         * All the other rotations are possible to specify using these 3 bits.
         * For example, rotating a quad 180 degrees is the same as flip it horizontally AND vertically.
         * Here is a transforms correspondence table:
         * |----------------------------------------------
         * |val| bitmask|  basic 3bits | corresponding   |
         * |   |        |  transforms  | transformations |
         * |----------------------------------------------
         * | 0 | (000)  |         R_0  | R_180 + H + V   |
         * | 1 | (001)  |     H + R_0  | R_180 + V       |
         * | 2 | (010)  |     V + R_0  | R_180 + H       |
         * | 3 | (011)  | V + H + R_0  | R_180           |
         * | 4 | (100)  |         R_90 | R_270 + H + V   |
         * | 5 | (101)  |     H + R_90 | R_270 + V       |
         * | 6 | (110)  |     V + R_90 | R_270 + H       |
         * | 7 | (111)  | V + H + R_90 | R_270           |
         * -----------------------------------------------
         *
         * Since we want to use arithmetic sum in Lua API (and avoid using of bit module)
         * and also want to avoid extra arithmetic operations in the engine (because we are doing them on Lua side anyways)
         * we can use mirrored values from basic transforms
         * R_180 = -(V + H + R_0) = -3
         * R_270 = -(V + H + R_90) = -7
         * To make sure that the final bitmask is equal to the original one we should use Math::Abs() function on it.
         */

        #undef SETCONSTANT

        lua_pop(L, 1);
    }
}
