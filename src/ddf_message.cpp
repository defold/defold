#include <string.h>
#include "ddf_message.h"
#include "ddf_load.h"
#include "ddf_util.h"

CDDFMessage::CDDFMessage(const SDDFDescriptor* message_descriptor, char* buffer, uint32_t buffer_size, bool dry_run)
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
            return DDF_ERROR_WIRE_FORMAT;                               \
        }                                                               \
        if (field->m_Label == DDF_LABEL_REPEATED)                       \
        {                                                               \
            AddScalar(field, (void*) &value, sizeof(CPP_TYPE));         \
        }                                                               \
        else                                                            \
        {                                                               \
            SetScalar(field, (void*) &value, sizeof(CPP_TYPE));         \
        }                                                               \
        return DDF_ERROR_OK;                                            \
    }                                                                   \

DDFError CDDFMessage::ReadScalarField(CDDFLoadContext* load_context,
                                      DDFWireType wire_type, 
                                      const SDDFFieldDescriptor* field, 
                                      CDDFInputBuffer* input_buffer)
{
    if (DDFWireTypeCorrespondence((DDFType) field->m_Type) != wire_type)
    {
        return DDF_ERROR_WIRE_FORMAT;
    }

    switch (field->m_Type)
    {
        READSCALARFIELD_CASE(DDF_TYPE_FLOAT, float, ReadFloat);
        READSCALARFIELD_CASE(DDF_TYPE_DOUBLE, double, ReadDouble);
        READSCALARFIELD_CASE(DDF_TYPE_INT32, int32_t, ReadInt32);
        READSCALARFIELD_CASE(DDF_TYPE_UINT32, uint32_t, ReadUInt32);
        READSCALARFIELD_CASE(DDF_TYPE_INT64, int64_t, ReadInt64);
        READSCALARFIELD_CASE(DDF_TYPE_UINT64, uint64_t, ReadUInt64);
        READSCALARFIELD_CASE(DDF_TYPE_ENUM, uint32_t, ReadUInt32);
            
    default:
        assert(0);
    }

    return DDF_ERROR_INTERNAL_ERROR;
}

#undef READSCALARFIELD_CASE

DDFError CDDFMessage::ReadStringField(CDDFLoadContext* load_context,
                                      DDFWireType wire_type, 
                                      const SDDFFieldDescriptor* field, 
                                      CDDFInputBuffer* input_buffer)
{
    if (wire_type != DDF_WIRETYPE_LENGTH_DELIMITED)
    {
        return DDF_ERROR_WIRE_FORMAT;
    }

    uint32_t length;
    if (!input_buffer->ReadVarInt32(&length))
    {
        return DDF_ERROR_WIRE_FORMAT;
    }
    
    const char* str_buf;
    if (input_buffer->Read(length, &str_buf))
    {
        if (field->m_Label == DDF_LABEL_REPEATED) 
        {
            AddString(load_context, field, str_buf, length);
        }
        else
        {
            SetString(load_context, field, str_buf, length);
        }
        return DDF_ERROR_OK;
    }
    else
    {
        return DDF_ERROR_WIRE_FORMAT;
    }
}

DDFError CDDFMessage::ReadMessageField(CDDFLoadContext* load_context,
                                       DDFWireType wire_type, 
                                       const SDDFFieldDescriptor* field, 
                                       CDDFInputBuffer* input_buffer)
{
    assert(field->m_MessageDescriptor);

    if (wire_type != DDF_WIRETYPE_LENGTH_DELIMITED)
    {
        return DDF_ERROR_WIRE_FORMAT;
    }

    uint32_t length;
    if (!input_buffer->ReadVarInt32(&length))
    {
        return DDF_ERROR_WIRE_FORMAT;
    }

    char* msg_buf = 0;
    if (field->m_Label == DDF_LABEL_REPEATED) 
    {
        msg_buf = (char*) AddMessage(field);
    }
    else
    {
        msg_buf = &m_Start[field->m_Offset];
        assert(msg_buf + field->m_MessageDescriptor->m_Size <= m_End);
    }
    CDDFMessage message(field->m_MessageDescriptor, (char*) msg_buf, field->m_MessageDescriptor->m_Size, m_DryRun);
    CDDFInputBuffer sub_buffer;
    if (!input_buffer->SubBuffer(length, &sub_buffer))
    {
        return DDF_ERROR_WIRE_FORMAT;
    }

    return DDFDoLoadMessage(load_context, &sub_buffer, field->m_MessageDescriptor, &message);
}

DDFError CDDFMessage::ReadField(CDDFLoadContext* load_context,
                                DDFWireType wire_type, 
                                const SDDFFieldDescriptor* field, 
                                CDDFInputBuffer* input_buffer)
{
    if ((DDFType) field->m_Type == DDF_TYPE_MESSAGE)
    {
        return ReadMessageField(load_context, wire_type, field, input_buffer);
    }
    else if ((DDFType) field->m_Type == DDF_TYPE_STRING)
    {
        return ReadStringField(load_context, wire_type, field, input_buffer);
    }
    else
    {
        // Assume scalar type
        return ReadScalarField(load_context, wire_type, field, input_buffer);
    }
}

void CDDFMessage::SetScalar(const SDDFFieldDescriptor* field, const void* buffer, int buffer_size)
{
    assert((DDFLabel) field->m_Label != DDF_LABEL_REPEATED);
    assert(field->m_MessageDescriptor == 0);

    assert(m_Start + field->m_Offset + buffer_size <= m_End);
    if (!m_DryRun)
    {
        memcpy(m_Start + field->m_Offset, buffer, buffer_size);
    }
}

void* CDDFMessage::AddScalar(const SDDFFieldDescriptor* field, const void* buffer, int buffer_size)
{
    assert((DDFLabel) field->m_Label == DDF_LABEL_REPEATED);
    assert(field->m_MessageDescriptor == 0);

    if (!m_DryRun)
    {
        DDFRepeatedField* repeated_field = (DDFRepeatedField*) &m_Start[field->m_Offset];
        uintptr_t dest = repeated_field->m_Array + repeated_field->m_ArrayCount * buffer_size;

        memcpy((void*) dest, buffer, buffer_size);
        repeated_field->m_ArrayCount++;

        return (void*) dest;
    }
    return 0;
}

void* CDDFMessage::AddMessage(const SDDFFieldDescriptor* field)
{
    assert((DDFLabel) field->m_Label == DDF_LABEL_REPEATED);
    assert(field->m_MessageDescriptor);

    if (!m_DryRun)
    {
        DDFRepeatedField* repeated_field = (DDFRepeatedField*) &m_Start[field->m_Offset];
        uintptr_t dest = repeated_field->m_Array + repeated_field->m_ArrayCount * field->m_MessageDescriptor->m_Size;

        memset((void*) dest, 0, field->m_MessageDescriptor->m_Size);
        repeated_field->m_ArrayCount++;

        return (void*) dest;
    }
    return 0;
}

void CDDFMessage::SetRepeatedBuffer(const SDDFFieldDescriptor* field, void* buffer)
{
    assert((DDFLabel) field->m_Label == DDF_LABEL_REPEATED);

    if (!m_DryRun)
    {
        DDFRepeatedField* repeated_field = (DDFRepeatedField*) &m_Start[field->m_Offset];
        repeated_field->m_Array = (uintptr_t) buffer;
        repeated_field->m_ArrayCount = 0;
    }
}

void CDDFMessage::SetString(CDDFLoadContext* load_context, const SDDFFieldDescriptor* field, const char* buffer, int buffer_len)
{
    assert((DDFType) field->m_Type == DDF_TYPE_STRING);

    // Always alloc
    char* str_buf = load_context->AllocString(buffer_len + 1);

    if (!m_DryRun)
    {
        const char** string_field = (const char**) &m_Start[field->m_Offset];        
        memcpy(str_buf, buffer, buffer_len);
        str_buf[buffer_len] = '\0';

        *string_field = str_buf;
    }
}

void CDDFMessage::AddString(CDDFLoadContext* load_context, const SDDFFieldDescriptor* field, const char* buffer, int buffer_len)
{
    assert((DDFLabel) field->m_Label == DDF_LABEL_REPEATED);
    assert(field->m_MessageDescriptor == 0);

    // Always alloc
    char* str_buf = load_context->AllocString(buffer_len + 1);

    if (!m_DryRun)
    {
        DDFRepeatedField* repeated_field = (DDFRepeatedField*) &m_Start[field->m_Offset];
        uintptr_t dest = repeated_field->m_Array + repeated_field->m_ArrayCount * sizeof(const char*);
        memcpy(str_buf, buffer, buffer_len);
        str_buf[buffer_len] = '\0';

        memcpy((void*) dest, &str_buf, sizeof(const char*));
        repeated_field->m_ArrayCount++;
    }
}

void CDDFMessage::AllocateRepeatedBuffer(CDDFLoadContext* load_context, const SDDFFieldDescriptor* field, int element_count)
{
    assert((DDFLabel) field->m_Label == DDF_LABEL_REPEATED);
    
    void* buf = load_context->AllocRepeated(field, element_count);
    SetRepeatedBuffer(field, buf);
}



