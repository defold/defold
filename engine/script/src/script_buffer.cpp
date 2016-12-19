#include "script.h"

#include <assert.h>
#include <stdint.h>
#include <string.h>

#include <dlib/buffer.h>
#include <dlib/dstrings.h>
#include <dlib/log.h>

extern "C"
{
#include <lua/lauxlib.h>
#include <lua/lualib.h>
}

namespace dmScript
{
    /*# Buffer API documentation
     *
     * @name Buffers
     * @namespace buffer
     */

#define SCRIPT_LIB_NAME "buffer"
#define SCRIPT_TYPE_NAME_BUFFER "buffer"

    bool IsBuffer(lua_State *L, int index)
    {
        int top = lua_gettop(L);
        void *p = lua_touserdata(L, index);
        bool result = false;
        if (p != 0x0)
        {  /* value is a userdata? */
            if (lua_getmetatable(L, index))
            {  /* does it have a metatable? */
                lua_getfield(L, LUA_REGISTRYINDEX, SCRIPT_TYPE_NAME_BUFFER);  /* get correct metatable */
                if (lua_rawequal(L, -1, -2))
                {  /* does it have the correct mt? */
                    result = true;
                }
                lua_pop(L, 2);  /* remove both metatables */
            }
        }
        assert(top == lua_gettop(L));
        return result;
    }

    void PushBuffer(lua_State* L, dmBuffer::HBuffer v)
    {
        int top = lua_gettop(L);
        dmBuffer::HBuffer* vp = (dmBuffer::HBuffer*)lua_newuserdata(L, sizeof(dmBuffer::HBuffer));
        *vp = v;
        luaL_getmetatable(L, SCRIPT_TYPE_NAME_BUFFER);
        lua_setmetatable(L, -2);
        assert(top + 1 == lua_gettop(L));
    }

    dmBuffer::HBuffer* CheckBuffer(lua_State* L, int index)
    {
        if (lua_type(L, index) == LUA_TUSERDATA)
        {
            return (dmBuffer::HBuffer*)luaL_checkudata(L, index, SCRIPT_TYPE_NAME_BUFFER);
        }
        luaL_typerror(L, index, SCRIPT_TYPE_NAME_BUFFER);
        return 0x0;
    }

    static int Buffer_gc(lua_State *L)
    {
        dmBuffer::HBuffer* buffer = CheckBuffer(L, 1);
        dmBuffer::Free(*buffer);
        *buffer = 0x0;
        return 0;
    }

    static int Buffer_tostring(lua_State *L)
    {
        int top = lua_gettop(L);
        dmBuffer::HBuffer* buffer = CheckBuffer(L, 1);
        uint32_t out_element_count = 0;
        dmBuffer::Result r = dmBuffer::GetElementCount(*buffer, &out_element_count);
        if (r == dmBuffer::RESULT_OK) {
            lua_pushfstring(L, "buffer.%s(elements=%d)", SCRIPT_TYPE_NAME_BUFFER, out_element_count);
        } else {
            lua_pushfstring(L, "buffer.%s(invalid)", SCRIPT_TYPE_NAME_BUFFER);
        }
        assert(top + 1 == lua_gettop(L));
        return 1;
    }

    static int Buffer_len(lua_State *L)
    {
        int top = lua_gettop(L);
        dmBuffer::HBuffer* buffer = CheckBuffer(L, 1);
        uint32_t out_element_count = 0;
        dmBuffer::Result r = dmBuffer::GetElementCount(*buffer, &out_element_count);
        if (r != dmBuffer::RESULT_OK) {
            assert(top == lua_gettop(L));
            return luaL_error(L, "%s.%s could not get buffer length", SCRIPT_LIB_NAME, SCRIPT_TYPE_NAME_BUFFER);
        }

        lua_pushnumber(L, out_element_count);
        assert(top + 1 == lua_gettop(L));
        return 1;
    }

    static const luaL_reg Buffer_methods[] =
    {
        {0,0}
    };
    static const luaL_reg Buffer_meta[] =
    {
        {"__gc",        Buffer_gc},
        {"__tostring",  Buffer_tostring},
        {"__len",       Buffer_len},
        {0,0}
    };

    static const luaL_reg methods[] =
    {
        {0, 0}
    };

    struct BufferTypeStruct {
        const char* m_Name;
        const luaL_reg* m_Methods;
        const luaL_reg* m_Metatable;
    };

    void InitializeBuffer(lua_State* L)
    {
        int top = lua_gettop(L);

        const uint32_t type_count = 1;
        BufferTypeStruct types[type_count] =
        {
            {SCRIPT_TYPE_NAME_BUFFER, Buffer_methods, Buffer_meta},
        };

        for (uint32_t i = 0; i < type_count; ++i)
        {
            // create methods table, add it to the globals
            luaL_register(L, types[i].m_Name, types[i].m_Methods);
            int methods_index = lua_gettop(L);
            // create metatable for the type, add it to the Lua registry
            luaL_newmetatable(L, types[i].m_Name);
            int metatable = lua_gettop(L);
            // fill metatable
            luaL_register(L, 0, types[i].m_Metatable);

            lua_pushliteral(L, "__metatable");
            lua_pushvalue(L, methods_index);// dup methods table
            lua_settable(L, metatable);

            lua_pop(L, 2);
        }
        luaL_register(L, SCRIPT_LIB_NAME, methods);
        lua_pop(L, 1);

        assert(top == lua_gettop(L));
    }

}
