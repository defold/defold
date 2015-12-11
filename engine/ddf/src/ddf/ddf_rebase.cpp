#include "ddf.h"
#include "ddf_util.h"

namespace dmDDF
{
    void RebaseMessagePointers(const Descriptor* desc, void* message_, void *current, void *target)
    {
        // ptrs are calculate as follows:
        uint8_t* message = (uint8_t*) message_;
        const int64_t diff = (uint8_t*)target - (uint8_t*)current;
        for (int i = 0; i < desc->m_FieldCount; ++i)
        {
            FieldDescriptor* field_desc = &desc->m_Fields[i];
            Type type = (Type) field_desc->m_Type;

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
            uint8_t* data_start = &message[field_desc->m_Offset];
            if (field_desc->m_Label == LABEL_REPEATED)
            {
                RepeatedField* repeated = (RepeatedField*) data_start;
                uintptr_t* base = (uintptr_t*) &repeated->m_Array;
                *base += diff;
                count = repeated->m_ArrayCount;
                data_start = (uint8_t *) (*base);
            }

            for (uint32_t j = 0; j < count; ++j)
            {
                uint8_t* data = data_start + j * element_size;

                switch (field_desc->m_Type)
                {
                    case TYPE_STRING:
                    {
                        // Do not write null-strings. Will result in null-pointer error in WriteString
                        uintptr_t *ptr = (uintptr_t*) data;
                        *ptr += diff;
                    }
                    break;

                    case TYPE_MESSAGE:
                    {
                        RebaseMessagePointers(field_desc->m_MessageDescriptor, data, current, target);
                    }
                    break;

                    case TYPE_BYTES:
                    {
                        RepeatedField* bytes_repeated = (RepeatedField*) data;
                        uintptr_t* base = (uintptr_t*) &bytes_repeated->m_Array;
                        *base += diff;
                    }
                    break;
                }
            }
        }
    }
}

