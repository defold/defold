#ifndef H_GPGS_TYPES
#define H_GPGS_TYPES

#include <gpg/game_services.h>
#include <dlib/array.h>
#include <dlib/mutex.h>
extern "C"
{
    #include <lua/lua.h>
}

namespace dmGpgs
{

    enum class Callback
    {
        AUTHENTICATION_LOGIN            = 0,
        AUTHENTICATION_LOGOUT           = 1,

        ACHIEVEMENT_FETCH               = 2,
        ACHIEVEMENT_SHOWALL             = 3,

        EVENT_FETCH                     = 4,

        LEADERBOARD_SHOW                = 5,
        LEADERBOARD_SHOWALL             = 6,

        PLAYER_FETCH_INFORMATION        = 7,
        PLAYER_FETCH_STATISTICS         = 8,

        QUEST_ACCEPT                    = 9,
        QUEST_CLAIM_MILESTONE           = 10,
        QUEST_FETCH                     = 11,
        QUEST_SHOW                      = 12,
        QUEST_SHOWALL                   = 13,

        SNAPSHOT_READ                   = 14,
        SNAPSHOT_SHOW                   = 15,

        MAX_NUM_CALLBACKS               = 16
    };

    struct Command
    {
        Command()
        : m_Callback(LUA_NOREF), m_Self(LUA_NOREF)
        , m_Context(NULL), m_ParameterStack(NULL)
        {

        }

        int m_Callback;
        int m_Self;
        lua_State* m_Context;
        lua_State* m_ParameterStack;
    };

    struct GooglePlayGameServices
    {
        GooglePlayGameServices()
        : m_Callbacks(), m_Commands(), m_Mutex(dmMutex::New())
        {
            m_Callbacks.SetCapacity(
                static_cast<int>(dmGpgs::Callback::MAX_NUM_CALLBACKS));
            int length = static_cast<int>(dmGpgs::Callback::MAX_NUM_CALLBACKS);
            for (int i = 0; i < length; ++i)
            {
                m_Callbacks.Push(dmGpgs::Command());
            }

            m_Commands.SetCapacity(8);
        }

        std::unique_ptr<gpg::GameServices> m_GameServices;
        dmArray<dmGpgs::Command> m_Callbacks;
        dmArray<dmGpgs::Command> m_Commands;

        dmMutex::Mutex m_Mutex;
    };

};

#endif