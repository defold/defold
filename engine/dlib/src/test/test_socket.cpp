#include <stdint.h>
#include <string.h>
#include <gtest/gtest.h>
#include "../dlib/socket.h"
#include "../dlib/thread.h"
#include "../dlib/time.h"
#include <arpa/inet.h>


inline dmSocket::Socket GetSocket(dmSocket::Domain domain)
{
    dmSocket::Socket instance = 0;
    uint64_t timeout = 3000; // milliseconds
    dmSocket::Result result = dmSocket::RESULT_OK;

    result = dmSocket::New(domain, dmSocket::TYPE_STREAM, dmSocket::PROTOCOL_TCP, &instance);
    if (result != dmSocket::RESULT_OK) return -1;
    
    result = dmSocket::SetSendTimeout(instance, timeout);
    if (result != dmSocket::RESULT_OK) return -1;

    result = dmSocket::SetReceiveTimeout(instance, timeout);
    if (result != dmSocket::RESULT_OK) return -1;

    result = dmSocket::SetNoDelay(instance, true);
    if (result != dmSocket::RESULT_OK) return -1;

    return instance;
}

TEST(Socket, NetworkOrder)
{
    dmSocket::Address address;
    const char* hostname = "localhost";
    dmSocket::Result result = dmSocket::RESULT_OK;

    result = dmSocket::GetHostByName(hostname, &address, true, false);
    ASSERT_EQ(dmSocket::RESULT_OK, result);

    // This checks so our format is in network order.
    ASSERT_EQ(inet_addr("127.0.0.1"), address.m_address[3]);
}

TEST(Socket, IPv4)
{
    dmSocket::Address instance;
    instance.m_family = dmSocket::DOMAIN_IPV4;
    ASSERT_EQ(&instance.m_address[3], dmSocket::IPv4(&instance));
}

TEST(Socket, IPv6)
{
    dmSocket::Address instance;
    instance.m_family = dmSocket::DOMAIN_IPV6;
    ASSERT_EQ(&instance.m_address[0], dmSocket::IPv6(&instance));
}

TEST(Socket, New_IPv4)
{
    dmSocket::Socket instance = 0;
    dmSocket::Result result = dmSocket::RESULT_OK;

    result = dmSocket::New(dmSocket::DOMAIN_IPV4, dmSocket::TYPE_STREAM, dmSocket::PROTOCOL_TCP, &instance);
    ASSERT_EQ(dmSocket::RESULT_OK, result);
    ASSERT_NE(-1, instance);
    ASSERT_TRUE(dmSocket::IsSocketIPv4(instance));
    ASSERT_FALSE(dmSocket::IsSocketIPv6(instance));
}

TEST(Socket, New_IPv6)
{
    dmSocket::Socket instance = 0;
    dmSocket::Result result = dmSocket::RESULT_OK;

    result = dmSocket::New(dmSocket::DOMAIN_IPV6, dmSocket::TYPE_STREAM, dmSocket::PROTOCOL_TCP, &instance);
    ASSERT_EQ(dmSocket::RESULT_OK, result);
    ASSERT_NE(-1, instance);
    ASSERT_TRUE(dmSocket::IsSocketIPv6(instance));
    ASSERT_FALSE(dmSocket::IsSocketIPv4(instance));
}

TEST(Socket, New_InvalidDomain)
{
    dmSocket::Socket instance = 0;
    dmSocket::Result result = dmSocket::RESULT_OK;

    result = dmSocket::New(dmSocket::DOMAIN_UNKNOWN, dmSocket::TYPE_STREAM, dmSocket::PROTOCOL_TCP, &instance);
    ASSERT_EQ(dmSocket::RESULT_AFNOSUPPORT, result);
    ASSERT_EQ(-1, instance);
    ASSERT_FALSE(dmSocket::IsSocketIPv6(instance));
    ASSERT_FALSE(dmSocket::IsSocketIPv4(instance));
}

TEST(Socket, SetReuseAddress_IPv4)
{
    dmSocket::Result result = dmSocket::RESULT_OK;
    dmSocket::Socket instance = GetSocket(dmSocket::DOMAIN_IPV4);

    result = dmSocket::SetReuseAddress(instance, true);
    ASSERT_EQ(dmSocket::RESULT_OK, result);
}

TEST(Socket, SetReuseAddress_IPv6)
{
    dmSocket::Result result = dmSocket::RESULT_OK;
    dmSocket::Socket instance = GetSocket(dmSocket::DOMAIN_IPV6);

    result = dmSocket::SetReuseAddress(instance, true);
    ASSERT_EQ(dmSocket::RESULT_OK, result);
}

TEST(Socket, AddMembership_IPv4)
{
    dmSocket::Result result = dmSocket::RESULT_OK;
    dmSocket::Socket instance = GetSocket(dmSocket::DOMAIN_IPV4);

    ASSERT_FALSE("This test has not been implemented yet");
}

TEST(Socket, AddMembership_IPv6)
{
    dmSocket::Result result = dmSocket::RESULT_OK;
    dmSocket::Socket instance = GetSocket(dmSocket::DOMAIN_IPV6);

    ASSERT_FALSE("This test has not been implemented yet");
}

TEST(Socket, SetMulticastIf_IPv4)
{
    dmSocket::Result result = dmSocket::RESULT_OK;
    dmSocket::Socket instance = GetSocket(dmSocket::DOMAIN_IPV4);

    ASSERT_FALSE("This test has not been implemented yet");
}

TEST(Socket, SetMulticastIf_IPv6)
{
    dmSocket::Result result = dmSocket::RESULT_OK;
    dmSocket::Socket instance = GetSocket(dmSocket::DOMAIN_IPV6);

    ASSERT_FALSE("This test has not been implemented yet");
}

TEST(Socket, Delete_IPv4)
{
    dmSocket::Result result = dmSocket::RESULT_OK;
    dmSocket::Socket instance = GetSocket(dmSocket::DOMAIN_IPV4);

    result = dmSocket::Delete(instance);
    ASSERT_EQ(dmSocket::RESULT_OK, result);
}

TEST(Socket, Delete_IPv6)
{
    dmSocket::Result result = dmSocket::RESULT_OK;
    dmSocket::Socket instance = GetSocket(dmSocket::DOMAIN_IPV6);

    result = dmSocket::Delete(instance);
    ASSERT_EQ(dmSocket::RESULT_OK, result);
}

TEST(Socket, Delete_InvalidSocket)
{
    dmSocket::Socket instance = -1;
    dmSocket::Result result = dmSocket::RESULT_OK;

    result = dmSocket::Delete(instance);
    ASSERT_EQ(dmSocket::RESULT_BADF, result);
}

// Accept

// Bind

TEST(Socket, Connect_IPv4)
{
    dmSocket::Socket instance = GetSocket(dmSocket::DOMAIN_IPV4);
    dmSocket::Result result = dmSocket::RESULT_OK;
    const char* hostname = "localhost";
    dmSocket::Address address;
    uint16_t port = 8008;

    result = dmSocket::GetHostByName(hostname, &address, true, false);
    ASSERT_EQ(dmSocket::RESULT_OK, result);

    result = dmSocket::Connect(instance, address, port);
    ASSERT_EQ(dmSocket::RESULT_OK, result) << "  Address: " << dmSocket::AddressToIPString(address);
}

TEST(Socket, Connect_IPv6)
{
    dmSocket::Socket instance = GetSocket(dmSocket::DOMAIN_IPV6);
    dmSocket::Result result = dmSocket::RESULT_OK;
    const char* hostname = "ipv6-test.com";
    dmSocket::Address address;
    uint16_t port = 80;

    result = dmSocket::GetHostByName(hostname, &address, false, true);
    ASSERT_EQ(dmSocket::RESULT_OK, result);

    result = dmSocket::Connect(instance, address, port);
    ASSERT_EQ(dmSocket::RESULT_OK, result) << "  Address: " << dmSocket::AddressToIPString(address);
}

TEST(Socket, Connect_IPv4_ConnectionRefused)
{
    dmSocket::Socket instance = GetSocket(dmSocket::DOMAIN_IPV4);
    dmSocket::Result result = dmSocket::RESULT_OK;
    const char* hostname = "localhost";
    dmSocket::Address address;
    uint16_t port = 47204; // We just assume that this port is not listening...

    result = dmSocket::GetHostByName(hostname, &address, true, false);
    ASSERT_EQ(dmSocket::RESULT_OK, result);

    result = dmSocket::Connect(instance, address, port);
    ASSERT_EQ(dmSocket::RESULT_CONNREFUSED, result);
}

TEST(Socket, Connect_IPv6_ConnectionRefused)
{
    dmSocket::Socket instance = GetSocket(dmSocket::DOMAIN_IPV6);
    dmSocket::Result result = dmSocket::RESULT_OK;
    const char* hostname = "localhost";
    dmSocket::Address address;
    uint16_t port = 47204; // We just assume that this port is not listening...

    result = dmSocket::GetHostByName(hostname, &address, false, true);
    ASSERT_EQ(dmSocket::RESULT_OK, result);

    result = dmSocket::Connect(instance, address, port);
    ASSERT_EQ(dmSocket::RESULT_CONNREFUSED, result);
}

// Listen

// Shutdown

TEST(Socket, GetName_IPv4)
{
    dmSocket::Socket instance = GetSocket(dmSocket::DOMAIN_IPV4);
    dmSocket::Result result = dmSocket::RESULT_OK;
    dmSocket::Address address;
    uint16_t port = 0;

    result = dmSocket::GetName(instance, &address, &port);
    ASSERT_EQ(dmSocket::RESULT_OK, result);
    ASSERT_EQ(dmSocket::DOMAIN_IPV4, address.m_family);
    ASSERT_EQ(0, *dmSocket::IPv4(&address));
    ASSERT_EQ(0, port);
}

TEST(Socket, GetName_IPv6)
{
    dmSocket::Socket instance = GetSocket(dmSocket::DOMAIN_IPV6);
    dmSocket::Result result = dmSocket::RESULT_OK;
    dmSocket::Address address;
    uint16_t port = 0;

    result = dmSocket::GetName(instance, &address, &port);
    ASSERT_EQ(dmSocket::RESULT_OK, result);
    ASSERT_EQ(dmSocket::DOMAIN_IPV6, address.m_family);
    ASSERT_EQ(0, address.m_address[0]);
    ASSERT_EQ(0, address.m_address[1]);
    ASSERT_EQ(0, address.m_address[2]);
    ASSERT_EQ(0, address.m_address[3]);
    ASSERT_EQ(0, port);
}

TEST(Socket, GetName_IPv4_Connected)
{
    dmSocket::Socket instance = GetSocket(dmSocket::DOMAIN_IPV4);
    dmSocket::Result result = dmSocket::RESULT_OK;
    const char* hostname = "localhost";
    dmSocket::Address address;
    uint16_t port = 8008;

    result = dmSocket::GetHostByName(hostname, &address, dmSocket::IsSocketIPv4(instance), dmSocket::IsSocketIPv6(instance));
    ASSERT_EQ(dmSocket::RESULT_OK, result);

    result = dmSocket::Connect(instance, address, port);
    ASSERT_EQ(dmSocket::RESULT_OK, result);

    uint16_t actual_port = 0;
    dmSocket::Address actual_address;
    result = dmSocket::GetName(instance, &actual_address, &actual_port);
    ASSERT_EQ(dmSocket::RESULT_OK, result);
    ASSERT_EQ(address.m_family, actual_address.m_family);
    ASSERT_EQ(*dmSocket::IPv4(&address), *dmSocket::IPv4(&actual_address));
    ASSERT_EQ(port, actual_port);
}

TEST(Socket, GetName_IPv6_Connected)
{
    dmSocket::Socket instance = GetSocket(dmSocket::DOMAIN_IPV6);
    dmSocket::Result result = dmSocket::RESULT_OK;
    const char* hostname = "localhost";
    dmSocket::Address address;
    uint16_t port = 8008;

    result = dmSocket::GetHostByName(hostname, &address, dmSocket::IsSocketIPv4(instance), dmSocket::IsSocketIPv6(instance));
    ASSERT_EQ(dmSocket::RESULT_OK, result);

    result = dmSocket::Connect(instance, address, port);
    ASSERT_EQ(dmSocket::RESULT_OK, result);

    uint16_t actual_port = 0;
    dmSocket::Address actual_address;
    result = dmSocket::GetName(instance, &actual_address, &actual_port);
    ASSERT_EQ(dmSocket::RESULT_OK, result);
    ASSERT_EQ(address.m_family, actual_address.m_family);
    ASSERT_EQ(address.m_address[0], actual_address.m_address[0]);
    ASSERT_EQ(address.m_address[1], actual_address.m_address[1]);
    ASSERT_EQ(address.m_address[2], actual_address.m_address[2]);
    ASSERT_EQ(address.m_address[3], actual_address.m_address[3]);
    ASSERT_EQ(port, actual_port);
}

TEST(Socket, SetBlocking_IPv4)
{
    dmSocket::Socket instance = GetSocket(dmSocket::DOMAIN_IPV4);
    dmSocket::Result result = dmSocket::RESULT_OK;

    result = dmSocket::SetBlocking(instance, true);
    ASSERT_EQ(dmSocket::RESULT_OK, result);

    result = dmSocket::SetBlocking(instance, false);
    ASSERT_EQ(dmSocket::RESULT_OK, result);
}

TEST(Socket, SetBlocking_IPv6)
{
    dmSocket::Socket instance = GetSocket(dmSocket::DOMAIN_IPV6);
    dmSocket::Result result = dmSocket::RESULT_OK;

    result = dmSocket::SetBlocking(instance, true);
    ASSERT_EQ(dmSocket::RESULT_OK, result);

    result = dmSocket::SetBlocking(instance, false);
    ASSERT_EQ(dmSocket::RESULT_OK, result);
}

TEST(Socket, SetNoDelay_IPv4)
{
    dmSocket::Socket instance = GetSocket(dmSocket::DOMAIN_IPV4);
    dmSocket::Result result = dmSocket::RESULT_OK;

    result = dmSocket::SetNoDelay(instance, true);
    ASSERT_EQ(dmSocket::RESULT_OK, result);

    result = dmSocket::SetNoDelay(instance, false);
    ASSERT_EQ(dmSocket::RESULT_OK, result);
}

TEST(Socket, SetNoDelay_IPv6)
{
    dmSocket::Socket instance = GetSocket(dmSocket::DOMAIN_IPV6);
    dmSocket::Result result = dmSocket::RESULT_OK;

    result = dmSocket::SetNoDelay(instance, true);
    ASSERT_EQ(dmSocket::RESULT_OK, result);

    result = dmSocket::SetNoDelay(instance, false);
    ASSERT_EQ(dmSocket::RESULT_OK, result);
}

TEST(Socket, SetTimeout_IPv4)
{
    dmSocket::Socket instance = 0;
    dmSocket::Result result = dmSocket::RESULT_OK;
    uint64_t timeout = 2000;

    result = dmSocket::New(dmSocket::DOMAIN_IPV4, dmSocket::TYPE_STREAM, dmSocket::PROTOCOL_TCP, &instance);
    ASSERT_EQ(dmSocket::RESULT_OK, result);

    result = dmSocket::SetSendTimeout(instance, timeout);
    ASSERT_EQ(dmSocket::RESULT_OK, result);

    result = dmSocket::SetNoDelay(instance, timeout);
    ASSERT_EQ(dmSocket::RESULT_OK, result);
}

TEST(Socket, SetTimeout_IPv6)
{
    dmSocket::Socket instance = 0;
    dmSocket::Result result = dmSocket::RESULT_OK;
    uint64_t timeout = 2000;

    result = dmSocket::New(dmSocket::DOMAIN_IPV6, dmSocket::TYPE_STREAM, dmSocket::PROTOCOL_TCP, &instance);
    ASSERT_EQ(dmSocket::RESULT_OK, result);

    result = dmSocket::SetSendTimeout(instance, timeout);
    ASSERT_EQ(dmSocket::RESULT_OK, result);

    result = dmSocket::SetNoDelay(instance, timeout);
    ASSERT_EQ(dmSocket::RESULT_OK, result);
}

TEST(Socket, AddressToIPString_IPv4)
{
    dmSocket::Address address;
    address.m_family = dmSocket::DOMAIN_IPV4;
    address.m_address[3] = 0x0100007f; // 127.0.0.1 in network order

    char* actual = dmSocket::AddressToIPString(address);
    ASSERT_EQ(9, strlen(actual));
    ASSERT_EQ(0, memcmp("127.0.0.1", actual, 9));
}

TEST(Socket, AddressToIPString_IPv6_Empty)
{
    dmSocket::Address address;
    address.m_family = dmSocket::DOMAIN_IPV6;
    address.m_address[0] = 0x00000000;
    address.m_address[1] = 0x00000000;
    address.m_address[2] = 0x00000000;
    address.m_address[3] = 0x00000000;

    char* actual = dmSocket::AddressToIPString(address);
    ASSERT_EQ(2, strlen(actual));
    ASSERT_EQ(0, memcmp("::", actual, 2));
}

TEST(Socket, AddressToIPString_IPv6_Localhost)
{
    dmSocket::Address address;
    address.m_family = dmSocket::DOMAIN_IPV6;

    address.m_address[0] = 0x00000000;
    address.m_address[1] = 0x00000000;
    address.m_address[2] = 0x00000000;
    address.m_address[3] = 0x01000000;

    char* actual = dmSocket::AddressToIPString(address);
    ASSERT_EQ(3, strlen(actual));
    ASSERT_EQ(0, memcmp("::1", actual, 3));
}

TEST(Socket, AddressToIPString_IPv6_FullAddress)
{
    dmSocket::Address address;
    address.m_family = dmSocket::DOMAIN_IPV6;

    address.m_address[0] = 0xd0410120;
    address.m_address[1] = 0xade80800;
    address.m_address[2] = 0x00000000;
    address.m_address[3] = 0x01000000;

    char* actual = dmSocket::AddressToIPString(address);
    ASSERT_EQ(19, strlen(actual));
    ASSERT_EQ(0, memcmp("2001:41d0:8:e8ad::1", actual, 19));
}

TEST(Socket, GetHostByName_IPv4_Localhost)
{
    dmSocket::Address address;
    dmSocket::Result result = dmSocket::RESULT_OK;
    const char* hostname = "localhost";

    result = dmSocket::GetHostByName(hostname, &address, true, false);
    ASSERT_EQ(dmSocket::RESULT_OK, result);
    ASSERT_EQ(dmSocket::DOMAIN_IPV4, address.m_family);
    ASSERT_EQ(0x0100007f, *dmSocket::IPv4(&address));
}

TEST(Socket, GetHostByName_IPv6_Localhost)
{
    dmSocket::Address address;
    dmSocket::Result result = dmSocket::RESULT_OK;
    const char* hostname = "localhost";

    result = dmSocket::GetHostByName(hostname, &address, false, true);
    ASSERT_EQ(dmSocket::RESULT_OK, result);
    ASSERT_EQ(dmSocket::DOMAIN_IPV6, address.m_family);
    ASSERT_EQ(0x00000000, address.m_address[0]);
    ASSERT_EQ(0x00000000, address.m_address[1]);
    ASSERT_EQ(0x00000000, address.m_address[2]);
    ASSERT_EQ(0x01000000, address.m_address[3]);
}

TEST(Socket, GetHostByName_IPv4_Unavailable)
{
    dmSocket::Address address;
    dmSocket::Result result = dmSocket::RESULT_OK;
    const char* hostname = "localhost.invalid";

    result = dmSocket::GetHostByName(hostname, &address, true, false);
    ASSERT_EQ(dmSocket::RESULT_HOST_NOT_FOUND, result);
}

TEST(Socket, GetHostByName_IPv6_Unavailable)
{
    dmSocket::Address address;
    dmSocket::Result result = dmSocket::RESULT_OK;
    const char* hostname = "localhost.invalid";

    result = dmSocket::GetHostByName(hostname, &address, false, true);
    ASSERT_EQ(dmSocket::RESULT_HOST_NOT_FOUND, result);
}

TEST(Socket, GetHostByName_NoValidAddressFamily)
{
    dmSocket::Address address;
    dmSocket::Result result = dmSocket::RESULT_OK;
    const char* hostname = "localhost";

    result = dmSocket::GetHostByName(hostname, &address, false, false);
    ASSERT_EQ(dmSocket::RESULT_UNKNOWN, result);
}

















TEST(Socket, LocalhostLookup)
{
    dmSocket::Address instance = dmSocket::AddressFromIPString("localhost");
    uint32_t expected[4] = { 0x0, 0x0, 0x0, 0x0001 };
    for (int i = 0; i < (sizeof(expected) / sizeof(uint32_t)); ++i)
    {
        EXPECT_EQ(expected[i], instance.m_address[i])
            << "  Lookup: " << instance << std::endl;
    }
}

TEST(Socket, GlobalLookup)
{
    dmSocket::Address instance = dmSocket::AddressFromIPString("::");
    uint32_t expected[4] = { 0x0, 0x0, 0x0, 0x0 };
    for (int i = 0; i < (sizeof(expected) / sizeof(uint32_t)); ++i)
    {
        EXPECT_EQ(expected[i], instance.m_address[i])
            << "  Lookup: " << instance << std::endl;
    }
}

TEST(Socket, Basic1)
{
    dmSocket::Address local_address = dmSocket::AddressFromIPString("localhost");

    dmSocket::Socket socket;
    dmSocket::Result r = dmSocket::New(local_address.m_family, dmSocket::TYPE_STREAM, dmSocket::PROTOCOL_TCP, &socket);
    EXPECT_EQ(r, dmSocket::RESULT_OK);

    r = dmSocket::Connect(socket, local_address, 1);
    EXPECT_EQ(dmSocket::RESULT_CONNREFUSED, r)
        << "  Expected(" << dmSocket::ResultToString(dmSocket::RESULT_CONNREFUSED) << "), Actual(" << dmSocket::ResultToString(r) << ")" << std::endl;

    r = dmSocket::Delete(socket);
    ASSERT_EQ(dmSocket::RESULT_OK, r)
        << "  Expected(" << dmSocket::ResultToString(dmSocket::RESULT_OK) << "), Actual(" << dmSocket::ResultToString(r) << ")" << std::endl;
}

TEST(Socket, GetHostByName1)
{
    dmSocket::Address local_address = dmSocket::AddressFromIPString("localhost");

    dmSocket::Address a;
    dmSocket::Result r = dmSocket::GetHostByName("localhost", &a);
    ASSERT_EQ(dmSocket::RESULT_OK, r)
        << "  Expected(" << dmSocket::ResultToString(dmSocket::RESULT_OK) << "), Actual(" << dmSocket::ResultToString(r) << ")" << std::endl;
    EXPECT_EQ(local_address, a);
}

TEST(Socket, GetHostByName2)
{
    dmSocket::Address a;
    dmSocket::Result r = dmSocket::GetHostByName("host.nonexistingdomain", &a);
    ASSERT_EQ(dmSocket::RESULT_HOST_NOT_FOUND, r)
        << "  Expected(" << dmSocket::ResultToString(dmSocket::RESULT_HOST_NOT_FOUND) << "), Actual(" << dmSocket::ResultToString(r) << ")" << std::endl;
}

TEST(Socket, ServerSocketIPv4)
{
    dmSocket::Socket socket;
    dmSocket::Result r = dmSocket::New(dmSocket::DOMAIN_IPV4, dmSocket::TYPE_STREAM, dmSocket::PROTOCOL_TCP, &socket);
    ASSERT_EQ(dmSocket::RESULT_OK, r)
        << "  Expected(" << dmSocket::ResultToString(dmSocket::RESULT_OK) << "), Actual(" << dmSocket::ResultToString(r) << ")" << std::endl;

    const int port = 9000;

    r = dmSocket::Bind(socket, dmSocket::AddressFromIPString("0.0.0.0"), port);
    ASSERT_EQ(dmSocket::RESULT_OK, r)
        << "  Expected(" << dmSocket::ResultToString(dmSocket::RESULT_OK) << "), Actual(" << dmSocket::ResultToString(r) << ")" << std::endl;

    r = dmSocket::Listen(socket, 1000);
    ASSERT_EQ(dmSocket::RESULT_OK, r)
        << "  Expected(" << dmSocket::ResultToString(dmSocket::RESULT_OK) << "), Actual(" << dmSocket::ResultToString(r) << ")" << std::endl;

    r = dmSocket::Delete(socket);
    ASSERT_EQ(dmSocket::RESULT_OK, r)
        << "  Expected(" << dmSocket::ResultToString(dmSocket::RESULT_OK) << "), Actual(" << dmSocket::ResultToString(r) << ")" << std::endl;
}

TEST(Socket, ServerSocketIPv6)
{
    dmSocket::Socket socket;
    dmSocket::Result r = dmSocket::New(dmSocket::DOMAIN_IPV6, dmSocket::TYPE_STREAM, dmSocket::PROTOCOL_TCP, &socket);
    ASSERT_EQ(dmSocket::RESULT_OK, r)
        << "  Expected(" << dmSocket::ResultToString(dmSocket::RESULT_OK) << "), Actual(" << dmSocket::ResultToString(r) << ")" << std::endl;

    const int port = 9000;

    r = dmSocket::Bind(socket, dmSocket::AddressFromIPString("::"), port);
    ASSERT_EQ(dmSocket::RESULT_OK, r)
        << "  Expected(" << dmSocket::ResultToString(dmSocket::RESULT_OK) << "), Actual(" << dmSocket::ResultToString(r) << ")" << std::endl;

    r = dmSocket::Listen(socket, 1000);
    ASSERT_EQ(dmSocket::RESULT_OK, r)
        << "  Expected(" << dmSocket::ResultToString(dmSocket::RESULT_OK) << "), Actual(" << dmSocket::ResultToString(r) << ")" << std::endl;

    r = dmSocket::Delete(socket);
    ASSERT_EQ(dmSocket::RESULT_OK, r)
        << "  Expected(" << dmSocket::ResultToString(dmSocket::RESULT_OK) << "), Actual(" << dmSocket::ResultToString(r) << ")" << std::endl;
}

TEST(Socket, ServerSocketIPv4_MultipleBind)
{
    dmSocket::Socket socket1, socket2;
    dmSocket::Result r;
    r = dmSocket::New(dmSocket::DOMAIN_IPV4, dmSocket::TYPE_STREAM, dmSocket::PROTOCOL_TCP, &socket1);
    ASSERT_EQ(dmSocket::RESULT_OK, r)
        << "  Expected(" << dmSocket::ResultToString(dmSocket::RESULT_OK) << "), Actual(" << dmSocket::ResultToString(r) << ")" << std::endl;
    r = dmSocket::New(dmSocket::DOMAIN_IPV4, dmSocket::TYPE_STREAM, dmSocket::PROTOCOL_TCP, &socket2);
    ASSERT_EQ(dmSocket::RESULT_OK, r)
        << "  Expected(" << dmSocket::ResultToString(dmSocket::RESULT_OK) << "), Actual(" << dmSocket::ResultToString(r) << ")" << std::endl;

    const int port = 9000;

    r = dmSocket::Bind(socket1, dmSocket::AddressFromIPString("0.0.0.0"), port);
    ASSERT_EQ(dmSocket::RESULT_OK, r)
        << "  Expected(" << dmSocket::ResultToString(dmSocket::RESULT_OK) << "), Actual(" << dmSocket::ResultToString(r) << ")" << std::endl;

    r = dmSocket::Bind(socket2, dmSocket::AddressFromIPString("0.0.0.0"), port);
    ASSERT_EQ(dmSocket::RESULT_ADDRINUSE, r)
        << "  Expected(" << dmSocket::ResultToString(dmSocket::RESULT_ADDRINUSE) << "), Actual(" << dmSocket::ResultToString(r) << ")" << std::endl;

    r = dmSocket::Delete(socket1);
    ASSERT_EQ(dmSocket::RESULT_OK, r)
        << "  Expected(" << dmSocket::ResultToString(dmSocket::RESULT_OK) << "), Actual(" << dmSocket::ResultToString(r) << ")" << std::endl;

    r = dmSocket::Delete(socket2);
    ASSERT_EQ(dmSocket::RESULT_OK, r)
        << "  Expected(" << dmSocket::ResultToString(dmSocket::RESULT_OK) << "), Actual(" << dmSocket::ResultToString(r) << ")" << std::endl;
}

TEST(Socket, ServerSocketIPv6_MultipleBind)
{
    dmSocket::Socket socket1, socket2;
    dmSocket::Result r;
    r = dmSocket::New(dmSocket::DOMAIN_IPV6, dmSocket::TYPE_STREAM, dmSocket::PROTOCOL_TCP, &socket1);
    ASSERT_EQ(dmSocket::RESULT_OK, r)
        << "  Expected(" << dmSocket::ResultToString(dmSocket::RESULT_OK) << "), Actual(" << dmSocket::ResultToString(r) << ")" << std::endl;
    r = dmSocket::New(dmSocket::DOMAIN_IPV6, dmSocket::TYPE_STREAM, dmSocket::PROTOCOL_TCP, &socket2);
    ASSERT_EQ(dmSocket::RESULT_OK, r)
        << "  Expected(" << dmSocket::ResultToString(dmSocket::RESULT_OK) << "), Actual(" << dmSocket::ResultToString(r) << ")" << std::endl;

    const int port = 9000;

    r = dmSocket::Bind(socket1, dmSocket::AddressFromIPString("::"), port);
    ASSERT_EQ(dmSocket::RESULT_OK, r)
        << "  Expected(" << dmSocket::ResultToString(dmSocket::RESULT_OK) << "), Actual(" << dmSocket::ResultToString(r) << ")" << std::endl;

    r = dmSocket::Bind(socket2, dmSocket::AddressFromIPString("::"), port);
    ASSERT_EQ(dmSocket::RESULT_ADDRINUSE, r)
        << "  Expected(" << dmSocket::ResultToString(dmSocket::RESULT_ADDRINUSE) << "), Actual(" << dmSocket::ResultToString(r) << ")" << std::endl;

    r = dmSocket::Delete(socket1);
    ASSERT_EQ(dmSocket::RESULT_OK, r)
        << "  Expected(" << dmSocket::ResultToString(dmSocket::RESULT_OK) << "), Actual(" << dmSocket::ResultToString(r) << ")" << std::endl;

    r = dmSocket::Delete(socket2);
    ASSERT_EQ(dmSocket::RESULT_OK, r)
        << "  Expected(" << dmSocket::ResultToString(dmSocket::RESULT_OK) << "), Actual(" << dmSocket::ResultToString(r) << ")" << std::endl;
}

TEST(Socket, ServerSocketIPv4_Accept)
{
    dmSocket::Socket socket;
    dmSocket::Result r = dmSocket::New(dmSocket::DOMAIN_IPV4, dmSocket::TYPE_STREAM, dmSocket::PROTOCOL_TCP, &socket);
    ASSERT_EQ(dmSocket::RESULT_OK, r)
        << "  Expected(" << dmSocket::ResultToString(dmSocket::RESULT_OK) << "), Actual(" << dmSocket::ResultToString(r) << ")" << std::endl;

    r = dmSocket::SetBlocking(socket, false);
    ASSERT_EQ(dmSocket::RESULT_OK, r)
        << "  Expected(" << dmSocket::ResultToString(dmSocket::RESULT_OK) << "), Actual(" << dmSocket::ResultToString(r) << ")" << std::endl;

    const int port = 9000;

    r = dmSocket::Bind(socket, dmSocket::AddressFromIPString("0.0.0.0"), port);
    ASSERT_EQ(dmSocket::RESULT_OK, r)
        << "  Expected(" << dmSocket::ResultToString(dmSocket::RESULT_OK) << "), Actual(" << dmSocket::ResultToString(r) << ")" << std::endl;

    r = dmSocket::Listen(socket, 1000);
    ASSERT_EQ(dmSocket::RESULT_OK, r)
        << "  Expected(" << dmSocket::ResultToString(dmSocket::RESULT_OK) << "), Actual(" << dmSocket::ResultToString(r) << ")" << std::endl;

    dmSocket::Address address;
    dmSocket::Socket client_socket;
    r = dmSocket::Accept(socket, &address, &client_socket);
    ASSERT_EQ(dmSocket::RESULT_WOULDBLOCK, r)
        << "  Expected(" << dmSocket::ResultToString(dmSocket::RESULT_WOULDBLOCK) << "), Actual(" << dmSocket::ResultToString(r) << ")" << std::endl;

    r = dmSocket::Delete(socket);
    ASSERT_EQ(dmSocket::RESULT_OK, r)
        << "  Expected(" << dmSocket::ResultToString(dmSocket::RESULT_OK) << "), Actual(" << dmSocket::ResultToString(r) << ")" << std::endl;
}

TEST(Socket, ServerSocketIPv6_Accept)
{
    dmSocket::Socket socket;
    dmSocket::Result r = dmSocket::New(dmSocket::DOMAIN_IPV6, dmSocket::TYPE_STREAM, dmSocket::PROTOCOL_TCP, &socket);
    ASSERT_EQ(dmSocket::RESULT_OK, r)
        << "  Expected(" << dmSocket::ResultToString(dmSocket::RESULT_OK) << "), Actual(" << dmSocket::ResultToString(r) << ")" << std::endl;

    r = dmSocket::SetBlocking(socket, false);
    ASSERT_EQ(dmSocket::RESULT_OK, r)
        << "  Expected(" << dmSocket::ResultToString(dmSocket::RESULT_OK) << "), Actual(" << dmSocket::ResultToString(r) << ")" << std::endl;

    const int port = 9000;

    r = dmSocket::Bind(socket, dmSocket::AddressFromIPString("::"), port);
    ASSERT_EQ(dmSocket::RESULT_OK, r)
        << "  Expected(" << dmSocket::ResultToString(dmSocket::RESULT_OK) << "), Actual(" << dmSocket::ResultToString(r) << ")" << std::endl;

    r = dmSocket::Listen(socket, 1000);
    ASSERT_EQ(dmSocket::RESULT_OK, r)
        << "  Expected(" << dmSocket::ResultToString(dmSocket::RESULT_OK) << "), Actual(" << dmSocket::ResultToString(r) << ")" << std::endl;

    dmSocket::Address address;
    dmSocket::Socket client_socket;
    r = dmSocket::Accept(socket, &address, &client_socket);
    ASSERT_EQ(dmSocket::RESULT_WOULDBLOCK, r)
        << "  Expected(" << dmSocket::ResultToString(dmSocket::RESULT_WOULDBLOCK) << "), Actual(" << dmSocket::ResultToString(r) << ")" << std::endl;

    r = dmSocket::Delete(socket);
    ASSERT_EQ(dmSocket::RESULT_OK, r)
        << "  Expected(" << dmSocket::ResultToString(dmSocket::RESULT_OK) << "), Actual(" << dmSocket::ResultToString(r) << ")" << std::endl;
}

bool g_ServerThread1Running = false;
#ifdef _WIN32
int  g_ServerThread1Port = 9003;
#else
int  g_ServerThread1Port = 9002;
#endif

static void ServerThread1(void* arg)
{
    dmSocket::Socket socket;
    dmSocket::Result r = dmSocket::New(dmSocket::DOMAIN_IPV6, dmSocket::TYPE_STREAM, dmSocket::PROTOCOL_TCP, &socket); // This has to be rewritten
    ASSERT_EQ(dmSocket::RESULT_OK, r)
        << "  Expected(" << dmSocket::ResultToString(dmSocket::RESULT_OK) << "), Actual(" << dmSocket::ResultToString(r) << ")" << std::endl;

    r = dmSocket::SetReuseAddress(socket, true);
    ASSERT_EQ(dmSocket::RESULT_OK, r)
        << "  Expected(" << dmSocket::ResultToString(dmSocket::RESULT_OK) << "), Actual(" << dmSocket::ResultToString(r) << ")" << std::endl;

    const int port = g_ServerThread1Port;

    r = dmSocket::Bind(socket, dmSocket::AddressFromIPString("::"), port);
    ASSERT_EQ(dmSocket::RESULT_OK, r)
        << "  Expected(" << dmSocket::ResultToString(dmSocket::RESULT_OK) << "), Actual(" << dmSocket::ResultToString(r) << ")" << std::endl;

    r = dmSocket::Listen(socket, 1000);
    ASSERT_EQ(dmSocket::RESULT_OK, r)
        << "  Expected(" << dmSocket::ResultToString(dmSocket::RESULT_OK) << "), Actual(" << dmSocket::ResultToString(r) << ")" << std::endl;

    dmSocket::Address address;
    dmSocket::Socket client_socket;
    g_ServerThread1Running = true;
    r = dmSocket::Accept(socket, &address, &client_socket);
    ASSERT_EQ(dmSocket::RESULT_OK, r)
        << "  Expected(" << dmSocket::ResultToString(dmSocket::RESULT_OK) << "), Actual(" << dmSocket::ResultToString(r) << ")" << std::endl;
    ASSERT_EQ(address, dmSocket::AddressFromIPString("localhost"));

    int value = 1234;
    int sent_bytes;
    r = dmSocket::Send(client_socket, &value, sizeof(value), &sent_bytes);
    ASSERT_EQ(dmSocket::RESULT_OK, r)
        << "  Expected(" << dmSocket::ResultToString(dmSocket::RESULT_OK) << "), Actual(" << dmSocket::ResultToString(r) << ")" << std::endl;
    ASSERT_EQ((int) sizeof(value), sent_bytes);

    r = dmSocket::Delete(client_socket);
    ASSERT_EQ(dmSocket::RESULT_OK, r)
        << "  Expected(" << dmSocket::ResultToString(dmSocket::RESULT_OK) << "), Actual(" << dmSocket::ResultToString(r) << ")" << std::endl;

    r = dmSocket::Delete(socket);
    ASSERT_EQ(dmSocket::RESULT_OK, r)
        << "  Expected(" << dmSocket::ResultToString(dmSocket::RESULT_OK) << "), Actual(" << dmSocket::ResultToString(r) << ")" << std::endl;
}

TEST(Socket, ClientServer1)
{
    dmThread::Thread thread = dmThread::New(&ServerThread1, 0x80000, 0, "server");

    dmSocket::Socket socket;
    dmSocket::Result r = dmSocket::New(dmSocket::DOMAIN_IPV6, dmSocket::TYPE_STREAM, dmSocket::PROTOCOL_TCP, &socket); // This has to be rewritten
    ASSERT_EQ(r, dmSocket::RESULT_OK);

    dmSocket::Address local_address = dmSocket::AddressFromIPString("localhost");

    for (int i = 0; i < 100; ++i)
    {
        if (g_ServerThread1Running)
            break;

        dmTime::Sleep(10000);
    }
    dmTime::Sleep(500); // Make sure that we are in "accept"

    ASSERT_EQ(true, g_ServerThread1Running);

    r = dmSocket::Connect(socket, local_address, g_ServerThread1Port);
    ASSERT_EQ(dmSocket::RESULT_OK, r)
        << "  Expected(" << dmSocket::ResultToString(dmSocket::RESULT_OK) << "), Actual(" << dmSocket::ResultToString(r) << ")" << std::endl;

    int value;
    int received_bytes;
    r = dmSocket::Receive(socket, &value, sizeof(value), &received_bytes);
    ASSERT_EQ(dmSocket::RESULT_OK, r)
        << "  Expected(" << dmSocket::ResultToString(dmSocket::RESULT_OK) << "), Actual(" << dmSocket::ResultToString(r) << ")" << std::endl;
    ASSERT_EQ((int) sizeof(received_bytes), received_bytes);
    ASSERT_EQ(1234, value);

    r = dmSocket::Delete(socket);
    ASSERT_EQ(dmSocket::RESULT_OK, r)
        << "  Expected(" << dmSocket::ResultToString(dmSocket::RESULT_OK) << "), Actual(" << dmSocket::ResultToString(r) << ")" << std::endl;

    dmThread::Join(thread);
}

static void PrintFlags(uint32_t f) {
    if (f & dmSocket::FLAGS_UP) {
        printf("UP ");
    }
    if (f & dmSocket::FLAGS_RUNNING) {
        printf("RUNNING ");
    }
}

TEST(Socket, Convert)
{
    dmSocket::Address a = dmSocket::AddressFromIPString("localhost");
    char* ip = dmSocket::AddressToIPString(a);
    ASSERT_STREQ("::1.0.0.0", ip);
    free(ip);
}

TEST(Socket, GetIfAddrs)
{
    uint32_t count;
    dmSocket::GetIfAddresses(0, 0, &count);
    ASSERT_EQ(0U, count);

    dmSocket::IfAddr as[16];
    dmSocket::GetIfAddresses(as, 16, &count);

    for (uint32_t i = 0; i < count; ++i) {
        const dmSocket::IfAddr& a = as[i];
        printf("%s ", a.m_Name);

        if (a.m_Flags & dmSocket::FLAGS_LINK) {
            printf("LINK %02x:%02x:%02x:%02x:%02x:%02x ",
                    a.m_MacAddress[0],a.m_MacAddress[1],a.m_MacAddress[2],a.m_MacAddress[3],a.m_MacAddress[4],a.m_MacAddress[5]);
        }

        if (a.m_Flags & dmSocket::FLAGS_INET) {
            printf("INET %s ", dmSocket::AddressToIPString(a.m_Address));
        }

        PrintFlags(a.m_Flags);
        printf("\n");
    }
}

TEST(Socket, Timeout)
{
    const uint64_t timeout = 50 * 1000;
    dmSocket::Socket server_socket;
    dmSocket::Result r = dmSocket::New(dmSocket::DOMAIN_IPV6, dmSocket::TYPE_STREAM, dmSocket::PROTOCOL_TCP, &server_socket); // This has to be rewritten
    ASSERT_EQ(dmSocket::RESULT_OK, r)
        << "  Expected(" << dmSocket::ResultToString(dmSocket::RESULT_OK) << "), Actual(" << dmSocket::ResultToString(r) << ")" << std::endl;

    r = dmSocket::SetReuseAddress(server_socket, true);
    ASSERT_EQ(dmSocket::RESULT_OK, r)
        << "  Expected(" << dmSocket::ResultToString(dmSocket::RESULT_OK) << "), Actual(" << dmSocket::ResultToString(r) << ")" << std::endl;

	r = dmSocket::Bind(server_socket, dmSocket::AddressFromIPString("localhost"), 0);
    ASSERT_EQ(dmSocket::RESULT_OK, r)
        << "  Expected(" << dmSocket::ResultToString(dmSocket::RESULT_OK) << "), Actual(" << dmSocket::ResultToString(r) << ")" << std::endl;

    r = dmSocket::Listen(server_socket, 1000);
    ASSERT_EQ(dmSocket::RESULT_OK, r)
        << "  Expected(" << dmSocket::ResultToString(dmSocket::RESULT_OK) << "), Actual(" << dmSocket::ResultToString(r) << ")" << std::endl;

    uint16_t port;
    dmSocket::Address address;
    r = dmSocket::GetName(server_socket, &address, &port);
    ASSERT_EQ(dmSocket::RESULT_OK, r)
        << "  Expected(" << dmSocket::ResultToString(dmSocket::RESULT_OK) << "), Actual(" << dmSocket::ResultToString(r) << ")" << std::endl;

    dmSocket::Socket client_socket;
    r = dmSocket::New(dmSocket::DOMAIN_IPV6, dmSocket::TYPE_STREAM, dmSocket::PROTOCOL_TCP, &client_socket); // This has to be rewritten
    ASSERT_EQ(dmSocket::RESULT_OK, r)
        << "  Expected(" << dmSocket::ResultToString(dmSocket::RESULT_OK) << "), Actual(" << dmSocket::ResultToString(r) << ")" << std::endl;

    r = dmSocket::SetReceiveTimeout(client_socket, timeout);
    ASSERT_EQ(dmSocket::RESULT_OK, r)
        << "  Expected(" << dmSocket::ResultToString(dmSocket::RESULT_OK) << "), Actual(" << dmSocket::ResultToString(r) << ")" << std::endl;

    r = dmSocket::SetSendTimeout(client_socket, timeout);
    ASSERT_EQ(dmSocket::RESULT_OK, r)
        << "  Expected(" << dmSocket::ResultToString(dmSocket::RESULT_OK) << "), Actual(" << dmSocket::ResultToString(r) << ")" << std::endl;

    r = dmSocket::Connect(client_socket, address, port);
    ASSERT_EQ(dmSocket::RESULT_OK, r)
        << "  Expected(" << dmSocket::ResultToString(dmSocket::RESULT_OK) << "), Actual(" << dmSocket::ResultToString(r) << ")" << std::endl;

    int received;
    char buf[4096];
    memset(buf, 0, sizeof(buf));

    for (int i = 0; i < 10; ++i) {
        uint64_t start = dmTime::GetTime();
        r = dmSocket::Receive(client_socket, buf, sizeof(buf), &received);
        uint64_t end = dmTime::GetTime();
        ASSERT_EQ(dmSocket::RESULT_WOULDBLOCK, r)
            << "  Expected(" << dmSocket::ResultToString(dmSocket::RESULT_WOULDBLOCK) << "), Actual(" << dmSocket::ResultToString(r) << ")" << std::endl;
        ASSERT_GE(end - start, timeout - 2500); // NOTE: Margin of 2500. Required on Linux
    }

    for (int i = 0; i < 10; ++i) {
        uint64_t start = dmTime::GetTime();
        for (int j = 0; j < 10000; ++j) {
            // Loop to ensure that we fill send buffers
            r = dmSocket::Send(client_socket, buf, sizeof(buf), &received);
            if (r != dmSocket::RESULT_OK) {
                break;
            }
        }
        uint64_t end = dmTime::GetTime();
        ASSERT_EQ(dmSocket::RESULT_WOULDBLOCK, r)
            << "  Expected(" << dmSocket::ResultToString(dmSocket::RESULT_WOULDBLOCK) << "), Actual(" << dmSocket::ResultToString(r) << ")" << std::endl;
        ASSERT_GE(end - start, timeout - 2500); // NOTE: Margin of 2500. Required on Linux
    }

    dmSocket::Delete(server_socket);
    dmSocket::Delete(client_socket);
}

int main(int argc, char **argv)
{
    dmSocket::Initialize();
    testing::InitGoogleTest(&argc, argv);
    int ret = RUN_ALL_TESTS();
    dmSocket::Finalize();
    return ret;
}

