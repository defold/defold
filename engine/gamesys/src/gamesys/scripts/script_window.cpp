// Copyright 2020-2025 The Defold Foundation
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

#include <string.h>
#include <dlib/log.h>
#include "../gamesys.h"
#include <script/script.h>
#include <hid/hid.h>
#include <platform/platform_window.h>

#include "script_window.h"

namespace dmGameSystem
{
    /*# Window API documentation
     *
     * Functions and constants to access the window, window event listeners
     * and screen dimming.
     *
     * @document
     * @name Window
     * @namespace window
     * @language Lua
     */

enum WindowEvent
{
    WINDOW_EVENT_FOCUS_LOST = 0,
    WINDOW_EVENT_FOCUS_GAINED = 1,
    WINDOW_EVENT_RESIZED = 2,
    WINDOW_EVENT_ICONFIED = 3,
    WINDOW_EVENT_DEICONIFIED = 4,
};


struct WindowInfo
{
    dmHID::HContext m_HidContext;
    dmPlatform::HWindow m_Window;
    dmScript::LuaCallbackInfo* m_Callback;
    int m_Width;
    int m_Height;
};

struct CallbackInfo
{
    WindowInfo*     m_Info;
    WindowEvent     m_Event;
    int             m_Size[2]; // valid if type == WINDOW_EVENT_RESIZED

    CallbackInfo() { memset(this, 0, sizeof(*this)); }
};

WindowInfo g_Window = {};

static void PushNumberOrNil(lua_State* L, const char* name, bool expression, lua_Number number)
{
    lua_pushstring(L, name);
    if( expression ) {
        lua_pushnumber(L, number);
    } else {
        lua_pushnil(L);
    }
    lua_rawset(L, -3);
}

static void RunCallback(CallbackInfo* cbinfo)
{
    if (!cbinfo->m_Info->m_Callback) // no callback set
        return;

    dmScript::LuaCallbackInfo* callback = cbinfo->m_Info->m_Callback;
    lua_State* L = dmScript::GetCallbackLuaContext(callback);
    DM_LUA_STACK_CHECK(L, 0);

    if (!dmScript::SetupCallback(callback))
    {
        return;
    }

    lua_pushnumber(L, (lua_Number)cbinfo->m_Event);

    lua_newtable(L);

    PushNumberOrNil(L, "width", cbinfo->m_Event == WINDOW_EVENT_RESIZED, cbinfo->m_Size[0]);
    PushNumberOrNil(L, "height", cbinfo->m_Event == WINDOW_EVENT_RESIZED, cbinfo->m_Size[1]);

    dmScript::PCall(L, 3, 0); // self + # user arguments

    dmScript::TeardownCallback(callback);
}

/*# sets a window event listener
 * Sets a window event listener. Only one window event listener can be set at a time.
 *
 * @name window.set_listener
 *
 * @param callback [type:function(self, event, data)|nil] A callback which receives info about window events. Pass an empty function or `nil` if you no longer wish to receive callbacks.
 *
 * `self`
 * : [type:object] The calling script
 *
 * `event`
 * : [type:constant] The type of event. Can be one of these:
 *
 * - `window.WINDOW_EVENT_FOCUS_LOST`
 * - `window.WINDOW_EVENT_FOCUS_GAINED`
 * - `window.WINDOW_EVENT_RESIZED`
 * - `window.WINDOW_EVENT_ICONIFIED`
 * - `window.WINDOW_EVENT_DEICONIFIED`
 *
 * `data`
 * : [type:table] The callback value `data` is a table which currently holds these values
 *
 * - [type:number] `width`: The width of a resize event. nil otherwise.
 * - [type:number] `height`: The height of a resize event. nil otherwise.
 *
 * @examples
 *
 * ```lua
 * function window_callback(self, event, data)
 *     if event == window.WINDOW_EVENT_FOCUS_LOST then
 *         print("window.WINDOW_EVENT_FOCUS_LOST")
 *     elseif event == window.WINDOW_EVENT_FOCUS_GAINED then
 *         print("window.WINDOW_EVENT_FOCUS_GAINED")
 *     elseif event == window.WINDOW_EVENT_ICONFIED then
 *         print("window.WINDOW_EVENT_ICONFIED")
 *     elseif event == window.WINDOW_EVENT_DEICONIFIED then
 *         print("window.WINDOW_EVENT_DEICONIFIED")
 *     elseif event == window.WINDOW_EVENT_RESIZED then
 *         print("Window resized: ", data.width, data.height)
 *     end
 * end
 *
 * function init(self)
 *     window.set_listener(window_callback)
 * end
 * ```
 */
static int SetListener(lua_State* L)
{
    luaL_checkany(L, 1);

    WindowInfo* window_info = &g_Window;

    if (lua_isnil(L, 1))
    {
        if (window_info->m_Callback)
            dmScript::DestroyCallback(window_info->m_Callback);
        window_info->m_Callback = 0;
        return 0;
    }

    if (window_info->m_Callback)
        dmScript::DestroyCallback(window_info->m_Callback);
    window_info->m_Callback = dmScript::CreateCallback(L, 1);

    if (!dmScript::IsCallbackValid(window_info->m_Callback))
        return luaL_error(L, "Failed to create callback");

    return 0;
}

/*# set the locking state for current mouse cursor
 *
 * Set the locking state for current mouse cursor on a PC platform.
 *
 * This function locks or unlocks the mouse cursor to the center point of the window. While the cursor is locked,
 * mouse position updates will still be sent to the scripts as usual.
 *
 * @name window.set_mouse_lock
 * @param flag [type:boolean] The lock state for the mouse cursor
 */
static int SetMouseLock(lua_State* L)
{
    DM_LUA_STACK_CHECK(L, 0);

    bool flag = dmScript::CheckBoolean(L, 1);

    // Hiding the cursor is the same thing as locking it currently
    if (flag)
    {
        dmHID::HideMouseCursor(g_Window.m_HidContext);
    }
    else
    {
        dmHID::ShowMouseCursor(g_Window.m_HidContext);
    }

    return 0;
}

/*# set the mode for screen dimming
 *
 * [icon:ios] [icon:android] Sets the dimming mode on a mobile device.
 *
 * The dimming mode specifies whether or not a mobile device should dim the screen after a period without user interaction. The dimming mode will only affect the mobile device while the game is in focus on the device, but not when the game is running in the background.
 *
 * This function has no effect on platforms that does not support dimming.
 *
 * @name window.set_dim_mode
 * @param mode [type:constant] The mode for screen dimming
 *
 * - `window.DIMMING_ON`
 * - `window.DIMMING_OFF`
 */

static int SetDimMode(lua_State* L)
{
    int top = lua_gettop(L);

    DimMode mode = (DimMode) luaL_checkint(L, 1);
    if (mode == DIMMING_ON)
    {
        dmGameSystem::PlatformSetDimMode(DIMMING_ON);
    }
    else if (mode == DIMMING_OFF)
    {
        dmGameSystem::PlatformSetDimMode(DIMMING_OFF);
    }
    else
    {
        assert(top == lua_gettop(L));
        return luaL_error(L, "The dim mode specified is not supported.");
    }

    assert(top == lua_gettop(L));
    return 0;
}

/*# get the mode for screen dimming
 *
 * [icon:ios] [icon:android] Returns the current dimming mode set on a mobile device.
 *
 * The dimming mode specifies whether or not a mobile device should dim the screen after a period without user interaction.
 *
 * On platforms that does not support dimming, `window.DIMMING_UNKNOWN` is always returned.
 *
 * @name window.get_dim_mode
 * @return mode [type:constant] The mode for screen dimming
 *
 * - `window.DIMMING_UNKNOWN`
 * - `window.DIMMING_ON`
 * - `window.DIMMING_OFF`
 */
static int GetDimMode(lua_State* L)
{
    DM_LUA_STACK_CHECK(L, 1);

    DimMode mode = dmGameSystem::PlatformGetDimMode();
    lua_pushnumber(L, (lua_Number) mode);

    return 1;
}

/*# get the window size
 *
 * This returns the current window size (width and height).
 *
 * @name window.get_size
 * @return width [type:number] The window width
 * @return height [type:number] The window height
 */
static int GetSize(lua_State* L)
{
    DM_LUA_STACK_CHECK(L, 2);

    lua_pushnumber(L, g_Window.m_Width);
    lua_pushnumber(L, g_Window.m_Height);

    return 2;
}


/*# get the cursor lock state
 *
 * This returns the current lock state of the mouse cursor
 *
 * @name window.get_mouse_lock
 * @return state [type:boolean] The lock state
 */
static int GetMouseLock(lua_State* L)
{
    DM_LUA_STACK_CHECK(L, 1);

    bool cursor_visible = dmHID::GetCursorVisible(g_Window.m_HidContext);
    // If cursor is visible, it is not locked
    lua_pushboolean(L, !cursor_visible);

    return 1;
}

/*# get the display scale
 *
 * This returns the content scale of the current display.
 *
 * @name window.get_display_scale
 * @return scale [type:number] The display scale
 */
static int GetDisplayScale(lua_State* L)
{
    DM_LUA_STACK_CHECK(L, 1);

    float scale = dmPlatform::GetDisplayScaleFactor(g_Window.m_Window);
    lua_pushnumber(L, scale);

    return 1;
}

/*# set the title of the window
 *
 * Sets the window title. Works on desktop platforms.
 *
 * @name window.set_title
 * @param title [type:string] The title, encoded as UTF-8
 */
static int SetTitle(lua_State* L)
{
    DM_LUA_STACK_CHECK(L, 0);

    const char* title = luaL_checkstring(L, 1);
    dmPlatform::SetWindowTitle(g_Window.m_Window, title);

    return 0;
}

/*# set the size of the window
 *
 * Sets the window size. Works on desktop platforms only.
 *
 * @name window.set_size
 * @param width [type:number] Width of window
 * @param height [type:number] Height of window
 */
static int SetSize(lua_State* L)
{
    DM_LUA_STACK_CHECK(L, 0);

    int width = luaL_checkinteger(L, 1);
    int height = luaL_checkinteger(L, 2);
    dmPlatform::SetWindowSize(g_Window.m_Window, width, height);

    return 0;
}


/*# set the position of the window
 *
 * Sets the window position.
 *
 * @name window.set_position
 * @param x [type:number] Horizontal position of window
 * @param y [type:number] Vertical position of window
 */
static int SetPosition(lua_State* L)
{
    DM_LUA_STACK_CHECK(L, 0);

    int x = luaL_checkinteger(L, 1);
    int y = luaL_checkinteger(L, 2);
    dmPlatform::SetWindowPosition(g_Window.m_Window, x, y);

    return 0;
}

static const luaL_reg Module_methods[] =
{
    {"set_listener",      SetListener},
    {"set_dim_mode",      SetDimMode},
    {"set_mouse_lock",    SetMouseLock},
    {"set_title",         SetTitle},
    {"get_dim_mode",      GetDimMode},
    {"get_size",          GetSize},
    {"set_size",          SetSize},
    {"set_position",      SetPosition},
    {"get_display_scale", GetDisplayScale},
    {"get_mouse_lock",    GetMouseLock},
    {0, 0}
};

/*# focus lost window event
 *
 * This event is sent to a window event listener when the game window or app screen has lost focus.
 *
 * @name window.WINDOW_EVENT_FOCUS_LOST
 * @constant
 */

/*# focus gained window event
 *
 * This event is sent to a window event listener when the game window or app screen has
 * gained focus.
 * This event is also sent at game startup and the engine gives focus to the game.
 *
 * @name window.WINDOW_EVENT_FOCUS_GAINED
 * @constant
 */

/*# resized window event
 *
 * This event is sent to a window event listener when the game window or app screen is resized.
 * The new size is passed along in the data field to the event listener.
 *
 * @name window.WINDOW_EVENT_RESIZED
 * @constant
 */

/*# iconify window event
 *
 * [icon:osx] [icon:windows] [icon:linux] This event is sent to a window event listener when the game window or app screen is
 * iconified (reduced to an application icon in a toolbar, application tray or similar).
 *
 * @name window.WINDOW_EVENT_ICONFIED
 * @constant
 */

/*# deiconified window event
 *
 * [icon:osx] [icon:windows] [icon:linux] This event is sent to a window event listener when the game window or app screen is
 * restored after being iconified.
 *
 * @name window.WINDOW_EVENT_DEICONIFIED
 * @constant
 */

/*# dimming mode on
  * Dimming mode is used to control whether or not a mobile device should dim the screen after a period without user interaction.
  * @name window.DIMMING_ON
  * @constant
  */

/*# dimming mode off
  * Dimming mode is used to control whether or not a mobile device should dim the screen after a period without user interaction.
  * @name window.DIMMING_OFF
  * @constant
  */

/*# dimming mode unknown
  * Dimming mode is used to control whether or not a mobile device should dim the screen after a period without user interaction.
  * This mode indicates that the dim mode can't be determined, or that the platform doesn't support dimming.
  * @name window.DIMMING_UNKNOWN
  * @constant
  */

static void LuaInit(lua_State* L)
{
    int top = lua_gettop(L);
    luaL_register(L, "window", Module_methods);

#define SETCONSTANT(name) \
        lua_pushnumber(L, (lua_Number) name); \
        lua_setfield(L, -2, #name);\

    SETCONSTANT(WINDOW_EVENT_FOCUS_LOST)
    SETCONSTANT(WINDOW_EVENT_FOCUS_GAINED)
    SETCONSTANT(WINDOW_EVENT_RESIZED)
    SETCONSTANT(WINDOW_EVENT_ICONFIED)
    SETCONSTANT(WINDOW_EVENT_DEICONIFIED)

    SETCONSTANT(DIMMING_UNKNOWN)
    SETCONSTANT(DIMMING_ON)
    SETCONSTANT(DIMMING_OFF)

#undef SETCONSTANT

    lua_pop(L, 1);
    assert(top == lua_gettop(L));
}

void ScriptWindowRegister(const ScriptLibContext& context)
{
    LuaInit(context.m_LuaState);
    g_Window.m_HidContext = context.m_HidContext;
    g_Window.m_Window = context.m_Window;
}

void ScriptWindowFinalize(const ScriptLibContext& context)
{
    if (g_Window.m_Callback)
        dmScript::DestroyCallback(g_Window.m_Callback);
    g_Window.m_Callback = 0;
    g_Window.m_HidContext = 0;
}

void ScriptWindowOnWindowFocus(bool focus)
{
    CallbackInfo cbinfo;
    cbinfo.m_Info = &g_Window;
    cbinfo.m_Event = focus ? WINDOW_EVENT_FOCUS_GAINED : WINDOW_EVENT_FOCUS_LOST;
    RunCallback(&cbinfo);
}

void ScriptWindowOnWindowIconify(bool iconify)
{
    CallbackInfo cbinfo;
    cbinfo.m_Info = &g_Window;
    cbinfo.m_Event = iconify ? WINDOW_EVENT_ICONFIED : WINDOW_EVENT_DEICONIFIED;
    RunCallback(&cbinfo);
}

void ScriptWindowOnWindowCreated(int width, int height)
{
    g_Window.m_Width = width;
    g_Window.m_Height = height;
}

void ScriptWindowOnWindowResized(int width, int height)
{
    g_Window.m_Width = width;
    g_Window.m_Height = height;

    CallbackInfo cbinfo;
    cbinfo.m_Info = &g_Window;
    cbinfo.m_Event = WINDOW_EVENT_RESIZED;
    cbinfo.m_Size[0] = width;
    cbinfo.m_Size[1] = height;
    RunCallback(&cbinfo);
}


} // namespace
