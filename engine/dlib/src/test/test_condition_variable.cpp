#include <stdint.h>
#include <gtest/gtest.h>
#include "../dlib/array.h"
#include "../dlib/thread.h"
#include "../dlib/mutex.h"
#include "../dlib/condition_variable.h"

#include <dlib/sol.h>

struct ThreadArg
{
    dmMutex::Mutex m_Mutex;
    dmConditionVariable::ConditionVariable m_Less;
    dmConditionVariable::ConditionVariable m_More;
    dmArray<int>    m_Buffer;
    int64_t         m_Sum;
};

static const int MAX = 15000;

static void Producer(void* arg)
{
    ThreadArg* a = (ThreadArg*) arg;

    int i = 0;
    while (i < MAX)
    {
        dmMutex::Lock(a->m_Mutex);
        if (a->m_Buffer.Full() && i < MAX) {
            dmConditionVariable::Wait(a->m_Less, a->m_Mutex);
        }

        while (!a->m_Buffer.Full() && i < MAX) {
            a->m_Buffer.Push(i);
            i++;
        }
        dmConditionVariable::Signal(a->m_More);
        dmMutex::Unlock(a->m_Mutex);
    }
}

static void Consumer(void* arg)
{
    ThreadArg* a = (ThreadArg*) arg;

    int i = 0;
    while (i < MAX)
    {
        dmMutex::Lock(a->m_Mutex);
        if (a->m_Buffer.Empty() && i < MAX) {
            dmConditionVariable::Wait(a->m_More, a->m_Mutex);
        }

        while (!a->m_Buffer.Empty() && i < MAX) {
            a->m_Sum += a->m_Buffer[0];
            a->m_Buffer.EraseSwap(0);
            i++;
        }
        dmConditionVariable::Signal(a->m_Less);
        dmMutex::Unlock(a->m_Mutex);
    }
}

TEST(dmConditionVariable, ProducerConsumer)
{
    ThreadArg a;
    a.m_Mutex = dmMutex::New();
    a.m_More = dmConditionVariable::New();
    a.m_Less = dmConditionVariable::New();
    a.m_Buffer.SetCapacity(16);
    a.m_Sum = 0;

    dmThread::Thread t1 = dmThread::New(&Producer, 0x80000, &a, "producer");
    dmThread::Thread t2 = dmThread::New(&Consumer, 0x80000, &a, "consumer");

    dmThread::Join(t1);
    dmThread::Join(t2);
    dmMutex::Delete(a.m_Mutex);
    dmConditionVariable::Delete(a.m_More);
    dmConditionVariable::Delete(a.m_Less);

    ASSERT_EQ((int64_t) (MAX/2) * (MAX-1), a.m_Sum);
}

int main(int argc, char **argv)
{
    dmSol::Initialize();
    testing::InitGoogleTest(&argc, argv);
    int r = RUN_ALL_TESTS();
    dmSol::FinalizeWithCheck();
    return r;
}
