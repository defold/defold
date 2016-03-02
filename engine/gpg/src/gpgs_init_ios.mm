#include "gpgs.h"
#include "gpgs_types_native.h"

#include <extension/extension.h>

#include <dlib/log.h>
#undef TYPE_BOOL
#include <script/script.h>
#define TYPE_BOOL                1

#include <gpg/game_services.h>
#include <gpg/builder.h>
#include <gpg/default_callbacks.h>
#include <gpg/debug.h>

#import <GoogleSignIn/GoogleSignIn.h>

extern dmGpgs::GooglePlayGameServices* g_gpgs = NULL;
extern id g_viewController = NULL;


@interface GPGSAppDelegate : NSObject <UIApplicationDelegate, GIDSignInUIDelegate>

- (BOOL)application:(UIApplication *)application
                   openURL:(NSURL *)url
                   sourceApplication:(NSString *)sourceApplication
                   annotation:(id)annotation;
- (void)signInWillDispatch:(GIDSignIn *)signIn error:(NSError *)error;
- (void)signIn:(GIDSignIn *)signIn presentViewController:(UIViewController *)viewController;
- (void)signIn:(GIDSignIn *)signIn dismissViewController:(UIViewController *)viewController;

@end

@implementation GPGSAppDelegate
    - (void)viewDidLoad {
      // [super viewDidLoad];

      // TODO(developer) Configure the sign-in button look/feel

      [GIDSignIn sharedInstance].uiDelegate = self;
      // [GIDSignIn sharedInstance].allowsSignInWithWebView = NO;

      // Uncomment to automatically sign in the user.
      //[[GIDSignIn sharedInstance] signInSilently];
    }

    // Implement these methods only if the GIDSignInUIDelegate is not a subclass of
    // UIViewController.

    // Stop the UIActivityIndicatorView animation that was started when the user
    // pressed the Sign In button
    - (void)signInWillDispatch:(GIDSignIn *)signIn error:(NSError *)error
    {
      // [myActivityIndicator stopAnimating];
        printf("signInWillDispatch\n");

    }

    // Present a view that prompts the user to sign in with Google
    - (void)signIn:(GIDSignIn *)signIn presentViewController:(UIViewController *)viewController
    {
        printf("signIn\n");
      // [self presentViewController:viewController animated:YES completion:nil];
    }

    // Dismiss the "Sign in with Google" view
    - (void)signIn:(GIDSignIn *)signIn dismissViewController:(UIViewController *)viewController
    {
        printf("signIn\n");
      // [self dismissViewControllerAnimated:YES completion:nil];
    }

    - (BOOL)application:(UIApplication *)application
      openURL:(NSURL *)url
      sourceApplication:(NSString *)sourceApplication
      annotation:(id)annotation {
        printf("\n");
      return [[GIDSignIn sharedInstance] handleURL:url sourceApplication:sourceApplication annotation:annotation];
    }

@end


namespace
{
    void OnAuthActionFinished(
        gpg::AuthOperation auth_operation, gpg::AuthStatus status)
    {
        if (auth_operation == gpg::AuthOperation::SIGN_IN)
        {
            dmGpgs::Authentication::Impl::LoginCallback();
        }
        else if (auth_operation == gpg::AuthOperation::SIGN_OUT)
        {
            dmGpgs::Authentication::Impl::LogoutCallback();
        }

    }

    void OnAuthActionStarted(gpg::AuthOperation auth_operation)
    {

    }

    void PushStack(lua_State* src, lua_State* dst)
    {
        for (int i = 1; i <= lua_gettop(src); ++i)
        {
            switch(lua_type(src, i))
            {
                case LUA_TNIL:
                    lua_pushnil(dst);
                    break;
                case LUA_TNUMBER:
                    lua_pushnumber(dst, lua_tonumber(src, -1));
                    break;
                case LUA_TBOOLEAN:
                    lua_pushboolean(dst, lua_toboolean(src, -1));
                    break;
                case LUA_TSTRING:
                    lua_pushstring(dst, lua_tostring(src, -1));
                    break;
                case LUA_TTABLE:
                    lua_newtable(dst);
                    lua_pushnil(src);  /* first key */
                    while (lua_next(src, i) != 0) {
                        switch (lua_type(src, -1))
                        {
                            case LUA_TNUMBER:
                                dmGpgs::AddTableEntry(dst,
                                    lua_tostring(src, -2),
                                    (double) lua_tonumber(src, -1));
                                break;
                            case LUA_TSTRING:
                                dmGpgs::AddTableEntry(dst,
                                    lua_tostring(src, -2),
                                    (const char*) lua_tostring(src, -1));
                                break;
                            case LUA_TBOOLEAN:
                                dmGpgs::AddTableEntry(dst,
                                    lua_tostring(src, -2),
                                    (bool) lua_toboolean(src, -1));
                                break;
                            default:
                                dmLogError("Datatype %s is not supported.",
                                    lua_typename(src, lua_type(src, -1)));
                        }

                        lua_pop(src, 1);
                    }
                    break;
                case LUA_TFUNCTION:
                    lua_pushcfunction(dst, lua_tocfunction(src, -1));
                    break;
                default:
                    dmLogError("Datatype %s has not been implemented.",
                        lua_typename(src, lua_type(src, -1)));
                    break;
            }
        }
    }
};

static const luaL_reg gpgs_methods[] =
{
    {"login", dmGpgs::Authentication::Login},
    {"get_login_status", dmGpgs::Authentication::GetLoginStatus},
    {"logout", dmGpgs::Authentication::Logout},

    {"achievement_unlock", dmGpgs::Achievement::Unlock},
    {"achievement_reveal", dmGpgs::Achievement::Reveal},
    {"achievement_increment", dmGpgs::Achievement::Increment},
    {"achievement_set", dmGpgs::Achievement::Set},
    {"achievement_fetch", dmGpgs::Achievement::Fetch},
    {"achievement_show_all", dmGpgs::Achievement::ShowAll},

    {"event_increment", dmGpgs::Event::Increment},
    {"event_fetch", dmGpgs::Event::Fetch},

    {"leaderboard_submit_score", dmGpgs::Leaderboard::SubmitScore},
    {"leaderboard_show", dmGpgs::Leaderboard::Show},
    {"leaderboard_show_all", dmGpgs::Leaderboard::ShowAll},

    {"player_fetch_info", dmGpgs::Player::FetchInformation},
    {"player_fetch_stats", dmGpgs::Player::FetchStatistics},

    {"quest_accept", dmGpgs::Quest::Accept},
    {"quest_claim_milestone", dmGpgs::Quest::ClaimMilestone},
    {"quest_fetch", dmGpgs::Quest::Fetch},
    {"quest_show", dmGpgs::Quest::Show},
    {"quest_show_all", dmGpgs::Quest::ShowAll},

    {"snapshot_save", dmGpgs::Snapshot::Commit},
    {"snapshot_read", dmGpgs::Snapshot::Read},
    {"snapshot_delete", dmGpgs::Snapshot::Delete},
    {"snapshot_show", dmGpgs::Snapshot::Show},
    {0,0}

};

dmExtension::Result AppInitializeGpgs(dmExtension::AppParams* params)
{
    return dmExtension::RESULT_OK;
}

dmExtension::Result AppFinalizeGpgs(dmExtension::AppParams* params)
{
    if (g_gpgs != NULL)
    {
        delete g_gpgs;
        g_gpgs = NULL;
    }

    return dmExtension::RESULT_OK;
}

dmExtension::Result InitializeGpgs(dmExtension::Params* params)
{
    if (g_gpgs == NULL) {

        printf("sven hej hej\n");
        g_gpgs = new dmGpgs::GooglePlayGameServices();
        const char* client_id = dmConfigFile::GetString(params->m_ConfigFile, "gpgs.client_id", "");

        gpg::IosPlatformConfiguration platform_configuration;
        platform_configuration.SetClientID(std::string(client_id));

        id delegate = [[GPGSAppDelegate alloc] init];
        [delegate viewDidLoad];
        // platform_configuration.SetOptionalViewControllerForPopups(delegate);
        dmExtension::RegisterUIApplicationDelegate(delegate);
        // platform_configuration.SetOptionalViewControllerForPopups(g_viewController);

        g_gpgs->m_GameServices = gpg::GameServices::Builder()
            .SetOnLog(gpg::DEFAULT_ON_LOG, gpg::LogLevel::VERBOSE)
            .SetOnAuthActionStarted(::OnAuthActionStarted)
            .SetOnAuthActionFinished(::OnAuthActionFinished)
            .EnableSnapshots()
            .Create(platform_configuration);
    }

    lua_State* L = params->m_L;
    int argc = lua_gettop(L);

    luaL_register(L, LIB_NAME, gpgs_methods);
    //dmGpgs::RegisterConstants(L);

    lua_pop(L, 1);
    assert(argc == lua_gettop(L));
    return dmExtension::RESULT_OK;
}

dmExtension::Result UpdateGpgs(dmExtension::Params* params)
{
    dmMutex::ScopedLock _lock(g_gpgs->m_Mutex);

    uint32_t i = 0;
    while (i < g_gpgs->m_Commands.Size())
    {
        dmGpgs::Command cmd = g_gpgs->m_Commands[i];
        if (cmd.m_Context == params->m_L)
        {
            lua_rawgeti(cmd.m_Context, LUA_REGISTRYINDEX, cmd.m_Callback);
            lua_rawgeti(cmd.m_Context, LUA_REGISTRYINDEX, cmd.m_Self);
            lua_pushvalue(cmd.m_Context, -1);
            dmScript::SetInstance(cmd.m_Context);
            if (dmScript::IsInstanceValid(cmd.m_Context))
            {

                dmLogWarning("cmd.m_ParameterStack: %X", cmd.m_ParameterStack);

                int arguments = lua_gettop(cmd.m_ParameterStack);
                ::PushStack(cmd.m_ParameterStack, cmd.m_Context);

                dmLogWarning("arguments: %d", arguments);

                int callback_ref = cmd.m_Callback;
                lua_State* context_ref = cmd.m_Context;

                cmd.m_Callback = LUA_NOREF;
                cmd.m_Self = LUA_NOREF;
                cmd.m_Context = NULL;
                lua_close(cmd.m_ParameterStack);
                cmd.m_ParameterStack = NULL;

                (void) dmScript::PCall(context_ref, arguments + 1,
                    LUA_MULTRET);
                luaL_unref(context_ref, LUA_REGISTRYINDEX, callback_ref);
            }
            else
            {
                dmLogError("Unable to run Google Play Game Services"
                    " callback because the instance has been deleted.");
                lua_pop(cmd.m_Context, 2);
            }

            g_gpgs->m_Commands.EraseSwap(i);
        }
        else
        {
            ++i;
        }
    }

    return dmExtension::RESULT_OK;
}

dmExtension::Result FinalizeGpgs(dmExtension::Params* params)
{
    return dmExtension::RESULT_OK;
}

DM_DECLARE_EXTENSION(GpgsExt, "Gpgs", AppInitializeGpgs, AppFinalizeGpgs,
    InitializeGpgs, UpdateGpgs, 0, FinalizeGpgs)