#include "script_tilemap.h"

#include <dlib/log.h>
#include "tile_ddf.h"
#include "../components/comp_tilegrid.h"
#include "../proto/physics_ddf.h"

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

        uintptr_t user_data;
        if (dmScript::GetUserData(L, &user_data) && user_data != 0)
        {
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

            dmMessage::Post(&sender, &receiver, dmGameSystemDDF::SetConstantTileMap::m_DDFDescriptor->m_NameHash, user_data, (uintptr_t)dmGameSystemDDF::SetConstantTileMap::m_DDFDescriptor, buffer, msg_size);
            assert(top == lua_gettop(L));
            return 0;
        }
        else
        {
            assert(top == lua_gettop(L));
            return luaL_error(L, "tilemap.set_constant is not available from this script-type.");
        }
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

        uintptr_t user_data;
        if (dmScript::GetUserData(L, &user_data) && user_data != 0)
        {
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

            dmMessage::Post(&sender, &receiver, dmGameSystemDDF::ResetConstantTileMap::m_DDFDescriptor->m_NameHash, user_data, (uintptr_t)dmGameSystemDDF::ResetConstantTileMap::m_DDFDescriptor, buffer, msg_size);
            assert(top == lua_gettop(L));
            return 0;
        }
        else
        {
            assert(top == lua_gettop(L));
            return luaL_error(L, "tilemap.reset_constant is not available from this script-type.");
        }
    }

    int TileMap_SetTile(lua_State* L)
    {
        int top = lua_gettop(L);

        uintptr_t user_data;
        dmMessage::URL receiver;
        dmGameObject::GetInstanceFromLua(L, 1, &user_data, &receiver);
        TileGridComponent* component = (TileGridComponent*) user_data;
        TileGridResource* resource = component->m_TileGridResource;

        const char* layer = luaL_checkstring(L, 2);
        dmhash_t layer_id = dmHashString64(layer);
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

    int TileMap_GetTile(lua_State* L)
    {
        int top = lua_gettop(L);

        uintptr_t user_data;
        dmGameObject::GetInstanceFromLua(L, 1, &user_data, 0);
        TileGridComponent* component = (TileGridComponent*) user_data;
        TileGridResource* resource = component->m_TileGridResource;

        const char* layer = luaL_checkstring(L, 2);
        dmhash_t layer_id = dmHashString64(layer);
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

    int TileMap_GetBounds(lua_State* L)
    {
        int top = lua_gettop(L);

        uintptr_t user_data;
        dmGameObject::GetInstanceFromLua(L, 1, &user_data, 0);
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

    void ScriptTileMapRegister(void* context)
    {
        lua_State* L = (lua_State*)context;
        luaL_register(L, "tilemap", TILEMAP_FUNCTIONS);
        lua_pop(L, 1);
    }
}
