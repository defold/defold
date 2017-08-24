#include <string.h>
#include <dlib/log.h>
#include "../gamesys.h"
#include <script/script.h>

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
     */

enum WindowEvent
{
    WINDOW_EVENT_FOCUS_LOST = 0,
    WINDOW_EVENT_FOCUS_GAINED = 1,
    WINDOW_EVENT_RESIZED = 2,
};


struct LuaListener
{
    LuaListener() : m_L(0), m_Callback(LUA_NOREF), m_Self(LUA_NOREF) {}
    lua_State* m_L;
    int        m_Callback;
    int        m_Self;
};

struct WindowInfo
{
    LuaListener m_Listener;
};

struct CallbackInfo
{
    WindowInfo*     m_Info;
    WindowEvent     m_Event;
    int             m_Size[2]; // valid if type == WINDOW_EVENT_RESIZED

    CallbackInfo() { memset(this, 0, sizeof(*this)); }
};

WindowInfo g_Window;


static void ClearListener(LuaListener* listener)
{
    if( listener->m_Callback != LUA_NOREF )
        dmScript::Unref(listener->m_L, LUA_REGISTRYINDEX, listener->m_Callback);
    if( listener->m_Self != LUA_NOREF )
        dmScript::Unref(listener->m_L, LUA_REGISTRYINDEX, listener->m_Self);
    listener->m_L = 0;
    listener->m_Callback = LUA_NOREF;
    listener->m_Self = LUA_NOREF;
}


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
    LuaListener* listener = &cbinfo->m_Info->m_Listener;
    if (listener->m_Callback == LUA_NOREF)
    {
        return;
    }

    lua_State* L = listener->m_L;
    int top = lua_gettop(L);

    lua_rawgeti(L, LUA_REGISTRYINDEX, listener->m_Callback);
    // Setup self
    lua_rawgeti(L, LUA_REGISTRYINDEX, listener->m_Self);
    lua_pushvalue(L, -1);

    dmScript::SetInstance(L);

    if (!dmScript::IsInstanceValid(L))
    {
        dmLogError("Could not run Window callback because the instance has been deleted.");
        lua_pop(L, 2);
        assert(top == lua_gettop(L));
        return;
    }

    lua_pushnumber(L, (lua_Number)cbinfo->m_Event);

    lua_newtable(L);

    PushNumberOrNil(L, "width", cbinfo->m_Event == WINDOW_EVENT_RESIZED, cbinfo->m_Size[0]);
    PushNumberOrNil(L, "height", cbinfo->m_Event == WINDOW_EVENT_RESIZED, cbinfo->m_Size[1]);

    int ret = lua_pcall(L, 3, 0, 0);
    if (ret != 0) {
        dmLogError("Error running Window callback: %s", lua_tostring(L,-1));
        lua_pop(L, 1);
    }
    assert(top == lua_gettop(L));
}

/*# sets a window event listener
 * Sets a window event listener.
 *
 * @name window.set_listener
 *
 * @param callback [type:function(self, event, data)] A callback which receives info about window events. Pass an empty function if you no longer wish to receive callbacks.
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
    WindowInfo* window_info = &g_Window;
    luaL_checktype(L, 1, LUA_TFUNCTION);
    lua_pushvalue(L, 1);
    int cb = dmScript::Ref(L, LUA_REGISTRYINDEX);

    ClearListener(&window_info->m_Listener);

    window_info->m_Listener.m_L = dmScript::GetMainThread(L);
    window_info->m_Listener.m_Callback = cb;

    dmScript::GetInstance(L);
    window_info->m_Listener.m_Self = dmScript::Ref(L, LUA_REGISTRYINDEX);
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
    int top = lua_gettop(L);

    DimMode mode = dmGameSystem::PlatformGetDimMode();
    lua_pushnumber(L, (lua_Number) mode);

    assert(top + 1 == lua_gettop(L));
    return 1;
}

static const luaL_reg Module_methods[] =
{
    {"set_listener", SetListener},
    {"set_dim_mode", SetDimMode},
    {"get_dim_mode", GetDimMode},
    {0, 0}
};

/*# focus lost window event
 *
 * This event is sent to a window event listener when the game window or app screen has lost focus.
 *
 * @name window.WINDOW_EVENT_FOCUS_LOST
 * @variable
 */

/*# focus gained window event
 *
 * This event is sent to a window event listener when the game window or app screen has 
 * gained focus.
 * This event is also sent at game startup and the engine gives focus to the game.
 *
 * @name window.WINDOW_EVENT_FOCUS_GAINED
 * @variable
 */

/*# resized window event
 *
 * This event is sent to a window event listener when the game window or app screen is resized.
 * The new size is passed along in the data field to the event listener.
 *
 * @name window.WINDOW_EVENT_RESIZED
 * @variable
 */

/*# dimming mode on
  * Dimming mode is used to control whether or not a mobile device should dim the screen after a period without user interaction.
  * @name window.DIMMING_ON
  * @variable
  */

/*# dimming mode off
  * Dimming mode is used to control whether or not a mobile device should dim the screen after a period without user interaction.
  * @name window.DIMMING_OFF
  * @variable
  */

/*# dimming mode unknown
  * Dimming mode is used to control whether or not a mobile device should dim the screen after a period without user interaction.
  * This mode indicates that the dim mode can't be determined, or that the platform doesn't support dimming.
  * @name window.DIMMING_UNKNOWN
  * @variable
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
}

void ScriptWindowFinalize(const ScriptLibContext& context)
{
    ClearListener(&g_Window.m_Listener);
}

void ScriptWindowOnWindowFocus(bool focus)
{
    CallbackInfo cbinfo;
    cbinfo.m_Info = &g_Window;
    cbinfo.m_Event = focus ? WINDOW_EVENT_FOCUS_GAINED : WINDOW_EVENT_FOCUS_LOST;
    RunCallback(&cbinfo);
}

void ScriptWindowOnWindowResized(int width, int height)
{
    CallbackInfo cbinfo;
    cbinfo.m_Info = &g_Window;
    cbinfo.m_Event = WINDOW_EVENT_RESIZED;
    cbinfo.m_Size[0] = width;
    cbinfo.m_Size[1] = height;
    RunCallback(&cbinfo);
}


} // namespace

