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

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include <dlib/array.h>
#include <dlib/zlib.h>
#include <dlib/math.h>
#include "script.h"

extern "C"
{
#include <lua/lua.h>
#include <lua/lauxlib.h>
}

#include "script_private.h"

namespace dmScript
{
    #define LIB_NAME "zlib"

    /*# Zlib compression API documentation
     *
     * Functions for compression and decompression of string buffers.
     *
     * @document
     * @name Zlib
     * @namespace zlib
     * @language Lua
     */

    static bool Writer(void* context, const void* buffer, uint32_t buffer_size)
    {
        dmArray<uint8_t>* out = (dmArray<uint8_t>*) context;
        if (out->Remaining() < buffer_size) {
            int r = out->Remaining();
            int offset = dmMath::Max((int) buffer_size - r, 32 * 1024);
            out->OffsetCapacity(offset);
        }
        out->PushArray((const uint8_t*) buffer, buffer_size);
        return true;
    }

    /*# Inflate (decompress) a buffer
     * A lua error is raised is on error
     *
     * @name zlib.inflate
     * @param buf [type:string] buffer to inflate
     * @return buf [type:string] inflated buffer
     * @examples
     *
     * ```lua
     * local data = "\120\94\11\201\200\44\86\0\162\68\133\226\146\162\204\188\116\133\242\204\146\12\133\210\188\228\252\220\130\162\212\226\226\212\20\133\148\196\146\68\61\0\44\67\14\201"
     * local uncompressed_data = zlib.inflate(data)
     * print(uncompressed_data) --> This is a string with uncompressed data.
     * ```
     */
    int Zlib_Inflate(lua_State* L)
    {
        dmArray<uint8_t> out;
        out.SetCapacity(32 * 1024);
        const char* in = luaL_checkstring(L, 1);
        int in_len = lua_strlen(L, 1);
        dmZlib::Result r = dmZlib::InflateBuffer(in, in_len, &out, Writer);
        if (r == dmZlib::RESULT_OK)
        {
            lua_pushlstring(L, (const char*) out.Begin(), out.Size());
            return 1;
        }
        else
        {
            out.SetCapacity(0); // Required as the destructor isn't run
            luaL_error(L, "Failed to inflate buffer (%d)", r);
            return 0;
        }

        assert(0); // never reached
        return 0;
    }

    /*# Deflate (compress) a buffer
     * A lua error is raised is on error
     *
     * @name zlib.deflate
     * @param buf [type:string] buffer to deflate
     * @return buf [type:string] deflated buffer
     * @examples
     *
     * ```lua
     * local data = "This is a string with uncompressed data."
     * local compressed_data = zlib.deflate(data)
     * local s = ""
     * for c in string.gmatch(compressed_data, ".") do
     *     s = s .. '\\' .. string.byte(c)
     * end
     * print(s) --> \120\94\11\201\200\44\86\0\162\68\133\226\146\162 ...
     * ```
     */
    int Zlib_Deflate(lua_State* L)
    {
        dmArray<uint8_t> out;
        out.SetCapacity(32 * 1024);
        const char* in = luaL_checkstring(L, 1);
        int in_len = lua_strlen(L, 1);
        dmZlib::Result r = dmZlib::DeflateBuffer(in, in_len, 3, &out, Writer);
        if (r == dmZlib::RESULT_OK)
        {
            lua_pushlstring(L, (const char*) out.Begin(), out.Size());
            return 1;
        }
        else
        {
            luaL_error(L, "Failed to deflate buffer (%d)", r);
            return 0;
        }

        assert(0); // never reached
        return 0;
    }

    static const luaL_reg ScriptZlib_methods[] =
    {
        {"inflate", Zlib_Inflate},
        {"deflate", Zlib_Deflate},
        {0, 0}
    };

    void InitializeZlib(lua_State* L)
    {
        int top = lua_gettop(L);

        lua_pushvalue(L, LUA_GLOBALSINDEX);
        luaL_register(L, LIB_NAME, ScriptZlib_methods);
        lua_pop(L, 2);

        assert(top == lua_gettop(L));
    }
}
