#include "iap.h"
#include "iap_private.h"

#include <dlib/dstrings.h>
#include <dlib/dstrings.h>
#include <dlib/log.h>
#include <script/script.h>
#include <extension/extension.h>
#include <gameroom/gameroom.h>

#define LIB_NAME "iap"

extern unsigned char IAP_GAMEROOM_LUA[];
extern uint32_t IAP_GAMEROOM_LUA_SIZE;

// Struct that holds results from a purchase transaction
struct PendingTransaction
{
    ~PendingTransaction()
    {
       free(m_PaymentId);
       free(m_Currency);
       free(m_ProductId);
       free(m_PurchaseToken);
       free(m_RequestId);
       free(m_Status);
       free(m_SignedRequest);
       free(m_ErrorMessage);
    }
    char*    m_PaymentId;
    char*    m_Currency;
    char*    m_ProductId;
    char*    m_PurchaseToken;
    char*    m_RequestId;
    char*    m_Status;
    char*    m_SignedRequest;
    char*    m_ErrorMessage;
    uint32_t m_Amount;
    uint64_t m_PurchaseTime;
    uint32_t m_Quantity;
    uint64_t m_ErrorCode;
};

// We use two different Lua callbacks; list and has_license
// This structs holds the relevant parts for a Lua callback
// we then create two instances of this struct inside the
// global IAP state struct below.
struct IAPCallback
{
    IAPCallback() {
        memset(this, 0, sizeof(*this));
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
        m_Listener.m_Callback = LUA_NOREF;
        m_Listener.m_Self = LUA_NOREF;
        m_PendingTransactions.SetCapacity(4);
    }

    IAPCallback                  m_ListCallback;
    IAPCallback                  m_LicenseCallback;

    IAPListener                  m_Listener;
    dmArray<PendingTransaction*> m_PendingTransactions;

} g_IAP;

////////////////////////////////////////////////////////////////////////////////
// Aux and callback checking functions
//

static void SetIAPCallback(lua_State* L, IAPCallback& callback, int index)
{
    lua_pushvalue(L, index);
    callback.m_Callback = dmScript::Ref(L, LUA_REGISTRYINDEX);

    dmScript::GetInstance(L);
    callback.m_Self = dmScript::Ref(L, LUA_REGISTRYINDEX);
    callback.m_L = dmScript::GetMainThread(L);
}

static void SetIAPListener(lua_State* L, int index)
{
    lua_pushvalue(L, index);
    g_IAP.m_Listener.m_Callback = dmScript::Ref(L, LUA_REGISTRYINDEX);

    dmScript::GetInstance(L);
    g_IAP.m_Listener.m_Self = dmScript::Ref(L, LUA_REGISTRYINDEX);
    g_IAP.m_Listener.m_L = dmScript::GetMainThread(L);
}

static void ClearIAPCallback(lua_State* L, IAPCallback& callback)
{
    dmScript::Unref(L, LUA_REGISTRYINDEX, callback.m_Callback);
    dmScript::Unref(L, LUA_REGISTRYINDEX, callback.m_Self);
    callback.m_Callback = LUA_NOREF;
    callback.m_Self = LUA_NOREF;
    callback.m_L = NULL;
}

static bool SetupIAPCallback(lua_State* L, IAPCallback& callback)
{
    lua_rawgeti(L, LUA_REGISTRYINDEX, callback.m_Callback);
    lua_rawgeti(L, LUA_REGISTRYINDEX, callback.m_Self);
    lua_pushvalue(L, -1);
    dmScript::SetInstance(L);

    if (!dmScript::IsInstanceValid(L))
    {
        dmLogError("Could not run IAP callback because the instance has been deleted.");
        lua_pop(L, 2);
        return false;
    }

    return true;
}

static bool SetupIAPListener(lua_State* L)
{
    lua_rawgeti(L, LUA_REGISTRYINDEX, g_IAP.m_Listener.m_Callback);
    lua_rawgeti(L, LUA_REGISTRYINDEX, g_IAP.m_Listener.m_Self);
    lua_pushvalue(L, -1);
    dmScript::SetInstance(L);

    if (!dmScript::IsInstanceValid(L))
    {
        dmLogError("Could not run IAP listener callback because the instance has been deleted.");
        lua_pop(L, 2);
        return false;
    }

    return true;
}

static bool HasIAPCallback(IAPCallback& callback)
{
    if (callback.m_Callback == LUA_NOREF ||
        callback.m_Self == LUA_NOREF ||
        callback.m_L == NULL) {
        return false;
    }
    return true;
}

static bool HasIAPListener()
{
    if (g_IAP.m_Listener.m_Callback == LUA_NOREF ||
        g_IAP.m_Listener.m_Self == LUA_NOREF ||
        g_IAP.m_Listener.m_L == NULL) {
        return false;
    }
    return true;
}

////////////////////////////////////////////////////////////////////////////////
// Functions for running callbacks; license and buy/purchases
//

static void ProcessTransaction(PendingTransaction* transaction)
{
    lua_State* L = g_IAP.m_Listener.m_L;
    DM_LUA_STACK_CHECK(L, 0);

    SetupIAPListener(L);

    lua_newtable(L); // Result argument

    int state = TRANS_STATE_FAILED;
    if (strcmp(transaction->m_Status, "initiated") == 0)
    {
        state = TRANS_STATE_UNVERIFIED;
    } else if (strcmp(transaction->m_Status, "completed") == 0)
    {
        state = TRANS_STATE_PURCHASED;
    }

    if (transaction->m_ErrorCode == 0)
    {
        // Check purchase state
        if (state != TRANS_STATE_FAILED)
        {
            // Default IAP API purchase response
            lua_pushstring(L, transaction->m_ProductId);
            lua_setfield(L, -2, "ident");
            lua_pushinteger(L, state);
            lua_setfield(L, -2, "state");
            lua_pushstring(L, transaction->m_PaymentId);
            lua_setfield(L, -2, "trans_ident");
            lua_pushinteger(L, transaction->m_PurchaseTime);
            lua_setfield(L, -2, "date");
            lua_pushstring(L, transaction->m_SignedRequest);
            lua_setfield(L, -2, "receipt");
            lua_pushstring(L, transaction->m_RequestId);
            lua_setfield(L, -2, "request_id");

            // Extenden fields, platform specifics
            lua_pushstring(L, transaction->m_Currency);
            lua_setfield(L, -2, "currency");
            lua_pushstring(L, transaction->m_PurchaseToken);
            lua_setfield(L, -2, "purchase_token");
            lua_pushinteger(L, transaction->m_Amount);
            lua_setfield(L, -2, "amount");
            lua_pushinteger(L, transaction->m_Quantity);
            lua_setfield(L, -2, "quantity");

            lua_pushnil(L); // Error argument

        } else {
            // Failed or unknown status
            lua_newtable(L);

            if (strcmp(transaction->m_Status, "failed") == 0)
            {
                lua_pushstring(L, "Purchase failed.");
                lua_setfield(L, -2, "reason");
            } else {
                lua_pushfstring(L, "Unknown purchase status: %s.", transaction->m_Status);
                lua_setfield(L, -2, "reason");
            }
            lua_pushnumber(L, TRANS_STATE_FAILED);
            lua_setfield(L, -2, "error");
        }

    } else {
        lua_newtable(L);
        lua_pushnumber(L, TRANS_STATE_FAILED);
        lua_setfield(L, -2, "error");
        lua_pushstring(L, transaction->m_ErrorMessage);
        lua_setfield(L, -2, "reason");
    }

    dmScript::PCall(L, 3, 0);
}

static void RunLicenseCallback(lua_State* L, bool has_license)
{
    DM_LUA_STACK_CHECK(L, 0);

    if (!HasIAPCallback(g_IAP.m_LicenseCallback)) {
        dmLogError("No callback set for has license callback.");
        return;
    }

    if (SetupIAPCallback(L, g_IAP.m_LicenseCallback))
    {
        lua_pushboolean(L, has_license);
        dmScript::PCall(L, 2, 0);
    }

    ClearIAPCallback(L, g_IAP.m_LicenseCallback);
}

// Does a shallow copy of a Lua table where the values needs to be strings.
// Used by IAP_List to create a copy of the products table so any modifications
// to the originally supplied products list is not changed during execution of
// the internal iap.__facebook_helper_list() function.
static bool IAP_List_CopyTable(lua_State* L, int from, int to)
{
    lua_pushnil(L);
    int i = 1;
    while(lua_next(L, from) != 0) {
        if (lua_type(L, -1) != LUA_TSTRING) {
            lua_pop(L, 1);
            return false;
        }

        lua_pushnumber(L, i++);
        lua_pushstring(L, lua_tostring(L, -2));
        lua_settable(L, to);

        lua_pop(L, 1);
    }

    return true;
}

static int IAP_List_WrapperCB(lua_State* L)
{
    DM_LUA_STACK_CHECK(L, 0);

    if (!HasIAPCallback(g_IAP.m_ListCallback))
    {
        dmLogError("Got iap.list result but no callback set.");
        return 0;
    }

    int top = lua_gettop(L);
    if (top < 2) {
        // Invalid result from iap.__facebook_helper_list.
        if (SetupIAPCallback(L, g_IAP.m_ListCallback)) {
            lua_pushnil(L);
            IAP_PushError(L, "Invalid result in iap.list.", REASON_UNSPECIFIED);
            dmScript::PCall(L, 3, 0);
        }

        ClearIAPCallback(L, g_IAP.m_ListCallback);
        return 0;
    }

    if (SetupIAPCallback(L, g_IAP.m_ListCallback)) {
        lua_pushvalue(L, 1); // copy of result table
        lua_pushvalue(L, 2); // copy of error table
        dmScript::PCall(L, 3, 0);
    }

    // Clear IAP callback info so iap.list can be called once again.
    ClearIAPCallback(L, g_IAP.m_ListCallback);

    return 0;
}

static int IAP_List(lua_State* L)
{
    DM_LUA_STACK_CHECK(L, 0);

    if (HasIAPCallback(g_IAP.m_ListCallback))
    {
        dmLogError("Unexpected callback set");
        return 0;
    }

    luaL_checktype(L, 1, LUA_TTABLE);
    luaL_checktype(L, 2, LUA_TFUNCTION);
    SetIAPCallback(L, g_IAP.m_ListCallback, 2);

    // Get internal function to get and parse products (defined in iap_gameroom.lua).
    lua_getglobal(L, "iap");
    lua_getfield(L, -1, "__facebook_helper_list");

    lua_newtable(L);
    if (!IAP_List_CopyTable(L, 1, lua_gettop(L))) {
        lua_pop(L, -3); // pop "iap" table, urls table and internal func
        ClearIAPCallback(L, g_IAP.m_ListCallback);
        // We were unable to copy urls table, throw Lua error
        luaL_error(L, "Could not create a copy of products table. Verify that every entry is a string.");
        return 0;
    }

    // Push wrapper callback
    lua_pushcfunction(L, IAP_List_WrapperCB);

    int ret = dmScript::PCall(L, 2, 0);
    if (ret != 0)
    {
        dmLogError("Error while running iap.__facebook_helper_list: %s", lua_tostring(L, -1));
        lua_pop(L, 2); // pop "iap" table + error string
    }
    lua_pop(L, 1); // pop "iap" table

    return 0;
}

static const char* GetTableStringValue(lua_State* L, int table_index, const char* key, const char* default_value)
{
    const char* r = default_value;

    lua_getfield(L, table_index, key);
    if (!lua_isnil(L, -1)) {

        int actual_lua_type = lua_type(L, -1);
        if (actual_lua_type != LUA_TSTRING) {
            dmLogError("Lua conversion expected entry '%s' to be a string but got %s",
                key, lua_typename(L, actual_lua_type));
        } else {
            r = lua_tostring(L, -1);
        }

    }
    lua_pop(L, 1);

    return r;
}

static int GetTableIntValue(lua_State* L, int table_index, const char* key, int default_value)
{
    int r = default_value;

    lua_getfield(L, table_index, key);
    if (!lua_isnil(L, -1)) {

        int actual_lua_type = lua_type(L, -1);
        if (actual_lua_type != LUA_TNUMBER) {
            dmLogError("Lua conversion expected entry '%s' to be a number but got %s",
                key, lua_typename(L, actual_lua_type));
        } else {
            r = lua_tointeger(L, -1);
        }

    }
    lua_pop(L, 1);

    return r;
}

static int IAP_Buy(lua_State* L)
{
    DM_LUA_STACK_CHECK(L, 0);

    if (!dmFBGameroom::CheckGameroomInit()) {
        return 0;
    }

    const char* product_id = luaL_checkstring(L, 1);

    // Figure out if this is a FB Purchases Lite or regular call
    // by checking if the product starts with "http".
    typedef fbgRequest (*pruchase_func_t)(const char*, uint32_t, uint32_t, uint32_t, const char*, const char*, const char*);
    pruchase_func_t purchase_func = fbg_PurchaseIAP;
    if (strncmp("http", product_id, 4) == 0) {
        purchase_func = fbg_PurchaseIAPWithProductURL;
    }

    if (lua_gettop(L) >= 2) {
        luaL_checktype(L, 2, LUA_TTABLE);
        purchase_func(
            product_id,
            GetTableIntValue(L, 2, "quantity", 0),
            GetTableIntValue(L, 2, "quantity_min", 0),
            GetTableIntValue(L, 2, "quantity_max", 0),
            GetTableStringValue(L, 2, "request_id", NULL),
            GetTableStringValue(L, 2, "price_point_id", NULL),
            GetTableStringValue(L, 2, "test_currency", NULL)
        );
    } else {
        purchase_func( product_id, 0, 0, 0, NULL, NULL, NULL );
    }

    return 0;
}

static int IAP_HasLicense(lua_State* L)
{
    DM_LUA_STACK_CHECK(L, 0);

    if (!dmFBGameroom::CheckGameroomInit()) {
        return 0;
    }

    if (HasIAPCallback(g_IAP.m_LicenseCallback)) {
        dmLogError("Unexpected callback set");
        return 0;
    }

    luaL_checktype(L, 1, LUA_TFUNCTION);
    SetIAPCallback(L, g_IAP.m_LicenseCallback, 1);

    fbg_HasLicense();

    return 0;
}

static int IAP_BuyLicense(lua_State* L)
{
    DM_LUA_STACK_CHECK(L, 0);

    if (!dmFBGameroom::CheckGameroomInit()) {
        return 0;
    }

    fbg_PayPremium();
    return 0;
}

static int IAP_Finish(lua_State* L)
{
    return 0;
}

// Not available through FB Gameroom
static int IAP_Restore(lua_State* L)
{
    lua_pushboolean(L, 0);
    return 0;
}

static int IAP_SetListener(lua_State* L)
{
    IAP* iap = &g_IAP;
    luaL_checktype(L, 1, LUA_TFUNCTION);

    SetIAPListener(L, 1);

    // On first set listener, trigger process old ones.
    if (!g_IAP.m_PendingTransactions.Empty()) {
        for (int i = 0; i < g_IAP.m_PendingTransactions.Size(); ++i)
        {
            ProcessTransaction(g_IAP.m_PendingTransactions[i]);
            delete g_IAP.m_PendingTransactions[i];
        }
        g_IAP.m_PendingTransactions.SetSize(0);
    }
    return 0;
}

static int IAP_GetProviderId(lua_State* L)
{
    lua_pushinteger(L, PROVIDER_ID_GAMEROOM);
    return 1;
}

static const luaL_reg IAP_methods[] =
{
    {"list", IAP_List},
    {"buy", IAP_Buy},
    {"has_license", IAP_HasLicense},
    {"buy_license", IAP_BuyLicense},
    {"finish", IAP_Finish},
    {"restore", IAP_Restore},
    {"set_listener", IAP_SetListener},
    {"get_provider_id", IAP_GetProviderId},
    {0, 0}
};

static dmExtension::Result InitializeIAP(dmExtension::Params* params)
{
    // Only continue if Gameroom is specified as a iap_provider under Windows.
    const char* iap_provider = dmConfigFile::GetString(params->m_ConfigFile, "windows.iap_provider", 0);
    if (iap_provider != 0x0 && strcmp(iap_provider, "Gameroom") == 0)
    {
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

        SETCONSTANT(PROVIDER_ID_GOOGLE)
        SETCONSTANT(PROVIDER_ID_AMAZON)
        SETCONSTANT(PROVIDER_ID_APPLE)
        SETCONSTANT(PROVIDER_ID_FACEBOOK)
        SETCONSTANT(PROVIDER_ID_GAMEROOM)

#undef SETCONSTANT

        lua_pop(L, 1);

        // Load iap_gameroom.lua which adds __facebook_helper_list to the iap table.
        if (luaL_loadbuffer(L, (const char*)IAP_GAMEROOM_LUA, IAP_GAMEROOM_LUA_SIZE, "(internal) iap_gameroom.lua") != 0)
        {
            dmLogError("Could not load iap_gameroom.lua: %s", lua_tolstring(L, -1, 0));
        }
        else
        {
            int ret = dmScript::PCall(L, 0, 0);
            if (ret != 0)
            {
                dmLogError("Error while running iap_gameroom.lua: %s", lua_tostring(L, -1));
                lua_pop(L, 1);
            }
        }

        assert(top == lua_gettop(L));
    }

    return dmExtension::RESULT_OK;
}

typedef size_t (*purchase_safe_str_t)(const fbgPurchaseHandle, char*, size_t);

// Helper function to check size needed and malloc strings for FBG API calls that fills strings.
// Used by UpdateIAP for fbgMessage_Purchase messages.
static char* GetSafeStr(const fbgPurchaseHandle handle, purchase_safe_str_t func)
{
    size_t size_needed = func(handle, 0, 0) + 1; // +1 for \0
    char* str = (char*)malloc(size_needed);
    func(handle, str, size_needed);
    return str;
}

static dmExtension::Result UpdateIAP(dmExtension::Params* params)
{
    if (!dmFBGameroom::CheckGameroomInit())
    {
        return dmExtension::RESULT_OK;
    }

    lua_State* L = params->m_L;

    fbgMessageHandle message;
    while ((message = dmFBGameroom::PopIAPMessage()) != NULL) {
        fbgMessageType message_type = fbg_Message_GetType(message);
        switch (message_type) {
            case fbgMessage_Purchase:
            {
                fbgPurchaseHandle purchase_handle = fbg_Message_Purchase(message);

                PendingTransaction* transaction = new PendingTransaction();
                transaction->m_PaymentId = GetSafeStr(purchase_handle, fbg_Purchase_GetPaymentID);
                transaction->m_Currency = GetSafeStr(purchase_handle, fbg_Purchase_GetCurrency);
                transaction->m_ProductId = GetSafeStr(purchase_handle, fbg_Purchase_GetProductId);
                transaction->m_PurchaseToken = GetSafeStr(purchase_handle, fbg_Purchase_GetPurchaseToken);
                transaction->m_RequestId = GetSafeStr(purchase_handle, fbg_Purchase_GetRequestId);
                transaction->m_Status = GetSafeStr(purchase_handle, fbg_Purchase_GetStatus);
                transaction->m_SignedRequest = GetSafeStr(purchase_handle, fbg_Purchase_GetSignedRequest);
                transaction->m_ErrorMessage = GetSafeStr(purchase_handle, fbg_Purchase_GetErrorMessage);
                transaction->m_Amount = fbg_Purchase_GetAmount(purchase_handle);
                transaction->m_PurchaseTime = fbg_Purchase_GetPurchaseTime(purchase_handle);
                transaction->m_Quantity = fbg_Purchase_GetQuantity(purchase_handle);
                transaction->m_ErrorCode = fbg_Purchase_GetErrorCode(purchase_handle);

                if (HasIAPListener())
                {
                    ProcessTransaction(transaction);
                    delete transaction;
                } else {
                    if (g_IAP.m_PendingTransactions.Full()) {
                        g_IAP.m_PendingTransactions.OffsetCapacity(4);
                    }
                    g_IAP.m_PendingTransactions.Push(transaction);
                }
            }
            break;
            case fbgMessage_HasLicense:
            {
                fbgHasLicenseHandle has_license_handle = fbg_Message_HasLicense(message);
                fbid has_license = fbg_HasLicense_GetHasLicense(has_license_handle);
                RunLicenseCallback(L, has_license);
            }
            break;
            default:
                dmLogError("Unknown Gameroom IAP message: %u", message_type);
            break;
        }

        fbg_FreeMessage(message);
    }
    return dmExtension::RESULT_OK;
}

DM_DECLARE_EXTENSION(IAPExt, "IAP", 0, 0, InitializeIAP, UpdateIAP, 0, 0)
