// Copyright 2020-2023 The Defold Foundation
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

#include "test_script.h"
#include <dlib/testutil.h>

#define JC_TEST_IMPLEMENTATION
#include <jc_test/jc_test.h>

namespace dmScriptTest
{

bool RunFile(lua_State* L, const char* filename)
{
    char path[1024];
    dmTestUtil::MakeHostPathf(path, sizeof(path), "build/src/test/%s", filename);

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

static ScriptTest* s_LogListenerContext = 0;

static void LogCallback(LogSeverity severity, const char* domain, const char* formatted_string)
{
    ScriptTest* i = (ScriptTest*)s_LogListenerContext;
    i->AppendToLog(formatted_string);
}

void ScriptTest::SetUp()
{
    s_LogListenerContext = this;
    dmLog::LogParams params;
    dmLog::LogInitialize(&params);
    dmLogRegisterListener(LogCallback);

    m_ResourceFactory = 0;

    char path[1024];
    dmTestUtil::MakeHostPath(path, sizeof(path), "src/test/test.config");

    dmConfigFile::Result r = dmConfigFile::Load(path, 0, 0, &m_ConfigFile);
    if (r == dmConfigFile::RESULT_OK)
    {
        dmResource::NewFactoryParams factory_params;
        m_ResourceFactory = dmResource::NewFactory(&factory_params, ".");

        m_Context = dmScript::NewContext(m_ConfigFile, m_ResourceFactory, true);
        dmScript::Initialize(m_Context);
        L = dmScript::GetLuaState(m_Context);
    }
    ASSERT_TRUE(m_ConfigFile != 0);
    ASSERT_TRUE(L != 0);
}

void ScriptTest::FinalizeLogs()
{
    if (s_LogListenerContext != 0)
    {
        dmLog::LogFinalize();
        s_LogListenerContext = 0;
    }
}

void ScriptTest::TearDown()
{
    dmConfigFile::Delete(m_ConfigFile);
    dmResource::DeleteFactory(m_ResourceFactory);

    dmScript::Finalize(m_Context);
    dmScript::DeleteContext(m_Context);

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
    return dmScriptTest::RunFile(L, filename);
}
bool ScriptTest::RunString(lua_State* L, const char* script)
{
    return dmScriptTest::RunString(L, script);
}

}
