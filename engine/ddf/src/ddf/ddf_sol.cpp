#include "ddf.h"
#include "ddf_util.h"
#include <sol/runtime.h>

namespace dmDDF
{
    void FixupRepeatedFieldCounts(const Descriptor* desc, void* message_)
    {
        uint8_t* message = (uint8_t*) message_;
        for (int i = 0; i < desc->m_FieldCount; ++i)
        {
            FieldDescriptor* field_desc = &desc->m_Fields[i];
            uint32_t count = 1;
            uint8_t* data_start = &message[field_desc->m_Offset];

            if (field_desc->m_Label == LABEL_REPEATED)
            {
                RepeatedField* repeated = (RepeatedField*) data_start;
                uintptr_t* base = (uintptr_t*) &repeated->m_Array;
                // everything but the below line is just data traversal to make this happen
                count = runtime_array_length(base);
                repeated->m_ArrayCount = count;
                data_start = (uint8_t *) (*base);
            }

            for (uint32_t j = 0; j < count; ++j)
            {
                if (field_desc->m_Type == TYPE_MESSAGE)
                {
                    uint8_t* data = data_start + j * field_desc->m_MessageDescriptor->m_Size;
                    FixupRepeatedFieldCounts(field_desc->m_MessageDescriptor, data);
                }
            }
        }
    }
}
