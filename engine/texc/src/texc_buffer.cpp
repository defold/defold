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
        if (out->Remaining() < buffer_size) {
            int r = out->Remaining();
            int offset = dmMath::Max((int) buffer_size - r, 32 * 1024);
            out->OffsetCapacity(offset);
        }
        out->PushArray((const uint8_t*) buffer, buffer_size);
        return true;
    }

    static Buffer* CompressBuffer_Deflate(void* data, uint32_t size)
    {
        uint8_t* delta_encoded_data = (uint8_t*)malloc(size);
        memcpy(delta_encoded_data, data, size);
        delta_encode(delta_encoded_data, size);
        data = delta_encoded_data;

        dmArray<uint8_t> out_data_array;
        out_data_array.SetCapacity(32 * 1024);
        dmZlib::DeflateBuffer(data, size, 9, &out_data_array, DeflateWriter);
        size = out_data_array.Size();

        uint8_t* out_data = (uint8_t*)malloc(size);
        memcpy(out_data, &out_data_array[0], size);

        free(delta_encoded_data);

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
