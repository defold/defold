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

#ifndef DM_RESOURCE_LOAD_QUEUE_H
#define DM_RESOURCE_LOAD_QUEUE_H

#include "../resource.h"
#include "../resource_private.h"
#include <dmsdk/resource/resource.h>

namespace dmLoadQueue
{
    // The following interface is used by resource.cpp to perform actual loading of files.
    // It is not exposed to the outside world.
    enum Result
    {
        RESULT_OK            = 0,
        RESULT_PENDING       = -1,
        RESULT_INVALID_PARAM = -2
    };

    typedef struct Queue* HQueue;
    typedef struct Request* HRequest;

    struct PreloadInfo
    {
        ResourcePreloadHintInfo     m_HintInfo;
        dmResource::HResourceType   m_Type;
        // TODO: Now that we have the type, we can access these at the call site
        FResourcePreload            m_CompleteFunction;
        void*                       m_Context;
    };

    struct LoadResult
    {
        dmResource::Result m_LoadResult;
        dmResource::Result m_PreloadResult;
        void* m_PreloadData;
        bool m_IsBufferOwnershipTransferred; // If true, the resource preloader has taken ownership of the data
    };

    HQueue CreateQueue(dmResource::HFactory factory);
    void DeleteQueue(HQueue queue);

    // If the queue does not want to accept any more requests at the moment, it returns 0
    // The name and canonical_path provided must have a lifetime that lasts until EndLoad is called
    HRequest BeginLoad(HQueue queue, const char* name, const char* canonical_path, PreloadInfo* info);

    // Actual load result will be put in load_result. Ptrs can be handled until FreeLoad has been called.
    Result EndLoad(HQueue queue, HRequest request, void** buffer, uint32_t* buffer_size, uint32_t* resource_size, LoadResult* load_result);

    // Free once completed.
    void FreeLoad(HQueue queue, HRequest request);
} // namespace dmLoadQueue

#endif // DM_RESOURCE_LOAD_QUEUE_H
