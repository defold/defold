#include "ddf_util.h"

int DDFScalarTypeSize(uint32_t type)
{
    switch(type)
    {
    case DDF_TYPE_BOOL:
        assert(false && "Bools not supported."); // TODO: Fix?
        return -1;

    case DDF_TYPE_INT32:
    case DDF_TYPE_FLOAT:
    case DDF_TYPE_FIXED32:
    case DDF_TYPE_UINT32:
    case DDF_TYPE_ENUM:
    case DDF_TYPE_SFIXED32:
    case DDF_TYPE_SINT32:
        return 4;

    case DDF_TYPE_SFIXED64:
    case DDF_TYPE_SINT64:
    case DDF_TYPE_DOUBLE:
    case DDF_TYPE_INT64:
    case DDF_TYPE_UINT64:
    case DDF_TYPE_FIXED64:
        return 8;

    /*
      DDF_TYPE_STRING
      DDF_TYPE_GROUP
      DDF_TYPE_MESSAGE
      DDF_TYPE_BYTES
      ...
    */
    default:
        assert(false && "Internal error");
        return -1;
    }
}


