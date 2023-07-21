// Copyright 2020-2023 The Defold Foundation
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

#if !defined(DM_LU_NULL_IMPLEMENTATION)

#include <stdint.h>
#include <stdlib.h>
#include <dlib/log.h>
#include <dlib/time.h>

#include "../job_thread.h"

static volatile bool g_TestAsyncCallbackComplete = false;

#if defined(DM_USE_SINGLE_THREAD)
class AsyncTestMultiThread : public jc_test_base_class
#else
class AsyncTestSingleThread : public jc_test_base_class
#endif
{
public:
    virtual void SetUp()
    {
        m_JobThread = dmJobThread::Create("test_jobs");
    }
    virtual void TearDown()
    {
        dmJobThread::Destroy(m_JobThread);
    }

    dmJobThread::HContext m_JobThread;
};

struct JobData
{
    char m_Char;
    int  m_Result;
};

static int ProcessData(void* context, void* _data)
{
    HashState64* hash_state = (HashState64*)context;
    JobData* data = (JobData*)_data;

    data->m_Char--;
    dmHashUpdateBuffer64(hash_state, &data->m_Char, 1);
    return data->m_Char;
}

static void FinishData(void* context, void* _data, int result)
{
    JobData* data = (JobData*)_data;
    data->m_Result = result;
}

static void PushJob(dmJobThread::HContext thread, HashState64* hash_state, JobData* data)
{
    dmJobThread::PushJob(thread, (dmJobThread::FProcess)ProcessData, (dmJobThread::FCallback)FinishData, (void*)hash_state, (void*)data);
}

#if defined(DM_USE_SINGLE_THREAD)
TEST_F(AsyncTestMultiThread, TestJobs)
#else
TEST_F(AsyncTestSingleThread, TestJobs)
#endif
{
    const char* test_data = "TESTSTRING";
    uint32_t num_jobs = (uint32_t)strlen(test_data);
    JobData* contexts = new JobData[num_jobs];

    HashState64 hash_state;
    dmHashInit64(&hash_state, false);

    for (uint32_t i = 0; i < num_jobs; ++i)
    {
        contexts[i].m_Char = test_data[i] + 1;
        contexts[i].m_Result = 0;

        PushJob(m_JobThread, &hash_state, &contexts[i]);
    }

    uint64_t time_start = dmTime::GetTime();
    while (true)
    {
        if ((dmTime::GetTime() - time_start) >= 5 * 10000000)
        {
            dmLogError("Test timed out!");
            break;
        }

        dmJobThread::Update(m_JobThread); // Flushes finished async jobs', and calls any Lua callbacks

        int num_finished = 0;
        for (uint32_t i = 0; i < num_jobs; ++i)
        {
            num_finished += (contexts[i].m_Result ? 1 : 0);
        }

        if (num_finished == num_jobs)
            break;
    }

    for (uint32_t i = 0; i < num_jobs; ++i)
    {
        ASSERT_EQ(test_data[i], (char)contexts[i].m_Result);
    }

    // we make sure they all are executed in sequence
    dmhash_t digest = dmHashFinal64(&hash_state);
    ASSERT_EQ(dmHashString64(test_data), digest);

    delete[] contexts;
}

#endif // DM_LU_NULL_IMPLEMENTATION

int main(int argc, char **argv)
{
    jc_test_init(&argc, argv);
    int ret = jc_test_run_all();
    return ret;
}
