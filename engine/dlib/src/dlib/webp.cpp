#include <assert.h>
#include "webp/webp/decode.h"
#include "webp.h"
#include <dlib/log.h>

namespace dmWebP
{
    Result DecodeRGB(const void* data, size_t data_size, void* output_buffer, size_t output_buffer_size, size_t output_stride)
    {
        uint8_t* decoded_data = WebPDecodeRGBInto((const uint8_t*) data, data_size, (uint8_t*) output_buffer, output_buffer_size, output_stride);
        if(decoded_data != output_buffer)
            return RESULT_MEM_ERROR;
        return RESULT_OK;
    }

    Result DecodeRGBA(const void* data, size_t data_size, void* output_buffer, size_t output_buffer_size, size_t output_stride)
    {
        uint8_t* decoded_data = WebPDecodeRGBAInto((const uint8_t*) data, data_size, (uint8_t*) output_buffer, output_buffer_size, output_stride);
        if(decoded_data != output_buffer)
            return RESULT_MEM_ERROR;
        return RESULT_OK;
    }

}
