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

#ifndef DM_TEST_SCRIPT_PRIVATE_H
#define DM_TEST_SCRIPT_PRIVATE_H

#include <setjmp.h>

extern "C"
{
#include <lua/lauxlib.h>
#include <lua/lualib.h>
}

namespace dmScriptTest
{
    struct LuaPanicScope
    {
        lua_State* m_L;
        lua_CFunction m_OldFn;
        jmp_buf m_JmpBuf;
        LuaPanicScope(lua_State* L, lua_CFunction fn);
        ~LuaPanicScope();
        jmp_buf* GetJmpBuf();
    };

#if !defined(_WIN32) // It currently doesn't work on Win32, due to LuaJIT's exception handling

    // Designates a local scope, panic protected via a setjmp
    // Use integer JMPVAL to check the return value from setjmp()
    #define DM_SCRIPT_TEST_PANIC_SCOPE(STATE, FN, JMPVAL) \
        dmScriptTest::LuaPanicScope panic_scope(STATE, FN); \
        JMPVAL = setjmp(*panic_scope.GetJmpBuf());

#endif
}

#endif // DM_TEST_SCRIPT_PRIVATE_H
