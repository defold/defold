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

    // An optional host adapter for userdata backed by a table. Push that table
    // and return true, or preserve the stack and return false. The adapter must
    // not run application code or raise an error, including on yielded threads.
    typedef bool (*UserdataTableResolver)(lua_State* L, int index);
    void SetUserdataTableResolver(HDebugger debugger, UserdataTableResolver resolver);

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
