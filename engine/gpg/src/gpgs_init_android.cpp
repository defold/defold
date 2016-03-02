#include "gpgs.h"
#include "gpgs_types_native.h"

#include <gpg/android_initialization.h>
#include <gpg/android_support.h>
#include <gpg/game_services.h>
#include <gpg/builder.h>
#include <gpg/default_callbacks.h>
#include <gpg/debug.h>

#include <dlib/log.h>
#include <script/script.h>
#include <extension/extension.h>
#include <android_native_app_glue.h>

extern dmGpgs::GooglePlayGameServices* g_gpgs = NULL;
extern struct android_app* g_AndroidApp;

namespace
{

    void OnActivityResult(void *env, void* activity, int32_t request_code,
        int32_t result_code, void* result)
    {
        gpg::AndroidSupport::OnActivityResult((JNIEnv*) env,
            (jobject) activity, request_code, result_code, (jobject) result);
    }

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

    {"unlock_achievement", dmGpgs::Achievement::Unlock},
    {"reveal_achievement", dmGpgs::Achievement::Reveal},
    {"increment_achievement", dmGpgs::Achievement::Increment},
    {"set_achievement", dmGpgs::Achievement::Set},
    {"fetch_achievement", dmGpgs::Achievement::Fetch},
    {"show_all_achievements", dmGpgs::Achievement::ShowAll},

    {"increment_event", dmGpgs::Event::Increment},
    {"fetch_event", dmGpgs::Event::Fetch},

    {"submit_score", dmGpgs::Leaderboard::SubmitScore},
    {"show_leaderboard", dmGpgs::Leaderboard::Show},
    {"show_all_leaderboards", dmGpgs::Leaderboard::ShowAll},

    {"fetch_player_info", dmGpgs::Player::FetchInformation},
    {"fetch_player_stats", dmGpgs::Player::FetchStatistics},

    {"accept_quest", dmGpgs::Quest::Accept},
    {"claim_quest_milestone", dmGpgs::Quest::ClaimMilestone},
    {"fetch_quest", dmGpgs::Quest::Fetch},
    {"show_quest", dmGpgs::Quest::Show},
    {"show_all_quests", dmGpgs::Quest::ShowAll},

    {"save_snapshot", dmGpgs::Snapshot::Commit},
    {"read_snapshot", dmGpgs::Snapshot::Read},
    {"delete_snapshot", dmGpgs::Snapshot::Delete},
    {"show_snapshot", dmGpgs::Snapshot::Show},
    {0,0}

};

dmExtension::Result AppInitializeGpgs(dmExtension::AppParams* params)
{
    gpg::AndroidInitialization::android_main(::g_AndroidApp);
    dmExtension::RegisterOnActivityResultListener(::OnActivityResult);

    g_gpgs = new dmGpgs::GooglePlayGameServices();
    gpg::AndroidPlatformConfiguration platform_configuration;
    platform_configuration.SetActivity(g_AndroidApp->activity->clazz);

    g_gpgs->m_GameServices = gpg::GameServices::Builder()
        .SetOnLog(gpg::DEFAULT_ON_LOG, gpg::LogLevel::VERBOSE)
        .SetOnAuthActionStarted(::OnAuthActionStarted)
        .SetOnAuthActionFinished(::OnAuthActionFinished)
        .EnableSnapshots()
        .Create(platform_configuration);

    return dmExtension::RESULT_OK;
}

dmExtension::Result AppFinalizeGpgs(dmExtension::AppParams* params)
{
    dmExtension::UnregisterOnActivityResultListener(::OnActivityResult);

    if (g_gpgs != NULL)
    {
        delete g_gpgs;
        g_gpgs = NULL;
    }

    return dmExtension::RESULT_OK;
}

dmExtension::Result InitializeGpgs(dmExtension::Params* params)
{
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
        dmGpgs::Command& cmd = g_gpgs->m_Commands[i];
        if (cmd.m_Context == params->m_L)
        {
            lua_rawgeti(cmd.m_Context, LUA_REGISTRYINDEX, cmd.m_Callback);
            lua_rawgeti(cmd.m_Context, LUA_REGISTRYINDEX, cmd.m_Self);
            lua_pushvalue(cmd.m_Context, -1);
            dmScript::SetInstance(cmd.m_Context);
            if (dmScript::IsInstanceValid(cmd.m_Context))
            {
                int arguments = lua_gettop(cmd.m_ParameterStack);
                ::PushStack(cmd.m_ParameterStack, cmd.m_Context);


                int callback_ref = cmd.m_Callback;
                lua_State* context_ref = cmd.m_Context;

                cmd.m_Callback = LUA_NOREF;
                cmd.m_Self = LUA_NOREF;
                cmd.m_Context = NULL;
                free(cmd.m_ParameterStack);
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