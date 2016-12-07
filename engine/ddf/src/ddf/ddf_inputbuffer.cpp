#include <assert.h>
#include "ddf_inputbuffer.h"

namespace dmDDF
{

    InputBuffer::InputBuffer() :
        m_Start(0), m_End(0), m_Current(0)
    {
    }

    InputBuffer::InputBuffer(const char* buffer, uint32_t buffer_size) :
        m_Start(buffer), m_End(buffer + buffer_size), m_Current(buffer)
    {
    }

    uint32_t InputBuffer::Tell()
    {
        assert(m_Current <= m_End);
        return (uint32_t) (m_Current - m_Start);
    }

    void InputBuffer::Seek(uint32_t pos)
    {
        m_Current = m_Start + pos;
        assert(m_Current <= m_End);
    }

    bool InputBuffer::Skip(uint32_t amount)
    {
        assert(m_Current <= m_End);
        m_Current += amount;
        return m_Current <= m_End;
    }

    bool InputBuffer::Read(int length, const char** buffer_out)
    {
        assert(buffer_out);
        assert(m_Current <= m_End);
        if (m_Current + length > m_End)
        {
            *buffer_out = 0;
            return false;
        }
        else
        {
            *buffer_out = m_Current;
            m_Current += length;
            return true;
        }
    }

    bool InputBuffer::ReadVarInt32(uint32_t *value)
    {
        assert(value);
        assert(m_Current <= m_End);

        uint64_t tmp;
        if (ReadVarInt64(&tmp))
        {
            *value = (uint32_t) tmp;
            return true;
        }
        else
        {
            return false;
        }
    }

    bool InputBuffer::Eof()
    {
        assert(m_Current <= m_End);
        return m_Current == m_End;
    }

    bool InputBuffer::ReadVarInt64(uint64_t* value)
    {
        uint64_t result = 0;
        int count = 0;

        while (true)
        {
            if (m_Current >= m_End || count == WIRE_MAXVARINTBYTES)
                return false;

            uint32_t b = *m_Current++;
            result |= ((uint64_t) (b & 0x7f)) << (7 * count);
            count++;

            if (!(b & 0x80))
            {
                break;
            }
        }

        *value = result;
        return true;
    }

    bool InputBuffer::ReadFixed32(uint32_t *value)
    {
        if (m_End - m_Current < 4)
            return false;

        char*p = (char*) value;
    #if defined(__LITTLE_ENDIAN__) || defined(i386) || defined(__x86_64) || defined(_M_IX86) || defined(_M_X64) || defined(ANDROID) || defined(__AVM2__)
        p[0] = m_Current[0];
        p[1] = m_Current[1];
        p[2] = m_Current[2];
        p[3] = m_Current[3];
    #else
    #error "Unable to determine endian"
    #endif

        m_Current += 4;
        return true;
    }

    bool InputBuffer::ReadFixed64(uint64_t *value)
    {
        if (m_End - m_Current < 8)
            return false;

        char*p = (char*) value;
    #if defined(__LITTLE_ENDIAN__) || defined(i386) || defined(__x86_64) || defined(_M_IX86) || defined(_M_X64) || defined(ANDROID) || defined(__AVM2__)
        p[0] = m_Current[0];
        p[1] = m_Current[1];
        p[2] = m_Current[2];
        p[3] = m_Current[3];
        p[4] = m_Current[4];
        p[5] = m_Current[5];
        p[6] = m_Current[6];
        p[7] = m_Current[7];
    #else
    #error "Unable to determine endian"
    #endif

        m_Current += 8;
        return true;
    }


    bool InputBuffer::ReadFloat(float* value)
    {
        union
        {
            float f;
            uint32_t i;
        };
        if (ReadFixed32(&i))
        {
            *value = f;
            return true;
        }
        else
        {
            return false;
        }
    }

    bool InputBuffer::ReadDouble(double* value)
    {
        union
        {
            double f;
            uint64_t i;
        };
        if (ReadFixed64(&i))
        {
            *value = f;
            return true;
        }
        else
        {
            return false;
        }
    }

    bool InputBuffer::ReadInt32(int32_t* value)
    {
        uint32_t v;
        if (ReadVarInt32(&v))
        {
            *value = (int32_t) v;
            return true;
        }
        else
        {
            return false;
        }
    }

    bool InputBuffer::ReadUInt32(uint32_t* value)
    {
        uint32_t v;
        if (ReadVarInt32(&v))
        {
            *value = v;
            return true;
        }
        else
        {
            return false;
        }
    }

    bool InputBuffer::ReadInt64(int64_t* value)
    {
        uint64_t v;
        if (ReadVarInt64(&v))
        {
            *value = (int64_t) v;
            return true;
        }
        else
        {
            return false;
        }
    }

    bool InputBuffer::ReadUInt64(uint64_t* value)
    {
        uint64_t v;
        if (ReadVarInt64(&v))
        {
            *value = v;
            return true;
        }
        else
        {
            return false;
        }
    }

    bool InputBuffer::ReadBool(bool* value)
    {
        uint32_t v;
        if (ReadVarInt32(&v))
        {
            *value = (bool)v;
            return true;
        }
        else
        {
            return false;
        }
    }

    bool InputBuffer::SubBuffer(uint32_t length, InputBuffer* sub_buffer)
    {
        if (m_Current + length > m_End)
        {
            return false;
        }
    #if 0
        const char* c = m_Current;
        m_Current += length;
        return InputBuffer(c, length);
    #else
        InputBuffer ret = InputBuffer(m_Start, m_End - m_Start);
        // NOTE: Very important to preserve start. Tell() is used to
        // uniquely identify repeated fields. See function DDFCalculateRepeated(.)
        ret.m_Start = m_Start;
        ret.m_Current = m_Current;
        ret.m_End = m_Current + length;
        m_Current += length;
        *sub_buffer = ret;
        return true;
    #endif
    }
}
