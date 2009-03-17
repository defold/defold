#include <assert.h>
#include "ddf_inputstream.h"

CDDFInputBuffer::CDDFInputBuffer(const char* buffer, uint32_t buffer_size) :
    m_Start(buffer), m_End(buffer + buffer_size), m_Current(buffer)
{
}

uint32_t CDDFInputBuffer::Tell()
{
    assert(m_Current <= m_End);
    return (uint32_t) (m_Current - m_Start);
}

void CDDFInputBuffer::Seek(uint32_t pos)
{
    m_Current = m_Start + pos;
    assert(m_Current <= m_End);
}

bool CDDFInputBuffer::Skip(uint32_t amount)
{
    assert(m_Current <= m_End);
    m_Current += amount;
    return m_Current <= m_End;
}

void CDDFInputBuffer::Reset()
{
    assert(m_Current <= m_End);
    m_Current = m_Start;
}

bool CDDFInputBuffer::ReadVarInt32(uint32_t *value)
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

bool CDDFInputBuffer::Eof()
{
    assert(m_Current <= m_End);
    return m_Current == m_End;
}

bool CDDFInputBuffer::ReadVarInt64(uint64_t* value)
{
    uint64_t result = 0;
    int count = 0;

    while (true)
    {
        if (m_Current >= m_End || count == DDF_WIRE_MAXVARINTBYTES)
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

bool CDDFInputBuffer::ReadFixed32(uint32_t *value)
{
    if (m_End - m_Current <= 4)
        return false;

    char*p = (char*) value;
#if defined(__LITTLE_ENDIAN__) || defined(i386) || defined(_M_IX86)
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

bool CDDFInputBuffer::ReadFixed64(uint64_t *value)
{
    if (m_End - m_Current <= 8)
        return false;

    char*p = (char*) value;
#if defined(__LITTLE_ENDIAN__) || defined(i386) || defined(_M_IX86)
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

// TODO: Return bool....???
CDDFInputBuffer CDDFInputBuffer::SubBuffer(uint32_t length)
{
    assert(m_Current + length <= m_End);
    const char* c = m_Current;
    m_Current += length;
    return CDDFInputBuffer(c, length);
}
