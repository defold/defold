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

        SNAPSHOT_COMMIT                 = 14,
        SNAPSHOT_READ                   = 15,
        SNAPSHOT_SHOW                   = 16,
        SNAPSHOT_DELETE                 = 17,

        MAX_NUM_CALLBACKS               = 18
    };

    struct CommitInfo
    {
        CommitInfo() : description(NULL), time_played_ms(0), progress(0), buffer(0), buffer_len(0)
        {

        }

        const char* description;
        uint32_t time_played_ms;
        int32_t progress;
        char* buffer;
        int32_t buffer_len;
    };

    struct Command
    {
        Command()
        : m_Callback(LUA_NOREF), m_Self(LUA_NOREF)
        , m_Context(NULL), m_ParameterStack(NULL), m_info(NULL)
        {

        }

        int m_Callback;
        int m_Self;
        lua_State* m_Context;
        lua_State* m_ParameterStack;
        void* m_info;
    };

    struct GooglePlayGameServices
    {
        GooglePlayGameServices()
        : m_Callbacks(), m_Commands(), m_Mutex(dmMutex::New())
        {
            int length = static_cast<int>(dmGpgs::Callback::MAX_NUM_CALLBACKS);
            m_Callbacks.SetCapacity(length);
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