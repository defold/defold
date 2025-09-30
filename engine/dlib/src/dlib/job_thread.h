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

#ifndef DM_JOB_THREAD_H
#define DM_JOB_THREAD_H

#include <stdint.h>

namespace dmJobThread
{
    typedef uint64_t HJob;

    typedef struct JobContext* HContext;
    typedef int (*FProcess)(HJob job, uint64_t tag, void* context, void* data);
    typedef void (*FCallback)(HJob job, uint64_t tag, void* context, void* data, int result);

    static const uint8_t DM_MAX_JOB_THREAD_COUNT = 8;

    enum JobStatus
    {
        JOB_STATUS_FREE             = 0,
        JOB_STATUS_CREATED          = 1,
        JOB_STATUS_QUEUED           = 2,
        JOB_STATUS_PROCESSING       = 3,
        JOB_STATUS_FINISHED         = 4,
        JOB_STATUS_CANCELED         = 5,
    };

    enum JobResult
    {
        JOB_RESULT_OK               = 0,
        JOB_RESULT_ERROR            = 1,
        JOB_RESULT_INVALID_HANDLE   = 2,
        JOB_RESULT_PENDING          = 3, // the job is still processing
    };

    struct JobThreadCreationParams
    {
        const char* m_ThreadNames[DM_MAX_JOB_THREAD_COUNT];
        uint8_t     m_ThreadCount;  // Set it to 0 to spawn no threads.
    };

    struct Job
    {
        FProcess    m_Process;
        FCallback   m_Callback;     // result: >0 if successful, 0 if failure or canceled
        void*       m_Context;      // User job context
        void*       m_Data;         // User payload data
        uint64_t    m_Tag;          // Use to identify items of the same batch
    };

    HContext Create(const JobThreadCreationParams& create_params);
    void     Destroy(HContext context);
    // Flushes any finished items and calls PostProcess
    void     Update(HContext context, uint64_t time_limit);
    uint32_t GetWorkerCount(HContext context);
    bool     PlatformHasThreadSupport();

    /** set a job as a dependency
     * @note Parent job will only run after all children has finished
     * @name CreateJob
     * @param context [type: HContext] the job thread context
     * @param job [type: Job*] the job creation parameters
     * @return hjob [type: HJob] returns the job if successful. 0 otherwise.
     */
    HJob CreateJob(HContext context, Job* job);

    /** set a job as a dependency
     * @note Parent job will only run after all children has finished
     * @name SetParent
     * @param context [type: HContext] the job thread context
     * @param child [type: HJob] the child job
     * @param parent [type: HJob] the parent job
     * @return result [type: JobResult] return JOB_RESULT_OK if job was pushed
     */
    JobResult SetParent(HContext context, HJob child, HJob parent);

    /** push a job onto the work queue
     * @name PushJob
     * @note A parent job needs to be pushed after its children
     * @param context [type: HContext] the job thread context
     * @param hjob [type: HJob] the job to cancel
     * @return result [type: JobResult] return JOB_RESULT_OK if job was pushed
     */
    JobResult PushJob(HContext context, HJob job);

    /** cancel a job
     * @name CancelJob
     * @param context [type: HContext] the job thread context
     * @param hjob [type: HJob] the job to cancel
     * @return result [type: JobResult] Returns JOB_RESULT_OK if was canceled. Returns JOB_RESULT_PENDING if it's still in flight
     */
    JobResult CancelJob(HContext context, HJob job);

    // Deprecated
    bool PushJob(HContext context, FProcess process, FCallback callback, void* user_context, void* data);

    // Debug
    void DebugPrintJobs(HContext context);
}

#endif // DM_JOB_THREAD_H
