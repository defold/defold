// Copyright 2020-2026 The Defold Foundation
// Licensed under the Defold License version 1.0. See https://www.defold.com/license

// A real Lua host for the DAP integration suite. It has no debugger control
// shortcuts: the test client can inspect/control Lua only through the TCP port.
#include "debugger.h"
#include <dlib/socket.h>
#include <dlib/time.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
extern "C"
{
#include <lua/lua.h>
#include <lua/lauxlib.h>
#include <lua/lualib.h>
#if defined(DM_LUA_USE_LUA51)
#include <lua/lstate.h>
#endif
}

static dmDebugger::HDebugger g_Debugger;
static int                   g_OldHookCalls;
static void                  OldHook(lua_State*, lua_Debug*)
{
    ++g_OldHookCalls;
}
static int Pump(lua_State*)
{
    dmDebugger::Update(g_Debugger);
    return 0;
}
static int Error(lua_State* L)
{
    dmDebugger::OnError(g_Debugger, L);
    lua_settop(L, 1);
    return 1;
}

static void Check(bool value, const char* message)
{
    if (!value)
    {
        fprintf(stderr, "%s\n", message);
        exit(20);
    }
}

#if defined(DM_LUA_USE_LUA51)
static int CheckResumeDepth(lua_State* L)
{
    lua_State* parent = lua_tothread(L, 1);
    Check(parent && L->nCcalls > parent->nCcalls, "Wrapped coroutine did not inherit its resumer's C-call depth");
    return 0;
}

// Checks that a wrapped coroutine's C-call depth exceeds its resumer's depth,
// preserving Lua 5.1's bookkeeping for nested native calls.
static void CheckWrappedResumeDepth(lua_State* L)
{
    // Check the depth invariant with a single resume, without approaching the
    // runtime's recursion limit.
    Check(luaL_loadstring(L, "local check, parent = ...; coroutine.wrap(function() check(parent) end)()") == 0, "Unable to load wrapped-resume depth check");
    lua_pushcfunction(L, CheckResumeDepth);
    lua_pushthread(L);
    Check(lua_pcall(L, 2, 0, 0) == 0, "Wrapped-resume depth check failed");
}
#endif

static lua_State* Create(const char* name)
{
    lua_State* L = luaL_newstate();
    Check(L != 0, "luaL_newstate failed");
    luaL_openlibs(L);
    lua_pushcfunction(L, Pump);
    lua_setglobal(L, "pump");
    lua_sethook(L, OldHook, LUA_MASKCOUNT, 100);
    dmDebugger::AddLuaState(g_Debugger, L, name);
#if defined(DM_LUA_USE_LUA51)
    CheckWrappedResumeDepth(L);
#endif
    Check(lua_gettop(L) == 0, "AddLuaState leaked Lua stack values");
    return L;
}

static int Run(lua_State* L, const char* path)
{
    lua_pushcfunction(L, Error);
    int result = luaL_loadfile(L, path);
    if (result == 0)
        result = lua_pcall(L, 0, 0, 1);
    if (result)
    {
        fprintf(stderr, "Lua: %s\n", lua_tostring(L, -1));
        lua_pop(L, 1);
    }
    lua_pop(L, 1);
    Check(lua_gettop(L) == 0, "Debugger corrupted the Lua stack");
    return result;
}

// Runs the DAP suite in one or two independent Lua states and checks that setup,
// script execution, and teardown preserve the Lua stack. Deleting the debugger
// must restore the previous hooks and allow subsequent Lua execution.
int main(int argc, char** argv)
{
    int updates = 0;
    if (argc > 2 && strcmp(argv[1], "--updates") == 0)
    {
        updates = atoi(argv[2]);
        argc -= 2;
        argv += 2;
    }
    if (argc < 2)
        return 1;
    Check(dmSocket::Initialize() == dmSocket::RESULT_OK, "Socket initialization failed");
    g_Debugger = dmDebugger::New(0);
    Check(g_Debugger != 0, "Debugger listener failed");
    lua_State* first = Create("main");
    lua_State* second = argc > 2 ? Create("second") : 0;
    printf("PORT %u\n", dmDebugger::GetPort(g_Debugger));
    fflush(stdout);
    dmDebugger::WaitForClient(g_Debugger);
    int result = Run(first, argv[1]);
    if (second)
        result |= Run(second, argv[2]);
    // Invoke separate callbacks from C, as the engine does between frames.
    for (int i = 0; i < updates; ++i)
    {
        lua_getglobal(first, "update");
        Check(lua_pcall(first, 0, 0, 0) == 0, "Update callback failed");
        Check(lua_gettop(first) == 0, "Debugger corrupted the callback Lua stack");
        dmDebugger::Update(g_Debugger);
    }
    // Let the client receive final responses before the host shuts down.
    for (int i = 0; i < 50; ++i)
    {
        dmDebugger::Update(g_Debugger);
        dmTime::Sleep(1000);
    }
    dmDebugger::Delete(g_Debugger);
    Check(lua_gethook(first) == OldHook && lua_gethookcount(first) == 100, "Original Lua hook was not restored");
    Check(lua_gettop(first) == 0, "Delete leaked Lua stack values");
    Check(luaL_dostring(first, "local n=0 for i=1,1000 do n=n+i end") == 0, "Lua stopped working after detach");
    Check(g_OldHookCalls > 0, "Original hook is not running after detach");
    if (second)
    {
        Check(lua_gethook(second) == OldHook, "Second context hook was not restored");
        lua_close(second);
    }
    lua_close(first);
    dmSocket::Finalize();
    printf("RESULT %d\n", result);
    return 0;
}
