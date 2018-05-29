#ifndef DM_LZ4LIB
#define DM_LZ4LIB

#include "shared_library.h"

#include <stdint.h>

#define DMLZ4_MAX_OUTPUT_SIZE (1 << 30)

namespace dmLZ4
{
    /**
     * Result codes
     */
    enum Result
    {
        RESULT_OK                    = 0,   //!< RESULT_OK
        RESULT_COMPRESSION_FAILED    = 1,   //!< RESULT_COMPRESSION_FAILED
        RESULT_OUTBUFFER_TOO_SMALL   = 2,   //!< RESULT_OUTBUFFER_TOO_SMALL
        RESULT_INPUT_SIZE_TOO_LARGE  = 3,   //!< RESULT_INPUT_SIZE_TOO_LARGE
        RESULT_OUTPUT_SIZE_TOO_LARGE = 4,   //!< RESULT_OUTPUT_SIZE_TOO_LARGE
    };

    /**
     * Decompress buffer from LZ4-format (inflate)
     * Note that we do not use any framing of the compressed data, so the *complete* data to decompress must
     * be in the buffer when calling this method.
     *
     * @param buffer buffer to decompress
     * @param buffer_size buffer size
     * @param decompressed_buffer Pre-allocated buffer to decompress data into
     * @param max_output max size of decompressed data
     * @param decompressed_size Actual decompressed size will be written to this
     * @return dmLZ4::RESULT_OK on success
     */
    Result DecompressBuffer(const void* buffer, uint32_t buffer_size, void* decompressed_buffer, uint32_t max_output, int* decompressed_size);


    /**
     * Decompress buffer from LZ4-format (inflate). Use this function when you know the decompressed size beforehand.
     * Note that we do not use any framing of the compressed data, so the *complete* data to decompress must
     * be in the buffer when calling this method.
     *
     * @param buffer buffer to decompress
     * @param buffer_size buffer size
     * @param decompressed_buffer Pre-allocated buffer to decompress data into
     * @param decompressed_size size of decompressed data
     * @return dmLZ4::RESULT_OK on success
     */
    Result DecompressBufferFast(const void* buffer, uint32_t buffer_size, void* decompressed_buffer, uint32_t decompressed_size);

    /**
     * Compress buffer to LZ4-format (deflate)
     * Note that we do not use any framing of the compressed data, so the *complete* data to compress must
     * be in the buffer when calling this method.
     *
     * @param buffer buffer to compress
     * @param buffer_size buffer size
     * @param compressed_buffer Pre-allocated buffer to compress data into
     * @param compressed_size Actual compressed size will be written to this
     * @return dmLZ4::RESULT_OK on success
     */
    Result CompressBuffer(const void* buffer, uint32_t buffer_size, void* compressed_buffer, int* compressed_size);

    /**
     * Helper method to get a "worst case" size of compressed data.
     *
     * @param uncompressed_size Size of data to compress
     * @param max_compressed_size Max compressed size will be written to this
     * @return Result code is dmLZ4::RESULT_OK on success
     */
    Result MaxCompressedSize(int uncompressed_size, int* max_compressed_size);

}

#endif // DM_LZ4
