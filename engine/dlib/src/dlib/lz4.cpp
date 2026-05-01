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

#include "lz4.h"
#include "../lz4/lz4.h"
#include "../lz4/lz4hc.h"

namespace dmLZ4
{
    Result DecompressBuffer(const void* buffer, uint32_t buffer_size, void* decompressed_buffer, uint32_t max_output, int* decompressed_size)
    {
        Result r;
        if(max_output <= DMLZ4_MAX_OUTPUT_SIZE)
        {
            *decompressed_size = LZ4_decompress_safe((const char*)buffer, (char *)decompressed_buffer, buffer_size, max_output);
            if(*decompressed_size < 0)
                r = dmLZ4::RESULT_OUTBUFFER_TOO_SMALL;
            else
                r = dmLZ4::RESULT_OK;
        }
        else
        {
            *decompressed_size = -1;
            r = dmLZ4::RESULT_OUTPUT_SIZE_TOO_LARGE;
        }
        return r;
    }

    Result CompressBuffer(const void* buffer, uint32_t buffer_size, void *compressed_buffer, int *compressed_size)
    {
        *compressed_size = LZ4_compress_HC((const char *)buffer, (char *)compressed_buffer, buffer_size, LZ4_compressBound(buffer_size), 9);

        Result r;
        if(*compressed_size == 0)
            r = dmLZ4::RESULT_COMPRESSION_FAILED;
        else
            r = dmLZ4::RESULT_OK;

        return r;
    }

    Result MaxCompressedSize(int uncompressed_size, int *max_compressed_size)
    {
        *max_compressed_size = LZ4_compressBound(uncompressed_size);

        Result r;
        if(*max_compressed_size == 0)
            r = dmLZ4::RESULT_INPUT_SIZE_TOO_LARGE;
        else
            r = dmLZ4::RESULT_OK;

        return r;
    }
}

extern "C" {
    DM_DLLEXPORT int LZ4DecompressBuffer(const void* buffer, uint32_t buffer_size, void* decompressed_buffer, uint32_t max_output, int* decompressed_size)
    {
        return dmLZ4::DecompressBuffer(buffer, buffer_size, decompressed_buffer, max_output, decompressed_size);
    }

    DM_DLLEXPORT int LZ4CompressBuffer(const void* buffer, uint32_t buffer_size, void* compressed_buffer, int* compressed_size)
    {
        return dmLZ4::CompressBuffer(buffer, buffer_size, compressed_buffer, compressed_size);
    }

    DM_DLLEXPORT int LZ4MaxCompressedSize(int uncompressed_size, int* max_compressed_size)
    {
        return dmLZ4::MaxCompressedSize(uncompressed_size, max_compressed_size);
    }
}
