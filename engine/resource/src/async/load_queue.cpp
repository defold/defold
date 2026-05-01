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

#include "load_queue.h"
#include "load_queue_private.h"

namespace dmLoadQueue
{

dmResource::Result DoLoadResource(dmResource::HFactory factory, HRequest request, dmResource::LoadBufferType* buffer, LoadResult* result)
{
    dmResource::HResourceType resource_type = request->m_PreloadInfo.m_Type;
    uint32_t preload_size = RESOURCE_INVALID_PRELOAD_SIZE;
    if (ResourceTypeIsStreaming(resource_type))
    {
        preload_size = ResourceTypeGetPreloadSize(resource_type);
    }

    result->m_LoadResult = dmResource::LoadResourceToBuffer(factory, request->m_CanonicalPath, request->m_Name, preload_size, &request->m_ResourceSize, &request->m_BufferSize, buffer);
    result->m_PreloadResult = dmResource::RESULT_PENDING;
    result->m_PreloadData   = 0;
    result->m_IsBufferOwnershipTransferred = false;

    if (result->m_LoadResult == dmResource::RESULT_OK)
    {
        if (request->m_BufferSize == 0)
        {
            // Only happens for unit tests, the buffer must not be null
            // We only check it, but we don't use it
            buffer->SetCapacity(1);
            buffer->SetSize(0);
        }

        assert(buffer->Size() == request->m_BufferSize);
        assert(request->m_BufferSize <= request->m_ResourceSize);
        if (request->m_PreloadInfo.m_CompleteFunction)
        {
            ResourcePreloadParams params;
            params.m_Factory                = factory;
            params.m_Context                = request->m_PreloadInfo.m_Context;
            params.m_Buffer                 = buffer->Begin();
            params.m_BufferSize             = request->m_BufferSize;
            params.m_FileSize               = request->m_ResourceSize;
            params.m_IsBufferPartial        = request->m_BufferSize != request->m_ResourceSize;
            params.m_IsBufferTransferrable  = 1;
            params.m_Filename               = request->m_CanonicalPath;
            params.m_HintInfo               = &request->m_PreloadInfo.m_HintInfo;

            // out
            params.m_PreloadData = &result->m_PreloadData;
            params.m_IsBufferOwnershipTransferred = &result->m_IsBufferOwnershipTransferred;

            result->m_PreloadResult = (dmResource::Result)request->m_PreloadInfo.m_CompleteFunction(&params);
        }
        else
        {
            result->m_PreloadResult = dmResource::RESULT_OK;
        }
    }
    return dmResource::RESULT_OK;
}

} // namespace dmLoadQueue
