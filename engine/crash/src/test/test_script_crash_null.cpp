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

#define JC_TEST_IMPLEMENTATION
#include <jc_test/jc_test.h>
#include <script/script.h>
#include <extension/extension.hpp>
#include <dlib/dstrings.h>
#include <dlib/hash.h>
#include <dlib/log.h>
#include <dlib/configfile.h>
#include <resource/resource.h>
#include "crash.h"

extern "C"
{
#include <lua/lauxlib.h>
#include <lua/lualib.h>
}

#if defined(__NX__)
    #define MOUNTFS "host:/"
#else
    #define MOUNTFS ""
#endif

#define PATH_FORMAT "build/src/test/%s"

class ScriptCrashTest : public jc_test_base_class
{
protected:
    void SetUp() override
    {
        dmCrash::Init("DefoldScriptTest", "0123456789abcdef");

        dmConfigFile::Result r = dmConfigFile::LoadFromBuffer(0, 0, 0, 0, &m_ConfigFile);
        ASSERT_EQ(dmConfigFile::RESULT_OK, r);

        dmResource::NewFactoryParams factory_params;
        m_ResourceFactory = dmResource::NewFactory(&factory_params, ".");

        dmScript::ContextParams script_context_params = {};
        script_context_params.m_Factory = m_ResourceFactory;
        script_context_params.m_ConfigFile = m_ConfigFile;
        m_Context = dmScript::NewContext(script_context_params);

        dmExtension::AppParams app_params;
        app_params.m_ConfigFile = m_ConfigFile;
        dmExtension::AppInitialize(&app_params);

        dmScript::Initialize(m_Context);
        L = dmScript::GetLuaState(m_Context);

    }

    void TearDown() override
    {
        dmConfigFile::Delete(m_ConfigFile);
        dmResource::DeleteFactory(m_ResourceFactory);
        dmScript::Finalize(m_Context);
        dmScript::DeleteContext(m_Context);

        dmExtension::AppParams app_params;
        app_params.m_ConfigFile = m_ConfigFile;
        dmExtension::AppFinalize(&app_params);
    }

    dmScript::HContext m_Context;
    dmConfigFile::HConfig m_ConfigFile;
    dmResource::HFactory m_ResourceFactory;
    lua_State* L;
};

bool RunFile(lua_State* L, const char* filename)
{
    char path[64];
    dmSnPrintf(path, 64, MOUNTFS PATH_FORMAT, filename);
    if (luaL_dofile(L, path) != 0)
    {
        dmLogError("%s", lua_tolstring(L, -1, 0));
        return false;
    }
    return true;
}

TEST_F(ScriptCrashTest, TestCrashNull)
{
    int top = lua_gettop(L);

    ASSERT_TRUE(RunFile(L, "test_crash_null.luac"));

    lua_getglobal(L, "functions");
    ASSERT_EQ(LUA_TTABLE, lua_type(L, -1));
    lua_getfield(L, -1, "test_crash_null");
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

extern "C" void dmExportedSymbols();

int main(int argc, char **argv)
{
    dmExportedSymbols();
    jc_test_init(&argc, argv);

    int ret = jc_test_run_all();
    return ret;
}
