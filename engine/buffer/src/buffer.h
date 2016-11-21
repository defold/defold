#ifndef DM_BUFFER
#define DM_BUFFER

#include <stdint.h>

#include <dlib/hash.h>
#include <dlib/align.h>

namespace dmBuffer
{
    typedef struct Buffer* HBuffer;

    enum ValueType
    {
        VALUE_TYPE_UINT8     = 0,
        VALUE_TYPE_UINT16    = 1,
        VALUE_TYPE_UINT32    = 2,
        VALUE_TYPE_UINT64    = 3,

        VALUE_TYPE_INT8      = 4,
        VALUE_TYPE_INT16     = 5,
        VALUE_TYPE_INT32     = 6,
        VALUE_TYPE_INT64     = 7,

        VALUE_TYPE_FLOAT32   = 8,
        VALUE_TYPE_FLOAT64   = 9,

        MAX_VALUE_TYPE_COUNT = 10,
    };

    enum Result
    {
        RESULT_OK,

        RESULT_GUARD_INVALID,

        RESULT_ALLOCATION_ERROR,

        RESULT_BUFFER_INVALID,
        RESULT_BUFFER_SIZE_ERROR,

        RESULT_STREAM_SIZE_ERROR,
        RESULT_STREAM_DOESNT_EXIST,
        RESULT_STREAM_WRONG_TYPE,
        RESULT_STREAM_WRONG_COUNT
    };

    struct StreamDeclaration
    {
        dmhash_t  m_Name;
        ValueType m_ValueType;
        uint8_t   m_ValueCount;
    };

    /**
     * Allocate a new HBuffer with a number of different streams.
     * @param num_elements The number of elements the buffer should hold
     * @param streams_decl Array of stream declarations
     * @param streams_decl_count Number of stream declarations inside the decl array (max 256)
     * @param out_buffer Pointer to HBuffer where to store the newly allocated buffer
     * @return Returns BUFFER_OK if buffer was allocated successfully
     */
    Result Allocate(uint32_t num_elements, const StreamDeclaration* streams_decl, uint8_t streams_decl_count, HBuffer* out_buffer);

    /**
     * Frees a HBuffer.
     * @param buffer Pointer to the buffer to free
     */
    void Free(HBuffer buffer);

    /**
     * Validate a buffer and its streams.
     * @param buffer Pointer to the buffer to validate
     * @return Returns BUFFER_OK if buffer is valid
     */
    Result ValidateBuffer(HBuffer buffer);

    /**
     * Get a stream from a buffer. Output stream is 16 byte aligned.
     * @param buffer Pointer to a buffer.
     * @param stream_name Hash of stream name to get
     * @param type Value type of expected stream
     * @param type_count Count of values per entry in stream
     * @param out_stream Pointer to void* where to store the stream
     * @param out_stride Pointer to uint32_t where to store the stride
     * @param out_element_count Pointer to uint32_t where to store the element count
     * @return Returns BUFFER_OK if the stream was successfully accessed
     */
    Result GetStream(HBuffer buffer, dmhash_t stream_name, ValueType type, uint32_t type_count, void** out_stream, uint32_t* out_stride, uint32_t* out_element_count);

}

#endif
