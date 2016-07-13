#include "window_private.h"
#include <dlib/log.h>
#include <extension/extension.h>

namespace dmWindow
{

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
    Event           m_Event;
    int             m_Size[2]; // valid if type == EVENT_RESIZED

    CallbackInfo() { memset(this, 0, sizeof(*this)); }
};

WindowInfo g_Window;


static void ClearListener(LuaListener* listener)
{
    if( listener->m_Callback != LUA_NOREF )
        luaL_unref(listener->m_L, LUA_REGISTRYINDEX, listener->m_Callback);
    if( listener->m_Self != LUA_NOREF )
        luaL_unref(listener->m_L, LUA_REGISTRYINDEX, listener->m_Self);
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
        dmLogError("No callback set");
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

    PushNumberOrNil(L, "width", cbinfo->m_Event == EVENT_RESIZED, cbinfo->m_Size[0]);
    PushNumberOrNil(L, "height", cbinfo->m_Event == EVENT_RESIZED, cbinfo->m_Size[1]);


    int ret = lua_pcall(L, 5, 0, 0);
    if (ret != 0) {
        dmLogError("Error running Window callback: %s", lua_tostring(L,-1));
        lua_pop(L, 1);
    }
    assert(top == lua_gettop(L));
}


int SetListener(lua_State* L)
{
    WindowInfo* window_info = &g_Window;
    luaL_checktype(L, 1, LUA_TFUNCTION);
    lua_pushvalue(L, 1);
    int cb = luaL_ref(L, LUA_REGISTRYINDEX);

    ClearListener(&window_info->m_Listener);

    window_info->m_Listener.m_L = dmScript::GetMainThread(L);
    window_info->m_Listener.m_Callback = cb;

    dmScript::GetInstance(L);
    window_info->m_Listener.m_Self = luaL_ref(L, LUA_REGISTRYINDEX);
    return 0;
}

static const luaL_reg Module_methods[] =
{
    {"set_listener", SetListener},
    {0, 0}
};

void LuaInit(lua_State* L)
{
    int top = lua_gettop(L);
    luaL_register(L, "window", Module_methods);

#define SETCONSTANT(name) \
        lua_pushnumber(L, (lua_Number) name); \
        lua_setfield(L, -2, #name);\

    SETCONSTANT(EVENT_FOCUS_LOST)
    SETCONSTANT(EVENT_FOCUS_GAINED)
    SETCONSTANT(EVENT_RESIZED)

#undef SETCONSTANT

    lua_pop(L, 1);
    assert(top == lua_gettop(L));
}


} // namespace


static dmExtension::Result AppInitialize(dmExtension::AppParams* params)
{
    dmLogWarning("MAWE: HELLO WINDOW WORLD!");
    return dmExtension::RESULT_OK;
}

static dmExtension::Result Initialize(dmExtension::Params* params)
{
    dmLogWarning("MAWE: HELLO WINDOW WORLD!");
    dmWindow::LuaInit(params->m_L);

    return dmExtension::RESULT_OK;
}

static dmExtension::Result Finalize(dmExtension::Params* params)
{
    return dmExtension::RESULT_OK;
}

static dmExtension::Result AppFinalize(dmExtension::AppParams* params)
{
    return dmExtension::RESULT_OK;
}

static void OnEvent(dmExtension::Params* params, const dmExtension::Event* event)
{
    if( event->m_Event == dmExtension::EVENT_ID_DEACTIVATEAPP )
    {
        dmWindow::CallbackInfo cbinfo;
        cbinfo.m_Info = &dmWindow::g_Window;
        cbinfo.m_Event = dmWindow::EVENT_FOCUS_LOST;
        RunCallback(&cbinfo);
    }
    else if( event->m_Event == dmExtension::EVENT_ID_ACTIVATEAPP )
    {
        dmWindow::CallbackInfo cbinfo;
        cbinfo.m_Info = &dmWindow::g_Window;
        cbinfo.m_Event = dmWindow::EVENT_FOCUS_GAINED;
        RunCallback(&cbinfo);
    }
}

DM_DECLARE_EXTENSION(WindowExt, "Window", AppInitialize, AppFinalize, Initialize, 0, OnEvent, Finalize);


