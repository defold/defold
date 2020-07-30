#include <dmsdk/sdk.h>
#include "tapticengine.h"
#define DLIB_LOG_DOMAIN "taptic_engine"

namespace dmTapticEngine
{
    static int isSupported(lua_State* L)
    {
        bool status = TapticEngine_IsSupported();
        // Log
        printf("taptic_engine -- is_supported : (%s)", status ? "true" : "false");
        lua_pushboolean(L, status);
        return 1;
    }

    static int impact(lua_State* L)
    {
        ImpactStyle style = (ImpactStyle)luaL_checkint(L, 1);
        TapticEngine_Impact(style);
        // Log
        printf("taptic_engine -- impact()");
        return 0;
    }

    static int notification(lua_State* L)
    {
        NotificationType type = (NotificationType)luaL_checkint(L, 1);
        TapticEngine_Notification(type);
        // Log
        printf("taptic_engine -- notification()");
        return 0;
    }

    static int selection(lua_State* L)
    {
        TapticEngine_Selection();
        // Log
        printf("taptic_engine -- selection()");
        return 0;
    }

    static const luaL_reg Module_methods[] = {
        { "isSupported", isSupported },
        { "impact", impact },
        { "notification", notification },
        { "selection", selection },
        { 0, 0 }
    };

    static void LuaInit(lua_State* L)
    {
        dmLogInfo("taptic_engine -- init from native iOS\n");
        int top = lua_gettop(L);
        luaL_register(L, "taptic_engine", Module_methods);

#define SETCONSTANT(name) \
        lua_pushnumber(L, (lua_Number) name); \
        lua_setfield(L, -2, #name);\

    SETCONSTANT(IMPACT_LIGHT)
    SETCONSTANT(IMPACT_MEDIUM)
    SETCONSTANT(IMPACT_HEAVY)

    SETCONSTANT(NOTIFICATION_SUCCESS)
    SETCONSTANT(NOTIFICATION_WARNING)
    SETCONSTANT(NOTIFICATION_ERROR)

#undef SETCONSTANT

    lua_pop(L, 1);
    assert(top == lua_gettop(L));
}

dmExtension::Result AppInitializeTaptic(dmExtension::AppParams* params)
{
    return dmExtension::RESULT_OK;
}

dmExtension::Result InitializeTaptic(dmExtension::Params* params)
{
    LuaInit(params->m_L);
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

DM_DECLARE_EXTENSION(TapticEngine, "taptic_engine", AppInitializeTaptic, AppFinalizeTaptic, InitializeTaptic, 0, 0, FinalizeTaptic)

} // namespace dmTapticEngine
