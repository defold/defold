#include "gpgs.h"

#include <cassert>
#include <cstdlib>

#include <dlib/log.h>
#include <script/script.h>

#define safe_return(L, argc, args) {                                                                    \
            if ((argc + args) != lua_gettop(L))                                                         \
            {                                                                                           \
                dmLogError("%s:%d (%s) - Unbalanced Stack, expected %d + %d arguments but found %d.",   \
                    __FILE__, __LINE__, __func__, argc, args, lua_gettop(L));                           \
                assert(false && "Stack is unbalanced");                                                 \
            }                                                                                           \
            dmLogInfo("%s:%d (%s) - Balanced Stack, containing %d argument(s)",                         \
                __FILE__, __LINE__, __func__, argc + args);                                             \
            return args;                                                                                \
        }

namespace
{

    const uint32_t MAX_BUFFER_SIZE =  128 * 1024;

    bool VerifyStack(lua_State* L, int argc, ...)
    {
        va_list params;
        va_start(params, argc);
        for (int i = 1; i <= argc; ++i)
        {
            int expected = va_arg(params, int);
            int actual = lua_type(L, i);
            if (expected != actual)
            {
                return false;
            }
        }

        return true;
    }

    void VerifyParameterList(lua_State* L, int argc_min, int argc_max, ...)
    {
        int argc = lua_gettop(L);
        if (argc < argc_min) {
            luaL_error(L, "Too few arguments, got %d, expected %d", argc, argc_min);
        }
        else if (argc > argc_max)
        {
            luaL_error(L, "Too many arguments, got %d, expected %d", argc, argc_min);
        }
        else
        {
            va_list params;
            va_start(params, argc);
            for (int i = 1; i <= argc; ++i)
            {
                bool required = (i <= argc_min);
                int expected = va_arg(params, int);
                int actual = lua_type(L, i);
                if (expected != actual)
                {
                    if (required)
                    {
                        luaL_error(L, "Required argument %d has incorrect datatype, got %s, expected %s", i, luaL_typename(L, lua_type(L, i)));
                    }
                    else
                    {
                        luaL_error(L, "Optional argument %d has incorrect datatype, got %s, expected %s", i, luaL_typename(L, lua_type(L, i)));
                    }
                }
            }
        }
    }

    void VerifyGoogleAuthentication(lua_State* L)
    {
        if (!dmGpgs::Authenticated())
        {
            luaL_error(L, "Must be authenticated to use Google Play Game Services");
        }
    }

};



// ----------------------------------------------------------------------------
// Authentication::Login
// ----------------------------------------------------------------------------
int dmGpgs::Authentication::Login(lua_State* L)
{
    int argc = lua_gettop(L);
    ::VerifyParameterList(L, 1, 1, LUA_TFUNCTION);

    dmGpgs::Authentication::Impl::Login(L, 1);

    safe_return(L, argc, 0);
}

bool dmGpgs::Authentication::Callback::Login(lua_State* L)
{
    return ::VerifyStack(L, 1, LUA_TBOOLEAN);
}



// ----------------------------------------------------------------------------
// Authentication::GetLoginStatus
// ----------------------------------------------------------------------------
int dmGpgs::Authentication::GetLoginStatus(lua_State* L)
{
    int argc = lua_gettop(L);
    ::VerifyParameterList(L, 0, 0);

    dmGpgs::Authentication::Impl::GetLoginStatus(L);

    safe_return(L, argc, 1);
}



// ----------------------------------------------------------------------------
// Authentication::Logout
// ----------------------------------------------------------------------------
int dmGpgs::Authentication::Logout(lua_State* L)
{
    int argc = lua_gettop(L);
    ::VerifyParameterList(L, 1, 1, LUA_TFUNCTION);
    ::VerifyGoogleAuthentication(L);

    dmGpgs::Authentication::Impl::Logout(L, 1);

    safe_return(L, argc, 0);
}

bool dmGpgs::Authentication::Callback::Logout(lua_State* L)
{
    return ::VerifyStack(L, 1, LUA_TBOOLEAN);
}



// ----------------------------------------------------------------------------
// Achievement::Unlock
// ----------------------------------------------------------------------------
int dmGpgs::Achievement::Unlock(lua_State* L)
{
    int argc = lua_gettop(L);
    ::VerifyParameterList(L, 1, 1, LUA_TSTRING);
    ::VerifyGoogleAuthentication(L);

    const char* identifier = luaL_checkstring(L, 1);

    dmGpgs::Achievement::Impl::Unlock(L, identifier);

    safe_return(L, argc, 0);
}



// ----------------------------------------------------------------------------
// Achievement::Reveal
// ----------------------------------------------------------------------------
int dmGpgs::Achievement::Reveal(lua_State* L)
{
    int argc = lua_gettop(L);
    ::VerifyParameterList(L, 1, 1, LUA_TSTRING);
    ::VerifyGoogleAuthentication(L);

    const char* identifier = luaL_checkstring(L, 1);

    dmGpgs::Achievement::Impl::Reveal(L, identifier);

    safe_return(L, argc, 0);
}



// ----------------------------------------------------------------------------
// Achievement::Increment
// ----------------------------------------------------------------------------
int dmGpgs::Achievement::Increment(lua_State* L)
{
    int argc = lua_gettop(L);
    ::VerifyParameterList(L, 1, 2, LUA_TSTRING, LUA_TNUMBER);
    ::VerifyGoogleAuthentication(L);

    const char* identifier = luaL_checkstring(L, 1);
    double steps = lua_isnumber(L, 2) ? luaL_checknumber(L, 2) : 1.0;

    dmGpgs::Achievement::Impl::Increment(L, identifier, (uint32_t) steps);

    safe_return(L, argc, 0);
}



// ----------------------------------------------------------------------------
// Achievement::Set
// ----------------------------------------------------------------------------
int dmGpgs::Achievement::Set(lua_State* L)
{
    int argc = lua_gettop(L);
    ::VerifyParameterList(L, 1, 2, LUA_TSTRING, LUA_TNUMBER);
    ::VerifyGoogleAuthentication(L);

    const char* identifier = luaL_checkstring(L, 1);
    double steps = lua_isnumber(L, 2) ? luaL_checknumber(L, 2) : 1.0;

    dmGpgs::Achievement::Impl::Set(L, identifier, (uint32_t) steps);

    safe_return(L, argc, 0);
}



// ----------------------------------------------------------------------------
// Achievement::Fetch
// ----------------------------------------------------------------------------
int dmGpgs::Achievement::Fetch(lua_State* L)
{
    int argc = lua_gettop(L);
    ::VerifyParameterList(L, 2, 2, LUA_TSTRING, LUA_TFUNCTION);
    ::VerifyGoogleAuthentication(L);

    const char* identifier = luaL_checkstring(L, 1);

    dmGpgs::Achievement::Impl::Fetch(L, identifier, 2);

    safe_return(L, argc, 0);
}

bool dmGpgs::Achievement::Callback::Fetch(lua_State* L)
{
    return ::VerifyStack(L, 2, LUA_TBOOLEAN, LUA_TTABLE);
}



// ----------------------------------------------------------------------------
// Achievement::ShowAll
// ----------------------------------------------------------------------------
int dmGpgs::Achievement::ShowAll(lua_State* L)
{
    int argc = lua_gettop(L);
    ::VerifyParameterList(L, 1, 1, LUA_TFUNCTION);
    ::VerifyGoogleAuthentication(L);

    dmGpgs::Achievement::Impl::ShowAll(L, 1);

    safe_return(L, argc, 0);
}

bool dmGpgs::Achievement::Callback::ShowAll(lua_State* L)
{
    return ::VerifyStack(L, 1, LUA_TBOOLEAN);
}



// ----------------------------------------------------------------------------
// Event::Increment
// ----------------------------------------------------------------------------
int dmGpgs::Event::Increment(lua_State* L)
{
    int argc = lua_gettop(L);
    ::VerifyParameterList(L, 1, 2, LUA_TSTRING, LUA_TNUMBER);
    ::VerifyGoogleAuthentication(L);

    const char* identifier = luaL_checkstring(L, 1);
    double steps = lua_isnumber(L, 2) ? luaL_checknumber(L, 2) : 1.0;

    dmGpgs::Event::Impl::Increment(L, identifier, (uint32_t) steps);

    safe_return(L, argc, 0);
}



// ----------------------------------------------------------------------------
// Event::Fetch
// ----------------------------------------------------------------------------
int dmGpgs::Event::Fetch(lua_State* L)
{
    int argc = lua_gettop(L);
    ::VerifyParameterList(L, 2, 2, LUA_TSTRING, LUA_TFUNCTION);
    ::VerifyGoogleAuthentication(L);

    const char* identifier = luaL_checkstring(L, 1);

    dmGpgs::Event::Impl::Fetch(L, identifier, 2);

    safe_return(L, argc, 0);
}

bool dmGpgs::Event::Callback::Fetch(lua_State* L)
{
    return ::VerifyStack(L, 2, LUA_TBOOLEAN, LUA_TTABLE);
}



// ----------------------------------------------------------------------------
// Leaderboard::SubmitScore
// ----------------------------------------------------------------------------
int dmGpgs::Leaderboard::SubmitScore(lua_State* L)
{
    int argc = lua_gettop(L);
    ::VerifyParameterList(L, 2, 3, LUA_TSTRING, LUA_TNUMBER, LUA_TSTRING);
    ::VerifyGoogleAuthentication(L);

    const char* identifier = luaL_checkstring(L, 1);
    double score = luaL_checknumber(L, 2);
    const char* description =
        lua_isstring(L, 3) ? luaL_checkstring(L, 3) : NULL;
    if (description == NULL || strlen(description) <= 64)
    {
        dmGpgs::Leaderboard::Impl::SubmitScore(
            L, identifier, score, description);
    }
    else
    {
        luaL_argerror(L, 3,
            "Description can be at most 64 URI-safe characters");
    }

    safe_return(L, argc, 0);
}



// ----------------------------------------------------------------------------
// Leaderboard::Show
// ----------------------------------------------------------------------------
int dmGpgs::Leaderboard::Show(lua_State* L)
{
    int argc = lua_gettop(L);
    ::VerifyParameterList(L, 2, 2, LUA_TSTRING, LUA_TFUNCTION);
    ::VerifyGoogleAuthentication(L);

    const char* identifier = luaL_checkstring(L, 1);
    dmGpgs::Leaderboard::Impl::Show(L, identifier, 2);

    safe_return(L, argc, 0);
}

bool dmGpgs::Leaderboard::Callback::Show(lua_State* L)
{
    return ::VerifyStack(L, 1, LUA_TBOOLEAN);
}



// ----------------------------------------------------------------------------
// Leaderboard::ShowAll
// ----------------------------------------------------------------------------
int dmGpgs::Leaderboard::ShowAll(lua_State* L)
{
    int argc = lua_gettop(L);
    ::VerifyParameterList(L, 1, 1, LUA_TFUNCTION);
    ::VerifyGoogleAuthentication(L);

    dmGpgs::Leaderboard::Impl::ShowAll(L, 1);

    safe_return(L, argc, 0);
}

bool dmGpgs::Leaderboard::Callback::ShowAll(lua_State* L)
{
    return ::VerifyStack(L, 1, LUA_TBOOLEAN);
}



// ----------------------------------------------------------------------------
// Player::FetchInformation
// ----------------------------------------------------------------------------
int dmGpgs::Player::FetchInformation(lua_State* L)
{
    int argc = lua_gettop(L);
    ::VerifyParameterList(L, 1, 1, LUA_TFUNCTION);
    ::VerifyGoogleAuthentication(L);

    dmGpgs::Player::Impl::FetchInformation(L, 1);

    safe_return(L, argc, 0);
}

bool dmGpgs::Player::Callback::FetchInformation(lua_State* L)
{
    return ::VerifyStack(L, 2, LUA_TBOOLEAN, LUA_TTABLE);
}



// ----------------------------------------------------------------------------
// Player::FetchStatistics
// ----------------------------------------------------------------------------
int dmGpgs::Player::FetchStatistics(lua_State* L)
{
    int argc = lua_gettop(L);
    ::VerifyParameterList(L, 1, 1, LUA_TFUNCTION);
    ::VerifyGoogleAuthentication(L);

    dmGpgs::Player::Impl::FetchStatistics(L, 1);

    safe_return(L, argc, 0);
}

bool dmGpgs::Player::Callback::FetchStatistics(lua_State* L)
{
    return ::VerifyStack(L, 2, LUA_TBOOLEAN, LUA_TTABLE);
}



// ----------------------------------------------------------------------------
// Quest::Accept
// ----------------------------------------------------------------------------
int dmGpgs::Quest::Accept(lua_State* L)
{
    int argc = lua_gettop(L);
    ::VerifyParameterList(L, 2, 2, LUA_TSTRING, LUA_TFUNCTION);
    ::VerifyGoogleAuthentication(L);

    const char* identifier = luaL_checkstring(L, 1);

    dmGpgs::Quest::Impl::Accept(L, identifier, 2);

    safe_return(L, argc, 0);
}

bool dmGpgs::Quest::Callback::Accept(lua_State* L)
{
    return ::VerifyStack(L, 3, LUA_TBOOLEAN, LUA_TTABLE, LUA_TTABLE);
}



// ----------------------------------------------------------------------------
// Quest::ClaimMilestone
// ----------------------------------------------------------------------------
int dmGpgs::Quest::ClaimMilestone(lua_State* L)
{
    int argc = lua_gettop(L);
    ::VerifyParameterList(L, 2, 2, LUA_TSTRING, LUA_TFUNCTION);
    ::VerifyGoogleAuthentication(L);

    const char* identifier = luaL_checkstring(L, 1);

    dmGpgs::Quest::Impl::ClaimMilestone(L, identifier, 2);

    safe_return(L, argc, 0);
}

bool dmGpgs::Quest::Callback::ClaimMilestone(lua_State* L)
{
    return ::VerifyStack(L, 3, LUA_TBOOLEAN, LUA_TTABLE, LUA_TTABLE);
}



// ----------------------------------------------------------------------------
// Quest::Fetch
// ----------------------------------------------------------------------------
int dmGpgs::Quest::Fetch(lua_State* L)
{
    int argc = lua_gettop(L);
    ::VerifyParameterList(L, 2, 2, LUA_TSTRING, LUA_TFUNCTION);
    ::VerifyGoogleAuthentication(L);

    const char* identifier = luaL_checkstring(L, 1);

    dmGpgs::Quest::Impl::Fetch(L, identifier, 2);

    safe_return(L, argc, 0);
}

bool dmGpgs::Quest::Callback::Fetch(lua_State* L)
{
    return ::VerifyStack(L, 3, LUA_TBOOLEAN, LUA_TTABLE, LUA_TTABLE);
}



// ----------------------------------------------------------------------------
// Quest::Show
// ----------------------------------------------------------------------------
int dmGpgs::Quest::Show(lua_State* L)
{
    int argc = lua_gettop(L);
    ::VerifyParameterList(L, 2, 2, LUA_TSTRING, LUA_TFUNCTION);
    ::VerifyGoogleAuthentication(L);

    const char* identifier = luaL_checkstring(L, 1);

    dmGpgs::Quest::Impl::Show(L, identifier, 2);

    safe_return(L, argc, 0);
}

bool dmGpgs::Quest::Callback::Show(lua_State* L)
{
    return ::VerifyStack(L, 3, LUA_TBOOLEAN, LUA_TTABLE, LUA_TTABLE);
}



// ----------------------------------------------------------------------------
// Quest::ShowAll
// ----------------------------------------------------------------------------
int dmGpgs::Quest::ShowAll(lua_State* L)
{
    int argc = lua_gettop(L);
    ::VerifyParameterList(L, 1, 1, LUA_TFUNCTION);
    ::VerifyGoogleAuthentication(L);

    dmGpgs::Quest::Impl::ShowAll(L, 1);

    safe_return(L, argc, 0);
}

bool dmGpgs::Quest::Callback::ShowAll(lua_State* L)
{
    return ::VerifyStack(L, 3, LUA_TBOOLEAN, LUA_TTABLE, LUA_TTABLE);
}



// ----------------------------------------------------------------------------
// Snapshot::Commit
// ----------------------------------------------------------------------------
int dmGpgs::Snapshot::Commit(lua_State* L)
{
    int argc = lua_gettop(L);
    ::VerifyParameterList(L, 7, 7, LUA_TSTRING, LUA_TNUMBER, LUA_TSTRING,
        LUA_TNUMBER, LUA_TNUMBER, LUA_TTABLE, LUA_TFUNCTION);
    ::VerifyGoogleAuthentication(L);

    const char* filename = luaL_checkstring(L, 1);
    uint32_t policy = (uint32_t) luaL_checknumber(L, 2);
    const char* description = luaL_checkstring(L, 3);
    uint32_t seconds_played = (uint32_t) luaL_checknumber(L, 4);
    int32_t progress = (int32_t) luaL_checknumber(L, 5);

    char* buffer = (char*) malloc(sizeof(char) * ::MAX_BUFFER_SIZE);
    uint32_t n_used = dmScript::CheckTable(L, buffer, ::MAX_BUFFER_SIZE, 6);

    dmGpgs::Snapshot::Impl::Commit(L, filename, policy, description, seconds_played, progress, buffer, n_used, 7);

    safe_return(L, argc, 0);
}

bool dmGpgs::Snapshot::Callback::Commit(lua_State* L)
{
    return ::VerifyStack(L, 2, LUA_TBOOLEAN, LUA_TTABLE);
}



// ----------------------------------------------------------------------------
// Snapshot::Delete
// ----------------------------------------------------------------------------
int dmGpgs::Snapshot::Delete(lua_State* L)
{
    int argc = lua_gettop(L);
    ::VerifyParameterList(L, 2, 2, LUA_TSTRING, LUA_TFUNCTION);
    ::VerifyGoogleAuthentication(L);

    const char* filename = luaL_checkstring(L, 1);

    dmGpgs::Snapshot::Impl::Delete(L, filename, 2);

    safe_return(L, argc, 0);
}

bool dmGpgs::Snapshot::Callback::Delete(lua_State* L)
{
    return ::VerifyStack(L, 1, LUA_TBOOLEAN);
}



// ----------------------------------------------------------------------------
// Snapshot::Read
// ----------------------------------------------------------------------------
int dmGpgs::Snapshot::Read(lua_State* L)
{
    int argc = lua_gettop(L);
    ::VerifyParameterList(L, 3, 3, LUA_TSTRING, LUA_TNUMBER, LUA_TFUNCTION);
    ::VerifyGoogleAuthentication(L);

    const char* filename = luaL_checkstring(L, 1);
    uint32_t policy = (uint32_t) luaL_checknumber(L, 2);

    dmGpgs::Snapshot::Impl::Read(L, filename, policy, 3);

    safe_return(L, argc, 0);
}

bool dmGpgs::Snapshot::Callback::Read(lua_State* L)
{
    return ::VerifyStack(L, 2, LUA_TBOOLEAN, LUA_TTABLE);
}



// ----------------------------------------------------------------------------
// Snapshot::Show
// ----------------------------------------------------------------------------
int dmGpgs::Snapshot::Show(lua_State* L)
{
    int argc = lua_gettop(L);
    ::VerifyParameterList(L, 2, 2, LUA_TSTRING, LUA_TFUNCTION);
    ::VerifyGoogleAuthentication(L);

    const char* title = luaL_checkstring(L, 1);

    dmGpgs::Snapshot::Impl::Show(L, title, 2);

    safe_return(L, argc, 0);
}

bool dmGpgs::Snapshot::Callback::Show(lua_State* L)
{
    return ::VerifyStack(L, 2, LUA_TBOOLEAN, LUA_TTABLE);
}
