#ifndef DDFINPUTSTREAM_H
#define DDFINPUTSTREAM_H

#include <stdint.h>

namespace dmDDF
{
    const int WIRE_MAXVARINTBYTES = 10;

    class InputBuffer
    {
    public:
        InputBuffer();
        InputBuffer(const char* buffer, uint32_t buffer_size);

        uint32_t            Tell();
        void                Seek(uint32_t pos);
        bool                Skip(uint32_t amount);
        bool                SubBuffer(uint32_t length, InputBuffer* sub_buffer);
        bool                Eof();

        bool                Read(int length, const char** buffer_out);

        bool                ReadVarInt32(uint32_t* value);
        bool                ReadVarInt64(uint64_t* value);
        bool                ReadFixed32(uint32_t* value);
        bool                ReadFixed64(uint64_t* value);

        bool                ReadFloat(float* value);
        bool                ReadDouble(double* value);
        bool                ReadInt32(int32_t* value);
        bool                ReadUInt32(uint32_t* value);
        bool                ReadInt64(int64_t* value);
        bool                ReadUInt64(uint64_t* value);
        bool                ReadBool(bool* value);

    private:
        const char* m_Start;
        const char* m_End;
        const char* m_Current;
    };
}

#endif // DDFINPUTSTREAM_H
