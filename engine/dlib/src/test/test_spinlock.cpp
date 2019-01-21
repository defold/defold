#include <stdint.h>
#include <stdlib.h>
#include <string>
#include "testutil.h"
#include "../dlib/spinlock.h"
#include "../dlib/thread.h"

// NOTE: volatile. Otherwise gcc will consider the addition loop invariant
volatile int32_t g_Value = 0;
dmSpinlock::lock_t g_Lock;

const int32_t ITER = 400000;

void Thread(void* arg)
{
    for (int i = 0; i < ITER; ++i)
    {
        dmSpinlock::Lock(&g_Lock);
        ++g_Value;
        dmSpinlock::Unlock(&g_Lock);
    }
}

TEST(dmSpinlock, Test)
{
    dmSpinlock::Init(&g_Lock);
    dmThread::Thread t1 = dmThread::New(Thread, 0xf000, 0, "t1");
    dmThread::Thread t2 = dmThread::New(Thread, 0xf000, 0, "t2");
    dmThread::Join(t1);
    dmThread::Join(t2);
    ASSERT_EQ(ITER * 2, g_Value);
}
