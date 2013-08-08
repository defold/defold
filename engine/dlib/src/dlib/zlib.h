#ifndef DM_ZLIB
#define DM_ZLIB

#include <stdint.h>

namespace dmZlib
{
    /**
     * Result codes
     */
    enum Result
    {
        RESULT_OK            =  0,   //!< RESULT_OK
        RESULT_STREAM_END    =  1,   //!< RESULT_STREAM_END
        RESULT_NEED_DICT     =  2,   //!< RESULT_NEED_DICT
        RESULT_ERRNO         = -1,   //!< RESULT_ERRNO
        RESULT_STREAM_ERROR  = -2,   //!< RESULT_STREAM_ERROR
        RESULT_DATA_ERROR    = -3,   //!< RESULT_DATA_ERROR
        RESULT_MEM_ERROR     = -4,   //!< RESULT_MEM_ERROR
        RESULT_BUF_ERROR     = -5,   //!< RESULT_BUF_ERROR
        RESULT_VERSION_ERROR = -6,   //!< RESULT_VERSION_ERROR

        RESULT_UNKNOWN       = -1000,//!< RESULT_UNKNOWN
    };

    /**
     * Write callback
     * @param context context
     * @param data data to write
     * @param data_len length to write
     * @return true on success
     */
    typedef bool (*Writer)(void* context, const void* data, uint32_t data_len);

    /**
     * Inflate (decompress) data. Both gzip and zlib format is supported and is
     * auto-detected
     * @param buffer buffer to inflate
     * @param buffer_size buffer size
     * @param context context
     * @param writer writer (inflated data)
     * @return RESULT_OK on success
     */
    Result InflateBuffer(const void* buffer, uint32_t buffer_size, void* context, Writer writer);

    /**
     * Deflate buffer into zlib-formast (compress)
     * @param buffer buffer to deflate (compress)
     * @param buffer_size buffer size
     * @param level compression level
     * @param context context
     * @param writer writer (compressed data)
     * @return RESULT_OK on success
     */
    Result DeflateBuffer(const void* buffer, uint32_t buffer_size, int level, void* context, Writer writer);
}


#endif // DM_ZLIB
