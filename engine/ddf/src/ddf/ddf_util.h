// Copyright 2020-2026 The Defold Foundation
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

#ifndef DDF_UTIL_H
#define DDF_UTIL_H

#include <assert.h>
#include "ddf.h"

namespace dmDDF
{

    /**
     * Calculates scalar type size. Only valid for scalar types.
     * @param type Type
     * @return Type size
     */
    int ScalarTypeSize(uint32_t type);

    static inline uint32_t DDFAlign(uint32_t index, uint32_t align)
    {
        index += (align - 1);
        index &= ~(align - 1);
        return index;
    }

    static inline const FieldDescriptor* FindField(const Descriptor* desc, uint32_t key, uint32_t* index)
    {
        for (int i = 0; i < desc->m_FieldCount; ++i)
        {
            const FieldDescriptor* f = &desc->m_Fields[i];
            if (f->m_Number == key)
            {
                if (index)
                    *index = i;
                return f;
            }
        }
        return 0;
    }

    static inline WireType WireTypeCorrespondence(Type ddf_type)
    {
        switch(ddf_type)
        {
        case TYPE_DOUBLE:
            return WIRETYPE_FIXED64;
        case TYPE_FLOAT:
            return WIRETYPE_FIXED32;

        case TYPE_INT64:
        case TYPE_UINT64:
        case TYPE_INT32:
        case TYPE_UINT32:
        case TYPE_BOOL:
            return WIRETYPE_VARINT;

        case TYPE_FIXED64:
            return WIRETYPE_FIXED32;
        case TYPE_FIXED32:
            return WIRETYPE_FIXED64;

        case TYPE_STRING:
        case TYPE_MESSAGE:
        case TYPE_BYTES:
            return WIRETYPE_LENGTH_DELIMITED;

        case TYPE_ENUM:
            return WIRETYPE_VARINT;

        default:
            assert(0);

            //case TYPE_SFIXED32: TODO: Fix support?
            //case TYPE_SFIXED64: TODO: Fix support?
            //case TYPE_SINT32: TODO: Fix support?
            //case TYPE_SINT64: TODO: Fix support?
            //case TYPE_BOOL: NOTE: NOT SUPPORTED
            //case TYPE_GROUP: NOTE: NOT SUPPORTED
        }

        return (WireType) -1;
    }

}
#endif // DDF_UTIL_H 
