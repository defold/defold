// Copyright 2020-2024 The Defold Foundation
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

#ifndef DMSDK_DDF_H
#define DMSDK_DDF_H

#include <stdint.h>
#include <dmsdk/dlib/static_assert.h>
#include <dmsdk/dlib/array.h>
#include <dmsdk/dlib/hash.h>

namespace dmDDF
{
    /*# SDK DDF (Defold Data Format) API documentation
     * [file:<dmsdk/ddf/ddf.h>]
     *
     * @document
     * @name Ddf
     * @namespace dmDDF
     */

    /*# descriptor handle
     * Opaque pointer that holds info about a message type.
     * @typedef
     * @name Descriptor
     */
    struct Descriptor;

    /*#
     * Store pointers as offset from base address. Needed when serializing entire messages (copy). Value (1 << 0)
     * @constant
     * @name OPTION_OFFSET_POINTERS
     */
    const uint32_t OPTION_OFFSET_POINTERS = (1 << 0);

    /*# result enumeration
     * Result enumeration.
     *
     * @enum
     * @name Result
     * @member dmDDF::RESULT_OK = 0,
     * @member dmDDF::RESULT_FIELDTYPE_MISMATCH = 1,
     * @member dmDDF::RESULT_WIRE_FORMAT_ERROR = 2,
     * @member dmDDF::RESULT_IO_ERROR = 3,
     * @member dmDDF::RESULT_VERSION_MISMATCH = 4,
     * @member dmDDF::RESULT_MISSING_REQUIRED = 5,
     * @member dmDDF::RESULT_INTERNAL_ERROR = 1000,
     */
    enum Result
    {
        RESULT_OK = 0,
        RESULT_FIELDTYPE_MISMATCH = 1,
        RESULT_WIRE_FORMAT_ERROR = 2,
        RESULT_IO_ERROR = 3,
        RESULT_VERSION_MISMATCH = 4,
        RESULT_MISSING_REQUIRED = 5,
        RESULT_INTERNAL_ERROR = 1000,
    };

    /*#
     * Get Descriptor from hash name
     * @name GetDescriptorFromHash
     * @param hash [type:dmhash_t] hash of type name
     * @return descriptor [type:dmDDF::Descriptor*] 0 if not found
     */
    const Descriptor* GetDescriptorFromHash(dmhash_t hash);

    /*#
     * Load/decode a DDF message from buffer
     * @name LoadMessage
     * @param buffer [type:const void*] Input buffer
     * @param buffer_size [type:uint32_t] Input buffer size in bytes
     * @param desc [type:dmDDF::Descriptor*] DDF descriptor
     * @param message [type:void**] (out) Destination pointer to message
     * @return RESULT_OK on success
     */
    Result LoadMessage(const void* buffer, uint32_t buffer_size, const Descriptor* desc, void** message);

    /*#
     * Load/decode a DDF message from buffer
     * @name LoadMessage
     * @param buffer [type:const void*] Input buffer
     * @param buffer_size [type:uint32_t] Input buffer size in bytes
     * @param desc [type:dmDDF::Descriptor*] DDF descriptor
     * @param message [type:void**] (out) Destination pointer to message
     * @param options [type:uint32_t] options, eg dmDDF::OPTION_OFFSET_POINTERS
     * @param size [type:uint32_t*] (out) loaded message size
     * @return RESULT_OK on success
     */
    Result LoadMessage(const void* buffer, uint32_t buffer_size, const Descriptor* desc, void** message, uint32_t options, uint32_t* size);

    /*#
     * Save message to array
     * @name SaveMessageToArray
     * @param message [type:const void*] Message
     * @param desc [type:dmDDF::Descriptor*] DDF descriptor
     * @param buffer [type:dmArray<uint8_t>&] Buffer to save to
     * @return RESULT_OK on success
     */
    Result SaveMessageToArray(const void* message, const Descriptor* desc, dmArray<uint8_t>& array);

    /*#
     * Load/decode a DDF message from buffer. Template variant
     * @name LoadMessage<T>
     * @param buffer [type:const void*] Input buffer
     * @param buffer_size [type:uint32_t] Input buffer size in bytes
     * @param message [type:T**] (out) Destination pointer to message
     * @return RESULT_OK on success
     */
    template <typename T>
    Result LoadMessage(const void* buffer, uint32_t buffer_size, T** message)
    {
        return LoadMessage(buffer, buffer_size, T::m_DDFDescriptor, (void**) message);
    }

    /*#
     * Load/decode a DDF message from file
     * @name LoadMessageFromFile
     * @param file_name [type:const char*] File name
     * @param desc [type:dmDDF::Descriptor*] DDF descriptor
     * @param message [type:void**] (out) Destination pointer to message
     * @return RESULT_OK on success
     */
    Result LoadMessageFromFile(const char* file_name, const Descriptor* desc, void** message);

    /*#
     * If the message was loaded with the flag dmDDF::OPTION_OFFSET_POINTERS, all pointers have their offset stored.
     * This function resolves those offsets into actual pointers
     * @name ResolvePointers
     * @param desc [type:dmDDF::Descriptor*] DDF descriptor
     * @param message [type:void*] (int/out) The message to patch pointers in
     * @return RESULT_OK on success
     */
    Result ResolvePointers(const Descriptor* desc, void* message);

    /*#
     * Free message
     * @name FreeMessage
     * @param message [type:void*] The message
     */
    void FreeMessage(void* message);
}

#endif // DMSDK_DDF_H
