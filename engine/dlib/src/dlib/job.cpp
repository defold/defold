#include <assert.h>
#include <pthread.h>
#include <stdio.h>

#include "array.h"
#include "align.h"
#include "atomic.h"
#include "dstrings.h"
#include "math.h"
#include "mutex.h"
#include "poolallocator.h"
#include "index_pool.h"
#include "condition_variable.h"
#include "static_assert.h"
#include "buffer.h"
#include "log.h"
#include "job.h"
#include "thread.h"
#include "profile.h"

/*
Links
  https://blog.molecular-matters.com/2015/08/24/job-system-2-0-lock-free-work-stealing-part-1-basics/
  https://blog.molecular-matters.com/2012/07/09/building-a-load-balanced-task-scheduler-part-4-false-sharing/
  https://manu343726.github.io/2017/03/13/lock-free-job-stealing-task-system-with-modern-c.html
  https://groups.google.com/forum/?#!starred/emscripten-discuss/CMfYljLWMvY
  Example https://pastebin.com/iyKwsYvK
*/

namespace dmJob
{
    dmThread::TlsKey g_WorkerTlsKey = dmThread::AllocTls();

    struct Job
    {
        Job* DM_ALIGNED(256) m_Parent;
        Job*                 m_Next;
        JobEntry             m_JobEntry;
        int32_atomic_t       m_UnfinishedJobs;
        int32_atomic_t       m_Version;
        Param                m_Params[MAX_JOB_PARAMS];
        uint8_t              m_ParamTypes[MAX_JOB_PARAMS];
        uint32_t             m_ParamCount;
        uint8_t              m_RunOnMain : 1;
        uint8_t              m_AutoComplete : 1;
    };

    const uint32_t MAX_WORKERS = 16;

    struct JobQueue
    {
        dmMutex::Mutex m_Lock;
        dmArray<Job*>  m_Jobs;

        JobQueue()
        {
            m_Lock = dmMutex::New();
        }

        ~JobQueue()
        {
            dmMutex::Delete(m_Lock);
        }

        void Push(Job* job)
        {
            DM_MUTEX_SCOPED_LOCK(m_Lock);
            if (m_Jobs.Full()) {
                m_Jobs.OffsetCapacity(64);
            }
            m_Jobs.Push(job);
        }

        Job* Pop()
        {
            DM_MUTEX_SCOPED_LOCK(m_Lock);
            uint32_t n = m_Jobs.Size();
            if (n == 0) {
                return 0;
            } else {
                Job* j = m_Jobs[n - 1];
                m_Jobs.SetSize(n - 1);
                return j;
            }
        }
    };

    struct Worker
    {
        dmThread::Thread  m_Thread;
        bool              m_Active;
        char              m_Name[32];
    };

    struct JobSystem
    {
        dmConditionVariable::ConditionVariable m_Condition;
        dmMutex::Mutex                         m_Lock;
        JobQueue                               m_Queue;
        JobQueue                               m_MainQueue;
        Worker*                                m_Workers[MAX_WORKERS];
        dmArray<Job>                           m_Jobs;
        dmIndexPool32                          m_JobPool;
        int32_atomic_t                         m_WorkerCount;
        int32_atomic_t                         m_ActiveJobs;
        int32_atomic_t                         m_CompletedJobs;
        int32_atomic_t                         m_NextVersion;
    };

    JobSystem* g_JobSystem = 0;

    static Job* WaitForJob(Worker* w)
    {
        JobSystem* js = g_JobSystem;
        Job *j = 0;
        do  {
            j = js->m_Queue.Pop();
            if (j == 0) {
                dmMutex::Lock(js->m_Lock);
                if (w->m_Active) {
                    dmConditionVariable::Wait(js->m_Condition, js->m_Lock);
                }
                dmMutex::Unlock(js->m_Lock);
            }
        } while (j == 0 && w->m_Active);
        return j;
    }

    static void Execute(Job *job);

    static void HelpOut(Job* job_waiting)
    {
        JobSystem* js = g_JobSystem;
        Job* j = js->m_Queue.Pop();
        if (j) {
            Execute(j);
        } else {
            // TODO: Add functionality to thread.h
            pthread_yield_np();
        }
    }

    void RunMainJobs()
    {
        JobSystem* js = g_JobSystem;
        Job* j = js->m_MainQueue.Pop();
        while (j) {
            Execute(j);
            j = js->m_MainQueue.Pop();
        }
    }

    static void FreeJob(Job* job) {
        JobSystem* js = g_JobSystem;
        DM_MUTEX_SCOPED_LOCK(js->m_Lock);

        dmAtomicIncrement32(&job->m_Version);
        uint32_t job_index = job - js->m_Jobs.Begin();
        memset(job, 0xcc, sizeof(*job));
        js->m_JobPool.Push(job_index);

    }

    static void Finish(Job *job)
    {
        JobSystem *js = g_JobSystem;
        dmAtomicIncrement32(&js->m_CompletedJobs);
        dmAtomicDecrement32(&js->m_ActiveJobs);

        Job *pj = job->m_Parent;
        int32_atomic_t uf = dmAtomicDecrement32(&job->m_UnfinishedJobs) - 1;
        assert(uf >= 0);
        if (pj) {
            uf = dmAtomicDecrement32(&pj->m_UnfinishedJobs) - 1;
            assert(uf >= 0);
        }
    }

    static void DoExecute(Job* job)
    {
        JobHandle jh;
        jh.m_Job = job;
        jh.m_Version = job->m_Version;
        Worker *w = (Worker *)dmThread::GetTlsValue(g_WorkerTlsKey);

        const char *profile_name = "Job Main";
        if (w) {
            profile_name = w->m_Name;
        }
        {
            DM_PROFILE(Job, profile_name);
            job->m_JobEntry(jh, job->m_Params, job->m_ParamTypes, job->m_ParamCount);
        }
        Finish(job);
    }

    static void TryFreeJob(Job* job)
    {
        if (job->m_AutoComplete || (job->m_Parent != 0 && job->m_UnfinishedJobs == 0)) {
            FreeJob(job);
        }
    }

    static Job *TryExecuteList(Job *job, bool *did_exec)
    {
        Job *first_job = job;
        Job *j = first_job;
        Job *prev_job = first_job;
        *did_exec = false;
        Job* to_free = 0;

        assert(job->m_Next != (Job *)0xcccccccccccccccc);
        while (j)
        {
            //assert(j->m_UnfinishedJobs >= 1);
            Job *next = j->m_Next;
            assert(next != (Job *)0xcccccccccccccccc);
            if (j->m_UnfinishedJobs == 1)
            {
                DoExecute(j);

                if (j == first_job)
                {
                    //printf("Removing first job %p\n", j);
                    to_free = j;
                    Job *n = j->m_Next;
                    assert(n != (Job *)0xcccccccccccccccc);
                    first_job = n;
                }
                else
                {
                    //printf("Removing job %p\n", j);
                    to_free = prev_job->m_Next;
                                  assert(j->m_Next != (Job *)0xcccccccccccccccc);
                    prev_job->m_Next = j->m_Next;
                }
                *did_exec = true;
                break;
            }
            prev_job = j;
            //j = j->m_Next;
            j = next;
        }

        if (to_free) {
            TryFreeJob(to_free);
        }

        assert(first_job != (Job *)0xcccccccccccccccc);
        return first_job;
    }

    static Job *TryExecuteList__(Job *job, bool *did_exec)
    {
        Job *first_job = job;
        Job *j = first_job;
        Job *prev_job = first_job;
        *did_exec = false;

        assert(job->m_Next != (Job *)0xcccccccccccccccc);
        while (j)
        {
            //assert(j->m_UnfinishedJobs >= 1);
            Job *next = j->m_Next;
            assert(next != (Job *)0xcccccccccccccccc);
            if (j->m_UnfinishedJobs == 1)
            {
                DoExecute(j);

                if (j == first_job)
                {
                    //printf("Removing first job %p\n", j);
                    Job *n = j->m_Next;
                    assert(n != (Job *)0xcccccccccccccccc);
                    TryFreeJob(j);
                    first_job = n;
                }
                else
                {
                    //printf("Removing job %p\n", j);
                    TryFreeJob(prev_job->m_Next);
                    assert(j->m_Next != (Job *)0xcccccccccccccccc);
                    prev_job->m_Next = j->m_Next;
                }
                *did_exec = true;
                break;
            }
            prev_job = j;
            //j = j->m_Next;
            j = next;
        }

        assert(first_job != (Job *)0xcccccccccccccccc);
        return first_job;
    }

    static void Execute(Job *job)
    {
        Job *first_job = job;
        JobSystem* js = g_JobSystem;

        while (first_job) {
            bool did_exec = false;
            first_job = TryExecuteList(first_job, &did_exec);
            if (!did_exec) {
                Job* new_j = js->m_Queue.Pop();
                if (new_j) {
                    new_j->m_Next = first_job;
                    first_job = new_j;
                }
            }
        }
    }

    void WorkerMain(void* w)
    {
        Worker* worker = (Worker*) w;
        dmThread::SetTlsValue(g_WorkerTlsKey, (void*) w);
        while (worker->m_Active) {
            Job* j = WaitForJob(worker);
            if (j) {
                Execute(j);
            }
        }
    }

    static Job* ToJob(HJob job)
    {
        Job* j = (Job*) job.m_Job;
        if (j->m_Version != job.m_Version)
        {
            dmLogWarning("Accessing stale job handle (0x%p %d)", j, job.m_Version);
            return 0;
        }
        return j;
    }

    #define CHECK_JOB(name, job) Job* name = ToJob(job); if (!name) return RESULT_STALE_HANDLE;

    Result Wait(HJob job)
    {
        CHECK_JOB(j, job);
        while (j->m_UnfinishedJobs > 0) {
            HelpOut(j);
        }

        FreeJob(j);
        //dmAtomicIncrement32(&j->m_Version);
        return RESULT_OK;
    }

    int GetActiveJobCount()
    {
        JobSystem* js = g_JobSystem;
        return js->m_ActiveJobs;
    }

    static Worker* NewWorker()
    {
        int index = dmAtomicIncrement32(&g_JobSystem->m_WorkerCount);
        Worker* w = new Worker;
        w->m_Active = true;
        DM_SNPRINTF(w->m_Name, sizeof(w->m_Name), "Worker %d", index);
        w->m_Thread = dmThread::New(WorkerMain, 0x80000, w, w->m_Name);
        g_JobSystem->m_Workers[index] = w;
        return w;
    }

    void Initialize(uint32_t worker_count, uint32_t max_jobs)
    {
        // Make sure that an array of jobs is properly aligned in order to
        // avoid false sharing
        DM_STATIC_ASSERT(sizeof(Job) % 64 == 0, Invalid_Struct_Size);

        if (g_JobSystem) {
            return;
        }
        assert(g_JobSystem == 0);
        worker_count = dmMath::Min(MAX_WORKERS, worker_count);
        g_JobSystem = new JobSystem;
        g_JobSystem->m_Condition = dmConditionVariable::New();
        g_JobSystem->m_Lock = dmMutex::New();
        g_JobSystem->m_WorkerCount = 0;
        g_JobSystem->m_ActiveJobs = 0;
        g_JobSystem->m_CompletedJobs = 0;
        g_JobSystem->m_NextVersion = 0;
        g_JobSystem->m_Jobs.SetCapacity(max_jobs);
        g_JobSystem->m_Jobs.SetSize(max_jobs);
        g_JobSystem->m_JobPool.SetCapacity(max_jobs);

        for (uint32_t i = 0; i < worker_count; i++) {
            (void) NewWorker();
        }
    }

    // TODO: Should we wait for all jobs to complete first?
    void Finalize()
    {
        if (!g_JobSystem)
        {
            return;
        }

        JobSystem* js = g_JobSystem;
        for (uint32_t i = 0; i < js->m_WorkerCount; i++)
        {
            js->m_Workers[i]->m_Active = false;
        }

        for (uint32_t i = 0; i < js->m_WorkerCount; i++)
        {
            printf("Shuting down thread %d\n", i);
            dmMutex::Lock(js->m_Lock);
            dmConditionVariable::Broadcast(js->m_Condition);
            dmMutex::Unlock(js->m_Lock);

            dmThread::Join(js->m_Workers[i]->m_Thread);
        }

        delete g_JobSystem;
        g_JobSystem = 0;
    }

    Result New(JobEntry entry, HJob* job)
    {
        JobSystem* js = g_JobSystem;
        DM_MUTEX_SCOPED_LOCK(js->m_Lock);

        if (js->m_JobPool.Remaining() == 0) {
            // TODO: Help out here instead?
            *job = INVALID_JOB;
            return RESULT_OUT_OF_JOBS;
        }

        uint32_t job_index = js->m_JobPool.Pop();
        Job* j = &js->m_Jobs[job_index];
        memset(j, 0, sizeof(Job));
        j->m_JobEntry = entry;
        j->m_UnfinishedJobs = 1;
        j->m_Version = dmAtomicIncrement32(&js->m_NextVersion);
        job->m_Job = j;
        job->m_Version = j->m_Version;

        dmAtomicIncrement32(&js->m_ActiveJobs);
        //*job = j;
        return RESULT_OK;
    }

    Result AddParamFloat(HJob job, float x)
    {
        CHECK_JOB(j, job);
        if (j->m_ParamCount >= MAX_JOB_PARAMS) {
            return RESULT_TOO_MAX_PARAMS;
        }
        uint32_t i = j->m_ParamCount++;
        j->m_Params[i].m_Float = x;
        j->m_ParamTypes[i] = PARAM_TYPE_FLOAT;
        return RESULT_OK;
    }

    Result AddParamDouble(HJob job, double x)
    {
        CHECK_JOB(j, job);
        if (j->m_ParamCount >= MAX_JOB_PARAMS) {
            return RESULT_TOO_MAX_PARAMS;
        }
        uint32_t i = j->m_ParamCount++;
        j->m_Params[i].m_Double = x;
        j->m_ParamTypes[i] = PARAM_TYPE_DOUBLE;
        return RESULT_OK;
    }

    Result AddParamInt(HJob job, int x)
    {
        CHECK_JOB(j, job);
        if (j->m_ParamCount >= MAX_JOB_PARAMS) {
            return RESULT_TOO_MAX_PARAMS;
        }
        uint32_t i = j->m_ParamCount++;
        j->m_Params[i].m_Int = x;
        j->m_ParamTypes[i] = PARAM_TYPE_INT;
        return RESULT_OK;
    }

    Result AddParamBuffer(HJob job, dmBuffer::HBuffer buffer)
    {
        CHECK_JOB(j, job);
        if (j->m_ParamCount >= MAX_JOB_PARAMS) {
            return RESULT_TOO_MAX_PARAMS;
        }
        uint32_t i = j->m_ParamCount++;
        j->m_Params[i].m_Buffer = buffer;
        j->m_ParamTypes[i] = PARAM_TYPE_BUFFER;
        return RESULT_OK;
    }

    Result AddParamPointer(HJob job, void* p)
    {
        CHECK_JOB(j, job);
        if (j->m_ParamCount >= MAX_JOB_PARAMS) {
            return RESULT_TOO_MAX_PARAMS;
        }
        uint32_t i = j->m_ParamCount++;
        j->m_Params[i].m_Pointer = p;
        j->m_ParamTypes[i] = PARAM_TYPE_POINTER;
        return RESULT_OK;
    }

    Result SetRunOnMain(HJob job, bool on_main)
    {
        CHECK_JOB(j, job);
        j->m_RunOnMain = on_main;
    }

    Result SetAutoComplete(HJob job, bool auto_complete)
    {
        CHECK_JOB(j, job);
        j->m_AutoComplete = auto_complete;
    }

    bool IsRootJob(HJob job)
    {
        CHECK_JOB(j, job);
        return j->m_Parent == 0;
    }

    Result Run(HJob job, HJob parent)
    {
        CHECK_JOB(j, job);
        JobSystem *js = g_JobSystem;

        j->m_Parent = (Job*) parent.m_Job;
        if (parent.m_Job) {
            CHECK_JOB(pj, parent);
            dmAtomicIncrement32(&pj->m_UnfinishedJobs);
            assert(pj->m_UnfinishedJobs <= js->m_JobPool.Capacity());
        }

        if (j->m_RunOnMain) {
            js->m_MainQueue.Push(j);
        } else {
            js->m_Queue.Push(j);
        }

        // TODO: Signal or broadcast?
        dmConditionVariable::Signal(js->m_Condition);
        return RESULT_OK;
    }

    Result Run(HJob job)
    {
        return Run(job, INVALID_JOB);
    }
}
