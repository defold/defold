#include <stdint.h>
#include <stdlib.h>
#include <gtest/gtest.h>
#include "../src/dlib/socks_proxy.h"

#ifndef _WIN32

const char*    CONST_LOOPBACK_ADDRESS_IPV4       = "127.0.0.1";
const char*    CONST_LOOPBACK_ADDRESS_IPV6       = "::1";

// unsetenv and setenv is missing on win32
// tests temporarily disabled

TEST(dmSocksProxy, NoSocksProxySet)
{
    unsetenv("DMSOCKS_PROXY");
    dmSocket::Address local_address = dmSocket::AddressFromIPString(CONST_LOOPBACK_ADDRESS_IPV4);

    dmSocket::Socket socket;
    dmSocket::Result socket_result;
    dmSocksProxy::Result r = dmSocksProxy::Connect(local_address, 1122, &socket, &socket_result);
    ASSERT_EQ(r, dmSocksProxy::RESULT_NO_DMSOCKS_PROXY_SET);
}

TEST(dmSocksProxy, ConnectionRefused)
{
    setenv("DMSOCKS_PROXY", "localhost", 1);
    setenv("DMSOCKS_PROXY_PORT", "1079", 1);

    dmSocket::Address local_address = dmSocket::AddressFromIPString(CONST_LOOPBACK_ADDRESS_IPV4);

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

    dmSocket::Address local_address = dmSocket::AddressFromIPString(CONST_LOOPBACK_ADDRESS_IPV4);

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
    dmSocket::GetHostByName("localhost", &local_address);

    dmSocket::Socket socket;
    dmSocket::Result socket_result;
    dmSocksProxy::Result r = dmSocksProxy::Connect(local_address, 7000, &socket, &socket_result);
    ASSERT_EQ(r, dmSocksProxy::RESULT_OK);
}

#endif

int main(int argc, char **argv)
{
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
