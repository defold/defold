#include "script.h"

#include <assert.h>
#include <string.h>

#include <dlib/dstrings.h>
#include <dlib/hash.h>

extern "C"
{
#include <lua/lauxlib.h>
#include <lua/lualib.h>
}

namespace dmScript
{
    #define SCRIPT_TYPE_NAME_HASH "hash"

    bool IsHash(lua_State *L, int index)
    {
        void *p = lua_touserdata(L, index);
        bool result = false;
        if (p != 0x0)
        {  /* value is a userdata? */
            if (lua_getmetatable(L, index))
            {  /* does it have a metatable? */
                lua_getfield(L, LUA_REGISTRYINDEX, SCRIPT_TYPE_NAME_HASH);  /* get correct metatable */
                if (lua_rawequal(L, -1, -2))
                {  /* does it have the correct mt? */
                    result = true;
                }
                lua_pop(L, 2);  /* remove both metatables */
            }
        }
        return result;
    }

    /*# hashes a string
     * All ids in the engine are represented as hashes, so a string needs to be hashed
     * before it can be compared with an id.
     *
     * @name hash
     * @param s string to hash (string)
     * @return a hashed string (hash)
     * @examples
     * <p>To compare a message_id in an on-message callback function:</p>
     * <pre>
     * function on_message(self, message_id, message)
     *     if message_id == hash("my_message") then
     *         -- Act on the message here
     *     end
     * end
     * </pre>
     */
    int Script_Hash(lua_State* L)
    {
        int top = lua_gettop(L);

        const char* str = luaL_checkstring(L, 1);
        dmhash_t hash = dmHashString64(str);
        PushHash(L, hash);

        assert(top + 1 == lua_gettop(L));

        return 1;
    }

    void PushHash(lua_State* L, dmhash_t hash)
    {
        dmhash_t* lua_hash = (dmhash_t*)lua_newuserdata(L, sizeof(dmhash_t));
        *lua_hash = hash;
        luaL_getmetatable(L, SCRIPT_TYPE_NAME_HASH);
        lua_setmetatable(L, -2);
    }

    dmhash_t CheckHash(lua_State* L, int index)
    {
        dmhash_t* lua_hash = 0x0;
        if (lua_isuserdata(L, index))
        {
            lua_hash = (dmhash_t*)lua_touserdata(L, index);
            return *lua_hash;
        }

        luaL_typerror(L, index, SCRIPT_TYPE_NAME_HASH);
        return 0;
    }

    int Script_eq(lua_State* L)
    {
        dmhash_t hash1 = CheckHash(L, 1);
        dmhash_t hash2 = CheckHash(L, 2);

        lua_pushboolean(L, hash1 == hash2);

        return 1;
    }

    static int Script_gc (lua_State *L)
    {
        dmhash_t hash = CheckHash(L, 1);
        (void)hash;
        return 0;
    }

    static int Script_tostring(lua_State* L)
    {
        dmhash_t hash = CheckHash(L, 1);
        char buffer[64];
        DM_SNPRINTF(buffer, sizeof(buffer), "%s: [%llu]", SCRIPT_TYPE_NAME_HASH, hash);
        lua_pushstring(L, buffer);
        return 1;
    }

    static int Script_concat(lua_State *L)
    {
        const char* s = luaL_checkstring(L, 1);
        dmhash_t hash = CheckHash(L, 2);
        size_t size = 64 + strlen(s);
        char* buffer = new char[size];
        DM_SNPRINTF(buffer, size, "%s[%llu]", s, hash);
        lua_pushstring(L, buffer);
        delete [] buffer;
        return 1;
    }

    static const luaL_reg ScriptHash_methods[] =
    {
        {SCRIPT_TYPE_NAME_HASH, Script_Hash},
        {0, 0}
    };

    void InitializeHash(lua_State* L)
    {
        int top = lua_gettop(L);
        luaL_newmetatable(L, SCRIPT_TYPE_NAME_HASH);

        luaL_openlib(L, 0x0, ScriptHash_methods, 0);
        lua_pushstring(L, "__gc");
        lua_pushcfunction(L, Script_gc);
        lua_settable(L, -3);

        lua_pushstring(L, "__eq");
        lua_pushcfunction(L, Script_eq);
        lua_settable(L, -3);

        lua_pushstring(L, "__tostring");
        lua_pushcfunction(L, Script_tostring);
        lua_settable(L, -3);

        lua_pushstring(L, "__concat");
        lua_pushcfunction(L, Script_concat);
        lua_settable(L, -3);

        lua_pushcfunction(L, Script_Hash);
        lua_setglobal(L, SCRIPT_TYPE_NAME_HASH);

        lua_pop(L, 1);

        assert(top == lua_gettop(L));
    }

    #undef SCRIPT_TYPE_NAME_HASH
}
