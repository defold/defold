// Copyright 2020-2026 The Defold Foundation
// Licensed under the Defold License version 1.0. See https://www.defold.com/license

#if !defined(DM_RELEASE) && !defined(__EMSCRIPTEN__)
#include "debugger.h"
#include <dlib/log.h>
#include <dlib/dstrings.h>
#include <dmsdk/extension/extension.hpp>
#include <script_extension.h>

namespace dmDebugger
{
    static HDebugger g_Debugger;
    static int       g_StateCount;

    static void      ScriptError(dmScript::HContext context, lua_State* L)
    {
        (void)context;
        OnError(g_Debugger, L);
    }
    static dmScript::ScriptExtension g_ScriptExtension = {};

    static void                      ScriptFinalize(dmScript::HContext context)
    {
        RemoveLuaState(g_Debugger, dmScript::GetLuaState(context));
        if (--g_StateCount == 0)
        {
            Delete(g_Debugger);
            g_Debugger = 0;
        }
    }

    static dmExtension::Result Initialize(dmExtension::Params* params)
    {
        if (!ConfigFileGetInt(params->m_ConfigFile, "debugger.enabled", 0))
            return dmExtension::RESULT_OK;
        if (!g_Debugger)
        {
            int port = ConfigFileGetInt(params->m_ConfigFile, "debugger.port", 8172);
            if (port < 0 || port > 65535)
            {
                dmLogError("Invalid debugger.port: %d", port);
                return dmExtension::RESULT_INIT_ERROR;
            }
            g_Debugger = New((uint16_t)port);
            if (!g_Debugger)
            {
                dmLogError("Unable to start Lua DAP debugger on port %d", port);
                return dmExtension::RESULT_INIT_ERROR;
            }
            dmLogInfo("Lua DAP debugger listening on 127.0.0.1:%u", GetPort(g_Debugger));
        }
        char name[32];
        dmSnPrintf(name, sizeof(name), "Lua context %d", ++g_StateCount);
        AddLuaState(g_Debugger, params->m_L, name);
        g_ScriptExtension.OnError = ScriptError;
        g_ScriptExtension.Finalize = ScriptFinalize;
        dmScript::RegisterScriptExtension(dmScript::GetScriptContext(params->m_L), &g_ScriptExtension);
        if (g_StateCount == 1 && ConfigFileGetInt(params->m_ConfigFile, "debugger.wait", 0))
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
