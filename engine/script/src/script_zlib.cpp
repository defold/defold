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
     * @name Zlib
     * @namespace zlib
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
     * @param buf buffer to inflate (string)
     * @return inflated buffer (string)
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
     * @param buf buffer to deflate (string)
     * @return deflated buffer (string)
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
