#include "ddf_util.h"

namespace dmDDF
{

    int ScalarTypeSize(uint32_t type)
    {
        switch(type)
        {
        case TYPE_BOOL:
            return sizeof(bool);

        case TYPE_INT32:
        case TYPE_FLOAT:
        case TYPE_FIXED32:
        case TYPE_UINT32:
        case TYPE_ENUM:
        case TYPE_SFIXED32:
        case TYPE_SINT32:
            return 4;

        case TYPE_SFIXED64:
        case TYPE_SINT64:
        case TYPE_DOUBLE:
        case TYPE_INT64:
        case TYPE_UINT64:
        case TYPE_FIXED64:
            return 8;

        /*
          TYPE_STRING
          TYPE_GROUP
          TYPE_MESSAGE
          TYPE_BYTES
          ...
        */
        default:
            assert(false && "Internal error");
            return -1;
        }
    }
}


