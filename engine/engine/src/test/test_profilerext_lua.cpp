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

#include <dlib/array.h>
#include <dlib/configfile.h>
#include <dlib/log.h>
#include <extension/extension.hpp>
#include <script/script.h>
#include <testmain/testmain.h>

#include <string.h>

#define JC_TEST_IMPLEMENTATION
#include <jc_test/jc_test.h>

namespace
{
    class ProfilerExtLuaTest : public jc_test_base_class
    {
    public:
        void SetUp() override
        {
            m_ExtensionInitialized = false;
            m_AppInitialized = false;
            s_LogCaptureContext = this;

            dmLog::LogParams log_params;
            dmLog::LogInitialize(&log_params);
            dmLogRegisterListener(LogListener);

            dmConfigFile::Result config_result = dmConfigFile::LoadFromBuffer(0, 0, 0, 0, &m_ConfigFile);
            ASSERT_EQ(dmConfigFile::RESULT_OK, config_result);

            dmScript::ContextParams script_context_params = {};
            script_context_params.m_ConfigFile = m_ConfigFile;
            m_Context = dmScript::NewContext(script_context_params);
            ASSERT_NE((dmScript::HContext) 0, m_Context);
            dmScript::Initialize(m_Context);
            m_L = dmScript::GetLuaState(m_Context);
            ASSERT_NE((lua_State*) 0, m_L);

            ExtensionAppParamsInitialize(&m_AppParams);
            m_AppParams.m_ConfigFile = m_ConfigFile;
            ASSERT_EQ(dmExtension::RESULT_OK, dmExtension::AppInitialize(&m_AppParams));

            ExtensionParamsInitialize(&m_Params);
            m_Params.m_L = m_L;
            m_Params.m_ConfigFile = m_ConfigFile;
            ASSERT_EQ(dmExtension::RESULT_OK, dmExtension::Initialize(&m_Params));
            m_ExtensionInitialized = true;
            m_AppInitialized = true;
        }

        void TearDown() override
        {
            if (m_ExtensionInitialized)
            {
                ASSERT_EQ(dmExtension::RESULT_OK, dmExtension::Finalize(&m_Params));
                m_ExtensionInitialized = false;
            }
            if (m_AppInitialized)
            {
                ASSERT_EQ(dmExtension::RESULT_OK, dmExtension::AppFinalize(&m_AppParams));
                m_AppInitialized = false;
            }

            ExtensionParamsFinalize(&m_Params);
            ExtensionAppParamsFinalize(&m_AppParams);

            dmScript::Finalize(m_Context);
            dmScript::DeleteContext(m_Context);
            dmConfigFile::Delete(m_ConfigFile);

            if (s_LogCaptureContext)
            {
                dmLogUnregisterListener(LogListener);
                dmLog::LogFinalize();
                s_LogCaptureContext = 0;
            }
        }

        bool RunString(const char* script)
        {
            if (luaL_dostring(m_L, script) != 0)
            {
                dmLogError("%s", lua_tostring(m_L, -1));
                lua_pop(m_L, 1);
                return false;
            }
            return true;
        }

        void FinalizeExtension()
        {
            ASSERT_TRUE(m_ExtensionInitialized);
            ASSERT_EQ(dmExtension::RESULT_OK, dmExtension::Finalize(&m_Params));
            m_ExtensionInitialized = false;
        }

        char* GetLog()
        {
            if (s_LogCaptureContext)
            {
                dmLog::LogFinalize();
                s_LogCaptureContext = 0;
            }

            m_Log.SetCapacity(m_Log.Size() + 1);
            m_Log.Push('\0');
            return m_Log.Begin();
        }

        static void LogListener(LogSeverity severity, const char* domain, const char* formatted_string)
        {
            (void) severity;
            (void) domain;

            if (s_LogCaptureContext != 0)
            {
                s_LogCaptureContext->AppendToLog(formatted_string);
            }
        }

        void AppendToLog(const char* log)
        {
            uint32_t len = strlen(log);
            m_Log.SetCapacity(m_Log.Size() + len + 1);
            m_Log.PushArray(log, len);
        }

        dmScript::HContext  m_Context;
        dmConfigFile::HConfig m_ConfigFile;
        lua_State*          m_L;
        ExtensionAppParams  m_AppParams;
        ExtensionParams     m_Params;
        dmArray<char>       m_Log;
        bool                m_ExtensionInitialized;
        bool                m_AppInitialized;

        static ProfilerExtLuaTest* s_LogCaptureContext;
    };

    ProfilerExtLuaTest* ProfilerExtLuaTest::s_LogCaptureContext = 0;
}

TEST_F(ProfilerExtLuaTest, ScopeEndWithoutBeginRaisesLuaError)
{
    ASSERT_TRUE(RunString(
        "local ok, err = pcall(function() profiler.scope_end() end)\n"
        "assert(not ok)\n"
        "assert(string.find(err, \"profiler.scope_end() called without a matching profiler.scope_begin()\", 1, true))\n"));
}

TEST_F(ProfilerExtLuaTest, UnclosedScopesAreAutoClosedOnPreRender)
{
    ASSERT_TRUE(RunString(
        "profiler.scope_begin(\"outer\")\n"
        "profiler.scope_begin(\"inner\")\n"));

    dmExtension::PreRender(&m_Params);

    ASSERT_TRUE(RunString(
        "local ok, err = pcall(function() profiler.scope_end() end)\n"
        "assert(not ok)\n"
        "assert(string.find(err, \"profiler.scope_end() called without a matching profiler.scope_begin()\", 1, true))\n"));

    char* log = GetLog();
    ASSERT_NE((char*) 0, strstr(log, "Lua profiler scope 'inner' was not closed before the end of the frame. Auto-closing it."));
    ASSERT_NE((char*) 0, strstr(log, "Lua profiler scope 'outer' was not closed before the end of the frame. Auto-closing it."));
}

TEST_F(ProfilerExtLuaTest, CoroutineScopesAreAutoClosedOnPreRender)
{
    ASSERT_TRUE(RunString(
        "co = coroutine.create(function()\n"
        "    profiler.scope_begin(\"coroutine_outer\")\n"
        "    profiler.scope_begin(\"coroutine_inner\")\n"
        "    coroutine.yield()\n"
        "    profiler.scope_end()\n"
        "end)\n"
        "assert(coroutine.resume(co))\n"));

    lua_getglobal(m_L, "co");
    lua_State* coroutine_state = lua_tothread(m_L, -1);
    ASSERT_NE((lua_State*) 0, coroutine_state);
    ASSERT_NE(m_L, coroutine_state);
    ASSERT_EQ(m_L, dmScript::GetMainThread(coroutine_state));
    lua_pop(m_L, 1);

    dmExtension::PreRender(&m_Params);

    ASSERT_TRUE(RunString(
        "local ok, err = coroutine.resume(co)\n"
        "assert(not ok)\n"
        "assert(string.find(err, \"profiler.scope_end() called without a matching profiler.scope_begin()\", 1, true))\n"));

    char* log = GetLog();
    ASSERT_NE((char*) 0, strstr(log, "Lua profiler scope 'coroutine_inner' was not closed before the end of the frame. Auto-closing it."));
    ASSERT_NE((char*) 0, strstr(log, "Lua profiler scope 'coroutine_outer' was not closed before the end of the frame. Auto-closing it."));
}

TEST_F(ProfilerExtLuaTest, CoroutineScopesAreAutoClosedOnFinalize)
{
    ASSERT_TRUE(RunString(
        "co = coroutine.create(function()\n"
        "    profiler.scope_begin(\"coroutine_finalize\")\n"
        "end)\n"
        "assert(coroutine.resume(co))\n"));

    FinalizeExtension();

    char* log = GetLog();
    ASSERT_NE((char*) 0, strstr(log, "Lua profiler scope 'coroutine_finalize' was not closed before the end of the frame. Auto-closing it."));
}

extern "C" void dmExportedSymbols();

int main(int argc, char **argv)
{
    dmExportedSymbols();
    TestMainPlatformInit();
    jc_test_init(&argc, argv);
    return jc_test_run_all();
}
