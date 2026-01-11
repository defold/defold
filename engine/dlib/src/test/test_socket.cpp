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
#include <stdlib.h> // free
#include <string.h>
#include <dlib/atomic.h>
#include <dlib/dstrings.h>
#include <dlib/socket.h>
#include <dlib/thread.h>
#include <dlib/time.h>
#if defined(__linux__) || defined(__MACH__) || defined(ANDROID) || defined(__EMSCRIPTEN__)
#include <arpa/inet.h> // inet_addr
#endif

#include <dlib/network_constants.h>

#define JC_TEST_IMPLEMENTATION
#include <jc_test/jc_test.h>

template <> char* jc_test_print_value(char* buffer, size_t buffer_len, dmSocket::Result r) {
    return buffer + dmSnPrintf(buffer, buffer_len, "%s", dmSocket::ResultToString(r));
}

const uint16_t CONST_TEST_PORT = 8008;

struct ServerThreadInfo
{

    ServerThreadInfo()
    {
        port = 0;
        domain = dmSocket::DOMAIN_MISSING;
        dmAtomicStore32(&listening, 0);
        dmAtomicStore32(&sent, 0);
    }

    uint16_t port;
    dmSocket::Domain domain;
    int32_atomic_t listening;
    int32_atomic_t sent;
};

void WaitForBool(int32_atomic_t* lock, uint32_t maximum_wait_ms)
{
    const uint32_t wait_timeout = 100;  // milliseconds
    uint32_t wait_count = 0;
    for (uint32_t i = 0; i < (maximum_wait_ms / wait_timeout); ++i)
    {
        if (dmAtomicGet32(lock)==0)
        {
            wait_count += 1;
            dmTime::Sleep(wait_timeout * 1000); // nanoseconds
        }
    }

    printf("Waited for %d/%d counts (%d ms each)\n", wait_count, (maximum_wait_ms / wait_timeout), wait_timeout);
    ASSERT_TRUE(dmAtomicGet32(lock) == 1);
    dmTime::Sleep(5 * wait_timeout * 1000); // nanoseconds
}

static void ServerThread(void* arg)
{
    struct ServerThreadInfo* info = (struct ServerThreadInfo *) arg;
    dmSocket::Result result = dmSocket::RESULT_OK;
    dmSocket::Socket server_sock = -1;
    dmSocket::Address server_addr;
    dmSocket::Socket client_sock = -1;
    dmSocket::Address client_addr;

#define T_ASSERT_EQ(_A, _B) \
    if ( (_A) != (_B) ) { \
        printf("%s:%d: ASSERT: %s != %s: %d != %d", __FILE__, __LINE__, #_A, #_B, (_A), (_B)); \
    } \
    assert( (_A) == (_B) );

    // Setup server socket and listen for a client to connect
    result = dmSocket::New(info->domain, dmSocket::TYPE_STREAM, dmSocket::PROTOCOL_TCP, &server_sock);
    T_ASSERT_EQ(dmSocket::RESULT_OK, result);

    result = dmSocket::SetReuseAddress(server_sock, true);
    T_ASSERT_EQ(dmSocket::RESULT_OK, result);

    const char* hostname = dmSocket::IsSocketIPv4(server_sock) ? DM_LOOPBACK_ADDRESS_IPV4 : DM_LOOPBACK_ADDRESS_IPV6;
    result = dmSocket::GetHostByName(hostname, &server_addr, dmSocket::IsSocketIPv4(server_sock), dmSocket::IsSocketIPv6(server_sock));
    T_ASSERT_EQ(dmSocket::RESULT_OK, result);

    result = dmSocket::Bind(server_sock, server_addr, info->port);
    T_ASSERT_EQ(dmSocket::RESULT_OK, result);

    result = dmSocket::Listen(server_sock, 1000); // Backlog = 1000
    T_ASSERT_EQ(dmSocket::RESULT_OK, result);

    // Wait for a client to connect
    dmAtomicStore32(&info->listening, 1);
    result = dmSocket::Accept(server_sock, &client_addr, &client_sock);
    T_ASSERT_EQ(dmSocket::RESULT_OK, result);

    // Send data to the client for verification
    int value = 0x00def01d;
    int written = 0;

    result = dmSocket::Send(client_sock, &value, sizeof(value), &written);
    T_ASSERT_EQ(dmSocket::RESULT_OK, result);
    T_ASSERT_EQ((int) sizeof(value), written);

    dmAtomicStore32(&info->sent, 1);

    // Teardown
    result = dmSocket::Delete(client_sock);
    T_ASSERT_EQ(dmSocket::RESULT_OK, result);

    result = dmSocket::Delete(server_sock);
    T_ASSERT_EQ(dmSocket::RESULT_OK, result);
}

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

    result = dmSocket::SetQuickAck(instance, true);
    if (result != dmSocket::RESULT_OK) return -1;

    return instance;
}

TEST(Socket, BitDifference_Difference)
{
    dmSocket::Address instance1;
    dmSocket::Address instance2;

    instance1.m_address[3] = 0x4e;
    instance2.m_address[3] = 0xe6;

    ASSERT_EQ(3u, dmSocket::BitDifference(instance1, instance2));
}

TEST(Socket, BitDifference_Equal)
{
    dmSocket::Address instance1;
    dmSocket::Address instance2;

    instance1.m_address[3] = 0xe6;
    instance2.m_address[3] = 0xe6;

    ASSERT_EQ(0U, dmSocket::BitDifference(instance1, instance2));
}

#if !defined(DM_TEST_NO_INET_ADDR)  // until we have a helper interface wrapper for inet_addr
TEST(Socket, NetworkOrder)
{
    dmSocket::Address address;
    const char* hostname = DM_LOOPBACK_ADDRESS_IPV4;
    dmSocket::Result result = dmSocket::RESULT_OK;

    result = dmSocket::GetHostByName(hostname, &address, true, false);
    ASSERT_EQ(dmSocket::RESULT_OK, result);

    // This checks so our format is in network order.
    ASSERT_EQ(inet_addr(DM_LOOPBACK_ADDRESS_IPV4), address.m_address[3]);
}
#endif

TEST(Socket, IPv4)
{
    dmSocket::Address instance;
    instance.m_family = dmSocket::DOMAIN_IPV4;
    ASSERT_EQ(&instance.m_address[3], dmSocket::IPv4(&instance));
}

#if !defined(DM_IPV6_UNSUPPORTED)
TEST(Socket, IPv6)
{
    dmSocket::Address instance;
    instance.m_family = dmSocket::DOMAIN_IPV6;
    ASSERT_EQ(&instance.m_address[0], dmSocket::IPv6(&instance));
}
#endif

TEST(Socket, New_IPv4)
{
    dmSocket::Socket instance = 0;
    dmSocket::Result result = dmSocket::RESULT_OK;

    result = dmSocket::New(dmSocket::DOMAIN_IPV4, dmSocket::TYPE_STREAM, dmSocket::PROTOCOL_TCP, &instance);
    ASSERT_EQ(dmSocket::RESULT_OK, result);
    ASSERT_NE(dmSocket::INVALID_SOCKET_HANDLE, instance);
    ASSERT_TRUE(dmSocket::IsSocketIPv4(instance));
    ASSERT_FALSE(dmSocket::IsSocketIPv6(instance));

    // Teardown
    result = dmSocket::Delete(instance);
    ASSERT_EQ(dmSocket::RESULT_OK, result);
}

#if !defined(DM_IPV6_UNSUPPORTED)
TEST(Socket, New_IPv6)
{
    dmSocket::Socket instance = 0;
    dmSocket::Result result = dmSocket::RESULT_OK;

    result = dmSocket::New(dmSocket::DOMAIN_IPV6, dmSocket::TYPE_STREAM, dmSocket::PROTOCOL_TCP, &instance);
    ASSERT_EQ(dmSocket::RESULT_OK, result);
    ASSERT_NE(dmSocket::INVALID_SOCKET_HANDLE, instance);
    ASSERT_TRUE(dmSocket::IsSocketIPv6(instance));
    ASSERT_FALSE(dmSocket::IsSocketIPv4(instance));

    // Teardown
    result = dmSocket::Delete(instance);
    ASSERT_EQ(dmSocket::RESULT_OK, result);
}
#endif

TEST(Socket, New_InvalidDomain)
{
    dmSocket::Socket instance = 0;
    dmSocket::Result result = dmSocket::RESULT_OK;

    result = dmSocket::New(dmSocket::DOMAIN_UNKNOWN, dmSocket::TYPE_STREAM, dmSocket::PROTOCOL_TCP, &instance);
    ASSERT_EQ(dmSocket::RESULT_AFNOSUPPORT, result);
    ASSERT_EQ(dmSocket::INVALID_SOCKET_HANDLE, instance);
    ASSERT_FALSE(dmSocket::IsSocketIPv6(instance));
    ASSERT_FALSE(dmSocket::IsSocketIPv4(instance));

    // Teardown
    result = dmSocket::Delete(instance);
    ASSERT_EQ(dmSocket::RESULT_BADF, result);
}

struct TestIPv4
{
    uint32_t* NetworkAddress(dmSocket::Address* address)
    {
        return dmSocket::IPv4(address);
    }

    const char* loopback_address = DM_LOOPBACK_ADDRESS_IPV4;
    const char* universal_bind_address = DM_UNIVERSAL_BIND_ADDRESS_IPV4;
    const dmSocket::Domain domain_type = dmSocket::DOMAIN_IPV4;
};

struct TestIPv6
{
    uint32_t* NetworkAddress(dmSocket::Address* address)
    {
        return dmSocket::IPv6(address);
    }

    const char* loopback_address = DM_LOOPBACK_ADDRESS_IPV6;
    const char* universal_bind_address = DM_UNIVERSAL_BIND_ADDRESS_IPV6;
    const dmSocket::Domain domain_type = dmSocket::DOMAIN_IPV6;
};

template<typename T>
class SocketTyped : public jc_test_base_class
{
protected:
    T instance;
};

typedef 
#if !defined(DM_IPV6_UNSUPPORTED)
jc_test_type2<TestIPv4, TestIPv6>
#else
jc_test_type1<TestIPv4>
#endif
TestTypes;
TYPED_TEST_CASE(SocketTyped, TestTypes);

TYPED_TEST(SocketTyped, SetReuseAddress)
{
    dmSocket::Result result = dmSocket::RESULT_OK;
    dmSocket::Socket instance = GetSocket(TestFixture::instance.domain_type);
    ASSERT_NE(dmSocket::INVALID_SOCKET_HANDLE, instance);

    result = dmSocket::SetReuseAddress(instance, true);
    ASSERT_EQ(dmSocket::RESULT_OK, result);

    // Teardown
    result = dmSocket::Delete(instance);
    ASSERT_EQ(dmSocket::RESULT_OK, result);
}


TYPED_TEST(SocketTyped, AddMembership)
{
    dmSocket::Result result = dmSocket::RESULT_OK;
    dmSocket::Socket instance = GetSocket(TestFixture::instance.domain_type);
    ASSERT_NE(dmSocket::INVALID_SOCKET_HANDLE, instance);

    // Teardown
    result = dmSocket::Delete(instance);
    ASSERT_EQ(dmSocket::RESULT_OK, result);
}

TYPED_TEST(SocketTyped, SetMulticastIf)
{
    dmSocket::Result result = dmSocket::RESULT_OK;
    dmSocket::Socket instance = GetSocket(TestFixture::instance.domain_type);
    ASSERT_NE(dmSocket::INVALID_SOCKET_HANDLE, instance);

    uint32_t count = 0;
    const uint32_t max_count = 16;
    dmSocket::IfAddr addresses[max_count];
    dmSocket::GetIfAddresses(addresses, max_count, &count);

    // Test has been disabled since we can't test multicast interfaces without
    // multiple interfaces. Neither the build infrastructure nor the
    // development environment at King currently supports this, thus this
    // functionality has to be tested manually.
    printf("[   INFO   ] Test for SetMulticastIf is disabled.\n");

    for (uint32_t i = 0; i < count; ++i)
    {
        dmSocket::Address address = addresses[i].m_Address;
        if (address.m_family == TestFixture::instance.domain_type)
        {
            result = dmSocket::SetMulticastIf(instance, address);
        }
    }

    // Teardown
    result = dmSocket::Delete(instance);
    ASSERT_EQ(dmSocket::RESULT_OK, result);
}

TYPED_TEST(SocketTyped, Delete)
{
    dmSocket::Result result = dmSocket::RESULT_OK;
    dmSocket::Socket instance = GetSocket(TestFixture::instance.domain_type);
    ASSERT_NE(dmSocket::INVALID_SOCKET_HANDLE, instance);

    result = dmSocket::Delete(instance);
    ASSERT_EQ(dmSocket::RESULT_OK, result);
}

TEST(Socket, Delete_InvalidSocket)
{
    dmSocket::Socket instance = dmSocket::INVALID_SOCKET_HANDLE;
    dmSocket::Result result = dmSocket::RESULT_OK;

    result = dmSocket::Delete(instance);
    ASSERT_EQ(dmSocket::RESULT_BADF, result);
}

// Accept

// Bind

TYPED_TEST(SocketTyped, Connect_ThreadServer)
{
    // Setup server thread
    struct ServerThreadInfo info;
    info.port = CONST_TEST_PORT;
    info.domain = TestFixture::instance.domain_type;
    const char* hostname = TestFixture::instance.loopback_address;
    dmThread::Thread thread = dmThread::New(&ServerThread, 0x80000, (void *) &info, "server");

    // Setup client
    dmSocket::Socket socket = GetSocket(TestFixture::instance.domain_type);
    ASSERT_NE(dmSocket::INVALID_SOCKET_HANDLE, socket);
    dmSocket::Address address;
    dmSocket::Result result = dmSocket::RESULT_OK;

    result = dmSocket::GetHostByName(hostname, &address, dmSocket::IsSocketIPv4(socket), dmSocket::IsSocketIPv6(socket));
    ASSERT_EQ(dmSocket::RESULT_OK, result);

    WaitForBool(&info.listening, 5000);

    result = dmSocket::Connect(socket, address, CONST_TEST_PORT);
    ASSERT_EQ(dmSocket::RESULT_OK, result);

    WaitForBool(&info.sent, 5000);

    // Receive data from the server
    int value = 0;
    int read = 0;

    result = dmSocket::Receive(socket, &value, sizeof(value), &read);
    ASSERT_EQ(dmSocket::RESULT_OK, result);
    ASSERT_EQ(0x00def01d, value);

    result = dmSocket::Delete(socket);
    ASSERT_EQ(dmSocket::RESULT_OK, result);

    dmThread::Join(thread);
}

static void RefusingServerThread(void* arg)
{
    dmSocket::Socket* server = (dmSocket::Socket*)arg;
    dmTime::Sleep(100 * 1000);
    dmSocket::Delete(*server);
}

TYPED_TEST(SocketTyped, Connect_ConnectionRefused)
{
    dmSocket::Socket server = GetSocket(TestFixture::instance.domain_type);
    ASSERT_NE(dmSocket::INVALID_SOCKET_HANDLE, server);
    dmSocket::Socket client = GetSocket(TestFixture::instance.domain_type);
    ASSERT_NE(dmSocket::INVALID_SOCKET_HANDLE, client);
    dmSocket::Result result = dmSocket::RESULT_OK;
    result = dmSocket::SetBlocking(client, true);
    ASSERT_EQ(dmSocket::RESULT_OK, result);
    const char* hostname = TestFixture::instance.loopback_address;
    dmSocket::Address address;

    result = dmSocket::GetHostByName(hostname, &address, dmSocket::IsSocketIPv4(server), dmSocket::IsSocketIPv6(server));
    ASSERT_EQ(dmSocket::RESULT_OK, result);

    result = dmSocket::Bind(server, address, 0);
    ASSERT_EQ(dmSocket::RESULT_OK, result);
    uint16_t port;
    result = dmSocket::GetName(server, &address, &port);
    ASSERT_EQ(dmSocket::RESULT_OK, result);
    dmThread::Thread thread = dmThread::New(&RefusingServerThread, 0x80000, (void *) &server, "server");

    result = dmSocket::Connect(client, address, port);
    ASSERT_EQ(dmSocket::RESULT_CONNREFUSED, result);

    // Teardown
    result = dmSocket::Delete(client);
    ASSERT_EQ(dmSocket::RESULT_OK, result);

    dmThread::Join(thread);
}

// Listen

// Shutdown

TYPED_TEST(SocketTyped, GetName_Connected)
{
    // Setup server thread
    struct ServerThreadInfo info;
    info.port = CONST_TEST_PORT;
    info.domain = TestFixture::instance.domain_type;
    dmThread::Thread thread = dmThread::New(&ServerThread, 0x80000, (void *) &info, "server");

    dmSocket::Socket instance = GetSocket(TestFixture::instance.domain_type);

    ASSERT_NE(dmSocket::INVALID_SOCKET_HANDLE, instance);
    dmSocket::Result result = dmSocket::RESULT_OK;
    const char* hostname = TestFixture::instance.loopback_address;
    dmSocket::Address address;
    uint16_t port = CONST_TEST_PORT;

    result = dmSocket::GetHostByName(hostname, &address, dmSocket::IsSocketIPv4(instance), dmSocket::IsSocketIPv6(instance));
    ASSERT_EQ(dmSocket::RESULT_OK, result);

    WaitForBool(&info.listening, 20000);

    dmTime::Sleep(500000);
    ASSERT_TRUE(dmAtomicGet32(&info.listening)==1);

    result = dmSocket::Connect(instance, address, port);
    ASSERT_EQ(dmSocket::RESULT_OK, result);

    uint16_t actual_port = 0;
    dmSocket::Address actual_address;
    result = dmSocket::GetName(instance, &actual_address, &actual_port);
    ASSERT_EQ(dmSocket::RESULT_OK, result);
    ASSERT_EQ(address.m_family, actual_address.m_family);
    ASSERT_EQ(*TestFixture::instance.NetworkAddress(&address), *TestFixture::instance.NetworkAddress(&actual_address));

    // Teardown
    result = dmSocket::Delete(instance);
    ASSERT_EQ(dmSocket::RESULT_OK, result);

    dmThread::Join(thread);
}

TYPED_TEST(SocketTyped, SetBlocking)
{
    dmSocket::Socket instance = GetSocket(TestFixture::instance.domain_type);
    ASSERT_NE(dmSocket::INVALID_SOCKET_HANDLE, instance);
    dmSocket::Result result = dmSocket::RESULT_OK;

    result = dmSocket::SetBlocking(instance, true);
    ASSERT_EQ(dmSocket::RESULT_OK, result);

    result = dmSocket::SetBlocking(instance, false);
    ASSERT_EQ(dmSocket::RESULT_OK, result);

    // Teardown
    result = dmSocket::Delete(instance);
    ASSERT_EQ(dmSocket::RESULT_OK, result);
}

TYPED_TEST(SocketTyped, SetNoDelay)
{
    dmSocket::Socket instance = GetSocket(TestFixture::instance.domain_type);
    ASSERT_NE(dmSocket::INVALID_SOCKET_HANDLE, instance);
    dmSocket::Result result = dmSocket::RESULT_OK;

    result = dmSocket::SetNoDelay(instance, true);
    ASSERT_EQ(dmSocket::RESULT_OK, result);

    result = dmSocket::SetNoDelay(instance, false);
    ASSERT_EQ(dmSocket::RESULT_OK, result);

    // Teardown
    result = dmSocket::Delete(instance);
    ASSERT_EQ(dmSocket::RESULT_OK, result);
}

TYPED_TEST(SocketTyped, SetTimeout)
{
    dmSocket::Socket instance = 0;
    dmSocket::Result result = dmSocket::RESULT_OK;
    uint64_t timeout = 2000;

    result = dmSocket::New(TestFixture::instance.domain_type, dmSocket::TYPE_STREAM, dmSocket::PROTOCOL_TCP, &instance);
    ASSERT_EQ(dmSocket::RESULT_OK, result);

    result = dmSocket::SetSendTimeout(instance, timeout);
    ASSERT_EQ(dmSocket::RESULT_OK, result);

    result = dmSocket::SetReceiveTimeout(instance, timeout);
    ASSERT_EQ(dmSocket::RESULT_OK, result);

    // Teardown
    result = dmSocket::Delete(instance);
    ASSERT_EQ(dmSocket::RESULT_OK, result);
}

TEST(Socket, AddressToIPString_IPv4)
{
    dmSocket::Address address;
    address.m_family = dmSocket::DOMAIN_IPV4;
    address.m_address[3] = 0x0100007f; // 127.0.0.1 in network order

    char* actual = dmSocket::AddressToIPString(address);
    ASSERT_EQ(9u, strlen(actual));
    ASSERT_EQ(0, memcmp(DM_LOOPBACK_ADDRESS_IPV4, actual, 9));

    // Teardown
    free(actual);
}

#if !defined(DM_IPV6_UNSUPPORTED)
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
    ASSERT_EQ(0, memcmp(DM_UNIVERSAL_BIND_ADDRESS_IPV6, actual, 2));

    // Teardown
    free(actual);
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
    ASSERT_EQ(0, memcmp(DM_LOOPBACK_ADDRESS_IPV6, actual, 3));

    // Teardown
    free(actual);
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

    // Teardown
    free(actual);
}
#endif

TEST(Socket, GetHostByName_IPv4_Localhost)
{
    dmSocket::Address address;
    dmSocket::Result result = dmSocket::RESULT_OK;
    const char* hostname = DM_LOOPBACK_ADDRESS_IPV4;

    result = dmSocket::GetHostByName(hostname, &address, true, false);
    ASSERT_EQ(dmSocket::RESULT_OK, result);
    ASSERT_EQ(dmSocket::DOMAIN_IPV4, address.m_family);
    ASSERT_EQ(0x0100007f, *dmSocket::IPv4(&address));
}

#if !defined(DM_IPV6_UNSUPPORTED)
TEST(Socket, GetHostByName_IPv6_Localhost)
{
    dmSocket::Address address;
    dmSocket::Result result = dmSocket::RESULT_OK;
    const char* hostname = DM_LOOPBACK_ADDRESS_IPV6;

    result = dmSocket::GetHostByName(hostname, &address, false, true);
    ASSERT_EQ(dmSocket::RESULT_OK, result);
    ASSERT_EQ(dmSocket::DOMAIN_IPV6, address.m_family);
    ASSERT_EQ(0x00000000, address.m_address[0]);
    ASSERT_EQ(0x00000000, address.m_address[1]);
    ASSERT_EQ(0x00000000, address.m_address[2]);
    ASSERT_EQ(0x01000000, address.m_address[3]);
}
#endif

TEST(Socket, GetHostByName_IPv4_External)
{
    dmSocket::Address address;
    dmSocket::Result result = dmSocket::RESULT_OK;
    const char* hostname = "build.defold.com";

    result = dmSocket::GetHostByName(hostname, &address, true, false);
    ASSERT_EQ(dmSocket::RESULT_OK, result);
    ASSERT_EQ(dmSocket::DOMAIN_IPV4, address.m_family);
}

#if !defined(DM_IPV6_UNSUPPORTED)
TEST(Socket, GetHostByName_IPv6_External)
{
#if !defined(_WIN32)
    dmSocket::Address address;
    dmSocket::Result result = dmSocket::RESULT_OK;
    const char* hostname = "ipv6-test.com";

    result = dmSocket::GetHostByName(hostname, &address, false, true);
    ASSERT_EQ(dmSocket::RESULT_OK, result);
    ASSERT_EQ(dmSocket::DOMAIN_IPV6, address.m_family);
#else
    printf("[   INFO   ] Test for GetHostByName/IPv6 is disabled on Windows.\n");
    ASSERT_TRUE(true);
#endif
}
#endif

TYPED_TEST(SocketTyped, GetHostByName_Unavailable)
{
    dmSocket::Address address;
    dmSocket::Result result = dmSocket::RESULT_OK;
    const char* hostname = "localhost.invalid";

    result = dmSocket::GetHostByName(hostname, &address, true, false);
    ASSERT_EQ(dmSocket::RESULT_HOST_NOT_FOUND, result);
}

TEST(Socket, GetHostByName_NoValidAddressFamily)
{
    dmSocket::Address address;
    dmSocket::Result result = dmSocket::RESULT_OK;
    const char* hostname = "localhost";

    result = dmSocket::GetHostByName(hostname, &address, false, false);
    ASSERT_EQ(dmSocket::RESULT_HOST_NOT_FOUND, result);
}

TYPED_TEST(SocketTyped, ServerSocket)
{
    dmSocket::Socket socket;
    dmSocket::Result r = dmSocket::New(TestFixture::instance.domain_type, dmSocket::TYPE_STREAM, dmSocket::PROTOCOL_TCP, &socket);
    ASSERT_EQ(dmSocket::RESULT_OK, r);

    const int port = 9003;

    dmSocket::Address bindaddress;
    r = dmSocket::GetHostByName(TestFixture::instance.universal_bind_address, &bindaddress, dmSocket::IsSocketIPv4(socket), dmSocket::IsSocketIPv6(socket));
    ASSERT_EQ(dmSocket::RESULT_OK, r);

    r = dmSocket::Bind(socket, bindaddress, port);
    ASSERT_EQ(dmSocket::RESULT_OK, r);

    r = dmSocket::Listen(socket, 1000);
    ASSERT_EQ(dmSocket::RESULT_OK, r);

    r = dmSocket::Delete(socket);
    ASSERT_EQ(dmSocket::RESULT_OK, r);
}

TYPED_TEST(SocketTyped, ServerSocket_MultipleBind)
{
    dmSocket::Socket socket1, socket2;
    dmSocket::Result r;
    r = dmSocket::New(TestFixture::instance.domain_type, dmSocket::TYPE_STREAM, dmSocket::PROTOCOL_TCP, &socket1);
    ASSERT_EQ(dmSocket::RESULT_OK, r);
    r = dmSocket::New(TestFixture::instance.domain_type, dmSocket::TYPE_STREAM, dmSocket::PROTOCOL_TCP, &socket2);
    ASSERT_EQ(dmSocket::RESULT_OK, r);

    const int port = 9003;

    dmSocket::Address bindaddress1;
    r = dmSocket::GetHostByName(TestFixture::instance.universal_bind_address, &bindaddress1, dmSocket::IsSocketIPv4(socket1), dmSocket::IsSocketIPv6(socket1));
    ASSERT_EQ(dmSocket::RESULT_OK, r);

    dmSocket::Address bindaddress2;
    r = dmSocket::GetHostByName(TestFixture::instance.universal_bind_address, &bindaddress2, dmSocket::IsSocketIPv4(socket2), dmSocket::IsSocketIPv6(socket2));
    ASSERT_EQ(dmSocket::RESULT_OK, r);

    r = dmSocket::Bind(socket1, bindaddress1, port);
    ASSERT_EQ(dmSocket::RESULT_OK, r);

    r = dmSocket::Bind(socket2, bindaddress2, port);
    ASSERT_EQ(dmSocket::RESULT_ADDRINUSE, r);

    r = dmSocket::Delete(socket1);
    ASSERT_EQ(dmSocket::RESULT_OK, r);

    r = dmSocket::Delete(socket2);
    ASSERT_EQ(dmSocket::RESULT_OK, r);
    ASSERT_EQ(dmSocket::RESULT_OK, r);
}

TYPED_TEST(SocketTyped, ServerSocket_Accept)
{
    dmSocket::Socket socket;
    dmSocket::Result r = dmSocket::New(TestFixture::instance.domain_type, dmSocket::TYPE_STREAM, dmSocket::PROTOCOL_TCP, &socket);
    ASSERT_EQ(dmSocket::RESULT_OK, r);

    r = dmSocket::SetBlocking(socket, false);
    ASSERT_EQ(dmSocket::RESULT_OK, r);

    const int port = 9003;

    dmSocket::Address bindaddress;
    r = dmSocket::GetHostByName(TestFixture::instance.universal_bind_address, &bindaddress, dmSocket::IsSocketIPv4(socket), dmSocket::IsSocketIPv6(socket));
    ASSERT_EQ(dmSocket::RESULT_OK, r);

    r = dmSocket::Bind(socket, bindaddress, port);
    ASSERT_EQ(dmSocket::RESULT_OK, r);

    r = dmSocket::Listen(socket, 1000);
    ASSERT_EQ(dmSocket::RESULT_OK, r);

    dmSocket::Address address;
    dmSocket::Socket client_socket;
    r = dmSocket::Accept(socket, &address, &client_socket);
    ASSERT_EQ(dmSocket::RESULT_WOULDBLOCK, r);

    r = dmSocket::Delete(socket);
    ASSERT_EQ(dmSocket::RESULT_OK, r);
}

static void PrintFlags(uint32_t f) {
    if (f & dmSocket::FLAGS_UP) {
        printf("UP ");
    }
    if (f & dmSocket::FLAGS_RUNNING) {
        printf("RUNNING ");
    }
}

TEST(Socket, GetIfAddrs)
{
    uint32_t count;
    dmSocket::GetIfAddresses(0, 0, &count);
    ASSERT_EQ(0U, count);

    dmSocket::IfAddr as[16];
    dmSocket::GetIfAddresses(as, 16, &count);
    ASSERT_NE(0U, count);

    for (uint32_t i = 0; i < count; ++i) {
        const dmSocket::IfAddr& a = as[i];
        printf("%s ", a.m_Name);

        if (a.m_Flags & dmSocket::FLAGS_LINK) {
            printf("LINK %02x:%02x:%02x:%02x:%02x:%02x ",
                    a.m_MacAddress[0],a.m_MacAddress[1],a.m_MacAddress[2],a.m_MacAddress[3],a.m_MacAddress[4],a.m_MacAddress[5]);
        }

        if (a.m_Flags & dmSocket::FLAGS_INET) {
            char* straddr = dmSocket::AddressToIPString(a.m_Address);
            printf("INET %s ", straddr);
            free((void*) straddr);
        }

        PrintFlags(a.m_Flags);
        printf("\n");
    }
}

#if !defined(DM_IPV6_UNSUPPORTED)
TEST(Socket, Timeout)
{
    const uint64_t timeout = 50 * 1000;
    dmSocket::Socket server_socket;
    dmSocket::Result r = dmSocket::New(dmSocket::DOMAIN_IPV6, dmSocket::TYPE_STREAM, dmSocket::PROTOCOL_TCP, &server_socket); // This has to be rewritten
    ASSERT_EQ(dmSocket::RESULT_OK, r);

    r = dmSocket::SetReuseAddress(server_socket, true);
    ASSERT_EQ(dmSocket::RESULT_OK, r);

    dmSocket::Address bindaddress;
    r = dmSocket::GetHostByName(DM_UNIVERSAL_BIND_ADDRESS_IPV6, &bindaddress, false, true);
    ASSERT_EQ(dmSocket::RESULT_OK, r);

	r = dmSocket::Bind(server_socket, bindaddress, 0);
    ASSERT_EQ(dmSocket::RESULT_OK, r);

    r = dmSocket::Listen(server_socket, 1000);
    ASSERT_EQ(dmSocket::RESULT_OK, r);

    uint16_t port;
    dmSocket::Address address;
    r = dmSocket::GetName(server_socket, &address, &port); // We do this to get the port
    ASSERT_EQ(dmSocket::RESULT_OK, r);

    r = dmSocket::GetHostByName(DM_LOOPBACK_ADDRESS_IPV6, &address, false, true); // We do this to get the address
    ASSERT_EQ(dmSocket::RESULT_OK, r);

    dmSocket::Socket client_socket;
    r = dmSocket::New(dmSocket::DOMAIN_IPV6, dmSocket::TYPE_STREAM, dmSocket::PROTOCOL_TCP, &client_socket); // This has to be rewritten
    ASSERT_EQ(dmSocket::RESULT_OK, r);

    r = dmSocket::SetReceiveTimeout(client_socket, timeout);
    ASSERT_EQ(dmSocket::RESULT_OK, r);

    r = dmSocket::SetSendTimeout(client_socket, timeout);
    ASSERT_EQ(dmSocket::RESULT_OK, r);

    r = dmSocket::Connect(client_socket, address, port);
    ASSERT_EQ(dmSocket::RESULT_OK, r);

    int received;
    char buf[4096];
    memset(buf, 0, sizeof(buf));

    for (int i = 0; i < 10; ++i) {
        uint64_t start = dmTime::GetMonotonicTime();
        r = dmSocket::Receive(client_socket, buf, sizeof(buf), &received);
        uint64_t end = dmTime::GetMonotonicTime();
        ASSERT_EQ(dmSocket::RESULT_WOULDBLOCK, r);
        ASSERT_GE(end - start, timeout - 2500); // NOTE: Margin of 2500. Required on Linux
    }

    for (int i = 0; i < 10; ++i) {
        uint64_t start = dmTime::GetMonotonicTime();
        for (int j = 0; j < 10000; ++j) {
            // Loop to ensure that we fill send buffers
            r = dmSocket::Send(client_socket, buf, sizeof(buf), &received);
            if (r != dmSocket::RESULT_OK) {
                break;
            }
        }
        uint64_t end = dmTime::GetMonotonicTime();
        ASSERT_EQ(dmSocket::RESULT_WOULDBLOCK, r);
        ASSERT_GE(end - start, timeout - 2500); // NOTE: Margin of 2500. Required on Linux
    }

    dmSocket::Delete(server_socket);
    dmSocket::Delete(client_socket);
}
#endif

int main(int argc, char **argv)
{
    dmSocket::Initialize();
    jc_test_init(&argc, argv);
    int ret = jc_test_run_all();
    dmSocket::Finalize();
    return ret;
}
