#include <dlib/array.h>
#include <dlib/log.h>
#include <extension/extension.h>
#include <script/script.h>

#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>
#import <StoreKit/StoreKit.h>

#define LIB_NAME "iap"

struct IAP;

@interface SKPaymentTransactionObserver : NSObject<SKPaymentTransactionObserver>
    @property IAP* m_IAP;
@end

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
        m_Callback = LUA_NOREF;
        m_Self = LUA_NOREF;
        m_InitCount = 0;
    }
    int                  m_InitCount;
    int                  m_Callback;
    int                  m_Self;
    IAPListener          m_Listener;
    SKPaymentTransactionObserver* m_Observer;
};

IAP g_IAP;

NS_ENUM(NSInteger, ErrorReason)
{
    REASON_UNSPECIFIED = 0,
    REASON_USER_CANCELED = 1,
};

static void PushError(lua_State*L, NSError* error, NSInteger reason)
{
    if (error != 0) {
        lua_newtable(L);
        lua_pushstring(L, "error");
        lua_pushstring(L, [error.localizedDescription UTF8String]);
        lua_rawset(L, -3);
        lua_pushstring(L, "reason");
        lua_pushnumber(L, reason);
        lua_rawset(L, -3);
    } else {
        lua_pushnil(L);
    }
}

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

    int ret = lua_pcall(L, 3, LUA_MULTRET, 0);
    if (ret != 0) {
        dmLogError("Error running iap callback: %s", lua_tostring(L,-1));
        lua_pop(L, 1);
    }

    luaL_unref(L, LUA_REGISTRYINDEX, g_IAP.m_Callback);
    luaL_unref(L, LUA_REGISTRYINDEX, g_IAP.m_Self);
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
    PushError(L, error, REASON_UNSPECIFIED);

    int ret = lua_pcall(L, 3, LUA_MULTRET, 0);
    if (ret != 0) {
        dmLogError("Error running iap callback: %s", lua_tostring(L,-1));
        lua_pop(L, 1);
    }

    luaL_unref(L, LUA_REGISTRYINDEX, g_IAP.m_Callback);
    luaL_unref(L, LUA_REGISTRYINDEX, g_IAP.m_Self);
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
        lua_pushlstring(L, (const char*) transaction.transactionReceipt.bytes, transaction.transactionReceipt.length);
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
        dmLogError("Could not run facebook callback because the instance has been deleted.");
        lua_pop(L, 2);
        assert(top == lua_gettop(L));
        return;
    }

    PushTransaction(L, transaction);

    if (transaction.transactionState == SKPaymentTransactionStateFailed) {
        if (transaction.error.code == SKErrorPaymentCancelled) {
            PushError(L, transaction.error, REASON_USER_CANCELED);
        } else {
            PushError(L, transaction.error, REASON_UNSPECIFIED);
        }
    } else {
        lua_pushnil(L);
    }

    int ret = lua_pcall(L, 3, LUA_MULTRET, 0);
    if (ret != 0) {
        dmLogError("Error running iap callback: %s", lua_tostring(L,-1));
        lua_pop(L, 1);
    }

    assert(top == lua_gettop(L));
}

@implementation SKPaymentTransactionObserver
    - (void)paymentQueue:(SKPaymentQueue *)queue updatedTransactions:(NSArray *)transactions
    {
        for (SKPaymentTransaction * transaction in transactions) {

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
                    if (has_listener > 0) {
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
 * @name iap.list
 * @param ids table (array) to get information about
 * @param callback result callback
 * @examples
 *
 * <pre>
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
 * function example(self)
 *     iap.list({"my_iap"}, iap_callback)
 * end
 * </pre>
 */
int IAP_List(lua_State* L)
{
    int top = lua_gettop(L);
    if (g_IAP.m_Callback != LUA_NOREF) {
        dmLogError("Unexpected callback set");
        luaL_unref(L, LUA_REGISTRYINDEX, g_IAP.m_Callback);
        luaL_unref(L, LUA_REGISTRYINDEX, g_IAP.m_Self);
        g_IAP.m_Callback = LUA_NOREF;
        g_IAP.m_Self = LUA_NOREF;
    }

    NSCountedSet* product_identifiers = [[[NSCountedSet alloc] init] autorelease];

    lua_pushnil(L);
    while (lua_next(L, 1) != 0) {
        const char* p = luaL_checkstring(L, -1);
        [product_identifiers addObject: [NSString stringWithUTF8String: p]];
        lua_pop(L, 1);
    }

    luaL_checktype(L, 2, LUA_TFUNCTION);
    lua_pushvalue(L, 2);
    g_IAP.m_Callback = luaL_ref(L, LUA_REGISTRYINDEX);

    dmScript::GetInstance(L);
    g_IAP.m_Self = luaL_ref(L, LUA_REGISTRYINDEX);

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
 * @name iap.buy
 * @param id product to buy (identifier)
 * @examples
 *
 * <pre>
 * local function iap_listener(self, transaction, error)
 *     if error == nil then
 *         print(transaction.ident)
 *         print(transaction.state)
 *         print(transaction.date)
 *         print(transaction.trans_ident) -- only available when state == TRANS_STATE_PURCHASED or state == TRANS_STATE_RESTORED
 *         print(transaction.receipt)     -- only available when state == TRANS_STATE_PURCHASED
 *     else
 *         print(error.error, error.reason)
 *     end
 * end
 * function example(self)
 *     iap.set_listener(iap_listener)
 *     iap.buy("my_iap")
 * end
 * </pre>
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

/*# restore products (non-consumable)
 *
 * @name iap.restore
 */
int IAP_Restore(lua_State* L)
{
    // TODO: Missing callback here for completion/error
    // See callback under "Handling Restored Transactions"
    // https://developer.apple.com/library/ios/documentation/StoreKit/Reference/SKPaymentTransactionObserver_Protocol/Reference/Reference.html
    int top = lua_gettop(L);
    [[SKPaymentQueue defaultQueue] restoreCompletedTransactions];
    assert(top == lua_gettop(L));
    return 0;
}

/*# set transaction listener
 *
 * The listener callback has the following signature: function(self, transaction, error) where transaction is a table
 * describing the transaction and error is a table. The error parameter is nil on success.
 * The transaction table has the following members:
 * <ul>
 * <li> ident: product identifier
 * <li> state: transaction state
 * <li> trans_ident: transaction identifier (only set when state == TRANS_STATE_RESTORED or state == TRANS_STATE_PURCHASED)
 * <li> receipt: receipt (only set when state == TRANS_STATE_PURCHASED)
 * <li> date: transaction date
 * <li> original_trans: original transaction (only set when state == TRANS_STATE_RESTORED)
 * </ul>
 * @name iap.set_listener
 * @param listener listener function
 */
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

static const luaL_reg IAP_methods[] =
{
    {"list", IAP_List},
    {"buy", IAP_Buy},
    {"restore", IAP_Restore},
    {"set_listener", IAP_SetListener},
    {0, 0}
};

/*# transaction purchasing state
 *
 * @name iap.TRANS_STATE_PURCHASING
 * @variable
 */

/*# transaction purchased state
 *
 * @name iap.TRANS_STATE_PURCHASED
 * @variable
 */

/*# transaction failed state
 *
 * @name iap.TRANS_STATE_FAILED
 * @variable
 */

/*# transaction restored state
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

dmExtension::Result InitializeIAP(dmExtension::Params* params)
{
    // TODO: Life-cycle managaemnt is *budget*. No notion of "static initalization"
    // Extend extension functionality with per system initalization?
    if (g_IAP.m_InitCount == 0) {
    }
    g_IAP.m_InitCount++;

    lua_State*L = params->m_L;
    int top = lua_gettop(L);
    luaL_register(L, LIB_NAME, IAP_methods);

#define SETCONSTANT(name, val) \
        lua_pushnumber(L, (lua_Number) val); \
        lua_setfield(L, -2, #name);\

    SETCONSTANT(TRANS_STATE_PURCHASING, SKPaymentTransactionStatePurchasing);
    SETCONSTANT(TRANS_STATE_PURCHASED, SKPaymentTransactionStatePurchased);
    SETCONSTANT(TRANS_STATE_FAILED, SKPaymentTransactionStateFailed);
    SETCONSTANT(TRANS_STATE_RESTORED, SKPaymentTransactionStateRestored);

    SETCONSTANT(REASON_UNSPECIFIED, REASON_UNSPECIFIED);
    SETCONSTANT(REASON_USER_CANCELED, REASON_USER_CANCELED);

#undef SETCONSTANT

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
        luaL_unref(g_IAP.m_Listener.m_L, LUA_REGISTRYINDEX, g_IAP.m_Listener.m_Callback);
        luaL_unref(g_IAP.m_Listener.m_L, LUA_REGISTRYINDEX, g_IAP.m_Listener.m_Self);
        g_IAP.m_Listener.m_L = 0;
        g_IAP.m_Listener.m_Callback = LUA_NOREF;
        g_IAP.m_Listener.m_Self = LUA_NOREF;
    }

    if (g_IAP.m_InitCount == 0) {
        if (g_IAP.m_Observer) {
            [[SKPaymentQueue defaultQueue] removeTransactionObserver: g_IAP.m_Observer];
            [g_IAP.m_Observer release];
            g_IAP.m_Observer = 0;
        }
    }
    return dmExtension::RESULT_OK;
}


DM_DECLARE_EXTENSION(IAPExt, "IAP", 0, 0, InitializeIAP, 0, 0, FinalizeIAP)
