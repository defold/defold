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
    typedef struct JobContext* HContext;
    typedef int (*FProcess)(void* context, void* data);
    typedef void (*FCallback)(void* context, void* data, int result);

    static const uint8_t DM_MAX_JOB_THREAD_COUNT = 8;

    struct JobThreadCreationParams
    {
        const char* m_ThreadNames[DM_MAX_JOB_THREAD_COUNT];
        uint8_t     m_ThreadCount;
    };

    HContext Create(const JobThreadCreationParams& create_params);
    void     Destroy(HContext context);
    void     Update(HContext context); // Flushes any items and calls PostProcess
    void     PushJob(HContext context, FProcess process, FCallback callback, void* user_context, void* data);
    uint32_t GetWorkerCount(HContext context);
    bool     PlatformHasThreadSupport();
}

#endif // DM_JOB_THREAD_H
