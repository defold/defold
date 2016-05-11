#ifndef DM_WEBP
#define DM_WEBP

#include <stdint.h>

namespace dmWebP
{
    /**
     * Result codes
     */
    enum Result
    {
        RESULT_OK            =  0,   //!< RESULT_OK
        RESULT_MEM_ERROR     = -1,   //!< RESULT_MEM_ERROR
        RESULT_BUF_ERROR     = -2,   //!< RESULT_BUF_ERROR
        RESULT_VERSION_ERROR = -3,   //!< RESULT_VERSION_ERROR
        RESULT_UNKNOWN       = -1000,//!< RESULT_UNKNOWN
    };

    /**
     * Decode WebP compressed data to RGB buffer
     * @param data input data to decode
     * @param data_size size of input data in bytes
     * @param output_buffer output buffer to write data to
     * @param output_buffer_size output output buffer size
     * @param output_buffer_stride distance (in bytes) between scanlines
     * @return RESULT_OK on success
     */
    Result DecodeRGB(const void* data, size_t data_size, void* output_buffer, size_t output_buffer_size, size_t output_buffer_stride);

    /**
     * Decode WebP compressed data to RGBA buffer
     * @param data input data to decode
     * @param data_size size of input data in bytes
     * @param output_buffer output buffer to write data to
     * @param output_buffer_size output output buffer size
     * @param output_buffer_stride distance (in bytes) between scanlines
     * @return RESULT_OK on success
     */
    Result DecodeRGBA(const void* data, size_t data_size, void* output_buffer, size_t output_buffer_size, size_t output_buffer_stride);

}


#endif // DM_WEBP
