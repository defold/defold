#include <stdint.h>
#include <gtest/gtest.h>
#include "dlib/hash.h"
#include "dlib/event.h"

const uint32_t m_HashSocket = 0xf7d98495;
const uint32_t m_HashEvent1 = 0x35d47694;

struct CustomEventData1
{
	uint32_t m_MyValue;
};

void HandleEvent(dmEvent::Event *event_object, void *user_ptr)
{
    switch(event_object->m_ID)
    {
        case m_HashEvent1:
        break;

        default:
            assert(false);
        break;
    }
}

TEST(dlib, TestEvent01)
{
    const uint32_t max_event_count = 16;
    dmEvent::Register(m_HashEvent1, sizeof(CustomEventData1));
    // TODO: +12 is considered as hack.. (sizeof(dmEvent::Event))
    dmEvent::CreateSocket(m_HashSocket, max_event_count * (sizeof(CustomEventData1) + 12));

    for (uint32_t iter = 0; iter < 1024; ++iter)
    {
        for (uint32_t i = 0; i < max_event_count; ++i)
	{
	    CustomEventData1 event_data1;
	    event_data1.m_MyValue = i;
	    dmEvent::Post(m_HashSocket, m_HashEvent1, &event_data1);
	}
	dmEvent::Dispatch(m_HashSocket, HandleEvent, 0);
    }

#if 0
    // TODO: Remove or make this test work to (drop events)
    for (uint32_t i = 0; i < 2048; ++i)
    {
        CustomEventData1 event_data1;
        event_data1.m_MyValue = i;
        dmEvent::Post(m_HashSocket, m_HashEvent1, &event_data1);
    }

    dmEvent::Dispatch(m_HashSocket, HandleEvent, 0);
#endif

    dmEvent::DestroySocket(m_HashSocket);
    dmEvent::Unregister(m_HashEvent1);

}

int main(int argc, char **argv)
{
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
