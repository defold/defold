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

class EngineServiceOpenAPITest : public jc_test_base_class
{
public:
    dmEngineService::HEngineService m_Service;
    dmArray<char> m_Body;
    char m_ContentType[64];
    char m_Allow[16];
    int m_Status;
    int32_atomic_t m_Done;
    const char* m_Method;
    const char* m_Path;
    dmHttpClient::Result m_Result;

    void SetUp()
    {
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
        dmHttpClient::HClient client = dmHttpClient::New(&params, &uri);
        self->m_Result = dmHttpClient::Request(client, self->m_Method, self->m_Path);
        dmHttpClient::Delete(client);
        dmAtomicStore32(&self->m_Done, 1);
    }

    void Fetch(const char* method, const char* path)
    {
        m_Method = method;
        m_Path = path;
        m_Status = 0;
        m_ContentType[0] = 0;
        m_Allow[0] = 0;
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

    static void ExtensionHandler(void*, dmWebServer::Request* request)
    {
        dmWebServer::SetStatusCode(request, 204);
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

// Native-extension metadata must appear and disappear with its handler, including after the profiler is initialized.
TEST_F(EngineServiceOpenAPITest, ExtensionLifecycle)
{
    static const char json[] = "{\"/extension/test\":{\"get\":{\"responses\":{\"204\":{\"description\":\"Extension response\"}}}}}";
    dmWebServer::HandlerParams params;
    params.m_Handler = ExtensionHandler;
    params.m_Userdata = 0;
    dmWebServer::HServer server = dmEngineService::GetWebServer(m_Service);
    ASSERT_EQ(dmWebServer::RESULT_OK, dmWebServer::AddHandler(server, "/extension", &params, json));
    dmEngineService::InitProfiler(m_Service, 0, 0);
    Fetch("GET", "/openapi.json");
    ASSERT_EQ(200, m_Status);
    ASSERT_NE((const char*)0, strstr(m_Body.Begin(), "\"/extension/test\""));
    ASSERT_EQ(dmWebServer::RESULT_OK, dmWebServer::RemoveHandler(server, "/extension"));
    Fetch("GET", "/openapi.json");
    ASSERT_EQ(200, m_Status);
    ASSERT_EQ((const char*)0, strstr(m_Body.Begin(), "\"/extension/test\""));
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

#endif
