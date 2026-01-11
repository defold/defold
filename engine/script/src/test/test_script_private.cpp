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

#include "test_script_private.h"

#include <assert.h>

namespace dmScriptTest
{

static jmp_buf*         g_CurrentJmpBuf = 0;
static lua_CFunction    g_CurrentPanicFn = 0;

static int TestLuaPanic(lua_State* L)
{
    assert(g_CurrentJmpBuf != 0);
    assert(g_CurrentPanicFn != 0);
    int result = g_CurrentPanicFn(L);
    longjmp(*g_CurrentJmpBuf, result); // Note that longjmp will automatically convert any 0 to a 1 (see documentation for longjmp)
    return result;
}

LuaPanicScope::LuaPanicScope(lua_State* L, lua_CFunction fn)
: m_L(L)
{
    assert(g_CurrentJmpBuf == 0);
    assert(g_CurrentPanicFn == 0);
    g_CurrentJmpBuf = &m_JmpBuf;
    g_CurrentPanicFn = fn;
    m_OldFn = lua_atpanic(m_L, TestLuaPanic);
}

LuaPanicScope::~LuaPanicScope()
{
    lua_atpanic(m_L, m_OldFn);
    assert(g_CurrentJmpBuf != 0);
    assert(g_CurrentPanicFn != 0);
    g_CurrentJmpBuf = 0;
    g_CurrentPanicFn = 0;
}

jmp_buf* LuaPanicScope::GetJmpBuf()
{
    return &m_JmpBuf;
}

}

