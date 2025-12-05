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

#include <string.h>
#include <dlib/log.h>
#include "ddf_load.h"
#include "ddf_util.h"

namespace dmDDF
{

    Result SkipField(InputBuffer* input_buffer, uint32_t type)
    {
        switch (type)
        {
        case WIRETYPE_VARINT:
        {
            uint64_t slask;
            if (input_buffer->ReadVarInt64(&slask))
                return RESULT_OK;
            else
                return RESULT_WIRE_FORMAT_ERROR;
        }
        break;
        case WIRETYPE_FIXED32:
        {
            uint32_t slask;
            if (input_buffer->ReadFixed32(&slask))
                return RESULT_OK;
            else
                return RESULT_WIRE_FORMAT_ERROR;
        }
        break;

        case WIRETYPE_FIXED64:
        {
            uint64_t slask;
            if (input_buffer->ReadFixed64(&slask))
                return RESULT_OK;
            else
                return RESULT_WIRE_FORMAT_ERROR;
        }
        break;

        case WIRETYPE_LENGTH_DELIMITED:
        {
            uint32_t length;
            if (!input_buffer->ReadVarInt32(&length))
                return RESULT_WIRE_FORMAT_ERROR;
            if (input_buffer->Skip(length))
                return RESULT_OK;
            else
                return RESULT_WIRE_FORMAT_ERROR;
        }
        break;

        default:
            return RESULT_WIRE_FORMAT_ERROR;
        }
    }

    static void DoLoadDefaultMessage(LoadContext* load_context, const Descriptor* desc, Message* message);

    static void DoLoadDefaultField(LoadContext* load_context, const FieldDescriptor* f, Message* message)
    {
        if (f->m_Label == LABEL_REPEATED)
        {
            // Nothing to read default
        }
        else if (f->m_Label == LABEL_REQUIRED)
        {
            // Try to load default but the field is required. Invalid definition
            dmLogWarning("Invalid message type. Required field (%s) in an optional message.", f->m_Name);
        }
        else if (f->m_Label == LABEL_OPTIONAL)
        {
            if (f->m_Type == TYPE_STRING && f->m_DefaultValue)
            {
                message->SetString(load_context, f, f->m_DefaultValue, strlen(f->m_DefaultValue));
            }
            else if (f->m_Type == TYPE_BYTES && f->m_DefaultValue)
            {
                dmLogWarning("Default values for 'bytes' is not supported");
            }
            else if (f->m_Type == TYPE_MESSAGE)
            {
                Message sub_message = message->SubMessage(f);
                DoLoadDefaultMessage(load_context, f->m_MessageDescriptor, &sub_message);
            }
            else if (f->m_DefaultValue)
            {
                // Assume scalar type
                message->SetScalar(f, f->m_DefaultValue, ScalarTypeSize(f->m_Type));
            }
        }
    }

    static bool HasDescriptorRecursive(const Descriptor* desc, const FieldDescriptor* f)
    {
        if (f->m_MessageDescriptor != 0x0)
        {
            if (f->m_MessageDescriptor == desc)
            {
                return true;
            }
            for (int i = 0; i < f->m_MessageDescriptor->m_FieldCount; ++i)
            {
                const FieldDescriptor* child_f = &f->m_MessageDescriptor->m_Fields[i];
                if (HasDescriptorRecursive(desc, child_f))
                {
                    return true;
                }
            }
        }
        return false;
    }

    static void DoLoadDefaultMessage(LoadContext* load_context, const Descriptor* desc, Message* message)
    {
        for (int i = 0; i < desc->m_FieldCount; ++i)
        {
            const FieldDescriptor* f = &desc->m_Fields[i];
            // We cannot support default values for oneof fields currently as we don't know which one to take
            if (f->m_OneOfIndex != DDF_NO_ONE_OF_INDEX)
            {
                dmLogWarning("Default values for 'oneof' fields are not supported for field %s.%s", desc->m_Name, f->m_Name);
                continue;
            }
            else if (HasDescriptorRecursive(desc, f))
            {
                dmLogWarning("Default values for field %s.%s is not supported, cyclic dependencies found", desc->m_Name, f->m_Name);
                continue;
            }

            DoLoadDefaultField(load_context, f, message);
        }
    }

    Result DoLoadMessage(LoadContext* load_context, InputBuffer* input_buffer,
                         const Descriptor* desc, Message* message)
    {
        uint8_t read_fields[DDF_MAX_FIELDS];
        memset(read_fields, 0, sizeof(read_fields));

        for (int i = 0; i < desc->m_FieldCount; ++i)
        {
            const FieldDescriptor* f = &desc->m_Fields[i];
            if (f->m_Label == LABEL_REPEATED)
            {
                // TODO: Verify buffer_pos!!!! Correct to use Tell()?
                uint32_t buffer_pos  = input_buffer->Tell();
                message->AllocateRepeatedBuffer(load_context, f, load_context->GetArrayCount(buffer_pos, f->m_Number));
            }
        }

        while (!input_buffer->Eof())
        {
            uint32_t tag;
            if (input_buffer->ReadVarInt32(&tag))
            {
                uint32_t key = tag >> 3;
                uint32_t type = tag & 0x7;

                if (key == 0)
                {
                    return RESULT_WIRE_FORMAT_ERROR;
                }

                uint32_t field_index;
                const FieldDescriptor* field = FindField(desc, key, &field_index);

                if (!field)
                {
                    // TODO: FIELD NOT FOUND. HOW TO HANDLE?!?!
                    Result e = SkipField(input_buffer, type);
                    if (e != RESULT_OK)
                    {
                        return e;
                    }
                    continue;
                }
                else
                {
                    assert(field_index < DDF_MAX_FIELDS);
                    read_fields[field_index] = 1;

                    Result e;
                    e = message->ReadField(load_context, (WireType) type, field, input_buffer);
                    if (e != RESULT_OK)
                    {
                        return e;
                    }

                    if (field->m_OneOfIndex != DDF_NO_ONE_OF_INDEX)
                    {
                        message->SetOneOf(desc, field);
                    }
                }
            }
            else
            {
                return RESULT_WIRE_FORMAT_ERROR;
            }
        }

        // Check for missing required fields
        for (int i = 0; i < desc->m_FieldCount; ++i)
        {
            const FieldDescriptor* f = &desc->m_Fields[i];
            if (f->m_OneOfIndex != DDF_NO_ONE_OF_INDEX)
            {
                continue;
            }
            else if (f->m_Label == LABEL_REQUIRED && read_fields[i] == 0)
            {
                // Required but not read
                dmLogWarning("Missing required field %s.%s", desc->m_Name, f->m_Name);
                return RESULT_MISSING_REQUIRED;
            }
            else if (f->m_Label == LABEL_OPTIONAL && read_fields[i] == 0)
            {
                DoLoadDefaultField(load_context, f, message);
            }
        }

        return RESULT_OK;
    }
}
