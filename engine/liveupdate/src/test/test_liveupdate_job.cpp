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

#if !defined(DM_LU_NULL_IMPLEMENTATION)

#include <stdint.h>
#include <stdlib.h>
#include <dlib/log.h>
#include <dlib/time.h>
#include <dlib/job_thread.h>

#if DM_TEST_THREAD_COUNT==0
#define TestClassName AsyncTestNoThread
#else
#define TestClassName AsyncTestThread
#endif

// Make the name nicer in the terminal output
#if DM_TEST_THREAD_COUNT==0
class AsyncTestNoThread : public jc_test_base_class
#else
class AsyncTestThread : public jc_test_base_class
#endif
{
public:
    void SetUp() override
    {
        dmJobThread::JobThreadCreationParams job_thread_create_param = {0};
        job_thread_create_param.m_ThreadCount = DM_TEST_THREAD_COUNT;
        m_JobThread = dmJobThread::Create(job_thread_create_param);
    }
    void TearDown() override
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

static int ProcessData(dmJobThread::HContext, dmJobThread::HJob, void* context, void* _data)
{
    HashState64* hash_state = (HashState64*)context;
    JobData* data = (JobData*)_data;

    data->m_Char--;
    dmHashUpdateBuffer64(hash_state, &data->m_Char, 1);
    return data->m_Char;
}

static void FinishData(dmJobThread::HContext, dmJobThread::HJob, dmJobThread::JobStatus status, void* context, void* _data, int result)
{
    JobData* data = (JobData*)_data;
    data->m_Result = result;
}

static void PushJob(dmJobThread::HContext thread, HashState64* hash_state, JobData* data)
{
    dmJobThread::Job job = {0};
    job.m_Process = ProcessData;
    job.m_Callback = FinishData;
    job.m_Context = (void*)hash_state;
    job.m_Data = (void*)data;
    dmJobThread::HJob hjob = dmJobThread::CreateJob(thread, &job);
    dmJobThread::PushJob(thread, hjob);
}

#if DM_TEST_THREAD_COUNT==0
TEST_F(AsyncTestNoThread, TestJobs)
#else
TEST_F(AsyncTestThread, TestJobs)
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

    uint64_t time_start = dmTime::GetMonotonicTime();
    while (true)
    {
        if ((dmTime::GetMonotonicTime() - time_start) >= 2 * 10000000)
        {
            dmLogError("Test timed out!");
            break;
        }

        dmJobThread::Update(m_JobThread, 0); // Flushes finished async jobs', and calls any Lua callbacks

        int num_finished = 0;
        for (uint32_t i = 0; i < num_jobs; ++i)
        {
            num_finished += (contexts[i].m_Result ? 1 : 0);
        }

        if (num_finished == num_jobs)
            break;

        dmTime::Sleep(1);
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
