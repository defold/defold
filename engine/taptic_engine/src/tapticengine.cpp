
#define DLIB_LOG_DOMAIN "taptic_engine"
#include <dmsdk/sdk.h>

namespace dmTapticEngine
{


/*# TapticEngine
 * @namespace taptic_engine
 *
 */

/*# Collision object physics API documentation
     *
     * Functions and messages for collision object physics interaction
     * with other objects (collisions and ray-casting) and control of
     * physical behaviors.
     *
     * @document
     * @name Collision object
     * @namespace physics
     */

/*# Physics.COPY_POSITION_X
     *
     * @name physics.COPY_POSITION_X
     * @variable
     */

#if defined(DM_PLATFORM_IOS)
#include "tapticengine.h"

static int isSupported(lua_State* L) {
    bool status = TapticEngine_IsSupported();
    lua_pushboolean(L, status);
    return 1;
}

static int impact(lua_State* L) {
    ImpactStyle style = (ImpactStyle) luaL_checkint(L, 1);
    TapticEngine_Impact(style);
    return 0;
}

static int notification(lua_State* L) {
    NotificationType type = (NotificationType) luaL_checkint(L, 1);
    TapticEngine_Notification(type);
    return 0;
}

static int selection(lua_State* L) {
    TapticEngine_Selection();
    return 0;
}

static const luaL_reg Module_methods[] =
{
    {"isSupported", isSupported},
    {"impact", impact},
    {"notification", notification},
    {"selection", selection},
    {0, 0}
};

static void LuaInit(lua_State* L)
{
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

dmExtension::Result AppInitializeTaptic(dmExtension::Params* params)
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

#else // unsupported platforms

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
    printf("init taptic\n");
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

#endif

DM_DECLARE_EXTENSION(TapticEngine, "taptic_engine", AppInitializeTaptic, AppFinalizeTaptic, InitializeTaptic, 0, 0, FinalizeTaptic)

} // namespace
