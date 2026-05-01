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
#include <stdlib.h>
#include <vector>
#define JC_TEST_IMPLEMENTATION
#include <jc_test/jc_test.h>
#include "../../src/dlib/hash.h"
#include "../../src/dlib/message.h"
#include "../../src/dlib/dstrings.h"
#include "../../src/dlib/thread.h"
#include "../../src/dlib/time.h"
#include "../../src/dlib/profile/profile.h"

const dmhash_t m_HashMessage1 = 0x35d47694;
const dmhash_t m_HashMessage2 = 0x35d47695;

struct CustomMessageData1
{
    uint32_t m_MyValue;
};

void HandleMessage(dmMessage::Message *message_object, void *user_ptr)
{
    switch(message_object->m_Id)
    {
        case m_HashMessage1:
            break;

        default:
            assert(false);
            break;
    }
}

TEST(dmMessage, Name)
{
    dmMessage::HSocket socket;
    ASSERT_EQ(dmMessage::RESULT_OK, dmMessage::NewSocket("test", &socket));
    ASSERT_EQ(0, strcmp("test", dmMessage::GetSocketName(socket)));
    ASSERT_EQ(dmMessage::RESULT_OK, dmMessage::DeleteSocket(socket));
    socket = 0;
    ASSERT_EQ((void*)0x0, (void*)dmMessage::GetSocketName(socket));
}

TEST(dmMessage, Validity)
{
    dmMessage::HSocket socket;
    ASSERT_EQ(dmMessage::RESULT_OK, dmMessage::NewSocket("test", &socket));
    ASSERT_TRUE(dmMessage::IsSocketValid(socket));
    ASSERT_EQ(dmMessage::RESULT_OK, dmMessage::DeleteSocket(socket));
    ASSERT_FALSE(dmMessage::IsSocketValid(socket));
}

TEST(dmMessage, Post)
{
    const uint32_t max_message_count = 16;
    dmMessage::URL receiver;
    dmMessage::ResetURL(&receiver);
    dmMessage::Result r;
    r = dmMessage::NewSocket("my_socket", &receiver.m_Socket);
    ASSERT_EQ(dmMessage::RESULT_OK, r);

    for (uint32_t iter = 0; iter < 1024; ++iter)
    {
        for (uint32_t i = 0; i < max_message_count; ++i)
        {
            CustomMessageData1 message_data1;
            message_data1.m_MyValue = i;
            ASSERT_EQ(dmMessage::RESULT_OK, dmMessage::Post(0x0, &receiver, m_HashMessage1, 0, 0x0, &message_data1, sizeof(CustomMessageData1), 0));
        }
        ASSERT_LT(0u, dmMessage::Dispatch(receiver.m_Socket, HandleMessage, 0));
    }

    for (uint32_t i = 0; i < 2048; ++i)
    {
        CustomMessageData1 message_data1;
        message_data1.m_MyValue = i;
        ASSERT_EQ(dmMessage::RESULT_OK, dmMessage::Post(0x0, &receiver, m_HashMessage1, 0, 0x0, &message_data1, sizeof(CustomMessageData1), 0));
    }

    ASSERT_LT(0u, dmMessage::Dispatch(receiver.m_Socket, HandleMessage, 0));
    ASSERT_EQ(dmMessage::RESULT_OK, dmMessage::DeleteSocket(receiver.m_Socket));
}

TEST(dmMessage, ParseURL)
{
    dmMessage::HSocket tmp_socket;
    ASSERT_EQ(dmMessage::RESULT_OK, dmMessage::NewSocket("socket", &tmp_socket));
    dmMessage::StringURL url;
    ASSERT_EQ(dmMessage::RESULT_OK, dmMessage::ParseURL(0x0, &url));
    ASSERT_EQ((void*)0, (void*)url.m_Socket);
    ASSERT_EQ(0u, url.m_SocketSize);
    ASSERT_EQ((void*)0, (void*)url.m_Path);
    ASSERT_EQ(0u, url.m_PathSize);
    ASSERT_EQ((void*)0, (void*)url.m_Fragment);
    ASSERT_EQ(0u, url.m_FragmentSize);
    ASSERT_EQ(dmMessage::RESULT_OK, dmMessage::ParseURL("", &url));
    ASSERT_EQ((void*)0, (void*)url.m_Socket);
    ASSERT_EQ(0, *url.m_Path);
    ASSERT_EQ((void*)0, url.m_Fragment);
    ASSERT_EQ(dmMessage::RESULT_OK, dmMessage::ParseURL("socket:", &url));
    ASSERT_EQ(0, strncmp("socket", url.m_Socket, url.m_SocketSize));
    ASSERT_EQ(dmMessage::RESULT_OK, dmMessage::ParseURL("path", &url));
    ASSERT_EQ(0, strncmp("path", url.m_Path, url.m_PathSize));
    ASSERT_EQ(dmMessage::RESULT_OK, dmMessage::ParseURL("#fragment", &url));
    ASSERT_EQ(0, strncmp("fragment", url.m_Fragment, url.m_FragmentSize));
    ASSERT_EQ(dmMessage::RESULT_OK, dmMessage::ParseURL("socket:path#fragment", &url));
    ASSERT_EQ(0, strncmp("socket", url.m_Socket, url.m_SocketSize));
    ASSERT_EQ(0, strncmp("path", url.m_Path, url.m_PathSize));
    ASSERT_EQ(0, strncmp("fragment", url.m_Fragment, url.m_FragmentSize));
    ASSERT_EQ(dmMessage::RESULT_OK, dmMessage::ParseURL("#", &url));
    ASSERT_EQ(0, strncmp("", url.m_Path, url.m_PathSize));
    ASSERT_EQ(0, strncmp("", url.m_Fragment, url.m_FragmentSize));
    ASSERT_EQ(dmMessage::RESULT_MALFORMED_URL, dmMessage::ParseURL("socket:path:fragment", &url));
    ASSERT_EQ(dmMessage::RESULT_MALFORMED_URL, dmMessage::ParseURL("socket#path#fragment", &url));
    ASSERT_EQ(dmMessage::RESULT_MALFORMED_URL, dmMessage::ParseURL("socket#path:fragment", &url));
    ASSERT_EQ(dmMessage::RESULT_OK, dmMessage::DeleteSocket(tmp_socket));
}

TEST(dmMessage, Bench)
{
    const uint32_t iter_count = 1024 * 16;
    dmMessage::URL receiver;
    dmMessage::ResetURL(&receiver);
    dmMessage::Result r;
    r = dmMessage::NewSocket("my_socket", &receiver.m_Socket);
    ASSERT_EQ(dmMessage::RESULT_OK, r);

    CustomMessageData1 message_data1;
    message_data1.m_MyValue = 0;

    // "Warm up", i.e. allocate all pages needed internally
    for (uint32_t iter = 0; iter < iter_count; ++iter)
    {
        ASSERT_EQ(dmMessage::RESULT_OK, dmMessage::Post(0x0, &receiver, m_HashMessage1, 0, 0x0, &message_data1, sizeof(CustomMessageData1), 0));
    }
    ASSERT_LT(0u, dmMessage::Dispatch(receiver.m_Socket, HandleMessage, 0));

    // Benchmark
    uint64_t start = dmTime::GetMonotonicTime();
    for (uint32_t iter = 0; iter < iter_count; ++iter)
    {
        ASSERT_EQ(dmMessage::RESULT_OK, dmMessage::Post(0x0, &receiver, m_HashMessage1, 0, 0x0, &message_data1, sizeof(CustomMessageData1), 0));
    }
    ASSERT_LT(0u, dmMessage::Dispatch(receiver.m_Socket, HandleMessage, 0));
    uint64_t end = dmTime::GetMonotonicTime();
    printf("Bench elapsed: %f ms (%f us per call)\n", (end-start) / 1000.0f, (end-start) / float(iter_count));

    ASSERT_EQ(0u, dmMessage::Dispatch(receiver.m_Socket, HandleMessage, 0));
    ASSERT_EQ(dmMessage::RESULT_OK, dmMessage::DeleteSocket(receiver.m_Socket));
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

    ASSERT_EQ(dmMessage::RESULT_OK, dmMessage::DeleteSocket(socket1));
}

TEST(dmMessage, OutofResources)
{
    std::vector<dmMessage::HSocket> sockets;
    dmMessage::Result r;
    char name[32];

    int i = 0;
    for (;;)
    {
        dmSnPrintf(name, sizeof(name), "my_socket_%d", i++);
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
        ASSERT_EQ(dmMessage::RESULT_OK, dmMessage::DeleteSocket(sockets[i]));
    }
    sockets.clear();

    // Create the sockets again
    for (int j = 0; j < i; ++j)
    {
        dmSnPrintf(name, sizeof(name), "my_socket_%d", j);
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
        ASSERT_EQ(dmMessage::RESULT_OK, dmMessage::DeleteSocket(sockets[i]));
    }

}

void HandleMessagePostDuring(dmMessage::Message *message_object, void *user_ptr)
{
    dmMessage::URL* receiver = (dmMessage::URL*) user_ptr;
    CustomMessageData1 message_data1;
    message_data1.m_MyValue = 123;

    switch(message_object->m_Id)
    {
        case m_HashMessage2:
            ASSERT_EQ(dmMessage::RESULT_OK, dmMessage::Post(0x0, receiver, m_HashMessage1, 0, 0x0, &message_data1, sizeof(CustomMessageData1), 0));
        break;

        default:
            assert(false);
        break;
    }
}

TEST(dmMessage, PostDuringDispatch)
{
    const uint32_t max_message_count = 16;
    dmMessage::URL receiver;
    dmMessage::ResetURL(&receiver);
    dmMessage::Result r;
    r = dmMessage::NewSocket("my_socket", &receiver.m_Socket);
    ASSERT_EQ(dmMessage::RESULT_OK, r);

    for (uint32_t iter = 0; iter < 1024; ++iter)
    {
        for (uint32_t i = 0; i < max_message_count; ++i)
        {
            CustomMessageData1 message_data1;
            message_data1.m_MyValue = i;
            ASSERT_EQ(dmMessage::RESULT_OK, dmMessage::Post(0x0, &receiver, m_HashMessage2, 0, 0x0, &message_data1, sizeof(CustomMessageData1), 0));
        }
        uint32_t count;
        count = dmMessage::Dispatch(receiver.m_Socket, HandleMessagePostDuring, &receiver);
        ASSERT_EQ(max_message_count, count);

        count = dmMessage::Dispatch(receiver.m_Socket, HandleMessage, 0);
        ASSERT_EQ(max_message_count, count);
    }

    uint32_t count;
    count = dmMessage::Dispatch(receiver.m_Socket, HandleMessage, 0);
    ASSERT_EQ(0U, count);

    ASSERT_EQ(dmMessage::RESULT_OK, dmMessage::DeleteSocket(receiver.m_Socket));
}

#if !defined(DM_NO_THREAD_SUPPORT)
#define T_ASSERT_EQ(_A, _B) \
    if ( (_A) != (_B) ) { \
        printf("%s:%d: ASSERT: %s != %s: %d != %d", __FILE__, __LINE__, #_A, #_B, (_A), (_B)); \
    } \
    assert( (_A) == (_B) );

static void PostThread(void* arg)
{
    dmMessage::URL* receiver = (dmMessage::URL*) arg;

    for (int i = 0; i < 1024; ++i)
    {
        uint32_t m = i;
        dmMessage::Result result = dmMessage::Post(0x0, receiver, m_HashMessage1, 0, 0x0, &m, sizeof(m), 0);
        T_ASSERT_EQ(dmMessage::RESULT_OK, result);

        if (i % 100 == 0) {
            dmTime::Sleep(1000);
        }
    }
}

TEST(dmMessage, ThreadTest1)
{
    dmMessage::URL receiver;
    dmMessage::ResetURL(&receiver);
    dmMessage::Result r;
    r = dmMessage::NewSocket("my_socket", &receiver.m_Socket);
    ASSERT_EQ(dmMessage::RESULT_OK, r);

    dmThread::Thread t1 = dmThread::New(&PostThread, 0xf0000, (void*) &receiver, "post1");
    dmThread::Thread t2 = dmThread::New(&PostThread, 0xf0000, (void*) &receiver, "post2");
    dmThread::Thread t3 = dmThread::New(&PostThread, 0xf0000, (void*) &receiver, "post3");
    dmThread::Thread t4 = dmThread::New(&PostThread, 0xf0000, (void*) &receiver, "post4");

    uint32_t count = 0;
    while (count < 1024 * 4)
    {
        count += dmMessage::Dispatch(receiver.m_Socket, HandleMessage, 0);
        dmTime::Sleep(1000);
    }
    ASSERT_EQ(1024U * 4U, count);

    dmThread::Join(t1);
    dmThread::Join(t2);
    dmThread::Join(t3);
    dmThread::Join(t4);

    count += dmMessage::Dispatch(receiver.m_Socket, HandleMessage, 0);
    ASSERT_EQ(1024U * 4U, count);

    ASSERT_EQ(dmMessage::RESULT_OK, dmMessage::DeleteSocket(receiver.m_Socket));
}

TEST(dmMessage, ThreadTest2)
{
    dmMessage::URL receiver;
    dmMessage::ResetURL(&receiver);
    dmMessage::Result r;
    r = dmMessage::NewSocket("my_socket", &receiver.m_Socket);
    ASSERT_EQ(dmMessage::RESULT_OK, r);

    dmThread::Thread t1 = dmThread::New(&PostThread, 0xf0000, (void*) &receiver, "post1");
    dmThread::Thread t2 = dmThread::New(&PostThread, 0xf0000, (void*) &receiver, "post2");
    dmThread::Thread t3 = dmThread::New(&PostThread, 0xf0000, (void*) &receiver, "post3");
    dmThread::Thread t4 = dmThread::New(&PostThread, 0xf0000, (void*) &receiver, "post4");

    uint32_t count = 0;
    while (count < 1024 * 4)
    {
        count += dmMessage::DispatchBlocking(receiver.m_Socket, HandleMessage, 0);
    }
    ASSERT_EQ(1024U * 4U, count);

    dmThread::Join(t1);
    dmThread::Join(t2);
    dmThread::Join(t3);
    dmThread::Join(t4);

    count += dmMessage::Dispatch(receiver.m_Socket, HandleMessage, 0);
    ASSERT_EQ(1024U * 4U, count);

    ASSERT_EQ(dmMessage::RESULT_OK, dmMessage::DeleteSocket(receiver.m_Socket));
}
#endif // DM_NO_THREAD_SUPPORT

void HandleIntegrityMessage(dmMessage::Message *message_object, void *user_ptr)
{
    dmhash_t hash = dmHashBuffer64(message_object->m_Data, message_object->m_DataSize);
    assert(hash == message_object->m_Id);
}

TEST(dmMessage, Integrity)
{
    dmMessage::URL receiver;
    dmMessage::ResetURL(&receiver);
    dmMessage::Result r;
    r = dmMessage::NewSocket("my_socket", &receiver.m_Socket);
    ASSERT_EQ(dmMessage::RESULT_OK, r);

    const uint32_t MESSAGE_SIZES[] = {
        1, 2, 4, 7, 9, 31, 32, 33, 511, 512, 513,
        dmMessage::DM_MESSAGE_MAX_DATA_SIZE / 3 - 1,
        dmMessage::DM_MESSAGE_MAX_DATA_SIZE / 3,
        dmMessage::DM_MESSAGE_MAX_DATA_SIZE / 3 + 1,
        dmMessage::DM_MESSAGE_MAX_DATA_SIZE / 2 - 1,
        dmMessage::DM_MESSAGE_MAX_DATA_SIZE / 2,
        dmMessage::DM_MESSAGE_MAX_DATA_SIZE / 2 + 1,
        dmMessage::DM_MESSAGE_MAX_DATA_SIZE - 1,
        dmMessage::DM_MESSAGE_MAX_DATA_SIZE};

    char msg[dmMessage::DM_MESSAGE_MAX_DATA_SIZE];
    for (uint32_t n = 0; n < (sizeof(MESSAGE_SIZES) / sizeof(uint32_t)); ++n)
    {
        uint32_t size = MESSAGE_SIZES[n];
        for (uint32_t iter = 0; iter < 15; ++iter)
        {
            for (uint32_t i = 0; i < size; ++i)
            {
                msg[i] = rand() % 255;
            }
            dmhash_t hash = dmHashBuffer64(msg, size);

            ASSERT_EQ(dmMessage::RESULT_OK, dmMessage::Post(0x0, &receiver, hash, 0, 0x0, msg, size, 0));
        }
        ASSERT_LT(0u, dmMessage::Dispatch(receiver.m_Socket, HandleIntegrityMessage, 0));
    }

    ASSERT_EQ(0u, dmMessage::Dispatch(receiver.m_Socket, HandleIntegrityMessage, 0));
    ASSERT_EQ(dmMessage::RESULT_OK, dmMessage::DeleteSocket(receiver.m_Socket));
}

void HandleUserDataMessage(dmMessage::Message *message_object, void *user_ptr)
{
    *((uint32_t*)user_ptr) = *((uint32_t*)message_object->m_UserData1);
}

TEST(dmMessage, UserData)
{
    dmMessage::URL receiver;
    dmMessage::ResetURL(&receiver);
    ASSERT_EQ(dmMessage::RESULT_OK, dmMessage::NewSocket("my_socket", &receiver.m_Socket));
    uint32_t sent = 1;
    uint32_t received = 0;
    ASSERT_EQ(dmMessage::RESULT_OK, dmMessage::Post(0x0, &receiver, 0, (uintptr_t)&sent, 0x0, 0x0, 0, 0));
    ASSERT_EQ(1u, dmMessage::Dispatch(receiver.m_Socket, HandleUserDataMessage, (void*)&received));
    ASSERT_EQ(sent, received);
    ASSERT_EQ(dmMessage::RESULT_OK, dmMessage::DeleteSocket(receiver.m_Socket));
}

TEST(dmMessage, MemLeaks)
{
    dmMessage::URL receiver;
    dmMessage::ResetURL(&receiver);
    ASSERT_EQ(dmMessage::RESULT_OK, dmMessage::NewSocket("my_socket", &receiver.m_Socket));
    for (uint32_t i = 0; i < 10000; ++i)
    {
        ASSERT_EQ(dmMessage::RESULT_OK, dmMessage::Post(0x0, &receiver, 0, 0, 0x0, 0x0, 0, 0));
    }
    ASSERT_EQ(dmMessage::RESULT_OK, dmMessage::DeleteSocket(receiver.m_Socket));
}

uint32_t g_PostDistpatchCalled = 0;

void CustomMessageDestroyCallback(dmMessage::Message* message)
{
    g_PostDistpatchCalled = *((uint32_t*)message->m_UserData1) + *((uint32_t*)message->m_UserData2);
}

TEST(dmMessage, MessagePostDispatch)
{
    dmMessage::URL receiver;
    dmMessage::ResetURL(&receiver);
    ASSERT_EQ(dmMessage::RESULT_OK, dmMessage::NewSocket("my_socket", &receiver.m_Socket));
    uint32_t sent1 = 42;
    uint32_t sent2 = 17;
    uint32_t received = 0;

    // Make sure we get a call after the dispatch
    ASSERT_EQ(dmMessage::RESULT_OK, dmMessage::Post(0x0, &receiver, 0, (uintptr_t)&sent1, (uintptr_t)&sent2, 0x0, 0x0, 0, CustomMessageDestroyCallback));
    ASSERT_EQ(1u, dmMessage::Dispatch(receiver.m_Socket, HandleUserDataMessage, (void*)&received));
    ASSERT_EQ(59, g_PostDistpatchCalled);

    g_PostDistpatchCalled = 0;
    sent1 = 7011;
    sent2 = 1100;
    ASSERT_EQ(dmMessage::RESULT_OK, dmMessage::Post(0x0, &receiver, 0, (uintptr_t)&sent1, (uintptr_t)&sent2, 0x0, 0x0, 0, CustomMessageDestroyCallback));

    // Make sure we get a call when the socket is destroyed
    ASSERT_EQ(dmMessage::RESULT_OK, dmMessage::DeleteSocket(receiver.m_Socket));
    ASSERT_EQ(8111, g_PostDistpatchCalled);
}


int main(int argc, char **argv)
{
    ProfileInitialize();
    jc_test_init(&argc, argv);
    int ret = jc_test_run_all();
    ProfileFinalize();
    return ret;
}
