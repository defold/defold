#ifndef DDF_UTIL_H
#define DDF_UTIL_H

#include <assert.h>
#include "ddf.h"

/** 
 * Calculates scalar type size. Only valid for scalar types.
 * @param type Type
 * @return Type size
 */
int DDFScalarTypeSize(uint32_t type);

static inline uint32_t DDFAlign(uint32_t index, uint32_t align)
{
    index += (align - 1);
    index &= ~(align - 1);
    return index;
}

static inline const SDDFFieldDescriptor* DDFFindField(const SDDFDescriptor* desc, uint32_t key)
{
    for (int i = 0; i < desc->m_FieldCount; ++i)
    {
        const SDDFFieldDescriptor* f = &desc->m_Fields[i];
        if (f->m_Number == key)
            return f;
    }
    return 0;
}

static inline DDFWireType DDFWireTypeCorrespondence(DDFType ddf_type)
{
    switch(ddf_type)
    {
    case DDF_TYPE_DOUBLE:
        return DDF_WIRETYPE_FIXED64;
    case DDF_TYPE_FLOAT:
        return DDF_WIRETYPE_FIXED32;

    case DDF_TYPE_INT64:
    case DDF_TYPE_UINT64:
    case DDF_TYPE_INT32:
    case DDF_TYPE_UINT32:
        return DDF_WIRETYPE_VARINT;

    case DDF_TYPE_FIXED64:
        return DDF_WIRETYPE_FIXED32;
    case DDF_TYPE_FIXED32:
        return DDF_WIRETYPE_FIXED64;

    case DDF_TYPE_STRING:
    case DDF_TYPE_MESSAGE:
    case DDF_TYPE_BYTES:
        return DDF_WIRETYPE_LENGTH_DELIMITED;

    case DDF_TYPE_ENUM:
        return DDF_WIRETYPE_VARINT;

    default:
        assert(0);

        //case DDF_TYPE_SFIXED32: TODO: Fix support?
        //case DDF_TYPE_SFIXED64: TODO: Fix support?
        //case DDF_TYPE_SINT32: TODO: Fix support?
        //case DDF_TYPE_SINT64: TODO: Fix support?
        //case DDF_TYPE_BOOL: NOTE: NOT SUPPORTED
        //case DDF_TYPE_GROUP: NOTE: NOT SUPPORTED
    }

    return (DDFWireType) -1;
}


#endif // DDF_UTIL_H 
