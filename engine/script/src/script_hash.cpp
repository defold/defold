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

#include "script.h"

#include <assert.h>
#include <string.h>

#include <dlib/dstrings.h>
#include <dlib/math.h>
#include <dlib/crypt.h>
#include "script_private.h"

#include <dmsdk/dlib/hash.h>

extern "C"
{
#include <lua/lauxlib.h>
#include <lua/lualib.h>
}

namespace dmScript
{
    /*# Built-ins API documentation
     *
     * Built-in scripting functions.
     *
     * @document
     * @name Built-ins
     * @namespace builtins
     * @language Lua
     */

    #define SCRIPT_TYPE_NAME_HASH "hash"
    static uint32_t SCRIPT_HASH_TYPE_HASH = 0;

    bool IsHash(lua_State *L, int index)
    {
        return dmScript::ToUserType(L, index, SCRIPT_HASH_TYPE_HASH) != 0;
    }

    dmhash_t* ToHash(lua_State *L, int index)
    {
        return (dmhash_t*)dmScript::ToUserType(L, index, SCRIPT_HASH_TYPE_HASH);
    }

    /*# hashes a string
     * All ids in the engine are represented as hashes, so a string needs to be hashed
     * before it can be compared with an id.
     *
     * @name hash
     * @param s [type:string] string to hash
     * @return hash [type:hash] a hashed string
     * @examples
     *
     * To compare a message_id in an on-message callback function:
     *
     * ```lua
     * function on_message(self, message_id, message, sender)
     *     if message_id == hash("my_message") then
     *         -- Act on the message here
     *     end
     * end
     * ```
     */
    static int Hash_new(lua_State* L)
    {
        int top = lua_gettop(L);

        dmhash_t hash;
        dmhash_t* phash = ToHash(L, 1);
        if (phash != 0)
        {
            hash = *phash;
        }
        else
        {
            const char* str = luaL_checkstring(L, 1);
            hash = dmHashString64(str);
        }
        PushHash(L, hash);

        assert(top + 1 == lua_gettop(L));
        return 1;
    }

    /*# get hex representation of a hash value as a string
     * Returns a hexadecimal representation of a hash value.
     * The returned string is always padded with leading zeros.
     *
     * @name hash_to_hex
     * @param h [type:hash] hash value to get hex string for
     * @return hex [type:string] hex representation of the hash
     * @examples
     *
     * ```lua
     * local h = hash("my_hash")
     * local hexstr = hash_to_hex(h)
     * print(hexstr) --> a2bc06d97f580aab
     * ```
     */
    static int HashToHex(lua_State* L)
    {
        int top = lua_gettop(L);

        dmhash_t hash = dmScript::CheckHash(L, 1);
        char buf[17];
        dmSnPrintf(buf, sizeof(buf), "%016llx", (unsigned long long)hash);
        lua_pushstring(L, buf);

        assert(top + 1 == lua_gettop(L));
        return 1;
    }

    static int HashMD5(lua_State* L)
    {
        int top = lua_gettop(L);

        size_t len;
        const char* str = luaL_checklstring(L, 1, &len);

        uint8_t d[16];
        dmCrypt::HashMd5((const uint8_t*)str, len, d);

        char md5[16 * 2 + 1]; // We need a terminal zero for snprintf
        dmSnPrintf(md5, sizeof(md5), "%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x",
                    d[0], d[1], d[2], d[3], d[4], d[5], d[6], d[7], d[8], d[9], d[10], d[11], d[12], d[13], d[14], d[15]);
        lua_pushstring(L, md5);

        assert(top + 1 == lua_gettop(L));
        return 1;
    }

    static int CreateLuaHashUserdata(lua_State* L, dmhash_t hash)
    {
        HContext context = dmScript::GetScriptContext(L);
        dmhash_t* lua_hash = (dmhash_t*)lua_newuserdata(L, sizeof(dmhash_t));
        *lua_hash = hash;
        luaL_getmetatable(L, SCRIPT_TYPE_NAME_HASH);
        lua_setmetatable(L, -2);

        // [-1] hash
        lua_rawgeti(L, LUA_REGISTRYINDEX, context->m_ContextTableRef);
        // [-2] hash
        // [-1] Context table
        lua_pushvalue(L, -2);
        // [-3] hash
        // [-2] Context table
        // [-1] hash
        int ref = luaL_ref(L, -2);
        // [-2] hash
        // [-1] Context table
        lua_pop(L, 1);
        // [-1] hash

        dmHashTable64<int>* instances = &context->m_HashInstances;
        if (instances->Full())
        {
            instances->SetCapacity(instances->Size(), instances->Capacity() + 256);
        }
        instances->Put(hash, ref);

        // Also store the value in the weak table with the hash as key
        lua_rawgeti(L, LUA_REGISTRYINDEX, context->m_ContextWeakTableRef);
        // [-2] userdata
        // [-1] weak_table
        lua_pushinteger(L, (lua_Integer)hash);
        // [-3] userdata
        // [-2] weak_table
        // [-1] key
        lua_pushvalue(L, -3);
        // [-4] userdata
        // [-3] weak_table
        // [-2] key
        // [-1] value
        lua_settable(L, -3); // weak_table[hash] = userdata
        // [-2] userdata
        // [-1] weak_table
        lua_pop(L, 1);
        // [-1] userdata

        return lua_gettop(L); // return stack index of the new userdata
    }

    void PushHash(lua_State* L, dmhash_t hash)
    {
        int top = lua_gettop(L);

        HContext context = dmScript::GetScriptContext(L);

        dmHashTable64<int>* instances = &context->m_HashInstances;
        int* refp = instances->Get(hash);

        if (refp)
        {
            lua_rawgeti(L, LUA_REGISTRYINDEX, context->m_ContextWeakTableRef);
            // [-1] weak_table
            lua_pushinteger(L, (lua_Integer)hash);
            // [-2] weak_table
            // [-1] key
            lua_gettable(L, -2);
            // [-2] weak_table
            // [-1] value or nil
            if (lua_isnil(L, -1))
            {
                lua_pop(L, 1); // remove nil
                //[-1] weak_table

                // Create new userdata and re-register it
                lua_pop(L, 1); // remove weak_table
                CreateLuaHashUserdata(L, hash);
            }
            else
            {
                lua_remove(L, -2); // remove weak_table
                // [-1] hash
            }
        }
        else
        {
            CreateLuaHashUserdata(L, hash);
        }

        assert(top + 1 == lua_gettop(L));
    }

    void ReleaseHash(lua_State* L, dmhash_t hash)
    {
        int top = lua_gettop(L);

        HContext context = dmScript::GetScriptContext(L);
        dmHashTable64<int>* instances = &context->m_HashInstances;
        int* refp = instances->Get(hash);
        if (!refp)
        {
            assert(top == lua_gettop(L));
            return;
        }

        int ref = *refp;
        // Retrieve the value from the strong table
        lua_rawgeti(L, LUA_REGISTRYINDEX, context->m_ContextTableRef);
        // [-1] context_table
        lua_rawgeti(L, -1, ref);
        // [-2] context_table
        // [-1] value

        if (!lua_isuserdata(L, -1))
        {
            lua_pop(L, 2);
            return;
        }

        lua_remove(L, -2);
        // [-1] value

        // Remove value from strong table
        lua_rawgeti(L, LUA_REGISTRYINDEX, context->m_ContextTableRef);
        // [-2] value
        // [-1] context_table
        luaL_unref(L, -1, ref);
        lua_pop(L, 2);
        // stack balanced

        assert(top == lua_gettop(L));
    }

    dmhash_t CheckHash(lua_State* L, int index)
    {
        return *(dmhash_t*)dmScript::CheckUserType(L, index, SCRIPT_HASH_TYPE_HASH, 0);
    }

    dmhash_t CheckHashOrString(lua_State* L, int index)
    {
        dmhash_t* phash = ToHash(L, index);
        if (phash != 0)
        {
            return *phash;
        }
        else if( lua_type(L, index) == LUA_TSTRING )
        {
            size_t len = 0;
            const char* s = lua_tolstring(L, index, &len);
            return dmHashBuffer64(s, len);
        }

        luaL_typerror(L, index, "hash or string expected");
        return 0;
    }

    const char* GetStringFromHashOrString(lua_State* L, int index, char* buffer, uint32_t bufferlength)
    {
        dmhash_t* phash = ToHash(L, index);
        if (phash != 0)
        {
            DM_HASH_REVERSE_MEM(hash_ctx, 128);
            const char* reverse = (const char*) dmHashReverseSafe64Alloc(&hash_ctx, *phash);
            dmStrlCpy(buffer, reverse, bufferlength);
        }
        else if (lua_type(L, index) == LUA_TSTRING)
        {
            size_t len = 0;
            const char* s = lua_tolstring(L, index, &len);
            dmStrlCpy(buffer, s, bufferlength);
        }
        else
        {
            dmStrlCpy(buffer, "<unknown type>", bufferlength);
        }
        return buffer;
    }

    static int Hash_eq(lua_State* L)
    {
        void* userdata_1 = lua_touserdata(L, 1);
        void* userdata_2 = lua_touserdata(L, 2);
        // At least one of the values must be a hash according to
        // Lua spec and we always de-duplicate hashes when created
        // so we can simply do equality check on pointers.
        lua_pushboolean(L, userdata_1 == userdata_2);
        return 1;
    }

    static int Hash_lt(lua_State* L)
    {
        dmhash_t hash_1 = CheckHash(L, 1);
        dmhash_t hash_2 = CheckHash(L, 2);
        lua_pushboolean(L, hash_1 < hash_2);
        return 1;
    }

    static int Hash_tostring(lua_State* L)
    {
        dmhash_t hash = CheckHash(L, 1);
        DM_HASH_REVERSE_MEM(hash_ctx, 64);
        const char* reverse = (const char*) dmHashReverseSafe64Alloc(&hash_ctx, hash);
        char buffer[256];
        dmSnPrintf(buffer, sizeof(buffer), "%s: [%s]", SCRIPT_TYPE_NAME_HASH, reverse);

        lua_pushstring(L, buffer);
        return 1;
    }

    static void PushStringHelper(lua_State* L, int index)
    {
        dmhash_t* phash = ToHash(L, index);
        if (phash != 0)
        {
            char buffer[256];
            DM_HASH_REVERSE_MEM(hash_ctx, 256);
            dmSnPrintf(buffer, sizeof(buffer), "[%s]", dmHashReverseSafe64Alloc(&hash_ctx, *phash));
            lua_pushstring(L, buffer);
        }
        else
        {
            lua_pushstring(L, luaL_checkstring(L, index));
        }
    }

    static int Hash_concat(lua_State *L)
    {
        // string .. hash
        // hash .. string
        // hash .. hash
        PushStringHelper(L, 1);
        PushStringHelper(L, 2);

        lua_concat(L, 2);
        return 1;
    }

    static const luaL_reg ScriptHash_methods[] =
    {
        {SCRIPT_TYPE_NAME_HASH, Hash_new},
        {0, 0}
    };

    static int Hash_gc(lua_State* L)
    {
        dmhash_t hash = *(dmhash_t*)lua_touserdata(L, 1);
        HContext context = dmScript::GetScriptContext(L);
        if (context)
        {
            int* refp = context->m_HashInstances.Get(hash);
            if (refp && context->m_ContextWeakTableRef != LUA_NOREF)
            {
                lua_rawgeti(L, LUA_REGISTRYINDEX, context->m_ContextWeakTableRef);
                // [-1] weak_table
                lua_rawgeti(L, -1, (lua_Integer)hash);
                // [-2] weak_table
                // [-1] hash
                if (lua_isnil(L, -1))
                {
                    context->m_HashInstances.Erase(hash);
                    dmHashReverseErase64(hash);
                }
                lua_pop(L, 2);
            }
        }
        return 0;
    }

    void InitializeHash(lua_State* L)
    {
        int top = lua_gettop(L);
        luaL_newmetatable(L, SCRIPT_TYPE_NAME_HASH);

        SCRIPT_HASH_TYPE_HASH = dmScript::SetUserType(L, -1, SCRIPT_TYPE_NAME_HASH);

        luaL_openlib(L, 0x0, ScriptHash_methods, 0);

        lua_pushliteral(L, "__eq");
        lua_pushcfunction(L, Hash_eq);
        lua_settable(L, -3);

        lua_pushliteral(L, "__tostring");
        lua_pushcfunction(L, Hash_tostring);
        lua_settable(L, -3);

        lua_pushliteral(L, "__concat");
        lua_pushcfunction(L, Hash_concat);
        lua_settable(L, -3);

        lua_pushliteral(L, "__lt");
        lua_pushcfunction(L, Hash_lt);
        lua_settable(L, -3);

        lua_pushliteral(L, "__gc");
        lua_pushcfunction(L, Hash_gc);
        lua_settable(L, -3);

        lua_pushcfunction(L, Hash_new);
        lua_setglobal(L, SCRIPT_TYPE_NAME_HASH);

        lua_pushcfunction(L, HashToHex);
        lua_setglobal(L, "hash_to_hex");

        lua_pushcfunction(L, HashMD5);
        lua_setglobal(L, "hashmd5");

        lua_pop(L, 1);

        assert(top == lua_gettop(L));
    }

    #undef SCRIPT_TYPE_NAME_HASH
}