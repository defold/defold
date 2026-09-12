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

// Uses the actual engine extension and dmScript::PCall. The DAP client supplies
// every debugger request; this host only implements the engine lifecycle.
#include <script/script.h>
#include <extension/extension.hpp>
#include <dlib/socket.h>
#include <dlib/time.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern "C" void            LuaDebugger();
static dmExtension::Params g_Params;

static void                LogListener(LogSeverity, const char*, const char* message)
{
    const char* prefix = "Lua DAP debugger listening on 127.0.0.1:";
    const char* port = strstr(message, prefix);
    if (port)
    {
        printf("PORT %u\n", (unsigned int)strtoul(port + strlen(prefix), 0, 10));
        fflush(stdout);
    }
}

static void Check(bool ok, const char* message)
{
    if (!ok)
    {
        fprintf(stderr, "%s\n", message);
        exit(20);
    }
}
static int Pump(lua_State*)
{
    dmExtension::Update(&g_Params);
    return 0;
}

static dmScript::HContext Create(dmConfigFile::HConfig config, const char* prelude = 0)
{
    dmScript::ContextParams params = {};
    params.m_ConfigFile = config;
    dmScript::HContext context = dmScript::NewContext(params);
    dmScript::Initialize(context);
    lua_State* L = dmScript::GetLuaState(context);
    lua_pushcfunction(L, Pump);
    lua_setglobal(L, "pump");
    g_Params.m_L = L;
    g_Params.m_ConfigFile = config;
    if (prelude)
        Check(luaL_dofile(L, prelude) == 0, "Prelude failed before extension initialization");
    dmExtension::Initialize(&g_Params);
    Check(lua_gettop(L) == 0, "Extension initialization changed the Lua stack");
    return context;
}
static void Destroy(dmScript::HContext context)
{
    g_Params.m_L = dmScript::GetLuaState(context);
    dmExtension::Finalize(&g_Params);
    dmScript::Finalize(context);
    dmScript::DeleteContext(context);
}
static dmConfigFile::HConfig Config(const char* text)
{
    dmConfigFile::HConfig config;
    Check(dmConfigFile::LoadFromBuffer(text, (uint32_t)strlen(text), 0, 0, &config) == dmConfigFile::RESULT_OK, "Invalid test config");
    return config;
}

// Checks that debugging is disabled by default, then runs DAP-controlled scripts
// through the real extension lifecycle and dmScript::PCall. Initialization and
// execution in each script context must preserve its Lua stack height. With
// --late-attach, scripts run without enabling or waiting for the debugger.
int main(int argc, char** argv)
{
    int updates = 0;
    if (argc > 2 && strcmp(argv[1], "--updates") == 0)
    {
        updates = atoi(argv[2]);
        argc -= 2;
        argv += 2;
    }
    const char* prelude = 0;
    if (argc > 2 && strcmp(argv[1], "--prelude") == 0)
    {
        prelude = argv[2];
        argc -= 2;
        argv += 2;
    }
    bool no_wait = argc > 1 && strcmp(argv[1], "--no-wait") == 0;
    if (no_wait)
    {
        --argc;
        ++argv;
    }
    const char* startup_port = 0;
    if (argc > 2 && strcmp(argv[1], "--startup-port") == 0)
    {
        startup_port = argv[2];
        argc -= 2;
        argv += 2;
    }
    bool late_attach = argc > 1 && strcmp(argv[1], "--late-attach") == 0;
    if (late_attach)
    {
        --argc;
        ++argv;
    }
    if (argc < 2 || argc > 3)
        return 1;
    dmSocket::Initialize();
    dmLog::LogParams log;
    dmLog::LogInitialize(&log);
    dmLogRegisterListener(LogListener);
    ExtensionParamsInitialize(&g_Params);
    LuaDebugger();

    if (!startup_port)
    {
        // No debugger.enabled setting: initialization must leave the Lua hook free.
        dmConfigFile::HConfig disabled = Config("[project]\ntitle=DAP test\n");
        dmScript::HContext    context = Create(disabled);
        Check(lua_gethook(dmScript::GetLuaState(context)) == 0, "Disabled debugger installed a hook");
        Destroy(context);
        dmConfigFile::Delete(disabled);
    }

    // A failed startup must be the first initialization: a preceding successful
    // initialization would mask an extension whose update callback stays disabled.
    char startup_config[128];
    snprintf(startup_config, sizeof(startup_config), "[debugger]\nenabled=1\nport=%s\nwait=%d\n", startup_port ? startup_port : "0", !no_wait);
    dmConfigFile::HConfig config = Config(late_attach ? "[debugger]\nport=0\n" : startup_config);
    dmScript::HContext    contexts[2];
    for (int i = 1; i < argc; ++i)
        contexts[i - 1] = Create(config, prelude);
    int result = 0;
    for (int i = 1; i < argc; ++i)
    {
        lua_State* L = dmScript::GetLuaState(contexts[i - 1]);
        g_Params.m_L = L;
        int status = luaL_loadfile(L, argv[i]);
        if (status == 0)
            status = dmScript::PCall(L, 0, 0);
        else
            lua_pop(L, 1);
        result |= status;
        Check(lua_gettop(L) == 0, "DAP corrupted the engine Lua stack");
        // Separate protected calls mirror engine update callbacks on this context.
        for (int frame = 0; frame < updates; ++frame)
        {
            lua_getglobal(L, "update");
            Check(dmScript::PCall(L, 0, 0) == 0, "Update callback failed");
            Check(lua_gettop(L) == 0, "DAP corrupted the callback Lua stack");
            dmExtension::Update(&g_Params);
        }
    }
    for (int i = 0; i < 50; ++i)
    {
        dmExtension::Update(&g_Params);
        dmTime::Sleep(1000);
    }
    for (int i = 1; i < argc; ++i)
        Destroy(contexts[i - 1]);
    dmConfigFile::Delete(config);
    ExtensionParamsFinalize(&g_Params);
    dmLog::LogFinalize();
    dmSocket::Finalize();
    printf("RESULT %d\n", result);
    return 0;
}
