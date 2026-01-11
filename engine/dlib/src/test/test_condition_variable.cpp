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
#include <dlib/array.h>
#include <dlib/thread.h>
#include <dlib/mutex.h>
#include <dlib/condition_variable.h>

struct ThreadArg
{
    dmMutex::HMutex m_Mutex;
    dmConditionVariable::HConditionVariable m_Less;
    dmConditionVariable::HConditionVariable m_More;
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
    jc_test_init(&argc, argv);
    return jc_test_run_all();
}
