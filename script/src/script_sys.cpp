#include <stdlib.h>
#include <stdio.h>
#include <sys/stat.h>

#if defined(__linux__) || defined(__MACH__)
#  include <sys/errno.h>
#elif defined(_WIN32)
#  include <errno.h>
#endif

#ifdef _WIN32
#include <direct.h>
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

    const uint32_t MAX_BUFFER_SIZE =  64 * 1024;

    int Sys_Save(lua_State* L)
    {
        char buffer[MAX_BUFFER_SIZE];
        const char* filename = luaL_checkstring(L, 1);
        luaL_checktype(L, 2, LUA_TTABLE);
        uint32_t n_used = CheckTable(L, buffer, sizeof(buffer), 2);
        FILE* file = fopen(filename, "wb");
        if (file != 0x0)
        {
            bool result = fwrite(buffer, 1, n_used, file) == n_used;
            fclose(file);
            if (result)
            {
                lua_pushboolean(L, result);
                return 1;
            }
        }
        return luaL_error(L, "Could not write to the file %s.", filename);
    }

    int Sys_Load(lua_State* L)
    {
        char buffer[MAX_BUFFER_SIZE];
        const char* filename = luaL_checkstring(L, 1);
        FILE* file = fopen(filename, "rb");
        if (file == 0x0)
        {
            lua_newtable(L);
            return 1;
        }
        fread(buffer, 1, sizeof(buffer), file);
        bool result = ferror(file) == 0 && feof(file) != 0;
        fclose(file);
        if (result)
        {
            PushTable(L, buffer);
            return 1;
        }
        else
        {
            return luaL_error(L, "Could not read from the file %s.", filename);
        }
    }

    int Sys_GetSaveFile(lua_State* L)
    {
        const char* application_id = luaL_checkstring(L, 1);
        const char* filename = luaL_checkstring(L, 2);
        char* home = 0;
        char* dm_home = getenv("DM_SAVE_HOME");
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
        dmStrlCat(buf, application_id, sizeof(buf));

        int ret;
#ifdef _WIN32
        ret = mkdir(buf);
#else
        ret = mkdir(buf, 0700);
#endif
        if (ret)
        {
            if (errno != EEXIST)
                luaL_error(L, "Unable to create save folder %s (%d)", buf, errno);
        }

        dmStrlCat(buf, "/", sizeof(buf));
        dmStrlCat(buf, filename, sizeof(buf));

        lua_pushstring(L, buf);

        return 1;
    }

    static const luaL_reg ScriptSys_methods[] =
    {
        {"save", Sys_Save},
        {"load", Sys_Load},
        {"get_save_file", Sys_GetSaveFile},
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
