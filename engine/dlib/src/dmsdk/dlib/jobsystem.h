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

#ifndef DMSDK_DLIB_JOBSYSTEM_H
#define DMSDK_DLIB_JOBSYSTEM_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*# SDK Job System API documentation
 *
 * Job system for asynchronous work with optional worker threads.
 *
 * Each job provides a process function and an optional callback.
 * - The process function performs the work and may run on a worker thread.
 * - The callback runs on the main thread and can interact with non-threaded systems (for example Lua).
 *
 * Jobs can have dependencies. A parent job will only run after all child jobs have finished.
 *
 * @document
 * @name Job System
 * @path engine/dlib/src/dmsdk/dlib/jobsystem.h
 * @language C
 */

/*# Job handle
 * @typedef
 * @name HJob
 */
typedef uint64_t HJob;

/*# Job system context
 * @typedef
 * @name HJobContext
 */
typedef struct JobContext* HJobContext;

/*# job status enumeration
 * @enum
 * @name JobSystemStatus
 * @member JOBSYSTEM_STATUS_FREE 0
 * @member JOBSYSTEM_STATUS_CREATED 1
 * @member JOBSYSTEM_STATUS_QUEUED 2
 * @member JOBSYSTEM_STATUS_PROCESSING 3
 * @member JOBSYSTEM_STATUS_FINISHED 4
 * @member JOBSYSTEM_STATUS_CANCELED 5
 */
enum JobSystemStatus
{
    JOBSYSTEM_STATUS_FREE             = 0,
    JOBSYSTEM_STATUS_CREATED          = 1,
    JOBSYSTEM_STATUS_QUEUED           = 2,
    JOBSYSTEM_STATUS_PROCESSING       = 3,
    JOBSYSTEM_STATUS_FINISHED         = 4,
    JOBSYSTEM_STATUS_CANCELED         = 5,
};

/*# job result enumeration
 * @enum
 * @name JobSystemResult
 * @member JOBSYSTEM_RESULT_OK 0
 * @member JOBSYSTEM_RESULT_ERROR 1
 * @member JOBSYSTEM_RESULT_INVALID_HANDLE 2
 * @member JOBSYSTEM_RESULT_CANCELED 3
 * @member JOBSYSTEM_RESULT_PENDING 4
 */
enum JobSystemResult
{
    JOBSYSTEM_RESULT_OK               = 0,
    JOBSYSTEM_RESULT_ERROR            = 1,
    JOBSYSTEM_RESULT_INVALID_HANDLE   = 2,
    JOBSYSTEM_RESULT_CANCELED         = 3,
    JOBSYSTEM_RESULT_PENDING          = 4,
};

/*# creation parameters
 * @struct
 * @name JobSystemCreateParams
 * @member m_ThreadNamePrefix [type:const char*] thread name prefix for worker threads
 * @member m_ThreadCount [type:uint8_t] number of worker threads (set to 0 to spawn no threads)
 */
struct JobSystemCreateParams
{
    const char* m_ThreadNamePrefix;
    uint8_t     m_ThreadCount;  // Set it to 0 to spawn no threads.
};

/*#
 * The callback to process the user data.
 * @note This call may occur on a separate thread
 * @typedef
 * @name FJobProcess
 * @param context [type: HJobContext] the job system context
 * @param job [type: HJob] the job
 * @param user_context [type: void*] the user context
 * @param user_data [type: void*] the user data
 * @return user_result [type: int32_t] user defined result. This is passed to the FJobCallback function.
 */
typedef int32_t (*FJobProcess)(HJobContext context, HJob job, void* user_context, void* user_data);

/*#
 * The callback to process the user data.
 * @note This call occurs on the game main thread
 * @typedef
 * @name FJobCallback
 * @param context [type: HJobContext] the job system context
 * @param job [type: HJob] the job
 * @param status [type: JobSystemStatus] the job status. JOBSYSTEM_STATUS_FINISHED if ok, or JOBSYSTEM_STATUS_CANCELED if it was canceled
 * @param user_context [type: void*] the user context
 * @param user_data [type: void*] the user data
 * @param user_result [type: int32_t] if status == JOBSYSTEM_STATUS_FINISHED, the user result returned from the process function. otherwise 0
 */
typedef void (*FJobCallback)(HJobContext context, HJob job, enum JobSystemStatus status, void* user_context, void* user_data, int32_t user_result);

/*#
 * Job parameters
 * @note This call may occur on a separate thread
 * @struct
 * @name Job
 * @member m_Process [type: FJobProcess] function to process the job. Called from a worker thread.
 * @member m_Callback [type: FJobCallback] function to process the job. Called from main thread.
 * @member m_Context [type: void*] the user context for the callbacks
 * @member m_Data [type: void*] the user data for the callbacks
 */
struct Job
{
    FJobProcess  m_Process;
    FJobCallback m_Callback;
    void*        m_Context;
    void*        m_Data;
};

/*# create job system context
 * Create a new job system context.
 * @name JobSystemCreate
 * @param create_params [type:JobSystemCreateParams*] creation parameters
 * @return context [type:HJobContext] job system context, or null on failure
 */
HJobContext JobSystemCreate(const struct JobSystemCreateParams* create_params);

/*# destroy job system context
 * Destroy a job system context and free its resources.
 * @name JobSystemDestroy
 * @param context [type:HJobContext] job system context
 */
void JobSystemDestroy(HJobContext context);

/*# update job system
 * Flush finished jobs and invoke callbacks on the main thread.
 * In single-threaded mode, this also processes jobs on the calling thread up to the time limit.
 * @name JobSystemUpdate
 * @param context [type:HJobContext] job system context
 * @param time_limit [type:uint64_t] max time (microseconds) to process jobs in single-threaded mode; 0 processes at most one job
 */
void JobSystemUpdate(HJobContext context, uint64_t time_limit);

/*# get worker count
 * @name JobSystemGetWorkerCount
 * @param context [type:HJobContext] job system context
 * @return count [type:uint32_t] number of worker threads
 */
uint32_t JobSystemGetWorkerCount(HJobContext context);

/*# create a job
 * @note Parent job will only run after all children has finished
 * @name JobSystemCreateJob
 * @param context [type:HJobContext] the job system context
 * @param job [type:Job*] the job creation parameters. This pointer is not stored, the data is copied as-is.
 * @return hjob [type:HJob] returns the job if successful. 0 otherwise.
 */
HJob JobSystemCreateJob(HJobContext context, struct Job* job);

/*# set a job as a dependency
 * @note Parent job will only run after all children has finished
 * @name JobSystemSetParent
 * @param context [type:HJobContext] the job system context
 * @param child [type:HJob] the child job
 * @param parent [type:HJob] the parent job
 * @return result [type:JobSystemResult] return JOBSYSTEM_RESULT_OK if job was pushed
 */
enum JobSystemResult JobSystemSetParent(HJobContext context, HJob child, HJob parent);

/*# push a job onto the work queue
 * @note A parent job needs to be pushed after its children
 * @name JobSystemPushJob
 * @param context [type:HJobContext] the job system context
 * @param job [type:HJob] the job to add to the work queue
 * @return result [type:JobSystemResult] return JOBSYSTEM_RESULT_OK if job was pushed
 */
enum JobSystemResult JobSystemPushJob(HJobContext context, HJob job);

/*# cancel a job (and its children)
 * @note Cancelled jobs will be flushed at the next JobSystemUpdate()
 * @name JobSystemCancelJob
 * @param context [type:HJobContext] the job system context
 * @param job [type:HJob] the job to cancel
 * @return result [type:JobSystemResult] Returns JOBSYSTEM_RESULT_OK if finished, JOBSYSTEM_RESULT_CANCELED if canceled, or JOBSYSTEM_RESULT_PENDING if the job (or any child) is still in flight
 * @examples
 * How to wait until a job has been cancelled or finished
 * ```c
 * enum JobSystemResult jr = JobSystemCancelJob(job_context, hjob);
 * while (JOBSYSTEM_RESULT_PENDING == jr)
 * {
 *     dmTime::Sleep(1000);
 *     jr = JobSystemCancelJob(job_context, hjob);
 * }
 * ```
 */
enum JobSystemResult JobSystemCancelJob(HJobContext context, HJob job);

/*# get the data from a job
 * @name JobSystemGetData
 * @param context [type:HJobContext] the job system context
 * @param job [type:HJob] the job
 * @return data [type:void*] The registered data pointer. 0 if the job handle is invalid.
 */
void* JobSystemGetData(HJobContext context, HJob job);

/*# get the context from a job
 * @name JobSystemGetContext
 * @param context [type:HJobContext] the job system context
 * @param job [type:HJob] the job
 * @return context [type:void*] The registered context pointer. 0 if the job handle is invalid.
 */
void* JobSystemGetContext(HJobContext context, HJob job);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // DMSDK_DLIB_JOBSYSTEM_H
