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

#if defined(_WIN32)
#include <winsock2.h>
#endif

#define JC_TEST_IMPLEMENTATION
#include <jc_test/jc_test.h>

#include <string.h>

#include <dlib/configfile.h>
#include <dlib/dstrings.h>
#include <dlib/profile.h>
#include <dlib/socket.h>
#include <extension.hpp>

#include "remotery/lib/Remotery.h"

extern "C" void ProfilerRemotery();

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

class ProfilerRemoteryLifecycleTest : public jc_test_base_class
{
public:
    dmSocket::Socket m_BlockingSocket;
    dmExtension::AppParams m_Params;
    rmtSettings m_Settings;

    void SetUp()
    {
        m_BlockingSocket = dmSocket::INVALID_SOCKET_HANDLE;
        m_Settings = *rmt_Settings();
        ExtensionAppParamsInitialize(&m_Params);
        ProfileInitialize();
    }

    void TearDown()
    {
        if (ProfileIsInitialized())
            ProfileFinalize();
        m_Params.m_ExitStatus = EXTENSION_APP_EXIT_CODE_EXIT;
        dmExtension::AppFinalize(&m_Params);
        if (m_Params.m_ConfigFile)
            dmConfigFile::Delete(m_Params.m_ConfigFile);
        ExtensionAppParamsFinalize(&m_Params);
        if (m_BlockingSocket != dmSocket::INVALID_SOCKET_HANDLE)
            dmSocket::Delete(m_BlockingSocket);
        *rmt_Settings() = m_Settings;
    }
};

// Verifies that a port conflict leaves Remotery disabled across reboots without
// corrupting the registry, and that a new app lifecycle can start it (#13602).
TEST_F(ProfilerRemoteryLifecycleTest, RebootAfterFailedInitialization)
{
    ASSERT_EQ(dmSocket::RESULT_OK, dmSocket::New(dmSocket::DOMAIN_IPV4, dmSocket::TYPE_STREAM, dmSocket::PROTOCOL_TCP, &m_BlockingSocket));
#if defined(_WIN32)
    // Prevent Remotery's SO_REUSEADDR from sharing the occupied port on Windows.
    const int exclusive_address = 1;
    ASSERT_EQ(0, setsockopt((SOCKET)m_BlockingSocket, SOL_SOCKET, SO_EXCLUSIVEADDRUSE, (const char*)&exclusive_address, sizeof(exclusive_address)));
#endif
    dmSocket::Address address = dmSocket::AddressFromIPString("127.0.0.1");
    ASSERT_EQ(dmSocket::RESULT_OK, dmSocket::Bind(m_BlockingSocket, address, 0));
    ASSERT_EQ(dmSocket::RESULT_OK, dmSocket::Listen(m_BlockingSocket, 1));
    uint16_t port = 0;
    ASSERT_EQ(dmSocket::RESULT_OK, dmSocket::GetName(m_BlockingSocket, &address, &port));

    char config[128];
    dmSnPrintf(config, sizeof(config), "[profiler]\nremotery_port = %u\n", (unsigned int)port);
    ASSERT_EQ(dmConfigFile::RESULT_OK, dmConfigFile::LoadFromBuffer(config, strlen(config), 0, 0, &m_Params.m_ConfigFile));
    ASSERT_EQ(dmExtension::RESULT_OK, dmExtension::AppInitialize(&m_Params));
    ASSERT_EQ((Remotery*)0, rmt_GetGlobalInstance());

    for (uint32_t i = 0; i < 2; ++i)
    {
        m_Params.m_ExitStatus = EXTENSION_APP_EXIT_CODE_REBOOT;
        ASSERT_EQ(dmExtension::RESULT_OK, dmExtension::AppFinalize(&m_Params));
        m_Params.m_ExitStatus = EXTENSION_APP_EXIT_CODE_NONE;
        ASSERT_EQ(dmExtension::RESULT_OK, dmExtension::AppInitialize(&m_Params));
        ASSERT_EQ((Remotery*)0, rmt_GetGlobalInstance());
        ASSERT_EQ(PROFILE_RESULT_OK, ProfileSetThreadName("Reboot"));
        ASSERT_EQ(PROFILE_RESULT_OK, ProfileFrameEnd(ProfileFrameBegin()));
    }

    ProfileFinalize();
    m_Params.m_ExitStatus = EXTENSION_APP_EXIT_CODE_EXIT;
    ASSERT_EQ(dmExtension::RESULT_OK, dmExtension::AppFinalize(&m_Params));
    dmSocket::Delete(m_BlockingSocket);
    m_BlockingSocket = dmSocket::INVALID_SOCKET_HANDLE;

    ProfileInitialize();
    // Final exit must have removed the old listener, including its stored name.
    ASSERT_EQ((Remotery*)0, rmt_GetGlobalInstance());
    m_Params.m_ExitStatus = EXTENSION_APP_EXIT_CODE_NONE;
    ASSERT_EQ(dmExtension::RESULT_OK, dmExtension::AppInitialize(&m_Params));
    Remotery* remotery = rmt_GetGlobalInstance();
    ASSERT_NE((Remotery*)0, remotery);

    m_Params.m_ExitStatus = EXTENSION_APP_EXIT_CODE_REBOOT;
    ASSERT_EQ(dmExtension::RESULT_OK, dmExtension::AppFinalize(&m_Params));
    m_Params.m_ExitStatus = EXTENSION_APP_EXIT_CODE_NONE;
    ASSERT_EQ(dmExtension::RESULT_OK, dmExtension::AppInitialize(&m_Params));
    ASSERT_EQ(remotery, rmt_GetGlobalInstance());
    ASSERT_EQ(PROFILE_RESULT_OK, ProfileSetThreadName("RebootWithRemotery"));
}

int main(int argc, char **argv)
{
    ProfilerRemotery();
    dmSocket::Initialize();
    jc_test_init(&argc, argv);
    int result = jc_test_run_all();
    dmSocket::Finalize();
    return result;
}
