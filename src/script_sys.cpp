#include <stdlib.h>
#include <sys/stat.h>

#if defined(__linux__) || defined(__MACH__)
#  include <sys/errno.h>
#elif defined(_WIN32)
#  include <errno.h>
#endif

#include <dlib/dstrings.h>
#include "script.h"

#include <string.h>
extern "C"
{
#include <lua/lua.h>
#include <lua/lauxlib.h>
}

namespace dmScript
{
#define LIB_NAME "sys"

    int Sys_GetSavegameFolder(lua_State* L)
    {
        const char* game_id = luaL_checkstring(L, 1);
        char* home = 0;
        char* dm_home = getenv("DM_SAVEGAME_HOME");
#if defined(__linux__) || defined(__MACH__)
        home = getenv("HOME");
#elif defined(_WIN32)
        home = getenv("USERPROFILE");
#else
#error "Unsupported platform"
#endif

        // Higher priority
        if (dm_home)
            home = dm_home;

        if (home == 0)
        {
            luaL_error(L, "Unable to locate home folder (HOME or USERPROFILE)");
        }

        char buf[1024];
        dmStrlCpy(buf, home, sizeof(buf));
        dmStrlCat(buf, "/", sizeof(buf));
        dmStrlCat(buf, game_id, sizeof(buf));

        int ret = mkdir(buf, 0700);
        if (ret)
        {
            if (errno != EEXIST)
                luaL_error(L, "Unable to create save-game folder %s (%d)", buf, errno);
        }

        lua_pushstring(L, buf);

        return 1;
    }

    static const luaL_reg ScriptSys_methods[] =
    {
        {"get_savegame_folder", Sys_GetSavegameFolder},
        {0, 0}
    };

    void InitializeSys(lua_State* L)
    {
        int top = lua_gettop(L);

        lua_pushvalue(L, LUA_GLOBALSINDEX);
        luaL_register(L, LIB_NAME, ScriptSys_methods);
        lua_pop(L, 2);

        assert(top == lua_gettop(L));
    }
}
