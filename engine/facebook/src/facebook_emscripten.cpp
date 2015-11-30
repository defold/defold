#include <assert.h>
#include <extension/extension.h>
#include <dlib/dstrings.h>
#include <dlib/log.h>
#include <dlib/json.h>
#include <script/script.h>
#include <unistd.h>
#include <stdlib.h>

#include "facebook.h"

#define LIB_NAME "facebook"

// Must match iOS for now
enum Audience
{
    AUDIENCE_NONE = 0,
    AUDIENCE_ONLYME = 10,
    AUDIENCE_FRIENDS = 20,
    AUDIENCE_EVERYONE = 30
};

struct Facebook
{
    Facebook()
    {
        memset(this, 0, sizeof(*this));
        m_Callback = LUA_NOREF;
        m_Self = LUA_NOREF;
        m_Initialized = false;
    }
    int m_Callback;
    int m_Self;
    const char* m_appId;
    const char* m_MeJson;
    const char* m_PermissionsJson;
    bool m_Initialized;
};

Facebook g_Facebook;

typedef void (*OnAccessTokenCallback)(void *L, const char* access_token);
typedef void (*OnPermissionsCallback)(void *L, const char* json_arr);
typedef void (*OnMeCallback)(void *L, const char* json);
typedef void (*OnShowDialogCallback)(void* L, const char* url, const char* error);
typedef void (*OnLoginCallback)(void *L, int state, const char* error);
typedef void (*OnRequestReadPermissionsCallback)(void *L, const char* error);
typedef void (*OnRequestPublishPermissionsCallback)(void *L, const char* error);

extern "C" {
    // Implementation in library_facebook.js
    void dmFacebookInitialize(const char* app_id);
    void dmFacebookAccessToken(OnAccessTokenCallback callback, lua_State* L);
    void dmFacebookPermissions(OnPermissionsCallback callback, lua_State* L);
    void dmFacebookMe(OnMeCallback callback, lua_State* L);
    void dmFacebookShowDialog(const char* params, const char* method, OnShowDialogCallback callback, lua_State* L);
    void dmFacebookDoLogin(int state_open, int state_closed, int state_failed, OnLoginCallback callback, lua_State* L);
    void dmFacebookDoLogout();
    void dmFacebookRequestReadPermissions(const char* permissions, OnRequestReadPermissionsCallback callback, lua_State* L);
    void dmFacebookRequestPublishPermissions(const char* permissions, int audience, OnRequestPublishPermissionsCallback callback, lua_State* L);
}

// TODO: Move out to common stuff (also in engine/iap/src/iap_android.cpp and script_json among others)
static int ToLua(lua_State*L, dmJson::Document* doc, int index)
{
    const dmJson::Node& n = doc->m_Nodes[index];
    const char* json = doc->m_Json;
    int l = n.m_End - n.m_Start;
    switch (n.m_Type)
    {
    case dmJson::TYPE_PRIMITIVE:
        if (l == 4 && memcmp(json + n.m_Start, "null", 4) == 0) {
            lua_pushnil(L);
        } else if (l == 4 && memcmp(json + n.m_Start, "true", 4) == 0) {
            lua_pushboolean(L, 1);
        } else if (l == 5 && memcmp(json + n.m_Start, "false", 5) == 0) {
            lua_pushboolean(L, 0);
        } else {
            double val = atof(json + n.m_Start);
            lua_pushnumber(L, val);
        }
        return index + 1;

    case dmJson::TYPE_STRING:
        lua_pushlstring(L, json + n.m_Start, l);
        return index + 1;

    case dmJson::TYPE_ARRAY:
        lua_createtable(L, n.m_Size, 0);
        ++index;
        for (int i = 0; i < n.m_Size; ++i) {
            index = ToLua(L, doc, index);
            lua_rawseti(L, -2, i+1);
        }
        return index;

    case dmJson::TYPE_OBJECT:
        lua_createtable(L, 0, n.m_Size);
        ++index;
        for (int i = 0; i < n.m_Size; i += 2) {
            index = ToLua(L, doc, index);
            index = ToLua(L, doc, index);
            lua_rawset(L, -3);
        }

        return index;
    }

    assert(false && "not reached");
    return index;
}

static void PushError(lua_State*L, const char* error, int error_code)
{
    // Could be extended with error codes etc
    if (error != NULL) {
        lua_newtable(L);
        lua_pushstring(L, "error");
        lua_pushstring(L, error);
        lua_rawset(L, -3);
        lua_pushstring(L, "error_code");
        lua_pushnumber(L, error_code);
        lua_rawset(L, -3);
    } else {
        lua_pushnil(L);
    }
}

static void RunStateCallback(lua_State* L, int state, const char *error, int error_code)
{
    if (g_Facebook.m_Callback != LUA_NOREF) {
        int top = lua_gettop(L);

        int callback = g_Facebook.m_Callback;
        g_Facebook.m_Callback = LUA_NOREF;
        lua_rawgeti(L, LUA_REGISTRYINDEX, callback);

        // Setup self
        lua_rawgeti(L, LUA_REGISTRYINDEX, g_Facebook.m_Self);
        lua_pushvalue(L, -1);
        dmScript::SetInstance(L);

        if (!dmScript::IsInstanceValid(L))
        {
            dmLogError("Could not run facebook callback because the instance has been deleted.");
            lua_pop(L, 2);
            assert(top == lua_gettop(L));
            return;
        }

        lua_pushnumber(L, (lua_Number) state);
        PushError(L, error, error_code);

        int ret = dmScript::PCall(L, 3, LUA_MULTRET);
        assert(top == lua_gettop(L));
        luaL_unref(L, LUA_REGISTRYINDEX, callback);
    } else {
        dmLogError("No callback set");
    }
}

static void RunCallback(lua_State* L, const char *error, int error_code)
{
    if (g_Facebook.m_Callback != LUA_NOREF) {
        int top = lua_gettop(L);

        int callback = g_Facebook.m_Callback;
        lua_rawgeti(L, LUA_REGISTRYINDEX, callback);

        // Setup self
        lua_rawgeti(L, LUA_REGISTRYINDEX, g_Facebook.m_Self);
        lua_pushvalue(L, -1);
        dmScript::SetInstance(L);

        if (!dmScript::IsInstanceValid(L))
        {
            dmLogError("Could not run facebook callback because the instance has been deleted.");
            lua_pop(L, 2);
            assert(top == lua_gettop(L));
            return;
        }

        PushError(L, error, error_code);

        int ret = dmScript::PCall(L, 2, LUA_MULTRET);
        assert(top == lua_gettop(L));
        luaL_unref(L, LUA_REGISTRYINDEX, callback);
    } else {
        dmLogError("No callback set");
    }
}

static void RunDialogResultCallback(lua_State* L, const char *result_json, const char *error, int error_code)
{
    if (g_Facebook.m_Callback != LUA_NOREF) {
        int top = lua_gettop(L);

        int callback = g_Facebook.m_Callback;
        lua_rawgeti(L, LUA_REGISTRYINDEX, callback);

        // Setup self
        lua_rawgeti(L, LUA_REGISTRYINDEX, g_Facebook.m_Self);
        lua_pushvalue(L, -1);
        dmScript::SetInstance(L);

        if (!dmScript::IsInstanceValid(L))
        {
            dmLogError("Could not run facebook callback because the instance has been deleted.");
            lua_pop(L, 2);
            assert(top == lua_gettop(L));
            return;
        }

        if(result_json != 0)
        {
            dmJson::Document doc;
            dmJson::Result r = dmJson::Parse(result_json, &doc);
            if (r == dmJson::RESULT_OK && doc.m_NodeCount > 0) {
                ToLua((lua_State *)L, &doc, 0);
            } else {
                dmLogError("Failed to parse dialog result JSON (%d)", r);
                lua_newtable((lua_State *)L);
            }
            dmJson::Free(&doc);
        }
        else
        {
            dmLogError("Got empty dialog result JSON (or FB error).");
            lua_newtable((lua_State *)L);
        }

        PushError(L, error, error_code);

        int ret = dmScript::PCall(L, 3, LUA_MULTRET);
        assert(top == lua_gettop(L));
        luaL_unref(L, LUA_REGISTRYINDEX, callback);
    } else {
        dmLogError("No callback set");
    }
}

static void VerifyCallback(lua_State* L)
{
    if (g_Facebook.m_Callback != LUA_NOREF) {
        dmLogError("Unexpected callback set");
        luaL_unref(L, LUA_REGISTRYINDEX, g_Facebook.m_Callback);
        luaL_unref(L, LUA_REGISTRYINDEX, g_Facebook.m_Self);
        g_Facebook.m_Callback = LUA_NOREF;
        g_Facebook.m_Self = LUA_NOREF;
    }
}

void OnLoginComplete(void* L, int state, const char* error, int error_code, const char* me_json, const char* permissions_json)
{
    dmLogDebug("FB login complete...(%d, %s, %d)", state, error, error_code);
    g_Facebook.m_MeJson = me_json;
    g_Facebook.m_PermissionsJson = permissions_json;
    RunStateCallback((lua_State*)L, state, error, error_code);
}

int Facebook_Login(lua_State* L)
{
    int top = lua_gettop(L);
    VerifyCallback(L);

    dmLogDebug("Logging in to FB...");
    luaL_checktype(L, 1, LUA_TFUNCTION);
    lua_pushvalue(L, 1);
    g_Facebook.m_Callback = luaL_ref(L, LUA_REGISTRYINDEX);

    dmScript::GetInstance(L);
    g_Facebook.m_Self = luaL_ref(L, LUA_REGISTRYINDEX);

    dmFacebookDoLogin(dmFacebook::STATE_OPEN, dmFacebook::STATE_CLOSED, dmFacebook::STATE_CLOSED_LOGIN_FAILED, (OnLoginCallback) OnLoginComplete, dmScript::GetMainThread(L));

    assert(top == lua_gettop(L));
    return 0;
}

int Facebook_Logout(lua_State* L)
{
    int top = lua_gettop(L);
    VerifyCallback(L);

    dmFacebookDoLogout();

    if (g_Facebook.m_MeJson) {
        g_Facebook.m_MeJson = 0;
    }
    if (g_Facebook.m_PermissionsJson) {
        g_Facebook.m_PermissionsJson = 0;
    }

    assert(top == lua_gettop(L));
    return 0;
}

// TODO: Move out to common stuff (also in engine/facebook/src/facebook_android.cpp)
void AppendArray(lua_State* L, char* buffer, uint32_t buffer_size, int idx)
{
    lua_pushnil(L);
    *buffer = 0;
    while (lua_next(L, idx) != 0)
    {
        if (!lua_isstring(L, -1))
            luaL_error(L, "permissions can only be strings (not %s)", lua_typename(L, lua_type(L, -1)));
        if (*buffer != 0)
            dmStrlCat(buffer, ",", buffer_size);
        const char* permission = lua_tostring(L, -1);
        dmStrlCat(buffer, permission, buffer_size);
        lua_pop(L, 1);
    }
}


void OnRequestReadPermissionsComplete(void* L, const char* error, int error_code, const char* permissions_json)
{
    if (permissions_json != 0)
    {
        g_Facebook.m_PermissionsJson = permissions_json;
    }
    RunCallback((lua_State*)L, error, error_code);
}

int Facebook_RequestReadPermissions(lua_State* L)
{
    int top = lua_gettop(L);
    VerifyCallback(L);

    luaL_checktype(L, top-1, LUA_TTABLE);
    luaL_checktype(L, top, LUA_TFUNCTION);
    lua_pushvalue(L, top);
    g_Facebook.m_Callback = luaL_ref(L, LUA_REGISTRYINDEX);

    dmScript::GetInstance(L);
    g_Facebook.m_Self = luaL_ref(L, LUA_REGISTRYINDEX);

    char permissions[512];
    AppendArray(L, permissions, 512, top-1);

    dmFacebookRequestReadPermissions(permissions, (OnRequestReadPermissionsCallback) OnRequestReadPermissionsComplete, dmScript::GetMainThread(L));

    assert(top == lua_gettop(L));
    return 0;
}

void OnRequestPublishPermissionsComplete(void* L, const char* error, int error_code, const char* permissions_json)
{
    if (permissions_json != 0)
    {
        g_Facebook.m_PermissionsJson = permissions_json;
    }
    RunCallback((lua_State*)L, error, error_code);
}

int Facebook_RequestPublishPermissions(lua_State* L)
{
    int top = lua_gettop(L);
    VerifyCallback(L);

    luaL_checktype(L, top-2, LUA_TTABLE);
    // Cannot find any doc that implies that audience is used in the javascript sdk...
    int audience = luaL_checkinteger(L, top-1);
    luaL_checktype(L, top, LUA_TFUNCTION);
    lua_pushvalue(L, top);
    g_Facebook.m_Callback = luaL_ref(L, LUA_REGISTRYINDEX);

    dmScript::GetInstance(L);
    g_Facebook.m_Self = luaL_ref(L, LUA_REGISTRYINDEX);

    char permissions[512];
    AppendArray(L, permissions, 512, top-2);

    dmFacebookRequestPublishPermissions(permissions, audience, (OnRequestPublishPermissionsCallback) OnRequestPublishPermissionsComplete, dmScript::GetMainThread(L));

    assert(top == lua_gettop(L));
    return 0;
}


void OnAccessTokenComplete(void* L, const char* access_token)
{
    if(access_token != 0)
    {
        lua_pushstring((lua_State *)L, access_token);
    }
    else
    {
        lua_pushnil((lua_State *)L);
        dmLogError("Access_token is null (logged out?).");
    }
}

int Facebook_AccessToken(lua_State* L)
{
    int top = lua_gettop(L);

    dmFacebookAccessToken( (OnAccessTokenCallback) OnAccessTokenComplete, L);

    assert(top + 1 == lua_gettop(L));
    return 1;
}

int Facebook_Permissions(lua_State* L)
{
    int top = lua_gettop(L);

    if(g_Facebook.m_PermissionsJson != 0)
    {
        // Note that the permissionsJsonArray contains a regular string array in json format,
        // e.g. ["foo", "bar", "baz", ...]
        dmJson::Document doc;
        dmJson::Result r = dmJson::Parse(g_Facebook.m_PermissionsJson, &doc);
        if (r == dmJson::RESULT_OK && doc.m_NodeCount > 0) {
            ToLua((lua_State *)L, &doc, 0);
        } else {
            dmLogError("Failed to parse Facebook_Permissions response (%d)", r);
            lua_newtable((lua_State *)L);
        }
        dmJson::Free(&doc);
    }
    else
    {
        dmLogError("Got empty Facebook_Permissions response (or FB error).");
        // This follows the iOS implementation...
        lua_newtable((lua_State *)L);
    }

    assert(top + 1 == lua_gettop(L));
    return 1;
}

int Facebook_Me(lua_State* L)
{
    int top = lua_gettop(L);

    if(g_Facebook.m_MeJson != 0)
    {
        // Note: this will pass ALL key/values back to lua, not just string types!
        dmJson::Document doc;
        dmJson::Result r = dmJson::Parse(g_Facebook.m_MeJson, &doc);
        if (r == dmJson::RESULT_OK && doc.m_NodeCount > 0) {
            ToLua(L, &doc, 0);
        } else {
            dmLogError("Failed to parse Facebook_Me response (%d)", r);
            lua_pushnil(L);
        }
        dmJson::Free(&doc);
    }
    else
    {
        // This follows the iOS implementation...
        dmLogError("Got empty Facebook_Me response (or FB error).");
        lua_pushnil(L);
    }

    assert(top + 1 == lua_gettop(L));
    return 1;
}




void OnShowDialogComplete(void* L, const char* result_json, const char* error, int error_code)
{
    RunDialogResultCallback((lua_State*)L, result_json, error, error_code);
}

int Facebook_ShowDialog(lua_State* L)
{
    int top = lua_gettop(L);
    VerifyCallback(L);

    const char* dialog = luaL_checkstring(L, 1);
    luaL_checktype(L, 2, LUA_TTABLE);
    luaL_checktype(L, 3, LUA_TFUNCTION);
    lua_pushvalue(L, 3);
    g_Facebook.m_Callback = luaL_ref(L, LUA_REGISTRYINDEX);
    dmScript::GetInstance(L);
    g_Facebook.m_Self = luaL_ref(L, LUA_REGISTRYINDEX);

    char params_json[1024];
    params_json[0] = '{';
    params_json[1] = '\0';
    char tmp[128];
    lua_pushnil(L);
    int i = 0;
    while (lua_next(L, 2) != 0) {
        const char* v = luaL_checkstring(L, -1);
        const char* k = luaL_checkstring(L, -2);
        DM_SNPRINTF(tmp, sizeof(tmp), "\"%s\": \"%s\"", k, v);
        if (i > 0) {
            dmStrlCat(params_json, ",", sizeof(params_json));
        }
        dmStrlCat(params_json, tmp, sizeof(params_json));
        lua_pop(L, 1);
        ++i;
    }
    dmStrlCat(params_json, "}", sizeof(params_json));

    dmFacebookShowDialog(params_json, dialog, (OnShowDialogCallback) OnShowDialogComplete, dmScript::GetMainThread(L));

    assert(top == lua_gettop(L));
    return 0;
}


static const luaL_reg Facebook_methods[] =
{
    {"login", Facebook_Login},
    {"logout", Facebook_Logout},
    {"access_token", Facebook_AccessToken},
    {"permissions", Facebook_Permissions},
    {"request_read_permissions", Facebook_RequestReadPermissions},
    {"request_publish_permissions", Facebook_RequestPublishPermissions},
    {"me", Facebook_Me},
    {"show_dialog", Facebook_ShowDialog},
    {0, 0}
};

dmExtension::Result InitializeFacebook(dmExtension::Params* params)
{
    if(!g_Facebook.m_Initialized)
    {
        // 355198514515820 is HelloFBSample. Default value in order to avoid exceptions
        // Better solution?
        g_Facebook.m_appId = dmConfigFile::GetString(params->m_ConfigFile, "facebook.appid", "355198514515820");

        dmFacebookInitialize(g_Facebook.m_appId);

        dmLogDebug("FB initialized.");

        g_Facebook.m_Initialized = true;
    }

    lua_State* L = params->m_L;
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

    lua_pop(L, 1);
    assert(top == lua_gettop(L));

    return dmExtension::RESULT_OK;
}

dmExtension::Result FinalizeFacebook(dmExtension::Params* params)
{
    // TODO: "Uninit" FB SDK here?

    g_Facebook = Facebook();

    return dmExtension::RESULT_OK;
}

DM_DECLARE_EXTENSION(FacebookExt, "Facebook", 0, 0, InitializeFacebook, 0, 0, FinalizeFacebook)
