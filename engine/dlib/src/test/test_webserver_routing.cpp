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

#include <string.h>
#include <dlib/array.h>
#include <dlib/atomic.h>
#include <dlib/dstrings.h>
#include <dlib/http/http_client.h>
#include <dlib/math.h>
#include <dlib/mutex.h>
#include <dlib/socket.h>
#include <dlib/thread.h>
#include <dlib/time.h>
#include <dlib/uri.h>
#include <dlib/webserver.h>
#define JC_TEST_IMPLEMENTATION
#include <jc_test/jc_test.h>

// Deadlock guards only; tests wait for responses or explicit state changes.
static const uint64_t TEST_TIMEOUT = 60000000;

struct WebServerRoutingTest : public jc_test_base_class
{
    dmWebServer::HServer      m_Server;
    uint16_t                  m_Port;
    dmArray<char>             m_Body;
    char                      m_ContentType[64];
    char                      m_Allow[32];
    int                       m_Status;
    int32_atomic_t            m_Done;
    const char*               m_Method;
    const char*               m_Path;
    const char*               m_RequestBody;
    dmHttpClient::Result      m_Result;
    dmArray<dmSocket::Socket> m_RawClients;
    dmMutex::HMutex           m_ClientMutex;
    dmThread::Thread          m_Watchdog;
    int32_atomic_t            m_StopWatchdog;
    int32_atomic_t            m_WatchdogFired;
    uint64_t                  m_ClientDeadline;

    void SetUp()
    {
        m_ClientMutex = dmMutex::New();
        m_Watchdog = 0;
        dmAtomicStore32(&m_StopWatchdog, 0);
        dmAtomicStore32(&m_WatchdogFired, 0);
        dmWebServer::NewParams params;
        ASSERT_EQ(dmWebServer::RESULT_OK, dmWebServer::New(&params, &m_Server));
        dmSocket::Address address;
        dmWebServer::GetName(m_Server, &address, &m_Port);
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
        dmWebServer::Delete(m_Server);
        ASSERT_EQ(0, dmAtomicGet32(&m_WatchdogFired));
    }

    static void Header(dmHttpClient::HResponse, void* context, int status, const char* key, const char* value)
    {
        WebServerRoutingTest* self = (WebServerRoutingTest*)context;
        self->m_Status = status;
        if (dmStrCaseCmp(key, "Content-Type") == 0)
        {
            dmStrlCpy(self->m_ContentType, value, sizeof(self->m_ContentType));
        }
        if (dmStrCaseCmp(key, "Allow") == 0)
        {
            dmStrlCpy(self->m_Allow, value, sizeof(self->m_Allow));
        }
    }

    static void Content(dmHttpClient::HResponse, void* context, int status, const void* data, uint32_t size,
                        int32_t, uint32_t, uint32_t, uint32_t, const char*)
    {
        WebServerRoutingTest* self = (WebServerRoutingTest*)context;
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
        WebServerRoutingTest* self = (WebServerRoutingTest*)context;
        char url[64];
        dmSnPrintf(url, sizeof(url), "http://127.0.0.1:%u", self->m_Port);
        dmURI::Parts uri;
        dmURI::Parse(url, &uri);
        dmHttpClient::NewParams params;
        params.m_Userdata = self;
        params.m_HttpHeader = Header;
        params.m_HttpContent = Content;
        params.m_RequestTimeout = TEST_TIMEOUT;
        params.m_MaxGetRetries = 1;
        params.m_HttpSendContentLength = ContentLength;
        params.m_HttpWrite = Write;
        dmHttpClient::HClient client = dmHttpClient::New(&params, &uri);
        self->m_Result = dmHttpClient::Request(client, self->m_Method, self->m_Path);
        dmHttpClient::Delete(client);
        dmAtomicStore32(&self->m_Done, 1);
    }

    static uint32_t ContentLength(dmHttpClient::HResponse, void* context)
    {
        return strlen(((WebServerRoutingTest*)context)->m_RequestBody);
    }

    static dmHttpClient::Result Write(dmHttpClient::HResponse response, uint32_t offset, uint32_t length, void* context)
    {
        WebServerRoutingTest* self = (WebServerRoutingTest*)context;
        return dmHttpClient::Write(response, self->m_RequestBody + offset, length);
    }

    void Fetch(const char* method, const char* path, const char* body)
    {
        m_Method = method;
        m_Path = path;
        m_RequestBody = body;
        m_Status = 0;
        m_ContentType[0] = 0;
        m_Allow[0] = 0;
        m_Body.SetSize(0);
        dmAtomicStore32(&m_Done, 0);
        dmThread::Thread client = dmThread::New(Request, 0x80000, this, "routing-client");
        while (!dmAtomicGet32(&m_Done))
        {
            ASSERT_EQ(dmWebServer::RESULT_OK, dmWebServer::Update(m_Server));
        }
        dmThread::Join(client);
        if (m_Body.Full())
        {
            m_Body.OffsetCapacity(1);
        }
        m_Body.Push(0);
        ASSERT_TRUE(m_Result == dmHttpClient::RESULT_OK || m_Result == dmHttpClient::RESULT_NOT_200_OK);
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
        if (dmSocket::Connect(socket, address, m_Port) != dmSocket::RESULT_OK)
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
            m_ClientDeadline = dmTime::GetMonotonicTime() + TEST_TIMEOUT;
        }
        if (!m_Watchdog)
        {
            m_Watchdog = dmThread::New(Watchdog, 0x80000, this, "routing-watchdog");
        }
        return socket;
    }

    static void Watchdog(void* context)
    {
        WebServerRoutingTest* self = (WebServerRoutingTest*)context;
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
        uint64_t deadline = dmTime::GetMonotonicTime() + TEST_TIMEOUT;
        while (!strstr(m_Body.Begin(), "\r\n0\r\n\r\n") && dmTime::GetMonotonicTime() < deadline)
        {
            ASSERT_EQ(dmWebServer::RESULT_OK, dmWebServer::Update(m_Server));
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

    static void ExtensionHandler(void* context, dmWebServer::Request* request)
    {
        const char* body = (const char*)context;
        dmWebServer::SetStatusCode(request, 200);
        dmWebServer::SendAttribute(request, "Content-Type", "text/plain");
        dmWebServer::Send(request, body, strlen(body));
    }

    static void EchoHeader(void*, dmWebServer::Request* request)
    {
        const char* value = dmWebServer::GetHeader(request, "X-Routing-Test");
        const char* body = value ? value : "absent";
        dmWebServer::Send(request, body, strlen(body));
    }

    static void EchoRequest(void* context, dmWebServer::Request* request)
    {
        ++*(uint32_t*)context;
        char response[256];
        dmSnPrintf(response, sizeof(response), "%s %s\n", request->m_Method, request->m_Resource);
        dmWebServer::Send(request, response, strlen(response));
        uint32_t remaining = request->m_ContentLength;
        while (remaining)
        {
            uint32_t received;
            ASSERT_EQ(dmWebServer::RESULT_OK, dmWebServer::Receive(request, response, dmMath::Min(remaining, (uint32_t)sizeof(response)), &received));
            ASSERT_GT(received, 0U);
            dmWebServer::Send(request, response, received);
            remaining -= received;
        }
    }
};

// Each registration flag is independent; exact matching preserves queries and allows a shorter prefix fallback.
TEST_F(WebServerRoutingTest, IndependentHandlerFlags)
{
    dmWebServer::HServer server = m_Server;
    uint32_t calls = 0;
    dmWebServer::HandlerParams params;
    params.m_Handler = EchoRequest;
    params.m_Userdata = &calls;
    ASSERT_EQ(dmWebServer::RESULT_OK, dmWebServer::AddHandler(server, "/test/exact", &params, 0, dmWebServer::MATCH_EXACT_PATH));
    ASSERT_EQ(dmWebServer::RESULT_OK, dmWebServer::AddHandler(server, "/test/get", &params, 0, dmWebServer::GET_ONLY));
    ASSERT_EQ(dmWebServer::RESULT_OK, dmWebServer::AddHandler(server, "/test/empty", &params, 0, dmWebServer::NO_REQUEST_BODY));
    params.m_Handler = ExtensionHandler;
    params.m_Userdata = (void*)"Fallback";
    ASSERT_EQ(dmWebServer::RESULT_OK, dmWebServer::AddHandler(server, "/test", &params));

    Fetch("POST", "/test/exact?value=/child", "body");
    ASSERT_EQ(200, m_Status);
    ASSERT_STREQ("POST /test/exact?value=/child\nbody", m_Body.Begin());
    Fetch("GET", "/test/exact/child", "");
    ASSERT_STREQ("Fallback", m_Body.Begin());
    Fetch("GET", "/test/exactly", "");
    ASSERT_STREQ("Fallback", m_Body.Begin());
    ASSERT_EQ(1U, calls);

    Fetch("GET", "/test/get/child?value=1", "");
    ASSERT_EQ(200, m_Status);
    ASSERT_STREQ("GET /test/get/child?value=1\n", m_Body.Begin());
    Fetch("DELETE", "/test/get/child", "");
    ASSERT_EQ(405, m_Status);
    ASSERT_STREQ("GET", m_Allow);
    ASSERT_STREQ("application/json", m_ContentType);
    ASSERT_EQ(2U, calls);
    dmSocket::Socket socket = OpenRequest("GET /test/get HTTP/1.1\r\nHost: localhost\r\nContent-Length: 4\r\n\r\nbody");
    ASSERT_NE(dmSocket::INVALID_SOCKET_HANDLE, socket);
    ReadResponse(socket);
    ASSERT_NE((const char*)0, strstr(m_Body.Begin(), "200 OK"));
    ASSERT_NE((const char*)0, strstr(m_Body.Begin(), "body"));
    ASSERT_EQ(3U, calls);

    Fetch("DELETE", "/test/empty/child?value=1", "");
    ASSERT_EQ(200, m_Status);
    ASSERT_STREQ("DELETE /test/empty/child?value=1\n", m_Body.Begin());
    ASSERT_EQ(4U, calls);
}

// Registration overloads without flags keep prefix routing, arbitrary methods, request bodies, and the root fallback.
TEST_F(WebServerRoutingTest, UnconstrainedHandlerRegistration)
{
    dmWebServer::HServer server = m_Server;
    uint32_t calls = 0;
    dmWebServer::HandlerParams params;
    params.m_Handler = EchoRequest;
    params.m_Userdata = &calls;
    for (uint32_t registration = 0; registration < 3; ++registration)
    {
        dmWebServer::Result result;
        if (registration == 0)
        {
            result = dmWebServer::AddHandler(server, "/", &params);
        }
        else if (registration == 1)
        {
            result = dmWebServer::AddHandler(server, "/", &params, 0);
        }
        else
        {
            result = dmWebServer::AddHandler(server, "/", &params, 0, 0);
        }
        ASSERT_EQ(dmWebServer::RESULT_OK, result);
        Fetch("POST", "/legacy/child?value=1", "body");
        ASSERT_EQ(200, m_Status);
        ASSERT_STREQ("POST /legacy/child?value=1\nbody", m_Body.Begin());
        ASSERT_EQ(registration + 1, calls);
        ASSERT_EQ(dmWebServer::RESULT_OK, dmWebServer::RemoveHandler(server, "/"));
    }
}

// Router errors must complete with an upload withheld, skip callbacks, and clear headers before the next request.
TEST_F(WebServerRoutingTest, RejectedHandlerRequests)
{
    dmWebServer::HServer server = m_Server;
    uint32_t calls = 0;
    dmWebServer::HandlerParams params;
    params.m_Handler = EchoRequest;
    params.m_Userdata = &calls;
    ASSERT_EQ(dmWebServer::RESULT_OK, dmWebServer::AddHandler(server, "/test/get", &params, 0, dmWebServer::GET_ONLY));
    ASSERT_EQ(dmWebServer::RESULT_OK, dmWebServer::AddHandler(server, "/test/empty", &params, 0, dmWebServer::NO_REQUEST_BODY));
    ASSERT_EQ(dmWebServer::RESULT_OK, dmWebServer::AddHandler(server, "/test/both", &params, 0, dmWebServer::GET_ONLY | dmWebServer::NO_REQUEST_BODY));
    params.m_Handler = EchoHeader;
    ASSERT_EQ(dmWebServer::RESULT_OK, dmWebServer::AddHandler(server, "/test/header", &params));
    const char* paths[] = { "/test/get", "/test/empty", "/test/both", "/unknown" };
    const char* statuses[] = { "405 Method Not Allowed", "400 Bad Request", "400 Bad Request", "404 Not Found" };
    for (uint32_t i = 0; i < DM_ARRAY_SIZE(paths); ++i)
    {
        char request[256];
        dmSnPrintf(request, sizeof(request), "DELETE %s HTTP/1.1\r\nHost: localhost\r\nX-Routing-Test: rejected\r\nContent-Length: 10\r\n\r\n", paths[i]);
        dmSocket::Socket socket = OpenRequest(request);
        ASSERT_NE(dmSocket::INVALID_SOCKET_HANDLE, socket);
        ReadResponse(socket);
        ASSERT_NE((const char*)0, strstr(m_Body.Begin(), statuses[i]));
        ASSERT_NE((const char*)0, strstr(m_Body.Begin(), "Connection: close"));
        ASSERT_EQ(0U, calls);
        Fetch("GET", "/test/header", "");
        ASSERT_EQ(200, m_Status);
        ASSERT_STREQ("absent", m_Body.Begin());
    }
}

// Partial headers must yield to another client, then finish normally once explicitly released.
TEST_F(WebServerRoutingTest, PartialRequestHeaders)
{
    dmWebServer::HandlerParams handler;
    handler.m_Handler = ExtensionHandler;
    handler.m_Userdata = (void*)"Response";
    ASSERT_EQ(dmWebServer::RESULT_OK, dmWebServer::AddHandler(m_Server, "/test", &handler));
    dmSocket::Socket stalled = OpenRequest("GET /test HTTP/1.1\r\nHost:");
    ASSERT_NE(dmSocket::INVALID_SOCKET_HANDLE, stalled);
    Fetch("GET", "/test", "");
    ASSERT_EQ(200, m_Status);
    const char suffix[] = " localhost\r\n\r\n";
    int sent;
    ASSERT_EQ(dmSocket::RESULT_OK, dmSocket::Send(stalled, suffix, sizeof(suffix) - 1, &sent));
    ReadResponse(stalled);
    ASSERT_NE((const char*)0, strstr(m_Body.Begin(), "200 OK"));
}

// Malformed framing must close with 400 without waiting for a body or leaking headers to the next client.
TEST_F(WebServerRoutingTest, MalformedRequestFraming)
{
    dmWebServer::HandlerParams handler;
    handler.m_Handler = EchoHeader;
    handler.m_Userdata = 0;
    ASSERT_EQ(dmWebServer::RESULT_OK, dmWebServer::AddHandler(m_Server, "/test/header", &handler));
    const char* headers[] = { "Content-Length: -1", "Content-Length: 4294967296", "Content-Length: 0garbage",
                              "Content-Length:", "Content-Length: 0\r\nContent-Length: 10", "Transfer-Encoding: chunked",
                              "Content-Length: 0\r\nTransfer-Encoding: chunked", "Content-Length : 10", " Content-Length: 10" };
    for (uint32_t i = 0; i < DM_ARRAY_SIZE(headers); ++i)
    {
        char request[256];
        dmSnPrintf(request, sizeof(request), "GET /test/header HTTP/1.1\r\nX-Routing-Test: rejected\r\n%s\r\n\r\n", headers[i]);
        dmSocket::Socket socket = OpenRequest(request);
        ASSERT_NE(dmSocket::INVALID_SOCKET_HANDLE, socket);
        ReadResponse(socket);
        ASSERT_NE((const char*)0, strstr(m_Body.Begin(), "400 Bad Request"));
        ASSERT_NE((const char*)0, strstr(m_Body.Begin(), "Connection: close"));
        Fetch("GET", "/test/header", "");
        ASSERT_STREQ("absent", m_Body.Begin());
    }
    dmSocket::Socket socket = OpenRequest("GET /test/header HTTP/1.1\r\nContent-Length:0\t \r\nX-Routing-Test: accepted\r\n\r\n");
    ASSERT_NE(dmSocket::INVALID_SOCKET_HANDLE, socket);
    ReadResponse(socket);
    ASSERT_NE((const char*)0, strstr(m_Body.Begin(), "200 OK"));
    ASSERT_NE((const char*)0, strstr(m_Body.Begin(), "accepted"));
}

int main(int argc, char** argv)
{
    dmSocket::Initialize();
    jc_test_init(&argc, argv);
    int result = jc_test_run_all();
    dmSocket::Finalize();
    return result;
}
