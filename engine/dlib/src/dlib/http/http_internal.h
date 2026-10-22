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

#ifndef DM_HTTP_INTERNAL_H
#define DM_HTTP_INTERNAL_H

#include <stdint.h>

#include <dmsdk/dlib/http.h>

#ifdef __cplusplus
#include <dlib/array.h>
#include <dlib/math.h>

namespace dmHttpService
{
    inline uint32_t GetResponseBufferCapacity(uint32_t capacity, uint64_t required_capacity)
    {
        assert(required_capacity <= UINT32_MAX);
        // Double capacity to keep cumulative allocation and copying linear in the response size.
        uint64_t grown_capacity = dmMath::Max((uint64_t)capacity * 2, (uint64_t)1024);
        return (uint32_t)dmMath::Min(dmMath::Max(required_capacity, grown_capacity), (uint64_t)UINT32_MAX);
    }

    inline void AppendResponseBuffer(dmArray<char>& buffer, const void* data, uint32_t data_size)
    {
        if (!data || data_size == 0)
        {
            return;
        }

        uint32_t left = buffer.Capacity() - buffer.Size();
        if (left < data_size)
        {
            buffer.SetCapacity(GetResponseBufferCapacity(buffer.Capacity(), (uint64_t)buffer.Size() + data_size));
        }
        buffer.PushArray((const char*)data, data_size);
    }
}

extern "C"
{
#endif

    HttpResult HttpNewServiceInternal(uint32_t max_concurrent_requests, HttpService** service);
    HttpResult HttpNewServiceWithCacheInternal(uint32_t max_concurrent_requests, void* http_cache, HttpService** service);
    void       HttpDeleteServiceInternal(HttpService* service);

#ifdef __cplusplus
}
#endif

#endif // DM_HTTP_INTERNAL_H
