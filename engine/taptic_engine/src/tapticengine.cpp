#include <dmsdk/sdk.h>
#define DLIB_LOG_DOMAIN "taptic_engine"

namespace dmTapticEngine
{
    dmExtension::Result AppInitializeTaptic(dmExtension::AppParams* params)
    {
        return dmExtension::RESULT_OK;
    }

    dmExtension::Result AppFinalizeTaptic(dmExtension::AppParams* params)
    {
        return dmExtension::RESULT_OK;
    }

    dmExtension::Result FinalizeTaptic(dmExtension::Params* params)
    {
        return dmExtension::RESULT_OK;
    }

    static int isSupported(lua_State* L)
    {
        printf("taptic_engine -- testing on editor\n");
        bool status = true;
        lua_pushboolean(L, status);
        return 1;
    }

    static int impact(lua_State* L)
    {
        printf("taptic_engine -- impact\n");
        return 0;
    }

    static int notification(lua_State* L)
    {
        printf("taptic_engine -- notification\n");
        return 0;
    }

    static int selection(lua_State* L)
    {
        printf("taptic_engine -- selection\n");
        return 0;
    }

    static const luaL_reg Module_methods[] = {
        { "is_supported", isSupported },
        { "impact", impact },
        { "notification", notification },
        { "selection", selection },
        { 0, 0 }
    };

    dmExtension::Result InitializeTaptic(dmExtension::Params* params)
    {
        dmLogInfo("taptic_engine -- init from editor version\n");
        lua_State* L = params->m_L;
        int top      = lua_gettop(L);
        lua_getglobal(L, "taptic_engine");
        if (lua_isnil(L, -1))
        {
            lua_pop(L, 1);
            luaL_register(L, "taptic_engine", Module_methods);
        }
        lua_pop(L, 1);
        assert(top == lua_gettop(L));
        return dmExtension::RESULT_OK;
    }

    DM_DECLARE_EXTENSION(TapticEngine, "taptic_engine", AppInitializeTaptic, AppFinalizeTaptic, InitializeTaptic, 0, 0, FinalizeTaptic)

} // namespace dmTapticEngine
