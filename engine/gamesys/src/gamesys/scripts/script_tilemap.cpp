#include <dlib/log.h>
#include <ddf/ddf.h>
#include <script/script.h>
#include "tile_ddf.h"
#include "../components/comp_tilegrid.h"
#include "../proto/physics_ddf.h"
#include "gamesys.h"
#include "../gamesys_private.h"
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

    /*# set a shader constant for a tile map
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
    int TileMap_SetConstant(lua_State* L)
    {
        int top = lua_gettop(L);

        dmGameObject::HInstance instance = CheckGoInstance(L);

        dmhash_t name_hash = dmScript::CheckHashOrString(L, 2);
        Vectormath::Aos::Vector4* value = dmScript::CheckVector4(L, 3);

        dmGameSystemDDF::SetConstantTileMap msg;
        msg.m_NameHash = name_hash;
        msg.m_Value = *value;

        dmMessage::URL receiver;
        dmMessage::URL sender;
        dmScript::ResolveURL(L, 1, &receiver, &sender);

        dmMessage::Post(&sender, &receiver, dmGameSystemDDF::SetConstantTileMap::m_DDFDescriptor->m_NameHash, (uintptr_t)instance, (uintptr_t)dmGameSystemDDF::SetConstantTileMap::m_DDFDescriptor, &msg, sizeof(msg), 0);
        assert(top == lua_gettop(L));
        return 0;
    }

    /*# reset a shader constant for a tile map
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
    int TileMap_ResetConstant(lua_State* L)
    {
        int top = lua_gettop(L);

        dmGameObject::HInstance instance = CheckGoInstance(L);
        dmhash_t name_hash = dmScript::CheckHashOrString(L, 2);

        dmGameSystemDDF::ResetConstantTileMap msg;
        msg.m_NameHash = name_hash;

        dmMessage::URL receiver;
        dmMessage::URL sender;
        dmScript::ResolveURL(L, 1, &receiver, &sender);

        dmMessage::Post(&sender, &receiver, dmGameSystemDDF::ResetConstantTileMap::m_DDFDescriptor->m_NameHash, (uintptr_t)instance, (uintptr_t)dmGameSystemDDF::ResetConstantTileMap::m_DDFDescriptor, &msg, sizeof(msg), 0);
        assert(top == lua_gettop(L));
        return 0;
    }

    /*# set a tile in a tile map
     * Replace a tile in a tile map with a new tile.
     * The coordinates of the tiles are indexed so that the "first" tile just
     * above and to the right of origo has coordinates 1,1.
     * Tiles to the left of and below origo are indexed 0, -1, -2 and so forth.
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
     * @name tilemap.set_tile
     * @param url [type:string|hash|url] the tile map
     * @param layer [type:string|hash] name of the layer for the tile
     * @param x [type:number] x-coordinate of the tile
     * @param y [type:number] y-coordinate of the tile
     * @param tile [type:number] index of new tile to set
     * @param [h-flipped] [type:boolean] optional if the tile should be horizontally flipped
     * @param [v-flipped] [type:boolean] optional i the tile should be vertically flipped
     * @examples
     *
     * ```lua
     * -- Clear the tile under the player.
     * tilemap.set_tile("/level#tilemap", "foreground", self.player_x, self.player_y, 0)
     * ```
     */
    int TileMap_SetTile(lua_State* L)
    {
        int top = lua_gettop(L);

        dmGameObject::HInstance sender_instance = CheckGoInstance(L);
        dmGameObject::HCollection collection = dmGameObject::GetCollection(sender_instance);

        uintptr_t user_data;
        dmMessage::URL receiver;
        dmGameObject::GetComponentUserDataFromLua(L, 1, collection, TILE_MAP_EXT, &user_data, &receiver, 0);
        TileGridComponent* component = (TileGridComponent*) user_data;
        TileGridResource* resource = component->m_TileGridResource;

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
        /*
         * NOTE AND BEWARE: Empty tile is encoded as 0xffffffff
         * That's why tile-index is subtracted by 1
         * See B2GRIDSHAPE_EMPTY_CELL in b2GridShape.h
         */
        uint32_t tile = ((uint16_t) luaL_checkinteger(L, 5)) - 1;
        int32_t cell_x = x - resource->m_MinCellX, cell_y = y - resource->m_MinCellY;
        if (cell_x < 0 || cell_x >= (int32_t)resource->m_ColumnCount || cell_y < 0 || cell_y >= (int32_t)resource->m_RowCount)
        {
            dmLogError("Could not set the tile since the supplied tile was out of range.");
            lua_pushboolean(L, 0);
            assert(top + 1 == lua_gettop(L));
            return 1;
        }
        uint32_t cell_index = CalculateCellIndex(layer_index, cell_x, cell_y, resource->m_ColumnCount, resource->m_RowCount);
        uint32_t region_x = cell_x / TILEGRID_REGION_WIDTH;
        uint32_t region_y = cell_y / TILEGRID_REGION_HEIGHT;
        uint32_t region_index = region_y * component->m_RegionsX + region_x;
        TileGridRegion* region = &component->m_Regions[region_index];
        region->m_Dirty = true;
        component->m_Cells[cell_index] = tile;
        TileGridComponent::Flags* flags = &component->m_CellFlags[cell_index];
        flags->m_FlipHorizontal = lua_toboolean(L, 6);
        flags->m_FlipVertical = lua_toboolean(L, 7);

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
            set_hull_ddf.m_FlipHorizontal = flags->m_FlipHorizontal;
            set_hull_ddf.m_FlipVertical = flags->m_FlipVertical;
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

    /*# get a tile from a tile map
     * Get the tile set at the specified position in the tilemap.
     * The position is identified by the tile index starting at origo
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
    int TileMap_GetTile(lua_State* L)
    {
        int top = lua_gettop(L);

        dmGameObject::HInstance sender_instance = CheckGoInstance(L);
        dmGameObject::HCollection collection = dmGameObject::GetCollection(sender_instance);

        uintptr_t user_data;
        dmGameObject::GetComponentUserDataFromLua(L, 1, collection, TILE_MAP_EXT, &user_data, 0, 0);
        TileGridComponent* component = (TileGridComponent*) user_data;
        TileGridResource* resource = component->m_TileGridResource;

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
        int32_t cell_x = x - resource->m_MinCellX, cell_y = y - resource->m_MinCellY;
        if (cell_x < 0 || cell_x >= (int32_t)resource->m_ColumnCount || cell_y < 0 || cell_y >= (int32_t)resource->m_RowCount)
        {
            dmLogError("Could not get the tile since the supplied tile was out of range.");
            lua_pushnil(L);
            assert(top + 1 == lua_gettop(L));
            return 1;
        }
        uint32_t cell_index = CalculateCellIndex(layer_index, cell_x, cell_y, resource->m_ColumnCount, resource->m_RowCount);
        uint16_t cell = (component->m_Cells[cell_index] + 1);
        lua_pushinteger(L,  cell);
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
    int TileMap_GetBounds(lua_State* L)
    {
        int top = lua_gettop(L);

        dmGameObject::HInstance sender_instance = CheckGoInstance(L);
        dmGameObject::HCollection collection = dmGameObject::GetCollection(sender_instance);

        uintptr_t user_data;
        dmGameObject::GetComponentUserDataFromLua(L, 1, collection, TILE_MAP_EXT, &user_data, 0, 0);
        TileGridComponent* component = (TileGridComponent*) user_data;
        TileGridResource* resource = component->m_TileGridResource;

        int x = resource->m_MinCellX + 1;
        int y = resource->m_MinCellY + 1;
        int w = resource->m_ColumnCount;
        int h = resource->m_RowCount;

        lua_pushinteger(L, x);
        lua_pushinteger(L, y);
        lua_pushinteger(L, w);
        lua_pushinteger(L, h);

        assert(top + 4 == lua_gettop(L));
        return 4;
    }

    static const luaL_reg TILEMAP_FUNCTIONS[] =
    {
        {"set_constant",    TileMap_SetConstant},
        {"reset_constant",  TileMap_ResetConstant},
        {"set_tile",        TileMap_SetTile},
        {"get_tile",        TileMap_GetTile},
        {"get_bounds",      TileMap_GetBounds},
        {0, 0}
    };

    void ScriptTileMapRegister(const ScriptLibContext& context)
    {
        lua_State* L = context.m_LuaState;
        luaL_register(L, "tilemap", TILEMAP_FUNCTIONS);
        lua_pop(L, 1);
    }
}
