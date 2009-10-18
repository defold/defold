#include <stdint.h>
#include <gtest/gtest.h>
#include "../dlib/thread.h"

struct ThreadArg
{
    uint32_t* m_P;
    uint32_t  m_Index;
    uint32_t  m_Value;
};

static void ThreadFunction(void* arg)
{
    ThreadArg* a = (ThreadArg*) arg;
    a->m_P[a->m_Index] = a->m_Value;
}

TEST(Thread, Basic1)
{
    uint32_t arr[4] = {0, 0};
    ThreadArg a1, a2;
    a1.m_P = arr;
    a1.m_Index = 0;
    a1.m_Value = 10;
    a2.m_P = arr;
    a2.m_Index = 1;
    a2.m_Value = 20;
    
    dmThread::Thread t1 = dmThread::New(&ThreadFunction, 0x80000, &a1);
    dmThread::Thread t2 = dmThread::New(&ThreadFunction, 0x80000, &a2);

    dmThread::Join(t1);
    dmThread::Join(t2);

    ASSERT_EQ(10, arr[0]);
    ASSERT_EQ(20, arr[1]);
}

int main(int argc, char **argv)
{
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}


