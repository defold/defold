// Copyright 2020-2024 The Defold Foundation
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
    const char* thread_name = "TestThread";
    dmJobThread::HContext ctx = dmJobThread::Create(1, &thread_name);
    dmJobThread::PushJob(ctx, process, callback, 0, 0);
    dmJobThread::Destroy(ctx);
}

TEST(dmJobThread, PushJobsMultipleThreads)
{
    const char* thread_names[] = {"TestThread1", "TestThread2", "TestThread3", "TestThread4"};
    dmJobThread::HContext ctx = dmJobThread::Create(DM_ARRAY_SIZE(thread_names), thread_names);

    uint8_t contexts[8];
    uint8_t datas[8];
    for (int i = 0; i < DM_ARRAY_SIZE(contexts); ++i)
    {
        contexts[i] = i;
        datas[i] = 0;
        dmJobThread::PushJob(ctx, process, callback, (void*) &contexts[i], (void*) &datas[i]);
    }

    uint64_t stop_time = dmTime::GetTime() + 1*1e5; // 100 ms
    bool tests_done = false;
    while (dmTime::GetTime() < stop_time && !tests_done)
    {
        dmJobThread::Update(ctx);

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
