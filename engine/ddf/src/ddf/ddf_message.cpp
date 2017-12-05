#include <string.h>
#include "ddf_message.h"
#include "ddf_load.h"
#include "ddf_util.h"

namespace dmDDF
{

    Message::Message(const Descriptor* message_descriptor, char* buffer, uint32_t buffer_size, bool dry_run)
    {
        m_MessageDescriptor = message_descriptor;
        m_Start = buffer;
        m_End = buffer + buffer_size;
        m_DryRun = dry_run;
    }

    #define READSCALARFIELD_CASE(DDF_TYPE, CPP_TYPE, READ_METHOD) \
        case DDF_TYPE:                                                      \
        {                                                                   \
            CPP_TYPE value;                                                 \
            if (!input_buffer->READ_METHOD(&value))                         \
            {                                                               \
                return RESULT_WIRE_FORMAT_ERROR;                            \
            }                                                               \
            if (field->m_Label == LABEL_REPEATED)                       \
            {                                                               \
                AddScalar(field, (void*) &value, sizeof(CPP_TYPE));         \
            }                                                               \
            else                                                            \
            {                                                               \
                SetScalar(field, (void*) &value, sizeof(CPP_TYPE));         \
            }                                                               \
            return RESULT_OK;                                               \
        }                                                                   \

    Result Message::ReadScalarField(LoadContext* load_context,
                                          WireType wire_type,
                                          const FieldDescriptor* field,
                                          InputBuffer* input_buffer)
    {
        if (WireTypeCorrespondence((Type) field->m_Type) != wire_type)
        {
            return RESULT_WIRE_FORMAT_ERROR;
        }

        switch (field->m_Type)
        {
            READSCALARFIELD_CASE(TYPE_FLOAT, float, ReadFloat);
            READSCALARFIELD_CASE(TYPE_DOUBLE, double, ReadDouble);
            READSCALARFIELD_CASE(TYPE_INT32, int32_t, ReadInt32);
            READSCALARFIELD_CASE(TYPE_UINT32, uint32_t, ReadUInt32);
            READSCALARFIELD_CASE(TYPE_INT64, int64_t, ReadInt64);
            READSCALARFIELD_CASE(TYPE_UINT64, uint64_t, ReadUInt64);
            READSCALARFIELD_CASE(TYPE_ENUM, uint32_t, ReadUInt32);
            READSCALARFIELD_CASE(TYPE_BOOL, bool, ReadBool);

        default:
            assert(0);
        }

        return RESULT_INTERNAL_ERROR;
    }

    #undef READSCALARFIELD_CASE

    Result Message::ReadStringField(LoadContext* load_context,
                                          WireType wire_type,
                                          const FieldDescriptor* field,
                                          InputBuffer* input_buffer)
    {
        if (wire_type != WIRETYPE_LENGTH_DELIMITED)
        {
            return RESULT_WIRE_FORMAT_ERROR;
        }

        uint32_t length;
        if (!input_buffer->ReadVarInt32(&length))
        {
            return RESULT_WIRE_FORMAT_ERROR;
        }

        const char* str_buf;
        if (input_buffer->Read(length, &str_buf))
        {
            if (field->m_Label == LABEL_REPEATED)
            {
                AddString(load_context, field, str_buf, length);
            }
            else
            {
                SetString(load_context, field, str_buf, length);
            }
            return RESULT_OK;
        }
        else
        {
            return RESULT_WIRE_FORMAT_ERROR;
        }
    }

    Result Message::ReadBytesField(LoadContext* load_context,
                                         WireType wire_type,
                                         const FieldDescriptor* field,
                                         InputBuffer* input_buffer)
    {
        if (wire_type != WIRETYPE_LENGTH_DELIMITED)
        {
            return RESULT_WIRE_FORMAT_ERROR;
        }

        uint32_t length;
        if (!input_buffer->ReadVarInt32(&length))
        {
            return RESULT_WIRE_FORMAT_ERROR;
        }

        const char* str_buf;
        if (input_buffer->Read(length, &str_buf))
        {
            assert (field->m_Label != LABEL_REPEATED);
            SetBytes(load_context, field, str_buf, length);
            return RESULT_OK;
        }
        else
        {
            return RESULT_WIRE_FORMAT_ERROR;
        }
    }

    Result Message::ReadMessageField(LoadContext* load_context,
                                           WireType wire_type,
                                           const FieldDescriptor* field,
                                           InputBuffer* input_buffer)
    {
        assert(field->m_MessageDescriptor);

        if (wire_type != WIRETYPE_LENGTH_DELIMITED)
        {
            return RESULT_WIRE_FORMAT_ERROR;
        }

        uint32_t length;
        if (!input_buffer->ReadVarInt32(&length))
        {
            return RESULT_WIRE_FORMAT_ERROR;
        }

        char* msg_buf = 0;
        if (field->m_Label == LABEL_REPEATED)
        {
            msg_buf = (char*) AddMessage(field);
        }
        else
        {
            msg_buf = &m_Start[field->m_Offset];
            assert(msg_buf + field->m_MessageDescriptor->m_Size <= m_End);
        }
        Message message(field->m_MessageDescriptor, (char*) msg_buf, field->m_MessageDescriptor->m_Size, m_DryRun);
        InputBuffer sub_buffer;
        if (!input_buffer->SubBuffer(length, &sub_buffer))
        {
            return RESULT_WIRE_FORMAT_ERROR;
        }

        return DoLoadMessage(load_context, &sub_buffer, field->m_MessageDescriptor, &message);
    }

    Message Message::SubMessage(const FieldDescriptor* field)
    {
        assert(field->m_MessageDescriptor != 0);

#ifndef NDEBUG
        bool found = false;
        for (uint32_t i = 0; i < m_MessageDescriptor->m_FieldCount; ++i)
        {
            if (&m_MessageDescriptor->m_Fields[i] == field)
            {
                found = true;
                break;
            }
        }
        assert(found);
#endif
        char* msg_buf = &m_Start[field->m_Offset];
        return Message(field->m_MessageDescriptor, (char*) msg_buf, field->m_MessageDescriptor->m_Size, m_DryRun);
    }

    Result Message::ReadField(LoadContext* load_context,
                              WireType wire_type,
                              const FieldDescriptor* field,
                              InputBuffer* input_buffer)
    {
        if ((Type) field->m_Type == TYPE_MESSAGE)
        {
            return ReadMessageField(load_context, wire_type, field, input_buffer);
        }
        else if ((Type) field->m_Type == TYPE_STRING)
        {
            return ReadStringField(load_context, wire_type, field, input_buffer);
        }
        else if ((Type) field->m_Type == TYPE_BYTES)
        {
            return ReadBytesField(load_context, wire_type, field, input_buffer);
        }
        else
        {
            // Assume scalar type
            return ReadScalarField(load_context, wire_type, field, input_buffer);
        }
    }

    void Message::SetScalar(const FieldDescriptor* field, const void* buffer, int buffer_size)
    {
        assert((Label) field->m_Label != LABEL_REPEATED);
        assert(field->m_MessageDescriptor == 0);

        assert(m_Start + field->m_Offset + buffer_size <= m_End);
        if (!m_DryRun)
        {
            memcpy(m_Start + field->m_Offset, buffer, buffer_size);
        }
    }

    void* Message::AddScalar(const FieldDescriptor* field, const void* buffer, int buffer_size)
    {
        assert((Label) field->m_Label == LABEL_REPEATED);
        assert(field->m_MessageDescriptor == 0);

        if (!m_DryRun)
        {
            RepeatedField* repeated_field = (RepeatedField*) &m_Start[field->m_Offset];
            uintptr_t dest = repeated_field->m_Array + repeated_field->m_ArrayCount * buffer_size;

            memcpy((void*) dest, buffer, buffer_size);
            repeated_field->m_ArrayCount++;

            return (void*) dest;
        }
        return 0;
    }

    void* Message::AddMessage(const FieldDescriptor* field)
    {
        assert((Label) field->m_Label == LABEL_REPEATED);
        assert(field->m_MessageDescriptor);

        if (!m_DryRun)
        {
            RepeatedField* repeated_field = (RepeatedField*) &m_Start[field->m_Offset];
            uintptr_t dest = repeated_field->m_Array + repeated_field->m_ArrayCount * field->m_MessageDescriptor->m_Size;

            memset((void*) dest, 0, field->m_MessageDescriptor->m_Size);
            repeated_field->m_ArrayCount++;

            return (void*) dest;
        }
        return 0;
    }

    void Message::SetRepeatedBuffer(const FieldDescriptor* field, void* buffer)
    {
        assert((Label) field->m_Label == LABEL_REPEATED);

        if (!m_DryRun)
        {
            RepeatedField* repeated_field = (RepeatedField*) &m_Start[field->m_Offset];
            repeated_field->m_Array = (uintptr_t) buffer;
            repeated_field->m_ArrayCount = 0;
        }
    }

    void Message::SetString(LoadContext* load_context, const FieldDescriptor* field, const char* buffer, int buffer_len)
    {
        assert((Type) field->m_Type == TYPE_STRING);

        // Always alloc
        char* str_buf = load_context->AllocString(buffer_len + 1);

        if (!m_DryRun)
        {
            const char** string_field = (const char**) &m_Start[field->m_Offset];
            memcpy(str_buf, buffer, buffer_len);
            str_buf[buffer_len] = '\0';

            if (load_context->GetOptions() & OPTION_OFFSET_POINTERS)
            {
                *string_field = (char*)(uintptr_t) load_context->GetOffset(str_buf);
            }
            else
            {
                *string_field = str_buf;
            }
        }
    }

    void Message::AddString(LoadContext* load_context, const FieldDescriptor* field, const char* buffer, int buffer_len)
    {
        assert((Label) field->m_Label == LABEL_REPEATED);
        assert(field->m_MessageDescriptor == 0);

        // Always alloc
        char* str_buf = load_context->AllocString(buffer_len + 1);

        if (!m_DryRun)
        {
            RepeatedField* repeated_field = (RepeatedField*) &m_Start[field->m_Offset];
            uintptr_t array = (uintptr_t)repeated_field->m_Array;
            if (load_context->GetOptions() & OPTION_OFFSET_POINTERS )
            {
                if (repeated_field->m_ArrayCount == 0) {
                    repeated_field->m_Array = (uintptr_t) load_context->GetOffset((void*)repeated_field->m_Array);
                }
                array = (uintptr_t)load_context->GetPointer(repeated_field->m_Array);
            }

            memcpy(str_buf, buffer, buffer_len);
            str_buf[buffer_len] = '\0';

            uintptr_t dest = array + repeated_field->m_ArrayCount * sizeof(const char*);
            if (load_context->GetOptions() & OPTION_OFFSET_POINTERS)
            {
                const char* offset = (const char*)(uintptr_t) load_context->GetOffset(str_buf);
                memcpy((void*) dest, &offset, sizeof(const char*));
            }
            else
            {
                memcpy((void*) dest, &str_buf, sizeof(const char*));
            }
            repeated_field->m_ArrayCount++;
        }
    }

    void Message::SetBytes(LoadContext* load_context, const FieldDescriptor* field, const char* buffer, int buffer_len)
    {
        assert((Type) field->m_Type == TYPE_BYTES);

        // Always alloc
        char* bytes_buf = load_context->AllocBytes(buffer_len);

        if (!m_DryRun)
        {
            memcpy(bytes_buf, buffer, buffer_len);

            RepeatedField* repeated_field = (RepeatedField*) &m_Start[field->m_Offset];
            assert(repeated_field->m_ArrayCount == 0);

            if (load_context->GetOptions() & OPTION_OFFSET_POINTERS)
            {
                repeated_field->m_Array = (uintptr_t) load_context->GetOffset(bytes_buf);
            }
            else
            {
                repeated_field->m_Array = (uintptr_t) bytes_buf;
            }
            repeated_field->m_ArrayCount = buffer_len;
        }
    }

    void Message::AllocateRepeatedBuffer(LoadContext* load_context, const FieldDescriptor* field, int element_count)
    {
        assert((Label) field->m_Label == LABEL_REPEATED);

        void* buf = load_context->AllocRepeated(field, element_count);
        SetRepeatedBuffer(field, buf);
    }

    Result DoResolvePointers(const Descriptor* desc, void* message)
    {
        for (int i = 0; i < desc->m_FieldCount; ++i)
        {
            const FieldDescriptor* field = &desc->m_Fields[i];
            void* fieldptr = (void*)((uintptr_t)message + field->m_Offset);
            if ((Type) field->m_Type == TYPE_MESSAGE)
            {
                Result r = DoResolvePointers(field->m_MessageDescriptor, fieldptr);
                if (r != RESULT_OK) {
                    return r;
                }
            }
            else if ((Type) field->m_Type == TYPE_STRING)
            {
                if (field->m_Label == LABEL_REPEATED) {
                    RepeatedField* repeated_field = (RepeatedField*) fieldptr;
                    repeated_field->m_Array = ((uintptr_t)message + repeated_field->m_Array);
                    char** strings = (char**)repeated_field->m_Array;
                    for (uint32_t i = 0; i < repeated_field->m_ArrayCount; ++i, ++strings)
                    {
                        uintptr_t offset = *(uintptr_t*)strings;
                        *strings = (char*)message + offset;
                    }
                }
                else
                {
                    const char** string_field = (const char**)fieldptr;
                    uintptr_t offset = *(uintptr_t*)fieldptr;
                    *string_field = (const char*)message + offset;
                }
            }
            else if ((Type) field->m_Type == TYPE_BYTES)
            {
                const uint8_t** bytes_field = (const uint8_t**)fieldptr;
                uintptr_t offset = *(uintptr_t*)fieldptr;
                *bytes_field = (const uint8_t*)message + offset;
            }

        }
        return RESULT_OK;
    }
}
