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
#include <dlib/atomic.h>
#include <dlib/jobsystem.h>
#include <dlib/socket.h>

static uint32_t g_WorkerCount;
// Deadlock guards only; success is determined by socket state and completion events.
static const uint64_t TEST_TIMEOUT = 60000000;

namespace dmTestSocket
{
    using namespace dmSocket;

    // Only the owning thread changes or inspects socket modes.
    static dmArray<Socket> g_NonBlockingSockets;
    static int32_atomic_t g_BlockedSocket;
    static int32_atomic_t g_BlockedSends;
    static int32_atomic_t g_EmptySelects;

    static bool IsNonBlocking(Socket socket)
    {
        for (uint32_t i = 0; i < g_NonBlockingSockets.Size(); ++i)
        {
            if (g_NonBlockingSockets[i] == socket)
            {
                return true;
            }
        }
        return false;
    }

    static Result SetBlocking(Socket socket, bool blocking)
    {
        Result result = dmSocket::SetBlocking(socket, blocking);
        if (result == RESULT_OK)
        {
            for (uint32_t i = 0; i < g_NonBlockingSockets.Size(); ++i)
            {
                if (g_NonBlockingSockets[i] == socket)
                {
                    g_NonBlockingSockets.EraseSwap(i);
                    break;
                }
            }
            if (!blocking)
            {
                if (g_NonBlockingSockets.Full())
                {
                    g_NonBlockingSockets.OffsetCapacity(16);
                }
                g_NonBlockingSockets.Push(socket);
            }
        }
        return result;
    }

    static Result Send(Socket socket, const void* buffer, int length, int* sent)
    {
        if (socket == dmAtomicLoadAcquire32(&g_BlockedSocket))
        {
            *sent = 0;
            dmAtomicIncrement32(&g_BlockedSends);
            return RESULT_WOULDBLOCK;
        }
        return dmSocket::Send(socket, buffer, length, sent);
    }

    static Result Select(Selector* selector, int32_t timeout)
    {
        // Match WSAPoll on every platform: at least one socket is required.
        if (selector->m_Poller.m_Pollfds.Empty())
        {
            dmAtomicIncrement32(&g_EmptySelects);
            return RESULT_INVAL;
        }
        return dmSocket::Select(selector, timeout);
    }
}

static HJobContext CreateTestJobs(const JobSystemCreateParams* params)
{
    JobSystemCreateParams copy = *params;
    copy.m_ThreadCount = g_WorkerCount;
    return JobSystemCreate(&copy);
}

// Compile the production transport under a private name so tests can force the
// no-worker path and backpressure without depending on native socket options.
#define JobSystemCreate CreateTestJobs
#define dmHttpServer dmTestHttpServer
#define dmSocket dmTestSocket
#include "../dlib/http/http_server.cpp"
#undef dmSocket
#undef dmHttpServer
#undef JobSystemCreate

#define JC_TEST_IMPLEMENTATION
#include <jc_test/jc_test.h>

class HttpServerAsyncTest : public jc_test_params_class<uint32_t>
{
    public:
    dmTestHttpServer::HServer m_Server;
    dmSocket::Address         m_Address;
    uint16_t                  m_Port;
    uint32_t                  m_Requests;

    static void Respond(void* context, const dmTestHttpServer::Request* request)
    {
        HttpServerAsyncTest* self = (HttpServerAsyncTest*)context;
        ++self->m_Requests;
        if (!strcmp(request->m_Resource, "/large"))
        {
            char padding[65536] = {};
            for (uint32_t i = 0; i < 64; ++i)
            {
                dmTestHttpServer::Send(request, padding, sizeof(padding));
            }
        }
        dmTestHttpServer::Send(request, "PONG", 4);
    }

    void SetUp()
    {
        g_WorkerCount = GetParam();
        dmTestSocket::g_NonBlockingSockets.SetSize(0);
        dmAtomicStore32(&dmTestSocket::g_BlockedSocket, dmSocket::INVALID_SOCKET_HANDLE);
        dmAtomicStore32(&dmTestSocket::g_BlockedSends, 0);
        dmAtomicStore32(&dmTestSocket::g_EmptySelects, 0);
        m_Requests = 0;
        dmTestHttpServer::NewParams params;
        params.m_MaxConnections = 2;
        params.m_Userdata = this;
        params.m_HttpResponse = Respond;
        ASSERT_EQ(dmTestHttpServer::RESULT_OK, dmTestHttpServer::New(&params, 0, &m_Server));
        dmTestHttpServer::GetName(m_Server, &m_Address, &m_Port);
        ASSERT_EQ(dmSocket::RESULT_OK, dmSocket::GetHostByName("127.0.0.1", &m_Address));
    }

    void TearDown()
    {
        dmTestHttpServer::Delete(m_Server);
        ASSERT_EQ(0, dmAtomicGet32(&dmTestSocket::g_EmptySelects));
    }

    dmSocket::Socket Connect()
    {
        dmSocket::Socket socket;
        if (dmSocket::New(dmSocket::DOMAIN_IPV4, dmSocket::TYPE_STREAM, dmSocket::PROTOCOL_TCP, &socket) != dmSocket::RESULT_OK)
        {
            return dmSocket::INVALID_SOCKET_HANDLE;
        }
        if (dmSocket::Connect(socket, m_Address, m_Port) != dmSocket::RESULT_OK)
        {
            dmSocket::Delete(socket);
            return dmSocket::INVALID_SOCKET_HANDLE;
        }
        dmSocket::SetBlocking(socket, false);
        return socket;
    }

    void SendRequest(dmSocket::Socket socket, const char* path = "/ping")
    {
        char request[128];
        uint32_t length = dmSnPrintf(request, sizeof(request), "GET %s HTTP/1.1\r\nHost: localhost\r\n\r\n", path);
        int sent;
        ASSERT_EQ(dmSocket::RESULT_OK, dmSocket::Send(socket, request, length, &sent));
        ASSERT_EQ((int)length, sent);
    }

    void WaitForConnections(uint32_t count)
    {
        uint64_t deadline = dmTime::GetMonotonicTime() + TEST_TIMEOUT;
        while (m_Server->m_Connections.Size() != count && dmTime::GetMonotonicTime() < deadline)
        {
            ASSERT_EQ(dmTestHttpServer::RESULT_OK, dmTestHttpServer::Update(m_Server));
        }
        ASSERT_EQ(count, m_Server->m_Connections.Size());
    }

    void WaitForRequests(uint32_t count)
    {
        uint64_t deadline = dmTime::GetMonotonicTime() + TEST_TIMEOUT;
        while (m_Requests != count && dmTime::GetMonotonicTime() < deadline)
        {
            ASSERT_EQ(dmTestHttpServer::RESULT_OK, dmTestHttpServer::Update(m_Server));
        }
        ASSERT_EQ(count, m_Requests);
    }

    void ReadPong(dmSocket::Socket socket)
    {
        char body[1024] = {};
        uint32_t size = 0;
        uint64_t deadline = dmTime::GetMonotonicTime() + TEST_TIMEOUT;
        while (!strstr(body, "PONG\r\n0\r\n\r\n") && dmTime::GetMonotonicTime() < deadline)
        {
            ASSERT_EQ(dmTestHttpServer::RESULT_OK, dmTestHttpServer::Update(m_Server));
            int received = 0;
            dmSocket::Result result = dmSocket::Receive(socket, body + size, sizeof(body) - size - 1, &received);
            if (result == dmSocket::RESULT_TRY_AGAIN || result == dmSocket::RESULT_WOULDBLOCK)
            {
                continue;
            }
            ASSERT_EQ(dmSocket::RESULT_OK, result);
            ASSERT_GT(received, 0);
            size += received;
            body[size] = 0;
        }
        ASSERT_NE((const char*)0, strstr(body, "PONG\r\n0\r\n\r\n"));
    }
};

// Idle servers and clients awaiting a response must avoid empty polls, which fail on Windows.
TEST_P(HttpServerAsyncTest, IdleAndPendingResponses)
{
    ASSERT_EQ(dmTestHttpServer::RESULT_OK, dmTestHttpServer::Update(m_Server));
    dmSocket::Socket client = Connect();
    ASSERT_NE(dmSocket::INVALID_SOCKET_HANDLE, client);
    WaitForConnections(1);
    dmAtomicStoreRelease32(&dmTestSocket::g_BlockedSocket, m_Server->m_Connections[0].m_Socket);
    SendRequest(client);
    WaitForRequests(1);
    dmTestHttpServer::AsyncResponse* response = m_Server->m_Connections[0].m_Response;
    ASSERT_NE((dmTestHttpServer::AsyncResponse*)0, response);
    ASSERT_EQ(0, dmAtomicLoadAcquire32(&response->m_Done));
    ASSERT_EQ(dmTestHttpServer::RESULT_OK, dmTestHttpServer::Update(m_Server));

    dmAtomicStoreRelease32(&dmTestSocket::g_BlockedSocket, dmSocket::INVALID_SOCKET_HANDLE);
    ReadPong(client);
    dmSocket::Delete(client);
    WaitForConnections(0);
    ASSERT_EQ(dmTestHttpServer::RESULT_OK, dmTestHttpServer::Update(m_Server));
}

// Check nonblocking mode before stale I/O so a regression fails without entering a blocking call.
TEST_P(HttpServerAsyncTest, StaleReadiness)
{
    ASSERT_TRUE(dmTestSocket::IsNonBlocking(m_Server->m_ServerSocket));
    dmSocket::Socket client = Connect();
    ASSERT_NE(dmSocket::INVALID_SOCKET_HANDLE, client);
    WaitForConnections(1);
    dmSocket::Socket extra;
    dmSocket::Address address;
    dmSocket::Result result = dmSocket::Accept(m_Server->m_ServerSocket, &address, &extra);
    ASSERT_TRUE(result == dmSocket::RESULT_TRY_AGAIN || result == dmSocket::RESULT_WOULDBLOCK);
    dmTestHttpServer::Connection* connection = &m_Server->m_Connections[0];
    ASSERT_TRUE(dmTestSocket::IsNonBlocking(connection->m_Socket));
    ASSERT_TRUE(dmTestHttpServer::HandleConnection(m_Server, connection));
    SendRequest(client);
    ReadPong(client);
    uint64_t deadline = dmTime::GetMonotonicTime() + TEST_TIMEOUT;
    while (connection->m_Response && dmTime::GetMonotonicTime() < deadline)
    {
        dmTestHttpServer::Update(m_Server);
    }
    ASSERT_EQ((dmTestHttpServer::AsyncResponse*)0, connection->m_Response);
    ASSERT_TRUE(dmTestSocket::IsNonBlocking(connection->m_Socket));
    ASSERT_TRUE(dmTestHttpServer::HandleConnection(m_Server, connection));
    SendRequest(client);
    ReadPong(client);
    dmSocket::Delete(client);
}

// A forced would-block cannot delay other clients; disconnect restores the exhausted connection limit.
TEST_P(HttpServerAsyncTest, BackpressureAndRecovery)
{
    dmSocket::Socket slow = Connect();
    ASSERT_NE(dmSocket::INVALID_SOCKET_HANDLE, slow);
    WaitForConnections(1);
    dmSocket::Socket server_socket = m_Server->m_Connections[0].m_Socket;
    ASSERT_TRUE(dmTestSocket::IsNonBlocking(server_socket));
    dmAtomicStoreRelease32(&dmTestSocket::g_BlockedSocket, server_socket);
    SendRequest(slow, "/large");
    WaitForRequests(1);
    ASSERT_EQ(g_WorkerCount, JobSystemGetWorkerCount(m_Server->m_ResponseSender->m_Jobs));
    uint64_t deadline = dmTime::GetMonotonicTime() + TEST_TIMEOUT;
    while (!dmAtomicGet32(&dmTestSocket::g_BlockedSends) && dmTime::GetMonotonicTime() < deadline)
    {
        dmTestHttpServer::Update(m_Server);
    }
    ASSERT_GT(dmAtomicGet32(&dmTestSocket::g_BlockedSends), 0);

    dmSocket::Socket fast = Connect();
    ASSERT_NE(dmSocket::INVALID_SOCKET_HANDLE, fast);
    SendRequest(fast);
    ReadPong(fast);
    ASSERT_EQ(2U, m_Requests);
    ASSERT_EQ(2U, m_Server->m_Connections.Size());
    dmTestHttpServer::AsyncResponse* pending = m_Server->m_Connections[0].m_Response;
    ASSERT_NE((dmTestHttpServer::AsyncResponse*)0, pending);
    ASSERT_EQ(0, dmAtomicLoadAcquire32(&pending->m_Done));

    dmSocket::Socket refused = Connect();
    ASSERT_NE(dmSocket::INVALID_SOCKET_HANDLE, refused);
    // The listener explicitly accepts and closes the excess connection.
    int received;
    char buffer[1];
    dmSocket::Result result = dmSocket::RESULT_WOULDBLOCK;
    deadline = dmTime::GetMonotonicTime() + TEST_TIMEOUT;
    while ((result == dmSocket::RESULT_WOULDBLOCK || result == dmSocket::RESULT_TRY_AGAIN) && dmTime::GetMonotonicTime() < deadline)
    {
        dmTestHttpServer::Update(m_Server);
        result = dmSocket::Receive(refused, buffer, sizeof(buffer), &received);
    }
    ASSERT_TRUE(result != dmSocket::RESULT_OK || received == 0);
    ASSERT_TRUE(result != dmSocket::RESULT_WOULDBLOCK && result != dmSocket::RESULT_TRY_AGAIN);
    dmSocket::Delete(refused);
    dmSocket::Shutdown(slow, dmSocket::SHUTDOWNTYPE_READWRITE);
    dmSocket::Delete(slow);
    dmAtomicStoreRelease32(&dmTestSocket::g_BlockedSocket, dmSocket::INVALID_SOCKET_HANDLE);
    WaitForConnections(1);
    dmSocket::Socket recovered = Connect();
    ASSERT_NE(dmSocket::INVALID_SOCKET_HANDLE, recovered);
    SendRequest(recovered);
    ReadPong(recovered);
    ASSERT_EQ(3U, m_Requests);
    dmSocket::Delete(fast);
    dmSocket::Delete(recovered);
}

static const uint32_t worker_counts[] = { 0, 1 };
INSTANTIATE_TEST_CASE_P(SenderModes, HttpServerAsyncTest, jc_test_values_in(worker_counts));

int main(int argc, char** argv)
{
    dmSocket::Initialize();
    jc_test_init(&argc, argv);
    int result = jc_test_run_all();
    dmSocket::Finalize();
    return result;
}
