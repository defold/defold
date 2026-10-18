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

#include "record_private.h"
#include "record_script.h"
#include <dmsdk/script/script.h>
#include <math.h>
#include <string.h>

/*# Video recording
 *
 * Record the game window to an MP4 file. One recording may run at a time,
 * shared with the engine's HTTP recording service.
 *
 * Recording is supported by desktop debug engines. Unsupported builds return
 * an error from [ref:recorder.start]. Application audio support depends on the
 * platform; audio is disabled by default.
 *
 * @document
 * @name Video recording
 * @namespace recorder
 * @language Lua
 */

namespace dmRecord
{
    /*# recording events
     * @enum
     * @name recorder.EVENT
     * @member recorder.EVENT_STARTED Recording has started.
     * @member recorder.EVENT_STOPPED Recording has stopped and the MP4 is ready to read.
     * @member recorder.EVENT_ERROR Recording failed; result.error contains the reason.
     */

    /*# recording options
     * @struct
     * @name recorder.options
     * @member path [type:string] Writable UTF-8 MP4 output path; the parent directory must exist.
     * @member fps? [type:integer] Capture rate from 1 to 60; defaults to 30.
     * @member audio? [type:boolean] Record application audio; defaults to false.
     */

    /*# recording event result
     * @struct
     * @name recorder.result
     * @member path [type:string] Output file path.
     * @member width [type:integer] Recorded video width in pixels.
     * @member height [type:integer] Recorded video height in pixels.
     * @member fps [type:integer] Capture rate.
     * @member audio [type:boolean] Whether application audio is included.
     * @member error? [type:string] Failure reason, present for [ref:recorder.EVENT_ERROR].
     */

    enum RecordEvent
    {
        EVENT_STARTED = 1,
        EVENT_STOPPED = 2,
        EVENT_ERROR = 3
    };

    static void* g_Window;
    static bool g_Initialized;
    static dmScript::LuaCallbackInfo* g_Callback;
    static uint64_t g_RecordingId;
    static bool g_Started;
    static bool g_StartPending;
    static bool g_EndPending;
    static Status g_StartStatus;
    static Status g_EndStatus;

    static void OnStatus(const Status* status)
    {
        if (!g_Callback || status->m_Id != g_RecordingId)
            return;
        if (status->m_Active)
        {
            if (!g_Started)
            {
                g_Started = true;
                g_StartPending = true;
                g_StartStatus = *status;
            }
        }
        else
        {
            g_EndPending = true;
            g_EndStatus = *status;
        }
    }

    static StatusListener g_Listener = {OnStatus, 0};

    static int ReturnError(lua_State* L, const char* error)
    {
        lua_pushnil(L);
        lua_pushstring(L, error);
        return 2;
    }

    /*# start recording
     * Starts an asynchronous recording of the game window at its native pixel size.
     * The caller owns the output file, which remains on disk after engine shutdown.
     * An existing file at the specified path may be overwritten.
     *
     * A successful return only means the request was accepted. The callback receives
     * [ref:recorder.EVENT_STARTED] when capture starts, followed by
     * [ref:recorder.EVENT_STOPPED] or [ref:recorder.EVENT_ERROR]. Startup failure produces
     * only EVENT_ERROR. Callbacks run during engine update, never inside start/stop.
     *
     * Window resizing stops and finalizes the recording. Deleting the calling script
     * discards its callback but does not stop recording. Engine shutdown finalizes
     * recording without invoking callbacks.
     *
     * @name recorder.start
     * @param options [type:recorder.options] recording options
     * @param callback [type:fun(self:script_instance, event:recorder.EVENT, result:recorder.result)] recording event callback
     * @return accepted [type:boolean|nil] true if accepted, otherwise nil
     * @return error [type:string|nil] reason the request was rejected
     * @examples
     * ```lua
     * local ok, err = recorder.start({
     *     path = sys.get_save_file("my_game", "capture.mp4"),
     *     fps = 30,
     *     audio = false,
     * }, function(self, event, result)
     *     if event == recorder.EVENT_STOPPED then
     *         print("Saved:", result.path)
     *     elseif event == recorder.EVENT_ERROR then
     *         print(result.error)
     *     end
     * end)
     * if not ok then print(err) end
     * ```
     */
    static int ScriptStart(lua_State* L)
    {
        luaL_checktype(L, 1, LUA_TTABLE);
        luaL_checktype(L, 2, LUA_TFUNCTION);
        lua_pushnil(L);
        while (lua_next(L, 1))
        {
            if (lua_type(L, -2) != LUA_TSTRING)
                return luaL_error(L, "recorder.start options must have string keys");
            const char* key = lua_tostring(L, -2);
            if (strcmp(key, "path") && strcmp(key, "fps") && strcmp(key, "audio"))
                return luaL_error(L, "unknown recorder.start option '%s'", key);
            lua_pop(L, 1);
        }

        StartParams params;
        lua_getfield(L, 1, "path");
        luaL_checktype(L, -1, LUA_TSTRING);
        size_t path_size;
        params.m_Path = lua_tolstring(L, -1, &path_size);
        if (!path_size || path_size >= sizeof(Status().m_Path) || strlen(params.m_Path) != path_size)
            return luaL_error(L, "recorder.start path must contain 1–1023 bytes without NUL characters");
        // Keep the string on the stack until Start copies it. An options
        // metatable may have generated it instead of retaining it in the table.
        lua_getfield(L, 1, "fps");
        if (!lua_isnil(L, -1))
        {
            luaL_checktype(L, -1, LUA_TNUMBER);
            lua_Number fps = lua_tonumber(L, -1);
            if (!(fps >= 1 && fps <= 60) || fps != floor(fps))
                return luaL_error(L, "recorder.start fps must be an integer from 1 to 60");
            params.m_Fps = (uint32_t)fps;
        }
        lua_pop(L, 1);
        lua_getfield(L, 1, "audio");
        if (!lua_isnil(L, -1))
        {
            luaL_checktype(L, -1, LUA_TBOOLEAN);
            params.m_Audio = lua_toboolean(L, -1);
        }
        lua_pop(L, 1);
        if (!g_Initialized)
            return ReturnError(L, "recording is shutting down");
        if (g_Callback)
            return ReturnError(L, "a recording operation is already active or awaiting its callback");

        dmScript::LuaCallbackInfo* callback = dmScript::CreateCallback(L, 2);
        if (!callback)
            return ReturnError(L, "recorder.start requires a script instance");
        params.m_Window = g_Window;
        Status status;
        if (!Start(&params, &status))
        {
            dmScript::DestroyCallback(callback);
            return ReturnError(L, status.m_Failure);
        }
        g_Callback = callback;
        g_RecordingId = status.m_Id;
        g_Started = g_StartPending = g_EndPending = false;
        lua_pushboolean(L, 1);
        return 1;
    }

    /*# stop recording
     * Requests finalization of the current recording, including one started through
     * HTTP. Wait for [ref:recorder.EVENT_STARTED] before calling stop for a Lua recording.
     * The original start callback receives [ref:recorder.EVENT_STOPPED] only after the
     * MP4 is finalized and ready to read, or [ref:recorder.EVENT_ERROR] on failure.
     * Returns an error if no recording is active or an operation is pending.
     *
     * @name recorder.stop
     * @return accepted [type:boolean|nil] true if accepted, otherwise nil
     * @return error [type:string|nil] reason the request was rejected
     */
    static int ScriptStop(lua_State* L)
    {
        if (!g_Initialized)
            return ReturnError(L, "recording is shutting down");
        Status status;
        if (!Stop(&status))
            return ReturnError(L, status.m_Failure);
        lua_pushboolean(L, 1);
        return 1;
    }

    static void Invoke(dmScript::LuaCallbackInfo* callback, RecordEvent event, const Status* status)
    {
        lua_State* L = dmScript::GetCallbackLuaContext(callback);
        DM_LUA_STACK_CHECK(L, 0);
        if (!dmScript::IsCallbackValid(callback) || !dmScript::SetupCallback(callback))
            return;
        lua_pushinteger(L, event);
        lua_newtable(L);
        lua_pushstring(L, status->m_Path);
        lua_setfield(L, -2, "path");
        lua_pushinteger(L, status->m_Width);
        lua_setfield(L, -2, "width");
        lua_pushinteger(L, status->m_Height);
        lua_setfield(L, -2, "height");
        lua_pushinteger(L, status->m_Fps);
        lua_setfield(L, -2, "fps");
        lua_pushboolean(L, status->m_Audio);
        lua_setfield(L, -2, "audio");
        if (event == EVENT_ERROR)
        {
            lua_pushstring(L, status->m_Failure);
            lua_setfield(L, -2, "error");
        }
        dmScript::PCall(L, 3, 0);
        dmScript::TeardownCallback(callback);
    }

    void UpdateScript()
    {
        Status status;
        GetStatus(&status);
        if (!g_Callback)
            return;
        if (!dmScript::IsCallbackValid(g_Callback))
        {
            dmScript::DestroyCallback(g_Callback);
            g_Callback = 0;
            return;
        }
        if (g_StartPending)
        {
            g_StartPending = false;
            Invoke(g_Callback, EVENT_STARTED, &g_StartStatus);
        }
        if (g_EndPending)
        {
            // Detach before invoking Lua: a completion callback may start a new recording.
            dmScript::LuaCallbackInfo* callback = g_Callback;
            Status completed = g_EndStatus;
            g_Callback = 0;
            g_EndPending = false;
            Invoke(callback, completed.m_Finalized ? EVENT_STOPPED : EVENT_ERROR, &completed);
            dmScript::DestroyCallback(callback);
        }
    }

    void InitializeScript(lua_State* L, void* native_window)
    {
        DM_LUA_STACK_CHECK(L, 0);
        const luaL_Reg methods[] = {{"start", ScriptStart}, {"stop", ScriptStop}, {0, 0}};
        luaL_register(L, "recorder", methods);
        lua_pushinteger(L, EVENT_STARTED);
        lua_setfield(L, -2, "EVENT_STARTED");
        lua_pushinteger(L, EVENT_STOPPED);
        lua_setfield(L, -2, "EVENT_STOPPED");
        lua_pushinteger(L, EVENT_ERROR);
        lua_setfield(L, -2, "EVENT_ERROR");
        lua_pop(L, 1);
        g_Window = native_window;
        AddStatusListener(&g_Listener);
        g_Initialized = true;
    }

    void FinalizeScript()
    {
        g_Initialized = false;
        RemoveStatusListener(&g_Listener);
        if (g_Callback)
        {
            dmScript::DestroyCallback(g_Callback);
            g_Callback = 0;
        }
        Finalize();
        g_Window = 0;
    }
}
