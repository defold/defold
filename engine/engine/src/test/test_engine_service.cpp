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
#include "../../../resource/src/resource_private.h"
#include <dlib/atomic.h>
#include <dlib/http/http_client.h>
#include <dlib/socket.h>
#include <dlib/thread.h>
#include <dlib/uri.h>
#include <dlib/webserver.h>

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

class EngineServiceOpenAPITest : public jc_test_params_class<bool>
{
public:
    dmEngineService::HEngineService m_Service;
    dmArray<char> m_Body;
    char m_ContentType[64];
    char m_Allow[32];
    char m_Connection[16];
    int m_Status;
    int32_atomic_t m_Done;
    const char* m_Method;
    const char* m_Path;
    const char* m_RequestBody;
    bool m_SendRequestBody;
    int32_t m_RequestPrefixSize;
    int m_ProbeStatus;
    dmHttpClient::Result m_ProbeResult;
    dmHttpClient::Result m_Result;

    void SetUp()
    {
        m_RequestPrefixSize = -1;
        dmSocket::Initialize();
        m_Service = dmEngineService::New(0);
        ASSERT_NE((dmEngineService::HEngineService)0, m_Service);
        dmEngineService::EngineState state;
        state.m_ConnectionAppMode = true;
        dmEngineService::InitState(m_Service, &state);
    }

    void TearDown()
    {
        dmEngineService::Delete(m_Service);
        dmSocket::Finalize();
    }

    static void Header(dmHttpClient::HResponse, void* context, int status, const char* key, const char* value)
    {
        EngineServiceOpenAPITest* self = (EngineServiceOpenAPITest*)context;
        self->m_Status = status;
        if (dmStrCaseCmp(key, "Content-Type") == 0)
            dmStrlCpy(self->m_ContentType, value, sizeof(self->m_ContentType));
        if (dmStrCaseCmp(key, "Allow") == 0)
            dmStrlCpy(self->m_Allow, value, sizeof(self->m_Allow));
        if (dmStrCaseCmp(key, "Connection") == 0)
            dmStrlCpy(self->m_Connection, value, sizeof(self->m_Connection));
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
        params.m_RequestTimeout = 5000000;
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
        return strlen(((EngineServiceOpenAPITest*)context)->m_RequestBody);
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
        params.m_RequestTimeout = 5000000;
        params.m_MaxGetRetries = 1;
        dmHttpClient::HClient client = dmHttpClient::New(&params, &uri);
        self->m_ProbeResult = dmHttpClient::Request(client, "GET", "/ping");
        dmHttpClient::Delete(client);

        // Release a blocking handler even if the probe timed out, so the regression fails without hanging the suite.
        return dmHttpClient::Write(response, self->m_RequestBody + offset + prefix_size, length - prefix_size);
    }

    void Fetch(const char* method, const char* path, const char* body = "", bool send_body = true)
    {
        m_Method = method;
        m_Path = path;
        m_RequestBody = body;
        m_SendRequestBody = send_body;
        m_ProbeStatus = 0;
        m_ProbeResult = dmHttpClient::RESULT_UNKNOWN;
        m_Status = 0;
        m_ContentType[0] = 0;
        m_Allow[0] = 0;
        m_Connection[0] = 0;
        m_Body.SetSize(0);
        dmAtomicStore32(&m_Done, 0);
        dmThread::Thread client = dmThread::New(Request, 0x80000, this, "openapi-client");
        while (!dmAtomicGet32(&m_Done))
            dmWebServer::Update(dmEngineService::GetWebServer(m_Service));
        dmThread::Join(client);
        if (m_Body.Full())
            m_Body.OffsetCapacity(1);
        m_Body.Push(0);
    }

    static void ExtensionHandler(void* context, dmWebServer::Request* request)
    {
        const char* body = (const char*)context;
        dmWebServer::SetStatusCode(request, 200);
        dmWebServer::SendAttribute(request, "Content-Type", "text/plain");
        dmWebServer::Send(request, body, strlen(body));
    }
};

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
    ResourceDescriptor*     m_Descriptor;

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
        m_Descriptor->m_LoadTimings = timings;
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
            const dmResource::LoadTimings& timings = m_Descriptor->m_LoadTimings;
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
    memset(&m_Descriptor->m_LoadTimings, 0, sizeof(m_Descriptor->m_LoadTimings));
    m_Descriptor->m_LoadTimings.m_Recorded = true;
    Fetch("GET", "/resources_data?timings=1");
    ExpectResponse(true);
}

// Unavailable timings still occupy a full RES2 record so the next resource starts at the correct byte.
TEST_F(EngineServiceResourceTest, UnavailableResourceTimings)
{
    memset(&m_Descriptor->m_LoadTimings, 0, sizeof(m_Descriptor->m_LoadTimings));
    Fetch("GET", "/resources_data?timings=1");
    ExpectResponse(true);
}

#endif
