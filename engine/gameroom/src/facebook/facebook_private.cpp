
#include "facebook_private.h"
#include "facebook_analytics.h"
#include <dlib/log.h>
#include <dlib/dstrings.h>

namespace dmFacebookGR {

static const luaL_reg Facebook_methods[] =
{
    {"login", Facebook_Login},
    {"logout", Facebook_Logout},
    {"access_token", Facebook_AccessToken},
    {"permissions", Facebook_Permissions},
    {"request_read_permissions", Facebook_RequestReadPermissions},
    {"request_publish_permissions", Facebook_RequestPublishPermissions},
    {"me", Facebook_Me},
    {"post_event", Facebook_PostEvent},
    {"enable_event_usage", Facebook_EnableEventUsage},
    {"disable_event_usage", Facebook_DisableEventUsage},
    {"show_dialog", Facebook_ShowDialog},
    {"login_with_read_permissions", Facebook_LoginWithReadPermissions},
    {"login_with_publish_permissions", Facebook_LoginWithPublishPermissions},
    {0, 0}
};

int Facebook_LoginWithPublishPermissions(lua_State* L)
{
    if (!PlatformFacebookInitialized())
    {
        return luaL_error(L, "Facebook module has not been initialized, is facebook.appid set in game.project?");
    }

    DM_LUA_STACK_CHECK(L, 0);
    luaL_checktype(L, 1, LUA_TTABLE);
    luaL_checktype(L, 2, LUA_TNUMBER);
    luaL_checktype(L, 3, LUA_TFUNCTION);

    char* permissions[128];
    int permission_count = luaTableToCArray(L, 1, permissions,
        sizeof(permissions) / sizeof(permissions[0]));
    if (permission_count == -1)
    {
        return luaL_error(L, "Facebook permissions must be strings");
    }

    int audience = luaL_checkinteger(L, 2);

    lua_pushvalue(L, 3);
    int callback = dmScript::Ref(L, LUA_REGISTRYINDEX);

    dmScript::GetInstance(L);
    int context = dmScript::Ref(L, LUA_REGISTRYINDEX);

    lua_State* thread = dmScript::GetMainThread(L);

    PlatformFacebookLoginWithPublishPermissions(
        L, (const char**) permissions, permission_count, audience, callback, context, thread);

    for (int i = 0; i < permission_count; ++i)
    {
        free(permissions[i]);
    }

    return 0;
}

int Facebook_LoginWithReadPermissions(lua_State* L)
{
    if (!PlatformFacebookInitialized())
    {
        return luaL_error(L, "Facebook module has not been initialized, is facebook.appid set in game.project?");
    }

    DM_LUA_STACK_CHECK(L, 0);
    luaL_checktype(L, 1, LUA_TTABLE);
    luaL_checktype(L, 2, LUA_TFUNCTION);

    char* permissions[128];
    int permission_count = luaTableToCArray(L, 1, permissions,
        sizeof(permissions) / sizeof(permissions[0]));
    if (permission_count == -1)
    {
        return luaL_error(L, "Facebook permissions must be strings");
    }

    lua_pushvalue(L, 2);
    int callback = dmScript::Ref(L, LUA_REGISTRYINDEX);

    dmScript::GetInstance(L);
    int context = dmScript::Ref(L, LUA_REGISTRYINDEX);

    lua_State* thread = dmScript::GetMainThread(L);

    PlatformFacebookLoginWithReadPermissions(
        L, (const char**) permissions, permission_count, callback, context, thread);

    for (int i = 0; i < permission_count; ++i)
    {
        free(permissions[i]);
    }

    return 0;
}

void LuaInit(lua_State* L)
{
    int top = lua_gettop(L);
    luaL_register(L, LIB_NAME, Facebook_methods);

    #define SETCONSTANT(name, val) \
        lua_pushnumber(L, (lua_Number) val); \
        lua_setfield(L, -2, #name);\

    SETCONSTANT(STATE_CREATED,              dmFacebookGR::STATE_CREATED);
    SETCONSTANT(STATE_CREATED_TOKEN_LOADED, dmFacebookGR::STATE_CREATED_TOKEN_LOADED);
    SETCONSTANT(STATE_CREATED_OPENING,      dmFacebookGR::STATE_CREATED_OPENING);
    SETCONSTANT(STATE_OPEN,                 dmFacebookGR::STATE_OPEN);
    SETCONSTANT(STATE_OPEN_TOKEN_EXTENDED,  dmFacebookGR::STATE_OPEN_TOKEN_EXTENDED);
    SETCONSTANT(STATE_CLOSED,               dmFacebookGR::STATE_CLOSED);
    SETCONSTANT(STATE_CLOSED_LOGIN_FAILED,  dmFacebookGR::STATE_CLOSED_LOGIN_FAILED);

    SETCONSTANT(GAMEREQUEST_ACTIONTYPE_NONE,   dmFacebookGR::GAMEREQUEST_ACTIONTYPE_NONE);
    SETCONSTANT(GAMEREQUEST_ACTIONTYPE_SEND,   dmFacebookGR::GAMEREQUEST_ACTIONTYPE_SEND);
    SETCONSTANT(GAMEREQUEST_ACTIONTYPE_ASKFOR, dmFacebookGR::GAMEREQUEST_ACTIONTYPE_ASKFOR);
    SETCONSTANT(GAMEREQUEST_ACTIONTYPE_TURN,   dmFacebookGR::GAMEREQUEST_ACTIONTYPE_TURN);

    SETCONSTANT(GAMEREQUEST_FILTER_NONE,        dmFacebookGR::GAMEREQUEST_FILTER_NONE);
    SETCONSTANT(GAMEREQUEST_FILTER_APPUSERS,    dmFacebookGR::GAMEREQUEST_FILTER_APPUSERS);
    SETCONSTANT(GAMEREQUEST_FILTER_APPNONUSERS, dmFacebookGR::GAMEREQUEST_FILTER_APPNONUSERS);

    SETCONSTANT(AUDIENCE_NONE,     dmFacebookGR::AUDIENCE_NONE);
    SETCONSTANT(AUDIENCE_ONLYME,   dmFacebookGR::AUDIENCE_ONLYME);
    SETCONSTANT(AUDIENCE_FRIENDS,  dmFacebookGR::AUDIENCE_FRIENDS);
    SETCONSTANT(AUDIENCE_EVERYONE, dmFacebookGR::AUDIENCE_EVERYONE);

#undef SETCONSTANT

    lua_pushstring(L, dmFacebookGR::GRAPH_API_VERSION);
    lua_setfield(L, -2, "GRAPH_API_VERSION");

    dmFacebookGR::Analytics::RegisterConstants(L);

    lua_pop(L, 1);
    assert(top == lua_gettop(L));
}

} // namespace
