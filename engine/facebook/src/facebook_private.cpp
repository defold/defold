
#include "facebook_private.h"
#include "facebook_analytics.h"
#include <dlib/log.h>
#include <dlib/dstrings.h>

void RunCallback(lua_State* L, int* _self, int* _callback, const char* error, int status)
{
    if (*_callback != LUA_NOREF)
    {
        int top = lua_gettop(L);

        lua_rawgeti(L, LUA_REGISTRYINDEX, *_callback);
        lua_rawgeti(L, LUA_REGISTRYINDEX, *_self);
        lua_pushvalue(L, -1);
        dmScript::SetInstance(L);

        if (dmScript::IsInstanceValid(L))
        {
            lua_newtable(L);

            if (error != NULL) {
                lua_pushstring(L, "error");
                lua_pushstring(L, error);
                lua_rawset(L, -3);
            }

            lua_pushstring(L, "status");
            lua_pushnumber(L, status);
            lua_rawset(L, -3);

            if (lua_pcall(L, 2, LUA_MULTRET, 0) != 0)
            {
                dmLogError("Error running facebook callback: %s", lua_tostring(L, -1));
                lua_pop(L, 1);
            }

            assert(top == lua_gettop(L));
            dmScript::Unref(L, LUA_REGISTRYINDEX, *_callback);
            dmScript::Unref(L, LUA_REGISTRYINDEX, *_self);
            *_callback = LUA_NOREF;
            *_self = LUA_NOREF;
        }
        else
        {
            dmLogError("Could not run facebook callback because the instance has been deleted.");
            lua_pop(L, 2);
            assert(top == lua_gettop(L));
        }
    }
    else
    {
        dmLogError("No callback set for facebook");
    }
}

uint32_t luaTableToCArray(lua_State* L, int index, char** buffer, uint32_t buffer_size)
{
    uint32_t entries = 0;
    memset(buffer, 0x0, buffer_size);

    lua_pushnil(L);
    while (lua_next(L, index))
    {
        if (lua_isstring(L, -1))
        {
            if (entries < buffer_size)
            {
                const char* permission = lua_tostring(L, -1);

                uint32_t permission_buffer_len = strlen(permission) + 1;
                char* permission_buffer = (char*) malloc(permission_buffer_len);
                DM_SNPRINTF(permission_buffer, permission_buffer_len, "%s", permission);

                buffer[entries++] = permission_buffer;
            }
        }
        else
        {
            for (uint32_t i = 0; i < entries; ++i)
            {
                free(buffer[i]);
            }
            return luaL_error(L, "Facebook permissions must be strings, got '%s'", lua_typename(L, lua_type(L, -1)));
        }

        lua_pop(L, 1);
    }

    return entries;
}

namespace dmFacebook {

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

    int top = lua_gettop(L);

    luaL_checktype(L, 1, LUA_TTABLE);
    luaL_checktype(L, 2, LUA_TNUMBER);
    luaL_checktype(L, 3, LUA_TFUNCTION);

    char* permissions[128] = { 0 };
    uint32_t permission_count = luaTableToCArray(L, 1, permissions, sizeof(permissions));

    int audience = luaL_checkinteger(L, 2);

    lua_pushvalue(L, 3);
    int callback = dmScript::Ref(L, LUA_REGISTRYINDEX);

    dmScript::GetInstance(L);
    int context = dmScript::Ref(L, LUA_REGISTRYINDEX);

    lua_State* thread = dmScript::GetMainThread(L);

    PlatformFacebookLoginWithPublishPermissions(
        L, (const char**) permissions, permission_count, audience, callback, context, thread);

    for (uint32_t i = 0; i < permission_count; ++i)
    {
        free(permissions[i]);
    }

    assert(top == lua_gettop(L));
    return 0;
}

int Facebook_LoginWithReadPermissions(lua_State* L)
{
    if (!PlatformFacebookInitialized())
    {
        return luaL_error(L, "Facebook module has not been initialized, is facebook.appid set in game.project?");
    }

    int top = lua_gettop(L);

    luaL_checktype(L, 1, LUA_TTABLE);
    luaL_checktype(L, 2, LUA_TFUNCTION);

    char* permissions[128] = { 0 };
    uint32_t permission_count = luaTableToCArray(L, 1, permissions, sizeof(permissions));

    lua_pushvalue(L, 2);
    int callback = dmScript::Ref(L, LUA_REGISTRYINDEX);

    dmScript::GetInstance(L);
    int context = dmScript::Ref(L, LUA_REGISTRYINDEX);

    lua_State* thread = dmScript::GetMainThread(L);

    PlatformFacebookLoginWithReadPermissions(
        L, (const char**) permissions, permission_count, callback, context, thread);

    for (uint32_t i = 0; i < permission_count; ++i)
    {
        free(permissions[i]);
    }

    assert(top == lua_gettop(L));
    return 0;
}

void LuaInit(lua_State* L)
{
    int top = lua_gettop(L);
    luaL_register(L, LIB_NAME, Facebook_methods);

    #define SETCONSTANT(name, val) \
        lua_pushnumber(L, (lua_Number) val); \
        lua_setfield(L, -2, #name);\

    SETCONSTANT(STATE_CREATED,              dmFacebook::STATE_CREATED);
    SETCONSTANT(STATE_CREATED_TOKEN_LOADED, dmFacebook::STATE_CREATED_TOKEN_LOADED);
    SETCONSTANT(STATE_CREATED_OPENING,      dmFacebook::STATE_CREATED_OPENING);
    SETCONSTANT(STATE_OPEN,                 dmFacebook::STATE_OPEN);
    SETCONSTANT(STATE_OPEN_TOKEN_EXTENDED,  dmFacebook::STATE_OPEN_TOKEN_EXTENDED);
    SETCONSTANT(STATE_CLOSED,               dmFacebook::STATE_CLOSED);
    SETCONSTANT(STATE_CLOSED_LOGIN_FAILED,  dmFacebook::STATE_CLOSED_LOGIN_FAILED);

    SETCONSTANT(GAMEREQUEST_ACTIONTYPE_NONE,   dmFacebook::GAMEREQUEST_ACTIONTYPE_NONE);
    SETCONSTANT(GAMEREQUEST_ACTIONTYPE_SEND,   dmFacebook::GAMEREQUEST_ACTIONTYPE_SEND);
    SETCONSTANT(GAMEREQUEST_ACTIONTYPE_ASKFOR, dmFacebook::GAMEREQUEST_ACTIONTYPE_ASKFOR);
    SETCONSTANT(GAMEREQUEST_ACTIONTYPE_TURN,   dmFacebook::GAMEREQUEST_ACTIONTYPE_TURN);

    SETCONSTANT(GAMEREQUEST_FILTER_NONE,        dmFacebook::GAMEREQUEST_FILTER_NONE);
    SETCONSTANT(GAMEREQUEST_FILTER_APPUSERS,    dmFacebook::GAMEREQUEST_FILTER_APPUSERS);
    SETCONSTANT(GAMEREQUEST_FILTER_APPNONUSERS, dmFacebook::GAMEREQUEST_FILTER_APPNONUSERS);

    SETCONSTANT(AUDIENCE_NONE,     dmFacebook::AUDIENCE_NONE);
    SETCONSTANT(AUDIENCE_ONLYME,   dmFacebook::AUDIENCE_ONLYME);
    SETCONSTANT(AUDIENCE_FRIENDS,  dmFacebook::AUDIENCE_FRIENDS);
    SETCONSTANT(AUDIENCE_EVERYONE, dmFacebook::AUDIENCE_EVERYONE);

#undef SETCONSTANT

    lua_pushstring(L, dmFacebook::GRAPH_API_VERSION);
    lua_setfield(L, -2, "GRAPH_API_VERSION");

    dmFacebook::Analytics::RegisterConstants(L);

    lua_pop(L, 1);
    assert(top == lua_gettop(L));
}

} // namespace
