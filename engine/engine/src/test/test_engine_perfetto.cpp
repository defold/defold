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
#include <test/http_service_test.h>
#include <testmain/testmain.h>
#include <dlib/json.hpp>
#include <dlib/webserver.h>

#include "../engine_service_perfetto.h"

static bool g_Available;
static bool g_StartFailure;
static uint32_t g_Starts;
static uint32_t g_Stops;
static int g_AndroidApiLevel;
static char g_Trigger[96];
static dmEngineService::SystemTraceState g_ChildState;

namespace dmEngineService
{
    bool SystemTraceAppInfo(char* package, uint32_t package_size)
    {
        dmStrlCpy(package, "com.example.game", package_size);
        return g_Available;
    }

    int SystemTraceGetAndroidApiLevel()
    {
        return g_AndroidApiLevel;
    }

    int SystemTraceStartTrigger(const char* trigger, char* error, uint32_t error_size)
    {
        ++g_Starts;
        dmStrlCpy(g_Trigger, trigger, sizeof(g_Trigger));
        if (g_StartFailure)
        {
            dmStrlCpy(error, "unable to start test helper", error_size);
            return -1;
        }

        return 123;
    }

    SystemTraceState SystemTracePollTrigger(int* process, char* error, uint32_t error_size)
    {
        if (g_ChildState != SYSTEM_TRACE_TRIGGER_PENDING)
        {
            *process = 0;
        }

        if (g_ChildState == SYSTEM_TRACE_FAILED)
        {
            dmStrlCpy(error, "test helper failed", error_size);
        }

        return g_ChildState;
    }

    void SystemTraceStopTrigger(int* process)
    {
        ++g_Stops;
        *process = 0;
    }

}

class SystemTraceTest : public HttpServiceTest
{
    public:
    dmEngineService::SystemTrace m_Trace;
    dmWebServer::HServer         m_Server;

    static void Ping(void*, dmWebServer::Request* request)
    {
        dmWebServer::Send(request, "PONG", 4);
    }

    void SetUp()
    {
        HttpServiceTest::SetUp();
        memset(&m_Trace, 0, sizeof(m_Trace));
        g_Available = true;
        g_StartFailure = false;
        g_Starts = 0;
        g_Stops = 0;
        g_AndroidApiLevel = 32;
        g_Trigger[0] = 0;
        g_ChildState = dmEngineService::SYSTEM_TRACE_TRIGGER_PENDING;
        dmWebServer::NewParams params;
        params.m_Port = 0;
        ASSERT_EQ(dmWebServer::RESULT_OK, dmWebServer::New(&params, &m_Server));
        dmWebServer::HandlerParams handler;
        handler.m_Handler = dmEngineService::SystemTraceRequest;
        handler.m_Userdata = &m_Trace;
        ASSERT_EQ(dmWebServer::RESULT_OK, dmWebServer::AddHandler(m_Server, "/profiler/perfetto", &handler, dmEngineService::SYSTEM_TRACE_OPENAPI));
        handler.m_Handler = Ping;
        ASSERT_EQ(dmWebServer::RESULT_OK, dmWebServer::AddHandler(m_Server, "/ping", &handler));
    }

    void TearDown()
    {
        dmEngineService::FinalizeSystemTrace(&m_Trace);
        dmWebServer::Delete(m_Server);
        HttpServiceTest::TearDown();
    }

    uint16_t GetPort()
    {
        dmSocket::Address address;
        uint16_t port;
        dmWebServer::GetName(m_Server, &address, &port);
        return port;
    }

    void Poll()
    {
        dmEngineService::UpdateSystemTrace(&m_Trace);
        dmWebServer::Update(m_Server);
    }

    void Trigger(const char* id)
    {
        char body[256];
        dmSnPrintf(body, sizeof(body), "{\"action\":\"trigger\",\"capture_id\":\"%s\"}", id);
        Fetch("POST", "/profiler/perfetto", body);
    }

    bool CopyConfigUrl(char* url, uint32_t size)
    {
        dmJson::Document json(64);
        if (!json.Parse(m_Body.Begin(), m_Body.Size() - 1, 0, dmJson::PARSE_DECODE_VALUES))
        {
            return false;
        }

        int data = json.Field(0, "data");
        dmStrlCpy(url, json.String(json.Field(data, "config_url"), ""), size);
        return url[0] != 0;
    }

};

// Prepare a bounded CPU/GPU trace config without starting a recorder or a helper inside the engine.
TEST_F(SystemTraceTest, PrepareConfiguration)
{
    Fetch("GET", "/profiler/perfetto/config/no-plan");
    ASSERT_EQ(409, m_Status);
    Fetch("POST", "/profiler/perfetto", "{\"duration_ms\":2500}");
    ASSERT_EQ(200, m_Status);
    ASSERT_STREQ("no-store", m_CacheControl);
    ASSERT_EQ(dmEngineService::SYSTEM_TRACE_PREPARED, m_Trace.m_State);
    ASSERT_EQ(0U, g_Starts);
    dmJson::Document json(64);
    ASSERT_TRUE(json.Parse(m_Body.Begin(), m_Body.Size() - 1, 0, dmJson::PARSE_DECODE_VALUES));
    int data = json.Field(0, "data");
    ASSERT_STREQ("host", json.String(json.Field(data, "recording_owner"), ""));
    char config_url[128];
    dmSnPrintf(config_url, sizeof(config_url), "/profiler/perfetto/config/%s", m_Trace.m_CaptureId);
    ASSERT_STREQ(config_url, json.String(json.Field(data, "config_url"), ""));
    const char* record_command = json.String(json.Field(data, "record_command"), "");
    ASSERT_NE((const char*)0, strstr(record_command, "adb shell -T \"set -o pipefail; perfetto --txt -c - -o - | cat\" < defold-"));
    ASSERT_NE((const char*)0, strstr(record_command, ".pbtxt > defold-"));
    ASSERT_EQ((const char*)0, strstr(m_Body.Begin(), "retrieve_command"));
    Fetch("GET", config_url);
    ASSERT_EQ(200, m_Status);
    ASSERT_STREQ("text/plain", m_ContentType);
    ASSERT_NE((const char*)0, strstr(m_Body.Begin(), "write_into_file: true"));
    ASSERT_NE((const char*)0, strstr(m_Body.Begin(), "file_write_period_ms: 1000"));
    ASSERT_NE((const char*)0, strstr(m_Body.Begin(), "flush_period_ms: 1000"));
    ASSERT_NE((const char*)0, strstr(m_Body.Begin(), "stop_delay_ms: 2500"));
    ASSERT_NE((const char*)0, strstr(m_Body.Begin(), "sched/sched_switch"));
    ASSERT_NE((const char*)0, strstr(m_Body.Begin(), "power/cpu_frequency"));
    ASSERT_NE((const char*)0, strstr(m_Body.Begin(), "power/gpu_frequency"));
    ASSERT_NE((const char*)0, strstr(m_Body.Begin(), "gpu.renderstages"));
    ASSERT_NE((const char*)0, strstr(m_Body.Begin(), m_Trace.m_Trigger));
    ASSERT_NE((const char*)0, strstr(m_Body.Begin(), "atrace_apps: \"com.example.game\""));
    ASSERT_EQ((const char*)0, strstr(m_Body.Begin(), "duration_ms:"));
    ASSERT_EQ((const char*)0, strstr(m_Body.Begin(), "cpufreq_period_ms"));
    ASSERT_TRUE(json.Parse(dmEngineService::SYSTEM_TRACE_OPENAPI, strlen(dmEngineService::SYSTEM_TRACE_OPENAPI), 0, dmJson::PARSE_DECODE_VALUES));
    ASSERT_GE(json.Field(0, "/profiler/perfetto/config/{capture_id}"), 0);
}

// Prepare two plans before either download and verify that the first URL cannot serve the second plan.
TEST_F(SystemTraceTest, SupersededConfigUrl)
{
    Fetch("POST", "/profiler/perfetto", "{\"duration_ms\":1000}");
    ASSERT_EQ(200, m_Status);
    char first_url[128];
    ASSERT_TRUE(CopyConfigUrl(first_url, sizeof(first_url)));

    Fetch("POST", "/profiler/perfetto", "{\"duration_ms\":2500}");
    ASSERT_EQ(200, m_Status);
    char current_url[128];
    ASSERT_TRUE(CopyConfigUrl(current_url, sizeof(current_url)));

    Fetch("GET", first_url);
    ASSERT_EQ(409, m_Status);
    ASSERT_NE((const char*)0, strstr(m_Body.Begin(), "\"code\":\"systemtrace_not_prepared\""));

    Fetch("GET", current_url);
    ASSERT_EQ(200, m_Status);
    ASSERT_NE((const char*)0, strstr(m_Body.Begin(), "stop_delay_ms: 2500"));
    ASSERT_NE((const char*)0, strstr(m_Body.Begin(), m_Trace.m_Trigger));
}

// Enable initial CPU frequency polling at the Android 13 parser boundary.
TEST_F(SystemTraceTest, CpuFrequencyPolling)
{
    g_AndroidApiLevel = 33;
    Fetch("POST", "/profiler/perfetto");
    ASSERT_EQ(200, m_Status);
    char config_url[128];
    ASSERT_TRUE(CopyConfigUrl(config_url, sizeof(config_url)));

    Fetch("GET", config_url);
    ASSERT_EQ(200, m_Status);
    ASSERT_NE((const char*)0, strstr(m_Body.Begin(), "power/cpu_frequency"));
    ASSERT_NE((const char*)0, strstr(m_Body.Begin(), "linux.sys_stats"));
    ASSERT_NE((const char*)0, strstr(m_Body.Begin(), "cpufreq_period_ms: 250"));
}

// Retry delivery for the same plan after an early signal, while rejecting stale IDs and concurrent helpers.
TEST_F(SystemTraceTest, TriggerLifecycle)
{
    Trigger("missing");
    ASSERT_EQ(409, m_Status);
    Fetch("POST", "/profiler/perfetto");
    char previous[64];
    dmStrlCpy(previous, m_Trace.m_CaptureId, sizeof(previous));
    Fetch("POST", "/profiler/perfetto", "{}");
    ASSERT_NE(0, strcmp(previous, m_Trace.m_CaptureId));
    Trigger(previous);
    ASSERT_EQ(409, m_Status);
    Trigger(m_Trace.m_CaptureId);
    ASSERT_EQ(200, m_Status);
    ASSERT_EQ(dmEngineService::SYSTEM_TRACE_TRIGGER_PENDING, m_Trace.m_State);
    ASSERT_STREQ(m_Trace.m_Trigger, g_Trigger);
    Fetch("POST", "/profiler/perfetto", "{}");
    ASSERT_EQ(409, m_Status);
    ASSERT_EQ(1U, g_Starts);
    g_ChildState = dmEngineService::SYSTEM_TRACE_TRIGGER_SENT;
    Fetch("GET", "/profiler/perfetto");
    ASSERT_EQ(200, m_Status);
    ASSERT_EQ(0, m_Trace.m_TriggerProcess);
    ASSERT_NE((const char*)0, strstr(m_Body.Begin(), "\"state\":\"trigger_sent\""));
    ASSERT_EQ((const char*)0, strstr(m_Body.Begin(), "\"complete\""));
    g_ChildState = dmEngineService::SYSTEM_TRACE_TRIGGER_PENDING;
    Trigger(m_Trace.m_CaptureId);
    ASSERT_EQ(200, m_Status);
    ASSERT_EQ(dmEngineService::SYSTEM_TRACE_TRIGGER_PENDING, m_Trace.m_State);
    ASSERT_STREQ(m_Trace.m_Trigger, g_Trigger);
    ASSERT_EQ(2U, g_Starts);
    Trigger(m_Trace.m_CaptureId);
    ASSERT_EQ(409, m_Status);
    ASSERT_EQ(2U, g_Starts);
}

// Missing OS helpers and child failures produce explicit errors; retries clear the previous failure.
TEST_F(SystemTraceTest, PlatformFailures)
{
    g_Available = false;
    Fetch("GET", "/profiler/perfetto");
    ASSERT_EQ(200, m_Status);
    ASSERT_NE((const char*)0, strstr(m_Body.Begin(), "\"trigger_available\":false"));
    Fetch("POST", "/profiler/perfetto", "{}");
    ASSERT_EQ(501, m_Status);
    g_Available = true;
    Fetch("POST", "/profiler/perfetto", "{}");
    g_StartFailure = true;
    Trigger(m_Trace.m_CaptureId);
    ASSERT_EQ(500, m_Status);
    ASSERT_EQ(dmEngineService::SYSTEM_TRACE_FAILED, m_Trace.m_State);
    ASSERT_EQ(0, m_Trace.m_TriggerProcess);
    g_StartFailure = false;
    Trigger(m_Trace.m_CaptureId);
    ASSERT_EQ(200, m_Status);
    ASSERT_STREQ("", m_Trace.m_Error);
    g_ChildState = dmEngineService::SYSTEM_TRACE_FAILED;
    Fetch("GET", "/profiler/perfetto");
    ASSERT_EQ(dmEngineService::SYSTEM_TRACE_FAILED, m_Trace.m_State);
    ASSERT_NE((const char*)0, strstr(m_Body.Begin(), "test helper failed"));
    g_ChildState = dmEngineService::SYSTEM_TRACE_TRIGGER_SENT;
    Trigger(m_Trace.m_CaptureId);
    ASSERT_EQ(200, m_Status);
    Fetch("GET", "/profiler/perfetto");
    ASSERT_EQ(dmEngineService::SYSTEM_TRACE_TRIGGER_SENT, m_Trace.m_State);
    ASSERT_EQ((const char*)0, strstr(m_Body.Begin(), "\"error\""));
}

// Shutdown reaps only the trigger child; it neither controls nor claims ownership of the host's recorder.
TEST_F(SystemTraceTest, Shutdown)
{
    Fetch("POST", "/profiler/perfetto", "{}");
    Trigger(m_Trace.m_CaptureId);
    dmEngineService::FinalizeSystemTrace(&m_Trace);
    ASSERT_EQ(1U, g_Stops);
    ASSERT_EQ(0, m_Trace.m_TriggerProcess);
    ASSERT_EQ(dmEngineService::SYSTEM_TRACE_IDLE, m_Trace.m_State);
    ASSERT_STREQ("", m_Trace.m_CaptureId);
}

// Reject malformed options and unsupported routes/methods before executing any helper.
TEST_F(SystemTraceTest, InvalidRequests)
{
    const char* bodies[] = { "[]", "null", "{", "{\"duration_ms\":0}", "{\"duration_ms\":300001}", "{\"duration_ms\":1.5}", "{\"duration_ms\":true}", "{\"duration_ms\":\"1\"}", "{\"path\":\"/sdcard/trace\"}", "{\"action\":\"unknown\"}", "{\"action\":\"trigger\"}", "{\"capture_id\":\"id\"}", "{\"action\":\"trigger\",\"capture_id\":\"id\",\"duration_ms\":1}" };
    for (uint32_t i = 0; i < sizeof(bodies) / sizeof(bodies[0]); ++i)
    {
        Fetch("POST", "/profiler/perfetto", bodies[i]);
        ASSERT_EQ(400, m_Status);
        ASSERT_EQ(0U, g_Starts);
    }

    Fetch("DELETE", "/profiler/perfetto");
    ASSERT_EQ(405, m_Status);
    ASSERT_STREQ("GET, POST", m_Allow);
    Fetch("POST", "/profiler/perfetto/config/no-plan", "{}");
    ASSERT_EQ(405, m_Status);
    ASSERT_STREQ("GET", m_Allow);
    Fetch("GET", "/profiler/perfetto/config");
    ASSERT_EQ(404, m_Status);
    Fetch("GET", "/profiler/perfetto/unknown");
    ASSERT_EQ(404, m_Status);
}

// Buffered uploads keep other endpoints responsive, and oversized uploads cannot prepare a capture.
TEST_F(SystemTraceTest, BufferedRequests)
{
    m_RequestPrefixSize = 1;
    Fetch("POST", "/profiler/perfetto", "{\"duration_ms\":1000}");
    ASSERT_EQ(200, m_ProbeStatus);
    ASSERT_EQ(200, m_Status);
    ASSERT_EQ(dmEngineService::SYSTEM_TRACE_PREPARED, m_Trace.m_State);
    m_RequestPrefixSize = -1;
    Fetch("POST", "/profiler/perfetto", "", false, 65536);
    ASSERT_EQ(413, m_Status);
    ASSERT_STREQ("close", m_Connection);
    ASSERT_EQ(0U, g_Starts);
}

int main(int argc, char** argv)
{
    TestMainPlatformInit();
    jc_test_init(&argc, argv);
    return jc_test_run_all();
}
