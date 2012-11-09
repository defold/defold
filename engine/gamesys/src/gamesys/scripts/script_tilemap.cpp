#include "script_tilemap.h"

#include "tile_ddf.h"

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

        dmMessage::URL sender;
        uintptr_t user_data;
        if (dmScript::GetURL(L, &sender) && dmScript::GetUserData(L, &user_data) && user_data != 0)
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

        dmMessage::URL sender;
        uintptr_t user_data;
        if (dmScript::GetURL(L, &sender) && dmScript::GetUserData(L, &user_data) && user_data != 0)
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

            const uint32_t buffer_size = 256;
            uint8_t buffer[buffer_size];
            dmGameSystemDDF::ResetConstantTileMap* request = (dmGameSystemDDF::ResetConstantTileMap*)buffer;

            uint32_t msg_size = sizeof(dmGameSystemDDF::ResetConstantTileMap);

            request->m_NameHash = name_hash;

            dmMessage::URL receiver;

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

    static const luaL_reg TILEMAP_FUNCTIONS[] =
    {
        {"set_constant",    TileMap_SetConstant},
        {"reset_constant",  TileMap_ResetConstant},
        {0, 0}
    };

    void ScriptTileMapRegister(void* context)
    {
        lua_State* L = (lua_State*)context;
        luaL_register(L, "tilemap", TILEMAP_FUNCTIONS);
        lua_pop(L, 1);
    }
}
