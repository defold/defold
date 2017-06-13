#include <dlib/log.h>
#include <script/script.h>

#include "gameroom.h"


struct GameroomIAPListener
{
    GameroomIAPListener()
    {
        m_L = 0;
        m_Callback = LUA_NOREF;
        m_Self = LUA_NOREF;
    }
    lua_State* m_L;
    int        m_Callback;
    int        m_Self;
} g_GameroomIAP;

/*
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
*/

int GameroomIAP_List(lua_State* L)
{
    // not supported?
    return 0;
}

void GameroomIAP_PushError(lua_State* L, const char* error, int reason)
{
    if (error != 0) {
        lua_newtable(L);
        lua_pushstring(L, "error");
        lua_pushstring(L, error);
        lua_rawset(L, -3);
        lua_pushstring(L, "reason");
        lua_pushnumber(L, reason);
        lua_rawset(L, -3);
    } else {
        lua_pushnil(L);
    }
}

void GameroomIAP_ErrorCallback(int error_code)
{
    /*
    lua_State* L = g_GameroomIAP.m_L;
    int top = lua_gettop(L);
    if (g_GameroomIAP.m_Callback == LUA_NOREF) {
        dmLogError("No callback set");
        return;
    }
    lua_rawgeti(L, LUA_REGISTRYINDEX, g_GameroomIAP.m_Callback);

    // Setup self
    lua_rawgeti(L, LUA_REGISTRYINDEX, g_GameroomIAP.m_Self);
    lua_pushvalue(L, -1);
    dmScript::SetInstance(L);

    if (!dmScript::IsInstanceValid(L))
    {
        dmLogError("Could not run IAP callback because the instance has been deleted.");
        lua_pop(L, 2);
        assert(top == lua_gettop(L));
        return;
    }
    lua_pushnil(L);
    switch(error_code)
    {
        case BILLING_RESPONSE_RESULT_USER_CANCELED:
            GameroomIAP_PushError(L, "user canceled purchase", REASON_USER_CANCELED);
            break;

        case BILLING_RESPONSE_RESULT_ITEM_ALREADY_OWNED:
            GameroomIAP_PushError(L, "product already owned", REASON_UNSPECIFIED);
            break;

        default:
            dmLogError("IAP error %d", error_code);
            GameroomIAP_PushError(L, "failed to buy product", REASON_UNSPECIFIED);
            break;
    }
    dmScript::PCall(L, 3, LUA_MULTRET);
    assert(top == lua_gettop(L));
    */
}

bool GameroomIAP_Listener_Callback(int data_table_index)
{
    lua_State* L = g_GameroomIAP.m_L;
    int top = lua_gettop(L);
    if (g_GameroomIAP.m_Callback == LUA_NOREF) {
        dmLogError("No callback set");
        return false;
    }
    lua_rawgeti(L, LUA_REGISTRYINDEX, g_GameroomIAP.m_Callback);

    // Setup self
    lua_rawgeti(L, LUA_REGISTRYINDEX, g_GameroomIAP.m_Self);
    lua_pushvalue(L, -1);
    dmScript::SetInstance(L);

    if (!dmScript::IsInstanceValid(L))
    {
        dmLogError("Could not run IAP callback because the instance has been deleted.");
        lua_pop(L, 2);
        dmLogError("lua_gettop(L): %u", lua_gettop(L));
        assert(top == lua_gettop(L));
        return false;
    }
    lua_pushvalue(L, data_table_index);
    dmScript::PCall(L, 2, LUA_MULTRET);

    assert(top == lua_gettop(L));
    return true;
}

int GameroomIAP_Buy(lua_State* L)
{
    if (g_GameroomIAP.m_Callback == LUA_NOREF) {
        dmLogError("No callback set");
        return 0;
    }
    int top = lua_gettop(L);
    const char* product_id = luaL_checkstring(L, 1);

    int option_quantity = 0;
    int option_quantity_min = 0;
    int option_quantity_max = 0;
    const char* option_request_id = 0x0;
    const char* option_price_point_id = 0x0;
    const char* option_test_currency = 0x0;

    if (top >= 2 && lua_istable(L, 2)) {
        luaL_checktype(L, 2, LUA_TTABLE);
        lua_pushvalue(L, 2);

        lua_getfield(L, -1, "quantity");
        option_quantity = lua_isnil(L, -1) ? option_quantity : luaL_checknumber(L, -1);
        lua_pop(L, 1);

        lua_getfield(L, -1, "quantity_min");
        option_quantity_min = lua_isnil(L, -1) ? option_quantity_min : luaL_checknumber(L, -1);
        lua_pop(L, 1);

        lua_getfield(L, -1, "quantity_max");
        option_quantity_max = lua_isnil(L, -1) ? option_quantity_max : luaL_checknumber(L, -1);
        lua_pop(L, 1);

        lua_getfield(L, -1, "request_id");
        option_request_id = lua_isnil(L, -1) ? option_request_id : luaL_checkstring(L, -1);
        lua_pop(L, 1);

        lua_getfield(L, -1, "price_point_id");
        option_price_point_id = lua_isnil(L, -1) ? option_price_point_id : luaL_checkstring(L, -1);
        lua_pop(L, 1);

        lua_getfield(L, -1, "test_currency");
        option_test_currency = lua_isnil(L, -1) ? option_test_currency : luaL_checkstring(L, -1);
        lua_pop(L, 1);

        lua_pop(L, 1); // drop copy of table reference
    }

    fbg_PurchaseIAP(
        product_id,
        option_quantity,
        option_quantity_min,
        option_quantity_max,
        option_request_id,
        option_price_point_id,
        option_test_currency
    );

    assert(top == lua_gettop(L));

    return 0;
}

int GameroomIAP_SetListener(lua_State* L)
{
    luaL_checktype(L, 1, LUA_TFUNCTION);
    lua_pushvalue(L, 1);
    int cb = dmScript::Ref(L, LUA_REGISTRYINDEX);

    if (g_GameroomIAP.m_Callback != LUA_NOREF) {
        dmScript::Unref(g_GameroomIAP.m_L, LUA_REGISTRYINDEX, g_GameroomIAP.m_Callback);
        dmScript::Unref(g_GameroomIAP.m_L, LUA_REGISTRYINDEX, g_GameroomIAP.m_Self);
    }
    g_GameroomIAP.m_L = dmScript::GetMainThread(L);
    g_GameroomIAP.m_Callback = cb;
    dmScript::GetInstance(L);
    g_GameroomIAP.m_Self = dmScript::Ref(L, LUA_REGISTRYINDEX);
    return 0;
}

int GameroomIAP_Finish(lua_State* L)
{
    return 0;
}

int GameroomIAP_Restore(lua_State* L)
{
    lua_pushboolean(L, 0);
    return 1;
}

int GameroomIAP_GetProviderId(lua_State* L)
{
    lua_pushstring(L, "GAMEROOM");
    //lua_pushinteger(L, PROVIDER_ID_FACEBOOK);
    return 1;
}

int GameroomIAP_BuyPremium(lua_State* L)
{
    return 0;
}


int GameroomIAP_HasPremium(lua_State* L)
{
    fbg_HasLicense();
    return 0;
}

static const luaL_reg GameroomIAP_methods[] =
{
    {"list", GameroomIAP_List},
    {"buy", GameroomIAP_Buy},
    {"finish", GameroomIAP_Finish},
    {"restore", GameroomIAP_Restore},
    {"set_listener", GameroomIAP_SetListener},
    {"get_provider_id", GameroomIAP_GetProviderId},
    {"buy_premium", GameroomIAP_BuyPremium},
    {"has_premium", GameroomIAP_HasPremium},
    {0, 0}
};

void RegisterGameroomIAP(lua_State* L)
{
    luaL_register(L, "iap", GameroomIAP_methods);
    lua_pop(L, 1);
}

typedef size_t (*purchase_field_cb) (const fbgPurchaseHandle, char*, size_t);

static size_t SetPurchaseField(lua_State* L, const char* field_name, char* scratch_buffer, size_t& scratch_buffer_size, const fbgPurchaseHandle purchase_handle, purchase_field_cb cb)
{
    size_t res = cb(purchase_handle, scratch_buffer, scratch_buffer_size);
    if (res == 0) {
        return 0;
    }

    if (res > scratch_buffer_size) {
        free(scratch_buffer);
        scratch_buffer_size = res;
        scratch_buffer = (char*)malloc(sizeof(char) * scratch_buffer_size);
        res = cb(purchase_handle, scratch_buffer, scratch_buffer_size);
    }

    lua_pushstring(L, scratch_buffer);
    lua_setfield(L, -2, field_name);

    return res;
}

void InitGameroomIAP(dmExtension::AppParams* params)
{
    // Nothing yet here.
}

void HandleGameroomIAPMessages(lua_State* L, fbgMessageHandle message, fbgMessageType message_type)
{
    switch (message_type)
    {
        case fbgMessage_Purchase: {

            lua_State* L = g_GameroomIAP.m_L;
            int top = lua_gettop(L);

            auto payHandle = fbg_Message_Purchase(message);

            uint64_t errorCode = fbg_Purchase_GetErrorCode(payHandle);
            dmLogError("errorCode: %llu", errorCode);

            uint32_t amount = fbg_Purchase_GetAmount(payHandle);
            uint64_t purchaseTime = fbg_Purchase_GetPurchaseTime(payHandle);
            uint32_t quantity = fbg_Purchase_GetQuantity(payHandle);

            size_t scratch_buffer_size = 1024;
            char* scratch_buffer = (char*)malloc(sizeof(char) * scratch_buffer_size);

            lua_newtable(L);
            SetPurchaseField(L, "payment_id", scratch_buffer, scratch_buffer_size, payHandle, fbg_Purchase_GetPaymentID);
            SetPurchaseField(L, "currency", scratch_buffer, scratch_buffer_size, payHandle, fbg_Purchase_GetCurrency);
            SetPurchaseField(L, "product_id", scratch_buffer, scratch_buffer_size, payHandle, fbg_Purchase_GetProductId);
            SetPurchaseField(L, "purchase_token", scratch_buffer, scratch_buffer_size, payHandle, fbg_Purchase_GetPurchaseToken);
            SetPurchaseField(L, "request_id", scratch_buffer, scratch_buffer_size, payHandle, fbg_Purchase_GetRequestId);
            SetPurchaseField(L, "status", scratch_buffer, scratch_buffer_size, payHandle, fbg_Purchase_GetStatus);
            SetPurchaseField(L, "signed_request", scratch_buffer, scratch_buffer_size, payHandle, fbg_Purchase_GetSignedRequest);
            SetPurchaseField(L, "error_message", scratch_buffer, scratch_buffer_size, payHandle, fbg_Purchase_GetErrorMessage);
            free(scratch_buffer);



            /*
            dmLogError(
                "Purchase Handle: %s\nAmount: %d\nCurrency: %s\nPurchase Time: %lld\n"
                "Product Id:%s\nPurchase Token: %s\nQuantity: %d\nRequest Id: %s\n"
                "Status: %s\nSignedRequest: %s\nError Code: %lld\nErrorMessage: %s",
                paymentId,
                (int)amount,
                currency,
                (long long)purchaseTime,
                productId,
                purchaseToken,
                (int)quantity,
                requestId,
                status,
                signedRequest,
                (long long)errorCode,
                errorMessage
            );
            */

            GameroomIAP_Listener_Callback(top+1);
            lua_pop(L, 1);
            assert(top == lua_gettop(L));
        }
        break;
    }
}
