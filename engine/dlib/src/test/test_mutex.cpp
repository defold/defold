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
#define JC_TEST_IMPLEMENTATION
#include <jc_test/jc_test.h>
#include <dlib/atomic.h>
#include <dlib/mutex.h>
#include <dlib/thread.h>
#include <dlib/time.h>

struct ThreadArg
{
    int32_atomic_t      m_Value;
    dmMutex::HMutex     m_Mutex;
};

static void ThreadFunctionBasic(void* arg)
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

    dmThread::Thread t1 = dmThread::New(&ThreadFunctionBasic, 0x80000, &a, "t1");
    dmThread::Thread t2 = dmThread::New(&ThreadFunctionBasic, 0x80000, &a, "t2");

    dmThread::Join(t1);
    dmThread::Join(t2);
    dmMutex::Delete(a.m_Mutex);

    ASSERT_EQ((uint32_t) 10000*2, a.m_Value);
}

static void ThreadFunctionTryLock(void* arg)
{
    ThreadArg* a = (ThreadArg*) arg;

    dmMutex::Lock(a->m_Mutex);
    dmAtomicStore32(&a->m_Value, 0);
    while(dmAtomicGet32(&a->m_Value) == 0)
        dmTime::Sleep(10*1000);
    dmMutex::Unlock(a->m_Mutex);
    dmAtomicStore32(&a->m_Value, 0);
}

TEST(Mutex, TryLock)
{
    ThreadArg a;
    a.m_Value = 1;
    a.m_Mutex = dmMutex::New();

    dmThread::Thread t1 = dmThread::New(&ThreadFunctionTryLock, 0x80000, &a, "t1");
    while(dmAtomicGet32(&a.m_Value) == 1)
        dmTime::Sleep(10*1000);
    ASSERT_FALSE(dmMutex::TryLock(a.m_Mutex));
    dmAtomicStore32(&a.m_Value, 1);
    dmThread::Join(t1);

    ASSERT_TRUE(dmMutex::TryLock(a.m_Mutex));
    dmMutex::Unlock(a.m_Mutex);
    dmMutex::Delete(a.m_Mutex);
}

int main(int argc, char **argv)
{
    jc_test_init(&argc, argv);
    return jc_test_run_all();
}
