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
                uint32_t buffer_pos = input_buffer->Tell();
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
            if (f->m_Label == LABEL_REQUIRED && read_fields[i] == 0)
            {
                dmLogWarning("Missing required field %s.%s", desc->m_Name, f->m_Name);
                return RESULT_MISSING_REQUIRED;
            }
        }

        return RESULT_OK;
    }
}
