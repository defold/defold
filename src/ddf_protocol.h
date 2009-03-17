#ifndef DDF_PROTOCOL_H
#define DDF_PROTOCOL_H

enum DDFWireType
{
    DDF_WIRETYPE_VARINT           = 0,
    DDF_WIRETYPE_FIXED64          = 1,
    DDF_WIRETYPE_LENGTH_DELIMITED = 2,
    DDF_WIRETYPE_START_GROUP      = 3,
    DDF_WIRETYPE_END_GROUP        = 4,
    DDF_WIRETYPE_FIXED32          = 5,
};

#endif
