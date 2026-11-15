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

#include "test_script.h"
#include <dlib/testutil.h>
#include <dlib/time.h>
#include <stdlib.h>

namespace dmScriptTest
{

bool RunFile(lua_State* L, const char* filename, const char* base_dir)
{
    char path[1024];
    dmTestUtil::MakeHostPathf(path, sizeof(path), "%s/%s", base_dir, filename);

    if (luaL_dofile(L, path) != 0)
    {
        dmLogError("%s", lua_tolstring(L, -1, 0));
        return false;
    }
    return true;
}


bool RunString(lua_State* L, const char* script)
{
    if (luaL_dostring(L, script) != 0)
    {
        dmLogError("%s", lua_tolstring(L, -1, 0));
        return false;
    }
    return true;
}

// Fixture timings are enabled in CI and opt-in locally, outside jc_test's body timings.
struct ScriptFixtureTimings
{
    ScriptFixtureTimings() : m_Enabled(getenv("DEFOLD_TEST_FIXTURE_TIMINGS") != 0 || getenv("GITHUB_ACTIONS") != 0)
    {
        memset(m_Time, 0, sizeof(m_Time));
        memset(m_Count, 0, sizeof(m_Count));
    }

    ~ScriptFixtureTimings()
    {
        const char* phases[] = { "log initialize", "context initialize", "context finalize", "log finalize" };
        for (uint32_t i = 0; m_Enabled && i < 4; ++i)
        {
            fprintf(stderr, "SCRIPT FIXTURE: %s: %u calls, %.3f ms total\n", phases[i], m_Count[i], m_Time[i] / 1000.0);
        }
    }

    uint64_t Begin()
    {
        return m_Enabled ? dmTime::GetMonotonicTime() : 0;
    }

    void End(uint32_t phase, uint64_t start)
    {
        if (m_Enabled)
        {
            m_Time[phase] += dmTime::GetMonotonicTime() - start;
            ++m_Count[phase];
        }
    }

    bool     m_Enabled;
    uint64_t m_Time[4];
    uint32_t m_Count[4];
};
static ScriptFixtureTimings s_FixtureTimings;
static ScriptTest* s_LogListenerContext = 0;

static void LogCallback(LogSeverity severity, const char* domain, const char* formatted_string)
{
    ScriptTest* i = (ScriptTest*)s_LogListenerContext;
    i->AppendToLog(formatted_string);
}

void ScriptTest::SetUp()
{
    uint64_t start = s_FixtureTimings.Begin();
    s_LogListenerContext = this;
    dmLog::LogParams params;
    dmLog::LogInitialize(&params);
    dmLogRegisterListener(LogCallback);
    s_FixtureTimings.End(0, start);
    start = s_FixtureTimings.Begin();

    m_ResourceFactory = 0;

    char path[1024];
    dmTestUtil::MakeHostPath(path, sizeof(path), "src/test/test.config");

    dmConfigFile::Result r = dmConfigFile::Load(path, 0, 0, &m_ConfigFile);
    if (r == dmConfigFile::RESULT_OK)
    {
        dmResource::NewFactoryParams factory_params;
        m_ResourceFactory = dmResource::NewFactory(&factory_params, ".");

        dmScript::ContextParams context_params = {};
        context_params.m_ConfigFile = m_ConfigFile;
        context_params.m_Factory    = m_ResourceFactory;
        m_Context = dmScript::NewContext(context_params);
        dmScript::Initialize(m_Context);
        L = dmScript::GetLuaState(m_Context);
    }
    s_FixtureTimings.End(1, start);
    ASSERT_TRUE(m_ConfigFile != 0);
    ASSERT_TRUE(L != 0);
}

void ScriptTest::FinalizeLogs()
{
    if (s_LogListenerContext != 0)
    {
        uint64_t start = s_FixtureTimings.Begin();
        dmLog::LogFinalize();
        s_FixtureTimings.End(3, start);
        s_LogListenerContext = 0;
    }
}

void ScriptTest::TearDown()
{
    uint64_t start = s_FixtureTimings.Begin();
    dmConfigFile::Delete(m_ConfigFile);
    dmResource::DeleteFactory(m_ResourceFactory);

    dmScript::Finalize(m_Context);
    dmScript::DeleteContext(m_Context);
    s_FixtureTimings.End(2, start);

    if (s_LogListenerContext)
        dmLogUnregisterListener(LogCallback);
    FinalizeLogs();
}

char* ScriptTest::GetLog()
{
    FinalizeLogs(); // Waits for the log thread to close

    m_Log.SetCapacity(m_Log.Size() + 1);
    m_Log.Push('\0');
    return m_Log.Begin();
}

void ScriptTest::AppendToLog(const char* log)
{
    if (strstr(log, "Log server started on port") != 0)
        return;
    uint32_t len = strlen(log);
    m_Log.SetCapacity(m_Log.Size() + len + 1);
    m_Log.PushArray(log, len);
}

bool ScriptTest::RunFile(lua_State* L, const char* filename)
{
    return dmScriptTest::RunFile(L, filename, "build/src/test/scripts");
}
bool ScriptTest::RunFile(lua_State* L, const char* filename, const char* base_dir)
{
    return dmScriptTest::RunFile(L, filename, base_dir);
}
bool ScriptTest::RunString(lua_State* L, const char* script)
{
    return dmScriptTest::RunString(L, script);
}

}
