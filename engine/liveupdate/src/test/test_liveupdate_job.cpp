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
#include <stdint.h>
#include <stdlib.h>
#include <dlib/log.h>
#include <dlib/time.h>
#include <resource/resource.h>
#include "../liveupdate.h"
#include "../liveupdate_private.h"

class LiveUpdateJob : public jc_test_base_class
{
public:
    virtual void SetUp()
    {
        dmResource::NewFactoryParams params;
        params.m_MaxResources = 16;
        params.m_Flags = RESOURCE_FACTORY_FLAGS_RELOAD_SUPPORT;
        m_Factory = dmResource::NewFactory(&params, ".");
        ASSERT_NE((void*) 0, m_Factory);

        dmLiveUpdate::Initialize(m_Factory);
        dmLiveUpdate::RegisterArchiveLoaders();
    }
    virtual void TearDown()
    {
        dmLiveUpdate::Finalize();
        if (m_Factory)
            dmResource::DeleteFactory(m_Factory);
    }

    dmResource::HFactory m_Factory;
};

struct JobCallbackContext
{
    int m_Count;
    int m_Multiply;
};

struct JobCallbackData
{
    int m_Input;
    int m_Result;
    int m_Expected;
};

static int JobItemProcess(void* jobctx, void* jobdata)
{
    JobCallbackContext* ctx = (JobCallbackContext*)jobctx;
    JobCallbackData* data = (JobCallbackData*)jobdata;
    data->m_Result = data->m_Input * ctx->m_Multiply;
}

static void JobItemCallback(void* jobctx, void* jobdata, int result)
{
    JobCallbackContext* ctx = (JobCallbackContext*)jobctx;
    ctx->m_Count++;

    JobCallbackData* data = (JobCallbackData*)jobdata;
    ASSERT_EQ(data->m_Expected, data->m_Result);

    printf("Finished job %d\n", data->m_Input);
    delete data;
}

TEST_F(LiveUpdateJob, TestJob)
{
    JobCallbackContext ctx;
    ctx.m_Count = 0;
    ctx.m_Multiply = 3;

    uint32_t num_jobs = 4;
    for (uint32_t i = 0; i < num_jobs; ++i)
    {
        JobCallbackData* data = new JobCallbackData;
        data->m_Input = i+1;
        data->m_Expected = data->m_Input * ctx.m_Multiply;
        ASSERT_TRUE(dmLiveUpdate::PushAsyncJob(JobItemProcess, JobItemCallback, &ctx, data));
    }

    while (ctx.m_Count < num_jobs)
    {
        dmTime::Sleep(100);
        dmLiveUpdate::Update();
    }
}


int main(int argc, char **argv)
{
    jc_test_init(&argc, argv);
    int ret = jc_test_run_all();
    return ret;
}
