#ifndef H_GPGS
#define H_GPGS

#include <vector>
extern "C"
{
    #include <lua/lua.h>
    #include <lua/lauxlib.h>
    #include <lua/lualib.h>
}

#define LIB_NAME "gpgs"

namespace dmGpgs
{

    inline void AddTableEntry(lua_State* L, const char* key, const char* value)
    {
        lua_pushstring(L, value);
        lua_setfield(L, -2, key);
    }

    inline void AddTableEntry(lua_State* L, const char* key, double value)
    {
        lua_pushnumber(L, value);
        lua_setfield(L, -2, key);
    }

    inline void AddTableEntry(lua_State* L, const char* key, bool value)
    {
        lua_pushboolean(L, value);
        lua_setfield(L, -2, key);
    }

    bool Authenticated();

    void RegisterConstants(lua_State* L);

    namespace Authentication
    {

        /*# Initialite a Google Play Games login
         *
         * This function will open up the Google Play Game interface that allows a user to log into Google Play Games with his/her account.
         *
         * @name login
         * @param callback Callback function with parameters (self, status). The callback is executed when the authentication request towards Google is complete. The parameter status will be true if the user was already authenticated, or if the authentication request was successful, and false otherwise. (function)
         *
         * @examples
         * <pre>
         * gpgs.login(function(self, status)
         *     if not status then
         *         print("Failed to login to Google Play Games.")
         *         return
         *     end
         * end)
         * </pre>
         */
        int Login(lua_State* L);

        /*# Get the current authentication status for Google Play Games
         *
         * This function returns the current authentication status for Google Play Games.
         *
         * @name get_login_status
         * @return Authentication status, true if the user is authenticated towards Google Play Games, false otherwise. (bool)
         *
         * @examples
         * <pre>
         * if not gpgs.get_login_status() then
         *     gpgs.login(function(self, status)
         *         -- Handle login logic ...
         *     end)
         * end
         * </pre>
         */
        int GetLoginStatus(lua_State* L);

        /*# Logout from Google Play Games
         *
         * This function will attempt to logout the user from Google Play Games. If the user is not authenticated this function will have no effect.
         *
         * @name logout
         * @param callback Callback function with parameters (self, status). The callback is executed when the logout request towards Google is complete. The parameter status indicates whether the user is authenticated or not and will be true if the user is still authenticated after the request has completed, but false if the user is no longer authenticated. (function)
         *
         * @examples
         * <pre>
         * if gpgs.get_login_status() then
         *     gpgs.logout(function(self, status)
         *         if status then
         *             print("Failed to logout from Google Play Games.")
         *             return
         *         end
         *     end)
         *
         *     print("Good bye!")
         * end
         * </pre>
         */
        int Logout(lua_State* L);

        namespace Callback
        {

            bool Login(lua_State* L);

            bool Logout(lua_State* L);

        }

        namespace Impl
        {

            void Login(lua_State* L, int callback);

            void LoginCallback();

            void GetLoginStatus(lua_State* L);

            void Logout(lua_State* L, int callback);

            void LogoutCallback();

        };

    };

    namespace Achievement
    {

        /*# Unlock a standard achievement
         *
         * This function will unlock a standard achievement for the user, marking it as completed. If the achievement is hidden it will be revealed as well.
         *
         * <p>Please refer to the following link for guidelines and best practices when implementing Google Play Games achievements into your Defold game.</p>
         * <p><a href="https://developers.google.com/games/services/common/concepts/achievements">https://developers.google.com/games/services/common/concepts/achievements</a></p>
         *
         * @name unlock_achievement
         * @param achievement_id The identifier for the achievement that should be unlocked. This identifier can be obtained from the achievement view in the Google Play dashboard. (string)
         *
         * @examples
         * <pre>
         * if player_score >= 5000 then
         *     -- The player has earned the achievement Hillclimber!
         *     gpgs.unlock_achievement("CgkIp6bKucAfEAIQBQ")
         * end
         * </pre>
         */
        int Unlock(lua_State* L);

        /*# Reveal an achievement
         *
         * This function will reveal a previously hidden achievement for the user. If the achievement has already been revealed or unlocked, this will have no effect.
         *
         * <p>Please refer to the following link for guidelines and best practices when implementing Google Play Games achievements into your Defold game.</p>
         * <p><a href="https://developers.google.com/games/services/common/concepts/achievements">https://developers.google.com/games/services/common/concepts/achievements</a></p>
         *
         * @name reveal_achievement
         * @param achievement_id The identifier for the achievement that should be revealed. This identifier can be obtained from the achievement view in the Google Play dashboard. (string)
         *
         * @examples
         * <pre>
         * if session_duration() >= 30 then
         *     -- The player should see the secret power user achievement.
         *     gpgs.reveal_achievement("CgkIp6bKucAfEAIQBA")
         * end
         * </pre>
         */
        int Reveal(lua_State* L);

        /*# Increment an incremental achievement
         *
         * This function will increment an incremental achievement for the user. Once an achievement reaches it's maximum required value it will be unlocked automatically, marking it as completed. The achievement must be an incremental achievement and once it has been unlocked further increments will have no effect.
         *
         * <p>Please refer to the following link for guidelines and best practices when implementing Google Play Games achievements into your Defold game.</p>
         * <p><a href="https://developers.google.com/games/services/common/concepts/achievements">https://developers.google.com/games/services/common/concepts/achievements</a></p>
         *
         * @name increment_achievement
         * @param achievement_id The identifier for the achievement that should be incremented. This identifier can be obtained from the achievement view in the Google Play dashboard. (string)
         * @param value The amount that the achievement should be increased with. This must be a positive integer. If this parameter is omitted the achievement will be incremented by 1. (number)
         *
         * @examples
         * <pre>
         * if self.last_monster_killed == "tiger" then
         *     print("You killed another tiger!")
         *     gpgs.increment_achievement("CgkIp6bKucAfEAIQBA", 1)
         * end
         * </pre>
         */
        int Increment(lua_State* L);

        /*# Set an incremental achievement to a specific value
         *
         * This function will attempt to set an incremental achievement to a specific value for the user. If the achievement has already reached a higher value than the provided value this function will have no effect. Once an achievement reaches it's maximum required value it will be unlocked automatically, marking it as completed. The achievement must be an incremental achievement and once it has been unlocked further increments will have no effect.
         *
         * <p>Please refer to the following link for guidelines and best practices when implementing Google Play Games achievements into your Defold game.</p>
         * <p><a href="https://developers.google.com/games/services/common/concepts/achievements">https://developers.google.com/games/services/common/concepts/achievements</a></p>
         *
         * @name set_achievement
         * @param achievement_id The identifier for the achievement that should be set. This identifier can be obtained from the achievement view in the Google Play dashboard. (string)
         * @param value The amount that the achievement should be set to. This must be a positive integer. (number)
         *
         * @examples
         * <pre>
         * print("The rumour is that you have killed at least 500 tigers.")
         * gpgs.set_achievement("CgkIp6bKucAfEAIQBA", 500)
         * </pre>
         */
        int Set(lua_State* L);

        /*# Fetch an achievement
         *
         * This function will fetch information about an achievement for the user. A table with information about the achievement will be passed to the callback function.
         *
         * <p><strong>Achievement table</strong></p>
         * <ul>
         *   <li>id (string)</li>
         *   <li>name (string)</li>
         *   <li>state (string)</li>
         *   <li>type (string)</li>
         *   <li>valid (bool)</li>
         *   <li>description (string)</li>
         *   <li>current_steps (number)</li>
         *   <li>total_steps (number)</li>
         *   <li>xp (number)</li>
         *   <li>icon_url (string)</li>
         *   <li>last_modified_time (number)</li>
         * </ul>
         *
         * @name fetch_achievement
         * @param achievement_id The identifier for the achievement that should be fetched. This identifier can be obtained from the achievement view in the Google Play dashboard. (string)
         * @param callback Callback function with parameters (self, achievement). The callback is executed when the achievement has been fetched from Google Play Games. The parameter achievement will contain information about the achievement, this information should only be processed if the achievement data is valid. (function)
         *
         * @examples
         * <pre>
         * gpgs.fetch_achievement("CgkIp6bKucAfEAIQBA", function(self, achievement)
         *     if data["current_steps"] > 100 then
         *         print("You are well on your way warrior!")
         *         print(data["description"])
         *     end
         * end)
         * </pre>
         */
        int Fetch(lua_State* L);

        /*# Display an interface of all achievements obtainable and completed by the user.
         *
         * This function will open up the Google Play Game interface that displays the revealed achievements for the user.
         *
         * @name show_all_achievements
         * @param callback Callback function with parameters (self, status). The callback is executed when the achievement interface is closed by the user. The parameter status will indicate whether the achievement interface was closed by user interaction or not. (function)
         *
         * @examples
         * <pre>
         * -- Pause the game while the achievement interface is visible
         * self.game_running = 0
         * gpgs.show_all_achievements(function(self, status)
         *   self.game_running = 1
         * end)
         * </pre>
         */
        int ShowAll(lua_State* L);

        namespace Callback
        {

            bool Fetch(lua_State* L);

            bool ShowAll(lua_State* L);

        }

        namespace Impl
        {

            void Unlock(lua_State* L, const char* identifier);

            void Reveal(lua_State* L, const char* identifier);

            void Increment(
                lua_State* L, const char* identifier, uint32_t steps);

            void Set(lua_State* L, const char* identifier, uint32_t steps);

            void Fetch(lua_State* L, const char* identifier, int callback);

            void ShowAll(lua_State* L, int callback);

        }
    };

    namespace Event
    {

        /*# Increment an event
         *
         * This function will increment an event for the user.
         *
         * <p>Please refer to the following link for guidelines and best practices when implementing Google Play Games events into your Defold game.</p>
         * <p><a href="https://developers.google.com/games/services/common/concepts/quests">https://developers.google.com/games/services/common/concepts/quests</a></p>
         *
         * @name increment_event
         * @param event_id The identifier for the event that should be incremented. This identifier can be obtained from the event view in the Google Play dashboard. (string)
         * @param value The amount that the event should be increased with. This must be a positive integer. If this parameter is omitted the achievement will be incremented by 1.
         *
         * @examples
         * <pre>
         * if self.last_monster_killed == "tiger" then
         *   if self.health <= 0.05 then
         *     print("You're a real dare devil!")
         *     gpgs.increment_event("CgkIp6bKucAfEAIQBg", 1)
         *   end
         * end
         * </pre>
         */
        int Increment(lua_State* L);

        /*# Fetch an event
         *
         * This function will fetch information about an event for the user. A table with information about the event will be passed to the callback function.
         *
         * <p><strong>Event table</strong></p>
         * <ul>
         *   <li>id (string)</li>
         *   <li>name (string)</li>
         *   <li>valid (bool)</li>
         *   <li>description (string)</li>
         *   <li>count (number)</li>
         *   <li>image_url (string)</li>
         *   <li>visibility (string)</li>
         * </ul>
         *
         * @name fetch_event
         * @param event_id The identifier for the event that should be fetched. This identifier can be obtained from the event view in the Google Play dashboard. (string)
         * @param callback Callback function with parameters (self, event). The callback is executed when the event has been fetched from Google Play Games. The parameter event will contain information about the event, this information should only be processed if the event data is valid. (function)
         *
         * @examples
         * <pre>
         * gpgs.fetch_event("CgkIp6bKucAfEAIQBg", function(self, event)
         *   if event["valid"] then
         *     if event["count"] == 1 then
         *       print(event["description"])
         *     end
         *   end
         * end)
         * </pre>
         */
        int Fetch(lua_State* L);

        namespace Callback
        {

            bool Fetch(lua_State* L);

        };

        namespace Impl
        {

            void Increment(
                lua_State* L, const char* identifier, uint32_t steps);

            void Fetch(lua_State* L, const char* identifier, int callback);

        };

    };

    namespace Leaderboard
    {

        /*# Submit a score
         *
         * This function will submit a score to Google Play Games for the user. The score is sent to a specific leaderboard that is specified by an identifier. The score will be ignored if it is worse than a score previously submitted.
         *
         * <p>Please refer to the following link for guidelines and best practices when implementing Google Play Games achievements into your Defold game.</p>
         * <p><a href="https://developers.google.com/games/services/common/concepts/leaderboards">https://developers.google.com/games/services/common/concepts/leaderboards</a></p>
         *
         * @name submit_score
         * @param leaderboard_id The identifier for the leaderboard that the score should be submitted to. This identifier can be obtained from the leaderboard view in the Google Play dashboard. (string)
         * @param score The score that should be submitted. (number)
         * @param metadata A short description that should be associated with the score.
         *
         * @examples
         * <pre>
         * if self.remaining_moves == 0 then
         *     local message = "You ran out of moves!"
         *     gpgs.submit_score("CgkIp6bKucAfEAIQAQ", self.score, message)
         * end
         * </pre>
         */
        int SubmitScore(lua_State* L);

        /*# Display an interface of the specified leaderboard
         *
         * This function will open up the Google Play Game interface that displays a specific leaderboard for the user.
         *
         * @name show_leaderboard
         * @param leaderboard_id The identifier for the leaderboard that should be displayed. This identifier can be obtained from the leaderboard view in the Google Play dashboard. (string)
         * @param callback Callback function with parameters (self, status). The callback is executed when the leaderboard interface is closed by the user. The parameter status will indicate whether the leaderboard interface was closed by user interaction or not. (function)
         *
         * @examples
         * <pre>
         * -- Pause the game while the leaderboard interface is visible
         * self.game_running = 0
         * gpgs.show_leaderboard(function(self, status)
         *   self.game_running = 1
         * end)
         * </pre>
         */
        int Show(lua_State* L);

        /*# Display an interface of all available leaderboards
         *
         * This function will open up the Google Play Game interface that displays all available leaderboards for the user.
         *
         * @name show_all_leaderboards
         * @param callback Callback function with parameters (self, status). The callback is executed when the leaderboard interface is closed by the user. The parameter status will indicate whether the leaderboard interface was closed by user interaction or not. (function)
         *
         * @examples
         * <pre>
         * -- Pause the game while the leaderboard interface is visible
         * self.game_running = 0
         * gpgs.show_all_leaderboards(function(self, status)
         *   self.game_running = 1
         * end)
         * </pre>
         */
        int ShowAll(lua_State* L);

        namespace Callback
        {

            bool Show(lua_State* L);

            bool ShowAll(lua_State* L);

        };

        namespace Impl
        {

            void SubmitScore(lua_State* L, const char* identifier,
                double score, const char* description);

            void Show(lua_State* L, const char* identifier, int callback);

            void ShowAll(lua_State* L, int callback);

        };

    };

    namespace Player
    {

        /*# Fetch information about the user
         *
         * This function will fetch information about the user from Google Play Game.
         *
         * <p><strong>Player information table</strong></p>
         * <ul>
         *   <li>id (string)</li>
         *   <li>name (string)</li>
         *   <li>icon_url (string)</li>
         *   <li>hires_url (string)</li>
         *   <li>has_level (bool)</li>
         *   <li>current_level (number)</li>
         *   <li>current_level_xp (number)</li>
         *   <li>current_level_timestamp (number)</li>
         *   <li>current_level_min (number)</li>
         *   <li>current_level_max (number)</li>
         *   <li>next_level (number)</li>
         *   <li>next_level_min (number)</li>
         *   <li>next_level_max (number)</li>
         *   <li>title (string)</li>
         * </ul>
         *
         * @name fetch_player_info
         * @param callback Callback function with parameters (self, info). The callback is executed when the player information has been fetched from Google Play Games. The parameter info will contain information about the player. (function)
         *
         * @examples
         * <pre>
         * gpgs.fetch_player_info(function(self, info)
         *   print("Hello, ", info["name"])
         * end)
         * </pre>
         */
        int FetchInformation(lua_State* L);

        /*# Fetch statistics about the user
         *
         * This function will fetch statistics about the user from Google Play Game. The statistics should only be considered valid if it's corresponding has_ value is true.
         *
         * <p><strong>Player statistics table</strong></p>
         * <ul>
         *   <li>has_average_session_length (bool)</li>
         *   <li>average_session_length (number)</li>
         *   <li>has_churn_probability (bool)</li>
         *   <li>churn_probability (number)</li>
         *   <li>has_days_since_last_played (bool)</li>
         *   <li>days_since_last_played (number)</li>
         *   <li>has_number_of_purchases (bool)</li>
         *   <li>number_of_purchases (number)</li>
         *   <li>has_number_of_sessions (bool)</li>
         *   <li>number_of_sessions (number)</li>
         *   <li>has_session_percentile (bool)</li>
         *   <li>session_percentile (number)</li>
         *   <li>has_spend_percentile (bool)</li>
         *   <li>spend_percentile (number)</li>
         * </ul>
         *
         * @name fetch_player_stats
         * @param callback Callback function with parameters (self, stats). The callback is executed when the player statistics has been fetched from Google Play Games. The parameter stats will contain statistics for the player. (function)
         *
         * @examples
         * <pre>
         * gpgs.fetch_player_stats(function(self, stats)
         *   if stats["has_days_since_last_played"] then
         *     if stats["days_since_last_played"] >= 7 then
         *       self.booster = self.booster + 1
         *       print("Welcome back! We've missed you!")
         *     end
         *   end
         * end)
         * </pre>
         */
        int FetchStatistics(lua_State* L);

        namespace Callback
        {

            bool FetchInformation(lua_State* L);

            bool FetchStatistics(lua_State* L);

        };

        namespace Impl
        {

            void FetchInformation(lua_State* L, int callback);

            void FetchStatistics(lua_State* L, int callback);

        };

    };

    namespace Quest
    {

        /*# Accept a quest for the user
         *
         * This function will accept an open quest for the user. Incrementing the associated events will start tracking progress toward the milestone goal.
         *
         * <p><strong>Quest table</strong></p>
         * <ul>
         *   <li>id (string)</li>
         *   <li>name (string)</li>
         *   <li>state (string)</li>
         *   <li>valid (bool)</li>
         *   <li>description (string)</li>
         *   <li>accepted_time (number)</li>
         *   <li>start_time (number)</li>
         *   <li>expiration_time (number)</li>
         *   <li>banner_url (string)</li>
         *   <li>icon_url (string)</li>
         *   <li>current_milestone (string)</li>
         * </ul>
         *
         * <p><strong>Milestone table</strong></p>
         * <ul>
         *   <li>id (string)</li>
         *   <li>state (string)</li>
         *   <li>event (string)</li>
         *   <li>quest (string)</li>
         *   <li>valid (bool)</li>
         *   <li>current_count (number)</li>
         *   <li>target_count (number)</li>
         *   <li>reward_data (string)</li>
         * </ul>
         *
         * <p>Please refer to the following link for guidelines and best practices when implementing Google Play Games quests into your Defold game.</p>
         * <p><a href="https://developers.google.com/games/services/common/concepts/quests">https://developers.google.com/games/services/common/concepts/quests</a></p>
         *
         * @name accept_quest
         * @param quest_id The identifier for the quest that should be accepted. This identifier can be obtained from the quest view in the Google Play dashboard. (string)
         * @param callback Callback function with parameters (self, status, quest, milestone). The callback is executed when the quest has been accepted. The parameter status will indicate whether the request was successful. The parameters quest and milestone will contain information about the quest and current milestone respectively. Information about the quest and milestone should only be used the data is valid, which can be verified via quest["valid"] and milestone["valid"]. (function)
         *
         * @examples
         * <pre>
         * gpgs.accept_quest("CgkIp6bKucAfEAIQBg", function(self, status, quest, milestone)
         *   if quest["valid"] then
         *     if quest["state"] == "accepted" then
         *       print(quest["description"])
         *     end
         *   end
         * end)
         * </pre>
         */
        int Accept(lua_State* L);

        /*# Claim the current milestone from a quest for the player.
         *
         * This function will claim an available milestone for the user. Doing so calls the server, marking the milestone as completed. If the milestone information passed to the callback is valid the player should be rewarded. The reward for the milestone is present as a string in the milestone information passed to the callback.
         *
         * <p><strong>Quest table</strong></p>
         * <ul>
         *   <li>id (string)</li>
         *   <li>name (string)</li>
         *   <li>state (string)</li>
         *   <li>valid (bool)</li>
         *   <li>description (string)</li>
         *   <li>accepted_time (number)</li>
         *   <li>start_time (number)</li>
         *   <li>expiration_time (number)</li>
         *   <li>banner_url (string)</li>
         *   <li>icon_url (string)</li>
         *   <li>current_milestone (string)</li>
         * </ul>
         *
         * <p><strong>Milestone table</strong></p>
         * <ul>
         *   <li>id (string)</li>
         *   <li>state (string)</li>
         *   <li>event (string)</li>
         *   <li>quest (string)</li>
         *   <li>valid (bool)</li>
         *   <li>current_count (number)</li>
         *   <li>target_count (number)</li>
         *   <li>reward_data (string)</li>
         * </ul>
         *
         * <p>Please refer to the following link for guidelines and best practices when implementing Google Play Games quests into your Defold game.</p>
         * <p><a href="https://developers.google.com/games/services/common/concepts/quests">https://developers.google.com/games/services/common/concepts/quests</a></p>
         *
         * @name claim_quest_milestone
         * @param quest_id The identifier for the quest which milestone should be claimed. This identifier can be obtained from the quest view in the Google Play dashboard. (string)
         * @param callback Callback function with parameters (self, status, quest, milestone). The callback is executed when the quest milestone has been claimed. The parameter status will indicate whether the request was successful. The parameters quest and milestone will contain information about the quest and current milestone respectively. Information about the quest and milestone should only be used the data is valid, which can be verified via quest["valid"] and milestone["valid"]. (function)
         *
         * @examples
         * <pre>
         * gpgs.fetch_quest("CgkIp6bKucAfEAIQBg", function(self, s, q, m)
         *   if m["valid"] then
         *     if m["state"] == "not claimed" then
         *       gpgs.claim_quest_milestone(q["id"], function(self, status, quest, milestone)
         *         self.reward_player(milestone["reward_data"])
         *       end)
         *     end
         *   end
         * end)
         * </pre>
         */
        int ClaimMilestone(lua_State* L);

        /*# Fetch a quest and the current milestone for the user.
         *
         * This function will fetch information about a quest and it's current milestone for the user.
         *
         * <p><strong>Quest table</strong></p>
         * <ul>
         *   <li>id (string)</li>
         *   <li>name (string)</li>
         *   <li>state (string)</li>
         *   <li>valid (bool)</li>
         *   <li>description (string)</li>
         *   <li>accepted_time (number)</li>
         *   <li>start_time (number)</li>
         *   <li>expiration_time (number)</li>
         *   <li>banner_url (string)</li>
         *   <li>icon_url (string)</li>
         *   <li>current_milestone (string)</li>
         * </ul>
         *
         * <p><strong>Milestone table</strong></p>
         * <ul>
         *   <li>id (string)</li>
         *   <li>state (string)</li>
         *   <li>event (string)</li>
         *   <li>quest (string)</li>
         *   <li>valid (bool)</li>
         *   <li>current_count (number)</li>
         *   <li>target_count (number)</li>
         *   <li>reward_data (string)</li>
         * </ul>
         *
         * @name fetch_quest
         * @param quest_id The identifier for the quest that should be fetched. This identifier can be obtained from the quest view in the Google Play dashboard. (string)
         * @param callback Callback function with parameters (self, status, quest, milestone). The callback is executed when the quest has been fetched. The parameter status will indicate whether the request was successful. The parameters quest and milestone will contain information about the quest and current milestone respectively. Information about the quest and milestone should only be used the data is valid, which can be verified via quest["valid"] and milestone["valid"]. (function)
         *
         * @examples
         * <pre>
         * gpgs.fetch_quest("CgkIp6bKucAfEAIQBg", function(self, status, quest, milestone)
         *   if m["state"] == "open" then
         *     print("The quest is now available!", quest["name"])
         *   end
         * end)
         * </pre>
         */
        int Fetch(lua_State* L);

        /*# Display an interface of the specified quest for the player.
         *
         * This function will open up the Google Play Games interface that displays a specific quest for the player.
         *
         * <p><strong>Quest table</strong></p>
         * <ul>
         *   <li>id (string)</li>
         *   <li>name (string)</li>
         *   <li>state (string)</li>
         *   <li>valid (bool)</li>
         *   <li>description (string)</li>
         *   <li>accepted_time (number)</li>
         *   <li>start_time (number)</li>
         *   <li>expiration_time (number)</li>
         *   <li>banner_url (string)</li>
         *   <li>icon_url (string)</li>
         *   <li>current_milestone (string)</li>
         * </ul>
         *
         * <p><strong>Milestone table</strong></p>
         * <ul>
         *   <li>id (string)</li>
         *   <li>state (string)</li>
         *   <li>event (string)</li>
         *   <li>quest (string)</li>
         *   <li>valid (bool)</li>
         *   <li>current_count (number)</li>
         *   <li>target_count (number)</li>
         *   <li>reward_data (string)</li>
         * </ul>
         *
         * @name show_quest
         * @param quest_id The identifier for the quest that should be displayed. This identifier can be obtained from the quest view in the Google Play dashboard. (string)
         * @param callback Callback function with parameters (self, status, quest, milestone). The callback is executed when the quest interface is closed by the user, the quest and milestone passed to the callback will depend on which actions the user took inside the interface. The parameter status will indicate whether the request was successful or not. The parameters quest and milestone will contain information about the quest and current milestone respectively if a quest was accepted, a milestone claimed or a similar action was taken by the user. Information about the quest and milestone should only be used the data is valid, which can be verified via quest["valid"] and milestone["valid"]. (function)
         *
         * @examples
         * <pre>
         * -- Pause the game while the quest interface is visible
         * self.game_running = 0
         * gpgs.show_quest(function(self, status, quest, milestone)
         *   self.game_running = 1
         *   -- Process quest selection
         * end)
         * </pre>
         */
        int Show(lua_State* L);

        /*# Display an interface of all quests for the player.
         *
         * This function will open up the Google Play Games interface that displays all quests for the player.
         *
         * <p><strong>Quest table</strong></p>
         * <ul>
         *   <li>id (string)</li>
         *   <li>name (string)</li>
         *   <li>state (string)</li>
         *   <li>valid (bool)</li>
         *   <li>description (string)</li>
         *   <li>accepted_time (number)</li>
         *   <li>start_time (number)</li>
         *   <li>expiration_time (number)</li>
         *   <li>banner_url (string)</li>
         *   <li>icon_url (string)</li>
         *   <li>current_milestone (string)</li>
         * </ul>
         *
         * <p><strong>Milestone table</strong></p>
         * <ul>
         *   <li>id (string)</li>
         *   <li>state (string)</li>
         *   <li>event (string)</li>
         *   <li>quest (string)</li>
         *   <li>valid (bool)</li>
         *   <li>current_count (number)</li>
         *   <li>target_count (number)</li>
         *   <li>reward_data (string)</li>
         * </ul>
         *
         * @name show_all_quests
         * @param callback Callback function with parameters (self, status, quest, milestone). The callback is executed when the quest interface is closed by the user, the quest and milestone passed to the callback will depend on which actions the user took inside the interface. The parameter status will indicate whether the request was successful or not. The parameters quest and milestone will contain information about the quest and current milestone respectively if a quest was accepted, a milestone claimed or a similar action was taken by the user. Information about the quest and milestone should only be used the data is valid, which can be verified via quest["valid"] and milestone["valid"]. (function)
         *
         * @examples
         * <pre>
         * -- Pause the game while the quest interface is visible
         * self.game_running = 0
         * gpgs.show_all_quests(function(self, status, quest, milestone)
         *   self.game_running = 1
         *   -- Process quest selection
         * end)
         * </pre>
         */
        int ShowAll(lua_State* L);

        namespace Callback
        {

            bool Accept(lua_State* L);

            bool ClaimMilestone(lua_State* L);

            bool Fetch(lua_State* L);

            bool Show(lua_State* L);

            bool ShowAll(lua_State* L);

        };

        namespace Impl
        {

            void Accept(lua_State* L, const char* identifier, int callback);

            void ClaimMilestone(lua_State* L, const char* identifier, int callback);

            void Fetch(lua_State* L, const char* identifier, int callback);

            void Show(lua_State* L, const char* identifier, int callback);

            void ShowAll(lua_State* L, int callback);

        };

    };

    namespace Snapshot
    {

        /*# Save the game state to Google Play Games for the player.
         *
         * Save the game state to Google Play Games for the player. The policy supplied will determine how a conflict is resolved if multiple devices has been used for the same account or some other conditions exists which makes it impossible to save the game.
         *
         * <p><strong>Snapshot table</strong></p>
         * <ul>
         *   <li>filename (string)</li>
         *   <li>description (string)</li>
         *   <li>valid (bool)</li>
         *   <li>is_open (bool)</li>
         *   <li>played_time (number)</li>
         *   <li>last_modified_time (number)</li>
         *   <li>progress_value (number)</li>
         *   <li>cover_image_url (number)</li>
         * </ul>
         *
         * <p><strong>Policy options</strong></p>
         * <ul>
         *   <li><strong>POLICY_LONGEST_PLAYTIME</strong> In the event of a conflict, the snapshot with the largest playtime value will be used. This policy is a good choice if the length of play time is a reasonable proxy for the "best" save game.</li>
         *   <li><strong>POLICY_LAST_KNOWN_GOOD</strong> In the event of a conflict, the base snapshot will be used. This policy is a reasonable choice if your game requires stability from the snapshot data. This policy ensures that only writes which are not contested are seen by the player, which guarantees that all clients converge.</li>
         *   <li><strong>POLICY_MOST_RECENTLY_MODIFIED</strong> In the event of a conflict, the remote will be used. This policy is a reasonable choice if your game can tolerate players on multiple devices clobbering their own changes. Because this policy blindly chooses the most recent data, it is possible that a player's changes may get lost.</li>
         *   <li><strong>POLICY_HIGHEST_PROGRESS</strong> In the case of a conflict, the snapshot with the highest progress value will be used. In the case of a tie, the last known good snapshot will be chosen instead. This policy is a good choice if your game uses the progress value of the snapshot to determine the best saved game.</li>
         * </ul>
         *
         * @name save_snapshot
         * @param filename The filename for the saved game state. (string)
         * @param policy The policy to use to resolve conflicts. (policy)
         * @param description The description of the saved game state. (string)
         * @param seconds_played The number of seconds played. (number)
         * @param progress The progress the player has made. (number)
         * @param game_state A table with information required to restore the current game state when loading the game. (table)
         * @param callback Callback function with parameters (self, status, snapshot). The callback is executed when the game state has been saved. The parameter status will indicate whether the request was successful or not. The parameter snapshot will contain information about the saved game state. (function)
         *
         * @examples
         * <pre>
         * local filename = "Save001"
         * local desc = "Fighting a pack of tigers!"
         * gpgs.save_snapshot(filename, desc, gpgs.POLICY_LONGEST_PLAYTIME,
         *     self.seconds_played, self.progress, self.data, function(self, status, snapshot)
         *   if snapshot["valid"] then
         *     print("Game saved successfully!")
         *   end
         * end)
         * </pre>
         */
        int Commit(lua_State* L);

        /*# Delete a game state from Google Play Games for the player.
         *
         * Delete the game state from Google Play Games for the player.
         *
         * @name delete_snapshot
         * @param filename The filename for the saved game state. (string)
         * @param callback Callback function with parameters (self, status). The callback is executed when the game state has been deleted. The parameter status will indicate whether the request was successful or not. (function)
         *
         * @examples
         * <pre>
         * gpgs.delete_snapshot("Save001", function(self, status)
         *   if status then
         *     print("The game was successfully deleted.")
         *   end
         * end)
         * </pre>
         */
        int Delete(lua_State* L);

        /*# Load a game state from Google Play Games for the player.
         *
         * Load a game state from Google Play Games for the player. A table with the same data that was provided as game_state when the game was saved will be passed to the callback.
         *
         * <p><strong>Policy options</strong></p>
         * <ul>
         *   <li><strong>POLICY_LONGEST_PLAYTIME</strong> In the event of a conflict, the snapshot with the largest playtime value will be used. This policy is a good choice if the length of play time is a reasonable proxy for the "best" save game.</li>
         *   <li><strong>POLICY_LAST_KNOWN_GOOD</strong> In the event of a conflict, the base snapshot will be used. This policy is a reasonable choice if your game requires stability from the snapshot data. This policy ensures that only writes which are not contested are seen by the player, which guarantees that all clients converge.</li>
         *   <li><strong>POLICY_MOST_RECENTLY_MODIFIED</strong> In the event of a conflict, the remote will be used. This policy is a reasonable choice if your game can tolerate players on multiple devices clobbering their own changes. Because this policy blindly chooses the most recent data, it is possible that a player's changes may get lost.</li>
         *   <li><strong>POLICY_HIGHEST_PROGRESS</strong> In the case of a conflict, the snapshot with the highest progress value will be used. In the case of a tie, the last known good snapshot will be chosen instead. This policy is a good choice if your game uses the progress value of the snapshot to determine the best saved game.</li>
         * </ul>
         *
         * @name read_snapshot
         * @param filename The filename for the saved game state. (string)
         * @param policy The policy to use to resolve conflicts. (policy)
         * @param callback Callback function with parameters (self, status, game_state). The callback is executed when the game state has been read from Google Play Games. The parameter status will indicate whether the request was successful or not. The parameter game_state will contain the game state that was saved. (function)
         */
        int Read(lua_State* L);

        /*# Show an interface for snapshots on Google Play Games for the player.
         *
         * Show an interface for snapshots on Google Play Games for the player.
         *
         * <p><strong>Snapshot table</strong></p>
         * <ul>
         *   <li>filename (string)</li>
         *   <li>description (string)</li>
         *   <li>valid (bool)</li>
         *   <li>is_open (bool)</li>
         *   <li>played_time (number)</li>
         *   <li>last_modified_time (number)</li>
         *   <li>progress_value (number)</li>
         *   <li>cover_image_url (number)</li>
         * </ul>
         *
         * @name show_snapshot
         * @param title The title that should be displayed on the top of the screen. (string)
         * @param callback Callback function with parameters (self, status, snapshot). The callback is executed when the snapshot interface has been closed. The parameter status will indicate whether the interface was closed by user interaction or not. The parameter snapshot will contain information about the saved game state. (function)
         *
         * @examples
         * <pre>
         * -- Pause the game while the interface is visible
         * self.game_running = 0
         * gpgs.show_all_quests(function(self, status, snapshot)
         *   self.game_running = 1
         *   -- Process snapshot selection
         * end)
         * </pre>
         */
        int Show(lua_State* L);

        namespace Callback
        {

            bool Commit(lua_State* L);

            bool Delete(lua_State* L);

            bool Read(lua_State* L);

            bool Show(lua_State* L);

        };

        namespace Impl
        {

            void Commit(lua_State* L, const char* filename, uint32_t policy, const char* description, uint32_t time_played, int32_t progress, char* buffer, int buffer_len, int callback);

            void Delete(lua_State* L, const char* filename, int callback);

            void Read(lua_State* L, const char* filename, uint32_t policy, int callback);

            void Show(lua_State* L, const char* title, int callback);

        };

    };

};

#endif