#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
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
#include <dlib/sys.h>
#include <dlib/array.h>
#include <dlib/log.h>
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

    const uint32_t MAX_TIMERS = 256;
    const uint32_t MAX_BUFFER_SIZE =  64 * 1024;

    struct Timer
    {
        float    m_Delay;
        float    m_Elapsed;
        float    m_Duration;

        int      m_UserValueRef;
        int      m_CallbackRef;
        uint16_t m_FirstUpdate : 1;
        uint16_t m_CompleteCalled : 1;
    };

    struct Sys
    {
        dmArray<Timer> m_Timers;
    };

    /*#
     * Save lua table to disk
     * @name sys.save
     * @param filename destination filename
     * @param table table to write to disk
     */
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

    /*#
     * Load lua table to disk
     * @name sys.load
     * @param filename source filename
     * @return loaded table
     */
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

    /*#
     * Get save-file path to operating system specific save-file location.
     * @name sys.get_save_file
     * @param application_id application-id of the application to get save-file for
     * @param file_name file-name to get path for
     * @return path to save-file
     */
    int Sys_GetSaveFile(lua_State* L)
    {
        const char* application_id = luaL_checkstring(L, 1);

        char app_support_path[1024];
        dmSys::Result r = dmSys::GetApplicationSupportPath(application_id, app_support_path, sizeof(app_support_path));
        if (r != dmSys::RESULT_OK)
        {
            luaL_error(L, "Unable to locate application support path (%d)", r);
        }

        const char* filename = luaL_checkstring(L, 2);
        char* dm_home = getenv("DM_SAVE_HOME");

        // Higher priority
        if (dm_home)
        {
            dmStrlCpy(app_support_path, dm_home, sizeof(app_support_path));
        }

        dmStrlCat(app_support_path, "/", sizeof(app_support_path));
        dmStrlCat(app_support_path, filename, sizeof(app_support_path));
        lua_pushstring(L, app_support_path);

        return 1;
    }

    /*#
     * Create timer with callback
     * @name sys.timer
     * @param user_data user-data object, eg self
     * @param callback callback function
     * @param duration timer duration
     * @return true on success
     */

    /*#
     * Create timer with callback
     * @name sys.timer
     * @param user_data user-data object, eg self
     * @param callback callback function
     * @param duration timer duration
     * @param delay timer delay
     * @return true on success
     */
    int Sys_Timer(lua_State* L)
    {
        int top = lua_gettop(L);
        (void) top;

        int user_value_ref;
        int callback_ref;
        float duration = 0;
        float delay = 0;

        uint32_t timer_index;
        Timer* timer;

        lua_getglobal(L, "__sys__");
        Sys* sys = (Sys*) lua_touserdata(L, -1);
        lua_pop(L, 1);
        if (sys->m_Timers.Full())
        {
            dmLogWarning("Out of timers (sys.timer) (max active %d)", MAX_TIMERS);
            assert(top== lua_gettop(L));
            lua_pushboolean(L, 0);
            return 1;
        }

        timer_index = sys->m_Timers.Size();
        sys->m_Timers.SetSize(timer_index+1);

        lua_pushvalue(L, 1);
        user_value_ref = lua_ref(L, LUA_REGISTRYINDEX);
        if (!lua_isfunction(L, 2))
        {
            luaL_typerror(L, 2, "function");
        }

        lua_pushvalue(L, 2);
        callback_ref = lua_ref(L, LUA_REGISTRYINDEX);

        duration = luaL_checknumber(L, 3);
        delay = 0;
        if (lua_isnumber(L, 4))
        {
            delay = lua_tonumber(L, 4);
        }

        timer = &sys->m_Timers[timer_index];

        timer->m_Delay = delay;
        timer->m_Elapsed = 0;
        timer->m_Duration = duration;
        timer->m_UserValueRef = user_value_ref;
        timer->m_CallbackRef = callback_ref;
        timer->m_FirstUpdate = 0;
        timer->m_CompleteCalled = 0;

        assert(top== lua_gettop(L));
        lua_pushboolean(L, 1);
        return 1;
    }

    static const luaL_reg ScriptSys_methods[] =
    {
        {"save", Sys_Save},
        {"load", Sys_Load},
        {"get_save_file", Sys_GetSaveFile},
        {"timer", Sys_Timer},
        {0, 0}
    };

    void InitializeSys(lua_State* L)
    {
        int top = lua_gettop(L);
        (void) top;

        lua_pushvalue(L, LUA_GLOBALSINDEX);
        luaL_register(L, LIB_NAME, ScriptSys_methods);
        lua_pop(L, 2);

        Sys* sys = new Sys();
        sys->m_Timers.SetCapacity(MAX_TIMERS);

        lua_pushlightuserdata(L, (void*) sys);
        lua_setglobal(L, "__sys__");

        assert(top == lua_gettop(L));
    }

    void FinalizeSys(lua_State* L)
    {
        int top = lua_gettop(L);

        lua_getglobal(L, "__sys__");
        Sys* sys = (Sys*) lua_touserdata(L, -1);
        lua_pop(L, 1);

        delete sys;

        assert(top == lua_gettop(L));
    }

    void Update(lua_State* L, float dt)
    {
        int top = lua_gettop(L);
        (void) top;

        lua_getglobal(L, "__sys__");
        Sys* sys = (Sys*) lua_touserdata(L, -1);
        lua_pop(L, 1);


        dmArray<Timer>* timers = &sys->m_Timers;
        uint32_t n = timers->Size();

        for (uint32_t i = 0; i < n; ++i)
        {
            Timer* timer = &(*timers)[i];

            if (timer->m_Elapsed >= timer->m_Duration)
            {
                continue;
            }

            if (timer->m_Delay > 0)
            {
                timer->m_Delay -= dt;
            }

            if (timer->m_Delay <= 0)
            {
                if (timer->m_FirstUpdate)
                {
                    timer->m_FirstUpdate = 0;
                    // Compensate Elapsed with Delay underflow
                    timer->m_Elapsed = -timer->m_Delay;
                }

                // NOTE: We add dt to elapsed before we calculate t.
                // Example: 60 updates with dt=1/60.0 should result in a complete animation
                timer->m_Elapsed += dt;
                float t = timer->m_Elapsed / timer->m_Duration;

                if (t > 1)
                    t = 1;

                if (timer->m_Elapsed + dt >= timer->m_Duration)
                {
                    if (!timer->m_CompleteCalled)
                    {
                        // NOTE: Very important to set m_CompleteCalled to 1
                        // before invoking the call-back. The call-back could potentially
                        // start a new timer that could reuse the same timer slot.
                        timer->m_CompleteCalled = 1;

                        lua_rawgeti(L, LUA_REGISTRYINDEX, timer->m_CallbackRef);
                        lua_rawgeti(L, LUA_REGISTRYINDEX, timer->m_UserValueRef);

                        int ret = lua_pcall(L, 1, 0, 0);
                        if (ret != 0)
                        {
                            dmLogError("Error running timer callback: %s", lua_tostring(L,-1));
                            lua_pop(L, 1);
                        }

                        lua_unref(L, timer->m_CallbackRef);
                        lua_unref(L, timer->m_UserValueRef);
                    }
                }
            }
        }

        n = timers->Size();
        for (uint32_t i = 0; i < n; ++i)
        {
            Timer* timer = &(*timers)[i];

            if (timer->m_Elapsed >= timer->m_Duration)
            {
                timers->EraseSwap(i);
                i--;
                n--;
                continue;
            }
        }

        assert(top == lua_gettop(L));
    }

}
