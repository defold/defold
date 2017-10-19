#include "iap.h"
#include "iap_private.h"
#include <dlib/array.h>
#include <dlib/log.h>
#include <script/script.h>
#include <extension/extension.h>

#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>
#import <StoreKit/StoreKit.h>

#define LIB_NAME "iap"

struct IAP;

@interface SKPaymentTransactionObserver : NSObject<SKPaymentTransactionObserver>
    @property IAP* m_IAP;
@end

/*# In-app purchases API documentation
 *
 * Functions and constants for interacting with Apple's In-app purchases
 * and Google's In-app billing.
 *
 * @document
 * @name In-app purchases
 * @namespace iap
 */

struct IAP
{
    IAP()
    {
        m_Callback = LUA_NOREF;
        m_Self = LUA_NOREF;
        m_InitCount = 0;
        m_AutoFinishTransactions = true;
        m_PendingTransactions = 0;
  }
    int                  m_InitCount;
    int                  m_Callback;
    int                  m_Self;
    bool                 m_AutoFinishTransactions;
    NSMutableDictionary* m_PendingTransactions;
    IAPListener          m_Listener;
    SKPaymentTransactionObserver* m_Observer;
};

IAP g_IAP;



@interface SKProductsRequestDelegate : NSObject<SKProductsRequestDelegate>
    @property lua_State* m_LuaState;
    @property (assign) SKProductsRequest* m_Request;
@end

@implementation SKProductsRequestDelegate
- (void)productsRequest:(SKProductsRequest *)request didReceiveResponse:(SKProductsResponse *)response{

    lua_State* L = self.m_LuaState;
    if (g_IAP.m_Callback == LUA_NOREF) {
        dmLogError("No callback set");
        return;
    }

    NSArray * skProducts = response.products;
    int top = lua_gettop(L);

    lua_rawgeti(L, LUA_REGISTRYINDEX, g_IAP.m_Callback);

    // Setup self
    lua_rawgeti(L, LUA_REGISTRYINDEX, g_IAP.m_Self);
    lua_pushvalue(L, -1);
    dmScript::SetInstance(L);

    if (!dmScript::IsInstanceValid(L))
    {
        dmLogError("Could not run facebook callback because the instance has been deleted.");
        lua_pop(L, 2);
        assert(top == lua_gettop(L));
        return;
    }

    lua_newtable(L);
    for (SKProduct * p in skProducts) {

        lua_pushstring(L, [p.productIdentifier UTF8String]);
        lua_newtable(L);

        lua_pushstring(L, "ident");
        lua_pushstring(L, [p.productIdentifier UTF8String]);
        lua_rawset(L, -3);

        lua_pushstring(L, "title");
        lua_pushstring(L, [p.localizedTitle UTF8String]);
        lua_rawset(L, -3);

        lua_pushstring(L, "description");
        lua_pushstring(L, [p.localizedDescription  UTF8String]);
        lua_rawset(L, -3);

        lua_pushstring(L, "price");
        lua_pushnumber(L, p.price.floatValue);
        lua_rawset(L, -3);

        NSNumberFormatter *formatter = [[[NSNumberFormatter alloc] init] autorelease];
        [formatter setNumberStyle: NSNumberFormatterCurrencyStyle];
        [formatter setLocale: p.priceLocale];
        NSString *price_string = [formatter stringFromNumber: p.price];

        lua_pushstring(L, "price_string");
        lua_pushstring(L, [price_string UTF8String]);
        lua_rawset(L, -3);

        lua_pushstring(L, "currency_code");
        lua_pushstring(L, [[p.priceLocale objectForKey:NSLocaleCurrencyCode] UTF8String]);
        lua_rawset(L, -3);

        lua_rawset(L, -3);
    }
    lua_pushnil(L);

    int ret = dmScript::PCall(L, 3, 0);
    if (ret != 0) {
        dmLogError("Error running iap callback");
    }

    dmScript::Unref(L, LUA_REGISTRYINDEX, g_IAP.m_Callback);
    dmScript::Unref(L, LUA_REGISTRYINDEX, g_IAP.m_Self);
    g_IAP.m_Callback = LUA_NOREF;
    g_IAP.m_Self = LUA_NOREF;

    assert(top == lua_gettop(L));
}

- (void)request:(SKRequest *)request didFailWithError:(NSError *)error{
    dmLogWarning("SKProductsRequest failed: %s", [error.localizedDescription UTF8String]);

    lua_State* L = self.m_LuaState;
    int top = lua_gettop(L);

    if (g_IAP.m_Callback == LUA_NOREF) {
        dmLogError("No callback set");
        return;
    }

    lua_rawgeti(L, LUA_REGISTRYINDEX, g_IAP.m_Callback);

    // Setup self
    lua_rawgeti(L, LUA_REGISTRYINDEX, g_IAP.m_Self);
    lua_pushvalue(L, -1);
    dmScript::SetInstance(L);

    if (!dmScript::IsInstanceValid(L))
    {
        dmLogError("Could not run iap callback because the instance has been deleted.");
        lua_pop(L, 2);
        assert(top == lua_gettop(L));
        return;
    }

    lua_pushnil(L);
    IAP_PushError(L, [error.localizedDescription UTF8String], REASON_UNSPECIFIED);

    int ret = dmScript::PCall(L, 3, 0);
    if (ret != 0) {
        dmLogError("Error running iap callback");
    }

    dmScript::Unref(L, LUA_REGISTRYINDEX, g_IAP.m_Callback);
    dmScript::Unref(L, LUA_REGISTRYINDEX, g_IAP.m_Self);
    g_IAP.m_Callback = LUA_NOREF;
    g_IAP.m_Self = LUA_NOREF;

    assert(top == lua_gettop(L));
}

- (void)requestDidFinish:(SKRequest *)request
{
    [self.m_Request release];
    [self release];
}

@end

static void PushTransaction(lua_State* L, SKPaymentTransaction* transaction)
{
    lua_newtable(L);

    lua_pushstring(L, "ident");
    lua_pushstring(L, [transaction.payment.productIdentifier UTF8String]);
    lua_rawset(L, -3);

    lua_pushstring(L, "state");
    lua_pushnumber(L, transaction.transactionState);
    lua_rawset(L, -3);

    if (transaction.transactionState == SKPaymentTransactionStatePurchased || transaction.transactionState == SKPaymentTransactionStateRestored) {
        lua_pushstring(L, "trans_ident");
        lua_pushstring(L, [transaction.transactionIdentifier UTF8String]);
        lua_rawset(L, -3);
    }

    if (transaction.transactionState == SKPaymentTransactionStatePurchased) {
        lua_pushstring(L, "receipt");
        if (floor(NSFoundationVersionNumber) <= NSFoundationVersionNumber_iOS_6_1) {
            lua_pushlstring(L, (const char*) transaction.transactionReceipt.bytes, transaction.transactionReceipt.length);
        } else {
            NSURL *receiptURL = [[NSBundle mainBundle] appStoreReceiptURL];
            NSData *receiptData = [NSData dataWithContentsOfURL:receiptURL];
            lua_pushlstring(L, (const char*) receiptData.bytes, receiptData.length);
        }
        lua_rawset(L, -3);
    }

    NSDateFormatter *dateFormatter = [[[NSDateFormatter alloc] init] autorelease];
    [dateFormatter setDateFormat:@"yyyy-MM-dd'T'HH:mm:ssZZZ"];
    lua_pushstring(L, "date");
    lua_pushstring(L, [[dateFormatter stringFromDate: transaction.transactionDate] UTF8String]);
    lua_rawset(L, -3);

    if (transaction.transactionState == SKPaymentTransactionStateRestored && transaction.originalTransaction) {
        lua_pushstring(L, "original_trans");
        PushTransaction(L, transaction.originalTransaction);
        lua_rawset(L, -3);
    }
}

void RunTransactionCallback(lua_State* L, int cb, int self, SKPaymentTransaction* transaction)
{
    int top = lua_gettop(L);

    lua_rawgeti(L, LUA_REGISTRYINDEX, cb);

    // Setup self
    lua_rawgeti(L, LUA_REGISTRYINDEX, self);
    lua_pushvalue(L, -1);
    dmScript::SetInstance(L);

    if (!dmScript::IsInstanceValid(L))
    {
        dmLogError("Could not run iap callback because the instance has been deleted.");
        lua_pop(L, 2);
        assert(top == lua_gettop(L));
        return;
    }

    PushTransaction(L, transaction);

    if (transaction.transactionState == SKPaymentTransactionStateFailed) {
        if (transaction.error.code == SKErrorPaymentCancelled) {
            IAP_PushError(L, [transaction.error.localizedDescription UTF8String], REASON_USER_CANCELED);
        } else {
            IAP_PushError(L, [transaction.error.localizedDescription UTF8String], REASON_UNSPECIFIED);
        }
    } else {
        lua_pushnil(L);
    }

    int ret = dmScript::PCall(L, 3, 0);
    if (ret != 0) {
        dmLogError("Error running iap callback");
    }

    assert(top == lua_gettop(L));
}

@implementation SKPaymentTransactionObserver
    - (void)paymentQueue:(SKPaymentQueue *)queue updatedTransactions:(NSArray *)transactions
    {
        for (SKPaymentTransaction * transaction in transactions) {

            if ((!g_IAP.m_AutoFinishTransactions) && (transaction.transactionState == SKPaymentTransactionStatePurchased)) {
                NSData *data = [transaction.transactionIdentifier dataUsingEncoding:NSUTF8StringEncoding];
                uint64_t trans_id_hash = dmHashBuffer64((const char*) [data bytes], [data length]);
                [g_IAP.m_PendingTransactions setObject:transaction forKey:[NSNumber numberWithInteger:trans_id_hash] ];
            }

            bool has_listener = false;
            if (self.m_IAP->m_Listener.m_Callback != LUA_NOREF) {
                const IAPListener& l = self.m_IAP->m_Listener;
                RunTransactionCallback(l.m_L, l.m_Callback, l.m_Self, transaction);
                has_listener = true;
            }

            switch (transaction.transactionState)
            {
                case SKPaymentTransactionStatePurchasing:
                    break;
                case SKPaymentTransactionStatePurchased:
                    if (has_listener > 0 && g_IAP.m_AutoFinishTransactions) {
                        [[SKPaymentQueue defaultQueue] finishTransaction:transaction];
                    }
                    break;
                case SKPaymentTransactionStateFailed:
                    if (has_listener > 0) {
                        [[SKPaymentQueue defaultQueue] finishTransaction:transaction];
                    }
                    break;
                case SKPaymentTransactionStateRestored:
                    if (has_listener > 0) {
                        [[SKPaymentQueue defaultQueue] finishTransaction:transaction];
                    }
                    break;
            }
        }
    }
@end

/*# list in-app products
 *
 * Get a list of all avaliable iap products.
 *
 * [icon:attention] Nested calls, that is calling iap.list from within callback is not supported.
 * Doing so will result in call being ignored with the engine reporting "Unexpected callback set".
 *
 * @name iap.list
 * @param ids [type:table] table (array) of identifiers to get products from
 * @param callback [type:function(self, products, error)] result callback
 *
 * self
 * :        [type:object] The current object.
 *
 * products
 * :        [type:table] The available iap products.
 *
 * error
 * :        [type:table] Any error message. [type:nil] if there is no error.
 *
 * @examples
 *
 * ```lua
 * local function iap_callback(self, products, error)
 *     if error == nil then
 *         for k,v in pairs(products) do
 *             print(v.ident)
 *             print(v.title)
 *             print(v.description)
 *             print(v.price)
 *             print(v.price_string)
 *             print(v.currency_code) -- only available on iOS
 *         end
 *     else
 *         print(error.error)
 *     end
 * end
 *
 * function init(self)
 *     iap.list({"my_iap"}, iap_callback)
 * end
 * ```
 */
int IAP_List(lua_State* L)
{
    int top = lua_gettop(L);
    if (g_IAP.m_Callback != LUA_NOREF) {
        dmLogError("Unexpected callback set");
        dmScript::Unref(L, LUA_REGISTRYINDEX, g_IAP.m_Callback);
        dmScript::Unref(L, LUA_REGISTRYINDEX, g_IAP.m_Self);
        g_IAP.m_Callback = LUA_NOREF;
        g_IAP.m_Self = LUA_NOREF;
    }

    NSCountedSet* product_identifiers = [[[NSCountedSet alloc] init] autorelease];

    luaL_checktype(L, 1, LUA_TTABLE);
    lua_pushnil(L);
    while (lua_next(L, 1) != 0) {
        const char* p = luaL_checkstring(L, -1);
        [product_identifiers addObject: [NSString stringWithUTF8String: p]];
        lua_pop(L, 1);
    }

    luaL_checktype(L, 2, LUA_TFUNCTION);
    lua_pushvalue(L, 2);
    g_IAP.m_Callback = dmScript::Ref(L, LUA_REGISTRYINDEX);

    dmScript::GetInstance(L);
    g_IAP.m_Self = dmScript::Ref(L, LUA_REGISTRYINDEX);

    SKProductsRequest* products_request = [[SKProductsRequest alloc] initWithProductIdentifiers: product_identifiers];
    SKProductsRequestDelegate* delegate = [SKProductsRequestDelegate alloc];
    delegate.m_LuaState = dmScript::GetMainThread(L);
    delegate.m_Request = products_request;
    products_request.delegate = delegate;
    [products_request start];

    assert(top == lua_gettop(L));
    return 0;
}

/*# buy product
 *
 * Perform a product purchase.
 *
 * [icon:attention] Calling `iap.finish` is required on a successful transaction if auto_finish_transactions is disabled in project settings.
 *
 * @name iap.buy
 * @param id [type:string] product to buy
 * @param [options] [type:table] optional parameters as properties.
 *
 * The options table has the following members:
 *
 * - request_id [icon:facebook]: optional custom unique request id to set for this transaction. The id becomes attached to the payment within the Graph API. Only available for Facebook IAP transactions.
 *
 * @examples
 *
 * ```lua
 * local function iap_listener(self, transaction, error)
 *     if error == nil then
 *         print(transaction.ident)
 *         print(transaction.state)
 *         print(transaction.date)
 *         print(transaction.trans_ident) -- only available when state == TRANS_STATE_PURCHASED, TRANS_STATE_UNVERIFIED or TRANS_STATE_RESTORED
 *         print(transaction.receipt)     -- only available when state == TRANS_STATE_PURCHASED or TRANS_STATE_UNVERIFIED
 *         print(transaction.request_id)  -- only available for Facebook IAP transactions (and if used in the iap.buy call parameters)
 *         print(transaction.user_id)     -- only available for Amazon IAP transactions
 *
 *         -- required if auto finish transactions is disabled in project settings
 *         if (transaction.state == iap.TRANS_STATE_PURCHASED) then
 *             -- do server-side verification of purchase here..
 *             iap.finish(transaction)
 *         end
 *     else
 *         print(error.error, error.reason)
 *     end
 * end
 *
 * function init(self)
 *     iap.set_listener(iap_listener)
 *     iap.buy("my_iap")
 * end
 * ```
 */
int IAP_Buy(lua_State* L)
{
    int top = lua_gettop(L);

    const char* id = luaL_checkstring(L, 1);
    SKMutablePayment* payment = [[SKMutablePayment alloc] init];
    payment.productIdentifier = [NSString stringWithUTF8String: id];
    payment.quantity = 1;

    [[SKPaymentQueue defaultQueue] addPayment:payment];
    [payment release];

    assert(top == lua_gettop(L));
    return 0;
}

/*# finish buying product
 *
 * Explicitly finish a product transaction.
 *
 * [icon:attention] Calling iap.finish is required on a successful transaction
 * if auto_finish_transactions is disabled in project settings (otherwise ignored).
 * The transaction.state field must equal iap.TRANS_STATE_PURCHASED.
 *
 * @name iap.finish
 * @param transaction [type:table] transaction table parameter as supplied in listener callback
 *
 */
int IAP_Finish(lua_State* L)
{
    if(g_IAP.m_AutoFinishTransactions)
    {
        dmLogWarning("Calling iap.finish when autofinish transactions is enabled. Ignored.");
        return 0;
    }

    int top = lua_gettop(L);

    luaL_checktype(L, 1, LUA_TTABLE);

    lua_getfield(L, -1, "state");
    if (lua_isnumber(L, -1))
    {
        if(lua_tointeger(L, -1) != SKPaymentTransactionStatePurchased)
        {
            dmLogError("Transaction error. Invalid transaction state for transaction finish (must be iap.TRANS_STATE_PURCHASED).");
            lua_pop(L, 1);
            assert(top == lua_gettop(L));
            return 0;
        }
    }
    lua_pop(L, 1);

    lua_getfield(L, -1, "trans_ident");
    if (!lua_isstring(L, -1)) {
        dmLogError("Transaction error. Invalid transaction data for transaction finish, does not contain 'trans_ident' key.");
        lua_pop(L, 1);
    }
    else
    {
          const char *str = lua_tostring(L, -1);
          uint64_t trans_ident_hash = dmHashBuffer64(str, strlen(str));
          lua_pop(L, 1);
          SKPaymentTransaction * transaction = [g_IAP.m_PendingTransactions objectForKey:[NSNumber numberWithInteger:trans_ident_hash]];
          if(transaction == 0x0) {
              dmLogError("Transaction error. Invalid trans_ident value for transaction finish.");
          } else {
              [[SKPaymentQueue defaultQueue] finishTransaction:transaction];
              [g_IAP.m_PendingTransactions removeObjectForKey:[NSNumber numberWithInteger:trans_ident_hash]];
          }
    }

    assert(top == lua_gettop(L));
    return 0;
}

/*# restore products (non-consumable)
 *
 * @name iap.restore
 * @return success [type:boolean] false if current store doesn't support handling restored transactions, otherwise true
 */
int IAP_Restore(lua_State* L)
{
    // TODO: Missing callback here for completion/error
    // See callback under "Handling Restored Transactions"
    // https://developer.apple.com/library/ios/documentation/StoreKit/Reference/SKPaymentTransactionObserver_Protocol/Reference/Reference.html
    int top = lua_gettop(L);
    [[SKPaymentQueue defaultQueue] restoreCompletedTransactions];
    assert(top == lua_gettop(L));
    lua_pushboolean(L, 1);
    return 1;
}

/*# set transaction listener
 *
 * Set the callback function to receive transaction events.
 *
 * @name iap.set_listener
 * @param listener [type:function(self, transaction, error)] listener callback function.
 * Pass an empty function if you no longer wish to receive callbacks.
 *
 * `self`
 * : [type:object] The current object.
 *
 * `transaction`
 * : [type:table] a table describing the transaction. The table contains the following fields:
 *
 * - `ident`: product identifier
 * - `state`: transaction state
 * - `date`: transaction date
 * - `original_trans`: original transaction (only set when state == TRANS_STATE_RESTORED)
 * - `trans_ident` : transaction identifier (only set when state == TRANS_STATE_RESTORED, TRANS_STATE_UNVERIFIED or TRANS_STATE_PURCHASED)
 * - `request_id`: transaction request id. (only if receipt is set and for Facebook IAP transactions when used in the iap.buy call parameters)
 * - `receipt`: receipt (only set when state == TRANS_STATE_PURCHASED or TRANS_STATE_UNVERIFIED)
 *
 * `error`
 * : [type:table] a table containing any error information. The error parameter is `nil` on success.
 *
 */
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

    if (g_IAP.m_Observer == 0) {
        SKPaymentTransactionObserver* observer = [[SKPaymentTransactionObserver alloc] init];
        observer.m_IAP = &g_IAP;
        // NOTE: We add the listener *after* a lua listener is set
        // The payment queue is persistent and "old" transaction might be processed
        // from previous session. We call "finishTransaction" when appropriate
        // for all transaction and we must ensure that the result is delivered to lua.
        [[SKPaymentQueue defaultQueue] addTransactionObserver: observer];
        g_IAP.m_Observer = observer;
    }

    return 0;
}

/*# get current provider id
 *
 * @name iap.get_provider_id
 * @return id [type:constant] provider id.
 *
 * - `iap.PROVIDER_ID_GOOGLE`
 * - `iap.PROVIDER_ID_AMAZON`
 * - `iap.PROVIDER_ID_APPLE`
 * - `iap.PROVIDER_ID_FACEBOOK`
 *
 */
int IAP_GetProviderId(lua_State* L)
{
    lua_pushinteger(L, PROVIDER_ID_APPLE);
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

/*# transaction purchasing state
 *
 * This is an intermediate mode followed by TRANS_STATE_PURCHASED.
 * Store provider support dependent.
 *
 * @name iap.TRANS_STATE_PURCHASING
 * @variable
 */

/*# transaction purchased state
 *
 * @name iap.TRANS_STATE_PURCHASED
 * @variable
 */

/*# transaction unverified state, requires verification of purchase
 *
 * @name iap.TRANS_STATE_UNVERIFIED
 * @variable
 */

/*# transaction failed state
 *
 * @name iap.TRANS_STATE_FAILED
 * @variable
 */

/*# transaction restored state
 *
 * This is only available on store providers supporting restoring purchases.
 *
 * @name iap.TRANS_STATE_RESTORED
 * @variable
 */

/*# unspecified error reason
 *
 * @name iap.REASON_UNSPECIFIED
 * @variable
 */

/*# user canceled reason
 *
 * @name iap.REASON_USER_CANCELED
 * @variable
 */


/*# iap provider id for Google
 *
 * @name iap.PROVIDER_ID_GOOGLE
 * @variable
 */

/*# provider id for Amazon
 *
 * @name iap.PROVIDER_ID_AMAZON
 * @variable
 */

/*# provider id for Apple
 *
 * @name iap.PROVIDER_ID_APPLE
 * @variable
 */

/*# provider id for Facebook
 *
 * @name iap.PROVIDER_ID_FACEBOOK
 * @variable
 */

dmExtension::Result InitializeIAP(dmExtension::Params* params)
{
    // TODO: Life-cycle managaemnt is *budget*. No notion of "static initalization"
    // Extend extension functionality with per system initalization?
    if (g_IAP.m_InitCount == 0) {
        g_IAP.m_AutoFinishTransactions = dmConfigFile::GetInt(params->m_ConfigFile, "iap.auto_finish_transactions", 1) == 1;
        g_IAP.m_PendingTransactions = [[NSMutableDictionary alloc]initWithCapacity:2];
    }
    g_IAP.m_InitCount++;


    lua_State*L = params->m_L;
    int top = lua_gettop(L);
    luaL_register(L, LIB_NAME, IAP_methods);

    // ensure ios payment constants values corresponds to iap constants.
    assert(TRANS_STATE_PURCHASING == SKPaymentTransactionStatePurchasing);
    assert(TRANS_STATE_PURCHASED == SKPaymentTransactionStatePurchased);
    assert(TRANS_STATE_FAILED == SKPaymentTransactionStateFailed);
    assert(TRANS_STATE_RESTORED == SKPaymentTransactionStateRestored);

    IAP_PushConstants(L);

    lua_pop(L, 1);
    assert(top == lua_gettop(L));

    return dmExtension::RESULT_OK;
}

dmExtension::Result FinalizeIAP(dmExtension::Params* params)
{
    --g_IAP.m_InitCount;

    // TODO: Should we support one listener per lua-state?
    // Or just use a single lua-state...?
    if (params->m_L == g_IAP.m_Listener.m_L && g_IAP.m_Listener.m_Callback != LUA_NOREF) {
        dmScript::Unref(g_IAP.m_Listener.m_L, LUA_REGISTRYINDEX, g_IAP.m_Listener.m_Callback);
        dmScript::Unref(g_IAP.m_Listener.m_L, LUA_REGISTRYINDEX, g_IAP.m_Listener.m_Self);
        g_IAP.m_Listener.m_L = 0;
        g_IAP.m_Listener.m_Callback = LUA_NOREF;
        g_IAP.m_Listener.m_Self = LUA_NOREF;
    }

    if (g_IAP.m_InitCount == 0) {
        if (g_IAP.m_PendingTransactions) {
             [g_IAP.m_PendingTransactions release];
             g_IAP.m_PendingTransactions = 0;
        }

        if (g_IAP.m_Observer) {
            [[SKPaymentQueue defaultQueue] removeTransactionObserver: g_IAP.m_Observer];
            [g_IAP.m_Observer release];
            g_IAP.m_Observer = 0;
        }
    }
    return dmExtension::RESULT_OK;
}


DM_DECLARE_EXTENSION(IAPExt, "IAP", 0, 0, InitializeIAP, 0, 0, FinalizeIAP)
