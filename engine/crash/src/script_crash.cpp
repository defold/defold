// Copyright 2020-2026 The Defold Foundation
// Copyright 2014-2020 King
// Copyright 2009-2014 Ragnar Svensson, Christian Murray
// Licensed under the Defold License version 1.0 (the "License"); you may not use
// this file except in compliance with the License.
//
// You may obtain a copy of the License, together with FAQs at
// https://www.defold.com/license
//
// Unless required by applicable law or agreed to in writing, software distributed
// under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
// CONDITIONS OF ANY KIND, either express or implied. See the License for the
// specific language governing permissions and limitations under the License.

#include <assert.h>
#include <extension/extension.hpp>
#include <dlib/dstrings.h>
#include <dlib/log.h>
#include <script/script.h>
#include <stdlib.h>

#include "crash.h"
#include "crash_private.h"

#include <dmsdk/dlua/dlua.h>

#define LIB_NAME "crash"

namespace dmCrash
{
    /*# Crash API documentation
     *
     * Native crash logging functions and constants.
     *
     * @document
     * @name Crash
     * @namespace crash
     * @language Lua
     */

    static HDump CheckHandle(dlua_State* L, int index)
    {
        HDump h = (HDump) dluaL_checkint(L, index);
        if (!IsValidHandle(h))
        {
            dluaL_error(L, "Provided handle is invalid");
        }
        return h;
    }

    /*# writes crash dump
     * Performs the same steps as if a crash had just occured but
     * allows the program to continue.
     * The generated dump can be read by crash.load_previous
     *
     * @name crash.write_dump
     */
    static int Crash_WriteDump(dlua_State* L)
    {
        WriteDump();
        return 0;
    }

    /*# sets the file location for crash dumps
     *
     * Crashes occuring before the path is set will be stored to a default engine location.
     *
     * @name crash.set_file_path
     * @param path [type:string] file path to use
     */
    static int Crash_SetFilePath(dlua_State* L)
    {
        const char* path = dluaL_checkstring(L, 1);
        SetFilePath(path);
        return 0;
    }

    /*# loads a previously written crash dump
     *
     * The crash dump will be removed from disk upon a successful
     * load, so loading is one-shot.
     *
     * @name crash.load_previous
     * @return handle [type:number|nil] handle to the loaded dump, or `nil` if no dump was found
     */
    static int Crash_LoadPrevious(dlua_State* L)
    {
        HDump dump = LoadPrevious();
        if (dump != 0)
        {
            dlua_pushnumber(L, dump);
            Purge();
        }
        else
        {
            dlua_pushnil(L);
        }
        return 1;
    }

    /*# releases a previously loaded crash dump
     *
     * @name crash.release
     * @param handle [type:number] handle to loaded crash dump
     */
    static int Crash_ReleasePrevious(dlua_State* L)
    {
        Release(CheckHandle(L, 1));
        return 0;
    }

    /*# stores user-defined string value
     *
     * Store a user value that will get written to a crash dump when
     * a crash occurs. This can be user id:s, breadcrumb data etc.
     * There are 32 slots indexed from 0. Each slot stores at most 255 characters.
     *
     * @name crash.set_user_field
     * @param index [type:number] slot index. 0-indexed
     * @param value [type:string] string value to store
     */
    static int Crash_SetUserField(dlua_State* L)
    {
        int index = dluaL_checkint(L, 1);
        const char* value = dluaL_checkstring(L, 2);

        if (index < 0 || index >= (int)AppState::USERDATA_SLOTS)
        {
            return dluaL_error(L, "User data slot index out of range. Max elements is %d", AppState::USERDATA_SLOTS);
        }

        if (strlen(value) > (int)(AppState::USERDATA_SIZE-1))
        {
            dmLogWarning("Userdata value will be truncated to max length %d", AppState::USERDATA_SIZE-1);
        }

        SetUserField(index, value);
        return 0;
    }

    /*# get all loaded modules from when the crash occured
     *
     * The function returns a table containing entries with sub-tables that
     * have fields 'name' and 'address' set for all loaded modules.
     *
     * @name crash.get_modules
     * @param handle [type:number] crash dump handle
     * @return modules [type:table] module table
     */
    static int Crash_GetModules(dlua_State* L)
    {
        int top = dlua_gettop(L);

        HDump h = CheckHandle(L, 1);

        dlua_newtable(L);
        for (uint32_t i=0;true;i++)
        {
            void* addr = GetModuleAddr(h, i);
            const char* name = GetModuleName(h, i);
            assert((!addr && !name) || (addr && name));
            if (!addr)
            {
                break;
            }

            dlua_pushnumber(L, i+1);

            dlua_newtable(L);
            dlua_pushliteral(L, "name");
            dlua_pushstring(L, name);
            dlua_settable(L, -3);

            char str[64];
            dmSnPrintf(str, sizeof(str), "%p", addr);
            dlua_pushliteral(L, "address");
            dlua_pushstring(L, str);
            dlua_settable(L, -3);

            dlua_settable(L, -3);
        }

        assert(dlua_gettop(L) == (top+1));
        return 1;
    }

    /*# reads user field from a loaded crash dump
     *
     * @name crash.get_user_field
     * @param handle [type:number] crash dump handle
     * @param index [type:number] user data slot index
     * @return value [type:string] user data value recorded in the crash dump
     */
    static int Crash_GetUserField(dlua_State* L)
    {
        HDump h = CheckHandle(L, 1);
        int index = dluaL_checkint(L, 2);
        if (index < 0 || index >= (int)AppState::USERDATA_SLOTS)
        {
            return dluaL_error(L, "User data slot index out of range. Max elements is %d", AppState::USERDATA_SLOTS);
        }

        const char *value = GetUserField(h, index);
        if (!value)
        {
            dlua_pushnil(L);
        }
        else
        {
            dlua_pushstring(L, value);
        }

        return 1;
    }

    /*# reads a system field from a loaded crash dump
     *
     * @name crash.get_sys_field
     * @param handle [type:number] crash dump handle
     * @param index [type:number] system field enum. Must be less than [ref:crash.SYSFIELD_MAX]
     * @return value [type:string|nil] value recorded in the crash dump, or `nil` if it didn't exist
     */
    static int Crash_GetSysField(dlua_State* L)
    {
        HDump h = CheckHandle(L, 1);
        int field = dluaL_checkint(L, 2);
        if (field < 0 || field >= SYSFIELD_MAX)
        {
            return dluaL_error(L, "Unknown system field provided");
        }

        const char *value = GetSysField(h, (SysField)field);
        if (!value)
        {
            dlua_pushnil(L);
        }
        else
        {
            dlua_pushstring(L, value);
        }

        return 1;
    }

    /*# read signal number from a crash report
     *
     * @name crash.get_signum
     * @param handle [type:number] crash dump handle
     * @return signal [type:number] signal number
     */
    static int Crash_GetSignum(dlua_State* L)
    {
        HDump h = CheckHandle(L, 1);
        dlua_pushnumber(L, GetSignum(h));
        return 1;
    }

    /*# read backtrace recorded in a loaded crash dump
     *
     * A table is returned containing the addresses of the call stack.
     *
     * @name crash.get_backtrace
     * @param handle [type:number] crash dump handle
     * @return backtrace [type:table] table containing the backtrace
     */
    static int Crash_GetBacktrace(dlua_State* L)
    {
        int top = dlua_gettop(L);
        HDump h = CheckHandle(L, 1);
        uint32_t count = GetBacktraceAddrCount(h);
        dlua_newtable(L);
      for (uint32_t i=0;i!=count;i++)
        {
            char str[64];
            dmSnPrintf(str, sizeof(str), "%p", GetBacktraceAddr(h, i));
            dlua_pushnumber(L, i+1);
            dlua_pushstring(L, str);
            dlua_settable(L, -3);
        }

        assert(dlua_gettop(L) == (top+1));
        return 1;
    }

    /*# read text blob recorded in a crash dump
     *
     * The format of read text blob is platform specific
     * and not guaranteed
     * but can be useful for manual inspection.
     *
     * @name crash.get_extra_data
     * @param handle [type:number] crash dump handle
     * @return blob [type:string] string with the platform specific data
     */
    static int Crash_GetExtraData(dlua_State* L)
    {
        HDump h = CheckHandle(L, 1);
        dlua_pushstring(L, GetExtraData(h));
        return 1;
    }

    static const dluaL_reg Crash_methods[] =
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

    static dmExtension::Result InitializeCrash(dmExtension::Params* params)
    {
        assert(dmCrash::IsInitialized());

        dlua_State* L = params->m_L;
        int top = dlua_gettop(L);
        dluaL_register(L, LIB_NAME, Crash_methods);

        #define SETCONSTANT(name) \
            dlua_pushnumber(L, (dlua_Number) name); \
            dlua_setfield(L, -2, #name);\

        /*# engine version as release number
         *
         * @name crash.SYSFIELD_ENGINE_VERSION
         * @constant
         */
        SETCONSTANT(SYSFIELD_ENGINE_VERSION);

        /*# engine version as hash
         *
         * @name crash.SYSFIELD_ENGINE_HASH
         * @constant
         */
        SETCONSTANT(SYSFIELD_ENGINE_HASH);

        /*# device model as reported by sys.get_sys_info
         *
         * @name crash.SYSFIELD_DEVICE_MODEL
         * @constant
         */
        SETCONSTANT(SYSFIELD_DEVICE_MODEL);

        /*# device manufacturer as reported by sys.get_sys_info
         *
         * @name crash.SYSFIELD_MANUFACTURER
         * @constant
         */
        SETCONSTANT(SYSFIELD_MANUFACTURER);

        /*# system name as reported by sys.get_sys_info
         *
         * @name crash.SYSFIELD_SYSTEM_NAME
         * @constant
         */
        SETCONSTANT(SYSFIELD_SYSTEM_NAME);

        /*# system version as reported by sys.get_sys_info
         *
         * @name crash.SYSFIELD_SYSTEM_VERSION
         * @constant
         */
        SETCONSTANT(SYSFIELD_SYSTEM_VERSION);

        /*# system language as reported by sys.get_sys_info
         *
         * @name crash.SYSFIELD_LANGUAGE
         * @constant
         */
        SETCONSTANT(SYSFIELD_LANGUAGE);

        /*# system device language as reported by sys.get_sys_info
         *
         * @name crash.SYSFIELD_DEVICE_LANGUAGE
         * @constant
         */
        SETCONSTANT(SYSFIELD_DEVICE_LANGUAGE);

        /*# system territory as reported by sys.get_sys_info
         *
         * @name crash.SYSFIELD_TERRITORY
         * @constant
         */
        SETCONSTANT(SYSFIELD_TERRITORY);

        /*# android build fingerprint
         *
         * @name crash.SYSFIELD_ANDROID_BUILD_FINGERPRINT
         * @constant
         */
        SETCONSTANT(SYSFIELD_ANDROID_BUILD_FINGERPRINT);

        /*# The max number of sysfields.
         *
         * @name crash.SYSFIELD_MAX
         * @constant
         */
        SETCONSTANT(SYSFIELD_MAX);


        #undef SETCONSTANT

        #define SETCUSTOMCONSTANT(name, value) \
            dlua_pushnumber(L, (dlua_Number) (value)); \
            dlua_setfield(L, -2, #name);\

        /*# The max number of user fields.
         *
         * @name crash.USERFIELD_MAX
         * @constant
         */
        SETCUSTOMCONSTANT(USERFIELD_MAX, AppState::USERDATA_SLOTS);

        /*# The max size of a single user field.
         *
         * @name crash.USERFIELD_SIZE
         * @constant
         */
        SETCUSTOMCONSTANT(USERFIELD_SIZE, AppState::USERDATA_SIZE-1);

        #undef SETCUSTOMCONSTANT

        dlua_pop(L, 1);
        assert(top == dlua_gettop(L));
        return dmExtension::RESULT_OK;
    }

    static dmExtension::Result FinalizeCrash(dmExtension::Params* params)
    {
        return dmExtension::RESULT_OK;
    }

    DM_DECLARE_EXTENSION(CrashExt, "Crash", 0, 0, InitializeCrash, 0, 0, FinalizeCrash)
}
