#ifndef DM_WEBP
#define DM_WEBP

#include <stddef.h>

namespace dmWebP
{
    /**
     * Result codes
     */
    enum Result
    {
        RESULT_OK            =  0,              //!< RESULT_OK
        RESULT_MEM_ERROR     = -1,              //!< RESULT_MEM_ERROR
        RESULT_BUF_ERROR     = -2,              //!< RESULT_BUF_ERROR
        RESULT_VERSION_ERROR = -3,              //!< RESULT_VERSION_ERROR
        RESULT_TEXTURE_DECODE_ERROR = -4,       //!< RESULT_TEXTURE_DECODE_ERROR
        RESULT_UNKNOWN       = -1000,           //!< RESULT_UNKNOWN
    };

    /**
     * TextureEncodeFormat type enumeration
     */
    enum TextureEncodeFormat
    {
        TEXTURE_ENCODE_FORMAT_PVRTC1   =  0,   //!< TEXTURE_ENCODE_FORMAT_PVRTC1
        TEXTURE_ENCODE_FORMAT_ETC1     =  1,   //!< TEXTURE_ENCODE_FORMAT_ETC1
        TEXTURE_ENCODE_FORMAT_L8       =  2,   //!< TEXTURE_ENCODE_FORMAT_L8
        TEXTURE_ENCODE_FORMAT_L8A8     =  3,   //!< TEXTURE_ENCODE_FORMAT_L8A8
        TEXTURE_ENCODE_FORMAT_RGB565   =  4,   //!< TEXTURE_ENCODE_FORMAT_RGB565
        TEXTURE_ENCODE_FORMAT_RGBA4444 =  5,   //!< TEXTURE_ENCODE_FORMAT_RGBA4444
    };

    /**
     * Decode WebP compressed RGB888 data to RGB888 buffer
     * @param data input data to decode
     * @param data_size size of input data in bytes
     * @param output_buffer output buffer to write data to
     * @param output_buffer_size output output buffer size
     * @param output_buffer_stride distance (in bytes) between scanlines
     * @return RESULT_OK on success
     */
    Result DecodeRGB(const void* data, size_t data_size, void* output_buffer, size_t output_buffer_size, size_t output_buffer_stride);

    /**
     * Decode WebP compressed RGBA8888 data to RGBA8888 buffer
     * @param data input data to decode
     * @param data_size size of input data in bytes
     * @param output_buffer output buffer to write data to
     * @param output_buffer_size output output buffer size
     * @param output_buffer_stride distance (in bytes) between scanlines
     * @return RESULT_OK on success
     */
    Result DecodeRGBA(const void* data, size_t data_size, void* output_buffer, size_t output_buffer_size, size_t output_buffer_stride);

    /**
     * Decode WebP compressed data of supported texture encoded format to buffer
     * This decodes internally envoded data of the supported formats to a buffer of native texture format
     * @param data input data to decode
     * @param data_size size of input data in bytes
     * @param output_buffer output buffer to write data to
     * @param output_buffer_size output output buffer size
     * @param output_buffer_stride distance (in bytes) between scanlines
     * @param format TextureEncodeFormat enumerated type
     * @return RESULT_OK on success
     */
    Result DecodeCompressedTexture(const void* data, size_t data_size, void* output_buffer, size_t output_buffer_size, size_t output_buffer_stride, TextureEncodeFormat format);

}


#endif // DM_WEBP
