#include "gpgs.h"
#include "gpgs_types_native.h"

#include <extension/extension.h>

extern dmGpgs::GooglePlayGameServices* g_gpgs = NULL;

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
    g_gpgs = new dmGpgs::GooglePlayGameServices();
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
    return dmExtension::RESULT_OK;
}

dmExtension::Result FinalizeGpgs(dmExtension::Params* params)
{
    return dmExtension::RESULT_OK;
}

DM_DECLARE_EXTENSION(GpgsExt, "Gpgs", AppInitializeGpgs, AppFinalizeGpgs,
    InitializeGpgs, UpdateGpgs, 0, FinalizeGpgs)