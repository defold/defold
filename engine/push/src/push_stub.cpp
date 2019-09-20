#include <dmsdk/extension/extension.h>
#include <assert.h>

namespace dmPushStub
{

static int Push_ThrowError(lua_State* L)
{
    return luaL_error(L, "push has been removed from core, please read /builtins/docs/push.md for more information.");
}

static const luaL_reg Push_methods[] =
{
    {"register", Push_ThrowError},
    {"set_listener", Push_ThrowError},
    {"set_badge_count", Push_ThrowError},
    {"schedule", Push_ThrowError},
    {"cancel", Push_ThrowError},
    {"get_scheduled", Push_ThrowError},
    {"get_all_scheduled", Push_ThrowError},
    {0, 0}
};

static void LuaInit(lua_State* L)
{
    int top = lua_gettop(L);
    lua_getglobal(L, "push");
    if (lua_isnil(L, -1)) {
        lua_pop(L, 1);
        luaL_register(L, "push", Push_methods);
    }
    lua_pop(L, 1);
    assert(top == lua_gettop(L));
}

static dmExtension::Result Push_AppInitialize(dmExtension::AppParams* params)
{
    return dmExtension::RESULT_OK;
}

static dmExtension::Result Push_AppFinalize(dmExtension::AppParams* params)
{
    return dmExtension::RESULT_OK;
}

static dmExtension::Result Push_Initialize(dmExtension::Params* params)
{
    LuaInit(params->m_L);
    return dmExtension::RESULT_OK;
}

DM_DECLARE_EXTENSION(PushExt, "Push", Push_AppInitialize, Push_AppFinalize, Push_Initialize, 0, 0, 0)

} // namespace
