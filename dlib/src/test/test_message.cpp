#include <stdint.h>
#include <vector>
#include <gtest/gtest.h>
#include "../../src/dlib/hash.h"
#include "../../src/dlib/message.h"
#include "../../src/dlib/dstrings.h"
#include "../../src/dlib/thread.h"
#include "../../src/dlib/time.h"
#include "../../src/dlib/profile.h"

const dmhash_t m_HashMessage1 = 0x35d47694;
const dmhash_t m_HashMessage2 = 0x35d47695;

struct CustomMessageData1
{
    uint32_t m_MyValue;
};

void HandleMessage(dmMessage::Message *message_object, void *user_ptr)
{
    switch(message_object->m_ID)
    {
        case m_HashMessage1:
        break;

        default:
            assert(false);
        break;
    }
}

TEST(dmMessage, Post)
{
    const uint32_t max_message_count = 16;
    dmMessage::HSocket socket;
    dmMessage::Result r;
    r = dmMessage::NewSocket("my_socket", &socket);
    ASSERT_EQ(dmMessage::RESULT_OK, r);

    for (uint32_t iter = 0; iter < 1024; ++iter)
    {
        for (uint32_t i = 0; i < max_message_count; ++i)
        {
            CustomMessageData1 message_data1;
            message_data1.m_MyValue = i;
            dmMessage::Post(socket, m_HashMessage1, &message_data1, sizeof(CustomMessageData1));
        }
        dmMessage::Dispatch(socket, HandleMessage, 0);
    }

    for (uint32_t i = 0; i < 2048; ++i)
    {
        CustomMessageData1 message_data1;
        message_data1.m_MyValue = i;
        dmMessage::Post(socket, m_HashMessage1, &message_data1, sizeof(CustomMessageData1));
    }

    dmMessage::Dispatch(socket, HandleMessage, 0);
    dmMessage::DeleteSocket(socket);
}

TEST(dmMessage, Bench)
{
    const uint32_t iter_count = 1024 * 16;
    dmMessage::HSocket socket;
    dmMessage::Result r;
    r = dmMessage::NewSocket("my_socket", &socket);
    ASSERT_EQ(dmMessage::RESULT_OK, r);

    CustomMessageData1 message_data1;
    message_data1.m_MyValue = 0;

    // "Warm up", i.e. allocate all pages needed internally
    for (uint32_t iter = 0; iter < iter_count; ++iter)
    {
        dmMessage::Post(socket, m_HashMessage1, &message_data1, sizeof(CustomMessageData1));
    }
    dmMessage::Dispatch(socket, HandleMessage, 0);

    // Benchmark
    uint64_t start = dmTime::GetTime();
    for (uint32_t iter = 0; iter < iter_count; ++iter)
    {
        dmMessage::Post(socket, m_HashMessage1, &message_data1, sizeof(CustomMessageData1));
    }
    dmMessage::Dispatch(socket, HandleMessage, 0);
    uint64_t end = dmTime::GetTime();
    printf("Bench elapsed: %f ms (%f us per call)\n", (end-start) / 1000.0f, (end-start) / float(iter_count));

    dmMessage::Dispatch(socket, HandleMessage, 0);
    dmMessage::DeleteSocket(socket);
}

TEST(dmMessage, Exists)
{
    dmMessage::HSocket socket1;
    dmMessage::HSocket socket2;
    dmMessage::Result r;
    r = dmMessage::NewSocket("my_socket", &socket1);
    ASSERT_EQ(dmMessage::RESULT_OK, r);

    r = dmMessage::NewSocket("my_socket", &socket2);
    ASSERT_EQ(dmMessage::RESULT_SOCKET_EXISTS, r);

    dmMessage::DeleteSocket(socket1);
}

TEST(dmMessage, OutofResources)
{
    std::vector<dmMessage::HSocket> sockets;
    dmMessage::Result r;
    char name[32];

    int i = 0;
    for (;;)
    {
        DM_SNPRINTF(name, sizeof(name), "my_socket_%d", i++);
        dmMessage::HSocket socket;
        r = dmMessage::NewSocket(name, &socket);
        if (r == dmMessage::RESULT_OK)
        {
            sockets.push_back(socket);
        }
        else
        {
            break;
        }
    }

    ASSERT_EQ(dmMessage::RESULT_SOCKET_OUT_OF_RESOURCES, r);
    for (uint32_t i = 0; i < sockets.size(); ++i)
    {
        dmMessage::DeleteSocket(sockets[i]);
    }
    sockets.clear();

    // Create the sockets again
    for (int j = 0; j < i; ++j)
    {
        DM_SNPRINTF(name, sizeof(name), "my_socket_%d", j);
        dmMessage::HSocket socket;
        r = dmMessage::NewSocket(name, &socket);
        if (r == dmMessage::RESULT_OK)
        {
            sockets.push_back(socket);
        }
        else
        {
            break;
        }
    }

    ASSERT_EQ(dmMessage::RESULT_SOCKET_OUT_OF_RESOURCES, r);
    for (uint32_t i = 0; i < sockets.size(); ++i)
    {
        dmMessage::DeleteSocket(sockets[i]);
    }

}

void HandleMessagePostDuring(dmMessage::Message *message_object, void *user_ptr)
{
    dmMessage::HSocket socket = *(dmMessage::HSocket*) user_ptr;
    CustomMessageData1 message_data1;
    message_data1.m_MyValue = 123;

    switch(message_object->m_ID)
    {
        case m_HashMessage2:
            dmMessage::Post(socket, m_HashMessage1, &message_data1, sizeof(CustomMessageData1));
        break;

        default:
            assert(false);
        break;
    }
}

TEST(dmMessage, PostDuringDispatch)
{
    const uint32_t max_message_count = 16;
    dmMessage::HSocket socket;
    dmMessage::Result r;
    r = dmMessage::NewSocket("my_socket", &socket);
    ASSERT_EQ(dmMessage::RESULT_OK, r);

    for (uint32_t iter = 0; iter < 1024; ++iter)
    {
        for (uint32_t i = 0; i < max_message_count; ++i)
        {
            CustomMessageData1 message_data1;
            message_data1.m_MyValue = i;
            dmMessage::Post(socket, m_HashMessage2, &message_data1, sizeof(CustomMessageData1));
        }
        uint32_t count;
        count = dmMessage::Dispatch(socket, HandleMessagePostDuring, &socket);
        ASSERT_EQ(max_message_count, count);

        count = dmMessage::Dispatch(socket, HandleMessage, 0);
        ASSERT_EQ(max_message_count, count);
    }

    uint32_t count;
    count = dmMessage::Dispatch(socket, HandleMessage, 0);
    ASSERT_EQ(0U, count);

    dmMessage::DeleteSocket(socket);
}

void PostThread(void* arg)
{
    dmMessage::HSocket socket = (dmMessage::HSocket) arg;

    for (int i = 0; i < 1024; ++i)
    {
        uint32_t m = i;
        dmMessage::Post(socket, m_HashMessage1, &m, sizeof(m));
    }
}

TEST(dmMessage, ThreadTest1)
{
    dmMessage::HSocket socket;
    dmMessage::Result r;
    r = dmMessage::NewSocket("my_socket", &socket);
    ASSERT_EQ(dmMessage::RESULT_OK, r);

    dmThread::Thread t1 = dmThread::New(&PostThread, 0xf0000, (void*) socket);
    dmThread::Thread t2 = dmThread::New(&PostThread, 0xf0000, (void*) socket);
    dmThread::Thread t3 = dmThread::New(&PostThread, 0xf0000, (void*) socket);
    dmThread::Thread t4 = dmThread::New(&PostThread, 0xf0000, (void*) socket);

    uint32_t count = 0;
    while (count < 1024 * 4)
    {
        count += dmMessage::Dispatch(socket, HandleMessage, 0);
    }
    ASSERT_EQ(1024U * 4U, count);

    dmThread::Join(t1);
    dmThread::Join(t2);
    dmThread::Join(t3);
    dmThread::Join(t4);

    count += dmMessage::Dispatch(socket, HandleMessage, 0);
    ASSERT_EQ(1024U * 4U, count);

    dmMessage::DeleteSocket(socket);
}

void HandleIntegrityMessage(dmMessage::Message *message_object, void *user_ptr)
{
    dmhash_t hash = dmHashBuffer64(message_object->m_Data, message_object->m_DataSize);
    assert(hash == message_object->m_ID);
}

TEST(dmMessage, Integrity)
{
    dmMessage::HSocket socket;
    dmMessage::Result r;
    r = dmMessage::NewSocket("my_socket", &socket);
    ASSERT_EQ(dmMessage::RESULT_OK, r);

    char msg[1024];
    for (uint32_t size = 1; size <= 1024; ++size)
    {
        for (uint32_t iter = 0; iter < 70; ++iter)
        {
            for (uint32_t i = 0; i < size; ++i)
            {
                msg[i] = rand() % 255;
            }
            dmhash_t hash = dmHashBuffer64(msg, size);

            dmMessage::Post(socket, hash, msg, size);
        }
        dmMessage::Dispatch(socket, HandleIntegrityMessage, 0);
    }

    dmMessage::Dispatch(socket, HandleIntegrityMessage, 0);
    dmMessage::DeleteSocket(socket);
}

int main(int argc, char **argv)
{
    dmProfile::Initialize(1024, 1024 * 1024, 64);
    testing::InitGoogleTest(&argc, argv);
    int ret = RUN_ALL_TESTS();
    dmProfile::Finalize();
    return ret;
}
