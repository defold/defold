#include <stdlib.h>
#include <unistd.h>
#include <dlib/array.h>
#include <dlib/log.h>
#include <dlib/dstrings.h>
#include <dlib/json.h>
#include <script/script.h>
#include <extension/extension.h>
#include "iap.h"

#define LIB_NAME "iap"
struct IAP;

struct IAPListener
{
    IAPListener()
    {
        m_L = 0;
        m_Callback = LUA_NOREF;
        m_Self = LUA_NOREF;
    }
    lua_State* m_L;
    int        m_Callback;
    int        m_Self;
};

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
    void dmIAPFBBuy(const char* item_id, OnIAPFBListenerCallback callback, lua_State* L);
}

// NOTE: Copy-paste from script_json
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

static void PushError(lua_State*L, const char* error, int reason)
{
    if (error != 0) {
        lua_newtable(L);
        lua_pushstring(L, "error");
        lua_pushstring(L, error);
        lua_rawset(L, -3);
        lua_pushstring(L, "reason");
        lua_pushinteger(L, reason);
        lua_rawset(L, -3);
    } else {
        lua_pushnil(L);
    }
}

static void VerifyCallback(lua_State* L)
{
    if (g_IAP.m_Callback != LUA_NOREF) {
        dmLogError("Unexpected callback set");
        luaL_unref(L, LUA_REGISTRYINDEX, g_IAP.m_Callback);
        luaL_unref(L, LUA_REGISTRYINDEX, g_IAP.m_Self);
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
                ToLua((lua_State *)L, &doc, 0);
                lua_pushnil(L);
            } else {
                dmLogError("Failed to parse list result JSON (%d)", r);
                lua_pushnil(L);
                PushError(L, "Failed to parse list result JSON", REASON_UNSPECIFIED);
            }
            dmJson::Free(&doc);
        }
        else
        {
            dmLogError("Got empty list result.");
            lua_pushnil(L);
            PushError(L, "Got empty list result.", REASON_UNSPECIFIED);
        }

        dmScript::PCall(L, 3, LUA_MULTRET);
        assert(top == lua_gettop(L));
        luaL_unref(L, LUA_REGISTRYINDEX, callback);

        g_IAP.m_Callback = LUA_NOREF;
    } else {
        dmLogError("No callback set");
    }

}

int IAP_List(lua_State* L)
{
    int top = lua_gettop(L);
    VerifyCallback(L);

    char buf[1024];
    buf[0] = '\0';
    int i = 0;
    lua_pushnil(L);
    while (lua_next(L, 1) != 0) {
        if (i > 0) {
            dmStrlCat(buf, ",", sizeof(buf));
        }
        const char* p = luaL_checkstring(L, -1);
        dmStrlCat(buf, p, sizeof(buf));
        lua_pop(L, 1);
        ++i;
    }
    luaL_checktype(L, 2, LUA_TFUNCTION);
    lua_pushvalue(L, 2);
    g_IAP.m_Callback = luaL_ref(L, LUA_REGISTRYINDEX);
    dmScript::GetInstance(L);
    g_IAP.m_Self = luaL_ref(L, LUA_REGISTRYINDEX);
    g_IAP.m_L = dmScript::GetMainThread(L);
    dmIAPFBList(buf, (OnIAPFBList)IAPList_Callback, g_IAP.m_L);

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
            ToLua(L, &doc, 0);
            lua_pushnil(L);
        } else {
            dmLogError("Failed to parse purchase response (%d)", r);
            lua_pushnil(L);
            PushError(L, "failed to parse purchase response", REASON_UNSPECIFIED);
        }
        dmJson::Free(&doc);
    } else {
        lua_pushnil(L);
        switch(error_code)
        {
            case BILLING_RESPONSE_RESULT_USER_CANCELED:
                PushError(L, "user canceled purchase", REASON_USER_CANCELED);
            break;

            case BILLING_RESPONSE_RESULT_ITEM_ALREADY_OWNED:
                PushError(L, "product already owned", REASON_UNSPECIFIED);
            break;

            default:
                dmLogError("IAP error %d", error_code);
                PushError(L, "failed to buy product", REASON_UNSPECIFIED);
            break;
        }
    }
    dmScript::PCall(L, 3, LUA_MULTRET);
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
    dmIAPFBBuy(id, (OnIAPFBListenerCallback)IAPListener_Callback, L);
    assert(top == lua_gettop(L));
    return 0;
}

int IAP_SetListener(lua_State* L)
{
    IAP* iap = &g_IAP;
    luaL_checktype(L, 1, LUA_TFUNCTION);
    lua_pushvalue(L, 1);
    int cb = luaL_ref(L, LUA_REGISTRYINDEX);

    if (iap->m_Listener.m_Callback != LUA_NOREF) {
        luaL_unref(iap->m_Listener.m_L, LUA_REGISTRYINDEX, iap->m_Listener.m_Callback);
        luaL_unref(iap->m_Listener.m_L, LUA_REGISTRYINDEX, iap->m_Listener.m_Self);
    }
    iap->m_Listener.m_L = dmScript::GetMainThread(L);
    iap->m_Listener.m_Callback = cb;
    dmScript::GetInstance(L);
    iap->m_Listener.m_Self = luaL_ref(L, LUA_REGISTRYINDEX);

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

int IAP_GetStoreId(lua_State* L)
{
    lua_pushinteger(L, STORE_ID_FACEBOOK);
    return 1;
}

static const luaL_reg IAP_methods[] =
{
    {"list", IAP_List},
    {"buy", IAP_Buy},
    {"finish", IAP_Finish},
    {"restore", IAP_Restore},
    {"set_listener", IAP_SetListener},
    {"get_store_id", IAP_GetStoreId},
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

#define SETCONSTANT(name) \
        lua_pushnumber(L, (lua_Number) name); \
        lua_setfield(L, -2, #name);\

    SETCONSTANT(TRANS_STATE_PURCHASING)
    SETCONSTANT(TRANS_STATE_PURCHASED)
    SETCONSTANT(TRANS_STATE_FAILED)
    SETCONSTANT(TRANS_STATE_RESTORED)
    SETCONSTANT(TRANS_STATE_UNVERIFIED)

    SETCONSTANT(REASON_UNSPECIFIED)
    SETCONSTANT(REASON_USER_CANCELED)

    SETCONSTANT(STORE_ID_GOOGLE)
    SETCONSTANT(STORE_ID_AMAZON)
    SETCONSTANT(STORE_ID_APPLE)
    SETCONSTANT(STORE_ID_FACEBOOK)

#undef SETCONSTANT

    lua_pop(L, 1);
    assert(top == lua_gettop(L));
    return dmExtension::RESULT_OK;
}

dmExtension::Result FinalizeIAP(dmExtension::Params* params)
{
    --g_IAP.m_InitCount;
    if (params->m_L == g_IAP.m_Listener.m_L && g_IAP.m_Listener.m_Callback != LUA_NOREF) {
        luaL_unref(g_IAP.m_Listener.m_L, LUA_REGISTRYINDEX, g_IAP.m_Listener.m_Callback);
        luaL_unref(g_IAP.m_Listener.m_L, LUA_REGISTRYINDEX, g_IAP.m_Listener.m_Self);
        g_IAP.m_Listener.m_L = 0;
        g_IAP.m_Listener.m_Callback = LUA_NOREF;
        g_IAP.m_Listener.m_Self = LUA_NOREF;
    }
    return dmExtension::RESULT_OK;
}

DM_DECLARE_EXTENSION(IAPExt, "IAP", 0, 0, InitializeIAP, 0, 0, FinalizeIAP)
