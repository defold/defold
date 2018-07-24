#ifndef WEBVIEW_COMMON_H
#define WEBVIEW_COMMON_H

#include <script/script.h>

namespace dmWebView
{

static const int MAX_NUM_WEBVIEWS = 4;

enum CallbackResult
{
    CALLBACK_RESULT_URL_OK = 0,
    CALLBACK_RESULT_URL_ERROR = -1,
    CALLBACK_RESULT_EVAL_OK = 1,
    CALLBACK_RESULT_EVAL_ERROR = -2,
};

struct WebViewInfo
{
    lua_State*  m_L;
    int         m_Self;
    int         m_Callback;

    WebViewInfo() : m_L(0), m_Self(LUA_NOREF), m_Callback(LUA_NOREF) {}
};

struct RequestInfo
{
    int         m_Hidden;

    RequestInfo() { memset(this, 0, sizeof(*this)); }
};


void LuaInit(lua_State* L);


struct CallbackInfo
{
    WebViewInfo*    m_Info;
    int             m_WebViewID;
    int             m_RequestID;
    CallbackResult  m_Type;
    const char*     m_Url;
    const char*     m_Result;

    CallbackInfo() { memset(this, 0, sizeof(*this)); }
};

void RunCallback(CallbackInfo* cbinfo);

void ClearWebViewInfo(WebViewInfo* info);


int Platform_Create(lua_State* L, dmWebView::WebViewInfo* info);
int Platform_Destroy(lua_State* L, int webview_id);
int Platform_Open(lua_State* L, int webview_id, const char* url, dmWebView::RequestInfo* options);
int Platform_OpenRaw(lua_State* L, int webview_id, const char* html, dmWebView::RequestInfo* options);
int Platform_Eval(lua_State* L, int webview_id, const char* code);
int Platform_SetVisible(lua_State* L, int webview_id, int visible);
int Platform_IsVisible(lua_State* L, int webview_id);
int Platform_SetPosition(lua_State* L, int webview_id, int x, int y, int width, int height);

} // namespace

#endif // WEBVIEW_COMMON_H

