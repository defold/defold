#ifndef DMSDK_BUFFER
#define DMSDK_BUFFER

#include <stdint.h>

#include "hash.h"
#include "align.h"

namespace dmBuffer
{
    /*# SDK Buffer API documentation
     * [file:<dmsdk/dlib/buffer.h>]
     *
     * Buffer API for data buffers as the main way to communicate between systems.
     *
     * @document
     * @name Buffer
     * @namespace dmBuffer
     */

    /*# HBuffer type definition
     *
     * ```cpp
     * typedef struct Buffer* HBuffer;
     * ```
     *
     * @typedef
     * @name dmBuffer::HBuffer
     *
     */
    typedef uint32_t HBuffer;

    /*# valueType enumeration
     *
     * ValueType enumeration.
     *
     * @enum
     * @name ValueType
     * @member dmBuffer::VALUE_TYPE_UINT8
     * @member dmBuffer::VALUE_TYPE_UINT16
     * @member dmBuffer::VALUE_TYPE_UINT32
     * @member dmBuffer::VALUE_TYPE_UINT64
     * @member dmBuffer::VALUE_TYPE_INT8
     * @member dmBuffer::VALUE_TYPE_INT16
     * @member dmBuffer::VALUE_TYPE_INT32
     * @member dmBuffer::VALUE_TYPE_INT64
     * @member dmBuffer::VALUE_TYPE_FLOAT32
     * @member dmBuffer::VALUE_TYPE_FLOAT64
     * @member dmBuffer::MAX_VALUE_TYPE_COUNT
     *
     */
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

    /*# result enumeration
     *
     * Result enumeration.
     *
     * @enum
     * @name Result
     * @member dmBuffer::RESULT_OK
     * @member dmBuffer::RESULT_GUARD_INVALID
     * @member dmBuffer::RESULT_ALLOCATION_ERROR
     * @member dmBuffer::RESULT_BUFFER_INVALID
     * @member dmBuffer::RESULT_BUFFER_SIZE_ERROR
     * @member dmBuffer::RESULT_STREAM_SIZE_ERROR
     * @member dmBuffer::RESULT_STREAM_MISSING
     * @member dmBuffer::RESULT_STREAM_TYPE_MISMATCH
     * @member dmBuffer::RESULT_STREAM_COUNT_MISMATCH
     *
     */
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

    /*# StreamDeclaration struct
     *
     * Buffer stream declaration structure
     *
     * @struct
     * @name dmBuffer::StreamDeclaration
     * @member m_Name [type:dmhash_t] Hash of stream name
     * @member m_ValueType [type:dmBuffer::ValueType] Stream ValueType type
     * @member m_ValueCount [type:uint8_t] Value count of stream type
     * @examples
     *
     * Declare a typical position stream:
     *
     * ```cpp
     * const dmBuffer::StreamDeclaration streams_decl[] = {
     *     {dmHashString64("position"), dmBuffer::VALUE_TYPE_FLOAT32, 3}
     * };
     * ```
     */
    struct StreamDeclaration
    {
        dmhash_t  m_Name;
        ValueType m_ValueType;
        uint8_t   m_ValueCount;
    };

    /*# create Buffer
     *
     * Creates a new HBuffer with a number of different streams.
     *
     * @name dmBuffer::Create
     * @param num_elements [type:uint32_t]  The number of elements the buffer should hold
     * @param streams_decl [type:const dmBuffer::StreamDeclaration*] Array of stream declarations
     * @param streams_decl_count [type:uint8_t] Number of stream declarations inside the decl array (max 256)
     * @param out_buffer [type:dmBuffer::HBuffer*] Pointer to HBuffer where to store the newly allocated buffer
     * @return result [type:dmBuffer::Result] BUFFER_OK if buffer was allocated successfully
     * @examples
     *
     * ```cpp
     * const dmBuffer::StreamDeclaration streams_decl[] = {
     *     {dmHashString64("position"), dmBuffer::VALUE_TYPE_FLOAT32, 3},
     *     {dmHashString64("texcoord0"), dmBuffer::VALUE_TYPE_UINT16, 2},
     *     {dmHashString64("color"), dmBuffer::VALUE_TYPE_UINT8, 4},
     * };
     * dmBuffer::HBuffer buffer = 0x0;
     * dmBuffer::Result r = dmBuffer::Create(1024, streams_decl, 3, &buffer);
     *
     * if (r == dmBuffer::RESULT_OK) {
     *     // success
     * } else {
     *     // handle error
     * }
     * ```
     */
    Result Create(uint32_t num_elements, const StreamDeclaration* streams_decl, uint8_t streams_decl_count, HBuffer* out_buffer);

    /*# destroy Buffer.
     *
     * Destroys a HBuffer and it's streams.
     *
     * @name dmBuffer::Destroy
     * @param buffer [type:dmBuffer::HBuffer] Buffer handle to the buffer to free
     * @examples
     *
     * ```cpp
     * const dmBuffer::StreamDeclaration streams_decl[] = {
     *     {dmHashString64("position"), dmBuffer::VALUE_TYPE_FLOAT32, 3},
     * };
     * dmBuffer::HBuffer buffer = 0x0;
     * dmBuffer::Result r = dmBuffer::Create(4, streams_decl, 1, &buffer);
     *
     * if (r == dmBuffer::RESULT_OK) {
     *     dmBuffer::Destroy(buffer);
     * } else {
     *     // handle error
     * }
     * ```
     */
    void Destroy(HBuffer buffer);

    /*# check buffer handle
     * 
     * Checks if a handle is still valid
     * @name dmBuffer::IsBufferValid
     * @param buffer [type:dmBuffer::HBuffer] The buffer
     * @return result [type:bool] True if the handle is valid
     */
    bool IsBufferValid(HBuffer buffer);

    /*# validate buffer.
     *
     * Validate a buffer and it's streams.
     *
     * @name dmBuffer::ValidateBuffer
     * @param buffer [type:dmBuffer::HBuffer] Buffer handle to the buffer to validate
     * @examples
     *
     * ```cpp
     * // Pass buffer to third party library that does operations on the buffer or streams.
     * ThirdPartyLib::PerformOperation(buffer);
     *
     * r = dmBuffer::ValidateBuffer(buffer);
     * if (r == dmBuffer::RESULT_OK) {
     *     // buffer and streams are valid
     * } else {
     *     // the third party lib made the buffer invalid
     * }
     * ```
     */
    Result ValidateBuffer(HBuffer buffer);

    /*# get stream from buffer.
     *
     * Get a stream from a buffer. Output stream is 16 byte aligned.
     *
     * @name dmBuffer::GetStream
     * @param buffer [type:dmBuffer::HBuffer] buffer handle.
     * @param stream_name [type:dmhash_t] Hash of stream name to get
     * @param out_stream [type:void**] Pointer to void* where to store the stream
     * @param out_size [type:uint32_t*] Pointer to uint32_t where to store the size (in bytes)
     * @return result [type:dmBuffer::Result] BUFFER_OK if the stream was successfully accessed
     * @examples
     *
     * ```cpp
     * uint16_t* stream = 0x0;
     * uint32_t size = 0;
     * dmBuffer::Result r = dmBuffer::GetStream(buffer, dmHashString64("numbers"), (void**)&stream, &size);
     *
     * if (r == dmBuffer::RESULT_OK) {
     *     for (int i = 0; i < size / sizeof(stream[0]); ++i)
     *     {
     *         stream[i*2+0] = (uint16_t)i;
     *         stream[i*2+1] = (uint16_t)i;
     *     }
     * } else {
     *     // handle error
     * }
     * ```
     */
    Result GetStream(HBuffer buffer, dmhash_t stream_name, void** out_stream, uint32_t* out_bytes);


    /*# get buffer as a byte array.
     *
     * Gets the buffer as a byte array. If the buffer contains multiple streams, only the first one is returned
     *
     * @name dmBuffer::GetBytes
     * @param buffer [type:dmBuffer::HBuffer] buffer handle.
     * @param out_bytes [type:void**] Pointer to void* where to store the bytes
     * @param out_size [type:uint32_t*] Pointer to uint32_t where to store the array size
     * @return result [type:dmBuffer::Result] BUFFER_OK if the buffer was successfully accessed
     * @examples
     *
     * ```cpp
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
     * ```
     */
    Result GetBytes(HBuffer buffer, void** out_bytes, uint32_t* out_size);

    /*# get buffer element count.
     *
     * Get element count for a buffer.
     *
     * @name dmBuffer::GetElementCount
     * @param buffer [type:dmBuffer::HBuffer] buffer handle.
     * @param out_size [type:uint32_t*] Pointer to uint32_t where to store the element count
     * @return result [type:dmBuffer::Result] BUFFER_OK if the element count was successfully accessed
     * @examples
     *
     * ```cpp
     * uint32_t element_count = 0;
     * dmBuffer::Result r = dmBuffer::GetElementCount(buffer, &element_count);
     *
     * if (r == dmBuffer::RESULT_OK) {
     *     printf("buffer %p has %d number of elements", buffer, element_count);
     * } else {
     *     // handle error
     * }
     * ```
     */
    Result GetElementCount(HBuffer buffer, uint32_t* out_element_count);

    /*# get stream type and type count
     * 
     * Gets the stream type
     *
     * @name dmBuffer::GetStreamType
     * @param buffer [type:dmBuffer::HBuffer] Pointer to a buffer.
     * @param stream_name [type:dmhash_t] Hash of stream name to get
     * @param type [type:dmBuffer::ValueType] The value type
     * @param type_count [type:uint32_t] The number of values
     * @return result [type:dmBuffer::Result] Returns BUFFER_OK if all went ok
    */
    Result GetStreamType(HBuffer buffer, dmhash_t stream_name, dmBuffer::ValueType* type, uint32_t* type_count);

	/*# get the number of streams in a buffer
	 *
	 * Gets the number of streams in a buffer
	 * @name dmBuffer::GetNumStreams
	 * @param buffer [type:dmBuffer::HBuffer] The buffer
	 * @return count [type:uint32_t] The number of streams
	*/
	uint32_t GetNumStreams(HBuffer buffer);

	/*# get the hashed name of a stream
	 *
	 * Gets the hashed name of the stream
	 * @name dmBuffer::GetStreamName
	 * @param buffer [type:dmBuffer::HBuffer] The buffer
	 * @param index [type:uint32_t] The index of the stream
	 * @param stream_name [type:dmhash_t*] The out variable that receives the name
	 * @return result [type:dmBuffer::Result] RESULT_OK if the stream exists
	*/
	Result GetStreamName(HBuffer buffer, uint32_t index, dmhash_t* stream_name);

    /*# get size of a value type
     *
     * Gets the size of a value type
     *
     * @name dmBuffer::GetSizeForValueType
     * @param type [type:dmBuffer::ValueType] The value type
     * @return size [type:uint32_t] The size in bytes
    */
    uint32_t GetSizeForValueType(dmBuffer::ValueType type);

    /*# result to string
     * 
     * Converts result to string
     *
     * @name dmBuffer::GetResultString
     * @param result [type:dmBuffer::Result] The result
     * @return result [type:const char*] The result as a string
    */
    const char* GetResultString(Result result);

    /*# value type to string
     * 
     * Converts a value type to string
     *
     * @name dmBuffer::GetValueTypeString
     * @param result [type:dmBuffer::ValueType] The value type
     * @return result [type:const char*] The value type as a string
    */
    const char* GetValueTypeString(ValueType value);
}

#endif // DMSDK_BUFFER_H
