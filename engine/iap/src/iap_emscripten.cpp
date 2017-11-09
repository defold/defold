#include <stdlib.h>
#include <unistd.h>
#include <dlib/array.h>
#include <dlib/log.h>
#include <dlib/dstrings.h>
#include <dlib/json.h>
#include <script/script.h>
#include <extension/extension.h>
#include "iap.h"
#include "iap_private.h"

#define LIB_NAME "iap"

struct IAP
{
    IAP()
    {
        memset(this, 0, sizeof(*this));
        m_Callback = LUA_NOREF;
        m_Self = LUA_NOREF;
        m_Listener.m_Callback = LUA_NOREF;
        m_Listener.m_Self = LUA_NOREF;
        m_autoFinishTransactions = true;
    }
    int                  m_InitCount;
    int                  m_Callback;
    int                  m_Self;
    bool                 m_autoFinishTransactions;
    lua_State*           m_L;
    IAPListener          m_Listener;

} g_IAP;

typedef void (*OnIAPFBList)(void *L, const char* json);
typedef void (*OnIAPFBListenerCallback)(void *L, const char* json, int error_code);

extern "C" {
    // Implementation in library_facebook_iap.js
    void dmIAPFBList(const char* item_ids, OnIAPFBList callback, lua_State* L);
    void dmIAPFBBuy(const char* item_id, const char* request_id, OnIAPFBListenerCallback callback, lua_State* L);
}


static void VerifyCallback(lua_State* L)
{
    if (g_IAP.m_Callback != LUA_NOREF) {
        dmLogError("Unexpected callback set");
        dmScript::Unref(L, LUA_REGISTRYINDEX, g_IAP.m_Callback);
        dmScript::Unref(L, LUA_REGISTRYINDEX, g_IAP.m_Self);
        g_IAP.m_Callback = LUA_NOREF;
        g_IAP.m_Self = LUA_NOREF;
        g_IAP.m_L = 0;
    }
}

void IAPList_Callback(void* Lv, const char* result_json)
{
    lua_State* L = (lua_State*) Lv;
    if (g_IAP.m_Callback != LUA_NOREF) {
        int top = lua_gettop(L);
        int callback = g_IAP.m_Callback;
        lua_rawgeti(L, LUA_REGISTRYINDEX, callback);

        // Setup self
        lua_rawgeti(L, LUA_REGISTRYINDEX, g_IAP.m_Self);
        lua_pushvalue(L, -1);
        dmScript::SetInstance(L);

        if (!dmScript::IsInstanceValid(L))
        {
            dmLogError("Could not run iap facebook callback because the instance has been deleted.");
            lua_pop(L, 2);
            assert(top == lua_gettop(L));
            return;
        }
        if(result_json != 0)
        {
            dmJson::Document doc;
            dmJson::Result r = dmJson::Parse(result_json, &doc);
            if (r == dmJson::RESULT_OK && doc.m_NodeCount > 0) {
                dmScript::JsonToLua(L, &doc, 0);
                lua_pushnil(L);
            } else {
                dmLogError("Failed to parse list result JSON (%d)", r);
                lua_pushnil(L);
                IAP_PushError(L, "Failed to parse list result JSON", REASON_UNSPECIFIED);
            }
            dmJson::Free(&doc);
        }
        else
        {
            dmLogError("Got empty list result.");
            lua_pushnil(L);
            IAP_PushError(L, "Got empty list result.", REASON_UNSPECIFIED);
        }

        dmScript::PCall(L, 3, 0);
        assert(top == lua_gettop(L));
        dmScript::Unref(L, LUA_REGISTRYINDEX, callback);

        g_IAP.m_Callback = LUA_NOREF;
    } else {
        dmLogError("No callback set");
    }

}

int IAP_List(lua_State* L)
{
    int top = lua_gettop(L);
    VerifyCallback(L);

    char* buf = IAP_List_CreateBuffer(L);
    if( buf == 0 )
    {
        assert(top == lua_gettop(L));
        return 0;
    }

    luaL_checktype(L, 2, LUA_TFUNCTION);
    lua_pushvalue(L, 2);
    g_IAP.m_Callback = dmScript::Ref(L, LUA_REGISTRYINDEX);
    dmScript::GetInstance(L);
    g_IAP.m_Self = dmScript::Ref(L, LUA_REGISTRYINDEX);
    g_IAP.m_L = dmScript::GetMainThread(L);
    dmIAPFBList(buf, (OnIAPFBList)IAPList_Callback, g_IAP.m_L);

    free(buf);
    assert(top == lua_gettop(L));
    return 0;
}

void IAPListener_Callback(void* Lv, const char* result_json, int error_code)
{
    lua_State* L = g_IAP.m_Listener.m_L;
    int top = lua_gettop(L);
    if (g_IAP.m_Listener.m_Callback == LUA_NOREF) {
        dmLogError("No callback set");
        return;
    }
    lua_rawgeti(L, LUA_REGISTRYINDEX, g_IAP.m_Listener.m_Callback);

    // Setup self
    lua_rawgeti(L, LUA_REGISTRYINDEX, g_IAP.m_Listener.m_Self);
    lua_pushvalue(L, -1);
    dmScript::SetInstance(L);

    if (!dmScript::IsInstanceValid(L))
    {
        dmLogError("Could not run IAP callback because the instance has been deleted.");
        lua_pop(L, 2);
        assert(top == lua_gettop(L));
        return;
    }
    if (result_json) {
        dmJson::Document doc;
        dmJson::Result r = dmJson::Parse(result_json, &doc);
        if (r == dmJson::RESULT_OK && doc.m_NodeCount > 0) {
            dmScript::JsonToLua(L, &doc, 0);
            lua_pushnil(L);
        } else {
            dmLogError("Failed to parse purchase response (%d)", r);
            lua_pushnil(L);
            IAP_PushError(L, "failed to parse purchase response", REASON_UNSPECIFIED);
        }
        dmJson::Free(&doc);
    } else {
        lua_pushnil(L);
        switch(error_code)
        {
            case BILLING_RESPONSE_RESULT_USER_CANCELED:
                IAP_PushError(L, "user canceled purchase", REASON_USER_CANCELED);
                break;

            case BILLING_RESPONSE_RESULT_ITEM_ALREADY_OWNED:
                IAP_PushError(L, "product already owned", REASON_UNSPECIFIED);
                break;

            default:
                dmLogError("IAP error %d", error_code);
                IAP_PushError(L, "failed to buy product", REASON_UNSPECIFIED);
                break;
        }
    }
    dmScript::PCall(L, 3, 0);
    assert(top == lua_gettop(L));
}


int IAP_Buy(lua_State* L)
{
    if (g_IAP.m_Listener.m_Callback == LUA_NOREF) {
        dmLogError("No callback set");
        return 0;
    }
    int top = lua_gettop(L);
    const char* id = luaL_checkstring(L, 1);
    const char* request_id = 0x0;

    if (top >= 2 && lua_istable(L, 2)) {
        luaL_checktype(L, 2, LUA_TTABLE);
        lua_pushvalue(L, 2);
        lua_getfield(L, -1, "request_id");
        request_id = lua_isnil(L, -1) ? 0x0 : luaL_checkstring(L, -1);
        lua_pop(L, 2);
    }

    dmIAPFBBuy(id, request_id, (OnIAPFBListenerCallback)IAPListener_Callback, L);
    assert(top == lua_gettop(L));
    return 0;
}

int IAP_SetListener(lua_State* L)
{
    IAP* iap = &g_IAP;
    luaL_checktype(L, 1, LUA_TFUNCTION);
    lua_pushvalue(L, 1);
    int cb = dmScript::Ref(L, LUA_REGISTRYINDEX);

    if (iap->m_Listener.m_Callback != LUA_NOREF) {
        dmScript::Unref(iap->m_Listener.m_L, LUA_REGISTRYINDEX, iap->m_Listener.m_Callback);
        dmScript::Unref(iap->m_Listener.m_L, LUA_REGISTRYINDEX, iap->m_Listener.m_Self);
    }
    iap->m_Listener.m_L = dmScript::GetMainThread(L);
    iap->m_Listener.m_Callback = cb;
    dmScript::GetInstance(L);
    iap->m_Listener.m_Self = dmScript::Ref(L, LUA_REGISTRYINDEX);

    return 0;
}

int IAP_Finish(lua_State* L)
{
    return 0;
}

int IAP_Restore(lua_State* L)
{
    lua_pushboolean(L, 0);
    return 1;
}

int IAP_GetProviderId(lua_State* L)
{
    lua_pushinteger(L, PROVIDER_ID_FACEBOOK);
    return 1;
}

static const luaL_reg IAP_methods[] =
{
    {"list", IAP_List},
    {"buy", IAP_Buy},
    {"finish", IAP_Finish},
    {"restore", IAP_Restore},
    {"set_listener", IAP_SetListener},
    {"get_provider_id", IAP_GetProviderId},
    {0, 0}
};

dmExtension::Result InitializeIAP(dmExtension::Params* params)
{
    if (g_IAP.m_InitCount == 0) {
        g_IAP.m_autoFinishTransactions = dmConfigFile::GetInt(params->m_ConfigFile, "iap.auto_finish_transactions", 1) == 1;
    }
    g_IAP.m_InitCount++;
    lua_State*L = params->m_L;
    int top = lua_gettop(L);
    luaL_register(L, LIB_NAME, IAP_methods);

    IAP_PushConstants(L);

    lua_pop(L, 1);
    assert(top == lua_gettop(L));
    return dmExtension::RESULT_OK;
}

dmExtension::Result FinalizeIAP(dmExtension::Params* params)
{
    --g_IAP.m_InitCount;
    if (params->m_L == g_IAP.m_Listener.m_L && g_IAP.m_Listener.m_Callback != LUA_NOREF) {
        dmScript::Unref(g_IAP.m_Listener.m_L, LUA_REGISTRYINDEX, g_IAP.m_Listener.m_Callback);
        dmScript::Unref(g_IAP.m_Listener.m_L, LUA_REGISTRYINDEX, g_IAP.m_Listener.m_Self);
        g_IAP.m_Listener.m_L = 0;
        g_IAP.m_Listener.m_Callback = LUA_NOREF;
        g_IAP.m_Listener.m_Self = LUA_NOREF;
    }
    return dmExtension::RESULT_OK;
}

DM_DECLARE_EXTENSION(IAPExt, "IAP", 0, 0, InitializeIAP, 0, 0, FinalizeIAP)
