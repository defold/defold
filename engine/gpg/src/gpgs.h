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

    namespace Authentication
    {

        int Login(lua_State* L);

        int GetLoginStatus(lua_State* L);

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

        int Unlock(lua_State* L);

        int Reveal(lua_State* L);

        int Increment(lua_State* L);

        int Set(lua_State* L);

        int Fetch(lua_State* L);

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

        int Increment(lua_State* L);

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

        int SubmitScore(lua_State* L);

        int Show(lua_State* L);

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

        int FetchInformation(lua_State* L);

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

        int Accept(lua_State* L);

        int ClaimMilestone(lua_State* L);

        int Fetch(lua_State* L);

        int Show(lua_State* L);

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

            void ClaimMilestone(
                lua_State* L, const char* identifier, int callback);

            void Fetch(lua_State* L, const char* identifier, int callback);

            void Show(lua_State* L, const char* identifier, int callback);

            void ShowAll(lua_State* L, int callback);

        };

    };

    namespace Snapshot
    {

        int Commit(lua_State* L);

        int Delete(lua_State* L);

        int Read(lua_State* L);

        int Show(lua_State* L);

        namespace Callback
        {

            bool Read(lua_State* L);

            bool Show(lua_State* L);

        };

        namespace Impl
        {

            void Commit(lua_State* L, const char* filename, uint32_t policy,
                const char* description, uint32_t time_played,
                int32_t progress, const std::vector<uint8_t>& buffer);

            void Delete(lua_State* L, const char* filename);

            void Read(lua_State* L, const char* filename,
                uint32_t policy, int callback);

            void Show(lua_State* L, const char* title, int callback);

        };

    };

};

#endif