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
#include <dlib/crypt/crypt.h>
#include "script_private.h"

#include <dmsdk/dlib/hash.h>

extern "C"
{
#include <dmsdk/dlua/dlua.h>
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

    static inline void PushHashWeakTableKey(dlua_State* L, dmhash_t hash)
    {
        // The weak table is indexed by full 64-bit hashes. Lua numbers are doubles
        // and dlua_rawgeti() takes an int, so numeric keys can lose
        // bits and let stale hash userdata erase a live cache entry.
        dlua_pushlstring(L, (const char*)&hash, sizeof(hash));
    }

    bool IsHash(dlua_State *L, int index)
    {
        return dmScript::ToUserType(L, index, SCRIPT_HASH_TYPE_HASH) != 0;
    }

    dmhash_t* ToHash(dlua_State *L, int index)
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
    static int Hash_new(dlua_State* L)
    {
        int top = dlua_gettop(L);

        dmhash_t hash;
        dmhash_t* phash = ToHash(L, 1);
        if (phash != 0)
        {
            hash = *phash;
        }
        else
        {
            const char* str = dluaL_checkstring(L, 1);
            hash = dmHashString64(str);
        }
        PushHash(L, hash);

        assert(top + 1 == dlua_gettop(L));
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
    static int HashToHex(dlua_State* L)
    {
        int top = dlua_gettop(L);

        dmhash_t hash = dmScript::CheckHash(L, 1);
        char buf[17];
        dmSnPrintf(buf, sizeof(buf), "%016llx", (unsigned long long)hash);
        dlua_pushstring(L, buf);

        assert(top + 1 == dlua_gettop(L));
        return 1;
    }

    static int HashMD5(dlua_State* L)
    {
        int top = dlua_gettop(L);

        size_t len;
        const char* str = dluaL_checklstring(L, 1, &len);

        uint8_t d[16];
        dmCrypt::HashMd5((const uint8_t*)str, len, d);

        char md5[16 * 2 + 1]; // We need a terminal zero for snprintf
        dmSnPrintf(md5, sizeof(md5), "%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x",
                    d[0], d[1], d[2], d[3], d[4], d[5], d[6], d[7], d[8], d[9], d[10], d[11], d[12], d[13], d[14], d[15]);
        dlua_pushstring(L, md5);

        assert(top + 1 == dlua_gettop(L));
        return 1;
    }

    static int CreateLuaHashUserdata(dlua_State* L, dmhash_t hash)
    {
        HContext context = dmScript::GetScriptContext(L);
        dmhash_t* lua_hash = (dmhash_t*)dlua_newuserdata(L, sizeof(dmhash_t));
        *lua_hash = hash;
        dluaL_getmetatable(L, SCRIPT_TYPE_NAME_HASH);
        dlua_setmetatable(L, -2);

        // [-1] hash
        dlua_rawgeti(L, DLUA_REGISTRYINDEX, context->m_ContextTableRef);
        // [-2] hash
        // [-1] Context table
        dlua_pushvalue(L, -2);
        // [-3] hash
        // [-2] Context table
        // [-1] hash
        int ref = dluaL_ref(L, -2);
        // [-2] hash
        // [-1] Context table
        dlua_pop(L, 1);
        // [-1] hash

        dmHashTable64<int>* instances = &context->m_HashInstances;
        if (instances->Full())
        {
            instances->SetCapacity(instances->Size(), instances->Capacity() + 256);
        }
        instances->Put(hash, ref);

        // Also store the value in the weak table with the hash as key
        dlua_rawgeti(L, DLUA_REGISTRYINDEX, context->m_ContextWeakTableRef);
        // [-2] userdata
        // [-1] weak_table
        PushHashWeakTableKey(L, hash);
        // [-3] userdata
        // [-2] weak_table
        // [-1] key
        dlua_pushvalue(L, -3);
        // [-4] userdata
        // [-3] weak_table
        // [-2] key
        // [-1] value
        dlua_rawset(L, -3); // weak_table[hash] = userdata
        // [-2] userdata
        // [-1] weak_table
        dlua_pop(L, 1);
        // [-1] userdata

        return dlua_gettop(L); // return stack index of the new userdata
    }

    void PushHash(dlua_State* L, dmhash_t hash)
    {
        int top = dlua_gettop(L);

        HContext context = dmScript::GetScriptContext(L);

        dmHashTable64<int>* instances = &context->m_HashInstances;
        int* refp = instances->Get(hash);

        if (refp)
        {
            dlua_rawgeti(L, DLUA_REGISTRYINDEX, context->m_ContextWeakTableRef);
            // [-1] weak_table
            PushHashWeakTableKey(L, hash);
            // [-2] weak_table
            // [-1] key
            dlua_rawget(L, -2);
            // [-2] weak_table
            // [-1] value or nil
            if (dlua_isnil(L, -1))
            {
                dlua_pop(L, 1); // remove nil
                //[-1] weak_table

                // Create new userdata and re-register it
                dlua_pop(L, 1); // remove weak_table
                CreateLuaHashUserdata(L, hash);
            }
            else
            {
                dlua_remove(L, -2); // remove weak_table
                // [-1] hash
            }
        }
        else
        {
            CreateLuaHashUserdata(L, hash);
        }

        assert(top + 1 == dlua_gettop(L));
    }

    void ReleaseHash(dlua_State* L, dmhash_t hash)
    {
        int top = dlua_gettop(L);

        HContext context = dmScript::GetScriptContext(L);
        dmHashTable64<int>* instances = &context->m_HashInstances;
        int* refp = instances->Get(hash);
        if (!refp)
        {
            assert(top == dlua_gettop(L));
            return;
        }

        int ref = *refp;
        // Retrieve the value from the strong table
        dlua_rawgeti(L, DLUA_REGISTRYINDEX, context->m_ContextTableRef);
        // [-1] context_table
        dlua_rawgeti(L, -1, ref);
        // [-2] context_table
        // [-1] value

        if (!dlua_isuserdata(L, -1))
        {
            dlua_pop(L, 2);
            return;
        }

        dlua_remove(L, -2);
        // [-1] value

        // Remove value from strong table
        dlua_rawgeti(L, DLUA_REGISTRYINDEX, context->m_ContextTableRef);
        // [-2] value
        // [-1] context_table
        dluaL_unref(L, -1, ref);
        dlua_pop(L, 2);
        // stack balanced

        assert(top == dlua_gettop(L));
    }

    dmhash_t CheckHash(dlua_State* L, int index)
    {
        return *(dmhash_t*)dmScript::CheckUserType(L, index, SCRIPT_HASH_TYPE_HASH, 0);
    }

    dmhash_t CheckHashOrString(dlua_State* L, int index)
    {
        dmhash_t* phash = ToHash(L, index);
        if (phash != 0)
        {
            return *phash;
        }
        else if( dlua_type(L, index) == DLUA_TSTRING )
        {
            size_t len = 0;
            const char* s = dlua_tolstring(L, index, &len);
            return dmHashBuffer64(s, len);
        }

        dluaL_typerror(L, index, "hash or string expected");
        return 0;
    }

    const char* GetStringFromHashOrString(dlua_State* L, int index, char* buffer, uint32_t bufferlength)
    {
        dmhash_t* phash = ToHash(L, index);
        if (phash != 0)
        {
            DM_HASH_REVERSE_MEM(hash_ctx, 128);
            const char* reverse = (const char*) dmHashReverseSafe64Alloc(&hash_ctx, *phash);
            dmStrlCpy(buffer, reverse, bufferlength);
        }
        else if (dlua_type(L, index) == DLUA_TSTRING)
        {
            size_t len = 0;
            const char* s = dlua_tolstring(L, index, &len);
            dmStrlCpy(buffer, s, bufferlength);
        }
        else
        {
            dmStrlCpy(buffer, "<unknown type>", bufferlength);
        }
        return buffer;
    }

    static int Hash_eq(dlua_State* L)
    {
        void* userdata_1 = dlua_touserdata(L, 1);
        void* userdata_2 = dlua_touserdata(L, 2);
        // At least one of the values must be a hash according to
        // Lua spec and we always de-duplicate hashes when created
        // so we can simply do equality check on pointers.
        dlua_pushboolean(L, userdata_1 == userdata_2);
        return 1;
    }

    static int Hash_lt(dlua_State* L)
    {
        dmhash_t hash_1 = CheckHash(L, 1);
        dmhash_t hash_2 = CheckHash(L, 2);
        dlua_pushboolean(L, hash_1 < hash_2);
        return 1;
    }

    static int Hash_tostring(dlua_State* L)
    {
        dmhash_t hash = CheckHash(L, 1);
        DM_HASH_REVERSE_MEM(hash_ctx, 64);
        const char* reverse = (const char*) dmHashReverseSafe64Alloc(&hash_ctx, hash);
        char buffer[256];
        dmSnPrintf(buffer, sizeof(buffer), "%s: [%s]", SCRIPT_TYPE_NAME_HASH, reverse);

        dlua_pushstring(L, buffer);
        return 1;
    }

    static void PushStringHelper(dlua_State* L, int index)
    {
        dmhash_t* phash = ToHash(L, index);
        if (phash != 0)
        {
            char buffer[256];
            DM_HASH_REVERSE_MEM(hash_ctx, 256);
            dmSnPrintf(buffer, sizeof(buffer), "[%s]", dmHashReverseSafe64Alloc(&hash_ctx, *phash));
            dlua_pushstring(L, buffer);
        }
        else
        {
            dlua_pushstring(L, dluaL_checkstring(L, index));
        }
    }

    static int Hash_concat(dlua_State *L)
    {
        // string .. hash
        // hash .. string
        // hash .. hash
        PushStringHelper(L, 1);
        PushStringHelper(L, 2);

        dlua_concat(L, 2);
        return 1;
    }

    static const dluaL_reg ScriptHash_methods[] =
    {
        {SCRIPT_TYPE_NAME_HASH, Hash_new},
        {0, 0}
    };

    static int Hash_gc(dlua_State* L)
    {
        dmhash_t hash = *(dmhash_t*)dlua_touserdata(L, 1);
        HContext context = dmScript::GetScriptContext(L);
        if (context)
        {
            int* refp = context->m_HashInstances.Get(hash);
            if (refp && context->m_ContextWeakTableRef != DLUA_NOREF)
            {
                dlua_rawgeti(L, DLUA_REGISTRYINDEX, context->m_ContextWeakTableRef);
                // [-1] weak_table
                PushHashWeakTableKey(L, hash);
                // [-2] weak_table
                // [-1] key
                dlua_rawget(L, -2);
                // [-2] weak_table
                // [-1] value or nil
                if (dlua_isnil(L, -1))
                {
                    context->m_HashInstances.Erase(hash);
                    dmHashReverseErase64(hash);
                }
                dlua_pop(L, 2);
            }
        }
        return 0;
    }

    void InitializeHash(dlua_State* L)
    {
        int top = dlua_gettop(L);
        dluaL_newmetatable(L, SCRIPT_TYPE_NAME_HASH);

        SCRIPT_HASH_TYPE_HASH = dmScript::SetUserType(L, -1, SCRIPT_TYPE_NAME_HASH);

        dluaL_openlib(L, 0x0, ScriptHash_methods, 0);

        dlua_pushliteral(L, "__eq");
        dlua_pushcfunction(L, Hash_eq);
        dlua_settable(L, -3);

        dlua_pushliteral(L, "__tostring");
        dlua_pushcfunction(L, Hash_tostring);
        dlua_settable(L, -3);

        dlua_pushliteral(L, "__concat");
        dlua_pushcfunction(L, Hash_concat);
        dlua_settable(L, -3);

        dlua_pushliteral(L, "__lt");
        dlua_pushcfunction(L, Hash_lt);
        dlua_settable(L, -3);

        dlua_pushliteral(L, "__gc");
        dlua_pushcfunction(L, Hash_gc);
        dlua_settable(L, -3);

        dlua_pushcfunction(L, Hash_new);
        dlua_setglobal(L, SCRIPT_TYPE_NAME_HASH);

        dlua_pushcfunction(L, HashToHex);
        dlua_setglobal(L, "hash_to_hex");

        dlua_pushcfunction(L, HashMD5);
        dlua_setglobal(L, "hashmd5");

        dlua_pop(L, 1);

        assert(top == dlua_gettop(L));
    }

    #undef SCRIPT_TYPE_NAME_HASH
}
