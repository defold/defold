// Copyright 2020-2024 The Defold Foundation
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
     */

    #define SCRIPT_TYPE_NAME_HASH "hash"
    static uint32_t SCRIPT_HASH_TYPE_HASH = 0;

    bool IsHash(lua_State *L, int index)
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
    static int Script_Hash(lua_State* L)
    {
        int top = lua_gettop(L);

        dmhash_t hash;
        if(IsHash(L, 1))
        {
            hash = *(dmhash_t*)lua_touserdata(L, 1);
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
    static int Script_HashToHex(lua_State* L)
    {
        int top = lua_gettop(L);

        dmhash_t hash = dmScript::CheckHash(L, 1);
        char buf[17];
        dmSnPrintf(buf, sizeof(buf), "%016llx", (unsigned long long)hash);
        lua_pushstring(L, buf);

        assert(top + 1 == lua_gettop(L));
        return 1;
    }

    static int Script_HashMD5(lua_State* L)
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

    void PushHash(lua_State* L, dmhash_t hash)
    {
        int top = lua_gettop(L);

        HContext context = dmScript::GetScriptContext(L);

        dmHashTable64<int>* instances = &context->m_HashInstances;
        int* refp = instances->Get(hash);
        if (refp)
        {
            lua_rawgeti(L, LUA_REGISTRYINDEX, context->m_ContextTableRef);
            // [-1] Context table
            lua_rawgeti(L, -1, *refp);
            // [-2] Context table
            // [-1] hash
            lua_remove(L, -2);
            // [-1] hash
        }
        else
        {
            dmhash_t* lua_hash = (dmhash_t*)lua_newuserdata(L, sizeof(dmhash_t));
            *lua_hash = hash;
            luaL_getmetatable(L, SCRIPT_TYPE_NAME_HASH);
            // [-2] hash
            // [-1] meta table
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

            if (instances->Full())
            {
                instances->SetCapacity(instances->Size(), instances->Capacity() + 256);
            }
            instances->Put(hash, ref);
        }

        assert(top + 1 == lua_gettop(L));
    }

    void ReleaseHash(lua_State* L, dmhash_t hash)
    {
        int top = lua_gettop(L);
        HContext context = dmScript::GetScriptContext(L);
        dmHashTable64<int>* instances = &context->m_HashInstances;
        int* refp = instances->Get(hash);
        if (refp != 0x0)
        {
            lua_rawgeti(L, LUA_REGISTRYINDEX, context->m_ContextTableRef);
            // [-1] context table
            luaL_unref(L, -1, *refp);
            lua_pop(L, 1);
            instances->Erase(hash);
        }

        assert(top == lua_gettop(L));
    }

    dmhash_t CheckHash(lua_State* L, int index)
    {
        return *(dmhash_t*)dmScript::CheckUserType(L, index, SCRIPT_HASH_TYPE_HASH, 0);
    }

    dmhash_t CheckHashOrString(lua_State* L, int index)
    {
        dmhash_t* lua_hash = 0x0;
        if (IsHash(L, index))
        {
            lua_hash = (dmhash_t*)lua_touserdata(L, index);
            return *lua_hash;
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
        if (lua_type(L, index) == LUA_TSTRING)
        {
            size_t len = 0;
            const char* s = lua_tolstring(L, index, &len);
            dmStrlCpy(buffer, s, bufferlength);
        }
        else if (IsHash(L, index))
        {
            dmhash_t* hash = (dmhash_t*)lua_touserdata(L, index);

            DM_HASH_REVERSE_MEM(hash_ctx, 128);
            const char* reverse = (const char*) dmHashReverseSafe64Alloc(&hash_ctx, *hash);
            dmStrlCpy(buffer, reverse, bufferlength);
        }
        else
        {
            dmStrlCpy(buffer, "<unknown type>", bufferlength);
        }
        return buffer;
    }

    static int Script_eq(lua_State* L)
    {
        void* userdata_1 = lua_touserdata(L, 1);
        void* userdata_2 = lua_touserdata(L, 2);
        // At least one of the values must be a hash according to
        // Lua spec and we always de-duplicate hashes when created
        // so we can simply do equality check on pointers.
        lua_pushboolean(L, userdata_1 == userdata_2);
        return 1;
    }

    static int Script_tostring(lua_State* L)
    {
        dmhash_t hash = CheckHash(L, 1);
        DM_HASH_REVERSE_MEM(hash_ctx, 64);
        const char* reverse = (const char*) dmHashReverseSafe64Alloc(&hash_ctx, hash);
        char buffer[64];
        dmSnPrintf(buffer, sizeof(buffer), "%s: [%s]", SCRIPT_TYPE_NAME_HASH, reverse);

        lua_pushstring(L, buffer);
        return 1;
    }

    static const char* GetStringHelper(dmAllocator* allocator, lua_State* L, int index)
    {
        if (dmScript::IsHash(L, index))
        {
            dmhash_t hash = *(dmhash_t*)lua_touserdata(L, index);
            return dmHashReverseSafe64Alloc(allocator, hash);
        }
        return luaL_checkstring(L, index);
    }

    static int Script_concat(lua_State *L)
    {
        // string .. hash
        // hash .. string
        // hash .. hash
        DM_HASH_REVERSE_MEM(hash_ctx1, 256);
        const char* s1 = GetStringHelper(&hash_ctx1, L, 1);

        DM_HASH_REVERSE_MEM(hash_ctx2, 256);
        const char* s2 = GetStringHelper(&hash_ctx2, L, 2);

        lua_pushstring(L, s1);
        lua_pushstring(L, s2);
        lua_concat(L, 2);
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

        SCRIPT_HASH_TYPE_HASH = dmScript::SetUserType(L, -1, SCRIPT_TYPE_NAME_HASH);

        luaL_openlib(L, 0x0, ScriptHash_methods, 0);

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

        lua_pop(L, 1);

        assert(top == lua_gettop(L));
    }

    #undef SCRIPT_TYPE_NAME_HASH
}
