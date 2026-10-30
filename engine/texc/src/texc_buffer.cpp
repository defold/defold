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


#include <dlib/array.h>
#include <dlib/math.h>
#include <dlib/zlib.h>

#include "texc.h"
#include "texc_private.h"

namespace dmTexc
{

    // https://en.wikipedia.org/wiki/Delta_encoding
    static void delta_encode(uint8_t* buffer, uint32_t length)
    {
        uint8_t last = 0;
        for (uint32_t i = 0; i < length; i++)
        {
            uint8_t current = buffer[i];
            buffer[i] = current - last;
            last = current;
        }
    }

    static bool DeflateWriter(void* context, const void* buffer, uint32_t buffer_size)
    {
        dmArray<uint8_t>* out = (dmArray<uint8_t>*) context;
        uint64_t required = (uint64_t)out->Size() + buffer_size;
        if (required > UINT32_MAX)
        {
            return false;
        }
        if (required > out->Capacity())
        {
            // Doubling keeps cumulative allocations and copies linear in the output size.
            uint64_t capacity = dmMath::Max(required, (uint64_t)out->Capacity() * 2);
            out->SetCapacity((uint32_t)dmMath::Min(capacity, (uint64_t)UINT32_MAX));
        }
        out->PushArray((const uint8_t*) buffer, buffer_size);
        return true;
    }

    static Buffer* CompressBuffer_Deflate(void* data, uint32_t size)
    {
        uint8_t* delta_encoded_data = size ? (uint8_t*)malloc(size) : 0;
        if (size)
        {
            if (!delta_encoded_data)
            {
                return 0;
            }
            memcpy(delta_encoded_data, data, size);
            delta_encode(delta_encoded_data, size);
        }

        dmArray<uint8_t> out_data_array;
        out_data_array.SetCapacity(32 * 1024);
        dmZlib::Result result = dmZlib::DeflateBuffer(delta_encoded_data, size, 9, &out_data_array, DeflateWriter);
        free(delta_encoded_data);
        if (result != dmZlib::RESULT_OK)
        {
            return 0;
        }
        size = out_data_array.Size();

        uint8_t* out_data = (uint8_t*)malloc(size);
        if (!out_data)
        {
            return 0;
        }
        memcpy(out_data, out_data_array.Begin(), size);

        Buffer* out = new Buffer;
        out->m_Data = out_data;
        out->m_IsCompressed = 1;
        out->m_DataCount = size;
        return out;
    }

    Buffer* CompressBuffer(uint8_t* data, uint32_t size)
    {
        Buffer* out = CompressBuffer_Deflate(data, size);
        return out;
    }

    void DestroyBuffer(Buffer* buffer)
    {
        free(buffer->m_Data);
        delete buffer;
    }
}
