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

struct IAP
{
    IAP()
     : m_ListCallback(0x0)
     , m_PremiumCallback(0x0)
     , m_Listener(0x0)
     , m_PendingTransactions()
    {
        m_PendingTransactions.SetCapacity(4);
    }

    dmScript::LuaCallbackInfo*    m_ListCallback;
    dmScript::LuaCallbackInfo*    m_PremiumCallback;
    dmScript::LuaCallbackInfo*    m_Listener;

    dmArray<PendingTransaction*> m_PendingTransactions;

} g_IAP;

////////////////////////////////////////////////////////////////////////////////
// Functions for running callbacks; premium and buy/purchases
//

static void PutProcessTransactionArguments(lua_State* L, void* user_context)
{
    PendingTransaction* transaction = (PendingTransaction*)user_context;

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
                lua_setfield(L, -2, "error");
            } else {
                lua_pushfstring(L, "Unknown purchase status: %s.", transaction->m_Status);
                lua_setfield(L, -2, "error");
            }
            lua_pushnumber(L, REASON_UNSPECIFIED);
            lua_setfield(L, -2, "reason");
        }

    } else {
        lua_newtable(L);
        lua_pushnumber(L, REASON_UNSPECIFIED);
        lua_setfield(L, -2, "reason");
        lua_pushstring(L, transaction->m_ErrorMessage);
        lua_setfield(L, -2, "error");
    }
}

static void ProcessTransaction(PendingTransaction* transaction)
{
    dmScript::InvokeCallback(g_IAP.m_Listener, PutProcessTransactionArguments, (void*)transaction);
}

static void PutPremiumArguments(lua_State* L, void* user_context)
{
    bool* has_premium = (bool*)user_context;
    lua_pushboolean(L, *has_premium);
}

static void RunPremiumCallback(bool has_premium)
{
    if (!dmScript::IsValidCallback(g_IAP.m_PremiumCallback)) {
        dmLogError("No callback set for has premium callback.");
        return;
    }

    dmScript::InvokeCallback(g_IAP.m_PremiumCallback, PutPremiumArguments, (void*)&has_premium);

    dmScript::DeleteCallback(g_IAP.m_PremiumCallback);
    g_IAP.m_PremiumCallback = 0x0;
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

static void PutListWrapperResultArguments(lua_State* L, void* user_context)
{
    lua_pushvalue(L, 1); // copy of result table
    lua_pushvalue(L, 2); // copy of error table
}

static void PutListWrapperErrorArguments(lua_State* L, void* user_context)
{
    lua_pushnil(L);
    IAP_PushError(L, "Invalid result in iap.list.", REASON_UNSPECIFIED);
}

static int IAP_List_WrapperCB(lua_State* L)
{
    DM_LUA_STACK_CHECK(L, 0);

    if (!dmScript::IsValidCallback(g_IAP.m_ListCallback)) {
        dmLogError("Got iap.list result but no callback set.");
        return 0;
    }

    // Check if there was no result, then call callback with an error instead.
    int top = lua_gettop(L);
    if (top < 2) {
        dmScript::InvokeCallback(g_IAP.m_ListCallback, PutListWrapperErrorArguments, NULL);
        dmScript::DeleteCallback(g_IAP.m_ListCallback);
        g_IAP.m_ListCallback = 0x0;
        return 0;
    }

    // Invoke callback with result
    dmScript::InvokeCallback(g_IAP.m_ListCallback, PutListWrapperResultArguments, NULL);

    // Clear IAP callback info so iap.list can be called once again.
    dmScript::DeleteCallback(g_IAP.m_ListCallback);
    g_IAP.m_ListCallback = 0x0;

    return 0;
}

static void PutListArguments(lua_State* L, void* user_context)
{
}

static int IAP_List(lua_State* L)
{
    DM_LUA_STACK_CHECK(L, 0);

    if (!dmFBGameroom::CheckGameroomInit()) {
        return luaL_error(L, "Facebook Gameroom module isn't initialized! Did you set the windows.iap_provider in game.project?");
    }

    if (dmScript::IsValidCallback(g_IAP.m_ListCallback))
    {
        dmLogError("List callback previously set");
        return 0;
    }

    luaL_checktype(L, 1, LUA_TTABLE);
    luaL_checktype(L, 2, LUA_TFUNCTION);

    // Get internal function to get and parse products (defined in iap_gameroom.lua).
    lua_getglobal(L, "iap");
    lua_getfield(L, -1, "__facebook_helper_list");

    lua_newtable(L);
    if (!IAP_List_CopyTable(L, 1, lua_gettop(L))) {
        lua_pop(L, 3); // pop "iap" table, internal func and urls table
        // We were unable to copy urls table, throw Lua error
        luaL_error(L, "Could not create a copy of products table in iap.list(). Verify that every entry is a string.");
        return 0;
    }

    g_IAP.m_ListCallback = dmScript::CreateCallback(L, 2);

    // Push wrapper callback
    lua_pushcfunction(L, IAP_List_WrapperCB);

    int ret = dmScript::PCall(L, 2, 0);
    if (ret != 0)
    {
        return luaL_error(L, "Error while running iap.__facebook_helper_list: %s", lua_tostring(L, -1));
    }
    lua_pop(L, 1); // pop "iap" table

    return 0;
}

static int IAP_Buy(lua_State* L)
{
    DM_LUA_STACK_CHECK(L, 0);

    if (!dmFBGameroom::CheckGameroomInit()) {
        return luaL_error(L, "Facebook Gameroom module isn't initialized! Did you set the windows.iap_provider in game.project?");
    }

    const char* product_id = luaL_checkstring(L, 1);

    // Figure out if this is a FB Purchases Lite or regular call
    // by checking if the product starts with "http".
    typedef fbgRequest (*purchase_func_t)(const char*, uint32_t, uint32_t, uint32_t, const char*, const char*, const char*);
    purchase_func_t purchase_func = fbg_PurchaseIAP;
    if (strncmp("http", product_id, 4) == 0) {
        purchase_func = fbg_PurchaseIAPWithProductURL;
    }

    if (lua_gettop(L) >= 2) {
        luaL_checktype(L, 2, LUA_TTABLE);
        purchase_func(
            product_id,
            dmScript::GetTableIntValue(L, 2, "quantity", 0),
            dmScript::GetTableIntValue(L, 2, "quantity_min", 0),
            dmScript::GetTableIntValue(L, 2, "quantity_max", 0),
            dmScript::GetTableStringValue(L, 2, "request_id", NULL),
            dmScript::GetTableStringValue(L, 2, "price_point_id", NULL),
            dmScript::GetTableStringValue(L, 2, "test_currency", NULL)
        );
    } else {
        purchase_func( product_id, 0, 0, 0, NULL, NULL, NULL );
    }

    return 0;
}

/*# check if user has already purchased premium license
 *
 * [icon:gameroom] Checks if a license for the game has been purchased by the user. 
 * You should provide a callback function that will be called with the result of the check.
 *
 * [icon:attention] This function does not work when testing the application
 * locally in the Gameroom client.
 *
 * @name iap.has_premium
 * @namespace iap
 * 
 * @param callback [type:function(self, has_premium)] result callback
 *
 * `self`
 * : [type:object] The current object.
 *
 * `has_premium`
 * : [type:boolean] `true` if the user has premium license, `false` otherwise.
 *
 * @examples
 *
 * ```lua
 * local function premium_result(self, has_premium)
 *   if has_premium then
 *      -- User has purchased this premium game.
 *   end
 * end
 *
 * function init()
 *   -- check is user has bought the game
 *   iap.has_premium(premium_result)
 * end
 * ```
 */
static int IAP_HasPremium(lua_State* L)
{
    DM_LUA_STACK_CHECK(L, 0);

    if (!dmFBGameroom::CheckGameroomInit()) {
        return luaL_error(L, "Facebook Gameroom module isn't initialized! Did you set the windows.iap_provider in game.project?");
    }

    if (dmScript::IsValidCallback(g_IAP.m_PremiumCallback)) {
        dmLogError("Previous iap.has_premium callback set.");
        return 0;
    }

    luaL_checktype(L, 1, LUA_TFUNCTION);
    g_IAP.m_PremiumCallback = dmScript::CreateCallback(L, 1);

    fbg_HasLicense();

    return 0;
}

/*# purchase a premium license
 *
 * [icon:gameroom] Performs a purchase of a premium game license. The purchase transaction 
 * is handled like regular iap purchases; calling the currently set iap_listener with the
 * transaction results.
 *
 * [icon:attention] This function does not work when testing the application
 * locally in the Gameroom client.
 *
 * @name iap.buy_premium
 * @namespace iap
 *
 * @examples
 *
 * ```lua
 * local function iap_listener(self, transaction, error)
 *   if error == nil then
 *     -- purchase is ok
 *     print(transaction.date)
 *     print(transaction.)
 *     -- required if auto finish transactions is disabled in project settings
 *     if (transaction.state == iap.TRANS_STATE_PURCHASED) then
 *       -- do server-side verification of purchase here..
 *       iap.finish(transaction)
 *     end
 *   else
 *     print(error.error, error.reason)
 *   end
 * end
 *
 * function init(self)
 *   -- set the listener function for iap transactions
 *   iap.set_listener(iap_listener)
 *   -- purchase premium license
 *   iap.buy_premium()
 * end
 * ```
 */
static int IAP_BuyPremium(lua_State* L)
{
    DM_LUA_STACK_CHECK(L, 0);

    if (!dmFBGameroom::CheckGameroomInit()) {
        return luaL_error(L, "Facebook Gameroom module isn't initialized! Did you set the windows.iap_provider in game.project?");
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
    return 1;
}

static int IAP_SetListener(lua_State* L)
{
    if (!dmFBGameroom::CheckGameroomInit()) {
        return luaL_error(L, "Facebook Gameroom module isn't initialized! Did you set the windows.iap_provider in game.project?");
    }

    luaL_checktype(L, 1, LUA_TFUNCTION);

    g_IAP.m_Listener = dmScript::CreateCallback(L, 1);

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
    {"has_premium", IAP_HasPremium},
    {"buy_premium", IAP_BuyPremium},
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
        lua_State* L = params->m_L;
        DM_LUA_STACK_CHECK(L, 0);

        luaL_register(L, LIB_NAME, IAP_methods);

        IAP_PushConstants(L);

        lua_pop(L, 1);

        // Load iap_gameroom.lua which adds __facebook_helper_list to the iap table.
        if (luaL_loadbuffer(L, (const char*)IAP_GAMEROOM_LUA, IAP_GAMEROOM_LUA_SIZE, "(internal) iap_gameroom.lua") != 0)
        {
            dmLogError("Could not load iap_gameroom.lua: %s", lua_tostring(L, -1));
            lua_pop(L, 1);
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

    dmArray<fbgMessageHandle>* messages = dmFBGameroom::GetIAPMessages();
    for (uint32_t i = 0; i < messages->Size(); ++i)
    {
        fbgMessageHandle message = (*messages)[i];
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

                if (dmScript::IsValidCallback(g_IAP.m_Listener))
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
                RunPremiumCallback(has_license);
            }
            break;
            default:
                dmLogError("Unknown Gameroom IAP message: %u", message_type);
            break;
        }

        fbg_FreeMessage(message);
    }

    // Clear message array
    messages->SetSize(0);
    return dmExtension::RESULT_OK;
}

static dmExtension::Result AppFinalizeIAP(dmExtension::AppParams* params)
{
    // Cleanup pending unhandled transactions
    if (!g_IAP.m_PendingTransactions.Empty()) {
        for (int i = 0; i < g_IAP.m_PendingTransactions.Size(); ++i)
        {
            delete g_IAP.m_PendingTransactions[i];
        }
        g_IAP.m_PendingTransactions.SetSize(0);
    }
    return dmExtension::RESULT_OK;
}

DM_DECLARE_EXTENSION(IAPExt, "IAP", 0, AppFinalizeIAP, InitializeIAP, UpdateIAP, 0, 0)
