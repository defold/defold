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

#if !defined(DM_RELEASE) && !defined(__EMSCRIPTEN__)
#include "debugger.h"
#include <dlib/array.h>
#include <dlib/log.h>
#include <dlib/dstrings.h>
#include <dmsdk/extension/extension.hpp>
#include <script_extension.h>
#include <stdio.h>

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

    static bool                 ResolveUserdataTable(lua_State* L, int index)
    {
        int top = lua_gettop(L);
        if (index < 0)
            index += top + 1;
        if (!lua_getmetatable(L, index))
            return false;
        // Match the registered engine metatables, as MobDebug's edn.lua does.
        // Other userdata and application metamethods are not inspected.
        const char* types[] = { "GOScriptInstance", "GuiScriptInstance", "RenderScriptInstance" };
        bool        instance = false;
        for (uint32_t i = 0; i < sizeof(types) / sizeof(types[0]); ++i)
        {
            lua_pushstring(L, types[i]);
            lua_rawget(L, LUA_REGISTRYINDEX);
            instance = lua_rawequal(L, -1, top + 1) != 0;
            lua_pop(L, 1);
            if (instance)
                break;
        }
        if (instance)
        {
            lua_pushliteral(L, "__get_instance_data_table_ref");
            lua_rawget(L, top + 1);
            if (lua_iscfunction(L, -1))
            {
                // Keep the native getter's call off the inspected thread, which
                // may be yielded. The temporary thread is pinned on L's stack.
                lua_State* inspection = lua_newthread(L);
                lua_pushvalue(L, top + 2);
                lua_pushvalue(L, index);
                lua_xmove(L, inspection, 2);
                if (lua_pcall(inspection, 1, 1, 0) == 0 && lua_type(inspection, -1) == LUA_TNUMBER)
                {
                    int reference = (int)lua_tointeger(inspection, -1);
                    lua_rawgeti(L, LUA_REGISTRYINDEX, reference);
                    if (lua_istable(L, -1))
                    {
                        lua_replace(L, top + 1);
                        lua_settop(L, top + 1);
                        return true;
                    }
                }
            }
        }
        lua_settop(L, top);
        return false;
    }

    static void AddState(const ScriptState& state)
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
        SetUserdataTableResolver(g_Debugger, ResolveUserdataTable);
        for (uint32_t i = 0; i < g_States.Size(); ++i)
            AddState(g_States[i]);
        dmLogInfo("Lua DAP debugger listening on 127.0.0.1:%u", GetPort(g_Debugger));
        // Publish an ephemeral port to piped clients before startup waits.
        fflush(stdout);
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
            }
            else if (!Start(port))
            {
                dmLogError("Unable to start Lua DAP debugger on port %d", port);
            }
            // The Lua module and lifecycle callbacks are initialized even when
            // the listener cannot start. Keep UpdateExtension enabled so a later
            // debugger.start() can retry and service the new listener.
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
