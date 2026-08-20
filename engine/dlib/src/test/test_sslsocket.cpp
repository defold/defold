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

#include <stdint.h>
#include <string.h>

#include <dlib/configfile.h>
#include <dlib/dstrings.h>
#include <dlib/log.h>
#include <dlib/socket.h>
#include <dlib/sslsocket.h>
#include <dlib/testutil.h>
#include <dlib/time.h>

#define JC_TEST_IMPLEMENTATION
#include <jc_test/jc_test.h>

static int g_HttpPortTLS = -1;
static char g_HttpAddress[128] = "localhost";

static void CloseSockets(dmSocket::Socket socket, dmSSLSocket::Socket ssl_socket)
{
    if (ssl_socket != dmSSLSocket::INVALID_SOCKET_HANDLE)
    {
        dmSSLSocket::Delete(ssl_socket);
    }
    if (socket != dmSocket::INVALID_SOCKET_HANDLE)
    {
        dmSocket::Delete(socket);
    }
}

static void ConnectTLSSocket(dmSocket::Socket* socket, dmSSLSocket::Socket* ssl_socket)
{
    *socket = dmSocket::INVALID_SOCKET_HANDLE;
    *ssl_socket = dmSSLSocket::INVALID_SOCKET_HANDLE;

    dmSocket::Result socket_result = dmSocket::New(dmSocket::DOMAIN_IPV4, dmSocket::TYPE_STREAM, dmSocket::PROTOCOL_TCP, socket);
    ASSERT_EQ(dmSocket::RESULT_OK, socket_result);

    dmSocket::Address address;
    socket_result = dmSocket::GetHostByName(g_HttpAddress, &address, true, false);
    ASSERT_EQ(dmSocket::RESULT_OK, socket_result);

    socket_result = dmSocket::Connect(*socket, address, g_HttpPortTLS);
    ASSERT_EQ(dmSocket::RESULT_OK, socket_result);

    dmSSLSocket::Result ssl_result = dmSSLSocket::New(*socket, g_HttpAddress, 10 * 1000000, ssl_socket);
    ASSERT_EQ(dmSSLSocket::RESULT_OK, ssl_result);
}

static void SendAll(dmSSLSocket::Socket ssl_socket, const char* buffer, uint32_t length)
{
    uint32_t total_sent = 0;
    uint64_t start = dmTime::GetMonotonicTime();
    while (total_sent < length)
    {
        int sent = 0;
        dmSocket::Result result = dmSSLSocket::Send(ssl_socket, buffer + total_sent, length - total_sent, &sent);
        if (result == dmSocket::RESULT_WOULDBLOCK)
        {
            ASSERT_LT(dmTime::GetMonotonicTime() - start, 5 * 1000000ULL);
            dmTime::Sleep(1000);
            continue;
        }

        ASSERT_EQ(dmSocket::RESULT_OK, result);
        ASSERT_GT(sent, 0);
        total_sent += sent;
    }
}

static void SendString(dmSSLSocket::Socket ssl_socket, const char* buffer)
{
    SendAll(ssl_socket, buffer, (uint32_t)strlen(buffer));
}

static void SendWebSocketHandshakeLikeExtension(dmSSLSocket::Socket ssl_socket)
{
    char host[160];
    dmSnPrintf(host, sizeof(host), "%s:%d", g_HttpAddress, g_HttpPortTLS);

    SendString(ssl_socket, "GET ");
    SendString(ssl_socket, "/");
    SendString(ssl_socket, " HTTP/1.1\r\n");
    SendString(ssl_socket, "Host: ");
    SendString(ssl_socket, host);
    SendString(ssl_socket, "\r\n");
    SendString(ssl_socket, "Upgrade: websocket\r\n");
    SendString(ssl_socket, "Connection: Upgrade\r\n");
    SendString(ssl_socket, "Sec-WebSocket-Key: 0123456789ABCDEF012345==\r\n");
    SendString(ssl_socket, "Sec-WebSocket-Version: 13\r\n");
    SendString(ssl_socket, "\r\n");
}

static bool WaitForRawSocketReadable(dmSocket::Socket socket, uint64_t timeout)
{
    dmSocket::Selector selector;
    dmSocket::SelectorZero(&selector);
    dmSocket::SelectorSet(&selector, dmSocket::SELECTOR_KIND_READ, socket);
    dmSocket::Result select_result = dmSocket::Select(&selector, timeout);
    return select_result == dmSocket::RESULT_OK && dmSocket::SelectorIsSet(&selector, dmSocket::SELECTOR_KIND_READ, socket);
}

// The SSL socket API must preserve nonblocking socket semantics. The Apple
// CFStream backend used to call CFReadStreamRead directly here, which can block
// when the TLS connection is established but no application data is available.
TEST(dmSSLSocket, ReceiveWithoutApplicationDataReturnsWouldBlock)
{
#if !defined(__MACH__)
    SKIP();
#endif

    dmSocket::Socket socket = dmSocket::INVALID_SOCKET_HANDLE;
    dmSSLSocket::Socket ssl_socket = dmSSLSocket::INVALID_SOCKET_HANDLE;

    ConnectTLSSocket(&socket, &ssl_socket);
    ASSERT_EQ(dmSocket::RESULT_OK, dmSSLSocket::SetReceiveTimeout(ssl_socket, 1000));

    char response[4096];
    int received = -1;
    uint64_t start = dmTime::GetMonotonicTime();
    dmSocket::Result receive_result = dmSSLSocket::Receive(ssl_socket, response, sizeof(response), &received);
    uint64_t elapsed = dmTime::GetMonotonicTime() - start;

    ASSERT_EQ(dmSocket::RESULT_WOULDBLOCK, receive_result);
    ASSERT_EQ(0, received);
    ASSERT_LT(elapsed, 100 * 1000ULL);

    CloseSockets(socket, ssl_socket);
}

// Reproduces the extension-websocket#65 read pattern: send the websocket
// handshake as several SSL writes, wait for the raw TCP socket to become
// readable, and then do one SSL receive. This must not stall for seconds.
TEST(dmSSLSocket, ReceiveReturnsAfterUnderlyingSocketIsReadable)
{
#if !defined(__MACH__)
    SKIP();
#endif

    dmSocket::Socket socket = dmSocket::INVALID_SOCKET_HANDLE;
    dmSSLSocket::Socket ssl_socket = dmSSLSocket::INVALID_SOCKET_HANDLE;

    ConnectTLSSocket(&socket, &ssl_socket);

    SendWebSocketHandshakeLikeExtension(ssl_socket);

    char response[4096];
    int received = 0;
    dmSocket::Result receive_result = dmSocket::RESULT_WOULDBLOCK;
    uint64_t start = dmTime::GetMonotonicTime();
    while (receive_result == dmSocket::RESULT_WOULDBLOCK && dmTime::GetMonotonicTime() - start < 1000 * 1000ULL)
    {
        ASSERT_TRUE(WaitForRawSocketReadable(socket, 5 * 1000000));
        receive_result = dmSSLSocket::Receive(ssl_socket, response, sizeof(response) - 1, &received);
        if (receive_result == dmSocket::RESULT_WOULDBLOCK)
        {
            dmTime::Sleep(10 * 1000);
        }
    }
    uint64_t elapsed = dmTime::GetMonotonicTime() - start;

    ASSERT_EQ(dmSocket::RESULT_OK, receive_result);
    ASSERT_GT(received, 0);
    ASSERT_LT(elapsed, 1000 * 1000ULL);

    CloseSockets(socket, ssl_socket);
}

static void Usage()
{
    dmLogError("Usage: <exe> <config>");
    dmLogError("Be sure to start the http server before starting this test.");
    dmLogError("You can use the config file created by the server");
}

int main(int argc, char **argv)
{
    jc_test_init(&argc, argv);

    if (argc > 1)
    {
        char path[512];
        dmTestUtil::MakeHostPath(path, sizeof(path), argv[1]);

        dmConfigFile::HConfig config;
        if (dmConfigFile::Load(path, argc, (const char**)argv, &config) != dmConfigFile::RESULT_OK)
        {
            dmLogError("Could not read config file '%s'", argv[1]);
            Usage();
            return 1;
        }

        const char* ip = dmConfigFile::GetString(config, "server.ip", "localhost");
        dmStrlCpy(g_HttpAddress, ip, sizeof(g_HttpAddress));
        dmTestUtil::GetSocketsFromConfig(config, 0, &g_HttpPortTLS, 0);
        dmConfigFile::Delete(config);
    }
    else
    {
        Usage();
        return 1;
    }

    dmLogSetLevel(LOG_SEVERITY_INFO);
    dmSocket::Initialize();
    dmSSLSocket::Initialize();

    int ret = jc_test_run_all();

    dmSSLSocket::Finalize();
    dmSocket::Finalize();
    return ret;
}
