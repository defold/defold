#include "ddf.h"
#include "ddf_util.h"
#include "ddf_save.h"
#include "ddf_outputstream.h"

static bool DDFCountSaveFunction(void* context, const void* buffer, uint32_t buffer_size)
{
    uint32_t* count = (uint32_t*) context;
    *count = *count + buffer_size;
    return true;
}

DDFError DDFDoSaveMessage(const void* message_, const SDDFDescriptor* desc, void* context, DDFSaveFunction save_function)
{
    DDFOutputStream output_stream(save_function, context);
    const uint8_t* message = (const uint8_t*) message_;

    for (int i = 0; i < desc->m_FieldCount; ++i)
    {
        bool write_result;
        SDDFFieldDescriptor* field_desc = &desc->m_Fields[i];
        DDFType type = (DDFType) field_desc->m_Type;

#define DDF_SAVEMESSAGE_CASE(t, wt, func) \
        write_result = output_stream.WriteTag(field_desc->m_Number, wt) && \
                       output_stream.func(*((t*) data)); \
        if (!write_result)\
            return DDF_ERROR_IO_ERROR; \
        break;

        int element_size = 0;
        if ( field_desc->m_Type == DDF_TYPE_MESSAGE )
        {
            element_size = field_desc->m_MessageDescriptor->m_Size;
        }
        else if ( field_desc->m_Type == DDF_TYPE_STRING )
        {
            element_size = sizeof(const char*);
        }
        else
        {
            element_size = DDFScalarTypeSize(type);
        }

        uint32_t count = 1;
        const uint8_t* data_start = &message[field_desc->m_Offset];
        if (field_desc->m_Label == DDF_LABEL_REPEATED)
        {
            DDFRepeatedField* repeated = (DDFRepeatedField*) data_start;
            count = repeated->m_ArrayCount;
            data_start = (const uint8_t *) repeated->m_Array;
        }

        for (int j = 0; j < count; ++j)
        {
            const uint8_t* data = data_start + j * element_size;

            switch (field_desc->m_Type)
            {
                case DDF_TYPE_DOUBLE:
                    DDF_SAVEMESSAGE_CASE(double, DDF_WIRETYPE_FIXED64, WriteDouble)

                case DDF_TYPE_FLOAT:
                    DDF_SAVEMESSAGE_CASE(float, DDF_WIRETYPE_FIXED32, WriteFloat)

                case DDF_TYPE_INT64:
                    DDF_SAVEMESSAGE_CASE(int64_t, DDF_WIRETYPE_VARINT, WriteVarInt64)

                case DDF_TYPE_UINT64:
                    DDF_SAVEMESSAGE_CASE(uint64_t, DDF_WIRETYPE_VARINT, WriteVarInt64)

                case DDF_TYPE_INT32:
                    DDF_SAVEMESSAGE_CASE(int32_t, DDF_WIRETYPE_VARINT, WriteVarInt32SignExtended)

                case DDF_TYPE_FIXED64:
                    assert(false);
                    break;

                case DDF_TYPE_FIXED32:
                    assert(false);
                    break;

                case DDF_TYPE_BOOL:
                    assert(false);
                    break;

                case DDF_TYPE_STRING:
                    DDF_SAVEMESSAGE_CASE(const char*, DDF_WIRETYPE_LENGTH_DELIMITED, WriteString)

                case DDF_TYPE_GROUP:
                    assert(false);
                    break;

                case DDF_TYPE_MESSAGE:
                {
                    uint32_t len = 0;
                    DDFError e = DDFSaveMessage(data, field_desc->m_MessageDescriptor, &len, &DDFCountSaveFunction);
                    if (e != DDF_ERROR_OK)
                        return e;

                    write_result = output_stream.WriteTag(field_desc->m_Number, DDF_WIRETYPE_LENGTH_DELIMITED) && output_stream.WriteVarInt32(len);
                    if (!write_result)
                        return DDF_ERROR_IO_ERROR;

                    e = DDFSaveMessage(data, field_desc->m_MessageDescriptor, context, save_function);
                    if (e != DDF_ERROR_OK)
                        return e;
                }
                break;

                case DDF_TYPE_BYTES:
                    assert(false);
                    break;

                case DDF_TYPE_UINT32:
                    DDF_SAVEMESSAGE_CASE(uint32_t, DDF_WIRETYPE_VARINT, WriteVarInt32)

                case DDF_TYPE_ENUM:
                    assert(false);
                    break;

                case DDF_TYPE_SFIXED32:
                    assert(false);
                    break;

                case DDF_TYPE_SFIXED64:
                    assert(false);
                    break;

                case DDF_TYPE_SINT32:
                    assert(false);
                    break;

                case DDF_TYPE_SINT64:
                    assert(false);
                    break;

                default:
                    assert(false);
            }
        }
    }

#undef DDF_SAVEMESSAGE_CASE

    return DDF_ERROR_OK;
}
