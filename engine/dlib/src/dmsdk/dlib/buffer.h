// Copyright 2020-2022 The Defold Foundation
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

#ifndef DMSDK_BUFFER
#define DMSDK_BUFFER

#include <stdint.h>

#include "hash.h"
#include "align.h"

namespace dmBuffer
{
    /*# SDK Buffer API documentation
     *
     * Buffer API for data buffers as the main way to communicate between systems.
     *
     * @document
     * @name Buffer
     * @namespace dmBuffer
     * @path engine/dlib/src/dmsdk/dlib/buffer.h
     */

    /*# HBuffer type definition
     *
     * ```cpp
     * typedef uint32_t HBuffer;
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
        MAX_VALUE_TYPE_COUNT = 9,
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
        RESULT_STREAM_MISMATCH,
        RESULT_METADATA_INVALID,
        RESULT_METADATA_NOT_EXIST,
    };

    /*# StreamDeclaration struct
     *
     * Buffer stream declaration structure
     *
     * @struct
     * @name dmBuffer::StreamDeclaration
     * @member m_Name [type:dmhash_t] Hash of stream name
     * @member m_Type [type:dmBuffer::ValueType] Stream ValueType type
     * @member m_Count [type:uint8_t] Component count of stream type. E.g. 3 for a Vector3
     * @member m_Flags [type:uint32_t] Flags for a stream.
     * @member m_Reserved [type:uint32_t] Reserved for future use.
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
        ValueType m_Type;
        uint8_t   m_Count;
        uint32_t  m_Flags;
        uint32_t  m_Reserved;
    };

    /*# create Buffer
     *
     * Creates a new HBuffer with a number of different streams.
     *
     * @name dmBuffer::Create
     * @param count [type:uint32_t] The number of "structs" the buffer should hold (e.g. vertex count)
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
    Result Create(uint32_t count, const StreamDeclaration* streams_decl, uint8_t streams_decl_count, HBuffer* out_buffer);

    /*# copy a Buffer
     *
     * Copies the data from one buffer to another buffer. The stream declaration needs to be the same in both buffers.
     *
     * @name dmBuffer::Copy
     * @param dst_buffer_handle [type:dmBuffer::HBuffer*] Pointer to HBuffer from where to copy buffer data.
     * @param src_buffer_handle [type:dmBuffer::HBuffer*] Pointer to HBuffer where to copy the buffer data.
     * @return result [type:dmBuffer::Result] BUFFER_OK if buffer was copied successfully
     * @examples
     *
     * ```cpp
     * dmBuffer::Result r = dmBuffer::Copy(buffer_a, buffer_b);
     *
     * if (r == dmBuffer::RESULT_OK) {
     *     // success
     * } else {
     *     // handle error
     * }
     * ```
     */
    Result Copy(const HBuffer dst_buffer_handle, const HBuffer src_buffer_handle);

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
     * @param stream [type:void**] Where to store the stream
     * @param count [type:uint32_t*] Where to store the count (e.g. vertex count). May be null.
     * @param components [type:uint32_t*] Where to store the number of components (e.g. 3 for a Vector3). May be null.
     * @param stride [type:uint32_t*] Where to store the (struct) stride. The stride can be added to the value pointer. May be null.
                    E.g. for a float array, the stride is (sizeof(Struct) / sizeof(float))
     * @return result [type:dmBuffer::Result] BUFFER_OK if the stream was successfully accessed
     * @examples
     *
     * ```cpp
     * float* positions = 0x0;
     * uint32_t size = 0;
     * uint32_t components = 0;
     * uint32_t stride = 0;
     * dmBuffer::Result r = dmBuffer::GetStream(buffer, dmHashString64("position"), (void**)&positions, &count, &components, &stride);
     *
     * if (r == dmBuffer::RESULT_OK) {
     *     for (int i = 0; i < count; ++i)
     *     {
     *         for (int c = 0; c < components; ++c)
     *         {
     *              positions[c] *= 1.1f;
     *         }
     *         positions += stride;
     *     }
     * } else {
     *     // handle error
     * }
     * ```
     */
    Result GetStream(HBuffer buffer, dmhash_t stream_name, void** stream, uint32_t* count, uint32_t* components, uint32_t* stride);


    /*# get buffer as a byte array.
     *
     * Gets the buffer as a byte array. If the buffer is interleaved (default), a pointer to the whole memory is returned.
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
     * dmBuffer::Result r = dmBuffer::GetBytes(buffer, (void**)&bytes, &size);
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

    /*# get buffer count.
     *
     * Get (struct) count for a buffer.
     *
     * @name dmBuffer::GetCount
     * @param buffer [type:dmBuffer::HBuffer] buffer handle.
     * @param count [type:uint32_t*] Pointer to uint32_t where to store the element count
     * @return result [type:dmBuffer::Result] BUFFER_OK if the element count was successfully accessed
     * @examples
     *
     * ```cpp
     * uint32_t count = 0;
     * dmBuffer::Result r = dmBuffer::GetCount(buffer, &count);
     *
     * if (r == dmBuffer::RESULT_OK) {
     *     printf("buffer %p has %d number of elements", buffer, count);
     * } else {
     *     // handle error
     * }
     * ```
     */
    Result GetCount(HBuffer buffer, uint32_t* count);

    /*# get stream type and type count
     *
     * Gets the stream type
     *
     * @name dmBuffer::GetStreamType
     * @param buffer [type:dmBuffer::HBuffer] Pointer to a buffer.
     * @param stream_name [type:dmhash_t] Hash of stream name to get
     * @param type [type:dmBuffer::ValueType*] The value type
     * @param components [type:uint32_t*] The number of values (E.g. 3 for a Vector3)
     * @return result [type:dmBuffer::Result] Returns BUFFER_OK if all went ok
     */
    Result GetStreamType(HBuffer buffer, dmhash_t stream_name, dmBuffer::ValueType* type, uint32_t* components);

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

    /*# Gets the current update number
     *
     * @name dmBuffer::GetContentVersion
     * @param type [type:dmBuffer::HBuffer] The value type
     * @param version [type:uint32_t*] The current version number
     * @return result [type:dmBuffer::Result] Returns BUFFER_OK if all went ok
     */
    Result GetContentVersion(HBuffer hbuffer, uint32_t* version);

    /*# Update the internal frame counter.
     * Used to know if a buffer has been updated.
     *
     * @name dmBuffer::UpdateContentVersion
     * @param type [type:dmBuffer::HBuffer] The value type
     * @return result [type:dmBuffer::Result] Returns BUFFER_OK if all went ok
     */
    Result UpdateContentVersion(HBuffer hbuffer);

    /*# set a metadata entry
     *
     * Create or update a new metadata entry with a number of values of a specific type.
     * It will allocate space to store these values.
     *
     * @name dmBuffer::SetMetaData
     * @param hbuffer [type:dmBuffer::HBuffer] A buffer handle
     * @param name_hash [type:dmhash_t] The entry name as a hash
     * @param data [type:void*] A pointer to an array of the values
     * @param count [type:uint32_t] Number of values in the array
     * @param type [type:dmBuffer::ValueType] The type of the values
     * @return result [type:dmBuffer::Result] RESULT_OK if the metadata entry was successfully stored
     */
    Result SetMetaData(HBuffer hbuffer, dmhash_t name_hash, const void* data, uint32_t count, ValueType type);

    /*# retrieve a metadata entry
     *
     * Retrieve metadata entry information
     *
     * @name dmBuffer::GetMetaData
     * @param hbuffer [type:dmBuffer::HBuffer] A buffer handle
     * @param name_hash [type:dmhash_t] The entry name as a hash
     * @param data [type:void**] Gets the internal address of metadata values
     * @param count [type:uint32_t] Gets the number of metadata values stored
     * @param type [type:dmBuffer::ValueType] Gets the type of values of the metadata
     */
    Result GetMetaData(HBuffer hbuffer, dmhash_t name_hash, void** data, uint32_t* count, ValueType* type);

}

#endif // DMSDK_BUFFER_H
