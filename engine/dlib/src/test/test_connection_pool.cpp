// Copyright 2020-2025 The Defold Foundation
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
#include <vector>
#include <set>
#include <dlib/configfile.h>
#include <dlib/connection_pool.h>
#include <dlib/socket.h>
#include <dlib/sslsocket.h>
#include <dlib/log.h>
#include <dlib/time.h>
#include <dlib/testutil.h>
#define JC_TEST_IMPLEMENTATION
#include <jc_test/jc_test.h>

int g_HttpPort = -1;
char g_HttpAddress[128] = "localhost";

static const uint32_t MAX_CONNECTIONS = 8;

class dmConnectionPoolTest: public jc_test_base_class
{
public:

    dmConnectionPool::HPool pool;
    void SetUp() override
    {
        dmConnectionPool::Params params;
        params.m_MaxConnections = MAX_CONNECTIONS;
        dmConnectionPool::Result result_pool = dmConnectionPool::New(&params, &pool);
        ASSERT_EQ(dmConnectionPool::RESULT_OK, result_pool);
    }

    void CheckStats(uint32_t free, uint32_t connected, uint32_t in_use)
    {
        dmConnectionPool::Stats stats;
        dmConnectionPool::GetStats(pool, &stats);
        ASSERT_EQ(free, stats.m_Free);
        ASSERT_EQ(connected, stats.m_Connected);
        ASSERT_EQ(in_use, stats.m_InUse);
    }

    void TearDown() override
    {
        dmConnectionPool::Delete(pool);
    }
};

TEST_F(dmConnectionPoolTest, Basic)
{
}


// NOTE: Tests disabled. Currently we need bash to start and shutdown http server.

TEST_F(dmConnectionPoolTest, Connect)
{
    dmConnectionPool::HConnection c;
    dmSocket::Result sr;
    dmConnectionPool::Result r = dmConnectionPool::Dial(pool, g_HttpAddress, g_HttpPort, false, 0, &c, &sr);
    ASSERT_EQ(dmConnectionPool::RESULT_OK, r);
    dmSocket::Socket sock = dmConnectionPool::GetSocket(pool, c);
    ASSERT_NE(0, sock);
    dmSSLSocket::Socket sslsock = dmConnectionPool::GetSSLSocket(pool, c);
    ASSERT_EQ((void*)0, sslsock);
    dmConnectionPool::Close(pool, c);
}

TEST_F(dmConnectionPoolTest, ConnectSSL)
{
    dmConnectionPool::HConnection c;
    dmSocket::Result sr;
    dmConnectionPool::Result r = dmConnectionPool::Dial(pool, "www.defold.com", 443, true, 0, &c, &sr);
    ASSERT_EQ(dmConnectionPool::RESULT_OK, r);
    dmSocket::Socket sock = dmConnectionPool::GetSocket(pool, c);
    ASSERT_NE(0, sock);
    dmSSLSocket::Socket sslsock = dmConnectionPool::GetSSLSocket(pool, c);
    ASSERT_NE((void*)0, sslsock);
    dmConnectionPool::Close(pool, c);
}

TEST_F(dmConnectionPoolTest, CreateSSLSocket)
{
    dmConnectionPool::HConnection c;
    dmSocket::Result sr;
    dmConnectionPool::Result r = dmConnectionPool::Dial(pool, "www.defold.com", 443, false, 0, &c, &sr);
    ASSERT_EQ(dmConnectionPool::RESULT_OK, r);
    dmSocket::Socket sock = dmConnectionPool::GetSocket(pool, c);
    ASSERT_NE(0, sock);
    dmSSLSocket::Socket sslsock = dmConnectionPool::GetSSLSocket(pool, c);
    ASSERT_EQ((void*)0, sslsock);

    r = dmConnectionPool::CreateSSLSocket(pool, c, "www.defold.com", 5*1000000, &sr);
    ASSERT_EQ(dmConnectionPool::RESULT_OK, r);
    sslsock = dmConnectionPool::GetSSLSocket(pool, c);
    ASSERT_NE((void*)0, sslsock);
    dmConnectionPool::Close(pool, c);
}


TEST_F(dmConnectionPoolTest, MaxConnections)
{
    SCOPED_TRACE("");
    CheckStats(MAX_CONNECTIONS, 0, 0);

    std::vector<dmConnectionPool::HConnection> connections;
    for (uint32_t i = 0; i < MAX_CONNECTIONS; ++i) {
        dmConnectionPool::HConnection c;
        dmSocket::Result sr;
        dmConnectionPool::Result r = dmConnectionPool::Dial(pool, g_HttpAddress, g_HttpPort, false, 0, &c, &sr);
        ASSERT_EQ(dmConnectionPool::RESULT_OK, r);
        connections.push_back(c);
    }

    SCOPED_TRACE("");
    CheckStats(0, 0, MAX_CONNECTIONS);

    dmConnectionPool::HConnection c;
    dmSocket::Result sr;
    dmConnectionPool::Result r = dmConnectionPool::Dial(pool, g_HttpAddress, g_HttpPort, false, 0, &c, &sr);
    ASSERT_EQ(dmConnectionPool::RESULT_OUT_OF_RESOURCES, r);

    for (uint32_t i = 0; i < MAX_CONNECTIONS; ++i) {
        dmConnectionPool::HConnection c = connections[i];
        dmConnectionPool::Close(pool, c);
    }

    SCOPED_TRACE("");
    CheckStats(MAX_CONNECTIONS, 0, 0);
}

TEST_F(dmConnectionPoolTest, KeepAlive)
{
    SCOPED_TRACE("");
    CheckStats(MAX_CONNECTIONS, 0, 0);

    std::vector<dmConnectionPool::HConnection> connections;
    std::set<uint16_t> local_ports;
    for (uint32_t i = 0; i < MAX_CONNECTIONS; ++i) {
        dmConnectionPool::HConnection c;
        dmSocket::Result sr;
        dmConnectionPool::Result r = dmConnectionPool::Dial(pool, g_HttpAddress, g_HttpPort, false, 0, &c, &sr);
        ASSERT_EQ(dmConnectionPool::RESULT_OK, r);
        connections.push_back(c);
        dmSocket::Address a;
        uint16_t p;
        dmSocket::GetName(dmConnectionPool::GetSocket(pool, c), &a, &p);
        local_ports.insert(p);
    }

    SCOPED_TRACE("");
    CheckStats(0, 0, MAX_CONNECTIONS);

    dmConnectionPool::HConnection c;
    dmSocket::Result sr;
    dmConnectionPool::Result r = dmConnectionPool::Dial(pool, g_HttpAddress, g_HttpPort, false, 0, &c, &sr);
    ASSERT_EQ(dmConnectionPool::RESULT_OUT_OF_RESOURCES, r);

    for (uint32_t i = 0; i < MAX_CONNECTIONS; ++i) {
        dmConnectionPool::HConnection c = connections[i];
        dmConnectionPool::Return(pool, c);
    }

    SCOPED_TRACE("");
    CheckStats(0, MAX_CONNECTIONS, 0);

    connections.clear();
    for (uint32_t i = 0; i < MAX_CONNECTIONS; ++i) {
        dmConnectionPool::HConnection c;
        dmSocket::Result sr;
        dmConnectionPool::Result r = dmConnectionPool::Dial(pool, g_HttpAddress, g_HttpPort, false, 0, &c, &sr);
        ASSERT_EQ(dmConnectionPool::RESULT_OK, r);

        dmSocket::Address a;
        uint16_t p;
        dmSocket::GetName(dmConnectionPool::GetSocket(pool, c), &a, &p);
        ASSERT_TRUE(local_ports.find(p) != local_ports.end());
        connections.push_back(c);
    }

    SCOPED_TRACE("");
    CheckStats(0, 0, MAX_CONNECTIONS);

    for (uint32_t i = 0; i < MAX_CONNECTIONS; ++i) {
        dmConnectionPool::HConnection c = connections[i];
        dmConnectionPool::Close(pool, c);
    }

    SCOPED_TRACE("");
    CheckStats(MAX_CONNECTIONS, 0, 0);
}

TEST_F(dmConnectionPoolTest, KeepAliveTimeout)
{
    dmConnectionPool::SetMaxKeepAlive(pool, 1);
    SCOPED_TRACE("");
    CheckStats(MAX_CONNECTIONS, 0, 0);

    std::vector<dmConnectionPool::HConnection> connections;
    std::set<uint16_t> local_ports;
    for (uint32_t i = 0; i < MAX_CONNECTIONS; ++i) {
        dmConnectionPool::HConnection c;
        dmSocket::Result sr;
        dmConnectionPool::Result r = dmConnectionPool::Dial(pool, g_HttpAddress, g_HttpPort, false, 0, &c, &sr);
        ASSERT_EQ(dmConnectionPool::RESULT_OK, r);
        connections.push_back(c);

        dmSocket::Address a;
        uint16_t p;
        dmSocket::GetName(dmConnectionPool::GetSocket(pool, c), &a, &p);
        local_ports.insert(p);
    }

    SCOPED_TRACE("");
    CheckStats(0, 0, MAX_CONNECTIONS);

    dmConnectionPool::HConnection c;
    dmSocket::Result sr;
    dmConnectionPool::Result r = dmConnectionPool::Dial(pool, g_HttpAddress, g_HttpPort, false, 0, &c, &sr);
    ASSERT_EQ(dmConnectionPool::RESULT_OUT_OF_RESOURCES, r);

    for (uint32_t i = 0; i < MAX_CONNECTIONS; ++i) {
        dmConnectionPool::HConnection c = connections[i];
        dmConnectionPool::Return(pool, c);
    }

    SCOPED_TRACE("");
    CheckStats(0, MAX_CONNECTIONS, 0);

    dmTime::Sleep(1000000U);

    connections.clear();
    for (uint32_t i = 0; i < MAX_CONNECTIONS; ++i) {
        dmConnectionPool::HConnection c;
        dmSocket::Result sr;
        dmConnectionPool::Result r = dmConnectionPool::Dial(pool, g_HttpAddress, g_HttpPort, false, 0, &c, &sr);
        ASSERT_EQ(dmConnectionPool::RESULT_OK, r);
        dmSocket::Address a;
        uint16_t p;
        dmSocket::GetName(dmConnectionPool::GetSocket(pool, c), &a, &p);

        ASSERT_TRUE(local_ports.find(p) == local_ports.end());
        connections.push_back(c);
    }

    SCOPED_TRACE("");
    CheckStats(0, 0, MAX_CONNECTIONS);

    for (uint32_t i = 0; i < MAX_CONNECTIONS; ++i) {
        dmConnectionPool::HConnection c = connections[i];
        dmConnectionPool::Close(pool, c);
    }

    SCOPED_TRACE("");
    CheckStats(MAX_CONNECTIONS, 0, 0);
}

TEST_F(dmConnectionPoolTest, ConnectFailed)
{
    dmConnectionPool::HConnection c;
    dmSocket::Result sr;
    dmConnectionPool::Result r = dmConnectionPool::Dial(pool, g_HttpAddress, 1111, false, 0, &c, &sr);
    ASSERT_EQ(dmConnectionPool::RESULT_SOCKET_ERROR, r);
}

static void Usage()
{
    dmLogError("Usage: <exe> <config>");
    dmLogError("Be sure to start the http server before starting this test.");
    dmLogError("You can use the config file created by the server");
}

int main(int argc, char **argv)
{
    if(argc > 1)
    {
        char path[256];
        dmTestUtil::MakeHostPath(path, sizeof(path), argv[1]);

        dmConfigFile::HConfig config;
        if( dmConfigFile::Load(path, argc, (const char**)argv, &config) != dmConfigFile::RESULT_OK )
        {
            dmLogError("Could not read config file '%s'", path);
            Usage();
            return 1;
        }
        dmTestUtil::GetSocketsFromConfig(config, &g_HttpPort, 0, 0);
        if (!dmTestUtil::GetIpFromConfig(config, g_HttpAddress, sizeof(g_HttpAddress))) {
            dmLogError("Failed to get server ip!");
        } else {
            dmLogInfo("Server ip: %s", g_HttpAddress);
        }
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
    jc_test_init(&argc, argv);
    int ret = jc_test_run_all();
    dmSSLSocket::Finalize();
    dmSocket::Finalize();
    return ret;
}
