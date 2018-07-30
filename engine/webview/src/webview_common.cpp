#include "webview_common.h"
#include <dlib/log.h>


namespace dmWebView
{

void RunCallback(CallbackInfo* cbinfo)
{
    if (cbinfo->m_Info->m_Callback == LUA_NOREF)
    {
        dmLogError("No callback set");
    }

    lua_State* L = cbinfo->m_Info->m_L;
    int top = lua_gettop(L);

    lua_rawgeti(L, LUA_REGISTRYINDEX, cbinfo->m_Info->m_Callback);
    // Setup self
    lua_rawgeti(L, LUA_REGISTRYINDEX, cbinfo->m_Info->m_Self);
    lua_pushvalue(L, -1);

    dmScript::SetInstance(L);

    if (!dmScript::IsInstanceValid(L))
    {
        dmLogError("Could not run WebView callback because the instance has been deleted.");
        lua_pop(L, 2);
        assert(top == lua_gettop(L));
        return;
    }

    lua_pushnumber(L, (lua_Number)cbinfo->m_WebViewID);
    lua_pushnumber(L, (lua_Number)cbinfo->m_RequestID);
    lua_pushnumber(L, (lua_Number)cbinfo->m_Type);

    lua_newtable(L);

    lua_pushstring(L, "url");
    if( cbinfo->m_Url ) {
        lua_pushstring(L, cbinfo->m_Url);
    } else {
        lua_pushnil(L);
    }
    lua_rawset(L, -3);

    lua_pushstring(L, "result");
    if( cbinfo->m_Result ) {
        lua_pushstring(L, cbinfo->m_Result);
    } else {
        lua_pushnil(L);
    }
    lua_rawset(L, -3);


    int ret = lua_pcall(L, 5, 0, 0);
    if (ret != 0) {
        dmLogError("Error running WebView callback: %s", lua_tostring(L,-1));
        lua_pop(L, 1);
    }
    assert(top == lua_gettop(L));
}

void ClearWebViewInfo(WebViewInfo* info)
{
    if( info->m_Callback != LUA_NOREF )
        dmScript::Unref(info->m_L, LUA_REGISTRYINDEX, info->m_Callback);
    if( info->m_Self != LUA_NOREF )
        dmScript::Unref(info->m_L, LUA_REGISTRYINDEX, info->m_Self);
    info->m_L = 0;
    info->m_Callback = LUA_NOREF;
    info->m_Self = LUA_NOREF;
}

/** Creates a web view
@param callback callback to use for requests
@return the web view id
*/
static int Create(lua_State* L)
{
    int top = lua_gettop(L);

    luaL_checktype(L, 1, LUA_TFUNCTION);
    lua_pushvalue(L, 1);

    WebViewInfo info;
    info.m_Callback = dmScript::Ref(L, LUA_REGISTRYINDEX);
    dmScript::GetInstance(L);
    info.m_Self = dmScript::Ref(L, LUA_REGISTRYINDEX);
    info.m_L = dmScript::GetMainThread(L);

    int webview_id = Platform_Create(L, &info);
    lua_pushnumber(L, webview_id);

    assert(top + 1 == lua_gettop(L));
    return 1;
}

/** Closes a web view
@param id the web view id
@return -1 if an error occurred. 0 if it was destroyed
*/
static int Destroy(lua_State* L)
{
    int top = lua_gettop(L);

    const int webview_id = luaL_checknumber(L, 1);

    int result = Platform_Destroy(L, webview_id);
    lua_pushnumber(L, result);

    assert(top + 1 == lua_gettop(L));
    return 1;
}

void ParseOptions(lua_State* L, int argumentindex, RequestInfo* requestinfo)
{
    luaL_checktype(L, argumentindex, LUA_TTABLE);
    lua_pushvalue(L, argumentindex);
    lua_pushnil(L);
    while (lua_next(L, -2)) {
        const char* attr = lua_tostring(L, -2);
        if( strcmp(attr, "hidden") == 0 )
        {
            luaL_checktype(L, -1, LUA_TBOOLEAN);
            requestinfo->m_Hidden = lua_toboolean(L, -1);
        }
        lua_pop(L, 1);
    }
    lua_pop(L, 1);
}

/** Opens an url in the view
@param url url
@return the request id
*/
static int Open(lua_State* L)
{
    int top = lua_gettop(L);
    const int webview_id = luaL_checknumber(L, 1);
    const char* url = luaL_checkstring(L, 2);

    RequestInfo requestinfo;
    if( top >= 3 && !lua_isnil(L, 3))
    {
        ParseOptions(L, 3, &requestinfo);
    }

    int request_id = Platform_Open(L, webview_id, url, &requestinfo);
    lua_pushnumber(L, request_id);

    assert(top + 1 == lua_gettop(L));
    return 1;
}

static int OpenRaw(lua_State* L)
{
    int top = lua_gettop(L);
    const int webview_id = luaL_checknumber(L, 1);
    const char* html = luaL_checkstring(L, 2);

    RequestInfo requestinfo;
    if( top >= 3 && !lua_isnil(L, 3))
    {
        ParseOptions(L, 3, &requestinfo);
    }

    int request_id = Platform_OpenRaw(L, webview_id, html, &requestinfo);
    lua_pushnumber(L, request_id);

    assert(top + 1 == lua_gettop(L));
    return 1;
}

static int Eval(lua_State* L)
{
    int top = lua_gettop(L);
    const int webview_id = luaL_checknumber(L, 1);
    const char* code = luaL_checkstring(L, 2);

    int request_id = Platform_Eval(L, webview_id, code);
    lua_pushnumber(L, request_id);

    assert(top + 1 == lua_gettop(L));
    return 1;
}

static int SetVisible(lua_State* L)
{
    int top = lua_gettop(L);
    const int webview_id = luaL_checknumber(L, 1);
    const int visible = luaL_checknumber(L, 2);

    Platform_SetVisible(L, webview_id, visible);

    assert(top == lua_gettop(L));
    return 0;
}

static int IsVisible(lua_State* L)
{
    int top = lua_gettop(L);
    const int webview_id = luaL_checknumber(L, 1);

    int visible = Platform_IsVisible(L, webview_id);
    lua_pushnumber(L, visible);

    assert(top + 1 == lua_gettop(L));
    return 1;
}

static int SetPosition(lua_State* L)
{
    int top = lua_gettop(L);
    const int webview_id = luaL_checknumber(L, 1);
    const int x = luaL_checknumber(L, 2);
    const int y = luaL_checknumber(L, 3);
    const int width = luaL_checknumber(L, 4);
    const int height = luaL_checknumber(L, 5);

    Platform_SetPosition(L, webview_id, x, y, width, height);

    assert(top == lua_gettop(L));
    return 0;
}

static const luaL_reg WebView_methods[] =
{
    {"create", Create},
    {"destroy", Destroy},
    {"open", Open},
    {"open_raw", OpenRaw},
    {"eval", Eval},
    {"set_visible", SetVisible},
    {"set_position", SetPosition},
    {"is_visible", IsVisible},
    {0, 0}
};

static void LuaInit(lua_State* L)
{
    int top = lua_gettop(L);
    luaL_register(L, "webview", WebView_methods);

#define SETCONSTANT(name) \
        lua_pushnumber(L, (lua_Number) name); \
        lua_setfield(L, -2, #name);\

    SETCONSTANT(CALLBACK_RESULT_URL_OK)
    SETCONSTANT(CALLBACK_RESULT_URL_ERROR)
    SETCONSTANT(CALLBACK_RESULT_EVAL_OK)
    SETCONSTANT(CALLBACK_RESULT_EVAL_ERROR)

#undef SETCONSTANT

    lua_pop(L, 1);
    assert(top == lua_gettop(L));
}

static dmExtension::Result WebView_AppInitialize(dmExtension::AppParams* params)
{
    return Platform_AppInitialize(params);
}

static dmExtension::Result WebView_AppFinalize(dmExtension::AppParams* params)
{
    return Platform_AppFinalize(params);
}

static dmExtension::Result WebView_Initialize(dmExtension::Params* params)
{
    LuaInit(params->m_L);

    return Platform_Initialize(params);
}

static dmExtension::Result WebView_Finalize(dmExtension::Params* params)
{
    return Platform_Finalize(params);
}

static dmExtension::Result WebView_Update(dmExtension::Params* params)
{
    return Platform_Update(params);
}

DM_DECLARE_EXTENSION(WebViewExt, "WebView", WebView_AppInitialize, WebView_AppFinalize, WebView_Initialize, WebView_Update, 0, WebView_Finalize)


} // namespace
