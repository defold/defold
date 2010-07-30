#include <stdint.h>
#include <gtest/gtest.h>
#include "../../src/dlib/hash.h"
#include "../../src/dlib/message.h"

const uint32_t m_HashSocket = 0xf7d98495;
const uint32_t m_HashMessage1 = 0x35d47694;

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

TEST(dlib, TestMessage01)
{
    const uint32_t max_message_count = 16;
    dmMessage::CreateSocket(m_HashSocket, max_message_count * (sizeof(CustomMessageData1)));

    for (uint32_t iter = 0; iter < 1024; ++iter)
    {
        for (uint32_t i = 0; i < max_message_count; ++i)
	{
	    CustomMessageData1 message_data1;
	    message_data1.m_MyValue = i;
	    dmMessage::Post(m_HashSocket, m_HashMessage1, &message_data1, sizeof(CustomMessageData1));
	}
	dmMessage::Dispatch(m_HashSocket, HandleMessage, 0);
    }

    for (uint32_t i = 0; i < 2048; ++i)
    {
        CustomMessageData1 message_data1;
        message_data1.m_MyValue = i;
        dmMessage::Post(m_HashSocket, m_HashMessage1, &message_data1, sizeof(CustomMessageData1));
    }

    dmMessage::Dispatch(m_HashSocket, HandleMessage, 0);
    dmMessage::DestroySocket(m_HashSocket);
}

int main(int argc, char **argv)
{
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
