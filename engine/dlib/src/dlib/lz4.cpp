#include "lz4.h"

#include "../lz4/lz4.h"
#include "../lz4/lz4hc.h"

namespace dmLZ4
{
    Result DecompressBuffer(const void* buffer, uint32_t buffer_size, void* decompressed_buffer, uint32_t max_output, int *decompressed_size)
    {
        Result r;
        // When we set an output size larger than 1G (or closer to 2G) lz4 fails for some reason...
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

    Result DecompressBufferFast(const void* buffer, uint32_t buffer_size, void* decompressed_buffer, uint32_t decompressed_size)
    {
        Result r;
        // When we set an output size larger than 1G (or closer to 2G) lz4 fails for some reason...
        if(decompressed_size <= DMLZ4_MAX_OUTPUT_SIZE)
        {
            int result = LZ4_decompress_fast((const char*)buffer, (char *)decompressed_buffer, decompressed_size);
            if(result < 0)
                r = dmLZ4::RESULT_OUTBUFFER_TOO_SMALL;
            else
                r = dmLZ4::RESULT_OK;
        }
        else
        {
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
