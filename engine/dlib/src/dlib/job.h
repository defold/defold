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

    typedef struct Job* HJob;

    const uint32_t MAX_JOB_PARAMS = 24;

    enum Result
    {
        RESULT_OK = 0,
        RESULT_TOO_MAX_PARAMS = -1,
    };

    struct Param
    {
        union 
        {
            float             m_Float;
            double            m_Double;
            dmBuffer::HBuffer m_Buffer;
        };
    };

    
    typedef void (*JobEntry)(Param* params);
    
    void RegisterJob(const char* name, JobEntry job_entry);

    void Init(uint32_t worker_count);

    Result New(JobEntry entry, HJob* job, HJob parent);
    Result AddParam(HJob job, float x);
    Result AddParam(HJob job, double x);
    Result AddParam(HJob job, dmBuffer::HBuffer b);

    Result Run(HJob job);
    Result Wait(HJob job);

}

#endif // DM_JOB_H
