// Copyright 2020-2026 The Defold Foundation
// Licensed under the Defold License version 1.0. See https://www.defold.com/license

#ifndef DM_LUA_DEBUGGER_H
#define DM_LUA_DEBUGGER_H

#include <stdint.h>

struct lua_State;

namespace dmDebugger
{
    typedef struct Debugger* HDebugger;

    // All calls, including Lua execution, must run on the Lua owner thread.
    // Socket initialization is the caller's responsibility. Port 0 selects a free port.
    HDebugger New(uint16_t port, const char* address = "127.0.0.1");
    void      Delete(HDebugger debugger);
    uint16_t  GetPort(HDebugger debugger);

    // Remove states before lua_close. A debugger does not own its Lua states.
    void AddLuaState(HDebugger debugger, lua_State* L, const char* name);
    void RemoveLuaState(HDebugger debugger, lua_State* L);
    void Update(HDebugger debugger);
    void WaitForClient(HDebugger debugger);

    // Call from a protected-call error handler BEFORE unwinding the Lua stack.
    // The original error value is at stack index 1 and is left untouched.
    void OnError(HDebugger debugger, lua_State* L);
} // namespace dmDebugger

#endif
