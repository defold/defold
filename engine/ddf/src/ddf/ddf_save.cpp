// Copyright 2020-2025 The Defold Foundation
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

#include "ddf.h"
#include "ddf_util.h"
#include "ddf_save.h"
#include "ddf_outputstream.h"

namespace dmDDF
{

    static bool DDFCountSaveFunction(void* context, const void* buffer, uint32_t buffer_size)
    {
        uint32_t* count = (uint32_t*) context;
        *count = *count + buffer_size;
        return true;
    }

    Result DoSaveMessage(const void* message_, const Descriptor* desc, void* context, SaveFunction save_function)
    {
        OutputStream output_stream(save_function, context);
        const uint8_t* message = (const uint8_t*) message_;

        for (int i = 0; i < desc->m_FieldCount; ++i)
        {
            bool write_result;
            FieldDescriptor* field_desc = &desc->m_Fields[i];
            Type type = (Type) field_desc->m_Type;

            if (field_desc->m_OneOfIndex != DDF_NO_ONE_OF_INDEX)
            {
                // Resolve the oneof member for this field
                assert(field_desc->m_OneOfIndex > 0);
                uint32_t oneof_offset = desc->m_OneOfDataOffsets[field_desc->m_OneOfIndex-1];
                uint8_t oneof_member  = message[oneof_offset];
                if (oneof_member != field_desc->m_Number)
                {
                    continue;
                }
            }

    #define DDF_SAVEMESSAGE_CASE(t, wt, func) \
            write_result = output_stream.WriteTag(field_desc->m_Number, wt) && \
                           output_stream.func(*((t*) data)); \
            if (!write_result)\
                return RESULT_IO_ERROR; \
            break;

            int element_size = 0;
            if ( field_desc->m_Type == TYPE_MESSAGE )
            {
                element_size = field_desc->m_MessageDescriptor->m_Size;
            }
            else if ( field_desc->m_Type == TYPE_STRING )
            {
                element_size = sizeof(const char*);
            }
            else if ( field_desc->m_Type == TYPE_BYTES )
            {
                element_size = sizeof(RepeatedField);
            }
            else
            {
                element_size = ScalarTypeSize(type);
            }

            uint32_t count = 1;
            const uint8_t* data_start = &message[field_desc->m_Offset];
            if (field_desc->m_Label == LABEL_REPEATED)
            {
                RepeatedField* repeated = (RepeatedField*) data_start;
                count = repeated->m_ArrayCount;
                data_start = (const uint8_t *) repeated->m_Array;
            }

            for (uint32_t j = 0; j < count; ++j)
            {
                const uint8_t* data = data_start + j * element_size;

                switch (field_desc->m_Type)
                {
                    case TYPE_DOUBLE:
                        DDF_SAVEMESSAGE_CASE(double, WIRETYPE_FIXED64, WriteDouble);

                    case TYPE_FLOAT:
                        DDF_SAVEMESSAGE_CASE(float, WIRETYPE_FIXED32, WriteFloat);

                    case TYPE_INT64:
                        DDF_SAVEMESSAGE_CASE(int64_t, WIRETYPE_VARINT, WriteVarInt64);

                    case TYPE_UINT64:
                        DDF_SAVEMESSAGE_CASE(uint64_t, WIRETYPE_VARINT, WriteVarInt64);

                    case TYPE_INT32:
                        DDF_SAVEMESSAGE_CASE(int32_t, WIRETYPE_VARINT, WriteVarInt32SignExtended);

                    case TYPE_FIXED64:
                        assert(false);
                        break;

                    case TYPE_FIXED32:
                        assert(false);
                        break;

                    case TYPE_BOOL:
                        DDF_SAVEMESSAGE_CASE(bool, WIRETYPE_VARINT, WriteBool);

                    case TYPE_STRING:
                        DDF_SAVEMESSAGE_CASE(const char*, WIRETYPE_LENGTH_DELIMITED, WriteString);

                    case TYPE_GROUP:
                        assert(false);
                        break;

                    case TYPE_MESSAGE:
                    {
                        const uint8_t* data_tmp = data;

                        // The data stored in the buffer for "not fully defined" fields is a pointer to a
                        // separate dynamic memory area, which means that we need to resolve the pointer
                        // to where the actual data resides.
                        if (!field_desc->m_FullyDefinedType)
                        {
                            uintptr_t dynamic_ptr = *((uintptr_t*) data);
                            if (dynamic_ptr == 0)
                            {
                                break;
                            }
                            data = (const uint8_t*) dynamic_ptr;
                        }

                        uint32_t len = 0;
                        Result e = DoSaveMessage(data, field_desc->m_MessageDescriptor, &len, &DDFCountSaveFunction);
                        if (e != RESULT_OK)
                            return e;

                        write_result = output_stream.WriteTag(field_desc->m_Number, WIRETYPE_LENGTH_DELIMITED) && output_stream.WriteVarInt32(len);
                        if (!write_result)
                            return RESULT_IO_ERROR;

                        e = DoSaveMessage(data, field_desc->m_MessageDescriptor, context, save_function);

                        if (e != RESULT_OK)
                            return e;

                        data = data_tmp;
                    }
                    break;

                    case TYPE_BYTES:
                    {
                        RepeatedField* bytes_repeated = (RepeatedField*) data;
                        write_result = output_stream.WriteTag(field_desc->m_Number, WIRETYPE_LENGTH_DELIMITED) &&
                                       output_stream.WriteVarInt32(bytes_repeated->m_ArrayCount) &&
                                       output_stream.Write((const void*) bytes_repeated->m_Array, bytes_repeated->m_ArrayCount);
                        if (!write_result)
                            return RESULT_IO_ERROR;
                    }
                    break;

                    case TYPE_UINT32:
                        DDF_SAVEMESSAGE_CASE(uint32_t, WIRETYPE_VARINT, WriteVarInt32);

                    case TYPE_ENUM:
                        DDF_SAVEMESSAGE_CASE(uint32_t, WIRETYPE_VARINT, WriteVarInt32);
                        break;

                    case TYPE_SFIXED32:
                        assert(false);
                        break;

                    case TYPE_SFIXED64:
                        assert(false);
                        break;

                    case TYPE_SINT32:
                        assert(false);
                        break;

                    case TYPE_SINT64:
                        assert(false);
                        break;

                    default:
                        assert(false);
                }
            }
        }

    #undef DDF_SAVEMESSAGE_CASE

        return RESULT_OK;
    }
}
