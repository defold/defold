#include <string.h>
#include <dlib/log.h>
#include "../gamesys.h"
#include <script/script.h>

#if defined(__EMSCRIPTEN__)
#include <emscripten.h>
#endif

#include "script_window.h"

namespace dmGameSystem
{
    /*# Window API documentation
     *
     * Functions and constants to access the window, window event listeners 
     * and screen dimming.
     *
     * @name Window
     * @namespace window
     */

enum WindowEvent
{
    WINDOW_EVENT_FOCUS_LOST = 0,
    WINDOW_EVENT_FOCUS_GAINED = 1,
    WINDOW_EVENT_RESIZED = 2,
    WINDOW_EVENT_MOUSE_ENTER = 3,
    WINDOW_EVENT_MOUSE_LEAVE = 4,
    WINDOW_EVENT_POINTER_LOCK_GAINED = 5,
    WINDOW_EVENT_POINTER_LOCK_LOST = 6,
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

    DM_LUA_STACK_CHECK(L);

    lua_rawgeti(L, LUA_REGISTRYINDEX, listener->m_Callback);
    // Setup self
    lua_rawgeti(L, LUA_REGISTRYINDEX, listener->m_Self);
    lua_pushvalue(L, -1);

    dmScript::SetInstance(L);

    if (!dmScript::IsInstanceValid(L))
    {
        dmLogError("Could not run Window callback because the instance has been deleted.");
        lua_pop(L, 2);
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
}

/*# Sets a window event listener
 * Sets a window event listener.
 *
 * @name window.set_listener
 *
 * @param callback (function) A callback which receives info about window events. Can be nil.
 *
 * <ul>
 *     <li>self (object) The calling script</li>
 *     <li>event (number) The type of event. Can be one of these:</li>
 *     <ul>
 *         <li>window.WINDOW_EVENT_FOCUS_LOST</li>
 *         <li>window.WINDOW_EVENT_FOCUS_GAINED</li>
 *         <li>window.WINDOW_EVENT_RESIZED</li>
 *         <li>window.WINDOW_EVENT_MOUSE_ENTER</li>
 *         <li>window.WINDOW_EVENT_MOUSE_LEAVE</li>
 *         <li>window.WINDOW_EVENT_POINTER_LOCK_GAINED</li>
 *         <li>window.WINDOW_EVENT_POINTER_LOCK_LOST</li>
 *     </ul>
 *     <li>data (table) The callback value ''data'' is a table which currently holds these values</li>
 *     <ul>
 *         <li>width (number) The width of a resize event. nil otherwise.</li>
 *         <li>height (number) The height of a resize event. nil otherwise.</li>
 *     </ul>
 * </ul>
 *
 * @examples
 * <pre>
 * local function window_callback(self, event, data)
 *     if event == window.WINDOW_EVENT_FOCUS_LOST then
 *         print("window.WINDOW_EVENT_FOCUS_LOST")
 *     elseif event == window.WINDOW_EVENT_FOCUS_GAINED then
 *         print("window.WINDOW_EVENT_FOCUS_GAINED")
 *     elseif event == window.WINDOW_EVENT_RESIZED then
 *         print("Window resized: ", data.width, data.height)
 *     elseif event == window.WINDOW_EVENT_MOUSE_ENTER then
 *         print("The mouse entered the window!")
 *     elseif event == window.WINDOW_EVENT_MOUSE_LEAVE then
 *         print("The mouse left the window!")
 *     elseif event == window.WINDOW_EVENT_POINTER_LOCK_GAINED then
 *         print("The mouse pointer was locked!")
 *     elseif event == window.WINDOW_EVENT_POINTER_LOCK_LOST then
 *         print("The mouse pointer was unlocked!")
 *     end
 * end
 *
 * function init(self)
 *     window.set_listener(window_callback)
 * end
 * </pre>
 */
 /*# window focus lost
  * Window focus is lost
  * @name window.WINDOW_EVENT_FOCUS_LOST
  * @variable
  */
 /*# window focus gained
  * Window focus has been gained
  * @name window.WINDOW_EVENT_FOCUS_GAINED
  * @variable
  */
 /*# window was resized
  * The window has been resized. The event data table is of the form { width: 800, height: 600 }
  * @name window.WINDOW_EVENT_RESIZED
  * @variable
  */
 /*# mouse cursor entered window
  * The mouse cursor has entered the window
  * @name window.WINDOW_EVENT_MOUSE_ENTER
  * @variable
  */
 /*# mouse cursor left the window
  * The mouse cursor has left the window
  * @name window.WINDOW_EVENT_MOUSE_LEAVE
  * @variable
  */
 /*# mouse cursor lock was obtained
  * The mouse cursor lock was successfully obtained [icon: HTML5]
  * @name window.WINDOW_EVENT_POINTER_LOCK_GAINED
  * @variable
  */
 /*# mouse cursor lock was lost
  * The mouse cursor lock was lost. This can happen if the user
  * presses the Escape key, or if the tab loses focus [icon: HTML5]
  * @name window.WINDOW_EVENT_POINTER_LOCK_LOST
  * @variable
  */


static int SetListener(lua_State* L)
{
    DM_LUA_STACK_CHECK(L);

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

/*# Set the mode for screen dimming
 * The dimming mode specifies whether or not a mobile device should dim the screen after a period without user interaction. The dimming mode will only affect the mobile device while the game is in focus on the device, but not when the game is running in the background.
 * @note Supported platforms: Android, iOS
 * @name window.set_dim_mode
 * @param mode (constant) The mode for screen dimming
 * <ul>
 *     <li><code>window.DIMMING_ON</code></li>
 *     <li><code>window.DIMMING_OFF</code></li>
 * </ul>
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
static int SetDimMode(lua_State* L)
{
    DM_LUA_STACK_CHECK(L);

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
        return luaL_error(L, "The dim mode specified is not supported.");
    }

    return 0;
}

/*# Get the mode for screen dimming
 * The dimming mode specifies whether or not a mobile device should dim the screen after a period without user interaction.
 * @note Supported platforms: Android, iOS
 * @name window.get_dim_mode
 * @return mode (constant) The mode for screen dimming
 * <ul>
 *     <li><code>window.DIMMING_UNKNOWN</code></li>
 *     <li><code>window.DIMMING_ON</code></li>
 *     <li><code>window.DIMMING_OFF</code></li>
 * </ul>
 */
static int GetDimMode(lua_State* L)
{
    int top = lua_gettop(L);

    DimMode mode = dmGameSystem::PlatformGetDimMode();
    lua_pushnumber(L, (lua_Number) mode);

    assert(top + 1 == lua_gettop(L));
    return 1;
}

/** Shows the mouse cursor
 * Shows the mouse cursor
 * @note Supported platforms: macOS, Windows, Linux, HTML5
 * @name window.show_mouse_cursor
 */
static int ShowMouseCursor(lua_State* L)
{
    glfwEnable(GLFW_MOUSE_CURSOR);
    return 1;
}

/** Hides the mouse cursor
 * Hides the mouse cursor. [icon: macOS, Windows, Linux, HTML5]
 * After hiding the cursor, the subsequent cursor positions are not bound to the screen coordinates.
 * Also note that no mouse enter/leave events will be sent when the mouse is hidden.
 *
 * @note [icon: HTML5] it also requires a user interaction (e.g. a click) to be allowed to lock the pointer. For this reason
 * there is the project setting html5.pointer_lock, which enables locking the pointer on the first clock on the canvas.
 * This will generate a window.WINDOW_EVENT_POINTER_LOCK_GAINED event.
 * @note [icon: HTML5] The user can press the Escape key at any time to unlock the pointer, which will generate a window.WINDOW_EVENT_POINTER_LOCK_LOST event.
 * You can read more here: https://developer.mozilla.org/en-US/docs/Web/API/Pointer_Lock_API
 * @name window.hide_mouse_cursor
 */
static int HideMouseCursor(lua_State* L)
{
    glfwDisable(GLFW_MOUSE_CURSOR);
    return 1;
}

static const luaL_reg Module_methods[] =
{
    {"set_listener", SetListener},
    {"set_dim_mode", SetDimMode},
    {"get_dim_mode", GetDimMode},
    {"show_mouse_cursor", ShowMouseCursor},
    {"hide_mouse_cursor", HideMouseCursor},
    {0, 0}
};

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
    SETCONSTANT(WINDOW_EVENT_MOUSE_ENTER)
    SETCONSTANT(WINDOW_EVENT_MOUSE_LEAVE)
    SETCONSTANT(WINDOW_EVENT_POINTER_LOCK_GAINED)
    SETCONSTANT(WINDOW_EVENT_POINTER_LOCK_LOST)

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

void ScriptWindowOnWindowCursorEnter(bool enter)
{
    CallbackInfo cbinfo;
    cbinfo.m_Info = &g_Window;
    cbinfo.m_Event = enter ? WINDOW_EVENT_MOUSE_ENTER : WINDOW_EVENT_MOUSE_LEAVE;
    RunCallback(&cbinfo);
}

void ScriptWindowOnWindowCursorLockChanged(bool locked)
{
    CallbackInfo cbinfo;
    cbinfo.m_Info = &g_Window;
    cbinfo.m_Event = locked ? WINDOW_EVENT_POINTER_LOCK_GAINED : WINDOW_EVENT_POINTER_LOCK_LOST;
    RunCallback(&cbinfo);
}

} // namespace

