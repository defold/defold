#include <assert.h>
#include <extension/extension.h>
#include <dlib/dstrings.h>
#include <dlib/log.h>
#include <dlib/json.h>
#include <script/script.h>
#include <unistd.h>
#include <stdlib.h>

#include "facebook_private.h"
#include "facebook_util.h"
#include "facebook_analytics.h"

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
typedef void (*OnLoginCallback)(void* L, int state, const char* error, const char* me_json, const char* permissions_json);
typedef void (*OnRequestReadPermissionsCallback)(void *L, const char* error);
typedef void (*OnRequestPublishPermissionsCallback)(void *L, const char* error);
typedef void (*OnLoginWithPermissionsCallback)(void* L, int status, const char* error, const char* permissions_json);

extern "C" {
    // Implementation in library_facebook.js
    void dmFacebookInitialize(const char* app_id, const char* version);
    void dmFacebookAccessToken(OnAccessTokenCallback callback, lua_State* L);
    void dmFacebookPermissions(OnPermissionsCallback callback, lua_State* L);
    void dmFacebookMe(OnMeCallback callback, lua_State* L);
    void dmFacebookShowDialog(const char* params, const char* method, OnShowDialogCallback callback, lua_State* L);
    void dmFacebookDoLogin(int state_open, int state_closed, int state_failed, OnLoginCallback callback, lua_State* L);
    void dmFacebookDoLogout();
    void dmFacebookLoginWithPermissions(int state_open, int state_closed, int state_failed,
        const char* permissions, OnLoginWithPermissionsCallback callback, lua_State* thread);
    void dmFacebookRequestReadPermissions(const char* permissions, OnRequestReadPermissionsCallback callback, lua_State* L);
    void dmFacebookRequestPublishPermissions(const char* permissions, int audience, OnRequestPublishPermissionsCallback callback, lua_State* L);
    void dmFacebookPostEvent(const char* event, double valueToSum, const char* keys, const char* values);
    void dmFacebookEnableEventUsage();
    void dmFacebookDisableEventUsage();
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

static void PushError(lua_State*L, const char* error)
{
    // Could be extended with error codes etc
    if (error != NULL) {
        lua_newtable(L);
        lua_pushstring(L, "error");
        lua_pushstring(L, error);
        lua_rawset(L, -3);
    } else {
        lua_pushnil(L);
    }
}

static void RunStateCallback(lua_State* L, int state, const char *error)
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
        PushError(L, error);

        dmScript::PCall(L, 3, 0);
        assert(top == lua_gettop(L));
        dmScript::Unref(L, LUA_REGISTRYINDEX, callback);
    } else {
        dmLogError("No callback set");
    }
}

static void RunCallback(lua_State* L, const char *error)
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

        PushError(L, error);

        dmScript::PCall(L, 2, 0);
        assert(top == lua_gettop(L));
        dmScript::Unref(L, LUA_REGISTRYINDEX, callback);
    } else {
        dmLogError("No callback set");
    }
}

static void RunDialogResultCallback(lua_State* L, const char *result_json, const char *error)
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

        PushError(L, error);

        dmScript::PCall(L, 3, 0);
        assert(top == lua_gettop(L));
        dmScript::Unref(L, LUA_REGISTRYINDEX, callback);
    } else {
        dmLogError("No callback set");
    }
}

static void VerifyCallback(lua_State* L)
{
    if (g_Facebook.m_Callback != LUA_NOREF) {
        dmLogError("Unexpected callback set");
        dmScript::Unref(L, LUA_REGISTRYINDEX, g_Facebook.m_Callback);
        dmScript::Unref(L, LUA_REGISTRYINDEX, g_Facebook.m_Self);
        g_Facebook.m_Callback = LUA_NOREF;
        g_Facebook.m_Self = LUA_NOREF;
    }
}

static void OnLoginComplete(void* L, int state, const char* error, const char* me_json, const char* permissions_json)
{
    dmLogDebug("FB login complete...(%d, %s)", state, error);
    g_Facebook.m_MeJson = me_json;
    g_Facebook.m_PermissionsJson = permissions_json;
    RunStateCallback((lua_State*)L, state, error);
}

namespace dmFacebook
{

int Facebook_Login(lua_State* L)
{
    if( !g_Facebook.m_appId )
    {
        return luaL_error(L, "Facebook module isn't initialized! Did you set the facebook.appid in game.project?");
    }
    int top = lua_gettop(L);
    VerifyCallback(L);

    dmLogDebug("Logging in to FB...");
    luaL_checktype(L, 1, LUA_TFUNCTION);
    lua_pushvalue(L, 1);
    g_Facebook.m_Callback = dmScript::Ref(L, LUA_REGISTRYINDEX);

    dmScript::GetInstance(L);
    g_Facebook.m_Self = dmScript::Ref(L, LUA_REGISTRYINDEX);

    dmFacebookDoLogin(dmFacebook::STATE_OPEN, dmFacebook::STATE_CLOSED, dmFacebook::STATE_CLOSED_LOGIN_FAILED, (OnLoginCallback) OnLoginComplete, dmScript::GetMainThread(L));

    assert(top == lua_gettop(L));
    return 0;
}

int Facebook_Logout(lua_State* L)
{
    if( !g_Facebook.m_appId )
    {
        return luaL_error(L, "Facebook module isn't initialized! Did you set the facebook.appid in game.project?");
    }
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
static void AppendArray(lua_State* L, char* buffer, uint32_t buffer_size, int idx)
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


bool PlatformFacebookInitialized()
{
    return !!g_Facebook.m_appId;
}

static void OnLoginWithPermissions(
    void* L, int status, const char* error, const char* permissions_json)
{
    if (permissions_json != 0x0)
    {
        g_Facebook.m_PermissionsJson = permissions_json;
    }

    RunCallback((lua_State*) L, &g_Facebook.m_Self, &g_Facebook.m_Callback, error, status);
}

void PlatformFacebookLoginWithReadPermissions(lua_State* L, const char** permissions,
    uint32_t permission_count, int callback, int context, lua_State* thread)
{
    // This function must always return so memory for `permissions` can be free'd.
    VerifyCallback(L);
    g_Facebook.m_Callback = callback;
    g_Facebook.m_Self = context;

    char cstr_permissions[2048];
    cstr_permissions[0] = 0x0;
    JoinCStringArray(permissions, permission_count, cstr_permissions,
        sizeof(cstr_permissions) / sizeof(cstr_permissions[0]), ",");

    dmFacebookLoginWithPermissions(
        dmFacebook::STATE_OPEN, dmFacebook::STATE_CLOSED, dmFacebook::STATE_CLOSED_LOGIN_FAILED,
        cstr_permissions, (OnLoginWithPermissionsCallback) OnLoginWithPermissions, thread);
}

void PlatformFacebookLoginWithPublishPermissions(lua_State* L, const char** permissions,
    uint32_t permission_count, int audience, int callback, int context, lua_State* thread)
{
    // This function must always return so memory for `permissions` can be free'd.

    // On HTML5 both loginWithPublishPermissions and loginWithReadPermissions are
    // regular logins with the permissions as the scope. We're not respecting
    // audience either, which means both of these functions has the exact same
    // functionality.
    (void) audience;
    PlatformFacebookLoginWithReadPermissions(
        L, permissions, permission_count, callback, context, thread);
}



static void OnRequestReadPermissionsComplete(void* L, const char* error, const char* permissions_json)
{
    if (permissions_json != 0)
    {
        g_Facebook.m_PermissionsJson = permissions_json;
    }
    RunCallback((lua_State*)L, error);
}

int Facebook_RequestReadPermissions(lua_State* L)
{
    if( !g_Facebook.m_appId )
    {
        return luaL_error(L, "Facebook module isn't initialized! Did you set the facebook.appid in game.project?");
    }
    int top = lua_gettop(L);
    VerifyCallback(L);

    luaL_checktype(L, top-1, LUA_TTABLE);
    luaL_checktype(L, top, LUA_TFUNCTION);
    lua_pushvalue(L, top);
    g_Facebook.m_Callback = dmScript::Ref(L, LUA_REGISTRYINDEX);

    dmScript::GetInstance(L);
    g_Facebook.m_Self = dmScript::Ref(L, LUA_REGISTRYINDEX);

    char permissions[512] = { 0 };
    AppendArray(L, permissions, 512, top-1);

    dmFacebookRequestReadPermissions(permissions, (OnRequestReadPermissionsCallback) OnRequestReadPermissionsComplete, dmScript::GetMainThread(L));

    assert(top == lua_gettop(L));
    return 0;
}

static void OnRequestPublishPermissionsComplete(void* L, const char* error, const char* permissions_json)
{
    if (permissions_json != 0)
    {
        g_Facebook.m_PermissionsJson = permissions_json;
    }
    RunCallback((lua_State*)L, error);
}

int Facebook_RequestPublishPermissions(lua_State* L)
{
    if( !g_Facebook.m_appId )
    {
        return luaL_error(L, "Facebook module isn't initialized! Did you set the facebook.appid in game.project?");
    }
    int top = lua_gettop(L);
    VerifyCallback(L);

    luaL_checktype(L, top-2, LUA_TTABLE);
    // Cannot find any doc that implies that audience is used in the javascript sdk...
    int audience = luaL_checkinteger(L, top-1);
    luaL_checktype(L, top, LUA_TFUNCTION);
    lua_pushvalue(L, top);
    g_Facebook.m_Callback = dmScript::Ref(L, LUA_REGISTRYINDEX);

    dmScript::GetInstance(L);
    g_Facebook.m_Self = dmScript::Ref(L, LUA_REGISTRYINDEX);

    char permissions[512] = { 0 };
    AppendArray(L, permissions, 512, top-2);

    dmFacebookRequestPublishPermissions(permissions, audience, (OnRequestPublishPermissionsCallback) OnRequestPublishPermissionsComplete, dmScript::GetMainThread(L));

    assert(top == lua_gettop(L));
    return 0;
}


static void OnAccessTokenComplete(void* L, const char* access_token)
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
    if( !g_Facebook.m_appId )
    {
        return luaL_error(L, "Facebook module isn't initialized! Did you set the facebook.appid in game.project?");
    }
    int top = lua_gettop(L);

    dmFacebookAccessToken( (OnAccessTokenCallback) OnAccessTokenComplete, L);

    assert(top + 1 == lua_gettop(L));
    return 1;
}

int Facebook_Permissions(lua_State* L)
{
    if( !g_Facebook.m_appId )
    {
        return luaL_error(L, "Facebook module isn't initialized! Did you set the facebook.appid in game.project?");
    }
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
    if( !g_Facebook.m_appId )
    {
        return luaL_error(L, "Facebook module isn't initialized! Did you set the facebook.appid in game.project?");
    }
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




static void OnShowDialogComplete(void* L, const char* result_json, const char* error)
{
    RunDialogResultCallback((lua_State*)L, result_json, error);
}

int Facebook_ShowDialog(lua_State* L)
{
    if( !g_Facebook.m_appId )
    {
        return luaL_error(L, "Facebook module isn't initialized! Did you set the facebook.appid in game.project?");
    }
    int top = lua_gettop(L);
    VerifyCallback(L);

    const char* dialog = luaL_checkstring(L, 1);
    luaL_checktype(L, 2, LUA_TTABLE);
    luaL_checktype(L, 3, LUA_TFUNCTION);
    lua_pushvalue(L, 3);
    g_Facebook.m_Callback = dmScript::Ref(L, LUA_REGISTRYINDEX);
    dmScript::GetInstance(L);
    g_Facebook.m_Self = dmScript::Ref(L, LUA_REGISTRYINDEX);

    lua_newtable(L);
    int to_index = lua_gettop(L);
    if (0 == dmFacebook::DialogTableToEmscripten(L, dialog, 2, to_index)) {
        lua_pop(L, 1);
        assert(top == lua_gettop(L));
        return luaL_error(L, "Could not convert show dialog param table.");
    }

    int size_needed = 1 + dmFacebook::LuaTableToJson(L, to_index, 0, 0);
    char* params_json = (char*)malloc(size_needed);

    if (params_json == 0 || 0 == dmFacebook::LuaTableToJson(L, to_index, params_json, size_needed)) {
        lua_pop(L, 1);
        assert(top == lua_gettop(L));
        if( params_json ) {
            free(params_json);
        }
        return luaL_error(L, "Dialog params table too large.");
    }
    lua_pop(L, 1);

    dmFacebookShowDialog(params_json, dialog, (OnShowDialogCallback) OnShowDialogComplete, dmScript::GetMainThread(L));
    free(params_json);

    assert(top == lua_gettop(L));
    return 0;
}

int Facebook_PostEvent(lua_State* L)
{
    int argc = lua_gettop(L);
    const char* event = dmFacebook::Analytics::GetEvent(L, 1);
    double valueToSum = luaL_checknumber(L, 2);

    // Transform LUA table to a format that can be used by all platforms.
    const char* keys[dmFacebook::Analytics::MAX_PARAMS] = { 0 };
    const char* values[dmFacebook::Analytics::MAX_PARAMS] = { 0 };
    unsigned int length = 0;
    // TABLE is an optional argument and should only be parsed if provided.
    if (argc == 3)
    {
        length = dmFacebook::Analytics::MAX_PARAMS;
        dmFacebook::Analytics::GetParameterTable(L, 3, keys, values, &length);
    }

    const char* json_keys = dmJson::CStringArrayToJsonString(keys, length);
    const char* json_values = dmJson::CStringArrayToJsonString(values, length);

    // Forward call to JavaScript
    dmFacebookPostEvent(event, valueToSum, json_keys, json_values);

    free((void*) json_keys);
    free((void*) json_values);

    return 0;
}

int Facebook_EnableEventUsage(lua_State* L)
{
    dmFacebookEnableEventUsage();

    return 0;
}

int Facebook_DisableEventUsage(lua_State* L)
{
    dmFacebookDisableEventUsage();

    return 0;
}

} // namespace

dmExtension::Result InitializeFacebook(dmExtension::Params* params)
{
    dmFacebook::LuaInit(params->m_L);

    if(!g_Facebook.m_Initialized)
    {
        g_Facebook.m_appId = dmConfigFile::GetString(params->m_ConfigFile, "facebook.appid", 0);
        if( g_Facebook.m_appId )
        {
            dmFacebookInitialize(g_Facebook.m_appId, dmFacebook::GRAPH_API_VERSION);

            dmLogDebug("FB initialized.");

            g_Facebook.m_Initialized = true;
        }
        else
        {
            dmLogDebug("No facebook.appid. Disabling module");
        }
    }

    return dmExtension::RESULT_OK;
}

dmExtension::Result FinalizeFacebook(dmExtension::Params* params)
{
    // TODO: "Uninit" FB SDK here?

    g_Facebook = Facebook();

    return dmExtension::RESULT_OK;
}

DM_DECLARE_EXTENSION(FacebookExt, "Facebook", 0, 0, InitializeFacebook, 0, 0, FinalizeFacebook)
