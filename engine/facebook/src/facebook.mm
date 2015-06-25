#include <dlib/log.h>
#include <extension/extension.h>
#include <script/script.h>
#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>
#import <FacebookSDK/FacebookSDK.h>
#import <objc/runtime.h>

// Facebook resources
extern unsigned char CLOSE_PNG[];
extern uint32_t CLOSE_PNG_SIZE;
extern unsigned char CLOSEat2X_PNG[];
extern uint32_t CLOSEat2X_PNG_SIZE;

static void InitSession();

@interface UIImage (Defold)
    + (id)imageNamedX:(NSString *)name;
@end

static bool IsRetina() {
    // See http://stackoverflow.com/a/4641481
    if ([[UIScreen mainScreen] respondsToSelector:@selector(displayLinkWithTarget:selector:)] &&
        ([UIScreen mainScreen].scale == 2.0)) {
        return true;
    }
    return false;
}

/*
 * We don't bundle required images from Facebook SDK. Instead we monkey-patch imageNamed
 * and load images from memory. Note that close.png and close@2x.png are only required
 * for the embedded login-view. The embedded login-view is only used when no CFBundleURLSchemes
 * for the facebook application is specified (Info.plist) and is considered a fallback.
 * Some other images than close*.png might be required for other views though.
 */
@implementation UIImage (Defold)
    + (id)imageNamedX:(NSString *)name {
        if ([name compare: @"FacebookSDKResources.bundle/FBDialog/images/close.png"] == NSOrderedSame) {
            NSData* data = 0;
            if (IsRetina()) {
                data = [NSData dataWithBytes: (const void*) &CLOSEat2X_PNG[0] length: CLOSEat2X_PNG_SIZE];
            } else {
                data = [NSData dataWithBytes: (const void*) &CLOSE_PNG[0] length: CLOSE_PNG_SIZE];
            }

            return [UIImage imageWithData: data];
        } else {
            return [self imageNamedX: name];
        }
    }
@end

static void SwizzleImageNamed()
{
    Class klass = object_getClass([UIImage class]);
    Method originalMethod = class_getInstanceMethod(klass, @selector(imageNamed:));
    Method categoryMethod = class_getInstanceMethod(klass, @selector(imageNamedX:));
    method_exchangeImplementations(originalMethod, categoryMethod);
}

#define LIB_NAME "facebook"

struct Facebook
{
    Facebook() {
        memset(this, 0, sizeof(*this));
        m_Callback = LUA_NOREF;
        m_Self = LUA_NOREF;
    }
    FBSession* m_Session;
    NSDictionary* m_Me;
    int m_Callback;
    int m_Self;
    id<UIApplicationDelegate> m_Delegate;
};

Facebook g_Facebook;

// AppDelegate used temporarily to hijack all AppDelegate messages
// An improvment could be to create generic proxy
@interface FacebookAppDelegate :  NSObject <UIApplicationDelegate>
    - (BOOL) application:(UIApplication *)application
                        openURL:(NSURL *)url
                        sourceApplication:(NSString *)sourceApplication
                        annotation:(id)annotation;
@end

@implementation FacebookAppDelegate

    - (BOOL) application:(UIApplication *)application
                        openURL:(NSURL *)url
                        sourceApplication:(NSString *)sourceApplication
                        annotation:(id)annotation  {
        InitSession();
        return [FBAppCall handleOpenURL:url
                sourceApplication:sourceApplication
                withSession:g_Facebook.m_Session];
    }

    - (void)applicationDidBecomeActive:(UIApplication *)application {
        [FBAppCall handleDidBecomeActiveWithSession:g_Facebook.m_Session];
    }

    -(BOOL) application:(UIApplication *)application handleOpenURL:(NSURL *)url {
        InitSession();
        return [FBAppCall handleOpenURL: url sourceApplication: @"Defold" withSession: g_Facebook.m_Session];
    }

@end

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

static void RunStateCallback(lua_State*L, FBSessionState status, NSError* error)
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

static void RunDialogResultCallback(lua_State*L, const char* url, NSError* error)
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
        lua_pushliteral(L, "url");
        if (url) {
            lua_pushstring(L, url);
        } else {
            lua_pushnil(L);
        }
        lua_rawset(L, -3);

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

static void InitSession()
{
    if (g_Facebook.m_Session == 0) {
        // This is done lazily to not initialize the FB SDK until we actually need it
        NSMutableArray *permissions = [[NSMutableArray alloc] initWithObjects: @"public_profile", @"email", @"user_friends", nil];
        g_Facebook.m_Session = [[FBSession alloc] initWithPermissions:permissions];
        [permissions release];
    }
}


int Facebook_Login(lua_State* L)
{
    int top = lua_gettop(L);
    VerifyCallback(L);

    InitSession();

    luaL_checktype(L, 1, LUA_TFUNCTION);
    lua_pushvalue(L, 1);
    g_Facebook.m_Callback = luaL_ref(L, LUA_REGISTRYINDEX);

    dmScript::GetInstance(L);
    g_Facebook.m_Self = luaL_ref(L, LUA_REGISTRYINDEX);
    lua_State* main_thread = dmScript::GetMainThread(L);

    if (!g_Facebook.m_Session.isOpen) {
        [g_Facebook.m_Session release];

        // We support only public_profile in login as facebook requires that you can't mix read and publish permissions
        // It's also better to check current permissions after successfull login and request additional if required.
        NSMutableArray *permissions = [[NSMutableArray alloc] initWithObjects: @"public_profile", @"email", @"user_friends", nil];
        g_Facebook.m_Session = [[FBSession alloc] initWithPermissions:permissions];
//        g_Facebook.m_Session = [[FBSession alloc] init];

        [permissions release];

        FBSession.activeSession = g_Facebook.m_Session;

        [g_Facebook.m_Session openWithCompletionHandler:^(FBSession *session,
                                              FBSessionState status,
                                              NSError *error) {

            if (session == g_Facebook.m_Session) {
                // Respond only to "current" session
                // as we'll get invokations here from previous session
                // when closeAndClearTokenInformation is invoked

                // NOTE: Callback is executed on all state changes.
                // We are only interested in FBSessionStateOpen and FBSessionStateClosedLoginFailed
                if (status == FBSessionStateOpen) {
                    [[FBRequest requestForMe] startWithCompletionHandler:
                     ^(FBRequestConnection *connection, NSDictionary<FBGraphUser> *user, NSError *error) {
                         if (!error) {
                             [g_Facebook.m_Me release];
                             g_Facebook.m_Me = [[NSDictionary alloc] initWithDictionary: user];
                             RunStateCallback(main_thread, status, error);
                         } else {
                             [g_Facebook.m_Me release];
                             g_Facebook.m_Me = 0;
                             dmLogWarning("Failed to fetch user-info: %s", [[error localizedDescription] UTF8String]);
                             RunStateCallback(main_thread, status, error);
                         }
                     }];
                } else if (status == FBSessionStateClosedLoginFailed) {
                    RunStateCallback(main_thread, status, error);
                }
            }
        }];
    } else {
        RunStateCallback(main_thread, FBSessionStateOpen, 0);
    }
    assert(top == lua_gettop(L));
    return 0;
}

int Facebook_Logout(lua_State* L)
{
    if (g_Facebook.m_Session) {
        FBSession* s = g_Facebook.m_Session;
        // Clear session in order to filter out callbacks in openWithCompletionHandler above
        g_Facebook.m_Session = 0;
        [s closeAndClearTokenInformation];
        [s release];
        [g_Facebook.m_Me release];
        g_Facebook.m_Me = 0;
    }
    return 0;
}

int Facebook_RequestReadPermissions(lua_State* L)
{
    int top = lua_gettop(L);
    VerifyCallback(L);

    InitSession();

    luaL_checktype(L, 1, LUA_TTABLE);
    luaL_checktype(L, 2, LUA_TFUNCTION);
    lua_pushvalue(L, 2);
    g_Facebook.m_Callback = luaL_ref(L, LUA_REGISTRYINDEX);

    dmScript::GetInstance(L);
    g_Facebook.m_Self = luaL_ref(L, LUA_REGISTRYINDEX);
    lua_State* main_thread = dmScript::GetMainThread(L);

    if (g_Facebook.m_Session.isOpen) {
        NSMutableArray *permissions = [[NSMutableArray alloc] init];
        AppendArray(L, permissions, 1);

        [g_Facebook.m_Session requestNewReadPermissions: permissions completionHandler:^(FBSession *session,
                                              NSError *error) {
            RunCallback(main_thread, error);
        }];

        [permissions release];

    } else {
        dmLogWarning("Session not open");
    }
    assert(top == lua_gettop(L));
    return 0;
}

int Facebook_RequestPublishPermissions(lua_State* L)
{
    int top = lua_gettop(L);
    VerifyCallback(L);

    InitSession();

    luaL_checktype(L, 1, LUA_TTABLE);
    FBSessionDefaultAudience audience = (FBSessionDefaultAudience) luaL_checkinteger(L, 2);
    luaL_checktype(L, 3, LUA_TFUNCTION);
    lua_pushvalue(L, 3);
    g_Facebook.m_Callback = luaL_ref(L, LUA_REGISTRYINDEX);

    dmScript::GetInstance(L);
    g_Facebook.m_Self = luaL_ref(L, LUA_REGISTRYINDEX);
    lua_State* main_thread = dmScript::GetMainThread(L);

    if (g_Facebook.m_Session.isOpen) {
        NSMutableArray *permissions = [[NSMutableArray alloc] init];
        AppendArray(L, permissions, 1);

        [g_Facebook.m_Session requestNewPublishPermissions: permissions defaultAudience: audience completionHandler:^(FBSession *session,
                                              NSError *error) {
            RunCallback(main_thread, error);
        }];

        [permissions release];

    } else {
        dmLogWarning("Session not open");
    }
    assert(top == lua_gettop(L));
    return 0;
}

int Facebook_AccessToken(lua_State* L)
{
    InitSession();
    if (g_Facebook.m_Session.isOpen) {
        FBSession* s = g_Facebook.m_Session;
        const char* token = [s.accessTokenData.accessToken UTF8String];
        lua_pushstring(L, token);
    } else {
        lua_pushnil(L);
    }
    return 1;
}

// TODO: Can we trust this one? Sometimes we only get "basic_info" even
// though the app is authorized for additional permissions
// Better to fetch via graph?
int Facebook_Permissions(lua_State* L)
{
    int top = lua_gettop(L);

    lua_newtable(L);
    InitSession();
    if (g_Facebook.m_Session) {
        NSArray* permissions = g_Facebook.m_Session.permissions;
        int i = 1;
        for (id p in permissions) {
            lua_pushnumber(L, i);
            lua_pushstring(L, [p UTF8String]);
            lua_rawset(L, -3);
            ++i;
        }
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

    const char* dialog = luaL_checkstring(L, 1);
    luaL_checktype(L, 2, LUA_TTABLE);
    luaL_checktype(L, 3, LUA_TFUNCTION);
    lua_pushvalue(L, 3);
    g_Facebook.m_Callback = luaL_ref(L, LUA_REGISTRYINDEX);
    dmScript::GetInstance(L);
    g_Facebook.m_Self = luaL_ref(L, LUA_REGISTRYINDEX);
    lua_State* main_thread = dmScript::GetMainThread(L);

    if (g_Facebook.m_Session) {
        FBSession* s = g_Facebook.m_Session;

        // NOTE: +dictionary is for temporary dictionaries and is autoreleased
        NSMutableDictionary *params = [NSMutableDictionary dictionary];

        lua_pushnil(L);
        while (lua_next(L, 2) != 0) {
            const char* v = luaL_checkstring(L, -1);
            const char* k = luaL_checkstring(L, -2);
            [params setObject: [NSString stringWithUTF8String: v] forKey: [NSString stringWithUTF8String: k]];
            lua_pop(L, 1);
        }

        // Workaround for a bug in the current SDK...
        [params setObject:s.accessTokenData.accessToken ? : @"" forKey:@"access_token"];
        [params setObject:s.appID ? : @"" forKey:@"app_id"];

        [FBWebDialogs
             presentDialogModallyWithSession: s
             dialog: [NSString stringWithUTF8String: dialog]
             parameters: params
             handler:^(FBWebDialogResult result, NSURL *resultURL, NSError *error) {
                 RunDialogResultCallback(main_thread, [[resultURL absoluteString] UTF8String], error);
             }
        ];
    } else {
        const char* msg = "No facebook session active";
        dmLogWarning("%s", msg);

        NSMutableDictionary *errorDetail = [NSMutableDictionary dictionary];
        [errorDetail setValue:@"Failed to do something wicked" forKey:NSLocalizedDescriptionKey];
        NSError* error = [NSError errorWithDomain:@"facebook" code:0 userInfo:errorDetail];

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

    SwizzleImageNamed();

    // 355198514515820 is HelloFBSample. Default value in order to avoid exceptions
    // Better solution?
    const char* app_id = dmConfigFile::GetString(params->m_ConfigFile, "facebook.appid", "355198514515820");
    [FBSettings setDefaultAppID: [NSString stringWithUTF8String: app_id]];

    // NOTE: Removed when upgraded to SDK 3.22 (removed)
    // This is probably related to whether we should send tracking data to Facebook or not
    // See FBAppEvents activateApp for related functionality
    //[FBSettings setShouldAutoPublishInstall: false];

    // The session is created lazily, check InitSession
    g_Facebook.m_Session = 0;

    return dmExtension::RESULT_OK;
}

dmExtension::Result AppFinalizeFacebook(dmExtension::AppParams* params)
{
    if (g_Facebook.m_Session) {
        dmExtension::UnregisterUIApplicationDelegate(g_Facebook.m_Delegate);
        [g_Facebook.m_Session release];
        g_Facebook.m_Session = 0;
    }
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

    SETCONSTANT(STATE_CREATED, FBSessionStateCreated);
    SETCONSTANT(STATE_CREATED_TOKEN_LOADED, FBSessionStateCreatedTokenLoaded);
    SETCONSTANT(STATE_CREATED_OPENING, FBSessionStateCreatedOpening);
    SETCONSTANT(STATE_OPEN, FBSessionStateOpen);
    SETCONSTANT(STATE_OPEN_TOKEN_EXTENDED, FBSessionStateOpenTokenExtended);
    SETCONSTANT(STATE_CLOSED_LOGIN_FAILED, FBSessionStateClosedLoginFailed);
    SETCONSTANT(STATE_CLOSED, FBSessionStateClosed);
    SETCONSTANT(AUDIENCE_NONE, FBSessionDefaultAudienceNone)
    SETCONSTANT(AUDIENCE_ONLYME, FBSessionDefaultAudienceOnlyMe)
    SETCONSTANT(AUDIENCE_FRIENDS, FBSessionDefaultAudienceFriends)
    SETCONSTANT(AUDIENCE_EVERYONE, FBSessionDefaultAudienceEveryone)

    lua_pop(L, 1);
    assert(top == lua_gettop(L));

    return dmExtension::RESULT_OK;
}

dmExtension::Result FinalizeFacebook(dmExtension::Params* params)
{
    return dmExtension::RESULT_OK;
}

DM_DECLARE_EXTENSION(FacebookExt, "Facebook", AppInitializeFacebook, AppFinalizeFacebook, InitializeFacebook, 0, FinalizeFacebook)
