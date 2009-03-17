#ifndef DDFINPUTSTREAM_H
#define DDFINPUTSTREAM_H

#include <stdint.h>

const int DDF_WIRE_MAXVARINTBYTES = 10;

class CDDFInputBuffer
{
public:
    CDDFInputBuffer(const char* buffer, uint32_t buffer_size);

    uint32_t            Tell();
    void                Seek(uint32_t pos);
    bool                Skip(uint32_t amount);
    void                Reset();
    CDDFInputBuffer     SubBuffer(uint32_t length);
    bool                Eof();
    bool                ReadVarInt32(uint32_t* value);
    bool                ReadVarInt64(uint64_t* value);
    bool                ReadFixed32(uint32_t* value);
    bool                ReadFixed64(uint64_t* value);

    const char* m_Start;
    const char* m_End;
    const char* m_Current;
};

#endif // DDFINPUTSTREAM_H
