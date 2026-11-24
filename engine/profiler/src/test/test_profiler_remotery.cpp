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

#include <string.h>

#include <dlib/socket.h>

#include "remotery/lib/Remotery.h"

static const int TEST_PORT = 17899;

// Returns true if the server closed the connection before the receive timeout
static bool SendPlainHttpRequest(int port)
{
    dmSocket::Socket socket;
    if (dmSocket::New(dmSocket::DOMAIN_IPV4, dmSocket::TYPE_STREAM, dmSocket::PROTOCOL_TCP, &socket) != dmSocket::RESULT_OK)
        return false;

    bool closed = false;
    dmSocket::SetReceiveTimeout(socket, 5 * 1000000);
    if (dmSocket::Connect(socket, dmSocket::AddressFromIPString("127.0.0.1"), port) == dmSocket::RESULT_OK)
    {
        const char request[] = "GET / HTTP/1.1\r\nHost: 127.0.0.1\r\n\r\n";
        int sent = 0;
        dmSocket::Send(socket, request, sizeof(request) - 1, &sent);

        char buffer[256];
        for (;;)
        {
            int received = 0;
            dmSocket::Result r = dmSocket::Receive(socket, buffer, sizeof(buffer), &received);
            if (r == dmSocket::RESULT_OK && received > 0)
                continue;
            closed = r == dmSocket::RESULT_OK || r == dmSocket::RESULT_CONNRESET;
            break;
        }
    }

    dmSocket::Delete(socket);
    return closed;
}

// Verifies the server closes a connection that fails the WebSocket handshake, guarding against leaking the accepted socket.
TEST(ProfilerRemotery, RejectedHandshakeClosesConnection)
{
    rmtSettings* settings = rmt_Settings();
    settings->port = TEST_PORT;
    settings->reuse_open_port = RMT_TRUE;
    settings->limit_connections_to_localhost = RMT_TRUE;
    settings->enableThreadSampler = RMT_FALSE;

    Remotery* remotery = 0;
    ASSERT_EQ(RMT_ERROR_NONE, rmt_CreateGlobalInstance(&remotery));

    ASSERT_TRUE(SendPlainHttpRequest(TEST_PORT));

    rmt_DestroyGlobalInstance(remotery);
}

int main(int argc, char **argv)
{
    dmSocket::Initialize();
    jc_test_init(&argc, argv);
    int result = jc_test_run_all();
    dmSocket::Finalize();
    return result;
}
