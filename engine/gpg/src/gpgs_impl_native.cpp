#include "gpgs.h"
#include "gpgs_types_native.h"

#include <gpg/types.h>
#include <gpg/status.h>
#include <gpg/achievement.h>
#include <gpg/achievement_manager.h>
#include <gpg/event.h>
#include <gpg/event_manager.h>
#include <gpg/leaderboard.h>
#include <gpg/leaderboard_manager.h>
#include <gpg/player.h>
#include <gpg/player_level.h>
#include <gpg/player_manager.h>
#include <gpg/playerstats.h>
#include <gpg/stats_manager.h>
#include <gpg/quest_manager.h>
#include <gpg/quest.h>
#include <gpg/quest_milestone.h>
#include <gpg/snapshot_manager.h>
#include <gpg/snapshot_metadata.h>
#include <gpg/snapshot_metadata_change.h>
#include <gpg/snapshot_metadata_change_builder.h>

#include <dlib/log.h>
#include <script/script.h>

extern dmGpgs::GooglePlayGameServices* g_gpgs;

void Achievement_Fetch_Callback(gpg::AchievementManager::FetchResponse const& response);
void Achievement_ShowAll_Callback(gpg::UIStatus const& response);
void Event_Fetch_Callback(const gpg::EventManager::FetchResponse& response);
void Leaderboard_Show_Callback(gpg::UIStatus const& response);
void Leaderboard_ShowAll_Callback(gpg::UIStatus const& response);
void Player_FetchInformation_Callback(gpg::PlayerManager::FetchSelfResponse const& response);
void Player_FetchStatistics_Callback(const gpg::StatsManager::FetchForPlayerResponse& response);
void Quest_AcceptFetch_Callback(gpg::QuestManager::FetchResponse const& response);
void Quest_Accept_Callback(gpg::QuestManager::AcceptResponse const& response);
void Quest_ClaimMilestoneFetch_Callback(gpg::QuestManager::FetchResponse const& response);
void Quest_ClaimMilestone_Callback(gpg::QuestManager::ClaimMilestoneResponse const& response);
void Quest_Fetch_Callback(gpg::QuestManager::FetchResponse const& response);
void Quest_ShowFetch_Callback(gpg::QuestManager::FetchResponse const& response);
void Quest_Show_Callback(gpg::QuestManager::QuestUIResponse const& response);
void Quest_ShowAll_Callback(gpg::QuestManager::QuestUIResponse const& response);
void Snapshot_Commit_Callback(gpg::SnapshotManager::OpenResponse const & response);
void Snapshot_Delete_Callback(gpg::SnapshotManager::OpenResponse const & response);
void Snapshot_Read_Callback(gpg::SnapshotManager::OpenResponse const & response);
void Snapshot_Show_Callback(gpg::SnapshotManager::SnapshotSelectUIResponse const& response);

namespace
{

    bool EmptyCallback(dmGpgs::Callback callback_type)
    {
        int ct_index = static_cast<int>(callback_type);
        return g_gpgs->m_Callbacks[ct_index].m_Callback == LUA_NOREF;
    }

    void ClearCallback(dmGpgs::Callback callback_type)
    {
        if (!::EmptyCallback(callback_type))
        {
            int ct_index = static_cast<int>(callback_type);
            dmGpgs::Command& command = g_gpgs->m_Callbacks[ct_index];
            luaL_unref(command.m_Context, LUA_REGISTRYINDEX, command.m_Callback);
            luaL_unref(command.m_Context, LUA_REGISTRYINDEX, command.m_Self);

            command.m_Callback = LUA_NOREF;
            command.m_Self = LUA_NOREF;
            command.m_Context = NULL;

            if (command.m_ParameterStack != NULL)
            {
                lua_close(command.m_ParameterStack);
                command.m_ParameterStack = NULL;
            }
        }
    }

    void CallbackError(dmGpgs::Callback callback_type)
    {
        ::ClearCallback(callback_type);
        dmLogError("Callback function was found in an errorneous state.");
    }

    void RegisterCallback(
        lua_State* L, dmGpgs::Callback callback_type, int callback_index)
    {
        dmMutex::ScopedLock _lock(g_gpgs->m_Mutex);
        ::ClearCallback(callback_type);

        int ct_index = static_cast<int>(callback_type);
        dmGpgs::Command& command = g_gpgs->m_Callbacks[ct_index];
        lua_pushvalue(L, callback_index);
        command.m_Callback = luaL_ref(L, LUA_REGISTRYINDEX);
        dmScript::GetInstance(L);
        command.m_Self = luaL_ref(L, LUA_REGISTRYINDEX);
        command.m_Context = L;
        command.m_ParameterStack = luaL_newstate();
    }

    lua_State* GetCallbackStack(dmGpgs::Callback callback_type)
    {
        dmMutex::ScopedLock _lock(g_gpgs->m_Mutex);

        int ct_index = static_cast<int>(callback_type);
        lua_State* state = g_gpgs->m_Callbacks[ct_index].m_ParameterStack;
        return state;
    }

    void QueueCallback(dmGpgs::Callback callback_type)
    {
        dmMutex::ScopedLock _lock(g_gpgs->m_Mutex);
        if (::EmptyCallback(callback_type))
        {
            dmLogError("Attempt to queue empty callback.");
        }
        else
        {
            int ct_index = static_cast<int>(callback_type);
            dmGpgs::Command& command = g_gpgs->m_Callbacks[ct_index];
            g_gpgs->m_Commands.Push(command); // Shallow copy
            command.m_Callback = LUA_NOREF;
            command.m_Self = LUA_NOREF;
            command.m_Context = NULL;
            command.m_ParameterStack = NULL;
        }
    }

    const char* AchievementStateToString(gpg::AchievementState state)
    {
        switch (state)
        {
            case gpg::AchievementState::HIDDEN:
                return "hidden";
            case gpg::AchievementState::UNLOCKED:
                return "unlocked";
            case gpg::AchievementState::REVEALED:
                return "revealed";
            default:
                return "unknown";
        }
    }

    const char* AchievementTypeToString(gpg::AchievementType type)
    {
        switch (type)
        {
            case gpg::AchievementType::STANDARD:
                return "standard";
            case gpg::AchievementType::INCREMENTAL:
                return "incremental";
            default:
                return "unknown";
        }
    }

    const char* EventVisibilityToString(gpg::EventVisibility visibility)
    {
        switch (visibility)
        {
            case gpg::EventVisibility::HIDDEN:
                return "hidden";
            case gpg::EventVisibility::REVEALED:
                return "revealed";
            default:
                return "unknown";
        }
    }

    const char* QuestStateToString(gpg::QuestState state)
    {
        switch (state)
        {
            case gpg::QuestState::UPCOMING:
                return "upcoming";
            case gpg::QuestState::OPEN:
                return "open";
            case gpg::QuestState::ACCEPTED:
                return "accepted";
            case gpg::QuestState::COMPLETED:
                return "completed";
            case gpg::QuestState::EXPIRED:
                return "expired";
            case gpg::QuestState::FAILED:
                return "failed";
            default:
                return "unknown";
        }
    }

    const char* MilestoneStateToString(gpg::QuestMilestoneState state)
    {
        switch (state)
        {
            case gpg::QuestMilestoneState::NOT_STARTED:
                return "not started";
            case gpg::QuestMilestoneState::NOT_COMPLETED:
                return "not completed";
            case gpg::QuestMilestoneState::COMPLETED_NOT_CLAIMED:
                return "not claimed";
            case gpg::QuestMilestoneState::CLAIMED:
                return "claimed";
            default:
                return "unknown";
        }
    }

    gpg::SnapshotConflictPolicy IntegerToConflictPolicy(uint32_t policy)
    {
        switch (policy)
        {
            case static_cast<int>(gpg::SnapshotConflictPolicy::LONGEST_PLAYTIME):
                return gpg::SnapshotConflictPolicy::LONGEST_PLAYTIME;
            case static_cast<int>(gpg::SnapshotConflictPolicy::LAST_KNOWN_GOOD):
                return gpg::SnapshotConflictPolicy::LAST_KNOWN_GOOD;
            case static_cast<int>(gpg::SnapshotConflictPolicy::MOST_RECENTLY_MODIFIED):
                return gpg::SnapshotConflictPolicy::MOST_RECENTLY_MODIFIED;
            case static_cast<int>(gpg::SnapshotConflictPolicy::HIGHEST_PROGRESS):
                return gpg::SnapshotConflictPolicy::HIGHEST_PROGRESS;
            default:
                return gpg::SnapshotConflictPolicy::MOST_RECENTLY_MODIFIED;
        }
    }

    void PushAchievement(
        lua_State* L, const gpg::Achievement& achievement)
    {
        dmGpgs::AddTableEntry(L, "id", achievement.Id().c_str());
        dmGpgs::AddTableEntry(L, "name", achievement.Name().c_str());
        dmGpgs::AddTableEntry(L, "state", ::AchievementStateToString(achievement.State()));
        dmGpgs::AddTableEntry(L, "type", ::AchievementTypeToString(achievement.Type()));
        dmGpgs::AddTableEntry(L, "valid", achievement.Valid());

        dmGpgs::AddTableEntry(L, "description", achievement.Description().c_str());
        dmGpgs::AddTableEntry(L, "current_steps", (double) achievement.CurrentSteps());
        dmGpgs::AddTableEntry(L, "total_steps", (double) achievement.TotalSteps());
        dmGpgs::AddTableEntry(L, "xp", (double) achievement.XP());
        dmGpgs::AddTableEntry(L, "icon_url", achievement.RevealedIconUrl().c_str());

        std::chrono::seconds modified_time = std::chrono::duration_cast<std::chrono::seconds>(achievement.LastModifiedTime());
        dmGpgs::AddTableEntry(L, "last_modified_time", (double) modified_time.count());
    }

    void PushEvent(lua_State* L, const gpg::Event& event)
    {
        dmGpgs::AddTableEntry(L, "id", event.Id().c_str());
        dmGpgs::AddTableEntry(L, "valid", event.Valid());
        dmGpgs::AddTableEntry(L, "name", event.Name().c_str());
        dmGpgs::AddTableEntry(L, "description", event.Description().c_str());
        dmGpgs::AddTableEntry(L, "count", (double) event.Count());
        dmGpgs::AddTableEntry(L, "image_url", event.ImageUrl().c_str());
        dmGpgs::AddTableEntry(L, "visibility", ::EventVisibilityToString(event.Visibility()));
    }

    void PushInformation(lua_State* L, const gpg::Player& player)
    {
        dmGpgs::AddTableEntry(L, "id", player.Id().c_str());
        dmGpgs::AddTableEntry(L, "name", player.Name().c_str());

        dmGpgs::AddTableEntry(L, "icon_url", player.AvatarUrl(gpg::ImageResolution::ICON).c_str());
        dmGpgs::AddTableEntry(L, "hires_url", player.AvatarUrl(gpg::ImageResolution::HI_RES).c_str());

        dmGpgs::AddTableEntry(L, "has_level", player.HasLevelInfo());
        dmGpgs::AddTableEntry(L, "current_level_min", (double) (player.HasLevelInfo() ? player.CurrentLevel().MinimumXP() : 0));
        dmGpgs::AddTableEntry(L, "current_level", (double) (player.HasLevelInfo() ? player.CurrentLevel().LevelNumber() : 0));
        dmGpgs::AddTableEntry(L, "current_level_max", (double) (player.HasLevelInfo() ? player.CurrentLevel().MaximumXP() : 0));
        dmGpgs::AddTableEntry(L, "next_level_min", (double) (player.HasLevelInfo() ? player.NextLevel().MinimumXP() : 0));
        dmGpgs::AddTableEntry(L, "next_level", (double) (player.HasLevelInfo() ? player.NextLevel().LevelNumber() : 0));
        dmGpgs::AddTableEntry(L, "next_level_max", (double) (player.HasLevelInfo() ? player.NextLevel().MaximumXP() : 0));
        dmGpgs::AddTableEntry(L, "current_level_xp", (double) player.CurrentXP());
        std::chrono::seconds duration = std::chrono::duration_cast<std::chrono::seconds>(player.LastLevelUpTime());
        dmGpgs::AddTableEntry(L, "current_level_timestamp", (double) duration.count());

        dmGpgs::AddTableEntry(L, "title", player.Title().c_str());
    }

    void PushStatistics(lua_State* L, const gpg::PlayerStats& stats)
    {
        dmGpgs::AddTableEntry(L, "has_average_session_length", (bool) stats.HasAverageSessionLength());
        dmGpgs::AddTableEntry(L, "average_session_length", (double) stats.AverageSessionLength());
        dmGpgs::AddTableEntry(L, "has_churn_probability", (bool) stats.HasChurnProbability());
        dmGpgs::AddTableEntry(L, "churn_probability", (double) stats.ChurnProbability());
        dmGpgs::AddTableEntry(L, "has_days_since_last_played", (bool) stats.HasDaysSinceLastPlayed());
        dmGpgs::AddTableEntry(L, "days_since_last_played", (double) stats.DaysSinceLastPlayed());
        dmGpgs::AddTableEntry(L, "has_number_of_purchases", (bool) stats.HasNumberOfPurchases());
        dmGpgs::AddTableEntry(L, "number_of_purchases", (double) stats.NumberOfPurchases());
        dmGpgs::AddTableEntry(L, "has_number_of_sessions", (bool) stats.HasNumberOfSessions());
        dmGpgs::AddTableEntry(L, "number_of_sessions", (double) stats.NumberOfSessions());
        dmGpgs::AddTableEntry(L, "has_session_percentile", (bool) stats.HasSessionPercentile());
        dmGpgs::AddTableEntry(L, "session_percentile", (double) stats.SessionPercentile());
        dmGpgs::AddTableEntry(L, "has_spend_percentile", (bool) stats.HasSpendPercentile());
        dmGpgs::AddTableEntry(L, "spend_percentile", (double) stats.SpendPercentile());
    }

    void PushQuest(lua_State* L, const gpg::Quest& quest)
    {
        dmGpgs::AddTableEntry(L, "id", quest.Id().c_str());
        dmGpgs::AddTableEntry(L, "name", quest.Name().c_str());
        dmGpgs::AddTableEntry(L, "description", quest.Description().c_str());
        dmGpgs::AddTableEntry(L, "state", ::QuestStateToString(quest.State()));
        dmGpgs::AddTableEntry(L, "valid", (bool) quest.Valid());
        std::chrono::seconds accepted_time = std::chrono::duration_cast<std::chrono::seconds>(quest.AcceptedTime());
        dmGpgs::AddTableEntry(L, "accepted_time", (double) accepted_time.count());
        std::chrono::seconds start_time = std::chrono::duration_cast<std::chrono::seconds>(quest.StartTime());
        dmGpgs::AddTableEntry(L, "start_time", (double) start_time.count());
        std::chrono::seconds expiration_time = std::chrono::duration_cast<std::chrono::seconds>(quest.ExpirationTime());
        dmGpgs::AddTableEntry(L, "expiration_time", (double) expiration_time.count());
        dmGpgs::AddTableEntry(L, "banner_url", quest.BannerUrl().c_str());
        dmGpgs::AddTableEntry(L, "icon_url", quest.IconUrl().c_str());
        dmGpgs::AddTableEntry(L, "current_milestone", quest.CurrentMilestone().Id().c_str());
    }

    void PushMilestone(lua_State* L, const gpg::QuestMilestone& milestone)
    {
        dmGpgs::AddTableEntry(L, "id", milestone.Id().c_str());
        dmGpgs::AddTableEntry(L, "state", ::MilestoneStateToString(milestone.State()));
        dmGpgs::AddTableEntry(L, "event", milestone.EventId().c_str());
        dmGpgs::AddTableEntry(L, "quest", milestone.QuestId().c_str());
        dmGpgs::AddTableEntry(L, "valid", (bool) milestone.Valid());
        dmGpgs::AddTableEntry(L, "current_count", (double) milestone.CurrentCount());
        dmGpgs::AddTableEntry(L, "target_count", (double) milestone.TargetCount());
        const std::vector<unsigned char>& reward_data_buffer = milestone.CompletionRewardData();
        std::string reward_data(reward_data_buffer.begin(), reward_data_buffer.end());
        dmGpgs::AddTableEntry(L, "reward_data", reward_data.c_str());
    }

    void PushSnapshotMetadata(lua_State* L, const gpg::SnapshotMetadata& snapshot)
    {
        dmGpgs::AddTableEntry(L, "filename", snapshot.FileName().c_str());
        dmGpgs::AddTableEntry(L, "description", snapshot.Description().c_str());
        dmGpgs::AddTableEntry(L, "valid", snapshot.Valid());
        dmGpgs::AddTableEntry(L, "is_open", snapshot.IsOpen());
        std::chrono::seconds played_time = std::chrono::duration_cast<std::chrono::seconds>(snapshot.PlayedTime());
        dmGpgs::AddTableEntry(L, "played_time", (double) played_time.count());
        std::chrono::seconds modified_time = std::chrono::duration_cast<std::chrono::seconds>(snapshot.LastModifiedTime());
        dmGpgs::AddTableEntry(L, "last_modified_time", (double) modified_time.count());
        dmGpgs::AddTableEntry(L, "progress_value", (double) snapshot.ProgressValue());
        dmGpgs::AddTableEntry(L, "cover_image_url", snapshot.CoverImageURL().c_str());
    }

};



bool dmGpgs::Authenticated()
{
    return g_gpgs->m_GameServices->IsAuthorized();
}

void dmGpgs::RegisterConstants(lua_State* L)
{
    // Add constants to table LIB_NAME
    lua_getglobal(L, LIB_NAME);

    #define SETCONSTANT(name, val) \
        lua_pushnumber(L, (lua_Number) val); lua_setfield(L, -2, #name);

    SETCONSTANT(POLICY_LONGEST_PLAYTIME,        static_cast<int>(gpg::SnapshotConflictPolicy::LONGEST_PLAYTIME));
    SETCONSTANT(POLICY_LAST_KNOWN_GOOD,         static_cast<int>(gpg::SnapshotConflictPolicy::LAST_KNOWN_GOOD));
    SETCONSTANT(POLICY_MOST_RECENTLY_MODIFIED,  static_cast<int>(gpg::SnapshotConflictPolicy::MOST_RECENTLY_MODIFIED));
    SETCONSTANT(POLICY_HIGHEST_PROGRESS,        static_cast<int>(gpg::SnapshotConflictPolicy::HIGHEST_PROGRESS));

    #undef SETCONSTANT

    // Pop table LIB_NAME
    lua_pop(L, 1);
}




// ----------------------------------------------------------------------------
// Authentication::Login
// ----------------------------------------------------------------------------
void dmGpgs::Authentication::Impl::Login(lua_State* L, int callback)
{
    ::RegisterCallback(L, dmGpgs::Callback::AUTHENTICATION_LOGIN, callback);
    g_gpgs->m_GameServices->StartAuthorizationUI();
}

void dmGpgs::Authentication::Impl::LoginCallback()
{
    // The authentication callback is handled differently between platforms
    // and therefore this method is called from gpgs_init.
    if (!::EmptyCallback(dmGpgs::Callback::AUTHENTICATION_LOGIN))
    {
        lua_State* L = ::GetCallbackStack(dmGpgs::Callback::AUTHENTICATION_LOGIN);

        lua_pushboolean(L, g_gpgs->m_GameServices->IsAuthorized());

        if (!dmGpgs::Authentication::Callback::Login(L))
            return ::CallbackError(dmGpgs::Callback::AUTHENTICATION_LOGIN);
        ::QueueCallback(dmGpgs::Callback::AUTHENTICATION_LOGIN);
    }
}



// ----------------------------------------------------------------------------
// Authentication::GetLoginStatus
// ----------------------------------------------------------------------------
void dmGpgs::Authentication::Impl::GetLoginStatus(lua_State* L)
{
    lua_pushboolean(L, g_gpgs->m_GameServices->IsAuthorized());
}



// ----------------------------------------------------------------------------
// Authentication::Logout
// ----------------------------------------------------------------------------
void dmGpgs::Authentication::Impl::Logout(lua_State* L, int callback)
{
    ::RegisterCallback(L, dmGpgs::Callback::AUTHENTICATION_LOGOUT, callback);
    g_gpgs->m_GameServices->SignOut();
}

void dmGpgs::Authentication::Impl::LogoutCallback()
{
    // The authentication callback is handled differently between platforms
    // and therefore this method is called from gpgs_init.
    if (!::EmptyCallback(dmGpgs::Callback::AUTHENTICATION_LOGOUT))
    {
        lua_State* L = ::GetCallbackStack(dmGpgs::Callback::AUTHENTICATION_LOGOUT);

        lua_pushboolean(L, g_gpgs->m_GameServices->IsAuthorized());

        if (!dmGpgs::Authentication::Callback::Logout(L))
            return ::CallbackError(dmGpgs::Callback::AUTHENTICATION_LOGOUT);
        ::QueueCallback(dmGpgs::Callback::AUTHENTICATION_LOGOUT);
    }
}



// ----------------------------------------------------------------------------
// Achievement::Unlock
// ----------------------------------------------------------------------------
void dmGpgs::Achievement::Impl::Unlock(lua_State* L, const char* identifier)
{
    g_gpgs->m_GameServices->Achievements().Unlock(identifier);
}



// ----------------------------------------------------------------------------
// Achievement::Reveal
// ----------------------------------------------------------------------------
void dmGpgs::Achievement::Impl::Reveal(lua_State* L, const char* identifier)
{
    g_gpgs->m_GameServices->Achievements().Reveal(identifier);
}



// ----------------------------------------------------------------------------
// Achievement::Increment
// ----------------------------------------------------------------------------
void dmGpgs::Achievement::Impl::Increment(lua_State* L, const char* identifier, uint32_t steps)
{
    g_gpgs->m_GameServices->Achievements().Increment(identifier, steps);
}



// ----------------------------------------------------------------------------
// Achievement::Set
// ----------------------------------------------------------------------------
void dmGpgs::Achievement::Impl::Set(lua_State* L, const char* identifier, uint32_t steps)
{
    g_gpgs->m_GameServices->Achievements().SetStepsAtLeast(identifier, steps);
}



// ----------------------------------------------------------------------------
// Achievement::Fetch
// ----------------------------------------------------------------------------
void dmGpgs::Achievement::Impl::Fetch(lua_State* L, const char* identifier, int callback)
{
    ::RegisterCallback(L, dmGpgs::Callback::ACHIEVEMENT_FETCH, callback);
    g_gpgs->m_GameServices->Achievements().Fetch(identifier, Achievement_Fetch_Callback);
}

void Achievement_Fetch_Callback(gpg::AchievementManager::FetchResponse const& response)
{
    lua_State* L = ::GetCallbackStack(dmGpgs::Callback::ACHIEVEMENT_FETCH);

    lua_pushboolean(L, gpg::IsSuccess(response.status));
    lua_newtable(L); // Achievement
    ::PushAchievement(L, response.data);

    if (!dmGpgs::Achievement::Callback::Fetch(L))
        return ::CallbackError(dmGpgs::Callback::ACHIEVEMENT_FETCH);
    ::QueueCallback(dmGpgs::Callback::ACHIEVEMENT_FETCH);
}



// ----------------------------------------------------------------------------
// Achievement::ShowAll
// ----------------------------------------------------------------------------
void dmGpgs::Achievement::Impl::ShowAll(lua_State* L, int callback)
{
    ::RegisterCallback(L, dmGpgs::Callback::ACHIEVEMENT_SHOWALL, callback);
    g_gpgs->m_GameServices->Achievements().ShowAllUI(Achievement_ShowAll_Callback);
}

void Achievement_ShowAll_Callback(gpg::UIStatus const& response)
{
    lua_State* L = ::GetCallbackStack(dmGpgs::Callback::ACHIEVEMENT_SHOWALL);

    lua_pushboolean(L, gpg::IsSuccess(response));

    if (!dmGpgs::Achievement::Callback::ShowAll(L))
        return ::CallbackError(dmGpgs::Callback::ACHIEVEMENT_SHOWALL);
    ::QueueCallback(dmGpgs::Callback::ACHIEVEMENT_SHOWALL);
}



// ----------------------------------------------------------------------------
// Event::Increment
// ----------------------------------------------------------------------------
void dmGpgs::Event::Impl::Increment(lua_State* L, const char* identifier, uint32_t steps)
{
    g_gpgs->m_GameServices->Events().Increment(identifier, steps);
}



// ----------------------------------------------------------------------------
// Event::Fetch
// ----------------------------------------------------------------------------
void dmGpgs::Event::Impl::Fetch(lua_State* L, const char* identifier, int callback)
{
    ::RegisterCallback(L, dmGpgs::Callback::EVENT_FETCH, callback);
    g_gpgs->m_GameServices->Events().Fetch(identifier, Event_Fetch_Callback);
}

void Event_Fetch_Callback(const gpg::EventManager::FetchResponse& response)
{
    lua_State* L = ::GetCallbackStack(dmGpgs::Callback::EVENT_FETCH);

    lua_pushboolean(L, gpg::IsSuccess(response.status));
    lua_newtable(L); // Event
    ::PushEvent(L, response.data);

    if (!dmGpgs::Event::Callback::Fetch(L))
        return ::CallbackError(dmGpgs::Callback::EVENT_FETCH);
    ::QueueCallback(dmGpgs::Callback::EVENT_FETCH);
}



// ----------------------------------------------------------------------------
// Leaderboard::SubmitScore
// ----------------------------------------------------------------------------
void dmGpgs::Leaderboard::Impl::SubmitScore(lua_State* L, const char* identifier, double score, const char* description)
{
    if (description != NULL)
    {
        g_gpgs->m_GameServices->Leaderboards().SubmitScore(
            identifier, score, description);
    }
    else
    {
        g_gpgs->m_GameServices->Leaderboards().SubmitScore(identifier, score);
    }
}



// ----------------------------------------------------------------------------
// Leaderboard::Show
// ----------------------------------------------------------------------------
void dmGpgs::Leaderboard::Impl::Show(lua_State* L, const char* identifier, int callback)
{
    ::RegisterCallback(L, dmGpgs::Callback::LEADERBOARD_SHOW, callback);
    g_gpgs->m_GameServices->Leaderboards().ShowUI(identifier, Leaderboard_Show_Callback);
}

void Leaderboard_Show_Callback(gpg::UIStatus const& response)
{
    lua_State* L = ::GetCallbackStack(dmGpgs::Callback::LEADERBOARD_SHOW);

    lua_pushboolean(L, gpg::IsSuccess(response));

    if (!dmGpgs::Leaderboard::Callback::Show(L))
        return ::CallbackError(dmGpgs::Callback::LEADERBOARD_SHOW);
    ::QueueCallback(dmGpgs::Callback::LEADERBOARD_SHOW);
}



// ----------------------------------------------------------------------------
// Leaderboard::ShowAll
// ----------------------------------------------------------------------------
void dmGpgs::Leaderboard::Impl::ShowAll(lua_State* L, int callback)
{
    ::RegisterCallback(L, dmGpgs::Callback::LEADERBOARD_SHOWALL, callback);
    g_gpgs->m_GameServices->Leaderboards().ShowAllUI(Leaderboard_ShowAll_Callback);
}

void Leaderboard_ShowAll_Callback(gpg::UIStatus const& response)
{
    lua_State* L = ::GetCallbackStack(dmGpgs::Callback::LEADERBOARD_SHOWALL);

    lua_pushboolean(L, gpg::IsSuccess(response));

    if (!dmGpgs::Leaderboard::Callback::ShowAll(L))
        return ::CallbackError(dmGpgs::Callback::LEADERBOARD_SHOWALL);
    ::QueueCallback(dmGpgs::Callback::LEADERBOARD_SHOWALL);
}



// ----------------------------------------------------------------------------
// Player::FetchInformation
// ----------------------------------------------------------------------------
void dmGpgs::Player::Impl::FetchInformation(lua_State* L, int callback)
{
    ::RegisterCallback(L, dmGpgs::Callback::PLAYER_FETCH_INFORMATION, callback);
    g_gpgs->m_GameServices->Players().FetchSelf(Player_FetchInformation_Callback);
}

void Player_FetchInformation_Callback(gpg::PlayerManager::FetchSelfResponse const& response)
{
    lua_State* L = ::GetCallbackStack(dmGpgs::Callback::PLAYER_FETCH_INFORMATION);

    lua_pushboolean(L, gpg::IsSuccess(response.status));
    lua_newtable(L); // Information
    ::PushInformation(L, response.data);

    if (!dmGpgs::Player::Callback::FetchInformation(L))
        return ::CallbackError(dmGpgs::Callback::PLAYER_FETCH_INFORMATION);
    ::QueueCallback(dmGpgs::Callback::PLAYER_FETCH_INFORMATION);
}



// ----------------------------------------------------------------------------
// Player::FetchStatistics
// ----------------------------------------------------------------------------
void dmGpgs::Player::Impl::FetchStatistics(lua_State* L, int callback)
{
    ::RegisterCallback(L, dmGpgs::Callback::PLAYER_FETCH_STATISTICS, callback);
    g_gpgs->m_GameServices->Stats().FetchForPlayer(Player_FetchStatistics_Callback);
}

void Player_FetchStatistics_Callback(const gpg::StatsManager::FetchForPlayerResponse& response)
{
    lua_State* L = ::GetCallbackStack(dmGpgs::Callback::PLAYER_FETCH_STATISTICS);

    lua_pushboolean(L, gpg::IsSuccess(response.status));
    lua_newtable(L); // Statistics
    ::PushStatistics(L, response.data);

    if (!dmGpgs::Player::Callback::FetchStatistics(L))
        return ::CallbackError(dmGpgs::Callback::PLAYER_FETCH_STATISTICS);
    ::QueueCallback(dmGpgs::Callback::PLAYER_FETCH_STATISTICS);
}



// ----------------------------------------------------------------------------
// Quest::Accept
// ----------------------------------------------------------------------------
void dmGpgs::Quest::Impl::Accept(lua_State* L, const char* identifier, int callback)
{
    ::RegisterCallback(L, dmGpgs::Callback::QUEST_ACCEPT, callback);
    g_gpgs->m_GameServices->Quests().Fetch(identifier, Quest_AcceptFetch_Callback);
}

void Quest_AcceptFetch_Callback(gpg::QuestManager::FetchResponse const& response)
{
    if (gpg::IsSuccess(response.status))
    {
        g_gpgs->m_GameServices->Quests().Accept(response.data, Quest_Accept_Callback);
    }
    else
    {
        lua_State* L = ::GetCallbackStack(dmGpgs::Callback::QUEST_ACCEPT);
        lua_pushboolean(L, false);
        lua_newtable(L); // Quest
        ::PushQuest(L, response.data);
        lua_newtable(L); // Milestone
        ::PushMilestone(L, response.data.CurrentMilestone());

        if (!dmGpgs::Quest::Callback::Accept(L))
            return ::CallbackError(dmGpgs::Callback::QUEST_ACCEPT);
        ::QueueCallback(dmGpgs::Callback::QUEST_ACCEPT);
    }
}

void Quest_Accept_Callback(gpg::QuestManager::AcceptResponse const& response)
{
    lua_State* L = ::GetCallbackStack(dmGpgs::Callback::QUEST_ACCEPT);
    lua_pushboolean(L, gpg::IsSuccess(response.status));
    lua_newtable(L); // Quest
    ::PushQuest(L, response.accepted_quest);
    lua_newtable(L); // Milestone
    ::PushMilestone(L, response.accepted_quest.CurrentMilestone());

    if (!dmGpgs::Quest::Callback::Accept(L))
        return ::CallbackError(dmGpgs::Callback::QUEST_ACCEPT);
    ::QueueCallback(dmGpgs::Callback::QUEST_ACCEPT);
}



// ----------------------------------------------------------------------------
// Quest::ClaimMilestone
// ----------------------------------------------------------------------------
void dmGpgs::Quest::Impl::ClaimMilestone(lua_State* L, const char* identifier, int callback)
{
    ::RegisterCallback(L, dmGpgs::Callback::QUEST_CLAIM_MILESTONE, callback);
    g_gpgs->m_GameServices->Quests().Fetch(identifier, Quest_ClaimMilestoneFetch_Callback);
}

void Quest_ClaimMilestoneFetch_Callback(gpg::QuestManager::FetchResponse const& response)
{
    if (gpg::IsSuccess(response.status))
    {
        g_gpgs->m_GameServices->Quests().ClaimMilestone(response.data.CurrentMilestone(), Quest_ClaimMilestone_Callback);
    }
    else
    {
        lua_State* L = ::GetCallbackStack(dmGpgs::Callback::QUEST_CLAIM_MILESTONE);
        lua_pushboolean(L, false);
        lua_newtable(L); // Quest
        ::PushQuest(L, response.data);
        lua_newtable(L); // Milestone
        ::PushMilestone(L, response.data.CurrentMilestone());

        if (!dmGpgs::Quest::Callback::ClaimMilestone(L))
            return ::CallbackError(dmGpgs::Callback::QUEST_CLAIM_MILESTONE);
        ::QueueCallback(dmGpgs::Callback::QUEST_CLAIM_MILESTONE);
    }
}

void Quest_ClaimMilestone_Callback(gpg::QuestManager::ClaimMilestoneResponse const& response)
{
    lua_State* L = ::GetCallbackStack(dmGpgs::Callback::QUEST_CLAIM_MILESTONE);
    lua_pushboolean(L, gpg::IsSuccess(response.status));
    lua_newtable(L); // Quest
    ::PushQuest(L, response.quest);
    lua_newtable(L); // Milestone
    ::PushMilestone(L, response.claimed_milestone);

    if (!dmGpgs::Quest::Callback::ClaimMilestone(L))
        return ::CallbackError(dmGpgs::Callback::QUEST_CLAIM_MILESTONE);
    ::QueueCallback(dmGpgs::Callback::QUEST_CLAIM_MILESTONE);
}



// ----------------------------------------------------------------------------
// Quest::Fetch
// ----------------------------------------------------------------------------
void dmGpgs::Quest::Impl::Fetch(lua_State* L, const char* identifier, int callback)
{
    ::RegisterCallback(L, dmGpgs::Callback::QUEST_FETCH, callback);
    g_gpgs->m_GameServices->Quests().Fetch(identifier, Quest_Fetch_Callback);
}

void Quest_Fetch_Callback(gpg::QuestManager::FetchResponse const& response)
{
    lua_State* L = ::GetCallbackStack(dmGpgs::Callback::QUEST_FETCH);
    lua_pushboolean(L, gpg::IsSuccess(response.status));
    lua_newtable(L); // Quest
    ::PushQuest(L, response.data);
    lua_newtable(L); // Milestone
    ::PushMilestone(L, response.data.CurrentMilestone());

    if (!dmGpgs::Quest::Callback::Fetch(L))
        return ::CallbackError(dmGpgs::Callback::QUEST_FETCH);
    ::QueueCallback(dmGpgs::Callback::QUEST_FETCH);
}



// ----------------------------------------------------------------------------
// Quest::Show
// ----------------------------------------------------------------------------
void dmGpgs::Quest::Impl::Show(lua_State* L, const char* identifier, int callback)
{
    ::RegisterCallback(L, dmGpgs::Callback::QUEST_SHOW, callback);
    g_gpgs->m_GameServices->Quests().Fetch(identifier, Quest_ShowFetch_Callback);
}

void Quest_ShowFetch_Callback(gpg::QuestManager::FetchResponse const& response)
{
    if (gpg::IsSuccess(response.status))
    {
        g_gpgs->m_GameServices->Quests().ShowUI(response.data, Quest_Show_Callback);
    }
    else
    {
        lua_State* L = ::GetCallbackStack(dmGpgs::Callback::QUEST_SHOW);
        lua_pushboolean(L, false);
        lua_newtable(L); // Quest
        ::PushQuest(L, response.data);
        lua_newtable(L); // Milestone
        ::PushMilestone(L, response.data.CurrentMilestone());

        if (!dmGpgs::Quest::Callback::Show(L))
            return ::CallbackError(dmGpgs::Callback::QUEST_SHOW);
        ::QueueCallback(dmGpgs::Callback::QUEST_SHOW);
    }
}

void Quest_Show_Callback(gpg::QuestManager::QuestUIResponse const& response)
{
    lua_State* L = ::GetCallbackStack(dmGpgs::Callback::QUEST_SHOW);
    lua_pushboolean(L, gpg::IsSuccess(response.status));
    lua_newtable(L); // Quest
    ::PushQuest(L, response.accepted_quest);
    lua_newtable(L); // Milestone
    ::PushMilestone(L, response.milestone_to_claim);

    if (!dmGpgs::Quest::Callback::Show(L))
        return ::CallbackError(dmGpgs::Callback::QUEST_SHOW);
    ::QueueCallback(dmGpgs::Callback::QUEST_SHOW);
}



// ----------------------------------------------------------------------------
// Quest::ShowAll
// ----------------------------------------------------------------------------
void dmGpgs::Quest::Impl::ShowAll(lua_State* L, int callback)
{
    ::RegisterCallback(L, dmGpgs::Callback::QUEST_SHOWALL, callback);
    g_gpgs->m_GameServices->Quests().ShowAllUI(Quest_ShowAll_Callback);
}

void Quest_ShowAll_Callback(gpg::QuestManager::QuestUIResponse const& response)
{
    lua_State* L = ::GetCallbackStack(dmGpgs::Callback::QUEST_SHOWALL);

    lua_pushboolean(L, gpg::IsSuccess(response.status));
    lua_newtable(L); // Quest
    ::PushQuest(L, response.accepted_quest);
    lua_newtable(L); // Milestone
    ::PushMilestone(L, response.milestone_to_claim);

    if (!dmGpgs::Quest::Callback::ShowAll(L))
        return ::CallbackError(dmGpgs::Callback::QUEST_SHOWALL);
    ::QueueCallback(dmGpgs::Callback::QUEST_SHOWALL);
}



// ----------------------------------------------------------------------------
// Snapshot::Commit
// ----------------------------------------------------------------------------
void dmGpgs::Snapshot::Impl::Commit(lua_State* L, const char* filename, uint32_t policy, const char* description, uint32_t time_played, int32_t progress, char* buffer, int buffer_len, int callback)
{
    std::chrono::milliseconds time_played_ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::seconds(time_played));
    gpg::SnapshotConflictPolicy conflict_policy = ::IntegerToConflictPolicy(policy);

    dmGpgs::CommitInfo* commit_info = (dmGpgs::CommitInfo*) malloc(sizeof(dmGpgs::CommitInfo));
    commit_info->description = strdup(description);
    commit_info->time_played_ms = time_played_ms.count();
    commit_info->progress = progress;
    commit_info->buffer = buffer;
    commit_info->buffer_len = buffer_len;

    ::RegisterCallback(L, dmGpgs::Callback::SNAPSHOT_COMMIT, callback);
    int ct_index = static_cast<int>(dmGpgs::Callback::SNAPSHOT_COMMIT);
    g_gpgs->m_Callbacks[ct_index].m_info = (void*) commit_info;

    g_gpgs->m_GameServices->Snapshots().Open(filename, conflict_policy, Snapshot_Commit_Callback);
}

void Snapshot_Commit_Callback(gpg::SnapshotManager::OpenResponse const & response)
{
    lua_State* L = ::GetCallbackStack(dmGpgs::Callback::SNAPSHOT_COMMIT);

    int ct_index = static_cast<int>(dmGpgs::Callback::SNAPSHOT_COMMIT);
    dmGpgs::CommitInfo* commit_info = (dmGpgs::CommitInfo*) g_gpgs->m_Callbacks[ct_index].m_info;

    std::vector<uint8_t> buffer(commit_info->buffer, commit_info->buffer + commit_info->buffer_len);

    if (gpg::IsSuccess(response.status))
    {
        const gpg::SnapshotMetadata& metadata = response.data;
        gpg::SnapshotMetadataChange metadata_change =
            gpg::SnapshotMetadataChange::Builder()
                .SetDescription(commit_info->description)
                .SetPlayedTime(std::chrono::milliseconds(commit_info->time_played_ms))
                .SetProgressValue(commit_info->progress)
                .Create();

        const gpg::SnapshotManager::CommitResponse& commit_response = g_gpgs->m_GameServices->Snapshots().CommitBlocking(metadata, metadata_change, buffer);

        lua_pushboolean(L, gpg::IsSuccess(commit_response.status));
        lua_newtable(L); // Metadata
        if (gpg::IsSuccess(commit_response.status))
        {
            ::PushSnapshotMetadata(L, commit_response.data);
        }
    }
    else
    {
        lua_pushboolean(L, false);
        lua_newtable(L); // Metadata
    }

    free((void*) commit_info->description);
    free((void*) commit_info->buffer);
    free(commit_info);
    if (!dmGpgs::Snapshot::Callback::Commit(L))
        return ::CallbackError(dmGpgs::Callback::SNAPSHOT_COMMIT);
    ::QueueCallback(dmGpgs::Callback::SNAPSHOT_COMMIT);
}



// ----------------------------------------------------------------------------
// Snapshot::Delete
// ----------------------------------------------------------------------------
void dmGpgs::Snapshot::Impl::Delete(lua_State* L, const char* filename, int callback)
{
    ::RegisterCallback(L, dmGpgs::Callback::SNAPSHOT_DELETE, callback);
    g_gpgs->m_GameServices->Snapshots().Open(filename, gpg::SnapshotConflictPolicy::LAST_KNOWN_GOOD, Snapshot_Delete_Callback);
}

void Snapshot_Delete_Callback(gpg::SnapshotManager::OpenResponse const & response)
{
    lua_State* L = ::GetCallbackStack(dmGpgs::Callback::SNAPSHOT_DELETE);
    if (gpg::IsSuccess(response.status))
    {
        g_gpgs->m_GameServices->Snapshots().Delete(response.data);
        lua_pushboolean(L, true);
    }
    else
    {
        lua_pushboolean(L, false);
    }

    if (!dmGpgs::Snapshot::Callback::Delete(L))
        return ::CallbackError(dmGpgs::Callback::SNAPSHOT_DELETE);
    ::QueueCallback(dmGpgs::Callback::SNAPSHOT_DELETE);
}



// ----------------------------------------------------------------------------
// Snapshot::Read
// ----------------------------------------------------------------------------
void dmGpgs::Snapshot::Impl::Read(lua_State* L, const char* filename, uint32_t policy, int callback)
{
    gpg::SnapshotConflictPolicy conflict_policy = ::IntegerToConflictPolicy(policy);

    ::RegisterCallback(L, dmGpgs::Callback::SNAPSHOT_READ, callback);
    g_gpgs->m_GameServices->Snapshots().Open(filename, conflict_policy, Snapshot_Read_Callback);
}

void Snapshot_Read_Callback(gpg::SnapshotManager::OpenResponse const & response)
{
    lua_State* L = ::GetCallbackStack(dmGpgs::Callback::SNAPSHOT_READ);

    if (gpg::IsSuccess(response.status))
    {
        const gpg::SnapshotManager::ReadResponse& read_response = g_gpgs->m_GameServices->Snapshots().ReadBlocking(response.data);

        lua_pushboolean(L, gpg::IsSuccess(read_response.status));
        if (!read_response.data.empty())
        {
            dmScript::PushTable(L, (const char*) read_response.data.data());
        }
        else
        {
            lua_newtable(L);
        }
    }
    else
    {
        lua_pushboolean(L, false);
        lua_newtable(L);
    }

    if (!dmGpgs::Snapshot::Callback::Read(L))
        return ::CallbackError(dmGpgs::Callback::SNAPSHOT_READ);
    ::QueueCallback(dmGpgs::Callback::SNAPSHOT_READ);
}



// ----------------------------------------------------------------------------
// Snapshot::Show
// ----------------------------------------------------------------------------
void dmGpgs::Snapshot::Impl::Show(lua_State* L, const char* title, int callback)
{
    ::RegisterCallback(L, dmGpgs::Callback::SNAPSHOT_SHOW, callback);
    g_gpgs->m_GameServices->Snapshots().ShowSelectUIOperation(false, true, 10, title, Snapshot_Show_Callback);
}

void Snapshot_Show_Callback(gpg::SnapshotManager::SnapshotSelectUIResponse const& response)
{
    lua_State* L = ::GetCallbackStack(dmGpgs::Callback::SNAPSHOT_SHOW);

    lua_pushboolean(L, gpg::IsSuccess(response.status));
    lua_newtable(L); // Snapshot
    ::PushSnapshotMetadata(L, response.data);

    if (!dmGpgs::Snapshot::Callback::Show(L))
        return ::CallbackError(dmGpgs::Callback::SNAPSHOT_SHOW);
    ::QueueCallback(dmGpgs::Callback::SNAPSHOT_SHOW);
}
