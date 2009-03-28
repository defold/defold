#include "ddf_load.h"
#include "ddf_util.h"

DDFError DDFSkipField(CDDFInputBuffer* input_buffer, uint32_t type)
{
    switch (type)
    {
    case DDF_WIRETYPE_VARINT:
    {
        uint64_t slask;
        if (input_buffer->ReadVarInt64(&slask))
            return DDF_ERROR_OK;
        else
            return DDF_ERROR_WIRE_FORMAT;
    }
    break;
    case DDF_WIRETYPE_FIXED32:
    {
        uint32_t slask;
        if (input_buffer->ReadFixed32(&slask))
            return DDF_ERROR_OK;
        else
            return DDF_ERROR_WIRE_FORMAT;
    }
    break;

    case DDF_WIRETYPE_FIXED64:
    {
        uint64_t slask;
        if (input_buffer->ReadFixed64(&slask))
            return DDF_ERROR_OK;
        else
            return DDF_ERROR_WIRE_FORMAT;
    }
    break;

    case DDF_WIRETYPE_LENGTH_DELIMITED:
    {
        uint32_t length;
        if (!input_buffer->ReadVarInt32(&length))
            return DDF_ERROR_WIRE_FORMAT;
        if (input_buffer->Skip(length))
            return DDF_ERROR_OK;
        else
            return DDF_ERROR_WIRE_FORMAT;
    }
    break;

    default:
        return DDF_ERROR_WIRE_FORMAT;
    }
}

DDFError DDFDoLoadMessage(CDDFLoadContext* load_context, CDDFInputBuffer* input_buffer, 
                          const SDDFDescriptor* desc, CDDFMessage* message)
{

    for (int i = 0; i < desc->m_FieldCount; ++i)
    {
        const SDDFFieldDescriptor* f = &desc->m_Fields[i];
        if (f->m_Label == DDF_LABEL_REPEATED)
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
                return DDF_ERROR_WIRE_FORMAT;
            }
            
            const SDDFFieldDescriptor* field = DDFFindField(desc, key);
            
            if (!field)
            {
                // TODO: FIELD NOT FOUND. HOW TO HANDLE?!?!
                DDFError e = DDFSkipField(input_buffer, type);
                if (e != DDF_ERROR_OK)
                {
                    return e;
                }
                continue;
            }
            else
            {
                DDFError e;
                e = message->ReadField(load_context, (DDFWireType) type, field, input_buffer);
                if (e != DDF_ERROR_OK)
                {
                    return e;
                }
            }
        }
        else
        {
            return DDF_ERROR_WIRE_FORMAT;
        }
    }

    return DDF_ERROR_OK;
}
