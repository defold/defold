#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>

#include "ddf.h"
#include "ddf_protocol.h"
#include "ddf_inputstream.h"
#include "private/ddf_private.h"

enum DDFPass
{
    DDF_PASS_PROCESS_REPEATED = 0,
    DDF_PASS_LOAD_MESSAGE = 1,
    DDF_PASS_LOAD_REPEATED = 2,
};

static inline uint32_t DDFAlign(uint32_t index, uint32_t align)
{
    index += (align - 1);
    index &= ~(align - 1);
    return index;
}

static const SDDFFieldDescriptor* DDFFindField(const SDDFDescriptor* desc, uint32_t key)
{
    for (int i = 0; i < desc->m_FieldCount; ++i)
    {
        const SDDFFieldDescriptor* f = &desc->m_Fields[i];
        if (f->m_Number == key)
            return f;
    }
    return 0;
}

static DDFError DDFReadVarintField(CDDFInputBuffer& ib, const SDDFFieldDescriptor* desc, CDDFMessageBuilder& builder)
{
    assert(desc);

    uint64_t value;

    if (!ib.ReadVarInt64(&value))
    {
        return DDF_ERROR_WIRE_FORMAT;
    }

    switch (desc->m_Type)
    {
    case DDF_TYPE_INT32:
    {
        int32_t t = (int32_t) value;
        builder.WriteField(desc, (const char*) &t, sizeof(t));
    }
    break;

    case DDF_TYPE_UINT32:
    {
        uint32_t t = (uint32_t) value;
        builder.WriteField(desc, (const char*) &t, sizeof(t));
    }
    break;

    case DDF_TYPE_INT64:
    {
        int64_t t = (int64_t) value;
        builder.WriteField(desc, (const char*) &t, sizeof(t));
    }
    break;

    case DDF_TYPE_UINT64:
    {
        uint64_t t = (uint64_t) value;
        builder.WriteField(desc, (const char*) &t, sizeof(t));
    }
    break;

    default:
        return DDF_ERROR_FIELDTYPE_MISMATCH;
    }

    return DDF_ERROR_OK;
}

static DDFError DDFSkipField(CDDFInputBuffer& ib, uint32_t type)
{
    switch (type)
    {
    case DDF_WIRETYPE_VARINT:
    {
        uint64_t slask;
        if (ib.ReadVarInt64(&slask))
            return DDF_ERROR_OK;
        else
            return DDF_ERROR_WIRE_FORMAT;
    }
    break;
    case DDF_WIRETYPE_FIXED32:
    {
        uint32_t slask;
        if (ib.ReadFixed32(&slask))
            return DDF_ERROR_OK;
        else
            return DDF_ERROR_WIRE_FORMAT;
    }
    break;

    case DDF_WIRETYPE_FIXED64:
    {
        uint64_t slask;
        if (ib.ReadFixed64(&slask))
            return DDF_ERROR_OK;
        else
            return DDF_ERROR_WIRE_FORMAT;
    }
    break;

    case DDF_WIRETYPE_LENGTH_DELIMITED:
    {
        uint32_t length;
        if (!ib.ReadVarInt32(&length))
            return DDF_ERROR_WIRE_FORMAT;
        if (ib.Skip(length))
            return DDF_ERROR_OK;
        else
            return DDF_ERROR_WIRE_FORMAT;
    }
    break;

    default:
        return DDF_ERROR_WIRE_FORMAT;
    }
}

static DDFError DDFDoLoadMessage(CDDFLoadContext& load_context, CDDFInputBuffer& ib, const SDDFDescriptor* desc,
                                 CDDFMessageBuilder* builder, CDDFMessageBuilder* msg_builder, DDFPass pass);

static DDFError DDFDoLoadField(CDDFLoadContext& load_context, CDDFInputBuffer& ib, const SDDFFieldDescriptor* field,
                               uint32_t type, CDDFMessageBuilder* builder, CDDFMessageBuilder& msg_builder, DDFPass pass)
{
    switch (type)
    {
    case DDF_WIRETYPE_VARINT:
    {
        DDFError e = DDFReadVarintField(ib, field, msg_builder);
        if (e != DDF_ERROR_OK)
            return e;
    }
    break;
    case DDF_WIRETYPE_FIXED32:
    {
        if (field->m_Type == DDF_TYPE_FLOAT)
        {
            union
            {
                float f;
                uint32_t i;
            };
            if (ib.ReadFixed32(&i))
            {
                msg_builder.WriteField(field, (const char*) &i, sizeof(i));
            }
            else
            {
                return DDF_ERROR_WIRE_FORMAT; // TODO: EOF instead?
            }
        }
        else
        {
            return DDF_ERROR_FIELDTYPE_MISMATCH;
        }
    }
    break;

    case DDF_WIRETYPE_FIXED64:
        if (field->m_Type == DDF_TYPE_DOUBLE)
        {
            union
            {
                double f;
                uint64_t i;
            };
            if (ib.ReadFixed64(&i))
            {
                msg_builder.WriteField(field, (const char*) &i, sizeof(i));
            }
            else
            {
                return DDF_ERROR_WIRE_FORMAT; // TODO: EOF instead?
            }
        }
        else
        {
            return DDF_ERROR_FIELDTYPE_MISMATCH;
        }
        break;

    case DDF_WIRETYPE_LENGTH_DELIMITED:
    {
        uint32_t length;
        ib.ReadVarInt32(&length);

        uint32_t current = ib.Tell();

        if (field->m_Type == DDF_TYPE_STRING)
        {
            // NOTE: The actual string if written "after" the message
            // ie. builder instead of msg_builder
            char zero_byte = 0;
            char* p = builder->m_Current;
            builder->WriteBuffer(ib.m_Current, length);
            builder->WriteBuffer(&zero_byte, 1);
            msg_builder.WriteField(field, (const char*) &p, sizeof(p));
            if (!ib.Skip(length))
                return DDF_ERROR_WIRE_FORMAT;
        }
        else if (field->m_Type == DDF_TYPE_MESSAGE)
        {
            assert(field->m_MessageDescriptor);

            CDDFInputBuffer sub_buffer = ib.SubBuffer(length);
            // TODO: jobbar hŠr.. detta funkar inte fšr ett meddelenade i ett meddelande.
            // Minnet Šr redan allokerat...
            //CDDFMessageBuilder sub_msg_builder = builder->Reserve(field->m_MessageDescriptor->m_Size);
            //DDFError e = DDFDoLoadMessage(load_context, sub_buffer, field->m_MessageDescriptor, builder, &sub_msg_builder, pass);

            DDFError e = DDFDoLoadMessage(load_context, sub_buffer, field->m_MessageDescriptor, builder, &msg_builder, pass);

            if (e == DDF_ERROR_OK)
            {
                //size += tmp_size;
                //size = DDFAlign(size, desc->m_Align);
            }
            else
            {
                return e;
            }
        }
        else
        {
            return DDF_ERROR_FIELDTYPE_MISMATCH;
        }

        uint32_t remove_me = ib.Tell();
        if (ib.Tell() != current + length)
        {
            return DDF_ERROR_WIRE_FORMAT;
        }

        /*                    std::string buffer;
                              input->ReadString(&buffer, length);
                              printf("%d: %s\n", key, buffer.c_str());
                              assert(0); // TODO*/
    }
    break;

    default:
        return DDF_ERROR_WIRE_FORMAT;
    }

    return DDF_ERROR_OK;
}

// TODO:
// Required fields...
// Glšm inte att hantera messages som inte Šr repeated....

static DDFError DDFCalculateRepeated(CDDFLoadContext& load_context, CDDFInputBuffer& ib, const SDDFDescriptor* desc)
{
    assert(desc);

    // Calculate number of entries in arrays, ie memory requirements for the entire message
    uint32_t start = ib.Tell();
    while (!ib.Eof())
    {
        uint32_t tag;
        if (ib.ReadVarInt32(&tag))
        {
            uint32_t key = tag >> 3;
            uint32_t type = tag & 0x7;

            if (key == 0)
                return DDF_ERROR_WIRE_FORMAT;

            const SDDFFieldDescriptor* field = DDFFindField(desc, key);

            if (field == 0)
            {
                DDFError e = DDFSkipField(ib, type);
                if (e != DDF_ERROR_OK)
                    return e;
            }
            else
            {
                if (field->m_Label == DDF_LABEL_REPEATED)
                {
                    load_context.IncreaseArrayCount(start, field->m_Number);
                }

                if (field->m_Type != DDF_TYPE_MESSAGE)
                {
                    DDFError e = DDFSkipField(ib, type);
                    if (e != DDF_ERROR_OK)
                        return e;
                }
                else
                {
                    assert(field->m_MessageDescriptor);
                    uint32_t length;
                    if (!ib.ReadVarInt32(&length))
                        return DDF_ERROR_WIRE_FORMAT;

                    CDDFInputBuffer sub_ib = ib.SubBuffer(length);
                    DDFError e = DDFCalculateRepeated(load_context, sub_ib, field->m_MessageDescriptor);
                    if (e != DDF_ERROR_OK)
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

static DDFError DDFDoLoadMessage(CDDFLoadContext& load_context, CDDFInputBuffer& ib, const SDDFDescriptor* desc,
                                 CDDFMessageBuilder* builder, CDDFMessageBuilder* msg_builder, DDFPass pass)
{
    assert(desc);
    assert(builder);

    // Reserve memory for message excluding array and strings in message
    //CDDFMessageBuilder msg_builder = builder->Reserve(desc->m_Size);

    uint32_t start = ib.Tell();

    // Calculate pointers to arrays in message
    uintptr_t pointer_offset = 0;
    for (int i = 0; i < desc->m_FieldCount; ++i)
    {
        const SDDFFieldDescriptor* f = &desc->m_Fields[i];
        if (f->m_Label == DDF_LABEL_REPEATED)
        {
            if (!msg_builder->m_DryRun)
                msg_builder->SetArrayPointer(f, (uintptr_t) (builder->m_Current + pointer_offset));
            uint32_t array_memory;
            if (f->m_Type == DDF_TYPE_MESSAGE)
            {
                array_memory = f->m_MessageDescriptor->m_Size * load_context.GetArrayCount(start, f->m_Number);
            }
            else if (f->m_Type == DDF_TYPE_STRING)
            {
                assert(0 && "TODO");
            }
            else
            {
                array_memory = DDFScalarTypeSize(f->m_Type) * load_context.GetArrayCount(start, f->m_Number);
            }

            builder->Reserve(array_memory);
            pointer_offset += array_memory;

            // Reset array count. Array count is used for indexing arrays below. O(1) memory allocations is teh... :-)
            if (!msg_builder->m_DryRun)
                msg_builder->SetArrayCount(f, 0); // TODO: Remove now when having load-context..
        }
    }

    // Actual load
    {
        while (!ib.Eof())
        {
            uint32_t tag;
            if (ib.ReadVarInt32(&tag))
            {
                uint32_t key = tag >> 3;
                uint32_t type = tag & 0x7;

                if (key == 0)
                    return DDF_ERROR_WIRE_FORMAT;

                const SDDFFieldDescriptor* field = DDFFindField(desc, key);

                if (!field)
                {
                    // TODO: FIELD NOT FOUND. HOW TO HANDLE?!?!
                    DDFError e = DDFSkipField(ib, type);
                    if (e != DDF_ERROR_OK)
                        return e;
                    continue;
                }
                else
                {
                    DDFError e;
                    if (field->m_Label == DDF_LABEL_REPEATED)
                    {
                        uint32_t array_element_offset = msg_builder->GetNextArrayElementOffset(field);
                        CDDFMessageBuilder array_element_msg_builder = builder->SubMessageBuilder(array_element_offset);
                        e = DDFDoLoadField(load_context, ib, field, type, builder, array_element_msg_builder, pass);
                    }
                    else
                    {
                        e = DDFDoLoadField(load_context, ib, field, type, builder, *msg_builder, pass);
                    }

                    if (e != DDF_ERROR_OK)
                        return e;
                }
            }
            else
            {
                return DDF_ERROR_WIRE_FORMAT;
            }
        }
    }

#if 0

    // Pass 2. Load length delimited elements, ie repated and strings.
    // Elements that are ocated "after" this message in memory

    ib.Seek(start);
    {
        while (!ib.Eof())
        {
            uint32_t tag;
            if (ib.ReadVarInt32(&tag))
            {
                uint32_t key = tag >> 3;
                uint32_t type = tag & 0x7;

                if (key == 0)
                    return DDF_ERROR_WIRE_FORMAT;

                const SDDFFieldDescriptor* field = DDFFindField(desc, key);

                if (!field || field->m_Label != DDF_LABEL_REPEATED)
                {
                    // TODO: FIELD NOT FOUND. HOW TO HANDLE?!?!
                    DDFError e = DDFSkipField(ib, type);
                    if (e != DDF_ERROR_OK)
                        return e;
                    continue;
                }
                else
                {
                    uint32_t array_element_offset = msg_builder->GetNextArrayElementOffset(field);
                    CDDFMessageBuilder array_element_msg_builder = builder->SubMessageBuilder(array_element_offset);
                    DDFDoLoadField(load_context, ib, field, type, builder, array_element_msg_builder, pass);
                }
            }
            else
            {
                return DDF_ERROR_WIRE_FORMAT;
            }
        }
    }
#endif
    return DDF_ERROR_OK;
}

DDFError DDFLoadMessage(const void* buffer, uint32_t buffer_size, const SDDFDescriptor* desc, void** message)
{
    assert(buffer);
    assert(desc);
    assert(message);

    *message = 0;

    CDDFLoadContext load_context;

    CDDFMessageBuilder builder_dry(0, UINT32_MAX, true);
    CDDFMessageBuilder msg_builder_dry = builder_dry.Reserve(desc->m_Size);
    CDDFInputBuffer ib((const char*) buffer, buffer_size);

    DDFError e = DDFCalculateRepeated(load_context, ib, desc);
    if (e != DDF_ERROR_OK)
        return e;

    ib.Reset();
    e = DDFDoLoadMessage(load_context, ib, desc, &builder_dry, &msg_builder_dry, DDF_PASS_PROCESS_REPEATED);
    if (e == DDF_ERROR_OK)
    {
        int msg_buf_size = (int) (builder_dry.m_Current - builder_dry.m_Start);
        char* msg_buf = (char*) malloc(msg_buf_size);
        CDDFMessageBuilder builder(msg_buf, msg_buf_size, false);
        CDDFMessageBuilder msg_builder = builder.Reserve(desc->m_Size);

        CDDFInputBuffer is((const char*) buffer, buffer_size);
        e = DDFDoLoadMessage(load_context, is, desc, &builder, &msg_builder, DDF_PASS_LOAD_MESSAGE);
        if (e != DDF_ERROR_OK)
        {
            free((void*) msg_buf);
        }
        else
        {
            *message = (void*) msg_buf;
        }
        return e;
    }
    else
    {
        return e;
    }
}

void DDFFreeMessage(void* message)
{
    assert(message);
    free(message);
}
