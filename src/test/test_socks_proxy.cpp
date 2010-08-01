#include <stdint.h>
#include <stdlib.h>
#include <gtest/gtest.h>
#include "../src/dlib/socks_proxy.h"

TEST(dmSocksProxy, NoSocksProxySet)
{
    unsetenv("DMSOCKS_PROXY");
    dmSocket::Address local_address = dmSocket::AddressFromIPString("127.0.0.1");

    dmSocket::Socket socket;
    dmSocket::Result socket_result;
    dmSocksProxy::Result r = dmSocksProxy::Connect(local_address, 1122, &socket, &socket_result);
    ASSERT_EQ(r, dmSocksProxy::RESULT_NO_DMSOCKS_PROXY_SET);
}

TEST(dmSocksProxy, ConnectionRefused)
{
    setenv("DMSOCKS_PROXY", "localhost", 1);

    dmSocket::Address local_address = dmSocket::AddressFromIPString("127.0.0.1");

    dmSocket::Socket socket;
    dmSocket::Result socket_result;
    dmSocksProxy::Result r = dmSocksProxy::Connect(local_address, 1122, &socket, &socket_result);
    ASSERT_EQ(r, dmSocksProxy::RESULT_SOCKET_ERROR);
    ASSERT_EQ(socket_result, dmSocket::RESULT_CONNREFUSED);
}

TEST(dmSocksProxy, ClientNotReachable)
{
    setenv("DMSOCKS_PROXY", "localhost", 1);
    setenv("DMSOCKS_PROXY_PORT", "1081", 1);

    dmSocket::Address local_address = dmSocket::AddressFromIPString("127.0.0.1");

    dmSocket::Socket socket;
    dmSocket::Result socket_result;
    dmSocksProxy::Result r = dmSocksProxy::Connect(local_address, 1122, &socket, &socket_result);
    ASSERT_EQ(r, dmSocksProxy::RESULT_NOT_REACHABLE);
}

TEST(dmSocksProxy, Connect)
{
    setenv("DMSOCKS_PROXY", "localhost", 1);
    setenv("DMSOCKS_PROXY_PORT", "1081", 1);

    dmSocket::Address local_address;
    dmSocket::GetHostByName("www.svd.se", &local_address);

    dmSocket::Socket socket;
    dmSocket::Result socket_result;
    dmSocksProxy::Result r = dmSocksProxy::Connect(local_address, 80, &socket, &socket_result);
    ASSERT_EQ(r, dmSocksProxy::RESULT_OK);
}

int main(int argc, char **argv)
{
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
