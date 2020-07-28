// Copyright 2020 The Defold Foundation
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
