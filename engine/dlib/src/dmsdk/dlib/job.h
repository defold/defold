#ifndef DMSDK_JOB_H
#define DMSDK_JOB_H

#include "buffer.h"

// TODO: Hack!? Expose atomic.h?
typedef volatile int32_t int32_atomic_t;

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
            void*             m_Pointer;
        };
    };

    enum ParamType
    {
        PARAM_TYPE_FLOAT    = 0,
        PARAM_TYPE_DOUBLE   = 1,
        PARAM_TYPE_INT      = 2,
        PARAM_TYPE_VECTOR4  = 3,
        PARAM_TYPE_MATRIX4  = 4,
        PARAM_TYPE_BUFFER   = 5,
        PARAM_TYPE_POINTER   = 6,
    };


    typedef void (*JobEntry)(HJob job, Param* params, uint8_t* param_types, int param_count);

    void RegisterJob(const char* name, JobEntry job_entry);

    void Init(uint32_t worker_count, uint32_t max_jobs);

    Result New(JobEntry entry, HJob* job, HJob parent);
    Result AddParamFloat(HJob job, float x);
    Result AddParamInt(HJob job, int x);
    Result AddParamDouble(HJob job, double x);
    Result AddParamBuffer(HJob job, dmBuffer::HBuffer b);
    Result AddParamPointer(HJob job, void* p);
    Result SetRunOnMain(HJob job, bool on_main);
    Result SetAutoComplete(HJob job, bool auto_complete);

    bool IsRootJob(HJob job);

    Result Run(HJob job);
    Result Wait(HJob job);
    void RunMainJobs();

    int GetActiveJobCount();

}

#endif // DMSDK_JOB_H
