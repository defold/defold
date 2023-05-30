// Copyright 2020-2023 The Defold Foundation
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

#ifndef DM_DDF_H
#define DM_DDF_H

#include <stdint.h>
#include <dmsdk/ddf/ddf.h>
#include <dmsdk/dlib/array.h>
#include <dmsdk/dlib/hash.h>

#define DDF_OFFSET_OF(T, F) (((uintptr_t) (&((T*) 16)->F)) - 16)
#define DDF_MAX_FIELDS (128)
#define DDF_NO_ONE_OF_INDEX 0

namespace dmDDF
{
    struct Descriptor;

    struct EnumValueDescriptor
    {
        const char*     m_Name;
        int32_t         m_Value;
    };

    struct EnumDescriptor
    {
        uint16_t             m_MajorVersion;
        uint16_t             m_MinorVersion;
        const char*          m_Name;
        EnumValueDescriptor* m_EnumValues;
        uint8_t              m_EnumValueCount;
    };

    struct FieldDescriptor
    {
        const char* m_Name;
        uint32_t    m_Number : 22;
        uint32_t    m_Type : 6;
        uint32_t    m_Label : 4;
        Descriptor* m_MessageDescriptor;
        uint32_t    m_Offset;
        const char* m_DefaultValue;
        uint8_t     m_OneOfIndex : 7;
        uint8_t     m_OneOfSet   : 1;
    };

    struct Descriptor
    {
        uint16_t         m_MajorVersion;
        uint16_t         m_MinorVersion;
        const char*      m_Name;
        uint64_t         m_NameHash;
        uint32_t         m_Size;
        FieldDescriptor* m_Fields;
        uint8_t          m_FieldCount;  // TODO: Where to check < 255...?
        void*            m_NextDescriptor;
    };

    struct RepeatedField
    {
        uintptr_t m_Array;
        uint32_t  m_ArrayCount;
    };

    enum Label
    {
        LABEL_OPTIONAL = 1,
        LABEL_REQUIRED = 2,
        LABEL_REPEATED = 3,
    };

    enum Type
    {
        TYPE_DOUBLE         = 1,
        TYPE_FLOAT          = 2,
        TYPE_INT64          = 3,   // Not ZigZag encoded.  Negative numbers
        // take 10 bytes.  Use TYPE_SINT64 if negative
        // values are likely.
        TYPE_UINT64         = 4,
        TYPE_INT32          = 5,   // Not ZigZag encoded.  Negative numbers
        // take 10 bytes.  Use TYPE_SINT32 if negative
        // values are likely.
        TYPE_FIXED64        = 6,
        TYPE_FIXED32        = 7,
        TYPE_BOOL           = 8,
        TYPE_STRING         = 9,
        TYPE_GROUP          = 10,  // Tag-delimited aggregate.
        TYPE_MESSAGE        = 11,  // Length-delimited aggregate.

        // New in version 2.
        TYPE_BYTES          = 12,
        TYPE_UINT32         = 13,
        TYPE_ENUM           = 14,
        TYPE_SFIXED32       = 15,
        TYPE_SFIXED64       = 16,
        TYPE_SINT32         = 17,  // Uses ZigZag encoding.
        TYPE_SINT64         = 18,  // Uses ZigZag encoding.
    };

    enum WireType
    {
        WIRETYPE_VARINT           = 0,
        WIRETYPE_FIXED64          = 1,
        WIRETYPE_LENGTH_DELIMITED = 2,
        WIRETYPE_START_GROUP      = 3,
        WIRETYPE_END_GROUP        = 4,
        WIRETYPE_FIXED32          = 5,
    };

    /**
     * Internal. Do not use.
     */
    struct InternalRegisterDescriptor
    {
        InternalRegisterDescriptor(Descriptor* descriptor);
    };

    /**
     * Register all ddf-types available
     */
    void RegisterAllTypes();

    /**
     * Get Descriptor from name
     * @param name type name
     * @return Descriptor. NULL of not found
     */
    const Descriptor* GetDescriptor(const char* name);

    /**
     * Save message to file
     * @param message Message
     * @param desc DDF descriptor
     * @param file_name File name
     * @return RESULT_OK on success
     */
    Result SaveMessageToFile(const void* message, const Descriptor* desc, const char* file_name);

    /**
     * Save function call-back
     * @param context [type:void*] Save context
     * @param buffer [type:const void*] Buffer to write
     * @param buffer_size [type:uint32_t] Buffer size
     * @return true on success
     */
    typedef bool (*SaveFunction)(void* context, const void* buffer, uint32_t buffer_size);

    /**
     * Save message
     * @param message Message
     * @param desc DDF descriptor
     * @param context Save context
     * @param save_function Save function
     * @return RESULT_OK on success
     */
    Result SaveMessage(const void* message, const Descriptor* desc, void* context, SaveFunction save_function);

    /**
     * Calculates capacity needed for a message
     * @param message Message
     * @param desc DDF descriptor
     * @param size Returned size
     * @return RESULT_OK on success
     */
    Result SaveMessageSize(const void* message, const Descriptor* desc, uint32_t* size);

    /**
     * Deep copy message
     * Use dmDDF::FreeMessage to free the message
     */
    Result CopyMessage(const void* message, const dmDDF::Descriptor* desc, void** out);

    /**
     * Get enum value for name. NOTE: Using this function for undefined names is considered as a fatal run-time error.
     * @param desc Enum descriptor
     * @param name Enum name
     * @return Enum value
     */
    int32_t GetEnumValue(const EnumDescriptor* desc, const char* name);

    /**
     * Get <b>*first*</b> enum name for value.
     * @param desc Descriptor
     * @param value Value to get name for
     * @return Enum name. NULL if none found.
     */
    const char* GetEnumName(const EnumDescriptor* desc, int32_t value);
}

#endif // DM_DDF_H
