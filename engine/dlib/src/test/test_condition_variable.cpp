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

// Verify notifications without waiters are safe, including non-threaded web builds.
TEST(dmConditionVariable, NotifyWithoutWaiters)
{
    dmConditionVariable::HConditionVariable condition = dmConditionVariable::New();
    dmConditionVariable::Signal(condition);
    dmConditionVariable::Broadcast(condition);
    dmConditionVariable::Delete(condition);
}

#if !defined(DM_NO_THREAD_SUPPORT)
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

// Verify a producer and consumer can hand off work using condition-variable signals.
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

struct BroadcastState
{
    dmMutex::HMutex                         m_Mutex;
    dmConditionVariable::HConditionVariable m_Ready;
    dmConditionVariable::HConditionVariable m_Release;
    uint32_t                                m_Waiting;
    uint32_t                                m_Woken;
    uint8_t                                 m_Released : 1;
};

static void WaitForBroadcast(void* context)
{
    BroadcastState* state = (BroadcastState*)context;
    dmMutex::Lock(state->m_Mutex);
    ++state->m_Waiting;
    dmConditionVariable::Signal(state->m_Ready);
    while (!state->m_Released)
    {
        dmConditionVariable::Wait(state->m_Release, state->m_Mutex);
    }
    ++state->m_Woken;
    dmMutex::Unlock(state->m_Mutex);
}

// Verify one broadcast releases every waiting thread; readiness is synchronized without sleeps.
TEST(dmConditionVariable, BroadcastWakesAllWaiters)
{
    BroadcastState state = {};
    state.m_Mutex = dmMutex::New();
    state.m_Ready = dmConditionVariable::New();
    state.m_Release = dmConditionVariable::New();
    const uint32_t thread_count = 3;
    dmThread::Thread threads[thread_count];
    for (uint32_t i = 0; i < thread_count; ++i)
    {
        threads[i] = dmThread::New(WaitForBroadcast, 0x80000, &state, "broadcast");
    }

    dmMutex::Lock(state.m_Mutex);
    while (state.m_Waiting != thread_count)
    {
        dmConditionVariable::Wait(state.m_Ready, state.m_Mutex);
    }
    // Each waiter holds this mutex until Wait atomically releases it, so all
    // waiters have reached the release condition before the producer gets here.
    state.m_Released = true;
    dmConditionVariable::Broadcast(state.m_Release);
    dmMutex::Unlock(state.m_Mutex);

    for (uint32_t i = 0; i < thread_count; ++i)
    {
        dmThread::Join(threads[i]);
    }
    ASSERT_EQ(thread_count, state.m_Woken);
    dmConditionVariable::Delete(state.m_Release);
    dmConditionVariable::Delete(state.m_Ready);
    dmMutex::Delete(state.m_Mutex);
}
#endif

int main(int argc, char **argv)
{
    jc_test_init(&argc, argv);
    return jc_test_run_all();
}
