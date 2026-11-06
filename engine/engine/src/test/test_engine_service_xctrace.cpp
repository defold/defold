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
#include <dlib/profile.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "../engine_service_xctrace_osx.cpp"

static char g_CompletionMessage[4096];

static void CaptureCompletionMessage(LogSeverity, const char*, const char* message)
{
    if (strstr(message, "Native trace complete:") || strstr(message, "Open in Instruments:"))
        dmStrlCat(g_CompletionMessage, message, sizeof(g_CompletionMessage));
}

class XcTraceTest : public jc_test_base_class
{
    public:
    char        m_Directory[128];
    char        m_Path[256];
    char*       m_Environment[5];
    const char* Variable(uint32_t i)
    {
        static const char* names[] = { "DM_PROFILE_NATIVE_XCTRACE", "DM_PROFILE_NATIVE_TRACE", "DM_PROFILE_NATIVE_TRACE_DELAY_MS", "DM_PROFILE_NATIVE_TRACE_DURATION_MS", "DM_PROFILE_NATIVE_TRACE_ENERGY" };
        return names[i];
    }
    void SetUp()
    {
        for (uint32_t i = 0; i < 5; ++i)
        {
            const char* value = getenv(Variable(i));
            m_Environment[i] = value ? strdup(value) : 0;
            unsetenv(Variable(i));
        }
        strcpy(m_Directory, "/tmp/defold-native-trace-test-XXXXXX");
        ASSERT_NE((char*)0, mkdtemp(m_Directory));
        snprintf(m_Path, sizeof(m_Path), "%s/test.trace", m_Directory);
        ProfileInitialize();
        dmEngineService::Initialize();
    }
    void TearDown()
    {
        dmEngineService::Finalize();
        ProfileFinalize();
        rmdir(m_Path);
        rmdir(m_Directory);
        for (uint32_t i = 0; i < 5; ++i)
        {
            if (m_Environment[i])
            {
                setenv(Variable(i), m_Environment[i], 1);
                free(m_Environment[i]);
            }
            else
                unsetenv(Variable(i));
        }
    }
    void WaitForCompletion()
    {
        pthread_mutex_lock(&dmEngineService::g_TraceMutex);
        while (dmEngineService::IsBusy())
            pthread_cond_wait(&dmEngineService::g_TraceCondition, &dmEngineService::g_TraceMutex);
        pthread_mutex_unlock(&dmEngineService::g_TraceMutex);
    }
};

// Power is converted from milliwatts to watts; impact belongs only to the requested PID.
TEST_F(XcTraceTest, EnergySamples)
{
    const char data[] =
    "<?xml version=\"1.0\"?><plist version=\"1.0\"><dict>"
    "<key>elapsed_ns</key><integer>500000000</integer>"
    "<key>processor</key><dict>"
    "<key>cpu_power</key><real>1250</real>"
    "<key>gpu_power</key><real>750</real>"
    "<key>ane_power</key><real>0</real></dict>"
    "<key>tasks</key><array>"
    "<dict><key>pid</key><integer>111</integer><key>energy_impact_per_s</key><real>99</real></dict>"
    "<dict><key>pid</key><integer>222</integer><key>energy_impact_per_s</key><real>1.5</real></dict>"
    "</array></dict></plist>";
    dmEngineService::EnergySample sample;
    ASSERT_TRUE(dmEngineService::ParseEnergySample((const uint8_t*)data, sizeof(data) - 1, 222, &sample));
    ASSERT_EQ(500000000ULL, sample.m_Window);
    ASSERT_EQ(7U, sample.m_Components);
    ASSERT_EQ(1.25, sample.m_Power[0]);
    ASSERT_EQ(0.75, sample.m_Power[1]);
    ASSERT_EQ(0.0, sample.m_Power[2]);
    ASSERT_EQ(1.5, sample.m_Impact);
    ASSERT_FALSE(dmEngineService::ParseEnergySample((const uint8_t*)data, sizeof(data) - 1, 333, &sample));
    ASSERT_FALSE(dmEngineService::ParseEnergySample((const uint8_t*)"invalid", 7, 222, &sample));
    dmEngineService::XcTraceParams params;
    ASSERT_TRUE(params.m_Energy);
}

// A completed trace logs its path for the editor and a shell-escaped Instruments command.
TEST_F(XcTraceTest, CompletionCommand)
{
    g_CompletionMessage[0] = 0;
    dmLog::LogParams params;
    dmLog::LogInitialize(&params);
    dmLogRegisterListener(CaptureCompletionMessage);
    dmStrlCpy(dmEngineService::g_TraceStatus.m_Path, "/tmp/user's \"trace\" $HOME `name` \\!.trace", sizeof(dmEngineService::g_TraceStatus.m_Path));
    dmEngineService::SetState(dmEngineService::XCTRACE_COMPLETE, 0);
    dmLog::LogFinalize();
    dmLogUnregisterListener(CaptureCompletionMessage);
    ASSERT_NE((const char*)0, strstr(g_CompletionMessage, "Native trace complete: /tmp/user's \"trace\" $HOME `name` \\!.trace\n"));
    ASSERT_NE((const char*)0, strstr(g_CompletionMessage, "open -a Instruments '/tmp/user'\\''s \"trace\" $HOME `name` \\!.trace'\n"));
}

// Pending captures reject overlapping requests, cancel without writing a trace, and allow another request.
TEST_F(XcTraceTest, BusyAndCancel)
{
    dmEngineService::XcTraceParams params;
    params.m_Path = m_Path;
    params.m_DelayMs = 60000;
    params.m_DurationMs = 0;
    ASSERT_EQ(dmEngineService::XCTRACE_OK, dmEngineService::StartXcTrace(params));
    ASSERT_EQ(dmEngineService::XCTRACE_BUSY, dmEngineService::StartXcTrace(params));
    dmEngineService::StopXcTrace();
    WaitForCompletion();
    dmEngineService::XcTraceStatus status;
    dmEngineService::GetXcTraceStatus(&status);
    ASSERT_EQ(dmEngineService::XCTRACE_CANCELLED, status.m_State);
    ASSERT_NE(0, access(m_Path, F_OK));
    ASSERT_EQ(dmEngineService::XCTRACE_OK, dmEngineService::StartXcTrace(params));
    dmEngineService::Finalize();
    dmEngineService::GetXcTraceStatus(&status);
    ASSERT_EQ(dmEngineService::XCTRACE_IDLE, status.m_State);
    ASSERT_NE(0, access(m_Path, F_OK));
}

// Existing .trace bundles and relative output paths cannot be silently replaced or resolved elsewhere.
TEST_F(XcTraceTest, InvalidOutput)
{
    dmEngineService::XcTraceParams params;
    params.m_Path = m_Path;
    ASSERT_EQ(0, mkdir(m_Path, 0700));
    ASSERT_EQ(dmEngineService::XCTRACE_INVALID, dmEngineService::StartXcTrace(params));
    params.m_Path = "relative.trace";
    ASSERT_EQ(dmEngineService::XCTRACE_INVALID, dmEngineService::StartXcTrace(params));
}

// An explicit invalid tool path fails with a useful status instead of falling back to another Xcode.
TEST_F(XcTraceTest, MissingExecutable)
{
    setenv("DM_PROFILE_NATIVE_XCTRACE", "/no-such-xcode/xctrace", 1);
    dmEngineService::XcTraceParams params;
    params.m_Path = m_Path;
    ASSERT_EQ(dmEngineService::XCTRACE_OK, dmEngineService::StartXcTrace(params));
    WaitForCompletion();
    dmEngineService::XcTraceStatus status;
    dmEngineService::GetXcTraceStatus(&status);
    ASSERT_EQ(dmEngineService::XCTRACE_ERROR, status.m_State);
    ASSERT_NE((const char*)0, strstr(status.m_Error, "xctrace was not found"));
}

// Invalid startup timing and energy values fail explicitly instead of starting a capture.
TEST_F(XcTraceTest, EnvironmentParameters)
{
    setenv("DM_PROFILE_NATIVE_TRACE", "1", 1);
    const char* invalid[] = { "-1", "1.5", "", "4294967296", "999999999999999999999" };
    for (uint32_t i = 0; i < sizeof(invalid) / sizeof(invalid[0]); ++i)
    {
        setenv("DM_PROFILE_NATIVE_TRACE_DURATION_MS", invalid[i], 1);
        dmEngineService::Initialize();
        dmEngineService::XcTraceStatus status;
        dmEngineService::GetXcTraceStatus(&status);
        ASSERT_EQ(dmEngineService::XCTRACE_ERROR, status.m_State);
    }
    unsetenv("DM_PROFILE_NATIVE_TRACE_DURATION_MS");
    setenv("DM_PROFILE_NATIVE_TRACE_ENERGY", "2", 1);
    dmEngineService::Initialize();
    dmEngineService::XcTraceStatus status;
    dmEngineService::GetXcTraceStatus(&status);
    ASSERT_EQ(dmEngineService::XCTRACE_ERROR, status.m_State);
    ASSERT_STREQ("Trace energy must be 0 or 1.", status.m_Error);
}

int main(int argc, char** argv)
{
    jc_test_init(&argc, argv);
    return jc_test_run_all();
}
