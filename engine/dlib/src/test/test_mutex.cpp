#include <stdint.h>
#include <gtest/gtest.h>
#include "../dlib/thread.h"
#include "../dlib/mutex.h"

struct ThreadArg
{
    uint32_t       m_Value;
    dmMutex::Mutex m_Mutex;
};

static void ThreadFunction(void* arg)
{
    ThreadArg* a = (ThreadArg*) arg;

    for (int i = 0; i < 10000; ++i)
    {
        dmMutex::Lock(a->m_Mutex);
        a->m_Value++;
        dmMutex::Unlock(a->m_Mutex);
    }
}

TEST(Mutex, Basic)
{
    ThreadArg a;
    a.m_Value = 0;
    a.m_Mutex = dmMutex::New();

    dmThread::Thread t1 = dmThread::New(&ThreadFunction, 0x80000, &a, "t1");
    dmThread::Thread t2 = dmThread::New(&ThreadFunction, 0x80000, &a, "t2");

    dmThread::Join(t1);
    dmThread::Join(t2);
    dmMutex::Delete(a.m_Mutex);

    ASSERT_EQ((uint32_t) 10000*2, a.m_Value);
}

int main(int argc, char **argv)
{
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
