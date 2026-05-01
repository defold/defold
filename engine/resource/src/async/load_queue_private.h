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

#ifndef DM_RESOURCE_LOAD_QUEUE_PRIVATE_H
#define DM_RESOURCE_LOAD_QUEUE_PRIVATE_H

#include "load_queue.h"
#include "../resource_private.h"
#include <dmsdk/resource/resource.h>

namespace dmLoadQueue
{
    struct Request
    {
        const char*                m_Name;
        const char*                m_CanonicalPath;
        PreloadInfo                m_PreloadInfo;
        uint32_t                   m_ResourceSize;
        uint32_t                   m_BufferSize;
        // for the threaded requests
        dmResource::LoadBufferType m_Buffer;
        LoadResult                 m_Result;
    };

    dmResource::Result DoLoadResource(dmResource::HFactory factory, HRequest request, dmResource::LoadBufferType* buffer, LoadResult* result);
} // namespace

#endif // DM_RESOURCE_LOAD_QUEUE_PRIVATE_H
