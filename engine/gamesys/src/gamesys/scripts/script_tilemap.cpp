#include <dlib/log.h>
#include <ddf/ddf.h>
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
    /*# set a shader constant for a tile map
     * The constant must be defined in the material assigned to the tile map.
     * Setting a constant through this function will override the value set for that constant in the material.
     * The value will be overridden until tilemap.reset_constant is called.
     * Which tile map to set a constant for is identified by the URL.
     *
     * @name tilemap.set_constant
     * @param url the tile map that should have a constant set (url)
     * @param name of the constant (string|hash)
     * @param value of the constant (vec4)
     * @examples
     * <p>
     * The following examples assumes that the tile map has id "tile map" and that the default-material in builtins is used.
     * If you assign a custom material to the tile map, you can set the constants defined there in the same manner.
     * </p>
     * <p>
     * How to tint a tile map to red:
     * </p>
     * <pre>
     * function init(self)
     *     tilemap.set_constant("#tilemap", "tint", vmath.vector4(1, 0, 0, 1))
     * end
     * </pre>
     */
    int TileMap_SetConstant(lua_State* L)
    {
        int top = lua_gettop(L);

        dmGameObject::HInstance instance = CheckGoInstance(L);

        dmhash_t name_hash;
        if (lua_isstring(L, 2))
        {
            name_hash = dmHashString64(lua_tostring(L, 2));
        }
        else if (dmScript::IsHash(L, 2))
        {
            name_hash = dmScript::CheckHash(L, 2);
        }
        else
        {
            return luaL_error(L, "name must be either a hash or a string");
        }
        Vectormath::Aos::Vector4* value = dmScript::CheckVector4(L, 3);

        const uint32_t buffer_size = 256;
        uint8_t buffer[buffer_size];
        dmGameSystemDDF::SetConstantTileMap* request = (dmGameSystemDDF::SetConstantTileMap*)buffer;

        uint32_t msg_size = sizeof(dmGameSystemDDF::SetConstantTileMap);

        request->m_NameHash = name_hash;
        request->m_Value = *value;

        dmMessage::URL receiver;
        dmMessage::URL sender;
        dmScript::ResolveURL(L, 1, &receiver, &sender);

        dmMessage::Post(&sender, &receiver, dmGameSystemDDF::SetConstantTileMap::m_DDFDescriptor->m_NameHash, (uintptr_t)instance, (uintptr_t)dmGameSystemDDF::SetConstantTileMap::m_DDFDescriptor, buffer, msg_size);
        assert(top == lua_gettop(L));
        return 0;
    }

    /*# reset a shader constant for a tile map
     * The constant must be defined in the material assigned to the tile map.
     * Resetting a constant through this function implies that the value defined in the material will be used.
     * Which tile map to reset a constant for is identified by the URL.
     *
     * @name tilemap.reset_constant
     * @param url the tile map that should have a constant reset (url)
     * @param name of the constant (string|hash)
     * @examples
     * <p>
     * The following examples assumes that the tile map has id "tilemap" and that the default-material in builtins is used.
     * If you assign a custom material to the tile map, you can reset the constants defined there in the same manner.
     * </p>
     * <p>
     * How to reset the tinting of a tile map:
     * </p>
     * <pre>
     * function init(self)
     *     tilemap.reset_constant("#tilemap", "tint")
     * end
     * </pre>
     */
    int TileMap_ResetConstant(lua_State* L)
    {
        int top = lua_gettop(L);

        dmGameObject::HInstance instance = CheckGoInstance(L);

        dmhash_t name_hash;
        if (lua_isstring(L, 2))
        {
            name_hash = dmHashString64(lua_tostring(L, 2));
        }
        else if (dmScript::IsHash(L, 2))
        {
            name_hash = dmScript::CheckHash(L, 2);
        }
        else
        {
            return luaL_error(L, "name must be either a hash or a string");
        }

        // TODO: Why is a separate buffer used here and not a stack-allocated dmGameSystemDDF::ResetConstantTileMap?
        // dmGameSystemDDF::ResetConstantTileMap contains no members that require "dynamic" memory, i.e. strings
        // See also TileMap_SetConstant
        const uint32_t buffer_size = 256;
        uint8_t buffer[buffer_size];
        dmGameSystemDDF::ResetConstantTileMap* request = (dmGameSystemDDF::ResetConstantTileMap*)buffer;

        uint32_t msg_size = sizeof(dmGameSystemDDF::ResetConstantTileMap);

        request->m_NameHash = name_hash;

        dmMessage::URL receiver;
        dmMessage::URL sender;
        dmScript::ResolveURL(L, 1, &receiver, &sender);

        dmMessage::Post(&sender, &receiver, dmGameSystemDDF::ResetConstantTileMap::m_DDFDescriptor->m_NameHash, (uintptr_t)instance, (uintptr_t)dmGameSystemDDF::ResetConstantTileMap::m_DDFDescriptor, buffer, msg_size);
        assert(top == lua_gettop(L));
        return 0;
    }

    /*# set a tile in a tile map
     * Replace a tile in a tile map with a new tile. The coordinates of the tile is 1-indexed so a 4 by 4
     * tile map centered around origo has the following x,y coordinates:
     * <pre>
     * +-------+-------+------+------+
     * | -2,1  | -1,1  | 0,1  | 1,1  |
     * +-------+-------+------+------+
     * | -2,0  | -1,0  | 0,0  | 1,0  |
     * +-------+-------O------+------+
     * | -2,-1 | -1,-1 | 0,-1 | 1,-1 |
     * +-------+-------+------+------+
     * | -2,-2 | -1,-2 | 0,-2 | 1,-2 |
     * +-------+-------+------+------+
     * </pre>
     * The coordinates must be within the bounds of the tile map as it were created. That is, it is not
     * possible to extend the size of a tile map by setting tiles outside the edges.
     * The tile to set is identified by its index starting with 1 in the top left corner of the tile set.
     * To clear a tile, set the tile to number 0. Which tile map and layer to manipulate is identified by
     * the URL and the layer name parameters.
     *
     * @name tilemap.set_tile
     * @param url the tile map (url)
     * @param name of the layer (string|hash)
     * @param x-coordinate of the tile (number)
     * @param y-coordinate of the tile (number)
     * @param new tile to set (number)
     * @param optional if the tile should be horizontally flipped (boolean)
     * @param optional i the tile should be vertically flipped (boolean)
     * @examples
     * <pre>
     * -- Clear the tile under the player.
     * tilemap.set_tile("/level#tilemap", "foreground", self.player_x, self.player_y, 0)
     * </pre>
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

        dmhash_t layer_id;
        if (lua_isstring(L, 2))
        {
            layer_id = dmHashString64(lua_tostring(L, 2));
        }
        else if (dmScript::IsHash(L, 2))
        {
            layer_id = dmScript::CheckHash(L, 2);
        }
        else
        {
            return luaL_error(L, "name must be either a hash or a string");
        }

        uint32_t layer_index = GetLayerIndex(component, layer_id);
        if (layer_index == ~0u)
        {
            dmLogError("Could not find layer %s.", (char*)dmHashReverse64(layer_id, 0x0));
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
            dmMessage::Result result = dmMessage::Post(&sender, &receiver, message_id, 0, descriptor, &set_hull_ddf, data_size);
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
     * Get the tile set at the specified position in the tilemap. The returned tile to set is identified
     * by its index starting with 1 in the top left corner of the tile set, or 0 if the tile is blank.
     * The coordinates of the tile is 1-indexed (see <code>tilemap.set_tile()</code>)
     * Which tile map and layer to query is identified by the URL and the layer name parameters.
     *
     * @name tilemap.get_tile
     * @param url the tile map (url)
     * @param name of the layer (string|hash)
     * @param x-coordinate of the tile (number)
     * @param y-coordinate of the tile (number)
     * @return index of the tile (number)
     * @examples
     * <pre>
     * -- get the tile under the player.
     * local tileno = tilemap.get_tile("/level#tilemap", "foreground", self.player_x, self.player_y)
     * </pre>
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

        dmhash_t layer_id;
        if (lua_isstring(L, 2))
        {
            layer_id = dmHashString64(lua_tostring(L, 2));
        }
        else if (dmScript::IsHash(L, 2))
        {
            layer_id = dmScript::CheckHash(L, 2);
        }
        else
        {
            return luaL_error(L, "name must be either a hash or a string");
        }

        uint32_t layer_index = GetLayerIndex(component, layer_id);
        if (layer_index == ~0u)
        {
            dmLogError("Could not find layer %s.", (char*)dmHashReverse64(layer_id, 0x0));
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
     * Get the tile set at the specified position in the tilemap. The returned tile to set is identified
     * by its index starting with 1 in the top left corner of the tile set. The coordinates of the tile is
     * 1-indexed (see <code>tilemap.set_tile()</code>) Which tile map and layer to query is identified by
     * the URL and the layer name parameters.
     *
     * @name tilemap.get_bounds
     * @param url the tile map (url)
     * @return x coordinate of the bottom left corner (number)
     * @return y coordinate of the bottom left corner (number)
     * @return number of columns in the tile map (number)
     * @return number of rows in the tile map (number)
     * @examples
     * <pre>
     * -- get the level bounds.
     * local x, y, w, h = tilemap.get_bounds("/level#tilemap")
     * </pre>
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
