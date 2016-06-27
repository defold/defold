#include "script.h"

#include <assert.h>
#include <string.h>

#include <dlib/dstrings.h>
#include <dlib/hash.h>
#include <dlib/md5.h>
#include "script_private.h"

extern "C"
{
#include <lua/lauxlib.h>
#include <lua/lualib.h>
}

namespace dmScript
{
    #define SCRIPT_TYPE_NAME_HASH "hash"
    #define SCRIPT_HASH_TABLE "__script_hash_table"

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
     * function on_message(self, message_id, message, sender)
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

    /*# get hex representation of a hash value as a string
     * The returned string is always padded with leading zeros
     *
     * @name hash_to_hex
     * @param h hash value to get hex string for
     * @return hex representation
     */
    int Script_HashToHex(lua_State* L)
    {
        int top = lua_gettop(L);

        dmhash_t hash = dmScript::CheckHash(L, 1);
        char buf[17];
        DM_SNPRINTF(buf, sizeof(buf), "%016llx", hash);
        lua_pushstring(L, buf);

        assert(top + 1 == lua_gettop(L));
        return 1;
    }

    int Script_HashMD5(lua_State* L)
    {
        int top = lua_gettop(L);

        dmMD5::State s;
        dmMD5::Init(&s);

        size_t len;
        const char* str = luaL_checklstring(L, 1, &len);
        dmMD5::Update(&s, str, len);
        dmMD5::Digest digest;
        dmMD5::Final(&s, &digest);
        uint8_t* d = &digest.m_Digest[0];

        char md5[DM_MD5_SIZE * 2 + 1]; // We need a terminal zero for snprintf
        DM_SNPRINTF(md5, sizeof(md5), "%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x",
                    d[0], d[1], d[2], d[3], d[4], d[5], d[6], d[7], d[8], d[9], d[10], d[11], d[12], d[13], d[14], d[15]);
        lua_pushstring(L, md5);

        assert(top + 1 == lua_gettop(L));
        return 1;
    }

    void PushHash(lua_State* L, dmhash_t hash)
    {
        int top = lua_gettop(L);

        lua_getglobal(L, SCRIPT_CONTEXT);
        Context* context = (Context*) (dmConfigFile::HConfig)lua_touserdata(L, -1);
        lua_pop(L, 1);

        dmHashTable64<int>* instances = &context->m_HashInstances;
        int* refp = instances->Get(hash);
        if (refp)
        {
            lua_rawgeti(L, LUA_REGISTRYINDEX, *refp);
        }
        else
        {
            dmhash_t* lua_hash = (dmhash_t*)lua_newuserdata(L, sizeof(dmhash_t));
            *lua_hash = hash;
            luaL_getmetatable(L, SCRIPT_TYPE_NAME_HASH);
            lua_setmetatable(L, -2);
            lua_pushvalue(L, -1);
            int ref = luaL_ref(L, LUA_REGISTRYINDEX);

            if (instances->Full())
            {
                instances->SetCapacity(instances->Size(), instances->Capacity() + 256);
            }
            instances->Put(hash, ref);
        }

        assert(top + 1 == lua_gettop(L));
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

    dmhash_t CheckHashOrString(lua_State* L, int index)
    {
        dmhash_t* lua_hash = 0x0;
        if (lua_isuserdata(L, index))
        {
            lua_hash = (dmhash_t*)lua_touserdata(L, index);
            return *lua_hash;
        }
        else if( lua_type(L, index) == LUA_TSTRING )
        {
            const char* s = lua_tostring(L, index);
            return dmHashString64(s);
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
        const char* reverse = (const char*) dmHashReverse64(hash, 0);
        if (reverse != 0x0)
        {
            DM_SNPRINTF(buffer, sizeof(buffer), "%s: [%s]", SCRIPT_TYPE_NAME_HASH, reverse);
        }
        else
        {
            DM_SNPRINTF(buffer, sizeof(buffer), "%s: [%llu (unknown)]", SCRIPT_TYPE_NAME_HASH, hash);
        }
        lua_pushstring(L, buffer);
        return 1;
    }

    static int Script_concat(lua_State *L)
    {
        const char* s = luaL_checkstring(L, 1);
        dmhash_t hash = CheckHash(L, 2);
        size_t size = 64 + strlen(s);
        char* buffer = new char[size];
        const char* reverse = (const char*) dmHashReverse64(hash, 0);
        if (reverse != 0x0)
        {
            DM_SNPRINTF(buffer, size, "%s[%s]", s, reverse);
        }
        else
        {
            DM_SNPRINTF(buffer, size, "%s[%llu (unknown)]", s, hash);
        }
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

        lua_pushcfunction(L, Script_HashToHex);
        lua_setglobal(L, "hash_to_hex");

        lua_pushcfunction(L, Script_HashMD5);
        lua_setglobal(L, "hashmd5");

        lua_newtable(L);
        lua_setglobal(L, SCRIPT_HASH_TABLE);

        lua_pop(L, 1);

        assert(top == lua_gettop(L));
    }

    #undef SCRIPT_TYPE_NAME_HASH
}
