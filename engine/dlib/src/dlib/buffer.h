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
        RESULT_STREAM_MISSING,
        RESULT_STREAM_TYPE_MISMATCH,
        RESULT_STREAM_COUNT_MISMATCH,
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
     * @examples
     * <pre>
     * const dmBuffer::StreamDeclaration streams_decl[] = {
     *     {dmHashString64("position"), dmBuffer::VALUE_TYPE_FLOAT32, 3},
     *     {dmHashString64("texcoord0"), dmBuffer::VALUE_TYPE_UINT16, 2},
     *     {dmHashString64("color"), dmBuffer::VALUE_TYPE_UINT8, 4},
     * };
     * dmBuffer::HBuffer buffer = 0x0;
     * dmBuffer::Result r = dmBuffer::Allocate(1024, streams_decl, 3, &buffer);
     *
     * if (r == dmBuffer::RESULT_OK) {
     *     // success
     * } else {
     *     // handle error
     * }
     * </pre>
     */
    Result Allocate(uint32_t num_elements, const StreamDeclaration* streams_decl, uint8_t streams_decl_count, HBuffer* out_buffer);

    /**
     * Frees a HBuffer.
     * @param buffer Pointer to the buffer to free
     * @examples
     * <pre>
     * const dmBuffer::StreamDeclaration streams_decl[] = {
     *     {dmHashString64("position"), dmBuffer::VALUE_TYPE_FLOAT32, 3},
     * };
     * dmBuffer::HBuffer buffer = 0x0;
     * dmBuffer::Result r = dmBuffer::Allocate(4, streams_decl, 1, &buffer);
     *
     * if (r == dmBuffer::RESULT_OK) {
     *     dmBuffer::Free(buffer);
     * } else {
     *     // handle error
     * }
     * </pre>
     */
    void Free(HBuffer buffer);

    /**
     * Validate a buffer and its streams.
     * @param buffer Pointer to the buffer to validate
     * @return Returns BUFFER_OK if buffer is valid
     * @examples
     * <pre>
     * // Pass buffer to third party library that does operations on the buffer or streams.
     * ThirdPartyLib::PerformOperation(buffer);
     *
     * r = dmBuffer::ValidateBuffer(buffer);
     * if (r == dmBuffer::RESULT_OK) {
     *     // buffer and streams are valid
     * } else {
     *     // the third party lib made the buffer invalid
     * }
     * </pre>
     */
    Result ValidateBuffer(HBuffer buffer);

    /**
     * Get a stream from a buffer. Output stream is 16 byte aligned.
     * @param buffer Pointer to a buffer.
     * @param stream_name Hash of stream name to get
     * @param type Value type of expected stream
     * @param type_count Count of values per entry in stream
     * @param out_stream Pointer to void* where to store the stream
     * @param out_stride Pointer to uint32_t where to store the stride, where stride is of unit sizeof(type)
     * @param out_element_count Pointer to uint32_t where to store the element count
     * @return Returns BUFFER_OK if the stream was successfully accessed
     * @examples
     * <pre>
     * uint16_t* stream = 0x0;
     * uint32_t stride = 0;
     * uint32_t element_count = 0;
     *
     * dmBuffer::Result r = dmBuffer::GetStream(buffer, dmHashString64("numbers"), dmBuffer::VALUE_TYPE_UINT16, 2, (void*)&stream, &stride, &element_count);
     *
     * if (r == dmBuffer::RESULT_OK) {
     *     for (int i = 0; i < element_count; ++i)
     *     {
     *         stream[0] = (uint16_t)i;
     *         stream[1] = (uint16_t)i;
     *         stream += stride;
     *     }
     * } else {
     *     // handle error
     * }
     * </pre>
     */
    Result GetStream(HBuffer buffer, dmhash_t stream_name, ValueType type, uint32_t type_count, void** out_stream, uint32_t* out_stride, uint32_t* out_element_count);

    /**
     * Gets the buffer as a byte array. If the buffer contains multiple streams, only the first one is returned
     * @param buffer Pointer to a buffer.
     * @param out_buffer Pointer to void* where to store the bytes
     * @param out_size Pointer to uint32_t where to store the array size
     * @return Returns BUFFER_OK if the buffer was successfully accessed
     * @examples
     * <pre>
     * uint8_t* bytes = 0x0;
     * uint32_t size = 0;
     *
     * dmBuffer::Result r = dmBuffer::GetBytes(buffer, (void*)&bytes, &size);
     *
     * if (r == dmBuffer::RESULT_OK) {
     *     for (int i = 0; i < size; ++i)
     *     {
     *         stream[i] = (uint8_t)(i & 0xFF);
     *     }
     * } else {
     *     // handle error
     * }
     * </pre>
     */
    Result GetBytes(HBuffer buffer, void** out_bytes, uint32_t* out_size);

    /**
     * Get element count for a buffer.
     * @param buffer Pointer to a buffer.
     * @param out_element_count Pointer to uint32_t where to store the element count
     * @return Returns BUFFER_OK if the element count was successfully accessed
     * @examples
     * <pre>

     * uint32_t element_count = 0;
     * dmBuffer::Result r = dmBuffer::GetElementCount(buffer, &element_count);
     *
     * if (r == dmBuffer::RESULT_OK) {
     *     printf("buffer %p has %d number of elemenets", buffer, element_count);
     * } else {
     *     // handle error
     * }
     * </pre>
     */
    Result GetElementCount(HBuffer buffer, uint32_t* out_element_count);

}

#endif
