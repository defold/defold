#ifndef DM_JOB_H
#define DM_JOB_H

#include "atomic.h"
#include "buffer.h"

namespace dmJob
{
    struct JobHandle
    {
        void*          m_Job;
        int32_atomic_t m_Version;
    };

    //typedef struct Job* HJob;
    typedef struct JobHandle HJob;

    const uint32_t MAX_JOB_PARAMS = 24;

    const JobHandle INVALID_JOB = {0, 0};

    enum Result
    {
        RESULT_OK = 0,
        RESULT_TOO_MAX_PARAMS = -1,
        RESULT_STALE_HANDLE = -2,
        RESULT_OUT_OF_JOBS = -3,
    };

    struct Param
    {
        union
        {
            float             m_Float;
            double            m_Double;
            int               m_Int;
            dmBuffer::HBuffer m_Buffer;
        };
    };


    typedef void (*JobEntry)(Param* params);

    void RegisterJob(const char* name, JobEntry job_entry);

    void Init(uint32_t worker_count, uint32_t max_jobs);

    Result New(JobEntry entry, HJob* job, HJob parent);
    Result AddParamFloat(HJob job, float x);
    Result AddParamInt(HJob job, int x);
    Result AddParamDouble(HJob job, double x);
    Result AddParamBuffer(HJob job, dmBuffer::HBuffer b);

    Result Run(HJob job);
    Result Wait(HJob job);

    int GetActiveJobCount();

}

#endif // DM_JOB_H
