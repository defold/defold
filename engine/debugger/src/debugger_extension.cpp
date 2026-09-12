// Copyright 2020-2026 The Defold Foundation
// Licensed under the Defold License version 1.0. See https://www.defold.com/license

#if !defined(DM_RELEASE) && !defined(__EMSCRIPTEN__)
#include "debugger.h"
#include <dlib/array.h>
#include <dlib/log.h>
#include <dlib/dstrings.h>
#include <dmsdk/extension/extension.hpp>
#include <script_extension.h>

namespace dmDebugger
{
    struct ScriptState
    {
        lua_State* m_L;
        uint32_t   m_Id;
    };

    static HDebugger            g_Debugger;
    static dmArray<ScriptState> g_States;
    static uint32_t             g_NextStateId;

    static void                 AddState(const ScriptState& state)
    {
        char name[32];
        dmSnPrintf(name, sizeof(name), "Lua context %u", state.m_Id);
        AddLuaState(g_Debugger, state.m_L, name);
    }

    static bool Start(int port)
    {
        if (g_Debugger)
            return true;
        g_Debugger = New((uint16_t)port);
        if (!g_Debugger)
            return false;
        for (uint32_t i = 0; i < g_States.Size(); ++i)
            AddState(g_States[i]);
        dmLogInfo("Lua DAP debugger listening on 127.0.0.1:%u", GetPort(g_Debugger));
        return true;
    }

    static int LuaStart(lua_State* L)
    {
        lua_Number port = luaL_optnumber(L, 1, lua_tointeger(L, lua_upvalueindex(1)));
        if (!(port >= 0 && port <= 65535 && port == (int)port))
            return luaL_argerror(L, 1, "port must be an integer between 0 and 65535");
        if (!Start((int)port))
            return luaL_error(L, "Unable to start Lua DAP debugger on port %d", (int)port);
        lua_pushinteger(L, GetPort(g_Debugger));
        return 1;
    }

    static void ScriptError(dmScript::HContext context, lua_State* L)
    {
        (void)context;
        OnError(g_Debugger, L);
    }
    static dmScript::ScriptExtension g_ScriptExtension = {};

    static void                      ScriptFinalize(dmScript::HContext context)
    {
        lua_State* L = dmScript::GetLuaState(context);
        RemoveLuaState(g_Debugger, L);
        for (uint32_t i = 0; i < g_States.Size(); ++i)
            if (g_States[i].m_L == L)
            {
                g_States.EraseSwap(i);
                break;
            }
        if (g_States.Empty())
        {
            Delete(g_Debugger);
            g_Debugger = 0;
            g_NextStateId = 0;
        }
    }

    static dmExtension::Result Initialize(dmExtension::Params* params)
    {
        int port = ConfigFileGetInt(params->m_ConfigFile, "debugger.port", 8172);
        // Retain only the context until debugging is requested. Normal startup
        // does not open a listener, replace coroutine functions, or install hooks.
        ScriptState state = { params->m_L, ++g_NextStateId };
        if (g_States.Full())
            g_States.OffsetCapacity(4);
        g_States.Push(state);
        g_ScriptExtension.OnError = ScriptError;
        g_ScriptExtension.Finalize = ScriptFinalize;
        dmScript::RegisterScriptExtension(dmScript::GetScriptContext(params->m_L), &g_ScriptExtension);

        static const luaL_Reg methods[] = { { 0, 0 } };
        luaL_register(params->m_L, "debugger", methods);
        lua_pushinteger(params->m_L, port);
        lua_pushcclosure(params->m_L, LuaStart, 1);
        lua_setfield(params->m_L, -2, "start");
        lua_pop(params->m_L, 1);

        if (g_Debugger)
            AddState(state);
        else if (ConfigFileGetInt(params->m_ConfigFile, "debugger.enabled", 0))
        {
            if (port < 0 || port > 65535)
            {
                dmLogError("Invalid debugger.port: %d", port);
                return dmExtension::RESULT_INIT_ERROR;
            }
            if (!Start(port))
            {
                dmLogError("Unable to start Lua DAP debugger on port %d", port);
                return dmExtension::RESULT_INIT_ERROR;
            }
        }
        if (g_Debugger && g_States.Size() == 1 && ConfigFileGetInt(params->m_ConfigFile, "debugger.wait", 0))
            WaitForClient(g_Debugger);
        return dmExtension::RESULT_OK;
    }
    static dmExtension::Result UpdateExtension(dmExtension::Params* params)
    {
        (void)params;
        Update(g_Debugger);
        return dmExtension::RESULT_OK;
    }
} // namespace dmDebugger

DM_DECLARE_EXTENSION(LuaDebugger, "LuaDebugger", 0, 0, dmDebugger::Initialize, dmDebugger::UpdateExtension, 0, 0)
#endif
