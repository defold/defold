#include <float.h>
#include <stdio.h>
#include <assert.h>

#include <dlib/hash.h>
#include <dlib/log.h>
#include <dlib/math.h>

#include "gamesys.h"
#include "gamesys_ddf.h"
#include "../gamesys_private.h"

#include "components/comp_collision_object.h"

#include "script_gfx.h"

extern "C"
{
#include <lua/lauxlib.h>
#include <lua/lualib.h>
}

namespace dmGameSystem
{
    struct GfxScriptContext
    {
    	dmResource::HFactory m_Factory;
    };


#define GFX_CONTEXT_NAME "__gfx_context"

    int Gfx_SetTexture(lua_State* L)
    {
        int top = lua_gettop(L);

        const char* texture_name = luaL_checkstring(L, 1);
        int width = luaL_checkinteger(L, 2);
        int height = luaL_checkinteger(L, 3);
        size_t buffer_size;
        luaL_checktype(L, 4, LUA_TSTRING);
        const char* buffer = lua_tolstring(L, 4, &buffer_size);

        //memset((void*) buffer, 0xff, 256 * 4 * 10);

        lua_getglobal(L, GFX_CONTEXT_NAME);
        GfxScriptContext* context = (GfxScriptContext*)lua_touserdata(L, -1);
        lua_pop(L, 1);

        dmGraphics::HTexture texture;
        dmResource::Result r = dmResource::Get(context->m_Factory, texture_name, (void**) &texture);
        if (r != dmResource::RESULT_OK) {
        	luaL_error(L, "Failed to find texture: %s", texture_name);
        }

        dmGraphics::TextureParams tex_params;

        tex_params.m_Format = dmGraphics::TEXTURE_FORMAT_RGBA;
        tex_params.m_Data = buffer;
        tex_params.m_DataSize = buffer_size;
        tex_params.m_Width = width;
        tex_params.m_Height = height;
        tex_params.m_MinFilter = dmGraphics::TEXTURE_FILTER_LINEAR;
        tex_params.m_MagFilter = dmGraphics::TEXTURE_FILTER_LINEAR;

        dmGraphics::SetTexture(texture, tex_params);

        dmResource::Release(context->m_Factory, (void*) texture);

        assert(top == lua_gettop(L));
        return 0;
    }

    static const luaL_reg GFX_FUNCTIONS[] =
    {
        {"set_texture", Gfx_SetTexture},
        {0, 0}
    };

    void ScriptGfxRegister(const ScriptLibContext& context)
    {
        lua_State* L = context.m_LuaState;
        luaL_register(L, "gfx", GFX_FUNCTIONS);
        lua_pop(L, 1);

        bool result = true;

        GfxScriptContext* gfx_context = new GfxScriptContext();
        gfx_context->m_Factory = context.m_Factory;
        lua_pushlightuserdata(L, gfx_context);
        lua_setglobal(L, GFX_CONTEXT_NAME);
    }

    void ScriptGfxFinalize(const ScriptLibContext& context)
    {
        lua_State* L = context.m_LuaState;
        if (L != 0x0)
        {
            int top = lua_gettop(L);
            (void)top;

            lua_getglobal(L, GFX_CONTEXT_NAME);
            GfxScriptContext* gfx_context = (GfxScriptContext*)lua_touserdata(L, -1);
            lua_pop(L, 1);
            if (gfx_context != 0x0)
            {
                delete gfx_context;
            }

            assert(top == lua_gettop(L));
        }
    }

}

#undef GFX_CONTEXT_NAME
#undef COLLISION_OBJECT_EXT
