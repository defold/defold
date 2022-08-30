// Copyright 2020-2022 The Defold Foundation
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

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <sys/stat.h>

#if defined(__linux__) || defined(__MACH__)
#  include <sys/errno.h>
#elif defined(_WIN32)
#  include <errno.h>
#elif defined(__EMSCRIPTEN__)
#  include <unistd.h>
#endif

#ifdef _WIN32
#include <direct.h>
#include <malloc.h>
#endif

#include <dlib/dstrings.h>
#include <dlib/sys.h>
#include <dlib/log.h>
#include <dlib/socket.h>
#include <dlib/path.h>
#include <resource/resource.h>
#include "script.h"
#include "script/sys_ddf.h"

#include <string.h>
extern "C"
{
#include <lua/lua.h>
#include <lua/lauxlib.h>
}

#include "script_private.h"

namespace dmScript
{

const uint32_t MAX_BUFFER_SIZE = 512 * 1024;

union SaveLoadBuffer
{
    uint32_t m_alignment; // This alignment is required for js-web
    char m_buffer[MAX_BUFFER_SIZE]; // Resides in .bss
} g_saveload;

#define LIB_NAME "sys"

    /*# System API documentation
     *
     * Functions and messages for using system resources, controlling the engine,
     * error handling and debugging.
     *
     * @document
     * @name System
     * @namespace sys
     */

    char* Sys_SetupTableSerializationBuffer(int required_size)
    {
        if (required_size > MAX_BUFFER_SIZE)
        {
            return (char*)malloc(required_size);
        }
        else
        {
            return g_saveload.m_buffer;
        }
    }

    void Sys_FreeTableSerializationBuffer(char* buffer)
    {
        if (buffer != g_saveload.m_buffer)
        {
            free(buffer);
        }
    }

    /*# saves a lua table to a file stored on disk
     * The table can later be loaded by <code>sys.load</code>.
     * Use <code>sys.get_save_file</code> to obtain a valid location for the file.
     * Internally, this function uses a workspace buffer sized output file sized 512kb.
     * This size reflects the output file size which must not exceed this limit.
     * Additionally, the total number of rows that any one table may contain is limited to 65536
     * (i.e. a 16 bit range). When tables are used to represent arrays, the values of
     * keys are permitted to fall within a 32 bit range, supporting sparse arrays, however
     * the limit on the total number of rows remains in effect.
     *
     * @name sys.save
     * @param filename [type:string] file to write to
     * @param table [type:table] lua table to save
     * @return success [type:boolean] a boolean indicating if the table could be saved or not
     * @examples
     *
     * Save data:
     *
     * ```lua
     * local my_table = {}
     * table.insert(my_table, "my_value")
     * local my_file_path = sys.get_save_file("my_game", "my_file")
     * if not sys.save(my_file_path, my_table) then
     *   -- Alert user that the data could not be saved
     * end
     * ```
     */

    int Sys_Save(lua_State* L)
    {
        const char* filename = luaL_checkstring(L, 1);

        luaL_checktype(L, 2, LUA_TTABLE);

        uint32_t table_size = CheckTableSize(L, 2);

        char* buffer = Sys_SetupTableSerializationBuffer(table_size);
        if (!buffer)
        {
            return luaL_error(L, "Could not allocate %d bytes for table serialization.", table_size);
        }
        uint32_t n_used = CheckTable(L, buffer, table_size, 2);

#if !defined(__EMSCRIPTEN__)

        char tmp_filename[DMPATH_MAX_PATH];
        // The counter and hash are there to make the files unique enough to avoid that the user
        // accidentally writes to it.
        static int save_counter = 0;
        uint32_t hash = dmHashString32(filename);
        int res = dmSnPrintf(tmp_filename, sizeof(tmp_filename), "%s.defoldtmp_%x_%d", filename, hash, save_counter++);
        if (res == -1)
        {
            Sys_FreeTableSerializationBuffer(buffer);
            return luaL_error(L, "Could not write to the file %s. Path too long.", filename);
        }

        FILE* file = fopen(tmp_filename, "wb");
        if (!file)
        {
            Sys_FreeTableSerializationBuffer(buffer);
            char errmsg[128] = {};
            dmStrError(errmsg, sizeof(errmsg), errno);
            return luaL_error(L, "Could not open the file %s, reason: %s.", tmp_filename, errmsg);
        }

        bool result = fwrite(buffer, 1, n_used, file) == n_used;
        result = (fclose(file) == 0) && result;
        Sys_FreeTableSerializationBuffer(buffer);

        if (!result)
        {
            dmSys::Unlink(tmp_filename);
            return luaL_error(L, "Could not write to the file %s.", filename);
        }

        if (dmSys::RenameFile(filename, tmp_filename) != dmSys::RESULT_OK)
        {
            return luaL_error(L, "Could not rename %s to the file %s.", tmp_filename, filename);
        }

        lua_pushboolean(L, result);
        return 1;

#else // __EMSCRIPTEN__

        FILE* file = fopen(filename, "wb");
        if (!file)
        {
            Sys_FreeTableSerializationBuffer(buffer);
            return luaL_error(L, "Could not write to the file %s.", filename);
        }

        bool result = fwrite(buffer, 1, n_used, file) == n_used;
        result = (fclose(file) == 0) && result;
        Sys_FreeTableSerializationBuffer(buffer);

        if (!result)
        {
            dmSys::Unlink(filename);
            return luaL_error(L, "Could not write to the file %s.", filename);
        }

        lua_pushboolean(L, result);
        return 1;
#endif
    }


    /*# loads a lua table from a file on disk
     * If the file exists, it must have been created by <code>sys.save</code> to be loaded.
     *
     * @name sys.load
     * @param filename [type:string] file to read from
     * @return loaded [type:table] lua table, which is empty if the file could not be found
     * @examples
     *
     * Load data that was previously saved, e.g. an earlier game session:
     *
     * ```lua
     * local my_file_path = sys.get_save_file("my_game", "my_file")
     * local my_table = sys.load(my_file_path)
     * if not next(my_table) then
     *   -- empty table
     * end
     * ```
     */
    int Sys_Load(lua_State* L)
    {
        const char* filename = luaL_checkstring(L, 1);
        FILE* file = fopen(filename, "rb");
        if (file == 0x0)
        {
            lua_newtable(L);
            return 1;
        }

        fseek(file, 0L, SEEK_END);
        uint32_t file_size = ftell(file);
        fseek(file, 0L, SEEK_SET);

        char* buffer = Sys_SetupTableSerializationBuffer(file_size);
        if (!buffer)
        {
            return luaL_error(L, "Could not allocate %d bytes for table deserialization.", file_size);
        }
        size_t nread = fread(buffer, 1, file_size, file);
        bool result = ferror(file) == 0;
        fclose(file);
        if (!result)
        {
            Sys_FreeTableSerializationBuffer(buffer);
            return luaL_error(L, "Could not read from the file %s.", filename);
        }
        PushTable(L, buffer, nread);
        Sys_FreeTableSerializationBuffer(buffer);
        return 1;
    }

    /*# gets the save-file path
     * The save-file path is operating system specific and is typically located under the user's home directory.
     *
     * @note Setting the environment variable `DM_SAVE_HOME` overrides the default application support path.
     *
     * @name sys.get_save_file
     * @param application_id [type:string] user defined id of the application, which helps define the location of the save-file
     * @param file_name [type:string] file-name to get path for
     * @return path [type:string] path to save-file
     * @examples
     *
     * Find a path where we can store data (the example path is on the macOS platform):
     *
     * ```lua
     * local my_file_path = sys.get_save_file("my_game", "my_file")
     * print(my_file_path) --> /Users/my_users/Library/Application Support/my_game/my_file
     * ```
     */
    int Sys_GetSaveFile(lua_State* L)
    {
        const char* application_id = luaL_checkstring(L, 1);

        char app_support_path[1024];
        dmSys::Result r = dmSys::GetApplicationSavePath(application_id, app_support_path, sizeof(app_support_path));
        if (r != dmSys::RESULT_OK)
        {
            return luaL_error(L, "Unable to locate application support path for \"%s\": (%d)", application_id, r);
        }

        const char* filename = luaL_checkstring(L, 2);
        char* dm_home = getenv("DM_SAVE_HOME");

        // Higher priority
        if (dm_home)
        {
            dmStrlCpy(app_support_path, dm_home, sizeof(app_support_path));
        }

        dmStrlCat(app_support_path, dmPath::PATH_CHARACTER, sizeof(app_support_path));
        dmStrlCat(app_support_path, filename, sizeof(app_support_path));
        lua_pushstring(L, app_support_path);

        return 1;
    }


    /*# gets the application path
     * The path from which the application is run.
     *
     * @name sys.get_application_path
     * @return path [type:string] path to application executable
     * @examples
     *
     * Find a path where we can store data (the example path is on the macOS platform):
     *
     * ```lua
     * -- macOS: /Applications/my_game.app
     * local application_path = sys.get_application_path()
     * print(application_path) --> /Applications/my_game.app
     *
     * -- Windows: C:\Program Files\my_game\my_game.exe
     * print(application_path) --> C:\Program Files\my_game
     *
     * -- Linux: /home/foobar/my_game/my_game
     * print(application_path) --> /home/foobar/my_game
     *
     * -- Android package name: com.foobar.my_game
     * print(application_path) --> /data/user/0/com.foobar.my_game
     *
     * -- iOS: my_game.app
     * print(application_path) --> /var/containers/Bundle/Applications/123456AB-78CD-90DE-12345678ABCD/my_game.app
     *
     * -- HTML5: http://www.foobar.com/my_game/
     * print(application_path) --> http://www.foobar.com/my_game
     * ```
     */
    int Sys_GetApplicationPath(lua_State* L)
    {
        char application_path[4096 + 2]; // Linux PATH_MAX is defined to 4096. Windows MAX_PATH is 260.
        dmSys::Result r = dmSys::GetApplicationPath(application_path, sizeof(application_path));
        if (r != dmSys::RESULT_OK)
        {
            return luaL_error(L, "Unable to locate application path: (%d)", r);
        }
        lua_pushstring(L, application_path);

        return 1;
    }

    /*# get config value
     * Get config value from the game.project configuration file.
     *
     * In addition to the project file, configuration values can also be passed
     * to the runtime as command line arguments with the `--config` argument.
     *
     * @name sys.get_config
     * @param key [type:string] key to get value for. The syntax is SECTION.KEY
     * @return value [type:string] config value as a string. nil if the config key doesn't exists
     * @examples
     *
     * Get display width
     *
     * ```lua
     * local width = tonumber(sys.get_config("display.width"))
     * ```
     *
     * Start the engine with a bootstrap config override and a custom config value
     *
     * ```
     * $ mygame --config=bootstrap.main_collection=/mytest.collectionc --config=mygame.testmode=1
     * ```
     *
     * Set and read a custom config value
     *
     * ```lua
     * local testmode = tonumber(sys.get_config("mygame.testmode"))
     * ```
     */

    /*# get config value with default value
     * Get config value from the game.project configuration file with default value
     *
     * @name sys.get_config
     * @param key [type:string] key to get value for. The syntax is SECTION.KEY
     * @param default_value [type:string] default value to return if the value does not exist
     * @return value [type:string] config value as a string. default_value if the config key does not exist
     * @examples
     *
     * Get user config value
     *
     * ```lua
     * local speed = tonumber(sys.get_config("my_game.speed", "10.23"))
     * ```
     */
    int Sys_GetConfig(lua_State* L)
    {
        int top = lua_gettop(L);

        const char* key = luaL_checkstring(L, 1);
        const char* default_value = 0;
        if (lua_isstring(L, 2))
        {
            default_value = lua_tostring(L, 2);
        }

        HContext context = dmScript::GetScriptContext(L);

        dmConfigFile::HConfig config_file = 0;
        if (context)
        {
            config_file = context->m_ConfigFile;
        }

        const char* value;
        if (config_file)
            value = dmConfigFile::GetString(config_file, key, default_value);
        else
            value = 0;

        if (value)
        {
            lua_pushstring(L, value);
        }
        else
        {
            lua_pushnil(L);
        }

        assert(top + 1 == lua_gettop(L));

        return 1;
    }

    /*# open url in default application
     * Open URL in default application, typically a browser
     *
     * @name sys.open_url
     * @param url [type:string] url to open
     * @param [attributes] [type:table] table with attributes
     *
     * `target`
     * - [type:string] [icon:html5]: Optional. Specifies the target attribute or the name of the window. The following values are supported:
     * - `_self` - URL replaces the current page. This is default.
     * - `_blank` - URL is loaded into a new window, or tab.
     * - `_parent` - URL is loaded into the parent frame.
     * - `_top` - URL replaces any framesets that may be loaded.
     * - `name` - The name of the window (Note: the name does not specify the title of the new window).
     *
     * @return success [type:boolean] a boolean indicating if the url could be opened or not
     * @examples
     *
     * Open an URL:
     *
     * ```lua
     * local success = sys.open_url("http://www.defold.com", {target = "_blank"})
     * if not success then
     *   -- could not open the url...
     * end
     * ```
     */
    int Sys_OpenURL(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1)
        int top = lua_gettop(L);
        const char* url = luaL_checkstring(L, 1);
        dmSys::Result result;
        if (top > 1)
        {
            luaL_checktype(L, 2, LUA_TTABLE);
            lua_pushvalue(L, 2);

            lua_getfield(L, -1, "target");
            const char* target = lua_isnil(L, -1) ? 0 : luaL_checkstring(L, -1);
            lua_pop(L, 1);

            lua_pop(L, 1);
            result = dmSys::OpenURL(url, target);
        }
        else
        {
            result = dmSys::OpenURL(url, 0);
        }

        lua_pushboolean(L, result == dmSys::RESULT_OK);

        return 1;
    }

    /*# loads resource from game data
     *
     * Loads a custom resource. Specify the full filename of the resource that you want
     * to load. When loaded, the file data is returned as a string.
     * If loading fails, the function returns nil plus the error message.
     *
     * In order for the engine to include custom resources in the build process, you need
     * to specify them in the "custom_resources" key in your "game.project" settings file.
     * You can specify single resource files or directories. If a directory is included
     * in the resource list, all files and directories in that directory is recursively
     * included:
     *
     * For example "main/data/,assets/level_data.json".
     *
     * @name sys.load_resource
     * @param filename [type:string] resource to load, full path
     * @return data [type:string] loaded data, or `nil` if the resource could not be loaded
     * @return error [type:string] the error message, or `nil` if no error occurred
     * @examples
     *
     * ```lua
     * -- Load level data into a string
     * local data, error = sys.load_resource("/assets/level_data.json")
     * -- Decode json string to a Lua table
     * if data then
     *   local data_table = json.decode(data)
     *   pprint(data_table)
     * else
     *   print(error)
     * end
     * ```
     */
    int Sys_LoadResource(lua_State* L)
    {
        int top = lua_gettop(L);
        const char* filename = luaL_checkstring(L, 1);

        HContext context = dmScript::GetScriptContext(L);

        void* resource;
        uint32_t resource_size;
        dmResource::Result r = dmResource::GetRaw(context->m_ResourceFactory, filename, &resource, &resource_size);
        if (r != dmResource::RESULT_OK) {
            lua_pushnil(L);
            lua_pushfstring(L, "Failed to load resource: %s (%d)", filename, r);
            assert(top + 2 == lua_gettop(L));
            return 2;
        }
        lua_pushlstring(L, (const char*) resource, resource_size);
        free(resource);
        assert(top + 1 == lua_gettop(L));
        return 1;
    }

    /*# get system information
     *
     * Returns a table with system information.
     * @name sys.get_sys_info
     * @param options [type:table] (optional) options table
     * - ignore_secure [type:boolean] this flag ignores values might be secured by OS e.g. `device_ident`
     * @return sys_info [type:table] table with system information in the following fields:
     *
     * `device_model`
     * : [type:string] [icon:ios][icon:android] Only available on iOS and Android.
     *
     * `manufacturer`
     * : [type:string] [icon:ios][icon:android] Only available on iOS and Android.
     *
     * `system_name`
     * : [type:string] The system name: "Darwin", "Linux", "Windows", "HTML5", "Android" or "iPhone OS"
     *
     * `system_version`
     * : [type:string] The system OS version.
     *
     * `api_version`
     * : [type:string] The API version on the system.
     *
     * `language`
     * : [type:string] Two character ISO-639 format, i.e. "en".
     *
     * `device_language`
     * : [type:string] Two character ISO-639 format (i.e. "sr") and, if applicable, followed by a dash (-) and an ISO 15924 script code (i.e. "sr-Cyrl" or "sr-Latn"). Reflects the device preferred language.
     *
     * `territory`
     * : [type:string] Two character ISO-3166 format, i.e. "US".
     *
     * `gmt_offset`
     * : [type:number] The current offset from GMT (Greenwich Mean Time), in minutes.
     *
     * `device_ident`
     * : [type:string] This value secured by OS. [icon:ios] "identifierForVendor" on iOS. [icon:android] "android_id" on Android. On Android, you need to add `READ_PHONE_STATE` permission to be able to get this data. We don't use this permission in Defold.
     *
     * `user_agent`
     * : [type:string] [icon:html5] The HTTP user agent, i.e. "Mozilla/5.0 (Macintosh; Intel Mac OS X 10_12_3) AppleWebKit/602.4.8 (KHTML, like Gecko) Version/10.0.3 Safari/602.4.8"
     *
     * @examples
     *
     * How to get system information:
     *
     * ```lua
     * local info = sys.get_sys_info()
     * if info.system_name == "HTML5" then
     *   -- We are running in a browser.
     * end
     * ```
     */
    static int Sys_GetSysInfo(lua_State* L)
    {
        int top = lua_gettop(L);

        dmSys::SystemInfo info;
        dmSys::GetSystemInfo(&info);

        bool ignore_secure_values = false;

        if ( top >= 1)
        {
            luaL_checktype(L, 1, LUA_TTABLE);
            lua_pushvalue(L, 1);

            lua_getfield(L, -1, "ignore_secure");
            ignore_secure_values = lua_isnil(L, -1) ? false : lua_toboolean(L, -1);
            lua_pop(L, 1);

            lua_pop(L, 1);
        }

        if (!ignore_secure_values)
        {
            dmSys::GetSecureInfo(&info);
        } 

        lua_newtable(L);
        lua_pushliteral(L, "device_model");
        lua_pushstring(L, info.m_DeviceModel);
        lua_rawset(L, -3);
        lua_pushliteral(L, "manufacturer");
        lua_pushstring(L, info.m_Manufacturer);
        lua_rawset(L, -3);
        lua_pushliteral(L, "system_name");
        lua_pushstring(L, info.m_SystemName);
        lua_rawset(L, -3);
        lua_pushliteral(L, "system_version");
        lua_pushstring(L, info.m_SystemVersion);
        lua_rawset(L, -3);
        lua_pushliteral(L, "api_version");
        lua_pushstring(L, info.m_ApiVersion);
        lua_rawset(L, -3);
        lua_pushliteral(L, "language");
        lua_pushstring(L, info.m_Language);
        lua_rawset(L, -3);
        lua_pushliteral(L, "device_language");
        lua_pushstring(L, info.m_DeviceLanguage);
        lua_rawset(L, -3);
        lua_pushliteral(L, "territory");
        lua_pushstring(L, info.m_Territory);
        lua_rawset(L, -3);
        lua_pushliteral(L, "gmt_offset");
        lua_pushinteger(L, info.m_GmtOffset);
        lua_rawset(L, -3);
        lua_pushliteral(L, "device_ident");
        lua_pushstring(L, info.m_DeviceIdentifier);
        lua_rawset(L, -3);
        lua_pushliteral(L, "user_agent");
        lua_pushstring(L, info.m_UserAgent ? info.m_UserAgent : "");
        lua_rawset(L, -3);

        assert(top + 1 == lua_gettop(L));
        return 1;
    }

    /*# get engine information
     *
     * Returns a table with engine information.
     *
     * @name sys.get_engine_info
     * @return engine_info [type:table] table with engine information in the following fields:
     *
     * `version`
     * : [type:string] The current Defold engine version, i.e. "1.2.96"
     *
     * `version_sha1`
     * : [type:string] The SHA1 for the current engine build, i.e. "0060183cce2e29dbd09c85ece83cbb72068ee050"
     *
     * `is_debug`
     * : [type:boolean] If the engine is a debug or release version
     *
     * @examples
     *
     * How to retrieve engine information:
     *
     * ```lua
     * -- Update version text label so our testers know what version we're running
     * local engine_info = sys.get_engine_info()
     * local version_str = "Defold " .. engine_info.version .. "\n" .. engine_info.version_sha1
     * gui.set_text(gui.get_node("version"), version_str)
     * ```
     */
    int Sys_GetEngineInfo(lua_State* L)
    {
        int top = lua_gettop(L);

        dmSys::EngineInfo info;
        dmSys::GetEngineInfo(&info);

        lua_newtable(L);
        lua_pushliteral(L, "version");
        lua_pushstring(L, info.m_Version);
        lua_rawset(L, -3);
        lua_pushliteral(L, "version_sha1");
        lua_pushstring(L, info.m_VersionSHA1);
        lua_rawset(L, -3);
        lua_pushliteral(L, "is_debug");
        lua_pushboolean(L, info.m_IsDebug);
        lua_rawset(L, -3);

        assert(top + 1 == lua_gettop(L));
        return 1;
    }

    /*# get application information
     *
     * Returns a table with application information for the requested app.
     *
     * [icon:iOS] On iOS, the `app_string` is an url scheme for the app that is queried. Your
     * game needs to list the schemes that are queried in an `LSApplicationQueriesSchemes` array
     * in a custom "Info.plist".
     *
     * [icon:android] On Android, the `app_string` is the package identifier for the app.
     *
     * @name sys.get_application_info
     * @param app_string [type:string] platform specific string with application package or query, see above for details.
     * @return app_info [type:table] table with application information in the following fields:
     *
     * `installed`
     * : [type:boolean] `true` if the application is installed, `false` otherwise.
     *
     * @examples
     *
     * Check if twitter is installed:
     *
     * ```lua
     * sysinfo = sys.get_sys_info()
     * twitter = {}
     *
     * if sysinfo.system_name == "Android" then
     *   twitter = sys.get_application_info("com.twitter.android")
     * elseif sysinfo.system_name == "iPhone OS" then
     *   twitter = sys.get_application_info("twitter:")
     * end
     *
     * if twitter.installed then
     *   -- twitter is installed!
     * end
     * ```
     *
     * [icon:iOS] Info.plist for the iOS app needs to list the schemes that are queried:
     *
     * ```xml
     * ...
     * <key>LSApplicationQueriesSchemes</key>
     *  <array>
     *    <string>twitter</string>
     *  </array>
     * ...
     * ```
     */
    int Sys_GetApplicationInfo(lua_State* L)
    {
        int top = lua_gettop(L);

        const char* id = luaL_checkstring(L, 1);

        dmSys::ApplicationInfo info;
        dmSys::GetApplicationInfo(id, &info);

        lua_newtable(L);
        lua_pushliteral(L, "installed");
        lua_pushboolean(L, info.m_Installed);
        lua_rawset(L, -3);

        assert(top + 1 == lua_gettop(L));
        return 1;
    }

    // Android version 6 Marshmallow (API level 23) and up does not support getting hw adr programmatically (https://developer.android.com/about/versions/marshmallow/android-6.0-changes.html#behavior-hardware-id).
    static bool IsAndroidMarshmallowOrAbove()
    {
        #ifdef ANDROID
            const long android_marshmallow_api_level = 23;
            return android_get_device_api_level() >= android_marshmallow_api_level;
        #endif
        return false;
    }

    /*# enumerate network interfaces
     *
     * Returns an array of tables with information on network interfaces.
     *
     * @name sys.get_ifaddrs
     * @return ifaddrs [type:table] an array of tables. Each table entry contain the following fields:
     *
     * `name`
     * : [type:string] Interface name
     *
     * `address`
     * : [type:string] IP address. [icon:attention] might be `nil` if not available.
     *
     * `mac`
     * : [type:string] Hardware MAC address. [icon:attention] might be nil if not available.
     *
     * `up`
     * : [type:boolean] `true` if the interface is up (available to transmit and receive data), `false` otherwise.
     *
     * `running`
     * : [type:boolean] `true` if the interface is running, `false` otherwise.
     *
     * @examples
     *
     * How to get the IP address of interface "en0":
     *
     * ```lua
     * ifaddrs = sys.get_ifaddrs()
     * for _,interface in ipairs(ifaddrs) do
     *   if interface.name == "en0" then
     *     local ip = interface.address
     *   end
     * end
     * ```
     */
    int Sys_GetIfaddrs(lua_State* L)
    {
        int top = lua_gettop(L);
        const uint32_t max_count = 16;
        dmSocket::IfAddr addresses[max_count];

        uint32_t count = 0;
        dmSocket::GetIfAddresses(addresses, max_count, &count);
        lua_createtable(L, count, 0);
        for (uint32_t i = 0; i < count; ++i)
        {
            dmSocket::IfAddr* ifa = &addresses[i];

            lua_newtable(L);

            lua_pushstring(L, ifa->m_Name);
            lua_setfield(L, -2, "name");

            if (ifa->m_Flags & dmSocket::FLAGS_INET)
            {
                char* ip = dmSocket::AddressToIPString(ifa->m_Address);
                if (ip)
                    lua_pushstring(L, ip);
                else
                    lua_pushnil(L);
                free(ip);
            }
            else
            {
                lua_pushnil(L);
            }
            lua_setfield(L, -2, "address");

            if (ifa->m_Address.m_family == dmSocket::DOMAIN_IPV4)
            {
                lua_pushstring(L, "ipv4");
            }
            else if (ifa->m_Address.m_family == dmSocket::DOMAIN_IPV6)
            {
                lua_pushstring(L, "ipv6");
            }
            else
            {
                lua_pushnil(L);
            }
            lua_setfield(L, -2, "family");

            if (ifa->m_Flags & dmSocket::FLAGS_LINK)
            {
                char tmp[64];
                dmSnPrintf(tmp, sizeof(tmp), "%02x:%02x:%02x:%02x:%02x:%02x",
                        ifa->m_MacAddress[0],
                        ifa->m_MacAddress[1],
                        ifa->m_MacAddress[2],
                        ifa->m_MacAddress[3],
                        ifa->m_MacAddress[4],
                        ifa->m_MacAddress[5]);
                lua_pushstring(L, tmp);
            }
            else if (IsAndroidMarshmallowOrAbove()) // Marshmallow and above should return const value MAC address (https://developer.android.com/about/versions/marshmallow/android-6.0-changes.html#behavior-hardware-id).
            {
                lua_pushstring(L, "02:00:00:00:00:00");
            }
            else
            {
                lua_pushnil(L);
            }
            lua_setfield(L, -2, "mac");

            lua_pushboolean(L, (ifa->m_Flags & dmSocket::FLAGS_UP) != 0);
            lua_setfield(L, -2, "up");

            lua_pushboolean(L, (ifa->m_Flags & dmSocket::FLAGS_RUNNING) != 0);
            lua_setfield(L, -2, "running");

            lua_rawseti(L, -2, i + 1);
        }

        assert(top + 1 == lua_gettop(L));
        return 1;
    }

    /*# set the error handler
     *
     * Set the Lua error handler function.
     * The error handler is a function which is called whenever a lua runtime error occurs.
     *
     * @name sys.set_error_handler
     * @param error_handler [type:function(source, message, traceback)] the function to be called on error
     *
     * `source`
     * : [type:string] The runtime context of the error. Currently, this is always `"lua"`.
     *
     * `message`
     * : [type:string] The source file, line number and error message.
     *
     * `traceback`
     * : [type:string] The stack traceback.
     *
     * @examples
     *
     * Install error handler that just prints the errors
     *
     * ```lua
     * local function my_error_handler(source, message, traceback)
     *   print(source)    --> lua
     *   print(message)   --> main/my.script:10: attempt to perform arithmetic on a string value
     *   print(traceback) --> stack traceback:
     *                    -->         main/test.script:10: in function 'boom'
     *                    -->         main/test.script:15: in function <main/my.script:13>
     * end
     *
     * local function boom()
     *   return 10 + "string"
     * end
     *
     * function init(self)
     *   sys.set_error_handler(my_error_handler)
     *   boom()
     *end
     * ```
     */
    int Sys_SetErrorHandler(lua_State* L)
    {
        int top = lua_gettop(L);
        luaL_checktype(L, 1, LUA_TFUNCTION);

        // store the supplied function in debug.<SCRIPT_ERROR_HANDLER_VAR> so that it can
        // be called from dmScript::PCall
        lua_getfield(L, LUA_GLOBALSINDEX, "debug");
        if (!lua_istable(L, -1)) {
            lua_pop(L, 1);
            return 1;
        }

        lua_pushvalue(L, 1);
        lua_setfield(L, -2, SCRIPT_ERROR_HANDLER_VAR);
        lua_pop(L, 1);

        assert(top == lua_gettop(L));
        return 0;
    }

    /*# set host to check for network connectivity against
     *
     * Sets the host that is used to check for network connectivity against.
     *
     * @name sys.set_connectivity_host
     * @param host [type:string] hostname to check against
     * @examples
     *
     * ```lua
     * sys.set_connectivity_host("www.google.com")
     * ```
     */
    static int Sys_SetConnectivityHost(lua_State* L)
    {
        int top = lua_gettop(L);
        dmSys::SetNetworkConnectivityHost( luaL_checkstring(L, 1) );
        assert(top == lua_gettop(L));
        return 0;
    }

    /*# get current network connectivity status
     *
     * [icon:ios] [icon:android] Returns the current network connectivity status
     * on mobile platforms.
     *
     * On desktop, this function always return `sys.NETWORK_CONNECTED`.
     *
     * @name sys.get_connectivity
     * @return status [type:constant] network connectivity status:
     *
     * - `sys.NETWORK_DISCONNECTED` (no network connection is found)
     * - `sys.NETWORK_CONNECTED_CELLULAR` (connected through mobile cellular)
     * - `sys.NETWORK_CONNECTED` (otherwise, Wifi)
     *
     * @examples
     *
     * Check if we are connected through a cellular connection
     *
     * ```lua
     * if (sys.NETWORK_CONNECTED_CELLULAR == sys.get_connectivity()) then
     *   print("Connected via cellular, avoid downloading big files!")
     * end
     * ```
     */
    static int Sys_GetConnectivity(lua_State* L)
    {
        int top = lua_gettop(L);
        lua_pushnumber(L, dmSys::GetNetworkConnectivity());
        assert(top + 1 == lua_gettop(L));
        return 1;
    }

    #define SYSTEM_SOCKET_NAME "@system"

    static void GetSystemURL(dmMessage::URL* out_url)
    {
        dmMessage::HSocket socket;
        dmMessage::Result result = dmMessage::GetSocket(SYSTEM_SOCKET_NAME, &socket);
        assert(result == dmMessage::RESULT_OK);
        assert(socket);

        dmMessage::URL url;
        out_url->m_Socket = socket;
        out_url->m_Path = 0;
        out_url->m_Fragment = 0;
    }

    /*# exits application
    * Terminates the game application and reports the specified <code>code</code> to the OS.
    *
    * @name sys.exit
    * @param code [type:number] exit code to report to the OS, 0 means clean exit
    * @examples
    *
    * This examples demonstrates how to exit the application when some kind of quit messages is received (maybe from gui or similar):
    *
    * ```lua
    * function on_message(self, message_id, message, sender)
    *     if message_id == hash("quit") then
    *         sys.exit(0)
    *     end
    * end
    * ```
    */
    static int Sys_Exit(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 0);

        dmSystemDDF::Exit msg;
        msg.m_Code = luaL_checkinteger(L, 1);

        dmMessage::URL url;
        GetSystemURL(&url);

        dmMessage::Result result = dmMessage::Post(0, &url, dmSystemDDF::Exit::m_DDFDescriptor->m_NameHash, 0, (uintptr_t) dmSystemDDF::Exit::m_DDFDescriptor, &msg, sizeof(msg), 0);
        assert(result == dmMessage::RESULT_OK);

        return 0;
    }

    /*# reboot engine with arguments
    * Reboots the game engine with a specified set of arguments.
    * Arguments will be translated into command line arguments. Calling reboot
    * function is equivalent to starting the engine with the same arguments.
    *
    * On startup the engine reads configuration from "game.project" in the
    * project root.
    *
    * @name sys.reboot
    * @param arg1 [type:string] argument 1
    * @param arg2 [type:string] argument 2
    * @param arg3 [type:string] argument 3
    * @param arg4 [type:string] argument 4
    * @param arg5 [type:string] argument 5
    * @param arg6 [type:string] argument 6
    * @examples
    *
    * How to reboot engine with a specific bootstrap collection.
    *
    * ```lua
    * local arg1 = '--config=bootstrap.main_collection=/my.collectionc'
    * local arg2 = 'build/default/game.projectc'
    * sys.reboot(arg1, arg2)
    * ```
    */
    static int Sys_Reboot(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 0);

#define PUSH_FIELD(name, index) \
        if (lua_isstring(L, index)) { \
            lua_pushstring(L, luaL_checkstring(L, index)); \
            lua_setfield(L, -2, name);\
        }

        lua_newtable(L);
        PUSH_FIELD("arg1", 1);
        PUSH_FIELD("arg2", 2);
        PUSH_FIELD("arg3", 3);
        PUSH_FIELD("arg4", 4);
        PUSH_FIELD("arg5", 5);
        PUSH_FIELD("arg6", 6);

#undef PUSH_FIELD

        uint8_t data[dmMessage::DM_MESSAGE_MAX_DATA_SIZE];
        uint32_t data_size = dmScript::CheckDDF(L, dmSystemDDF::Reboot::m_DDFDescriptor, (char*)data, sizeof(data), -1);

        dmMessage::URL url;
        GetSystemURL(&url);

        dmMessage::Result result = dmMessage::Post(0, &url, dmSystemDDF::Reboot::m_DDFDescriptor->m_NameHash, 0, (uintptr_t) dmSystemDDF::Reboot::m_DDFDescriptor, data, data_size, 0);
        if (result != dmMessage::RESULT_OK)
        {
            return DM_LUA_ERROR("Failed to send reboot message!");
        }
        lua_pop(L, 1);
        return 0;
    }

    /*# set vsync swap interval
    * Set the vsync swap interval. The interval with which to swap the front and back buffers
    * in sync with vertical blanks (v-blank), the hardware event where the screen image is updated
    * with data from the front buffer. A value of 1 swaps the buffers at every v-blank, a value of
    * 2 swaps the buffers every other v-blank and so on. A value of 0 disables waiting for v-blank
    * before swapping the buffers. Default value is 1.
    *
    * When setting the swap interval to 0 and having `vsync` disabled in
    * "game.project", the engine will try to respect the set frame cap value from
    * "game.project" in software instead.
    *
    * This setting may be overridden by driver settings.
    *
    * @name sys.set_vsync_swap_interval
    * @param swap_interval [type:number] target swap interval.
    * @examples
    *
    * Setting the swap intervall to swap every v-blank
    *
    * ```lua
    * sys.set_vsync_swap_interval(1)
    * ```
    */
    static int Sys_SetVsyncSwapInterval(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 0);

        dmSystemDDF::SetVsync msg;
        msg.m_SwapInterval = luaL_checkinteger(L, 1);

        dmMessage::URL url;
        GetSystemURL(&url);

        dmMessage::Result result = dmMessage::Post(0, &url, dmSystemDDF::SetVsync::m_DDFDescriptor->m_NameHash, 0, (uintptr_t) dmSystemDDF::SetVsync::m_DDFDescriptor, &msg, sizeof(msg), 0);
        assert(result == dmMessage::RESULT_OK);

        return 0;
    }

    /*# set update frequency
    * Set game update-frequency (frame cap). This option is equivalent to `display.update_frequency` in
    * the "game.project" settings but set in run-time. If `Vsync` checked in "game.project", the rate will
    * be clamped to a swap interval that matches any detected main monitor refresh rate. If `Vsync` is
    * unchecked the engine will try to respect the rate in software using timers. There is no
    * guarantee that the frame cap will be achieved depending on platform specifics and hardware settings.
    *
    * @name sys.set_update_frequency
    * @param frequency [type:number] target frequency. 60 for 60 fps
    * @examples
    *
    * Setting the update frequency to 60 frames per second
    *
    * ```lua
    * sys.set_update_frequency(60)
    * ```
    */
    static int Sys_SetUpdateFrequency(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 0);

        dmSystemDDF::SetUpdateFrequency msg;
        msg.m_Frequency = luaL_checkinteger(L, 1);

        dmMessage::URL url;
        GetSystemURL(&url);

        dmMessage::Result result = dmMessage::Post(0, &url, dmSystemDDF::SetUpdateFrequency::m_DDFDescriptor->m_NameHash, 0, (uintptr_t) dmSystemDDF::SetUpdateFrequency::m_DDFDescriptor, &msg, sizeof(msg), 0);
        assert(result == dmMessage::RESULT_OK);

        return 0;
    }

    /*# serializes a lua table to a buffer and returns it
     * The buffer can later deserialized by <code>sys.deserialize</code>.
     * This method has all the same limitations as <code>sys.save</code>.
     *
     * @name sys.serialize
     * @param table [type:table] lua table to serialize
     * @return buffer [type:string] serialized data buffer
     * @examples
     *
     * Serialize table:
     *
     * ```lua
     * local my_table = {}
     * table.insert(my_table, "my_value")
     * local buffer = sys.serialize(my_table)
     * ```
     */

    static int Sys_Serialize(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);
        luaL_checktype(L, 1, LUA_TTABLE);

        uint32_t table_size = CheckTableSize(L, 1);
        char* buffer = Sys_SetupTableSerializationBuffer(table_size);
        if (!buffer)
        {
            return luaL_error(L, "Could not allocate %d bytes for table serialization.", table_size);
        }

        uint32_t n_used = CheckTable(L, buffer, table_size, 1);
        lua_pushlstring(L, (const char*)buffer, n_used);
        Sys_FreeTableSerializationBuffer(buffer);
        return 1;

    }

    /*# deserializes buffer into a lua table
     *
     * @name sys.deserialize
     * @param buffer [type:string] buffer to deserialize from
     * @return table [type:table] lua table with deserialized data
     * @examples
     *
     * Deserialize a lua table that was previously serialized:
     *
     * ```lua
     * local buffer = sys.serialize(my_table)
     * local table = sys.deserialize(buffer)
     * ```
     */

    static int Sys_Deserialize(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);
        size_t bytes_lenght;
        const char* bytes = luaL_checklstring(L, 1, &bytes_lenght);
        PushTable(L, bytes, bytes_lenght);
        return 1;
    }

    static const luaL_reg ScriptSys_methods[] =
    {
        {"save", Sys_Save},
        {"load", Sys_Load},
        {"get_save_file", Sys_GetSaveFile},
        {"get_config", Sys_GetConfig},
        {"open_url", Sys_OpenURL},
        {"load_resource", Sys_LoadResource},
        {"get_sys_info", Sys_GetSysInfo},
        {"get_engine_info", Sys_GetEngineInfo},
        {"get_application_info", Sys_GetApplicationInfo},
        {"get_application_path", Sys_GetApplicationPath},
        {"get_ifaddrs", Sys_GetIfaddrs},
        {"set_error_handler", Sys_SetErrorHandler},
        {"set_connectivity_host", Sys_SetConnectivityHost},
        {"get_connectivity", Sys_GetConnectivity},
        {"exit", Sys_Exit},
        {"reboot", Sys_Reboot},
        {"set_update_frequency", Sys_SetUpdateFrequency},
        {"set_vsync_swap_interval", Sys_SetVsyncSwapInterval},
        {"serialize", Sys_Serialize},
        {"deserialize", Sys_Deserialize},
        {0, 0}
    };

    /*# no network connection found
     * @name sys.NETWORK_DISCONNECTED
     * @variable
     */

    /*# network connected through mobile cellular
     * @name sys.NETWORK_CONNECTED_CELLULAR
     * @variable
     */

    /*# network connected through other, non cellular, connection
     * @name sys.NETWORK_CONNECTED
     * @variable
     */

    void InitializeSys(lua_State* L)
    {
        int top = lua_gettop(L);

        lua_pushvalue(L, LUA_GLOBALSINDEX);
        luaL_register(L, LIB_NAME, ScriptSys_methods);

#define SETCONSTANT(name, val) \
        lua_pushnumber(L, (lua_Number) val); \
        lua_setfield(L, -2, #name);\

        SETCONSTANT(NETWORK_CONNECTED, dmSys::NETWORK_CONNECTED);
        SETCONSTANT(NETWORK_CONNECTED_CELLULAR, dmSys::NETWORK_CONNECTED_CELLULAR);
        SETCONSTANT(NETWORK_DISCONNECTED, dmSys::NETWORK_DISCONNECTED);

#undef SETCONSTANT

        lua_pop(L, 2);

        assert(top == lua_gettop(L));
    }
}
