#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>

#include "ddf.h"
#include "ddf_inputbuffer.h"
#include "ddf_load.h"
#include "ddf_save.h"
#include "ddf_util.h"
#include "config.h"

// TODO:
// Required fields...
// Glšm inte att hantera messages som inte Šr repeated....

static DDFError DDFCalculateRepeated(CDDFLoadContext* load_context, CDDFInputBuffer* ib, const SDDFDescriptor* desc)
{
    assert(desc);

    // Calculate number of entries in arrays, ie memory requirements for the entire message
    uint32_t start = ib->Tell();
    while (!ib->Eof())
    {
        uint32_t tag;
        if (ib->ReadVarInt32(&tag))
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
                    load_context->IncreaseArrayCount(start, field->m_Number);
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
                    if (!ib->ReadVarInt32(&length))
                        return DDF_ERROR_WIRE_FORMAT;

                    #if 1
                    CDDFInputBuffer sub_ib;
                    if (!ib->SubBuffer(length, &sub_ib))
                    {
                        return DDF_ERROR_WIRE_FORMAT;
                    }

                    DDFError e = DDFCalculateRepeated(load_context, &sub_ib, field->m_MessageDescriptor);
                    #else
                    CDDFInputBuffer sub_ib = ib;
                    sub_ib->m_End = sub_ib->m_Current + length;
                    DDFError e = DDFCalculateRepeated(load_context, &sub_ib, field->m_MessageDescriptor);
                    #endif
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

DDFError DDFLoadMessage(const void* buffer, uint32_t buffer_size, const SDDFDescriptor* desc, void** out_message)
{
    assert(buffer);
    assert(desc);
    assert(out_message);

    if (desc->m_MajorVersion != DDF_MAJOR_VERSION)
        return DDF_ERROR_VERSION_MISMATCH;

    CDDFLoadContext load_context(0, 0, true);
    CDDFMessage dry_message = load_context.AllocMessage(desc);

    CDDFInputBuffer input_buffer((const char*) buffer, buffer_size);

    DDFError e = DDFCalculateRepeated(&load_context, &input_buffer, desc);
    if (e != DDF_ERROR_OK)
    {
        return e;
    }

    input_buffer.Seek(0);
    e = DDFDoLoadMessage(&load_context, &input_buffer, desc, &dry_message);

    int message_buffer_size = load_context.GetMemoryUsage();
    char* message_buffer = (char*) malloc(message_buffer_size);
    load_context.SetMemoryBuffer(message_buffer, message_buffer_size, false);
    CDDFMessage message = load_context.AllocMessage(desc);

    input_buffer.Seek(0);
    e = DDFDoLoadMessage(&load_context, &input_buffer, desc, &message);
    if ( e == DDF_ERROR_OK )
    {
        *out_message = (void*) message_buffer;
    }
    else
    {
        free((void*) message_buffer);
        *out_message = 0;
    }
    return e;
}

DDFError DDFLoadMessageFromFile(const char* file_name, const SDDFDescriptor* desc, void** message)
{
    FILE* f = fopen(file_name, "rb");
    if (f)
    {
        if (fseek(f, 0, SEEK_END) != 0)
        {
            return DDF_ERROR_IO_ERROR;
        }

        long size = ftell(f);

        if (fseek(f, 0, SEEK_SET) != 0)
        {
            return DDF_ERROR_IO_ERROR;
        }

        void* buffer = malloc(size);
        if ( fread(buffer, 1, size, f) != size )
        {
            free(buffer);
            return DDF_ERROR_IO_ERROR;
        }

        DDFError e = DDFLoadMessage(buffer, (uint32_t) size, desc, message);
        fclose(f);
        free(buffer);
        return e;
    }
    else
    {
        return DDF_ERROR_IO_ERROR;
    }
}

DDFError DDFSaveMessage(const void* message, const SDDFDescriptor* desc, void* context, DDFSaveFunction save_function)
{
    return DDFDoSaveMessage(message, desc, context, save_function);
}

static bool DDFFileSaveFunction(void* context, const void* buffer, uint32_t buffer_size)
{
    size_t nwritten = fwrite(buffer, 1, buffer_size, (FILE*) context);
    return nwritten == buffer_size;
}

DDFError DDFSaveMessageToFile(const void* message, const SDDFDescriptor* desc, const char* file_name)
{
    FILE* file = fopen(file_name, "wb");
    if (!file)
        return DDF_ERROR_IO_ERROR;
    DDFError ret = DDFSaveMessage(message, desc, file, &DDFFileSaveFunction);
    fclose(file);
    return ret;
}

int32_t DDFGetEnumValue(const SDDFEnumDescriptor* desc, const char* name)
{
    assert(desc);
    assert(name);

    uint32_t n = desc->m_EnumValueCount;
    for (uint32_t i = 0; i < n; ++i)
    {
        if (strcmp(name, desc->m_EnumValues[i].m_Name) == 0)
        {
            return desc->m_EnumValues[i].m_Value;
        }
    }

    assert(false);
    return INT32_MIN;
}

const char* DDFGetEnumName(const SDDFEnumDescriptor* desc, int32_t value)
{
    uint32_t n = desc->m_EnumValueCount;
    for (uint32_t i = 0; i < n; ++i)
    {
        if (desc->m_EnumValues[i].m_Value == value)
        {
            return desc->m_EnumValues[i].m_Name;
        }
    }

    return 0;
}

void DDFFreeMessage(void* message)
{
    assert(message);
    free(message);
}
