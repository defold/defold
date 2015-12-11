#include <stdint.h>
#include <gtest/gtest.h>
#include <vector>
#include <set>
#include "../dlib/connection_pool.h"
#include "../dlib/log.h"
#include "../dlib/time.h"

#include <dlib/sol.h>

static const uint16_t HTTP_PORT = 7000;
static const uint32_t MAX_CONNECTIONS = 8;

class dmConnectionPoolTest: public ::testing::Test
{
public:

    dmConnectionPool::HPool pool;
    virtual void SetUp()
    {
        dmConnectionPool::Params params;
        params.m_MaxConnections = MAX_CONNECTIONS;
        dmConnectionPool::Result r = dmConnectionPool::New(&params, &pool);
        ASSERT_EQ(dmConnectionPool::RESULT_OK, r);
    }

    void CheckStats(uint32_t free, uint32_t connected, uint32_t in_use)
    {
        dmConnectionPool::Stats stats;
        dmConnectionPool::GetStats(pool, &stats);
        ASSERT_EQ(free, stats.m_Free);
        ASSERT_EQ(connected, stats.m_Connected);
        ASSERT_EQ(in_use, stats.m_InUse);
    }

    virtual void TearDown()
    {
        dmConnectionPool::Delete(pool);
    }
};

TEST_F(dmConnectionPoolTest, Basic)
{
}

#ifndef _WIN32

// NOTE: Tests disabled. Currently we need bash to start and shutdown http server.

TEST_F(dmConnectionPoolTest, Connect)
{
    dmConnectionPool::HConnection c;
    dmSocket::Result sr;
    dmConnectionPool::Result r = dmConnectionPool::Dial(pool, "localhost", HTTP_PORT, false, 0, &c, &sr);
    ASSERT_EQ(dmConnectionPool::RESULT_OK, r);
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
        dmConnectionPool::Result r = dmConnectionPool::Dial(pool, "localhost", HTTP_PORT, false, 0, &c, &sr);
        ASSERT_EQ(dmConnectionPool::RESULT_OK, r);
        connections.push_back(c);
    }

    SCOPED_TRACE("");
    CheckStats(0, 0, MAX_CONNECTIONS);

    dmConnectionPool::HConnection c;
    dmSocket::Result sr;
    dmConnectionPool::Result r = dmConnectionPool::Dial(pool, "localhost", HTTP_PORT, false, 0, &c, &sr);
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
        dmConnectionPool::Result r = dmConnectionPool::Dial(pool, "localhost", HTTP_PORT, false, 0, &c, &sr);
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
    dmConnectionPool::Result r = dmConnectionPool::Dial(pool, "localhost", HTTP_PORT, false, 0, &c, &sr);
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
        dmConnectionPool::Result r = dmConnectionPool::Dial(pool, "localhost", HTTP_PORT, false, 0, &c, &sr);
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
        dmConnectionPool::Result r = dmConnectionPool::Dial(pool, "localhost", HTTP_PORT, false, 0, &c, &sr);
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
    dmConnectionPool::Result r = dmConnectionPool::Dial(pool, "localhost", HTTP_PORT, false, 0, &c, &sr);
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
        dmConnectionPool::Result r = dmConnectionPool::Dial(pool, "localhost", HTTP_PORT, false, 0, &c, &sr);
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

#endif

TEST_F(dmConnectionPoolTest, ConnectFailed)
{
    dmConnectionPool::HConnection c;
    dmSocket::Result sr;
    dmConnectionPool::Result r = dmConnectionPool::Dial(pool, "localhost", 1111, false, 0, &c, &sr);
    ASSERT_EQ(dmConnectionPool::RESULT_SOCKET_ERROR, r);
}

int main(int argc, char **argv)
{
    dmSol::Initialize();
    dmLogSetlevel(DM_LOG_SEVERITY_INFO);
    dmSocket::Initialize();
    testing::InitGoogleTest(&argc, argv);
    int ret = RUN_ALL_TESTS();
    dmSocket::Finalize();
    dmSol::FinalizeWithCheck();
    return ret;
}
