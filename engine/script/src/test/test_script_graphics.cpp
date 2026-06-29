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

#include "script.h"
#include "test_script.h"

#include <testmain/testmain.h>
#include <dlib/log.h>

class ScriptGraphicsTest : public dmScriptTest::ScriptTest
{
};

// Drives the full Lua-side suite via test_graphics.lua's `functions.test_graphics`.
// The Lua side asserts on shape/types and on the "no graphics context" baseline
// that the script test harness produces (no HContext is wired into dmScript here).
TEST_F(ScriptGraphicsTest, TestGraphics)
{
    int top = lua_gettop(L);

    ASSERT_TRUE(RunFile(L, "test_graphics.luac"));

    lua_getglobal(L, "functions");
    ASSERT_EQ(LUA_TTABLE, lua_type(L, -1));
    lua_getfield(L, -1, "test_graphics");
    ASSERT_EQ(LUA_TFUNCTION, lua_type(L, -1));
    int result = dmScript::PCall(L, 0, LUA_MULTRET);
    if (result == LUA_ERRRUN)
    {
        ASSERT_TRUE(false);
    }
    else
    {
        ASSERT_EQ(0, result);
    }
    lua_pop(L, 1);

    ASSERT_EQ(top, lua_gettop(L));
}

// Spot-check from C++ side: graphics.get_engine_adapters must include "null"
// when the test binary links GRAPHICS_NULL. This guards against the Lua
// suite silently being skipped (e.g. if `functions` table wasn't registered).
TEST_F(ScriptGraphicsTest, GetEngineAdaptersIncludesNull)
{
    int top = lua_gettop(L);

    ASSERT_TRUE(RunString(L, "return graphics.get_engine_adapters()"));
    ASSERT_EQ(LUA_TTABLE, lua_type(L, -1));

    bool has_null = false;
    int count = 0;
    lua_pushnil(L);
    while (lua_next(L, -2) != 0)
    {
        if (lua_type(L, -1) == LUA_TSTRING)
        {
            const char* name = lua_tostring(L, -1);
            if (name && strcmp(name, "null") == 0)
                has_null = true;
            count++;
        }
        lua_pop(L, 1); // pop value, keep key for next iteration
    }
    lua_pop(L, 1); // pop the table itself

    ASSERT_GT(count, 0);
    ASSERT_TRUE(has_null);

    ASSERT_EQ(top, lua_gettop(L));
}

// Spot-check from C++ side: graphics.get_adapter_info returns a well-formed
// table even with no graphics context wired in. Pulls out the family field
// and confirms it's the "none" sentinel.
TEST_F(ScriptGraphicsTest, GetAdapterInfoFamilyWithoutContext)
{
    int top = lua_gettop(L);

    ASSERT_TRUE(RunString(L, "return graphics.get_adapter_info()"));
    ASSERT_EQ(LUA_TTABLE, lua_type(L, -1));

    lua_getfield(L, -1, "family");
    ASSERT_EQ(LUA_TSTRING, lua_type(L, -1));
    ASSERT_STREQ("none", lua_tostring(L, -1));
    lua_pop(L, 1);

    lua_getfield(L, -1, "limits");
    ASSERT_EQ(LUA_TTABLE, lua_type(L, -1));
    lua_pop(L, 1);

    lua_getfield(L, -1, "extensions");
    ASSERT_EQ(LUA_TTABLE, lua_type(L, -1));
    lua_pop(L, 1);

    lua_getfield(L, -1, "features");
    ASSERT_EQ(LUA_TTABLE, lua_type(L, -1));
    lua_pop(L, 1);

    lua_pop(L, 1); // pop the table itself
    ASSERT_EQ(top, lua_gettop(L));
}

extern "C" void dmExportedSymbols();

int main(int argc, char **argv)
{
    dmExportedSymbols();
    TestMainPlatformInit();
    jc_test_init(&argc, argv);
    return jc_test_run_all();
}
