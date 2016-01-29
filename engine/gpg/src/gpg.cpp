#include <script/script.h>
#include <extension/extension.h>
#include <dlib/log.h>

#include <gpg/android_initialization.h>
#include <gpg/android_support.h>
#include <gpg/achievement.h>
#include <gpg/achievement_manager.h>
#include <gpg/builder.h>
#include <gpg/debug.h>
#include <gpg/default_callbacks.h>
#include <gpg/game_services.h>
#include <gpg/leaderboard_manager.h>
#include <gpg/achievement_manager.h>

#include <android_native_app_glue.h>

extern struct android_app* g_AndroidApp;

extern "C"
{
    #include <lua/lauxlib.h>
    #include <lua/lualib.h>
}

#define LIB_NAME "gpg"

struct Gpg
{
    Gpg()
    {
        memset(this, 0, sizeof(*this));
    }

    std::unique_ptr<gpg::GameServices> m_GameServices;
};

Gpg* g_Gpg;

static void OnAuthActionStarted(gpg::AuthOperation auth_operation)
{
    dmLogWarning("STARTED: %d", auth_operation);
}

static void OnAuthActionFinished(gpg::AuthOperation auth_operation, gpg::AuthStatus status)
{
    // TODO: Log warning if status == -3 (NOT_AUTH). Probably wrong certificate
    // OR maybe NOT! We can this on startup as well the first time (autologin)
    dmLogWarning("FINISHED: %d %d", auth_operation, status);
    dmLogWarning("IsAuth: %d", g_Gpg->m_GameServices->IsAuthorized());
}

static int Login(lua_State* L)
{
    g_Gpg->m_GameServices->StartAuthorizationUI();
    return 0;
}

static int Logout(lua_State* L)
{
    g_Gpg->m_GameServices->SignOut();
    return 0;
}

static int ShowLeaderboard(lua_State* L)
{
    const char* leaderboard = luaL_checkstring(L, 1);
    g_Gpg->m_GameServices->Leaderboards().ShowUI(leaderboard);
    return 0;
}

static int SubmitScore(lua_State* L)
{
    const char* leaderboard = luaL_checkstring(L, 1);
    // TODO: Potential problem with double and precision?
    double score = luaL_checknumber(L, 2);
    g_Gpg->m_GameServices->Leaderboards().SubmitScore(leaderboard, (uint64_t) score);
    return 0;
}

static int ShowAchievements(lua_State* L)
{
    g_Gpg->m_GameServices->Achievements().ShowAllUI();
    return 0;
}

static int UnlockAchievement(lua_State* L)
{
    const char* achievement = luaL_checkstring(L, 1);
    g_Gpg->m_GameServices->Achievements().Unlock(achievement);
    return 0;
}

static const luaL_reg Gpg_methods[] =
{
    {"login", Login},
    {"logout", Logout},
    {"show_leaderboard", ShowLeaderboard},
    {"submit_score", SubmitScore},
    {"show_achievements", ShowAchievements},
    {"unlock_achievement", UnlockAchievement},
    {0,0}
};

static const luaL_reg Gpg_meta[] =
{
    {0, 0}
};

void OnActivityResult(void *env, void* activity, int32_t request_code, int32_t result_code, void* result)
{
    gpg::AndroidSupport::OnActivityResult((JNIEnv*) env, (jobject) activity, request_code, result_code, (jobject) result);
}

dmExtension::Result AppInitializeGpg(dmExtension::AppParams* params)
{
    gpg::AndroidInitialization::android_main(g_AndroidApp);
    dmExtension::RegisterOnActivityResultListener(OnActivityResult);

    g_Gpg = new Gpg;
    gpg::AndroidPlatformConfiguration platform_configuration;
    platform_configuration.SetActivity(g_AndroidApp->activity->clazz);

    g_Gpg->m_GameServices = gpg::GameServices::Builder()
        .SetLogging(gpg::DEFAULT_ON_LOG, gpg::LogLevel::VERBOSE)
        .SetOnAuthActionStarted(OnAuthActionStarted)
        .SetOnAuthActionFinished(OnAuthActionFinished)
        .Create(platform_configuration);


    return dmExtension::RESULT_OK;
}

dmExtension::Result InitializeGpg(dmExtension::Params* params)
{
    lua_State*L = params->m_L;
    int top = lua_gettop(L);
    luaL_register(L, LIB_NAME, Gpg_methods);

    lua_pop(L, 1);
    assert(top == lua_gettop(L));

    return dmExtension::RESULT_OK;
}

dmExtension::Result AppFinalizeGpg(dmExtension::AppParams* params)
{
    dmExtension::UnregisterOnActivityResultListener(OnActivityResult);

    delete g_Gpg;
    g_Gpg = 0;
    return dmExtension::RESULT_OK;
}

dmExtension::Result FinalizeGpg(dmExtension::Params* params)
{
    return dmExtension::RESULT_OK;
}

DM_DECLARE_EXTENSION(GpgExt, "Gpg", AppInitializeGpg, AppFinalizeGpg, InitializeGpg, 0, 0, FinalizeGpg)

