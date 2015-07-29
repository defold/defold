#include <dlib/log.h>
#include <extension/extension.h>
#include <script/script.h>
#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>
#import <FBSDKCoreKit/FBSDKCoreKit.h>
#import <FBSDKLoginKit/FBSDKLoginKit.h>
#import <FBSDKShareKit/FBSDKShareKit.h>
#import <objc/runtime.h>

#include "facebook.h"

#define LIB_NAME "facebook"

struct Facebook
{
    Facebook() {
        memset(this, 0, sizeof(*this));
        m_Callback = LUA_NOREF;
        m_Self = LUA_NOREF;
    }

    FBSDKLoginManager *m_Login;
    NSDictionary* m_Me;
    int m_Callback;
    int m_Self;
    lua_State* m_MainThread;
    id<UIApplicationDelegate,
       FBSDKSharingDelegate,
       FBSDKAppInviteDialogDelegate,
       FBSDKGameRequestDialogDelegate> m_Delegate;
};

Facebook g_Facebook;

static void RunDialogResultCallback(lua_State*L, NSDictionary* result, NSError* error);

// AppDelegate used temporarily to hijack all AppDelegate messages
// An improvment could be to create generic proxy
@interface FacebookAppDelegate : NSObject <UIApplicationDelegate,
    FBSDKSharingDelegate, FBSDKAppInviteDialogDelegate,
    FBSDKGameRequestDialogDelegate>

- (BOOL)application:(UIApplication *)application
                   openURL:(NSURL *)url
                   sourceApplication:(NSString *)sourceApplication
                   annotation:(id)annotation;

- (BOOL)application:(UIApplication *)application
                   didFinishLaunchingWithOptions:(NSDictionary *)launchOptions;
@end

@implementation FacebookAppDelegate
    - (BOOL)application:(UIApplication *)application
                       openURL:(NSURL *)url
                       sourceApplication:(NSString *)sourceApplication
                       annotation:(id)annotation {
        return [[FBSDKApplicationDelegate sharedInstance] application:application
                                                              openURL:url
                                                    sourceApplication:sourceApplication
                                                           annotation:annotation];
    }

    - (void)applicationDidBecomeActive:(UIApplication *)application {
        [FBSDKAppEvents activateApp];
    }

    - (BOOL)application:(UIApplication *)application didFinishLaunchingWithOptions:(NSDictionary *)launchOptions {
        return [[FBSDKApplicationDelegate sharedInstance] application:application
                                        didFinishLaunchingWithOptions:launchOptions];
    }

    // Sharing related methods
    - (void)sharer:(id<FBSDKSharing>)sharer didCompleteWithResults :(NSDictionary *)results {
        RunDialogResultCallback(g_Facebook.m_MainThread, results, 0);
    }

    - (void)sharer:(id<FBSDKSharing>)sharer didFailWithError:(NSError *)error {
        RunDialogResultCallback(g_Facebook.m_MainThread, 0, error);
    }

    - (void)sharerDidCancel:(id<FBSDKSharing>)sharer {
        NSMutableDictionary *errorDetail = [NSMutableDictionary dictionary];
        [errorDetail setValue:@"Share dialog was cancelled" forKey:NSLocalizedDescriptionKey];
        RunDialogResultCallback(g_Facebook.m_MainThread, 0, [NSError errorWithDomain:@"facebook" code:0 userInfo:errorDetail]);
    }

    // App invite related methods
    - (void) appInviteDialog: (FBSDKAppInviteDialog *)appInviteDialog didCompleteWithResults:(NSDictionary *)results {
        RunDialogResultCallback(g_Facebook.m_MainThread, results, 0);
    }

    - (void) appInviteDialog: (FBSDKAppInviteDialog *)appInviteDialog didFailWithError:(NSError *)error {
        RunDialogResultCallback(g_Facebook.m_MainThread, 0, error);
    }

    // Game request related methods
    - (void)gameRequestDialog:(FBSDKGameRequestDialog *)gameRequestDialog didCompleteWithResults:(NSDictionary *)results {
        RunDialogResultCallback(g_Facebook.m_MainThread, results, 0);
    }

    - (void)gameRequestDialog:(FBSDKGameRequestDialog *)gameRequestDialog didFailWithError:(NSError *)error {
        RunDialogResultCallback(g_Facebook.m_MainThread, 0, error);
    }

    - (void)gameRequestDialogDidCancel:(FBSDKGameRequestDialog *)gameRequestDialog {
        NSMutableDictionary *errorDetail = [NSMutableDictionary dictionary];
        [errorDetail setValue:@"Game request dialog was cancelled" forKey:NSLocalizedDescriptionKey];
        RunDialogResultCallback(g_Facebook.m_MainThread, 0, [NSError errorWithDomain:@"facebook" code:0 userInfo:errorDetail]);
    }


@end

////////////////////////////////////////////////////////////////////////////////
// Helper functions for Lua API

static id LuaToObjC(lua_State* L, int index);
static NSArray* TableToNSArray(lua_State* L, int table_index)
{
    int top = lua_gettop(L);
    NSMutableArray* arr = [[NSMutableArray alloc] init];
    lua_pushnil(L);
    while (lua_next(L, table_index) != 0) {
        [arr addObject: LuaToObjC(L, lua_gettop(L))];
        lua_pop(L, 1);
    }

    assert(top == lua_gettop(L));
    return arr;
}

static id LuaToObjC(lua_State* L, int index)
{
    int top = lua_gettop(L);
    id r = nil;

    if (lua_type(L, index) == LUA_TSTRING) {
        r = [NSString stringWithUTF8String: lua_tostring(L, index)];

    } else if (lua_type(L, index) == LUA_TTABLE) {
        r = TableToNSArray(L, index);

    } else if (lua_type(L, index) == LUA_TNUMBER) {
        r = [NSNumber numberWithDouble: lua_tonumber(L, index)];

    } else if (lua_type(L, index) == LUA_TBOOLEAN) {
        r = [NSNumber numberWithBool:lua_toboolean(L, index)];

    }

    assert(top == lua_gettop(L));
    return r;
}

static id GetTableValue(lua_State* L, int table_index, NSArray* keys)
{
    id r = nil;
    int top = lua_gettop(L);
    for (NSString *key in keys) {
        lua_getfield(L, table_index, [key UTF8String]);
        if (!lua_isnil(L, -1))
        {
            r = LuaToObjC(L, lua_gettop(L));
        }
        lua_pop(L, 1);
    }

    assert(top == lua_gettop(L));
    return r;
}

static void PushError(lua_State*L, NSError* error)
{
    // Could be extended with error codes etc
    if (error != 0) {
        lua_newtable(L);
        lua_pushstring(L, "error");
        lua_pushstring(L, [error.localizedDescription UTF8String]);
        lua_rawset(L, -3);
    } else {
        lua_pushnil(L);
    }
}

static void VerifyCallback(lua_State* L)
{
    if (g_Facebook.m_Callback != LUA_NOREF) {
        dmLogError("Unexpected callback set");
        luaL_unref(L, LUA_REGISTRYINDEX, g_Facebook.m_Callback);
        luaL_unref(L, LUA_REGISTRYINDEX, g_Facebook.m_Self);
        g_Facebook.m_Callback = LUA_NOREF;
        g_Facebook.m_Self = LUA_NOREF;
    }
}

static void RunStateCallback(lua_State*L, dmFacebook::State status, NSError* error)
{
    if (g_Facebook.m_Callback != LUA_NOREF) {
        int top = lua_gettop(L);

        lua_rawgeti(L, LUA_REGISTRYINDEX, g_Facebook.m_Callback);
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

        lua_pushnumber(L, (lua_Number) status);
        PushError(L, error);

        int ret = lua_pcall(L, 3, LUA_MULTRET, 0);
        if (ret != 0) {
            dmLogError("Error running facebook callback: %s", lua_tostring(L,-1));
            lua_pop(L, 1);
        }
        assert(top == lua_gettop(L));
        luaL_unref(L, LUA_REGISTRYINDEX, g_Facebook.m_Callback);
        luaL_unref(L, LUA_REGISTRYINDEX, g_Facebook.m_Self);
        g_Facebook.m_Callback = LUA_NOREF;
        g_Facebook.m_Self = LUA_NOREF;
    } else {
        dmLogError("No callback set");
    }
}

static void RunCallback(lua_State*L, NSError* error)
{
    if (g_Facebook.m_Callback != LUA_NOREF) {
        int top = lua_gettop(L);

        lua_rawgeti(L, LUA_REGISTRYINDEX, g_Facebook.m_Callback);
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

        int ret = lua_pcall(L, 2, LUA_MULTRET, 0);
        if (ret != 0) {
            dmLogError("Error running facebook callback: %s", lua_tostring(L,-1));
            lua_pop(L, 1);
        }
        assert(top == lua_gettop(L));
        luaL_unref(L, LUA_REGISTRYINDEX, g_Facebook.m_Callback);
        luaL_unref(L, LUA_REGISTRYINDEX, g_Facebook.m_Self);
        g_Facebook.m_Callback = LUA_NOREF;
        g_Facebook.m_Self = LUA_NOREF;
    } else {
        dmLogError("No callback set");
    }
}

static void RunDialogResultCallback(lua_State*L, NSDictionary* result, NSError* error)
{
    if (g_Facebook.m_Callback != LUA_NOREF) {
        int top = lua_gettop(L);

        lua_rawgeti(L, LUA_REGISTRYINDEX, g_Facebook.m_Callback);
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

        lua_createtable(L, 0, 1);
        if (result) {
            for (id key in result) {
                id obj = [result objectForKey: key];
                if ([obj isKindOfClass:[NSString class]]) {
                    lua_pushstring(L, [key UTF8String]);
                    lua_pushstring(L, [obj UTF8String]);
                    lua_rawset(L, -3);
                }
            }
        }

        PushError(L, error);

        int ret = lua_pcall(L, 3, LUA_MULTRET, 0);
        if (ret != 0) {
            dmLogError("Error running facebook callback: %s", lua_tostring(L,-1));
            lua_pop(L, 1);
        }
        assert(top == lua_gettop(L));
        luaL_unref(L, LUA_REGISTRYINDEX, g_Facebook.m_Callback);
        luaL_unref(L, LUA_REGISTRYINDEX, g_Facebook.m_Self);
        g_Facebook.m_Callback = LUA_NOREF;
        g_Facebook.m_Self = LUA_NOREF;
    } else {
        dmLogError("No callback set");
    }
}

static void AppendArray(lua_State*L, NSMutableArray* array, int table)
{
    lua_pushnil(L);
    while (lua_next(L, table) != 0) {
        const char* p = luaL_checkstring(L, -1);
        [array addObject: [NSString stringWithUTF8String: p]];
        lua_pop(L, 1);
    }
}

static void UpdateUserData(lua_State* L)
{
    // Login successfull, now grab user info
    // In SDK 4+ we explicitly have to set which fields we want,
    // since earlier SDK versions returned these by default.
    NSMutableDictionary *params = [NSMutableDictionary dictionary];
    [params setObject:@"last_name,link,id,gender,email,locale,name,first_name,updated_time" forKey:@"fields"];
    [[[FBSDKGraphRequest alloc] initWithGraphPath:@"me" parameters:params]
    startWithCompletionHandler:^(FBSDKGraphRequestConnection *connection, id graphresult, NSError *error) {

        [g_Facebook.m_Me release];
        if (!error) {
            g_Facebook.m_Me = [[NSDictionary alloc] initWithDictionary: graphresult];
            RunStateCallback(L, dmFacebook::STATE_OPEN, error);
        } else {
            g_Facebook.m_Me = nil;
            dmLogWarning("Failed to fetch user-info: %s", [[error localizedDescription] UTF8String]);
            RunStateCallback(L, dmFacebook::STATE_CLOSED_LOGIN_FAILED, error);
        }

    }];
}

static FBSDKDefaultAudience convertDefaultAudience(int fromLuaInt) {
    switch (fromLuaInt) {
        case 2:
            return FBSDKDefaultAudienceOnlyMe;
        case 4:
            return FBSDKDefaultAudienceEveryone;
        case 3:
        default:
            return FBSDKDefaultAudienceFriends;
    }
}

static FBSDKGameRequestActionType convertGameRequestAction(int fromLuaInt) {
    switch (fromLuaInt) {
        case 3:
            return FBSDKGameRequestActionTypeAskFor;
        case 4:
            return FBSDKGameRequestActionTypeTurn;
        case 2:
            return FBSDKGameRequestActionTypeSend;
        case 1:
        default:
            return FBSDKGameRequestActionTypeNone;
    }
}

static FBSDKGameRequestFilter convertGameRequestFilters(int fromLuaInt) {
    switch (fromLuaInt) {
        case 3:
            return FBSDKGameRequestFilterAppNonUsers;
        case 2:
            return FBSDKGameRequestFilterAppUsers;
        case 1:
        default:
            return FBSDKGameRequestFilterNone;
    }
}

////////////////////////////////////////////////////////////////////////////////
// Lua API
int Facebook_Login(lua_State* L)
{
    int top = lua_gettop(L);
    VerifyCallback(L);

    luaL_checktype(L, 1, LUA_TFUNCTION);
    lua_pushvalue(L, 1);
    g_Facebook.m_Callback = luaL_ref(L, LUA_REGISTRYINDEX);

    dmScript::GetInstance(L);
    g_Facebook.m_Self = luaL_ref(L, LUA_REGISTRYINDEX);
    lua_State* main_thread = dmScript::GetMainThread(L);

    if ([FBSDKAccessToken currentAccessToken]) {

        UpdateUserData(main_thread);

    } else {

        NSMutableArray *permissions = [[NSMutableArray alloc] initWithObjects: @"public_profile", @"email", @"user_friends", nil];
        [g_Facebook.m_Login logInWithReadPermissions: permissions handler:^(FBSDKLoginManagerLoginResult *result, NSError *error) {

            if (error) {
                RunStateCallback(main_thread, dmFacebook::STATE_CLOSED_LOGIN_FAILED, error);
            } else if (result.isCancelled) {
                NSMutableDictionary *errorDetail = [NSMutableDictionary dictionary];
                [errorDetail setValue:@"Login was cancelled" forKey:NSLocalizedDescriptionKey];
                RunStateCallback(main_thread, dmFacebook::STATE_CLOSED_LOGIN_FAILED, [NSError errorWithDomain:@"facebook" code:0 userInfo:errorDetail]);
            } else {

                if ([result.grantedPermissions containsObject:@"public_profile"] &&
                    [result.grantedPermissions containsObject:@"email"] &&
                    [result.grantedPermissions containsObject:@"user_friends"]) {

                    UpdateUserData(main_thread);

                } else {
                    // FIXME: Skip this check and ignore if we didn't get all permissions.
                    //        This will show in the facebook.permissions() call anyway.
                    NSMutableDictionary *errorDetail = [NSMutableDictionary dictionary];
                    [errorDetail setValue:@"Not granted all requested permissions." forKey:NSLocalizedDescriptionKey];
                    RunStateCallback(main_thread, dmFacebook::STATE_CLOSED_LOGIN_FAILED, [NSError errorWithDomain:@"facebook" code:0 userInfo:errorDetail]);
                }
            }

        }];
    }

    assert(top == lua_gettop(L));
    return 0;
}

int Facebook_Logout(lua_State* L)
{
    [g_Facebook.m_Login logOut];
    [g_Facebook.m_Me release];
    g_Facebook.m_Me = 0;
    return 0;
}

int Facebook_RequestReadPermissions(lua_State* L)
{
    int top = lua_gettop(L);
    VerifyCallback(L);

    luaL_checktype(L, 1, LUA_TTABLE);
    luaL_checktype(L, 2, LUA_TFUNCTION);
    lua_pushvalue(L, 2);
    g_Facebook.m_Callback = luaL_ref(L, LUA_REGISTRYINDEX);

    dmScript::GetInstance(L);
    g_Facebook.m_Self = luaL_ref(L, LUA_REGISTRYINDEX);
    lua_State* main_thread = dmScript::GetMainThread(L);

    NSMutableArray *permissions = [[NSMutableArray alloc] init];
    AppendArray(L, permissions, 1);

    [g_Facebook.m_Login logInWithReadPermissions: permissions handler:^(FBSDKLoginManagerLoginResult *result, NSError *error) {
        RunCallback(main_thread, error);
    }];

    assert(top == lua_gettop(L));
    return 0;
}

int Facebook_RequestPublishPermissions(lua_State* L)
{
    int top = lua_gettop(L);
    VerifyCallback(L);

    luaL_checktype(L, 1, LUA_TTABLE);
    FBSDKDefaultAudience audience = convertDefaultAudience(luaL_checkinteger(L, 2));
    luaL_checktype(L, 3, LUA_TFUNCTION);
    lua_pushvalue(L, 3);
    g_Facebook.m_Callback = luaL_ref(L, LUA_REGISTRYINDEX);

    dmScript::GetInstance(L);
    g_Facebook.m_Self = luaL_ref(L, LUA_REGISTRYINDEX);
    lua_State* main_thread = dmScript::GetMainThread(L);

    NSMutableArray *permissions = [[NSMutableArray alloc] init];
    AppendArray(L, permissions, 1);

    [g_Facebook.m_Login setDefaultAudience: audience];
    [g_Facebook.m_Login logInWithPublishPermissions: permissions handler:^(FBSDKLoginManagerLoginResult *result, NSError *error) {
        RunCallback(main_thread, error);
    }];

    assert(top == lua_gettop(L));
    return 0;
}

int Facebook_AccessToken(lua_State* L)
{
    const char* token = [[[FBSDKAccessToken currentAccessToken] tokenString] UTF8String];
    lua_pushstring(L, token);
    return 1;
}

int Facebook_Permissions(lua_State* L)
{
    int top = lua_gettop(L);

    lua_newtable(L);
    NSSet* permissions = [[FBSDKAccessToken currentAccessToken] permissions];
    int i = 1;
    for (NSString* p in permissions) {
        lua_pushnumber(L, i);
        lua_pushstring(L, [p UTF8String]);
        lua_rawset(L, -3);
        ++i;
    }

    assert(top + 1 == lua_gettop(L));
    return 1;
}

int Facebook_Me(lua_State* L)
{
    int top = lua_gettop(L);

    if (g_Facebook.m_Me) {
        lua_newtable(L);
        for (id key in g_Facebook.m_Me) {
            id obj = [g_Facebook.m_Me objectForKey: key];
            if ([obj isKindOfClass:[NSString class]]) {
                lua_pushstring(L, [key UTF8String]);
                lua_pushstring(L, [obj UTF8String]);
                lua_rawset(L, -3);
            }
        }
    } else {
        lua_pushnil(L);
    }

    assert(top + 1 == lua_gettop(L));
    return 1;
}



/*# show facebook web dialog
 *  Note that for certain dialogs, e.g. apprequests, both "title" and "message" are mandatory. If not
 *  specified a generic error will be presented in the dialog. See https://developers.facebook.com/docs/dialogs for more
 *  information about parameters.
 * @name show_dialog
 * @param dialog dialog to show, "feed", "apprequests", etc
 * @param param table with dialog parameters, "title", "message", etc
 * @param callback function called, with parameters (self, result, error), when the dialog is closed. Result is table with an url-field set.
 */
static int Facebook_ShowDialog(lua_State* L)
{
    int top = lua_gettop(L);
    VerifyCallback(L);

    dmhash_t dialog = dmHashString64(luaL_checkstring(L, 1));
    luaL_checktype(L, 2, LUA_TTABLE);
    luaL_checktype(L, 3, LUA_TFUNCTION);
    lua_pushvalue(L, 3);
    g_Facebook.m_Callback = luaL_ref(L, LUA_REGISTRYINDEX);
    dmScript::GetInstance(L);
    g_Facebook.m_Self = luaL_ref(L, LUA_REGISTRYINDEX);
    g_Facebook.m_MainThread = dmScript::GetMainThread(L);

    if (dialog == dmHashString64("feed") ||
        dialog == dmHashString64("link")) {

        FBSDKShareLinkContent* content = [[FBSDKShareLinkContent alloc] init];
        content.contentTitle       = GetTableValue(L, 2, @[@"contentTitle", @"title"]);
        content.contentDescription = GetTableValue(L, 2, @[@"contentDescription", @"description"]);
        content.imageURL           = [NSURL URLWithString:GetTableValue(L, 2, @[@"imageURL"])];
        content.contentURL         = [NSURL URLWithString:GetTableValue(L, 2, @[@"contentURL", @"link"])];
        content.peopleIDs          = GetTableValue(L, 2, @[@"peopleIDs"]);
        content.placeID            = GetTableValue(L, 2, @[@"placeID"]);
        content.ref                = GetTableValue(L, 2, @[@"ref"]);

        [FBSDKShareDialog showFromViewController:nil withContent:content delegate:g_Facebook.m_Delegate];

    } else if (dialog == dmHashString64("appinvite")) {

        FBSDKAppInviteContent* content = [[FBSDKAppInviteContent alloc] init];
        content.appLinkURL               = [NSURL URLWithString:GetTableValue(L, 2, @[@"appLinkURL"])];
        content.appInvitePreviewImageURL = [NSURL URLWithString:GetTableValue(L, 2, @[@"appInvitePreviewImageURL"])];

        [FBSDKAppInviteDialog showWithContent:content delegate:g_Facebook.m_Delegate];

    } else if (dialog == dmHashString64("gamerequest") ||
               dialog == dmHashString64("apprequests")) {

        FBSDKGameRequestContent* content = [[FBSDKGameRequestContent alloc] init];
        content.title      = GetTableValue(L, 2, @[@"title"]);
        content.message    = GetTableValue(L, 2, @[@"message"]);
        content.actionType = convertGameRequestAction([GetTableValue(L, 2, @[@"actionType"]) unsignedIntValue]);
        content.filters    = convertGameRequestFilters([GetTableValue(L, 2, @[@"filters"]) unsignedIntValue]);
        content.data       = GetTableValue(L, 2, @[@"data"]);
        content.objectID   = GetTableValue(L, 2, @[@"objectID"]);

        // handle SDK < 4.0 deprecated way of specifying recipients and suggestions
        NSString* recipients = GetTableValue(L, 2, @[@"to"]);
        NSString* suggestions = GetTableValue(L, 2, @[@"suggestions"]);

        if (recipients != nil) {
            content.recipients = [recipients componentsSeparatedByString:@","];
        } else {
            content.recipients = GetTableValue(L, 2, @[@"recipients"]);
        }
        if (suggestions != nil) {
            content.recipientSuggestions = [suggestions componentsSeparatedByString:@","];
        } else {
            content.recipientSuggestions = GetTableValue(L, 2, @[@"recipientSuggestions"]);
        }


        [FBSDKGameRequestDialog showWithContent:content delegate:g_Facebook.m_Delegate];

    } else {
        NSMutableDictionary *errorDetail = [NSMutableDictionary dictionary];
        [errorDetail setValue:@"Invalid dialog type" forKey:NSLocalizedDescriptionKey];
        NSError* error = [NSError errorWithDomain:@"facebook" code:0 userInfo:errorDetail];

        lua_State* main_thread = dmScript::GetMainThread(L);
        RunDialogResultCallback(main_thread, 0, error);
    }

    assert(top == lua_gettop(L));
    return 0;
}

static const luaL_reg Facebook_methods[] =
{
    {"login", Facebook_Login},
    {"logout", Facebook_Logout},
    {"access_token", Facebook_AccessToken},
    {"permissions", Facebook_Permissions},
    {"request_read_permissions", Facebook_RequestReadPermissions},
    {"request_publish_permissions", Facebook_RequestPublishPermissions},
    {"me", Facebook_Me},
    {"show_dialog", Facebook_ShowDialog},
    {0, 0}
};

dmExtension::Result AppInitializeFacebook(dmExtension::AppParams* params)
{
    g_Facebook.m_Delegate = [[FacebookAppDelegate alloc] init];
    dmExtension::RegisterUIApplicationDelegate(g_Facebook.m_Delegate);

    // 355198514515820 is HelloFBSample. Default value in order to avoid exceptions
    // Better solution?
    const char* app_id = dmConfigFile::GetString(params->m_ConfigFile, "facebook.appid", "355198514515820");
    [FBSDKSettings setAppID: [NSString stringWithUTF8String: app_id]];

    g_Facebook.m_Login = [[FBSDKLoginManager alloc] init];

    return dmExtension::RESULT_OK;
}

dmExtension::Result AppFinalizeFacebook(dmExtension::AppParams* params)
{
    dmExtension::UnregisterUIApplicationDelegate(g_Facebook.m_Delegate);
    [g_Facebook.m_Login release];
    return dmExtension::RESULT_OK;
}

dmExtension::Result InitializeFacebook(dmExtension::Params* params)
{
    lua_State*L = params->m_L;
    int top = lua_gettop(L);
    luaL_register(L, LIB_NAME, Facebook_methods);

#define SETCONSTANT(name, val) \
        lua_pushnumber(L, (lua_Number) val); \
        lua_setfield(L, -2, #name);\

    SETCONSTANT(STATE_CREATED,              dmFacebook::STATE_CREATED);
    SETCONSTANT(STATE_CREATED_TOKEN_LOADED, dmFacebook::STATE_CREATED_TOKEN_LOADED);
    SETCONSTANT(STATE_CREATED_OPENING,      dmFacebook::STATE_CREATED_OPENING);
    SETCONSTANT(STATE_OPEN,                 dmFacebook::STATE_OPEN);
    SETCONSTANT(STATE_OPEN_TOKEN_EXTENDED,  dmFacebook::STATE_OPEN_TOKEN_EXTENDED);
    SETCONSTANT(STATE_CLOSED,               dmFacebook::STATE_CLOSED);
    SETCONSTANT(STATE_CLOSED_LOGIN_FAILED,  dmFacebook::STATE_CLOSED_LOGIN_FAILED);

    SETCONSTANT(GAMEREQUEST_ACTIONTYPE_NONE,   dmFacebook::GAMEREQUEST_ACTIONTYPE_NONE);
    SETCONSTANT(GAMEREQUEST_ACTIONTYPE_SEND,   dmFacebook::GAMEREQUEST_ACTIONTYPE_SEND);
    SETCONSTANT(GAMEREQUEST_ACTIONTYPE_ASKFOR, dmFacebook::GAMEREQUEST_ACTIONTYPE_ASKFOR);
    SETCONSTANT(GAMEREQUEST_ACTIONTYPE_TURN,   dmFacebook::GAMEREQUEST_ACTIONTYPE_TURN);

    SETCONSTANT(GAMEREQUEST_FILTER_NONE,        dmFacebook::GAMEREQUEST_FILTER_NONE);
    SETCONSTANT(GAMEREQUEST_FILTER_APPUSERS,    dmFacebook::GAMEREQUEST_FILTER_APPUSERS);
    SETCONSTANT(GAMEREQUEST_FILTER_APPNONUSERS, dmFacebook::GAMEREQUEST_FILTER_APPNONUSERS);

    SETCONSTANT(AUDIENCE_NONE,     dmFacebook::AUDIENCE_NONE);
    SETCONSTANT(AUDIENCE_ONLYME,   dmFacebook::AUDIENCE_ONLYME);
    SETCONSTANT(AUDIENCE_FRIENDS,  dmFacebook::AUDIENCE_FRIENDS);
    SETCONSTANT(AUDIENCE_EVERYONE, dmFacebook::AUDIENCE_EVERYONE);

#undef SETCONSTANT

    lua_pop(L, 1);
    assert(top == lua_gettop(L));

    return dmExtension::RESULT_OK;
}

dmExtension::Result FinalizeFacebook(dmExtension::Params* params)
{
    return dmExtension::RESULT_OK;
}

DM_DECLARE_EXTENSION(FacebookExt, "Facebook", AppInitializeFacebook, AppFinalizeFacebook, InitializeFacebook, 0, FinalizeFacebook)
