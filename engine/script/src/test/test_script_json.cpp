// Copyright 2020-2025 The Defold Foundation
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

#include <string.h> // memcpy

#include "script.h"
#include "test_script.h"

#include <testmain/testmain.h>
#include <dlib/log.h>

class ScriptJsonTest : public dmScriptTest::ScriptTest
{
};

TEST_F(ScriptJsonTest, TestJson)
{
    int top = lua_gettop(L);

    ASSERT_TRUE(RunFile(L, "test_json.luac"));

    lua_getglobal(L, "functions");
    ASSERT_EQ(LUA_TTABLE, lua_type(L, -1));
    lua_getfield(L, -1, "test_json");
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

TEST_F(ScriptJsonTest, TestJsonToLua)
{
    int top = lua_gettop(L);

    {
        const char* json_original = "{\"foo\":\"bar\",\"num\":16}hello";
        size_t json_length = 22;
        // Make it fully dynamic so that ASAN can catch it
        const char* json = (const char*)malloc(json_length);
        memcpy((void*)json, (void*)json_original, json_length);

        int ret = dmScript::JsonToLua(L, json, json_length);
        ASSERT_EQ(1, ret);
        lua_pop(L, 1);
        free((void*)json);
    }

    ASSERT_EQ(top, lua_gettop(L));
}

TEST_F(ScriptJsonTest, TestJsonToLua_Issue10304)
{
    int top = lua_gettop(L);

    {
        const char* json_original = "xxxx";
        size_t json_length = 4;
        // Make it fully dynamic so that ASAN can catch it
        const char* json = (const char*)malloc(json_length);
        memcpy((void*)json, (void*)json_original, json_length);

        int ret = dmScript::JsonToLua(L, json, json_length);
        ASSERT_EQ(0, ret);
        int newtop = lua_gettop(L);
        ASSERT_EQ(0, newtop - top);
        free((void*)json);
    }

    ASSERT_EQ(top, lua_gettop(L));
}


TEST_F(ScriptJsonTest, TestLuaToJson)
{
    int top = lua_gettop(L);

    {
        lua_newtable(L);
            lua_pushstring(L, "str");
            lua_setfield(L, -2, "a");
            lua_pushnumber(L, 1.5);
            lua_setfield(L, -2, "b");

            lua_newtable(L);
                lua_pushinteger(L, 7);
                lua_setfield(L, -2, "d");

            lua_setfield(L, -2, "c");

        char* json = 0;
        size_t json_length;
        int ret = dmScript::LuaToJson(L, &json, &json_length);
        ASSERT_EQ(1, ret);

        ASSERT_EQ(31u, (uint32_t)json_length);
        // Since the ordering will be different
        // {"a":"str","b":1.5,"c":{"d":7}}
        ASSERT_EQ('{', json[0]);
        ASSERT_EQ('}', json[json_length-1]);
        ASSERT_EQ('\0', json[json_length]);
        ASSERT_TRUE(strstr(json, "\"a\":\"str\"") != 0);
        ASSERT_TRUE(strstr(json, "\"b\":1.5") != 0);
        ASSERT_TRUE(strstr(json, "\"c\":{\"d\":7}") != 0);

        lua_pop(L, 1);
        free((void*)json);
    }

    ASSERT_EQ(top, lua_gettop(L));
}

extern "C" void dmExportedSymbols();

int main(int argc, char **argv)
{
    dmExportedSymbols();
    TestMainPlatformInit();
    jc_test_init(&argc, argv);

    int ret = jc_test_run_all();
    dmLog::LogFinalize();
    return ret;
}
