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

/**
 * The job system is designed to be able to do jobs asynchronously, if threads are available, but also support single threaded systems.
 * Each job implements a "process" function, and an optional "callback" function.
 * - The process function is the one that should be doing the work.
 *   Note that for single threaded systems, this work is still done on the main thread.
 * - The callback function is always called on the main thread.
 *   This may be good if you need to sync with non-threaded systems like Lua
 *
 * - Jobs can have dependencies
 *   Each job can have a parent job.
 *   Setting a parent must be done before any of the jobs are pushed onto the queue.
 *   The parent job won't be run processed until all the children are processed.
 */

namespace dmJobThread
{
    typedef uint64_t HJob;

    typedef struct JobContext* HContext;

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
        JOB_RESULT_CANCELED         = 3,
        JOB_RESULT_PENDING          = 4, // the job is still processing
    };

    struct JobThreadCreationParams
    {
        const char* m_ThreadNamePrefix;
        uint8_t     m_ThreadCount;  // Set it to 0 to spawn no threads.
    };

    /*#
     * The callback to process the user data.
     * @note This call may occur on a separate thread
     * @typedef
     * @name FProcess
     * @param context [type: HContext] the job thread context
     * @param job [type: HJob] the job
     * @param user_context [type: void*] the user context
     * @param user_data [type: void*] the user data
     * @return user_result [type: int32_t] user defined result
     */
    typedef int32_t (*FProcess)(HContext context, HJob job, void* user_context, void* user_data);

    /*#
     * The callback to process the user data.
     * @note This call occurs on the game main thread
     * @typedef
     * @name FCallback
     * @param context [type: HContext] the job thread context
     * @param job [type: HJob] the job
     * @param status [type: JobStatus] the job status of the job. status == JOB_STATUS_FINISHED if ok, or JOB_STATUS_CANCELED if it was canceled.
     * @param user_context [type: void*] the user context
     * @param user_data [type: void*] the user data
     * @param user_result [type: int32_t] if status == JOB_STATUS_FINISHED, the user result returned from the process function. otherwise 0
     */
    typedef void (*FCallback)(HContext context, HJob job, JobStatus status, void* user_context, void* user_data, int32_t user_result);

    /*#
     * The callback to process the user data.
     * @note This call may occur on a separate thread
     * @struct
     * @name Job
     * @member m_Process [type: FProcess] function to process the job. Called from a worker thread.
     * @member m_Callback [type: FCallback] function to process the job. Called from main thread.
     * @member m_Context [type: void*] the user context for the callbacks
     * @member m_Data [type: void*] the user data for the callbacks
     */
    struct Job
    {
        FProcess    m_Process;
        FCallback   m_Callback;     // result: >0 if successful, 0 if failure or canceled
        void*       m_Context;      // User job context
        void*       m_Data;         // User payload data
    };

    HContext Create(const JobThreadCreationParams& create_params);
    void     Destroy(HContext context);
    // Flushes any finished items and calls PostProcess
    void     Update(HContext context, uint64_t time_limit);
    uint32_t GetWorkerCount(HContext context);
    bool     PlatformHasThreadSupport();

    /*# set a job as a dependency
     * @note Parent job will only run after all children has finished
     * @name CreateJob
     * @param context [type: HContext] the job thread context
     * @param job [type: Job*] the job creation parameters. This pointer is not stored, the data is copied as-is.
     * @return hjob [type: HJob] returns the job if successful. 0 otherwise.
     */
    HJob CreateJob(HContext context, Job* job);

    /*# set a job as a dependency
     * @note Parent job will only run after all children has finished
     * @name SetParent
     * @param context [type: HContext] the job thread context
     * @param child [type: HJob] the child job
     * @param parent [type: HJob] the parent job
     * @return result [type: JobResult] return JOB_RESULT_OK if job was pushed
     */
    JobResult SetParent(HContext context, HJob child, HJob parent);

    /*# push a job onto the work queue
     * @name PushJob
     * @note A parent job needs to be pushed after its children
     * @param context [type: HContext] the job thread context
     * @param job [type: HJob] the job to add to the work queue
     * @return result [type: JobResult] return JOB_RESULT_OK if job was pushed
     */
    JobResult PushJob(HContext context, HJob job);

    /*# cancel a job (and it's children)
     * @note Cancelled jobs will be flushed at the next Update()
     * @name CancelJob
     * @param context [type: HContext] the job thread context
     * @param job [type: HJob] the job to cancel
     * @return result [type: JobResult] Returns JOB_RESULT_OK if finished, JOB_RESULT_CANCELED if canceled, or JOB_RESULT_PENDING if the job (or any child) is still in flight
     * @examples
     * How to wait until a job has been cancelled or finished
     * ```c++
     * dmJobThread::JobResult jr = dmJobThread::CancelJob(job_context, hjob);
     * while (dmJobThread::JOB_RESULT_PENDING == jr)
     * {
     *     dmTime::Sleep(1000);
     *     jr = dmJobThread::CancelJob(job_context, hjob);
     * }
     * ```
     */
    JobResult CancelJob(HContext context, HJob job);

    /*# get the data from a job
     * @name GetData
     * @param context [type: HContext] the job thread context
     * @param job [type: HJob] the job to cancel
     * @return data [type: void*] The registered data pointer. 0 if the job handle is invalid.
     */
    void* GetData(HContext context, HJob job);

    /*# get the context from a job
     * @name GetContext
     * @param context [type: HContext] the job thread context
     * @param job [type: HJob] the job to cancel
     * @return context [type: void*] The registered context pointer. 0 if the job handle is invalid.
     */
    void* GetContext(HContext context, HJob job);

    // Debug
    void DebugPrintJobs(HContext context);
}

#endif // DM_JOB_THREAD_H
