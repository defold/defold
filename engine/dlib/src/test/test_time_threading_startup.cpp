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

#define JC_TEST_IMPLEMENTATION
#include <jc_test/jc_test.h>

#include <dlib/condition_variable.h>
#include <dlib/mutex.h>
#include <dlib/thread.h>
#include <dlib/time.h>

struct ClockStartup
{
    dmMutex::HMutex                         m_Mutex;
    dmConditionVariable::HConditionVariable m_Condition;
    uint32_t                                m_ReadyCount;
    bool                                    m_Start;
};

struct ClockReader
{
    ClockStartup* m_Startup;
    uint64_t      m_Times[128];
};

static void ReadClock(void* arg)
{
    ClockReader* reader = (ClockReader*)arg;
    ClockStartup* startup = reader->m_Startup;
    dmMutex::Lock(startup->m_Mutex);
    ++startup->m_ReadyCount;
    dmConditionVariable::Broadcast(startup->m_Condition);
    while (!startup->m_Start)
    {
        dmConditionVariable::Wait(startup->m_Condition, startup->m_Mutex);
    }
    dmMutex::Unlock(startup->m_Mutex);

    for (uint32_t i = 0; i < sizeof(reader->m_Times) / sizeof(reader->m_Times[0]); ++i)
    {
        reader->m_Times[i] = dmTime::GetMonotonicTime();
    }
}

// A separate executable exercises concurrent first clock calls without earlier tests initializing the Windows frequency cache.
TEST(dmTimeStartup, ConcurrentFirstCalls)
{
    const uint32_t thread_count = 8;
    ClockStartup startup = {};
    startup.m_Mutex = dmMutex::New();
    startup.m_Condition = dmConditionVariable::New();
    ClockReader readers[thread_count];
    dmThread::Thread threads[thread_count];
    for (uint32_t i = 0; i < thread_count; ++i)
    {
        readers[i].m_Startup = &startup;
        threads[i] = dmThread::New(ReadClock, 0x80000, &readers[i], "clock-reader");
    }

    dmMutex::Lock(startup.m_Mutex);
    while (startup.m_ReadyCount != thread_count)
    {
        dmConditionVariable::Wait(startup.m_Condition, startup.m_Mutex);
    }
    startup.m_Start = true;
    dmConditionVariable::Broadcast(startup.m_Condition);
    dmMutex::Unlock(startup.m_Mutex);

    for (uint32_t i = 0; i < thread_count; ++i)
    {
        dmThread::Join(threads[i]);
#if defined(_WIN32)
        dmThread::Detach(threads[i]);
#endif
    }
    dmConditionVariable::Delete(startup.m_Condition);
    dmMutex::Delete(startup.m_Mutex);

    uint64_t end = dmTime::GetMonotonicTime();
    for (uint32_t i = 0; i < thread_count; ++i)
    {
        for (uint32_t j = 1; j < sizeof(readers[i].m_Times) / sizeof(readers[i].m_Times[0]); ++j)
        {
            ASSERT_LE(readers[i].m_Times[j - 1], readers[i].m_Times[j]);
            ASSERT_LE(readers[i].m_Times[j], end);
        }
    }
}

int main(int argc, char** argv)
{
    jc_test_init(&argc, argv);
    return jc_test_run_all();
}
