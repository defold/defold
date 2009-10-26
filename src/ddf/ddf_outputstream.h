#ifndef DDF_OUTPUTSTREAM
#define DDF_OUTPUTSTREAM

#include "ddf.h"

class DDFOutputStream
{
public:
    DDFOutputStream(DDFSaveFunction save_function, void* context);

    bool Write(const void* buffer, int length);

    bool WriteTag(uint32_t number, DDFWireType type);

    bool WriteVarInt32SignExtended(int32_t value);

    bool WriteVarInt32(uint32_t value);
    bool WriteVarInt64(uint64_t value);
    bool WriteFixed32(uint32_t value);
    bool WriteFixed64(uint64_t value);

    bool WriteFloat(float value);
    bool WriteDouble(double value);
    bool WriteInt32(int32_t value);
    bool WriteUInt32(uint32_t value);
    bool WriteInt64(int64_t value);
    bool WriteUInt64(uint64_t value);

    bool WriteString(const char* str);

    DDFSaveFunction m_SaveFunction;
    void*           m_Context;
};

#endif // DDF_OUTPUTSTREAM
