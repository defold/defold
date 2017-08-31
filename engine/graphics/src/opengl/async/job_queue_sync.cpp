
#include <assert.h>
#include <dlib/log.h>
#include "job_queue.h"

namespace dmGraphics
{
    void JobQueuePush(const JobDesc& job)
    {
        assert(job.m_Func);
        job.m_Func(job.m_Context);
        if(job.m_FuncComplete)
        {
            job.m_FuncComplete(job.m_Context);
        }
    }

    void JobQueueInitialize()
    {
        dmLogDebug("AsyncInitialize: Auxillary context unsupported (threads not supported)");
    }

    void JobQueueFinalize()
    {
    }

    bool JobQueueIsAsync()
    {
        return false;
    }

};
