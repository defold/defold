#include <assert.h>
#include <extension/extension.h>
#include <dlib/dstrings.h>
#include <dlib/log.h>
#include <dlib/json.h>
#include <script/script.h>
#include <stdlib.h>

#include "crash.h"
#include "crash_private.h"

extern "C"
{
#include <lua/lua.h>
#include <lua/lauxlib.h>
}

#define LIB_NAME "crash"

namespace dmCrash
{
    static HDump CheckHandle(lua_State* L, int index)
    {
        HDump h = (HDump) luaL_checkint(L, index);
        if (!IsValidHandle(h))
        {
            luaL_error(L, "Provided handle is invalid");
        }
        return h;
    }

    /*# Perform the same steps as if a crash had just occured, but allow the program to continue.
     * The generated dump can be read by crash.load_previous
     *
     * @name crash.write_dump
     */
    int Crash_WriteDump(lua_State* L)
    {
        WriteDump();
        return 0;
    }

    /*# Set the location for where crash dumps will be written. Crashes occuring before the path
     * is set will be stored to a default engine location.
     *
     * @name crash.set_file_path
     * @param path file path to use
     */
    int Crash_SetFilePath(lua_State* L)
    {
        const char* path = luaL_checkstring(L, 1);
        SetFilePath(path);
        return 0;
    }

    /*# Load a previously written crash dump. The crash dump will be removed from disk upon a successful
     * load, so loading is one-shot.
     *
     * @name crash.load_previous
     * @return handle to the loaded dump, or nil if no dump was found
     */
    int Crash_LoadPrevious(lua_State* L)
    {
        HDump dump = LoadPrevious();
        if (dump != 0)
        {
            lua_pushnumber(L, dump);
            Purge();
        }
        else
        {
            lua_pushnil(L);
        }
        return 1;
    }

    /*# Release a previously loaded crash dump.
     *
     * @name crash.release
     * @param handle to loaded crash dump
     */
    int Crash_ReleasePrevious(lua_State* L)
    {
        Release(CheckHandle(L, 1));
        return 0;
    }

    /*# Store user-defined string value that will get written to a crash dump when
     * a crash occurs. This can be user id:s, breadcrumb data etc. There are 32 slots
     * indexed from 0. Each slot stores at most 255 characters.
     *
     * @name crash.set_user_field
     * @param index Slot index (0-indexed)
     * @param value Value to store
     */
    int Crash_SetUserField(lua_State* L)
    {
        int index = luaL_checkint(L, 1);
        const char* value = luaL_checkstring(L, 2);

        if (index < 0 || index >= (int)AppState::USERDATA_SLOTS)
        {
            return luaL_error(L, "User data slot index out of range. Max elements is %d", AppState::USERDATA_SLOTS);
        }

        if (strlen(value) > (int)(AppState::USERDATA_SIZE-1))
        {
            dmLogWarning("Userdata value will be truncated to max length %d", AppState::USERDATA_SIZE-1);
        }

        SetUserField(index, value);
        return 0;
    }

    /*# Get a table of all loaded modules from when the crash occured. The function returns a table
     * containing entries with sub-tables that have 'name' and 'address' set.
     *
     * @name crash.get_modules
     * @param handle Crash dump handle
     * @return module table
     */
    int Crash_GetModules(lua_State* L)
    {
        int top = lua_gettop(L);

        HDump h = CheckHandle(L, 1);

        lua_newtable(L);
        for (uint32_t i=0;true;i++)
        {
            void* addr = GetModuleAddr(h, i);
            const char* name = GetModuleName(h, i);
            assert((!addr && !name) || (addr && name));
            if (!addr)
            {
                break;
            }

            lua_pushnumber(L, i+1);

            lua_newtable(L);
            lua_pushstring(L, "name");
            lua_pushstring(L, name);
            lua_settable(L, -3);

            char str[64];
            sprintf(str, "%p", addr);
            lua_pushstring(L, "address");
            lua_pushstring(L, str);
            lua_settable(L, -3);

            lua_settable(L, -3);
        }

        assert(lua_gettop(L) == (top+1));
        return 1;
    }

    /*# Read user field from a loaded crash dump
     *
     * @name crash.get_user_field
     * @param handle Crash dump handle
     * @param index User data slot index
     * @return User data value recorded in the crash dump
     */
    int Crash_GetUserField(lua_State* L)
    {
        HDump h = CheckHandle(L, 1);
        int index = luaL_checkint(L, 2);
        if (index < 0 || index >= (int)AppState::USERDATA_SLOTS)
        {
            return luaL_error(L, "User data slot index out of range. Max elements is %d", AppState::USERDATA_SLOTS);
        }

        const char *value = GetUserField(h, index);
        if (!value)
        {
            lua_pushnil(L);
        }
        else
        {
            lua_pushstring(L, value);
        }

        return 1;
    }

    /*# Read a system field from a loaded crash dump
     *
     * @name crash.get_sys_field
     * @param handle Crash dump handle
     * @param index System field enum
     * @return Value recorded in the crash dump
     */
    int Crash_GetSysField(lua_State* L)
    {
        HDump h = CheckHandle(L, 1);
        int field = luaL_checkint(L, 2);
        if (field < 0 || field >= SYSFIELD_MAX)
        {
            return luaL_error(L, "Unknown system field provided");
        }

        const char *value = GetSysField(h, (SysField)field);
        if (!value)
        {
            lua_pushnil(L);
        }
        else
        {
            lua_pushstring(L, value);
        }

        return 1;
    }

    /*# Read signal number from a crash report
     *
     * @name crash.get_signum
     * @param handle Crash dump handle
     * @return signal number
     */
    int Crash_GetSignum(lua_State* L)
    {
        HDump h = CheckHandle(L, 1);
        lua_pushnumber(L, GetSignum(h));
        return 1;
    }

    /*# Read back trace recorded in a loaded crash dump. A table is returned containing
     * the addresses of the call stack.
     *
     * @name crash.get_backtrace
     * @param handle Crash dump handle
     * @return Table containing the backtrace
     */
    int Crash_GetBacktrace(lua_State* L)
    {
        int top = lua_gettop(L);
        HDump h = CheckHandle(L, 1);
        uint32_t count = GetBacktraceAddrCount(h);
        lua_newtable(L);
      for (uint32_t i=0;i!=count;i++)
        {
            char str[64];
            sprintf(str, "%p", GetBacktraceAddr(h, i));
            lua_pushnumber(L, i+1);
            lua_pushstring(L, str);
            lua_settable(L, -3);
        }

        assert(lua_gettop(L) == (top+1));
        return 1;
    }

    /*# Read platform-specific text blob recorded in a crash dump. Format is not guaranteed
     * but can be useful for manual inspection.
     *
     * @name crash.get_extra_data
     * @param handle Crash dump handle
     * @return String with the platform specific data
     */
    int Crash_GetExtraData(lua_State* L)
    {
        HDump h = CheckHandle(L, 1);
        lua_pushstring(L, GetExtraData(h));
        return 1;
    }

    static const luaL_reg Crash_methods[] =
    {
        {"set_file_path", Crash_SetFilePath},
        {"load_previous", Crash_LoadPrevious},
        {"get_user_field", Crash_GetUserField},
        {"get_sys_field", Crash_GetSysField},
        {"get_backtrace", Crash_GetBacktrace},
        {"get_modules", Crash_GetModules},
        {"get_extra_data", Crash_GetExtraData},
        {"get_signum", Crash_GetSignum},
        {"release", Crash_ReleasePrevious},
        {"set_user_field", Crash_SetUserField},
        {"write_dump", Crash_WriteDump},
        {0, 0}
    };

    dmExtension::Result InitializeCrash(dmExtension::Params* params)
    {
        assert(dmCrash::IsInitialized());

        lua_State* L = params->m_L;
        int top = lua_gettop(L);
        luaL_register(L, LIB_NAME, Crash_methods);

        #define SETCONSTANT(name) \
            lua_pushnumber(L, (lua_Number) name); \
            lua_setfield(L, -2, #name);\

        /*# engine version as release number
         *
         * @name crash.SYSFIELD_ENGINE_VERSION
         * @variable
         */
        SETCONSTANT(SYSFIELD_ENGINE_VERSION);

        /*# engine version as hash
         *
         * @name crash.SYSFIELD_ENGINE_HASH
         * @variable
         */
        SETCONSTANT(SYSFIELD_ENGINE_HASH);

        /*# device model as reported by sys.get_sys_info
         *
         * @name crash.SYSFIELD_DEVICE_MODEL
         * @variable
         */
        SETCONSTANT(SYSFIELD_DEVICE_MODEL);

        /*# device manufacturer as reported by sys.get_sys_info
         *
         * @name crash.SYSFIELD_DEVICE_MANUFACTURER
         * @variable
         */
        SETCONSTANT(SYSFIELD_MANUFACTURER);

        /*# system name as reported by sys.get_sys_info
         *
         * @name crash.SYSFIELD_SYSTEM_NAME
         * @variable
         */
        SETCONSTANT(SYSFIELD_SYSTEM_NAME);

        /*# system version as reported by sys.get_sys_info
         *
         * @name crash.SYSFIELD_SYSTEM_VERSION
         * @variable
         */
        SETCONSTANT(SYSFIELD_SYSTEM_VERSION);

        /*# system language as reported by sys.get_sys_info
         *
         * @name crash.SYSFIELD_LANGUAGE
         * @variable
         */
        SETCONSTANT(SYSFIELD_LANGUAGE);

        /*# system device language as reported by sys.get_sys_info
         *
         * @name crash.SYSFIELD_DEVICE_LANGUAGE
         * @variable
         */
        SETCONSTANT(SYSFIELD_DEVICE_LANGUAGE);

        /*# system territory as reported by sys.get_sys_info
         *
         * @name crash.SYSFIELD_TERRITORY
         * @variable
         */
        SETCONSTANT(SYSFIELD_TERRITORY);

        /*# android build fingerprint
         *
         * @name crash.SYSFIELD_ANDROID_BUILD_FINGERPRINT
         * @variable
         */
        SETCONSTANT(SYSFIELD_ANDROID_BUILD_FINGERPRINT);

        #undef SETCONSTANT

        lua_pop(L, 1);
        assert(top == lua_gettop(L));
        return dmExtension::RESULT_OK;
    }

    dmExtension::Result FinalizeCrash(dmExtension::Params* params)
    {
        return dmExtension::RESULT_OK;
    }

    DM_DECLARE_EXTENSION(CrashExt, "Crash", 0, 0, InitializeCrash, 0, FinalizeCrash)
}

