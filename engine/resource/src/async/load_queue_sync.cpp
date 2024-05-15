// Copyright 2020-2024 The Defold Foundation
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

#include "resource.h"
#include "resource_private.h"
#include "load_queue.h"

#include <dlib/dstrings.h>
#include <dlib/log.h>

namespace dmLoadQueue
{
    // Implementation of the LoadQueue API where all loads happen during the EndLoad call.
    // EndLoad thus never returns _PENDING

    struct Request
    {
        const char* m_Name;
        const char* m_CanonicalPath;
        PreloadInfo m_PreloadInfo;
    };

    struct Queue
    {
        dmResource::HFactory       m_Factory;
        Request                    m_SingleBuffer;
        Request*                   m_ActiveRequest;
        dmResource::LoadBufferType m_LoadBuffer;
    };

    HQueue CreateQueue(dmResource::HFactory factory)
    {
        Queue* q           = new Queue();
        q->m_ActiveRequest = 0;
        q->m_Factory       = factory;
        return q;
    }

    void DeleteQueue(HQueue queue)
    {
        delete queue;
    }

    HRequest BeginLoad(HQueue queue, const char* name, const char* canonical_path, PreloadInfo* info)
    {
        if (queue->m_ActiveRequest != 0)
        {
            return 0;
        }

        queue->m_ActiveRequest                  = &queue->m_SingleBuffer;
        queue->m_ActiveRequest->m_Name          = name;
        queue->m_ActiveRequest->m_CanonicalPath = canonical_path;
        queue->m_ActiveRequest->m_PreloadInfo   = *info;
        return queue->m_ActiveRequest;
    }

    Result EndLoad(HQueue queue, HRequest request, void** buf, uint32_t* size, LoadResult* load_result)
    {
        if (!queue || !request || queue->m_ActiveRequest != request)
        {
            return RESULT_INVALID_PARAM;
        }

        load_result->m_LoadResult    = dmResource::LoadResource(queue->m_Factory, request->m_CanonicalPath, request->m_Name, buf, size);
        load_result->m_PreloadResult = dmResource::RESULT_PENDING;
        load_result->m_PreloadData   = 0;

        if (load_result->m_LoadResult == dmResource::RESULT_OK && request->m_PreloadInfo.m_CompleteFunction)
        {
            dmResource::ResourcePreloadParams params;
            params.m_Factory             = queue->m_Factory;
            params.m_Context             = request->m_PreloadInfo.m_Context;
            params.m_Buffer              = *buf;
            params.m_BufferSize          = *size;
            params.m_HintInfo            = &request->m_PreloadInfo.m_HintInfo;
            params.m_PreloadData         = &load_result->m_PreloadData;
            load_result->m_PreloadResult = request->m_PreloadInfo.m_CompleteFunction(params);
        }
        return RESULT_OK;
    }

    void FreeLoad(HQueue queue, HRequest request)
    {
        queue->m_ActiveRequest   = 0;
        request->m_Name          = 0x0;
        request->m_CanonicalPath = 0x0;
    }
} // namespace dmLoadQueue
