// Copyright 2020-2025 The Defold Foundation
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

#include "dlib/job_thread.h"
#include "dlib/array.h"
#include "dlib/time.h"

#define JC_TEST_IMPLEMENTATION
#include <jc_test/jc_test.h>

int process(void* context, void* data)
{
    return 1;
}

void callback(void* context, void* data, int result)
{
    uint8_t* ctx = (uint8_t*) context;
    uint8_t* d = (uint8_t*) data;
    *d = result;
}

TEST(dmJobThread, PushJobs)
{
    dmJobThread::JobThreadCreationParams job_thread_create_param;
    job_thread_create_param.m_ThreadNames[0] = "DefoldTestJobThread1";
    job_thread_create_param.m_ThreadCount    = 1;

    dmJobThread::HContext ctx = dmJobThread::Create(job_thread_create_param);
    dmJobThread::PushJob(ctx, process, callback, 0, 0);
    dmJobThread::Destroy(ctx);
}

TEST(dmJobThread, PushJobsMultipleThreads)
{
    dmJobThread::JobThreadCreationParams job_thread_create_params;
    job_thread_create_params.m_ThreadNames[0] = "DefoldTestJobThread1";
    job_thread_create_params.m_ThreadNames[1] = "DefoldTestJobThread2";
    job_thread_create_params.m_ThreadNames[2] = "DefoldTestJobThread3";
    job_thread_create_params.m_ThreadNames[3] = "DefoldTestJobThread4";
    job_thread_create_params.m_ThreadCount    = 4;

    dmJobThread::HContext ctx = dmJobThread::Create(job_thread_create_params);

    uint8_t contexts[8];
    uint8_t datas[8];
    for (int i = 0; i < DM_ARRAY_SIZE(contexts); ++i)
    {
        contexts[i] = i;
        datas[i] = 0;
        dmJobThread::PushJob(ctx, process, callback, (void*) &contexts[i], (void*) &datas[i]);
    }

    uint64_t stop_time = dmTime::GetMonotonicTime() + 1*1e6; // 1 second
    bool tests_done = false;
    while (dmTime::GetMonotonicTime() < stop_time && !tests_done)
    {
        dmJobThread::Update(ctx, 0);

        tests_done = true;
        for (int i = 0; i < DM_ARRAY_SIZE(contexts); ++i)
        {
            tests_done &= datas[i];
        }

        dmTime::Sleep(20*1000);
    }

    dmJobThread::Destroy(ctx);

    ASSERT_TRUE(tests_done);
}

int main(int argc, char **argv)
{
    jc_test_init(&argc, argv);
    return jc_test_run_all();
}
