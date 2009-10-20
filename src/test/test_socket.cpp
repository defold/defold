#include <stdint.h>
#include <gtest/gtest.h>
#include "../dlib/socket.h"

TEST(Socket, Basic1)
{
    dmSocket::Socket socket;
    dmSocket::Result r = dmSocket::New(dmSocket::TYPE_STREAM, dmSocket::PROTOCOL_TCP, &socket);
    EXPECT_EQ(r, dmSocket::RESULT_OK);

    dmSocket::Address local_address = dmSocket::AddressFromIPString("127.0.0.1");
    r = dmSocket::Connect(socket, local_address, 1);

    EXPECT_EQ(dmSocket::RESULT_CONNREFUSED, r);

    r = dmSocket::Delete(socket);
    ASSERT_EQ(dmSocket::RESULT_OK, r);
}

TEST(Socket, GetHostByName1)
{
    dmSocket::Address local_address = dmSocket::AddressFromIPString("127.0.0.1");

    dmSocket::Address a;
    dmSocket::Result r = dmSocket::GetHostByName("localhost", &a);
    ASSERT_EQ(dmSocket::RESULT_OK, r);
    EXPECT_EQ(local_address, a);
}

TEST(Socket, GetHostByName2)
{
    dmSocket::Address a;
    dmSocket::Result r = dmSocket::GetHostByName("host.nonexistingdomain", &a);
#ifdef _WIN32
    // NOTE: No idea why.
    ASSERT_EQ(dmSocket::RESULT_NO_DATA, r);
#else
    ASSERT_EQ(dmSocket::RESULT_HOST_NOT_FOUND, r);
#endif
}

TEST(Socket, ServerSocket1)
{
    dmSocket::Socket socket;
    dmSocket::Result r = dmSocket::New(dmSocket::TYPE_STREAM, dmSocket::PROTOCOL_TCP, &socket);
    ASSERT_EQ(dmSocket::RESULT_OK, r);

    const int port = 9000;

    r = dmSocket::Bind(socket, dmSocket::AddressFromIPString("0.0.0.0"), port);
    ASSERT_EQ(dmSocket::RESULT_OK, r);

    r = dmSocket::Listen(socket, 1000);
    ASSERT_EQ(dmSocket::RESULT_OK, r);
    
    r = dmSocket::Delete(socket);
    ASSERT_EQ(dmSocket::RESULT_OK, r);
}

TEST(Socket, ServerSocket2)
{
    dmSocket::Socket socket1, socket2;
    dmSocket::Result r;
    r = dmSocket::New(dmSocket::TYPE_STREAM, dmSocket::PROTOCOL_TCP, &socket1);
    ASSERT_EQ(dmSocket::RESULT_OK, r);
    r = dmSocket::New(dmSocket::TYPE_STREAM, dmSocket::PROTOCOL_TCP, &socket2);
    ASSERT_EQ(dmSocket::RESULT_OK, r);

    const int port = 9000;

    r = dmSocket::Bind(socket1, dmSocket::AddressFromIPString("0.0.0.0"), port);
    ASSERT_EQ(dmSocket::RESULT_OK, r);

    r = dmSocket::Bind(socket2, dmSocket::AddressFromIPString("0.0.0.0"), port);
    ASSERT_EQ(dmSocket::RESULT_ADDRINUSE, r);

    r = dmSocket::Delete(socket1);
    ASSERT_EQ(dmSocket::RESULT_OK, r);

    r = dmSocket::Delete(socket2);
    ASSERT_EQ(dmSocket::RESULT_OK, r);
}

TEST(Socket, ServerSocket3)
{
    dmSocket::Socket socket;
    dmSocket::Result r = dmSocket::New(dmSocket::TYPE_STREAM, dmSocket::PROTOCOL_TCP, &socket);
    ASSERT_EQ(dmSocket::RESULT_OK, r);

    r = dmSocket::SetBlocking(socket, false);
    ASSERT_EQ(dmSocket::RESULT_OK, r);

    const int port = 9000;

    r = dmSocket::Bind(socket, dmSocket::AddressFromIPString("0.0.0.0"), port);
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

int main(int argc, char **argv)
{
    dmSocket::Setup();
    testing::InitGoogleTest(&argc, argv);
    int ret = RUN_ALL_TESTS();
    dmSocket::Shutdown();
    return ret;
}

