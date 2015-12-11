#include "array.h"
#include "log.h"

#include <assert.h>
#include <string.h>
#include <stdio.h>

#include <sol/runtime.h>
#include <sol/reflect.h>

static bool IsPODType(Type *t)
{
    switch (t->kind)
    {
        case KIND_INT32:
        case KIND_UINT32:
        case KIND_INT8:
        case KIND_UINT8:
        case KIND_INT16:
        case KIND_UINT16:
        case KIND_INT64:
        case KIND_UINT64:
        case KIND_FLOAT32:
        case KIND_FLOAT64:
        case KIND_BOOL:
            return true;
        default:
            return false;
    }
}

// Copies POD data from source (all of it) to destination array
extern "C" int SolArrayCopy(::Any dest, ::Any source)
{
    Type* src_type = reflect_get_any_type(source);
    Type* dst_type = reflect_get_any_type(dest);
    if (!src_type || !src_type->referenced_type || !src_type->referenced_type->array_type)
    {
        dmLogError("ArrayCopy: Source array is not an array");
        return 0;
    }
    if (!dst_type || !dst_type->referenced_type || !dst_type->referenced_type->array_type)
    {
        dmLogError("ArrayCopy: Destination array is not an array");
        return 0;
    }
    
    ArrayType *src_arr = src_type->referenced_type->array_type;
    ArrayType *dst_arr = dst_type->referenced_type->array_type;
    
    if (!IsPODType(src_arr->element_type))
    {
        dmLogError("ArrayCopy: Source array is not of valid type.");
        return 0;
    }
    if (!IsPODType(dst_arr->element_type))
    {
        dmLogError("ArrayCopy: Destination array is not of valid type.");
        return 0;
    }
    
    void *out = (void*)reflect_get_any_value(dest);
    void *in = (void*)reflect_get_any_value(source);
    if (!out)
    {
        dmLogError("ArrayCopy: Destination is nil");
        return 0;
    }
    
    if (!in)
    {
        dmLogError("ArrayCopy: Source is nil");
        return 0;
    }
    
    uint32_t bytes_available = runtime_array_length(out) * dst_arr->stride;
    uint32_t bytes_required = runtime_array_length(in) * src_arr->stride;
    
    if (bytes_required < bytes_available)
    {
        dmLogError("Bytes available %d, bytes required %d", bytes_available, bytes_required);
        return 0;
    }
    
    memcpy(out, in, bytes_required);
    return bytes_required / dst_arr->stride;
}

