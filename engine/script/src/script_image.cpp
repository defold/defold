#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>

#include <dlib/log.h>
#include <dlib/image.h>
#include "script.h"

extern "C"
{
#include <lua/lua.h>
#include <lua/lauxlib.h>
}

#include "script_private.h"

namespace dmScript
{
    #define LIB_NAME "image"

    int Image_Load(lua_State* L)
    {
        int top = lua_gettop(L);
        luaL_checktype(L, 1, LUA_TSTRING);
        size_t buffer_len = 0;
        const char* buffer = lua_tolstring(L, 1, &buffer_len);

        dmImage::Image image;
        dmImage::Result r = dmImage::Load(buffer, buffer_len, &image);
        if (r == dmImage::RESULT_OK) {

            int bytes_per_pixel = 1;
            switch (image.m_Type)
            {
            case dmImage::TYPE_RGB:
                bytes_per_pixel = 3;
                break;
            case dmImage::TYPE_RGBA:
                bytes_per_pixel = 4;
                break;
            case dmImage::TYPE_LUMINANCE:
                bytes_per_pixel = 1;
                break;
            default:
                dmImage::Free(&image);
                luaL_error(L, "unknown image type %d", image.m_Type);
            }

            lua_newtable(L);

            lua_pushliteral(L, "width");
            lua_pushinteger(L, image.m_Width);
            lua_rawset(L, -3);

            lua_pushliteral(L, "height");
            lua_pushinteger(L, image.m_Height);
            lua_rawset(L, -3);

            lua_pushliteral(L, "type");
            lua_pushinteger(L, image.m_Type);
            lua_rawset(L, -3);

            lua_pushliteral(L, "buffer");
            lua_pushlstring(L, (const char*) image.m_Buffer, bytes_per_pixel * image.m_Width * image.m_Height);
            lua_rawset(L, -3);

            dmImage::Free(&image);

        } else {
            dmLogWarning("failed to load image (%d)", r);
            lua_pushnil(L);
        }

        assert(top + 1 == lua_gettop(L));
        return 1;
    }

    static const luaL_reg ScriptImage_methods[] =
    {
        {"load", Image_Load},
        {0, 0}
    };

    void InitializeImage(lua_State* L)
    {
        int top = lua_gettop(L);

        luaL_register(L, LIB_NAME, ScriptImage_methods);

#define SETCONSTANT(name) \
        lua_pushnumber(L, (lua_Number) dmImage::name); \
        lua_setfield(L, -2, #name);\

        SETCONSTANT(TYPE_RGB)
        SETCONSTANT(TYPE_RGBA)
        SETCONSTANT(TYPE_LUMINANCE)

#undef SETCONSTANT


        lua_pop(L, 1);

        assert(top == lua_gettop(L));
    }
}
