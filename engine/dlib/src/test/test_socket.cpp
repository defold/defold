#include <stdint.h>
#include <gtest/gtest.h>
#include "../dlib/socket.h"
#include "../dlib/thread.h"
#include "../dlib/time.h"

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
    dmSocket::Socket socket;
    dmSocket::Result r = dmSocket::New(dmSocket::TYPE_STREAM, dmSocket::PROTOCOL_TCP, &socket);
    EXPECT_EQ(r, dmSocket::RESULT_OK);

    dmSocket::Address local_address = dmSocket::AddressFromIPString("localhost");
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

TEST(Socket, ServerSocket1)
{
    dmSocket::Socket socket;
    dmSocket::Result r = dmSocket::New(dmSocket::TYPE_STREAM, dmSocket::PROTOCOL_TCP, &socket);
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

TEST(Socket, ServerSocket2)
{
    dmSocket::Socket socket1, socket2;
    dmSocket::Result r;
    r = dmSocket::New(dmSocket::TYPE_STREAM, dmSocket::PROTOCOL_TCP, &socket1);
    ASSERT_EQ(dmSocket::RESULT_OK, r)
        << "  Expected(" << dmSocket::ResultToString(dmSocket::RESULT_OK) << "), Actual(" << dmSocket::ResultToString(r) << ")" << std::endl;
    r = dmSocket::New(dmSocket::TYPE_STREAM, dmSocket::PROTOCOL_TCP, &socket2);
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

TEST(Socket, ServerSocket3)
{
    dmSocket::Socket socket;
    dmSocket::Result r = dmSocket::New(dmSocket::TYPE_STREAM, dmSocket::PROTOCOL_TCP, &socket);
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
    dmSocket::Result r = dmSocket::New(dmSocket::TYPE_STREAM, dmSocket::PROTOCOL_TCP, &socket);
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
    dmSocket::Result r = dmSocket::New(dmSocket::TYPE_STREAM, dmSocket::PROTOCOL_TCP, &socket);
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
    dmSocket::Result r = dmSocket::New(dmSocket::TYPE_STREAM, dmSocket::PROTOCOL_TCP, &server_socket);
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
    r = dmSocket::New(dmSocket::TYPE_STREAM, dmSocket::PROTOCOL_TCP, &client_socket);
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

