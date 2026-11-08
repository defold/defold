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

#include "test_engine.h"
#include "../engine_service_private.h"
#include <resource/resource_test.hpp>
#include <dlib/atomic.h>
#include <dlib/http/http_client.h>
#include <dlib/mutex.h>
#include <dlib/socket.h>
#include <dlib/thread.h>
#include <dlib/time.h>
#include <dlib/testutil.h>
#include <dlib/uri.h>
#include <dlib/webserver.h>
#include <resource/resource_ddf.h>

TEST_F(EngineTest, ServiceInstanceNameSanitizesAddress)
{
    char out[64];
    dmEngineService::BuildServiceInstanceName("Local Host", "8001", 0, out, sizeof(out));
    ASSERT_STREQ("defold-local-host-8001", out);
}

TEST_F(EngineTest, SanitizeMDNSLabelReplacesLeadingDashWhenRequested)
{
    char out[64];
    dmEngineService::SanitizeMDNSLabel("-service-id", "defold", true, 'd', out, sizeof(out));
    ASSERT_STREQ("dservice-id", out);
}

TEST_F(EngineTest, ManufacturerModelNameOmitsDanglingSeparator)
{
    char out[64];
    dmEngineService::BuildManufacturerModelName("Acme", 0, out, sizeof(out));
    ASSERT_STREQ("Acme", out);

    dmEngineService::BuildManufacturerModelName(0, "Device", out, sizeof(out));
    ASSERT_STREQ("Device", out);
}

TEST_F(EngineTest, ServiceInstanceNameDropsAddressBeforeDanglingSeparator)
{
    char out[12];
    dmEngineService::BuildServiceInstanceName("very-long-local-address", "8001", 0, out, sizeof(out));
    ASSERT_STREQ("defold-8001", out);
}

TEST_F(EngineTest, ServiceInstanceNameAvoidsTrailingDashWithoutPort)
{
    char out[8];
    dmEngineService::BuildServiceInstanceName("very-long-local-address", 0, 0, out, sizeof(out));
    ASSERT_STREQ("defold", out);
}

TEST_F(EngineTest, ServiceInstanceNameIncludesSuffix)
{
    char out[64];
    dmEngineService::BuildServiceInstanceName("Local Host", "8001", "89abcdef", out, sizeof(out));
    ASSERT_STREQ("defold-local-host-8001-89abcdef", out);
}

TEST_F(EngineTest, ServiceInstanceNamePreservesSuffixWhenTruncatingAddress)
{
    char out[64];
    dmEngineService::BuildServiceInstanceName("very-long-local-address-name-for-discovery-target-node", "8001", "89abcdef", out, sizeof(out));
    ASSERT_STREQ("defold-very-long-local-address-name-for-discovery-8001-89abcdef", out);
}

TEST_F(EngineTest, DiscoveryIdentityUsesRoutableAddress)
{
    char out[128];
    dmEngineService::BuildDiscoveryIdentity("fe80::1", "editor-host", "Acme", "Device", "Linux", out, sizeof(out));
    ASSERT_STREQ("fe80::1", out);
}

TEST_F(EngineTest, DiscoveryIdentityFallsBackToHostnameForLoopback)
{
    char out[128];
    dmEngineService::BuildDiscoveryIdentity("127.0.0.1", "editor-host", "Acme", "Device", "Linux", out, sizeof(out));
    ASSERT_STREQ("editor-host", out);
}

TEST_F(EngineTest, DiscoveryIdentityFallsBackToManufacturerAndModel)
{
    char out[128];
    dmEngineService::BuildDiscoveryIdentity("localhost", "localhost", "Acme", "Device", "Android", out, sizeof(out));
    ASSERT_STREQ("Acme-Device", out);
}

TEST_F(EngineTest, DiscoveryRetryDelayBackoffCapsAtThirtySeconds)
{
    ASSERT_EQ(1000000ULL, dmEngineService::GetDiscoveryRetryDelayUsec(0));
    ASSERT_EQ(2000000ULL, dmEngineService::GetDiscoveryRetryDelayUsec(1));
    ASSERT_EQ(16000000ULL, dmEngineService::GetDiscoveryRetryDelayUsec(4));
    ASSERT_EQ(30000000ULL, dmEngineService::GetDiscoveryRetryDelayUsec(5));
    ASSERT_EQ(30000000ULL, dmEngineService::GetDiscoveryRetryDelayUsec(8));
}

#if !defined(__EMSCRIPTEN__)

// Deadlock guards only; tests wait for responses or explicit state changes.
static const uint64_t SERVICE_TEST_TIMEOUT = 60000000;

class EngineServiceOpenAPITest : public jc_test_params_class<bool>
{
public:
    dmEngineService::HEngineService m_Service;
    dmArray<char> m_Body;
    char m_ContentType[64];
    char m_Allow[32];
    char m_Connection[16];
    char m_CacheControl[32];
    int m_Status;
    int32_atomic_t m_Done;
    const char* m_Method;
    const char* m_Path;
    const char* m_RequestBody;
    uint32_t m_RequestBodySize;
    bool m_SendRequestBody;
    int32_t m_RequestPrefixSize;
    int m_ProbeStatus;
    dmHttpClient::Result m_ProbeResult;
    dmHttpClient::Result m_Result;
    dmEngine::HEngine m_PollEngine;
    uint32_t m_PollStepCount;
    uint64_t m_PollTime;
    uint64_t m_MaxPollTime;
    uint32_t m_PollCount;
    dmArray<dmSocket::Socket> m_RawClients;
    dmMutex::HMutex m_ClientMutex;
    dmThread::Thread m_Watchdog;
    int32_atomic_t m_StopWatchdog;
    int32_atomic_t m_WatchdogFired;
    uint64_t m_ClientDeadline;

    void SetUp()
    {
        m_RequestPrefixSize = -1;
        m_PollEngine = 0;
        m_PollStepCount = UINT32_MAX;
        m_PollTime = 0;
        m_MaxPollTime = 0;
        m_PollCount = 0;
        m_ClientMutex = dmMutex::New();
        m_Watchdog = 0;
        dmAtomicStore32(&m_StopWatchdog, 0);
        dmAtomicStore32(&m_WatchdogFired, 0);
        dmSocket::Initialize();
        m_Service = dmEngineService::New(0);
        ASSERT_NE((dmEngineService::HEngineService)0, m_Service);
        dmEngineService::OnEngineInit(m_Service);
        dmEngineService::EngineState state = {};
        state.m_ConnectionAppMode = true;
        dmEngineService::SetState(m_Service, &state);
    }

    void TearDown()
    {
        dmAtomicStore32(&m_StopWatchdog, 1);
        if (m_Watchdog)
        {
            dmThread::Join(m_Watchdog);
        }
        for (uint32_t i = 0; i < m_RawClients.Size(); ++i)
        {
            dmSocket::Delete(m_RawClients[i]);
        }
        dmMutex::Delete(m_ClientMutex);
        dmEngineService::Delete(m_Service);
        dmSocket::Finalize();
        ASSERT_EQ(0, dmAtomicGet32(&m_WatchdogFired));
    }

    static void Header(dmHttpClient::HResponse, void* context, int status, const char* key, const char* value)
    {
        EngineServiceOpenAPITest* self = (EngineServiceOpenAPITest*)context;
        self->m_Status = status;
        if (dmStrCaseCmp(key, "Content-Type") == 0)
        {
            dmStrlCpy(self->m_ContentType, value, sizeof(self->m_ContentType));
        }
        if (dmStrCaseCmp(key, "Allow") == 0)
        {
            dmStrlCpy(self->m_Allow, value, sizeof(self->m_Allow));
        }
        if (dmStrCaseCmp(key, "Connection") == 0)
        {
            dmStrlCpy(self->m_Connection, value, sizeof(self->m_Connection));
        }
        if (dmStrCaseCmp(key, "Cache-Control") == 0)
        {
            dmStrlCpy(self->m_CacheControl, value, sizeof(self->m_CacheControl));
        }
    }

    static void Content(dmHttpClient::HResponse, void* context, int status, const void* data, uint32_t size,
                        int32_t, uint32_t, uint32_t, uint32_t, const char*)
    {
        EngineServiceOpenAPITest* self = (EngineServiceOpenAPITest*)context;
        self->m_Status = status;
        if (size)
        {
            uint32_t offset = self->m_Body.Size();
            self->m_Body.SetCapacity(offset + size + 1);
            self->m_Body.SetSize(offset + size);
            memcpy(self->m_Body.Begin() + offset, data, size);
        }
    }

    static void Request(void* context)
    {
        EngineServiceOpenAPITest* self = (EngineServiceOpenAPITest*)context;
        char url[64];
        dmSnPrintf(url, sizeof(url), "http://127.0.0.1:%u", dmEngineService::GetPort(self->m_Service));
        dmURI::Parts uri;
        dmURI::Parse(url, &uri);
        dmHttpClient::NewParams params;
        params.m_Userdata = self;
        params.m_HttpHeader = Header;
        params.m_HttpContent = Content;
        params.m_RequestTimeout = SERVICE_TEST_TIMEOUT;
        params.m_MaxGetRetries = 1;
        params.m_HttpSendContentLength = ContentLength;
        params.m_HttpWrite = self->m_RequestPrefixSize < 0 ? Write : WritePartialBody;
        dmHttpClient::HClient client = dmHttpClient::New(&params, &uri);
        self->m_Result = dmHttpClient::Request(client, self->m_Method, self->m_Path);
        dmHttpClient::Delete(client);
        dmAtomicStore32(&self->m_Done, 1);
    }

    static uint32_t ContentLength(dmHttpClient::HResponse, void* context)
    {
        return ((EngineServiceOpenAPITest*)context)->m_RequestBodySize;
    }

    static dmHttpClient::Result Write(dmHttpClient::HResponse response, uint32_t offset, uint32_t length, void* context)
    {
        EngineServiceOpenAPITest* self = (EngineServiceOpenAPITest*)context;
        if (!self->m_SendRequestBody)
            return dmHttpClient::RESULT_OK;
        return dmHttpClient::Write(response, self->m_RequestBody + offset, length);
    }

    static void ProbeContent(dmHttpClient::HResponse, void* context, int status, const void*, uint32_t, int32_t, uint32_t, uint32_t, uint32_t, const char*)
    {
        *(int*)context = status;
    }

    static dmHttpClient::Result WritePartialBody(dmHttpClient::HResponse response, uint32_t offset, uint32_t length, void* context)
    {
        EngineServiceOpenAPITest* self = (EngineServiceOpenAPITest*)context;
        uint32_t prefix_size = self->m_RequestPrefixSize;
        dmHttpClient::Result written = dmHttpClient::Write(response, self->m_RequestBody + offset, prefix_size);
        if (written != dmHttpClient::RESULT_OK)
            return written;

        // Require another request to complete while this upload is incomplete.
        char url[64];
        dmSnPrintf(url, sizeof(url), "http://127.0.0.1:%u", dmEngineService::GetPort(self->m_Service));
        dmURI::Parts uri;
        dmURI::Parse(url, &uri);
        dmHttpClient::NewParams params;
        params.m_Userdata = &self->m_ProbeStatus;
        params.m_HttpContent = ProbeContent;
        params.m_RequestTimeout = SERVICE_TEST_TIMEOUT;
        params.m_MaxGetRetries = 1;
        dmHttpClient::HClient client = dmHttpClient::New(&params, &uri);
        self->m_ProbeResult = dmHttpClient::Request(client, "GET", "/ping");
        dmHttpClient::Delete(client);

        // Release a blocking handler even if the probe timed out, so the regression fails without hanging the suite.
        return dmHttpClient::Write(response, self->m_RequestBody + offset + prefix_size, length - prefix_size);
    }

    void Fetch(const char* method, const char* path, const char* body = "", bool send_body = true, uint32_t body_size = UINT32_MAX)
    {
        m_Method = method;
        m_Path = path;
        m_RequestBody = body;
        m_RequestBodySize = body_size == UINT32_MAX ? strlen(body) : body_size;
        m_SendRequestBody = send_body;
        m_ProbeStatus = 0;
        m_ProbeResult = dmHttpClient::RESULT_UNKNOWN;
        m_Status = 0;
        m_ContentType[0] = 0;
        m_Allow[0] = 0;
        m_Connection[0] = 0;
        m_CacheControl[0] = 0;
        m_Body.SetSize(0);
        m_PollTime = 0;
        m_MaxPollTime = 0;
        m_PollCount = 0;
        dmAtomicStore32(&m_Done, 0);
        dmThread::Thread client = dmThread::New(Request, 0x80000, this, "openapi-client");
        while (!dmAtomicGet32(&m_Done))
        {
            Poll();
        }
        dmThread::Join(client);
        if (m_Body.Full())
        {
            m_Body.OffsetCapacity(1);
        }
        m_Body.Push(0);
    }

    void Poll()
    {
        uint64_t start = dmTime::GetMonotonicTime();
        if (m_PollEngine)
        {
            if (m_PollStepCount == UINT32_MAX)
            {
                dmEngine::Step(m_PollEngine);
            }
            else
            {
                dmEngine::StepFrames(m_PollEngine, 1.0f / 60.0f, m_PollStepCount);
            }
        }
        else
        {
            dmEngineService::Update(m_Service, 0);
        }
        uint64_t elapsed = dmTime::GetMonotonicTime() - start;
        m_PollTime += elapsed;
        if (elapsed > m_MaxPollTime)
        {
            m_MaxPollTime = elapsed;
        }
        ++m_PollCount;
    }

    dmSocket::Socket OpenRequest(const char* request)
    {
        dmSocket::Address address;
        dmSocket::GetHostByName("127.0.0.1", &address);
        dmSocket::Socket socket;
        if (dmSocket::New(dmSocket::DOMAIN_IPV4, dmSocket::TYPE_STREAM, dmSocket::PROTOCOL_TCP, &socket) != dmSocket::RESULT_OK)
        {
            return dmSocket::INVALID_SOCKET_HANDLE;
        }
        if (dmSocket::Connect(socket, address, dmEngineService::GetPort(m_Service)) != dmSocket::RESULT_OK)
        {
            dmSocket::Delete(socket);
            return dmSocket::INVALID_SOCKET_HANDLE;
        }
        int sent;
        dmSocket::Send(socket, request, strlen(request), &sent);
        dmSocket::SetBlocking(socket, false);
        {
            DM_MUTEX_SCOPED_LOCK(m_ClientMutex);
            if (m_RawClients.Full())
            {
                m_RawClients.OffsetCapacity(16);
            }
            m_RawClients.Push(socket);
            m_ClientDeadline = dmTime::GetMonotonicTime() + SERVICE_TEST_TIMEOUT;
        }
        if (!m_Watchdog)
        {
            m_Watchdog = dmThread::New(Watchdog, 0x80000, this, "service-watchdog");
        }
        return socket;
    }

    static void Watchdog(void* context)
    {
        EngineServiceOpenAPITest* self = (EngineServiceOpenAPITest*)context;
        while (!dmAtomicGet32(&self->m_StopWatchdog))
        {
            {
                DM_MUTEX_SCOPED_LOCK(self->m_ClientMutex);
                if (!self->m_RawClients.Empty() && dmTime::GetMonotonicTime() >= self->m_ClientDeadline)
                {
                    // Only a regression watchdog: releasing withheld uploads lets
                    // a blocking handler return so the ordinary assertions fail.
                    dmAtomicStore32(&self->m_WatchdogFired, 1);
                    for (uint32_t i = 0; i < self->m_RawClients.Size(); ++i)
                    {
                        dmSocket::Shutdown(self->m_RawClients[i], dmSocket::SHUTDOWNTYPE_READWRITE);
                    }
                    return;
                }
            }
            dmTime::Sleep(1000);
        }
    }

    void CloseRequest(dmSocket::Socket socket)
    {
        DM_MUTEX_SCOPED_LOCK(m_ClientMutex);
        for (uint32_t i = 0; i < m_RawClients.Size(); ++i)
        {
            if (m_RawClients[i] == socket)
            {
                m_RawClients.EraseSwap(i);
                break;
            }
        }
        dmSocket::Shutdown(socket, dmSocket::SHUTDOWNTYPE_READWRITE);
        dmSocket::Delete(socket);
    }

    void ReadResponse(dmSocket::Socket socket)
    {
        m_Body.SetSize(0);
        m_Body.SetCapacity(65536);
        m_Body.Push(0);
        uint64_t deadline = dmTime::GetMonotonicTime() + SERVICE_TEST_TIMEOUT;
        while (!strstr(m_Body.Begin(), "\r\n0\r\n\r\n") && dmTime::GetMonotonicTime() < deadline)
        {
            Poll();
            char buffer[4096];
            int received = 0;
            dmSocket::Result result = dmSocket::Receive(socket, buffer, sizeof(buffer), &received);
            if (result == dmSocket::RESULT_TRY_AGAIN || result == dmSocket::RESULT_WOULDBLOCK)
            {
                continue;
            }
            if (result != dmSocket::RESULT_OK || !received)
            {
                break;
            }
            uint32_t offset = m_Body.Size() - 1;
            if (offset + received + 1 > m_Body.Capacity())
            {
                m_Body.OffsetCapacity(65536);
            }
            m_Body.SetSize(offset + received + 1);
            memcpy(m_Body.Begin() + offset, buffer, received);
            m_Body[m_Body.Size() - 1] = 0;
        }
        CloseRequest(socket);
        ASSERT_NE((const char*)0, strstr(m_Body.Begin(), "\r\n0\r\n\r\n"));
    }

    void WaitForResponse(dmSocket::Socket socket)
    {
        uint64_t deadline = dmTime::GetMonotonicTime() + SERVICE_TEST_TIMEOUT;
        bool ready = false;
        while (!ready && dmTime::GetMonotonicTime() < deadline)
        {
            Poll();
            dmSocket::Selector selector;
            dmSocket::SelectorSet(&selector, dmSocket::SELECTOR_KIND_READ, socket);
            ASSERT_EQ(dmSocket::RESULT_OK, dmSocket::Select(&selector, 0));
            ready = dmSocket::SelectorIsSet(&selector, dmSocket::SELECTOR_KIND_READ, socket);
        }
        ASSERT_TRUE(ready);
    }

    static void ExtensionHandler(void* context, dmWebServer::Request* request)
    {
        const char* body = (const char*)context;
        dmWebServer::SetStatusCode(request, 200);
        dmWebServer::SendAttribute(request, "Content-Type", "text/plain");
        dmWebServer::Send(request, body, strlen(body));
    }
};

// Existing flat response fields keep their types; clients can explicitly detect the observation contract.
TEST_F(EngineServiceOpenAPITest, ObservationContract)
{
    Fetch("GET", "/info");
    ASSERT_EQ(200, m_Status);
    ASSERT_STREQ("no-store", m_CacheControl);
    const char* fields[] = { "\"version\": \"", "\"platform\": \"", "\"sha1\": \"", "\"log_port\": \"", "\"service_api_version\": 1" };
    for (uint32_t i = 0; i < DM_ARRAY_SIZE(fields); ++i)
    {
        ASSERT_NE((const char*)0, strstr(m_Body.Begin(), fields[i]));
    }
    Fetch("GET", "/state");
    ASSERT_EQ(200, m_Status);
    ASSERT_STREQ("no-store", m_CacheControl);
    ASSERT_NE((const char*)0, strstr(m_Body.Begin(), "\"connection_mode\": true"));
    ASSERT_NE((const char*)0, strstr(m_Body.Begin(), "\"ready\": false"));
    ASSERT_NE((const char*)0, strstr(m_Body.Begin(), "\"frame\": 0"));
    ASSERT_NE((const char*)0, strstr(m_Body.Begin(), "\"engine_instance_id\": \""));
    ASSERT_EQ((const char*)0, strstr(m_Body.Begin(), "\"data\""));
    Fetch("GET", "/openapi.json");
    const char* schema_fields[] = { "\"service_api_version\"", "\"engine_instance_id\"", "\"ready\"", "\"frame\"", "4294967295", "\"nullable\": true" };
    for (uint32_t i = 0; i < DM_ARRAY_SIZE(schema_fields); ++i)
    {
        ASSERT_NE((const char*)0, strstr(m_Body.Begin(), schema_fields[i]));
    }
}

// Observation constraints hold with or without the profiler's fallback, which handles only unmatched paths.
TEST_P(EngineServiceOpenAPITest, ObservationValidation)
{
    if (GetParam())
    {
        dmEngineService::InitProfiler(m_Service, 0, 0);
    }
    const char* paths[] = { "/ping", "/info", "/state", "/openapi.json" };
    for (uint32_t i = 0; i < DM_ARRAY_SIZE(paths); ++i)
    {
        char path[64];
        dmSnPrintf(path, sizeof(path), "%s/other", paths[i]);
        Fetch("GET", path);
        ASSERT_EQ(GetParam() ? 200 : 404, m_Status);
        if (GetParam())
        {
            ASSERT_STREQ("text/html", m_ContentType);
        }
        dmSnPrintf(path, sizeof(path), "%sXYZ", paths[i]);
        Fetch("GET", path);
        ASSERT_EQ(GetParam() ? 200 : 404, m_Status);
        if (GetParam())
        {
            ASSERT_STREQ("text/html", m_ContentType);
        }
        dmSnPrintf(path, sizeof(path), "%s?wait=1", paths[i]);
        Fetch("GET", path);
        ASSERT_EQ(200, m_Status);
        Fetch("DELETE", paths[i]);
        ASSERT_EQ(405, m_Status);
        ASSERT_STREQ("GET", m_Allow);
        char request[256];
        dmSnPrintf(request, sizeof(request), "GET %s HTTP/1.1\r\nHost: localhost\r\nContent-Length: 10\r\n\r\n", paths[i]);
        dmSocket::Socket stalled = OpenRequest(request);
        ASSERT_NE(dmSocket::INVALID_SOCKET_HANDLE, stalled);
        Fetch("GET", "/state"); // Must complete before the upload is released.
        ASSERT_EQ(200, m_Status);
        ReadResponse(stalled);
        ASSERT_NE((const char*)0, strstr(m_Body.Begin(), "400 Bad Request"));
        ASSERT_NE((const char*)0, strstr(m_Body.Begin(), "Connection: close"));
    }
    Fetch("GET", "/unknown");
    ASSERT_EQ(GetParam() ? 200 : 404, m_Status);
    if (GetParam())
    {
        ASSERT_STREQ("text/html", m_ContentType);
    }
}

// Preserve profiler HTML for root and unmatched URLs without imposing observation method restrictions.
TEST_F(EngineServiceOpenAPITest, ProfilerFallbackCompatibility)
{
    dmEngineService::InitProfiler(m_Service, 0, 0);
    Fetch("GET", "/");
    ASSERT_EQ(200, m_Status);
    ASSERT_STREQ("text/html", m_ContentType);
    dmArray<char> html;
    html.SetCapacity(m_Body.Size());
    html.PushArray(m_Body.Begin(), m_Body.Size());

    const char* paths[] = { "/", "/unknown?value=1" };
    const char* methods[] = { "GET", "POST", "DELETE" };
    for (uint32_t i = 0; i < DM_ARRAY_SIZE(paths); ++i)
    {
        for (uint32_t j = 0; j < DM_ARRAY_SIZE(methods); ++j)
        {
            Fetch(methods[j], paths[i]);
            ASSERT_EQ(200, m_Status);
            ASSERT_STREQ("text/html", m_ContentType);
            ASSERT_STREQ(html.Begin(), m_Body.Begin());
        }
    }
}

// Wrap retains the instance, while replacement and teardown clear readiness and consume new identities.
TEST_F(EngineServiceOpenAPITest, ObservationLifetime)
{
    dmEngineService::EngineState state = { false, true, UINT32_MAX };
    dmEngineService::SetState(m_Service, &state);
    Fetch("GET", "/state");
    char previous[256];
    dmStrlCpy(previous, m_Body.Begin(), sizeof(previous));
    ASSERT_NE((const char*)0, strstr(previous, "\"frame\": 4294967295"));
    state.m_FrameCount = 0;
    dmEngineService::SetState(m_Service, &state);
    Fetch("GET", "/state");
    ASSERT_NE((const char*)0, strstr(m_Body.Begin(), "\"frame\": 0"));
    dmEngineService::OnEngineDelete(m_Service);
    Fetch("GET", "/state");
    ASSERT_STREQ("{\"connection_mode\": false, \"engine_instance_id\": null, \"ready\": false, \"frame\": null}", m_Body.Begin());
    for (uint32_t i = 0; i < 4; ++i)
    {
        dmEngineService::OnEngineInit(m_Service);
        Fetch("GET", "/state");
        ASSERT_NE(0, strcmp(previous, m_Body.Begin()));
        ASSERT_NE((const char*)0, strstr(m_Body.Begin(), "\"ready\": false"));
        dmStrlCpy(previous, m_Body.Begin(), sizeof(previous));
    }
}

// Measure owning-thread idle/first-request/saturated polling without treating host-dependent timings as assertions.
TEST_F(EngineServiceOpenAPITest, ObservationPollingCost)
{
    for (uint32_t i = 0; i < 1000; ++i)
    {
        Poll();
    }
    double idle = (double)m_PollTime / m_PollCount;
    Fetch("GET", "/state");
    ASSERT_EQ(200, m_Status);
    uint64_t first = m_MaxPollTime;
    dmSocket::Socket stalled[15];
    for (uint32_t i = 0; i < DM_ARRAY_SIZE(stalled); ++i)
    {
        stalled[i] = OpenRequest("GET /state HTTP/1.1\r\nHost:");
        ASSERT_NE(dmSocket::INVALID_SOCKET_HANDLE, stalled[i]);
    }
    Fetch("GET", "/state");
    ASSERT_EQ(200, m_Status);
    printf("M0 service poll (us): idle mean %.2f, first request max %llu, 15 stalled clients mean %.2f / max %llu\n",
           idle, (unsigned long long)first, (double)m_PollTime / m_PollCount, (unsigned long long)m_MaxPollTime);
    for (uint32_t i = 0; i < DM_ARRAY_SIZE(stalled); ++i)
    {
        CloseRequest(stalled[i]);
    }
}

class EngineServiceRuntimeTest : public EngineServiceOpenAPITest
{
    public:
    dmEngine::HEngine m_Engine;
    char              m_Snapshot[256];
    uint32_t          m_SlowRequests;

    void SetUp()
    {
        dmEngineInitialize();
        EngineServiceOpenAPITest::SetUp();
        m_Engine = dmEngine::New(m_Service);
        m_SlowRequests = 0;
    }

    void TearDown()
    {
        m_PollEngine = 0;
        dmEngine::SetRenderEnabled(true);
        dmEngine::SetUpdateEnabled(true);
        dmEngine::Delete(m_Engine);
        EngineServiceOpenAPITest::TearDown();
        dmEngineFinalize();
    }

    bool InitProject(bool fail_bootstrap = false)
    {
        char project[512];
        dmTestUtil::MakeHostPathf(project, sizeof(project), "%s/game.projectc", CONTENT_ROOT);
        const char* argv[] = { "test_engine", "--config=dmengine.unload_builtins=0",
                               "--config=bootstrap.main_collection=/service/runtime.collectionc",
                               fail_bootstrap ? "--config=test.fail_bootstrap=1" : "--config=test.fail_bootstrap=0", project };
        return dmEngine::Init(m_Engine, DM_ARRAY_SIZE(argv), (char**)argv);
    }

    void ExpectState(bool ready, uint32_t frame)
    {
        Fetch("GET", "/state");
        ASSERT_EQ(200, m_Status);
        ASSERT_NE((const char*)0, strstr(m_Body.Begin(), ready ? "\"ready\": true" : "\"ready\": false"));
        char expected[48];
        dmSnPrintf(expected, sizeof(expected), "\"frame\": %u}", frame);
        ASSERT_NE((const char*)0, strstr(m_Body.Begin(), expected));
    }

    static void SlowResponse(void* context, dmWebServer::Request* request)
    {
        EngineServiceRuntimeTest* self = (EngineServiceRuntimeTest*)context;
        dmWebServer::Send(request, self->m_Snapshot, strlen(self->m_Snapshot));
        char padding[65536];
        memset(padding, 'x', sizeof(padding));
        for (uint32_t i = 0; i < 64; ++i)
        {
            dmWebServer::Send(request, padding, sizeof(padding));
        }
        ++self->m_SlowRequests;
    }
};

// An unread large response must allow engine frames, observations and reboot to complete.
TEST_F(EngineServiceRuntimeTest, SlowReaderAcrossReboot)
{
    ASSERT_TRUE(InitProject());
    ExpectState(true, 0);
    dmStrlCpy(m_Snapshot, m_Body.Begin(), sizeof(m_Snapshot));
    dmWebServer::HandlerParams handler;
    handler.m_Handler = SlowResponse;
    handler.m_Userdata = this;
    ASSERT_EQ(dmWebServer::RESULT_OK, dmWebServer::AddHandler(dmEngineService::GetWebServer(m_Service), "/test/slow", &handler));
    dmSocket::Socket slow = OpenRequest("GET /test/slow HTTP/1.1\r\nHost: localhost\r\n\r\n");
    ASSERT_NE(dmSocket::INVALID_SOCKET_HANDLE, slow);
    m_PollEngine = m_Engine;
    uint64_t deadline = dmTime::GetMonotonicTime() + SERVICE_TEST_TIMEOUT;
    while (!m_SlowRequests && dmTime::GetMonotonicTime() < deadline)
    {
        Poll();
    }
    ASSERT_EQ(1U, m_SlowRequests);
    uint32_t frame = m_Engine->m_Stats.m_FrameCount;
    Fetch("GET", "/state");
    ASSERT_EQ(200, m_Status);
    ASSERT_GT(m_Engine->m_Stats.m_FrameCount, frame);
    m_PollEngine = 0;
    dmEngine::Delete(m_Engine);
    m_Engine = dmEngine::New(m_Service);
    ASSERT_TRUE(InitProject());
    ReadResponse(slow);
    ASSERT_NE((const char*)0, strstr(m_Body.Begin(), m_Snapshot));
    ExpectState(true, 0);
    ASSERT_NE(0, strcmp(m_Snapshot, m_Body.Begin()));
}

// Publish actual bootstrap success, preserve it through hot reload, and reset identity across engine replacement.
TEST_F(EngineServiceRuntimeTest, ProjectLifecycle)
{
    ASSERT_TRUE(InitProject());
    ExpectState(true, 0);
    char previous[256];
    dmStrlCpy(previous, m_Body.Begin(), sizeof(previous));
    ResourceDescriptor* descriptor;
    ASSERT_EQ(dmResource::RESULT_OK, dmResource::ReloadResource(m_Engine->m_Factory, "/service/runtime.scriptc", &descriptor));
    ExpectState(true, 0);
    ASSERT_STREQ(previous, m_Body.Begin());

    dmSocket::Socket pending = OpenRequest("GET /state HTTP/1.1\r\nHost: localhost\r\n\r\n");
    ASSERT_NE(dmSocket::INVALID_SOCKET_HANDLE, pending);
    WaitForResponse(pending); // Serialization completed before replacement; the response remains unread.
    dmEngine::Delete(m_Engine);
    m_Engine = dmEngine::New(m_Service);
    ASSERT_TRUE(InitProject());
    ReadResponse(pending);
    ASSERT_NE((const char*)0, strstr(m_Body.Begin(), previous));
    ExpectState(true, 0);
    ASSERT_NE(0, strcmp(previous, m_Body.Begin()));
}

// A bootstrap script error keeps readiness false without changing the engine's existing startup policy.
TEST_F(EngineServiceRuntimeTest, BootstrapFailure)
{
    ASSERT_TRUE(InitProject(true));
    ExpectState(false, 0);
    dmEngine::Step(m_Engine);
    ExpectState(false, 0);
    ASSERT_EQ(1U, m_Engine->m_Stats.m_FrameCount);
}

// Connection mode and failed initialization attempts never claim project readiness.
TEST_F(EngineServiceRuntimeTest, ConnectionAndInitializationFailure)
{
    const char* connect[] = { "test_engine", "--config=dmengine.unload_builtins=0" };
    ASSERT_TRUE(dmEngine::Init(m_Engine, DM_ARRAY_SIZE(connect), (char**)connect));
    ExpectState(false, 0);
    ASSERT_NE((const char*)0, strstr(m_Body.Begin(), "\"connection_mode\": true"));
    dmEngine::Delete(m_Engine);
    m_Engine = dmEngine::New(m_Service);
    const char* invalid[] = { "test_engine", "missing-m0-project.projectc" };
    ASSERT_FALSE(dmEngine::Init(m_Engine, DM_ARRAY_SIZE(invalid), (char**)invalid));
    ExpectState(false, 0);
    ASSERT_NE((const char*)0, strstr(m_Body.Begin(), "\"engine_instance_id\": \""));
}

// Samples exclude the in-flight frame, retain 32-bit wrap, and advance for complete render-disabled updates.
TEST_F(EngineServiceRuntimeTest, AuthoritativeFrames)
{
    ASSERT_TRUE(InitProject());
    dmEngine::SetRenderEnabled(false);
    m_Engine->m_Stats.m_FrameCount = UINT32_MAX;
    dmEngine::Step(m_Engine);
    ASSERT_EQ(0U, m_Engine->m_Stats.m_FrameCount);
    ExpectState(true, UINT32_MAX);
    dmEngine::Step(m_Engine);
    ASSERT_EQ(1U, m_Engine->m_Stats.m_FrameCount);
    ExpectState(true, 0);
    ExpectState(true, 0); // Observation alone does not advance the counter.
    ASSERT_EQ(1U, m_Engine->m_Stats.m_FrameCount);
}

// Throttled and externally skipped iterations still service requests, including a stalled discovery upload.
TEST_F(EngineServiceRuntimeTest, SkippedSimulationPolling)
{
    ASSERT_TRUE(InitProject());
    // Consume the null HID backend's initial connection/input notifications.
    dmEngine::Step(m_Engine);
    dmEngine::Step(m_Engine);
    dmEngine::SetEngineThrottle(m_Engine, true, 0);
    m_Engine->m_ThrottleCooldown = 1;
    m_PollEngine = m_Engine;
    ExpectState(true, 2);
    dmSocket::Socket stalled = OpenRequest("GET /openapi.json HTTP/1.1\r\nContent-Length: 1024\r\n\r\n");
    ASSERT_NE(dmSocket::INVALID_SOCKET_HANDLE, stalled);
    ExpectState(true, 2);
    ReadResponse(stalled);
    ASSERT_NE((const char*)0, strstr(m_Body.Begin(), "400 Bad Request"));
    ASSERT_EQ(2U, m_Engine->m_Stats.m_FrameCount);
    Fetch("GET", "/resources_data");
    ASSERT_EQ(200, m_Status);
#if defined(DM_PLATFORM_MACOS)
    Fetch("POST", "/profiler/gputrace", "{\"path\":\"/tmp/m0-unused.gputrace\"}");
    ASSERT_EQ(501, m_Status); // Null graphics retains its rejection outside a render phase.
#endif
    dmEngine::SetEngineThrottle(m_Engine, false, 0);
    dmEngine::SetUpdateEnabled(false);
    ExpectState(true, 2);
    dmEngine::SetUpdateEnabled(true);
    m_PollEngine = 0;
    dmEngine::Step(m_Engine);
    ASSERT_EQ(3U, m_Engine->m_Stats.m_FrameCount);
}

// Explicit step counts exercise zero-step polling and multi-step publication on every platform without a clock race.
TEST_F(EngineServiceRuntimeTest, ScheduledStepsPolling)
{
    ASSERT_TRUE(InitProject());
    m_PollEngine = m_Engine;
    m_PollStepCount = 0;
    ExpectState(true, 0);
    ASSERT_EQ(0U, m_Engine->m_Stats.m_FrameCount);
    m_PollEngine = 0;
    dmEngine::StepFrames(m_Engine, 1.0f / 60.0f, 2);
    ASSERT_EQ(2U, m_Engine->m_Stats.m_FrameCount);
    ExpectState(true, 1);
}

class EngineServiceReloadTest : public EngineServiceOpenAPITest
{
public:
    dmMessage::HSocket m_ResourceSocket;
    dmResourceDDF::Reload* m_ExpectedReload;

    void SetUp()
    {
        m_ExpectedReload = 0;
        EngineServiceOpenAPITest::SetUp();
        ASSERT_EQ(dmMessage::RESULT_OK, dmMessage::NewSocket("@resource", &m_ResourceSocket));
    }

    void TearDown()
    {
        ASSERT_EQ(dmMessage::RESULT_OK, dmMessage::DeleteSocket(m_ResourceSocket));
        EngineServiceOpenAPITest::TearDown();
    }

    static void CheckReload(dmMessage::Message* message, void* context)
    {
        EngineServiceReloadTest* self = (EngineServiceReloadTest*)context;
        ASSERT_EQ((uintptr_t)dmResourceDDF::Reload::m_DDFDescriptor, message->m_Descriptor);
        ASSERT_EQ(0U, message->m_DataSize);
        ASSERT_NE((uintptr_t)0, message->m_UserData1);
        ASSERT_NE((dmResourceDDF::Reload*)0, self->m_ExpectedReload);
        dmResourceDDF::Reload* reload = (dmResourceDDF::Reload*)message->m_UserData1;
        ASSERT_EQ(self->m_ExpectedReload->m_Resources.m_Count, reload->m_Resources.m_Count);
        const char* base = (const char*)reload;
        const uintptr_t* offsets = (const uintptr_t*)(base + (uintptr_t)reload->m_Resources.m_Data);
        for (uint32_t i = 0; i < reload->m_Resources.m_Count; ++i)
            ASSERT_STREQ(self->m_ExpectedReload->m_Resources[i], base + offsets[i]);
    }
};

// Reloads larger than both the old 1 KiB HTTP limit and a message page must
// arrive as one batch with every path in order, including uploads above 64 KiB.
TEST_F(EngineServiceReloadTest, LargeReloadBatches)
{
    const uint32_t counts[] = { 0, 1, 40, 256, 4096 };
    dmArray<char> paths;
    paths.SetCapacity(4096 * 64);
    paths.SetSize(paths.Capacity());
    dmArray<const char*> names;
    names.SetCapacity(4096);
    for (uint32_t i = 0; i < names.Capacity(); ++i)
    {
        char* path = paths.Begin() + i * 64;
        dmSnPrintf(path, 64, "/assets/data/item-%04u.json", i);
        names.Push(path);
    }
    dmResourceDDF::Reload reload = {};
    reload.m_Resources.m_Data = names.Begin();
    m_ExpectedReload = &reload;
    for (uint32_t i = 0; i < DM_ARRAY_SIZE(counts); ++i)
    {
        reload.m_Resources.m_Count = counts[i];
        dmArray<uint8_t> encoded;
        ASSERT_EQ(dmDDF::RESULT_OK, dmDDF::SaveMessageToArray(&reload, dmResourceDDF::Reload::m_DDFDescriptor, encoded));
        if (counts[i] == 40)
            ASSERT_GT(encoded.Size(), 1024U);
        if (counts[i] == 4096)
            ASSERT_GT(encoded.Size(), 64U * 1024U);
        Fetch("POST", "/post/@resource/reload", (const char*)encoded.Begin(), true, encoded.Size());
        ASSERT_EQ(dmHttpClient::RESULT_OK, m_Result);
        ASSERT_EQ(200, m_Status);
        ASSERT_EQ(1U, dmMessage::Dispatch(m_ResourceSocket, CheckReload, this));
    }
}

// A malformed reload must fail before posting anything and must not wait for
// more body bytes after the complete, invalid payload has already been read.
TEST_F(EngineServiceReloadTest, InvalidReload)
{
    const char truncated[] = { 0x0a, 0x05, 'a' };
    Fetch("POST", "/post/@resource/reload", truncated, true, sizeof(truncated));
    ASSERT_EQ(dmHttpClient::RESULT_NOT_200_OK, m_Result);
    ASSERT_EQ(400, m_Status);
    ASSERT_STREQ("Invalid message", m_Body.Begin());
    ASSERT_EQ(0U, dmMessage::Dispatch(m_ResourceSocket, CheckReload, this));
}

// Reject oversized reload headers without allocating or waiting for the body.
TEST_F(EngineServiceReloadTest, ReloadBodyLimit)
{
    Fetch("POST", "/post/@resource/reload", "", false, 64U * 1024U * 1024U + 1);
    ASSERT_EQ(dmHttpClient::RESULT_NOT_200_OK, m_Result);
    ASSERT_EQ(400, m_Status);
    ASSERT_STREQ("close", m_Connection);
    ASSERT_STREQ("Too large message", m_Body.Begin());
    ASSERT_EQ(0U, dmMessage::Dispatch(m_ResourceSocket, CheckReload, this));
}

// The actual discovery endpoint must expose all built-ins and take precedence over the profiler's root fallback.
TEST_F(EngineServiceOpenAPITest, BuiltinDocument)
{
    dmEngineService::InitProfiler(m_Service, 0, 0);
    dmEngineService::InitProfiler(m_Service, 0, 0); // Reboot reuses the service.
    Fetch("GET", "/openapi.json");
    ASSERT_EQ(dmHttpClient::RESULT_OK, m_Result);
    ASSERT_EQ(200, m_Status);
    ASSERT_STREQ("application/json", m_ContentType);
    ASSERT_NE((const char*)0, strstr(m_Body.Begin(), "\"openapi\":\"3.0.3\""));
    const char* paths[] = {"/ping", "/info", "/state", "/openapi.json", "/scene_graph", "/resources_data", "/gameobjects_data", "/",
        "/post/{socket}/{message_type}", "/post/@system/reboot", "/post/@system/exit", "/post/@system/run_script",
        "/post/@render/resize", "/post/@resource/reload"};
    for (uint32_t i = 0; i < DM_ARRAY_SIZE(paths); ++i)
    {
        char key[128];
        dmSnPrintf(key, sizeof(key), "\"%s\":", paths[i]);
        const char* occurrence = strstr(m_Body.Begin(), key);
        ASSERT_NE((const char*)0, occurrence);
        ASSERT_EQ((const char*)0, strstr(occurrence + strlen(key), key));
    }
    Fetch("GET", "/");
    ASSERT_EQ(200, m_Status);
    ASSERT_STREQ("text/html", m_ContentType);
}

// Register and advertise GPU tracing only when its platform implementation is built.
TEST_F(EngineServiceOpenAPITest, GPUTraceRegistration)
{
    dmEngineService::InitProfiler(m_Service, 0, 0);
    dmEngineService::InitProfiler(m_Service, 0, 0); // Reboot reuses the service.
    Fetch("GET", "/openapi.json");
    ASSERT_EQ(200, m_Status);
    const char* key = "\"/profiler/gputrace\":";
    const char* occurrence = strstr(m_Body.Begin(), key);
    dmWebServer::HServer server = dmEngineService::GetWebServer(m_Service);
#if defined(DM_PLATFORM_MACOS)
    ASSERT_NE((const char*)0, occurrence);
    ASSERT_EQ((const char*)0, strstr(occurrence + strlen(key), key));
    ASSERT_EQ(dmWebServer::RESULT_OK, dmWebServer::RemoveHandler(server, "/profiler/gputrace"));
#else
    ASSERT_EQ((const char*)0, occurrence);
    ASSERT_EQ(dmWebServer::RESULT_HANDLER_NOT_REGISTRED, dmWebServer::RemoveHandler(server, "/profiler/gputrace"));
#endif
}

#if defined(DM_PLATFORM_MACOS)
// GPU trace requests reach the service even with the profiler's HTML fallback and a null graphics adapter.
TEST_F(EngineServiceOpenAPITest, GPUTraceUnsupported)
{
    dmEngineService::InitProfiler(m_Service, 0, 0);
    const char* methods[] = { "GET", "POST", "DELETE" };
    for (uint32_t i = 0; i < DM_ARRAY_SIZE(methods); ++i)
    {
        Fetch(methods[i], "/profiler/gputrace", "{\"path\":\"/tmp/frame.gputrace\"}");
        ASSERT_EQ(501, m_Status);
        ASSERT_STREQ("application/json", m_ContentType);
        ASSERT_NE((const char*)0, strstr(m_Body.Begin(), "\"code\":\"gputrace_unsupported\""));
    }
}

// Prefix routing must reject unknown GPU trace paths and methods instead of accepting a capture request.
TEST_F(EngineServiceOpenAPITest, GPUTraceRouting)
{
    dmEngineService::InitProfiler(m_Service, 0, 0);
    Fetch("GET", "/profiler/gputrace/unknown");
    ASSERT_EQ(404, m_Status);
    Fetch("GET", "/profiler/gputrace_other");
    ASSERT_EQ(404, m_Status);
    Fetch("PUT", "/profiler/gputrace", "{}");
    ASSERT_EQ(405, m_Status);
    ASSERT_STREQ("GET, POST, DELETE", m_Allow);
    Fetch("GET", "/profiler/gputrace?unused=value");
    ASSERT_EQ(501, m_Status);
}

// The size limit includes headers; oversized requests are rejected even when only headers arrive.
TEST_F(EngineServiceOpenAPITest, GPUTraceBodyLimit)
{
    dmEngineService::InitProfiler(m_Service, 0, 0);
    uint32_t body_sizes[] = { 65535, 65537 };
    for (uint32_t i = 0; i < DM_ARRAY_SIZE(body_sizes); ++i)
    {
        dmArray<char> body;
        body.SetCapacity(body_sizes[i] + 1);
        body.SetSize(body.Capacity());
        memset(body.Begin(), ' ', body_sizes[i]);
        body[body_sizes[i]] = 0;
        for (uint32_t send_body = 0; send_body < 2; ++send_body)
        {
            Fetch("POST", "/profiler/gputrace", body.Begin(), send_body != 0);
            ASSERT_EQ(413, m_Status);
            ASSERT_STREQ("close", m_Connection);
            ASSERT_NE((const char*)0, strstr(m_Body.Begin(), "\"code\":\"request_too_large\""));
            Fetch("GET", "/ping");
            ASSERT_EQ(200, m_Status);
            ASSERT_STREQ("PONG\n", m_Body.Begin());
        }
    }
}

// Headers-only and partial uploads must allow other requests to finish, then resume when the remaining body arrives.
TEST_F(EngineServiceOpenAPITest, GPUTraceIncompleteBody)
{
    dmEngineService::InitProfiler(m_Service, 0, 0);
    dmArray<char> large_body;
    large_body.SetCapacity(65001);
    large_body.SetSize(large_body.Capacity());
    memset(large_body.Begin(), ' ', large_body.Size() - 1);
    large_body[large_body.Size() - 1] = 0;
    const char* bodies[] = { " ", "{}", large_body.Begin() };
    for (uint32_t prefix_size = 0; prefix_size < DM_ARRAY_SIZE(bodies); ++prefix_size)
    {
        m_RequestPrefixSize = prefix_size;
        Fetch("POST", "/profiler/gputrace", bodies[prefix_size]);
        ASSERT_EQ(dmHttpClient::RESULT_OK, m_ProbeResult);
        ASSERT_EQ(200, m_ProbeStatus);
        ASSERT_EQ(501, m_Status);
        ASSERT_NE((const char*)0, strstr(m_Body.Begin(), "\"code\":\"gputrace_unsupported\""));
    }
    m_RequestPrefixSize = -1;
    Fetch("GET", "/ping");
    ASSERT_EQ(200, m_Status);
    ASSERT_STREQ("PONG\n", m_Body.Begin());
}
#endif

// Extension requests and metadata must survive either profiler initialization order and removal/re-registration.
TEST_P(EngineServiceOpenAPITest, ExtensionLifecycle)
{
    static const char json[] = "{\"/extension/test\":{\"get\":{\"responses\":{\"200\":{\"description\":\"Extension response\"}}}}}";
    static const char response[] = "Extension response";
    dmWebServer::HandlerParams params;
    params.m_Handler = ExtensionHandler;
    params.m_Userdata = (void*)response;
    dmWebServer::HServer server = dmEngineService::GetWebServer(m_Service);
    if (GetParam())
        dmEngineService::InitProfiler(m_Service, 0, 0);
    ASSERT_EQ(dmWebServer::RESULT_OK, dmWebServer::AddHandler(server, "/extension", &params, json));

    Fetch("GET", "/extension/test");
    ASSERT_EQ(dmHttpClient::RESULT_OK, m_Result);
    ASSERT_EQ(200, m_Status);
    ASSERT_STREQ("text/plain", m_ContentType);
    ASSERT_STREQ(response, m_Body.Begin());

    dmEngineService::InitProfiler(m_Service, 0, 0);
    for (uint32_t registration = 0; registration < 2; ++registration)
    {
        if (registration != 0)
            ASSERT_EQ(dmWebServer::RESULT_OK, dmWebServer::AddHandler(server, "/extension", &params, json));

        Fetch("GET", "/openapi.json");
        ASSERT_EQ(dmHttpClient::RESULT_OK, m_Result);
        ASSERT_EQ(200, m_Status);
        ASSERT_STREQ("application/json", m_ContentType);
        ASSERT_NE((const char*)0, strstr(m_Body.Begin(), "\"/extension/test\""));

        Fetch("GET", "/extension/test");
        ASSERT_EQ(dmHttpClient::RESULT_OK, m_Result);
        ASSERT_EQ(200, m_Status);
        ASSERT_STREQ("text/plain", m_ContentType);
        ASSERT_STREQ(response, m_Body.Begin());

        ASSERT_EQ(dmWebServer::RESULT_OK, dmWebServer::RemoveHandler(server, "/extension"));
        Fetch("GET", "/openapi.json");
        ASSERT_EQ(dmHttpClient::RESULT_OK, m_Result);
        ASSERT_EQ(200, m_Status);
        ASSERT_STREQ("application/json", m_ContentType);
        ASSERT_EQ((const char*)0, strstr(m_Body.Begin(), "\"/extension/test\""));

        Fetch("GET", "/extension/test");
        ASSERT_EQ(dmHttpClient::RESULT_OK, m_Result);
        ASSERT_EQ(200, m_Status);
        ASSERT_STREQ("text/html", m_ContentType);

        // Removing an extension may move the root fallback ahead of the other profiler handlers.
        Fetch("GET", "/scene_graph");
        ASSERT_EQ(500, m_Status);
        ASSERT_STREQ("text/plain", m_ContentType);
        ASSERT_STREQ("Profiler state is not initialized", m_Body.Begin());

        dmEngineService::InitProfiler(m_Service, 0, 0); // Reboot reuses the service.
    }

    Fetch("GET", "/");
    ASSERT_EQ(dmHttpClient::RESULT_OK, m_Result);
    ASSERT_EQ(200, m_Status);
    ASSERT_STREQ("text/html", m_ContentType);
}

static const bool profiler_initialization_orders[] = {false, true};
INSTANTIATE_TEST_CASE_P(ProfilerInitializationOrder, EngineServiceOpenAPITest, jc_test_values_in(profiler_initialization_orders));

// Both registration APIs must serve undocumented extensions through HTTP without changing discovery.
TEST_F(EngineServiceOpenAPITest, UndocumentedExtensionLifecycle)
{
    static const char response[] = "Undocumented extension response";
    dmWebServer::HandlerParams params;
    params.m_Handler = ExtensionHandler;
    params.m_Userdata = (void*)response;
    dmWebServer::HServer server = dmEngineService::GetWebServer(m_Service);
    dmEngineService::InitProfiler(m_Service, 0, 0);
    Fetch("GET", "/openapi.json");
    ASSERT_EQ(200, m_Status);
    dmArray<char> original;
    original.SetCapacity(m_Body.Size());
    original.PushArray(m_Body.Begin(), m_Body.Size());

    for (uint32_t registration = 0; registration < 2; ++registration)
    {
        dmWebServer::Result result = registration == 0
            ? dmWebServer::AddHandler(server, "/undocumented", &params)
            : dmWebServer::AddHandler(server, "/undocumented", &params, 0);
        ASSERT_EQ(dmWebServer::RESULT_OK, result);
        Fetch("GET", "/undocumented");
        ASSERT_EQ(dmHttpClient::RESULT_OK, m_Result);
        ASSERT_EQ(200, m_Status);
        ASSERT_STREQ(response, m_Body.Begin());
        Fetch("GET", "/openapi.json");
        ASSERT_EQ(200, m_Status);
        ASSERT_STREQ(original.Begin(), m_Body.Begin());

        ASSERT_EQ(dmWebServer::RESULT_OK, dmWebServer::RemoveHandler(server, "/undocumented"));
        Fetch("GET", "/undocumented");
        ASSERT_EQ(200, m_Status);
        ASSERT_STREQ("text/html", m_ContentType);
        Fetch("GET", "/openapi.json");
        ASSERT_EQ(200, m_Status);
        ASSERT_STREQ(original.Begin(), m_Body.Begin());
    }
}

// Overlapping extension prefixes must select the most specific handler regardless of registration and removal order.
TEST_F(EngineServiceOpenAPITest, MostSpecificHandler)
{
    static const char parent_json[] = "{\"/extension\":{\"get\":{\"responses\":{\"200\":{\"description\":\"Parent response\"}}}}}";
    static const char child_json[] = "{\"/extension/child\":{\"get\":{\"responses\":{\"200\":{\"description\":\"Child response\"}}}}}";
    static const char parent_response[] = "Parent response";
    static const char child_response[] = "Child response";
    dmWebServer::HandlerParams params;
    params.m_Handler = ExtensionHandler;
    params.m_Userdata = (void*)parent_response;
    dmWebServer::HServer server = dmEngineService::GetWebServer(m_Service);
    ASSERT_EQ(dmWebServer::RESULT_OK, dmWebServer::AddHandler(server, "/extension", &params, parent_json));
    params.m_Userdata = (void*)child_response;
    ASSERT_EQ(dmWebServer::RESULT_OK, dmWebServer::AddHandler(server, "/extension/child", &params, child_json));
    dmEngineService::InitProfiler(m_Service, 0, 0);

    Fetch("GET", "/extension/child");
    ASSERT_EQ(dmHttpClient::RESULT_OK, m_Result);
    ASSERT_STREQ(child_response, m_Body.Begin());

    ASSERT_EQ(dmWebServer::RESULT_OK, dmWebServer::RemoveHandler(server, "/extension"));
    Fetch("GET", "/extension/child");
    ASSERT_EQ(dmHttpClient::RESULT_OK, m_Result);
    ASSERT_STREQ(child_response, m_Body.Begin());

    params.m_Userdata = (void*)parent_response;
    ASSERT_EQ(dmWebServer::RESULT_OK, dmWebServer::AddHandler(server, "/extension", &params, parent_json));
    Fetch("GET", "/extension/child");
    ASSERT_EQ(dmHttpClient::RESULT_OK, m_Result);
    ASSERT_STREQ(child_response, m_Body.Begin());

    ASSERT_EQ(dmWebServer::RESULT_OK, dmWebServer::RemoveHandler(server, "/extension/child"));
    Fetch("GET", "/extension/child");
    ASSERT_EQ(dmHttpClient::RESULT_OK, m_Result);
    ASSERT_STREQ(parent_response, m_Body.Begin());
}

// Discovery must reject unsupported methods and avoid serving its JSON for unrelated prefix matches.
TEST_F(EngineServiceOpenAPITest, DiscoveryRequestValidation)
{
    Fetch("DELETE", "/openapi.json");
    ASSERT_EQ(405, m_Status);
    ASSERT_STREQ("GET", m_Allow);
    Fetch("GET", "/openapi.json/other");
    ASSERT_EQ(404, m_Status);
    Fetch("GET", "/info");
    ASSERT_EQ(200, m_Status);
    ASSERT_STREQ("application/json", m_ContentType);
    Fetch("GET", "/state");
    ASSERT_EQ(200, m_Status);
    ASSERT_STREQ("application/json", m_ContentType);
}

class EngineServiceResourceTest : public EngineServiceOpenAPITest
{
public:
    dmResource::HFactory    m_Factory;
    dmGameObject::HRegister m_Register;
    void*                   m_Resource;
    HResourceDescriptor     m_Descriptor;
    dmResource::LoadTimings m_Timings;

    void SetUp()
    {
        m_Factory = 0;
        m_Register = 0;
        m_Resource = 0;
        EngineServiceOpenAPITest::SetUp();
        if (!m_Service)
            return;
        m_Register = dmGameObject::NewRegister();
        dmResource::NewFactoryParams params;
        m_Factory = dmResource::NewFactory(&params, ".");
        ASSERT_NE((dmResource::HFactory)0, m_Factory);
        ASSERT_EQ(dmResource::RESULT_OK, dmResource::RegisterType(m_Factory, "profile", 0, 0, Create, 0, Destroy, 0));
        uint32_t data = 42;
        ASSERT_EQ(dmResource::RESULT_OK, dmResource::CreateResource(m_Factory, "/test.profile", &data, sizeof(data), &m_Resource));
        ASSERT_EQ(dmResource::RESULT_OK, dmResource::GetDescriptorWithExt(m_Factory, dmHashString64("/test.profile"), 0, 0, &m_Descriptor));

        // Seed distinct measurements to verify field order and width without relying on clock resolution.
        dmResource::LoadTimings timings = { UINT32_MAX, 0x80000000U, 3, 5, 7, 11, 0x7fffffe5U, 17, true };
        m_Timings = timings;
        dmResource::TestSetLoadTimings(m_Descriptor, m_Timings);
        dmEngineService::InitProfiler(m_Service, m_Factory, m_Register);
    }

    void TearDown()
    {
        EngineServiceOpenAPITest::TearDown();
        if (m_Resource)
            dmResource::Release(m_Factory, m_Resource);
        if (m_Factory)
            dmResource::DeleteFactory(m_Factory);
        if (m_Register)
            dmGameObject::DeleteRegister(m_Register);
    }

    static dmResource::Result Create(const dmResource::ResourceCreateParams* params)
    {
        ResourceDescriptorSetResource(params->m_Resource, new uint32_t(42));
        ResourceDescriptorSetResourceSize(params->m_Resource, 256);
        return dmResource::RESULT_OK;
    }

    static dmResource::Result Destroy(const dmResource::ResourceDestroyParams* params)
    {
        delete (uint32_t*)ResourceDescriptorGetResource(params->m_Resource);
        return dmResource::RESULT_OK;
    }

    void ExpectBytes(uint32_t* offset, const void* expected, uint32_t size)
    {
        ASSERT_LE(*offset + size, m_Body.Size() - 1);
        ASSERT_ARRAY_EQ_LEN((const char*)expected, m_Body.Begin() + *offset, size);
        *offset += size;
    }

    void ExpectString(uint32_t* offset, const char* expected)
    {
        uint16_t length = (uint16_t)strlen(expected);
        ExpectBytes(offset, &length, sizeof(length));
        ExpectBytes(offset, expected, length);
    }

    void ExpectResponse(bool include_timings)
    {
        ASSERT_EQ(dmHttpClient::RESULT_OK, m_Result);
        ASSERT_EQ(200, m_Status);
        ASSERT_STREQ("application/octet-stream", m_ContentType);
        uint32_t offset = 0;
        ExpectString(&offset, include_timings ? "RES2" : "RESS");
        ExpectString(&offset, "/test.profile");
        ExpectString(&offset, ".profile");
        uint32_t sizes_and_refs[] = { 256, sizeof(uint32_t), 1 };
        ExpectBytes(&offset, sizes_and_refs, sizeof(sizes_and_refs));
        if (include_timings)
        {
            const dmResource::LoadTimings& timings = m_Timings;
            uint32_t                       recorded = timings.m_Recorded ? 1 : 0;
            uint64_t                       durations[] = { timings.m_Total, timings.m_Preload, timings.m_Read, timings.m_Create, timings.m_PostCreate, timings.m_Queue, timings.m_Wait };
            ExpectBytes(&offset, &recorded, sizeof(recorded));
            ExpectBytes(&offset, durations, sizeof(durations));
            ExpectBytes(&offset, &timings.m_PostCreateCount, sizeof(timings.m_PostCreateCount));
        }
        ASSERT_EQ(offset + 1, m_Body.Size()); // Fetch appends a trailing zero outside the HTTP body.
    }
};

// Existing clients must receive the unchanged RESS layout even when timing data is available.
TEST_F(EngineServiceResourceTest, LegacyResources)
{
    Fetch("GET", "/resources_data");
    ExpectResponse(false);
}

// RES2 must zero-extend 32-bit durations through UINT32_MAX into the existing 64-bit wire fields.
TEST_F(EngineServiceResourceTest, ResourceTimings)
{
    Fetch("GET", "/resources_data?timings=1");
    ExpectResponse(true);
}

// A completed zero-duration measurement must remain distinguishable from unavailable timing data.
TEST_F(EngineServiceResourceTest, ZeroResourceTimings)
{
    memset(&m_Timings, 0, sizeof(m_Timings));
    m_Timings.m_Recorded = true;
    dmResource::TestSetLoadTimings(m_Descriptor, m_Timings);
    Fetch("GET", "/resources_data?timings=1");
    ExpectResponse(true);
}

// Unavailable timings still occupy a full RES2 record so the next resource starts at the correct byte.
TEST_F(EngineServiceResourceTest, UnavailableResourceTimings)
{
    memset(&m_Timings, 0, sizeof(m_Timings));
    dmResource::TestSetLoadTimings(m_Descriptor, m_Timings);
    Fetch("GET", "/resources_data?timings=1");
    ExpectResponse(true);
}

#endif
