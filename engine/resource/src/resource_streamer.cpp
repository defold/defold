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

#include <dlib/log.h>
#include <dlib/job_thread.h>
#include <dmsdk/resource/resource.h>
#include "resource_private.h"
#include "resource_util.h"

namespace dmResource {

struct ResourceStreamJob
{
    HResourceDescriptor     m_Resource;
    FPreloadDataCallback    m_Callback;
    void*                   m_CallbackCtx;
    dmArray<char>           m_Data;
    uint32_t                m_Offset;
    uint32_t                m_Size;
    const char*             m_Path;
    const char*             m_CanonicalPath;
};

static int JobProcess(dmJobThread::HContext, dmJobThread::HJob hjob, void* context, void* data)
{
    HFactory factory = (HFactory)context;
    ResourceStreamJob* job = (ResourceStreamJob*)data;

    if (job->m_Data.Capacity() < job->m_Size)
    {
        job->m_Data.SetCapacity(job->m_Size);
    }

    DM_MUTEX_SCOPED_LOCK(dmResource::GetLoadMutex(factory));

    uint32_t resource_size;
    uint32_t buffer_size;
    dmResource::Result result = dmResource::LoadResourceToBufferWithOffset(factory, job->m_CanonicalPath, job->m_Path, job->m_Offset, job->m_Size, &resource_size, &buffer_size, &job->m_Data);
    if (dmResource::RESULT_OK != result)
    {
        dmLogError("Failed to read chunk (offset: %u, size: %u) from '%s' (%s)", job->m_Offset, job->m_Size, job->m_Path, dmResource::ResultToString(result));
        return 0;
    }
    return 1;
}

static void JobCallback(dmJobThread::HContext, dmJobThread::HJob hjob, dmJobThread::JobStatus status, void* context, void* data, int result)
{
    HFactory factory = (HFactory)context;
    ResourceStreamJob* job = (ResourceStreamJob*)data;
    assert(job->m_Callback);

    dmResource::Result r = dmResource::GetDescriptor(factory, job->m_CanonicalPath, &job->m_Resource);
    if (dmResource::RESULT_OK != r)
    {
        dmLogError("RESOURCE STREAMER: No resource found for '%s'", job->m_Path);
    }
    else
    {
        if (job->m_Callback)
        {
            job->m_Callback(factory, job->m_CallbackCtx, job->m_Resource, job->m_Offset, job->m_Data.Size(), (uint8_t*)job->m_Data.Begin());
        }
    }

    free((void*)job->m_Path);
    free((void*)job->m_CanonicalPath);
    delete job;
}


Result PreloadData(HFactory factory, const char* path, uint32_t offset, uint32_t size, FPreloadDataCallback cbk, void* cbk_ctx)
{
    ResourceStreamJob* job = new ResourceStreamJob;
    memset(job, 0, sizeof(*job));
    job->m_Callback = cbk;
    job->m_CallbackCtx = cbk_ctx;
    job->m_Offset = offset;
    job->m_Size = size;

    char canonical_path[RESOURCE_PATH_MAX];
    dmResource::GetCanonicalPath(path, canonical_path, sizeof(canonical_path));

    job->m_CanonicalPath = strdup(canonical_path);
    job->m_Path = strdup(path);

    dmJobThread::HContext job_context = dmResource::GetJobThread(factory);
    assert(job_context != 0);

    dmJobThread::Job threadjob = {0};
    threadjob.m_Process = JobProcess;
    threadjob.m_Callback = JobCallback;
    threadjob.m_Context = (void*) factory;
    threadjob.m_Data = (void*) job;

    dmJobThread::HJob hjob = dmJobThread::CreateJob(job_context, &threadjob);
    dmJobThread::PushJob(job_context, hjob);

    return dmResource::RESULT_OK;
}

}
