// Copyright 2020-2026 The Defold Foundation
// Copyright 2014-2020 King
// Copyright 2009-2014 Ragnar Svensson, Christian Murray
// Licensed under the Defold License version 1.0 (the "License"); you may not use
// this file except in compliance with the License.
//
// You may obtain a copy of the License, together with FAQs at
// https://www.defold.com/license
//
// Unless required by applicable law or agreed to in writing, software distributed
// under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
// CONDITIONS OF ANY KIND, either express or implied. See the License for the
// specific language governing permissions and limitations under the License.

#include "json.h"
#include <dlib/dstrings.h>
#include <dlib/hash.h>
#include <dlib/hashtable.h>
#include <simd/simd.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <limits.h>
#include <locale.h>

namespace dmJson
{
    // dmArray allocates with a uint32_t byte count. Node indices also fit in int.
    static const uint32_t MAX_NODE_CAPACITY = UINT32_MAX / sizeof(Node);

    static double ReadNumber(char* begin, char* end)
    {
        bool negative = *begin == '-';
        char* digits = begin + negative;
        char* point = 0;
        // All integers of at most 15 decimal digits fit exactly in a double.
        // Longer tokens keep strtod's rounding and range handling.
        if (end - digits <= 15)
        {
            uint64_t integer = 0;
            while (digits != end)
            {
                uint32_t digit = (uint32_t)(*digits - '0');
                if (digit > 9)
                {
                    break;
                }
                integer = integer * 10 + digit;
                ++digits;
            }
            if (digits == end)
            {
                double value = (double)integer;
                return negative ? -value : value;
            }
            // The token scan has already checked number syntax. Its first
            // nondigit is the decimal point or the start of the exponent.
            point = *digits == '.' ? digits : 0;
        }
        else
        {
            point = (char*)memchr(begin, '.', end - begin);
        }
        // Isolate the JSON token so a comma cannot become a decimal separator.
        // Translate its decimal point for strtod without changing the game's locale.
        char next = *end;
        *end = 0;
        const char* decimal = point ? localeconv()->decimal_point : ".";
        double value;
        if (point && decimal[1])
        {
            uint32_t prefix_size = (uint32_t)(point - begin);
            uint32_t decimal_size = (uint32_t)strlen(decimal);
            if (decimal_size > UINT32_MAX - (uint32_t)(end - begin))
            {
                *end = next;
                return HUGE_VAL;
            }
            dmArray<char> localized;
            localized.SetCapacity((uint32_t)(end - begin) + decimal_size);
            memcpy(localized.Begin(), begin, prefix_size);
            memcpy(localized.Begin() + prefix_size, decimal, decimal_size);
            memcpy(localized.Begin() + prefix_size + decimal_size, point + 1, end - point);
            value = strtod(localized.Begin(), 0);
        }
        else
        {
            if (point)
            {
                *point = decimal[0];
            }
            value = strtod(begin, 0);
            if (point)
            {
                *point = '.';
            }
        }
        *end = next;
        return value;
    }

    uint32_t Utf8Bytes(const char* text, uint32_t size)
    {
        unsigned char first = (unsigned char)text[0];
        if (first < 0x80)
        {
            return 1;
        }
        uint32_t count = first >= 0xf0 ? 4 : first >= 0xe0 ? 3 : 2;
        if (first < 0xc2 || first > 0xf4 || size < count)
        {
            return 0;
        }
        uint32_t cp = first & (0x7f >> count);
        for (uint32_t i = 1; i < count; ++i)
        {
            unsigned char c = text[i];
            if ((c & 0xc0) != 0x80)
            {
                return 0;
            }
            cp = (cp << 6) | (c & 63);
        }
        if ((count == 3 && cp < 0x800) || (count == 4 && cp < 0x10000) || cp > 0x10ffff || (cp >= 0xd800 && cp <= 0xdfff))
        {
            return 0;
        }
        return count;
    }

    static bool PlainStringBlock(const char* text, bool ascii_only)
    {
        dsimd_uint4_t bytes = SimdLoadUint(text);
        dsimd_uint4_t ones = SimdSplatUint(0x01010101);
        dsimd_uint4_t quotes = SimdXorUint(bytes, SimdSplatUint(0x22222222));
        dsimd_uint4_t slashes = SimdXorUint(bytes, SimdSplatUint(0x5c5c5c5c));
        // A zero byte in x sets a high bit in (x - 0x01010101) & ~x.
        // Borrow may also mark a neighboring byte, but cannot hide a match.
        dsimd_uint4_t special = SimdOrUint(SimdAndUint(SimdSubUint(quotes, ones), SimdNotUint(quotes)),
                                           SimdAndUint(SimdSubUint(slashes, ones), SimdNotUint(slashes)));
        // The same test with 0x20 detects control bytes. Reading strings can
        // skip UTF-8 bytes already validated by Parse; writing must check them.
        special = SimdOrUint(special, SimdAndUint(SimdSubUint(bytes, SimdSplatUint(0x20202020)), SimdNotUint(bytes)));
        if (ascii_only)
        {
            special = SimdOrUint(special, bytes);
        }
        special = SimdAndUint(special, SimdSplatUint(0x80808080));
        return SimdAll(SimdEqualUint(special, SimdSplatUint(0)));
    }

    // The first byte is known to be ordinary text. Probe a short scalar prefix
    // so escape-heavy strings do not repeatedly test overlapping SIMD blocks.
    static uint32_t PlainStringBytes(const char* text, uint32_t size, bool ascii_only)
    {
        uint32_t i = 1;
        for (; i < size && i < 8; ++i)
        {
            unsigned char c = (unsigned char)text[i];
            if (c < 32 || c == '"' || c == '\\' || (ascii_only && c >= 0x80))
            {
                return i;
            }
        }
        while (size - i >= 16 && PlainStringBlock(text + i, ascii_only))
        {
            i += 16;
        }
        for (; i < size; ++i)
        {
            unsigned char c = (unsigned char)text[i];
            if (c < 32 || c == '"' || c == '\\' || (ascii_only && c >= 0x80))
            {
                break;
            }
        }
        return i;
    }

    void WriteString(const char* text, uint32_t size, FWrite write, void* context)
    {
        write(context, "\"", 1);
        uint32_t start = 0;
        for (uint32_t i = 0; i < size;)
        {
            unsigned char c = (unsigned char)text[i];
            uint32_t bytes = 0;
            if (c >= 0x80)
            {
                bytes = Utf8Bytes(text + i, size - i);
            }
            else if (c >= 32 && c != '"' && c != '\\')
            {
                i += PlainStringBytes(text + i, size - i, true);
                continue;
            }
            if (bytes)
            {
                i += bytes;
                continue;
            }
            if (i > start)
            {
                write(context, text + start, i - start);
            }
            const char hex[] = "0123456789abcdef";
            char escaped[] = { '\\', 'u', '0', '0', hex[c >> 4], hex[c & 15] };
            uint32_t length = sizeof(escaped);
            char short_escape = c == '"' || c == '\\' ? c :
                                c == '\b' ? 'b' : c == '\f' ? 'f' :
                                c == '\n' ? 'n' : c == '\r' ? 'r' : c == '\t' ? 't' : 0;
            if (short_escape)
            {
                escaped[1] = short_escape;
                length = 2;
            }
            write(context, escaped, length);
            start = ++i;
        }
        if (size > start)
        {
            write(context, text + start, size - start);
        }
        write(context, "\"", 1);
    }

    Document::Document(uint32_t initial_capacity)
    : m_Text(0)
    , m_Cursor(0)
    , m_End(0)
    , m_TextCapacity(0)
    , m_Valid(true)
    , m_DecodeValues(true)
    {
        assert(initial_capacity <= MAX_NODE_CAPACITY);
        m_Nodes.SetCapacity(initial_capacity);
    }
    Document::~Document()
    {
        free(m_Text);
    }
    void Document::Space()
    {
        while (m_Cursor < m_End && (*m_Cursor == ' ' || *m_Cursor == '\n' || *m_Cursor == '\r' || *m_Cursor == '\t'))
        {
            ++m_Cursor;
        }
    }
    static int Hex(char c)
    {
        if (c >= '0' && c <= '9')
        {
            return c - '0';
        }
        if (c >= 'a' && c <= 'f')
        {
            return c - 'a' + 10;
        }
        if (c >= 'A' && c <= 'F')
        {
            return c - 'A' + 10;
        }
        return -1;
    }
    static int Unicode(char* text)
    {
        int value = 0;
        for (int i = 0; i < 4; ++i)
        {
            int n = Hex(text[i]);
            if (n < 0)
            {
                return -1;
            }
            value = value * 16 + n;
        }
        return value;
    }
    char* Document::ReadString(bool decode)
    {
        if (m_Cursor == m_End || *m_Cursor++ != '"')
        {
            m_Valid = false;
            return 0;
        }
        char* begin = m_Cursor;
        char* out = begin;
        while (m_Cursor < m_End)
        {
            uint32_t plain_bytes = 0;
            while (plain_bytes < 8 && m_Cursor < m_End)
            {
                unsigned char c = (unsigned char)*m_Cursor;
                if (c < 32 || c == '"' || c == '\\')
                {
                    break;
                }
                ++m_Cursor;
                ++plain_bytes;
                if (decode)
                {
                    *out++ = c;
                }
            }
            if (plain_bytes == 8)
            {
                char* run = m_Cursor;
                while (m_End - m_Cursor >= 16 && PlainStringBlock(m_Cursor, false))
                {
                    m_Cursor += 16;
                }
                uint32_t length = (uint32_t)(m_Cursor - run);
                if (decode && length)
                {
                    if (out != run)
                    {
                        memmove(out, run, length);
                    }
                    out += length;
                }
                continue;
            }
            if (m_Cursor == m_End)
            {
                break;
            }
            unsigned char c = *m_Cursor++;
            if (c == '"')
            {
                if (decode)
                {
                    *out = 0;
                }
                return decode ? begin : 0;
            }
            if (c < 32 || m_Cursor == m_End)
            {
                break;
            }
            c = *m_Cursor++;
            if (c == 'b')
            {
                c = '\b';
            }
            else if (c == 'f')
            {
                c = '\f';
            }
            else if (c == 'n')
            {
                c = '\n';
            }
            else if (c == 'r')
            {
                c = '\r';
            }
            else if (c == 't')
            {
                c = '\t';
            }
            else if (c == 'u')
            {
                if (m_End - m_Cursor < 4)
                {
                    break;
                }
                int cp = Unicode(m_Cursor);
                m_Cursor += 4;
                // Decoded JSON values are C strings. Reject embedded NUL.
                if (cp < 0 || (decode && cp == 0) || (cp >= 0xdc00 && cp <= 0xdfff))
                {
                    break;
                }
                if (cp >= 0xd800 && cp <= 0xdbff)
                {
                    if (m_End - m_Cursor < 6 || m_Cursor[0] != '\\' || m_Cursor[1] != 'u')
                    {
                        break;
                    }
                    int low = Unicode(m_Cursor + 2);
                    if (low < 0xdc00 || low > 0xdfff)
                    {
                        break;
                    }
                    m_Cursor += 6;
                    cp = 0x10000 + ((cp - 0xd800) << 10) + low - 0xdc00;
                }
                if (!decode)
                {
                    continue;
                }
                if (cp < 0x80)
                {
                    *out++ = cp;
                }
                else
                {
                    if (cp >= 0x10000)
                    {
                        *out++ = 0xf0 | (cp >> 18);
                        *out++ = 0x80 | ((cp >> 12) & 63);
                    }
                    else if (cp >= 0x800)
                    {
                        *out++ = 0xe0 | (cp >> 12);
                    }
                    else
                    {
                        *out++ = 0xc0 | (cp >> 6);
                    }
                    if (cp >= 0x800)
                    {
                        *out++ = 0x80 | ((cp >> 6) & 63);
                    }
                    *out++ = 0x80 | (cp & 63);
                }
                continue;
            }
            else if (c != '"' && c != '\\' && c != '/')
            {
                break;
            }
            if (decode)
            {
                *out++ = c;
            }
        }
        m_Valid = false;
        return 0;
    }
    int Document::Value(int depth)
    {
        Space();
        if (depth > 64 || m_Cursor == m_End || m_Nodes.Size() == MAX_NODE_CAPACITY)
        {
            m_Valid = false;
            return -1;
        }
        Node node = {};
        node.m_First = node.m_Next = -1;
        node.m_Offset = (uint32_t)(m_Cursor - m_Text);
        if (m_Nodes.Full())
        {
            uint32_t capacity = m_Nodes.Capacity();
            capacity = capacity > MAX_NODE_CAPACITY / 2 ? MAX_NODE_CAPACITY : capacity * 2;
            m_Nodes.SetCapacity(capacity ? capacity : 32);
        }
        int id = m_Nodes.Size();
        m_Nodes.Push(node);
        char c = *m_Cursor;
        if (c == '{' || c == '[')
        {
            m_Nodes[id].m_Type = c == '{' ? TYPE_OBJECT : TYPE_ARRAY;
            char end = c == '{' ? '}' : ']';
            ++m_Cursor;
            Space();
            int                previous = -1;
            uint32_t           field_count = 0;
            dmHashTable64<int> fields;
            if (m_Cursor < m_End && *m_Cursor == end)
            {
                ++m_Cursor;
                m_Nodes[id].m_Length = (uint32_t)(m_Cursor - m_Text) - node.m_Offset;
                return id;
            }
            while (m_Valid && m_Cursor < m_End)
            {
                const char* name = 0;
                uint32_t    name_length = 0;
                uint64_t    name_hash = 0;
                if (c == '{')
                {
                    name = ReadString(true);
                    if (!m_Valid)
                    {
                        break;
                    }
                    name_length = (uint32_t)(m_Cursor - name - 1);
                    Space();
                    if (!m_Valid || m_Cursor == m_End || *m_Cursor++ != ':')
                    {
                        break;
                    }
                    // Most JSON objects are small. Index wider objects to avoid
                    // scanning all previous fields for every decoded key.
                    if (field_count == 16)
                    {
                        fields.SetCapacity(32);
                        for (int field = m_Nodes[id].m_First; field >= 0; field = m_Nodes[field].m_Next)
                        {
                            const char* key = m_Nodes[field].m_Name;
                            uint64_t    hash = dmHashBufferNoReverse64(key, (uint32_t)strlen(key));
                            if (!fields.Get(hash))
                            {
                                fields.Put(hash, field);
                            }
                        }
                    }
                    if (fields.Capacity())
                    {
                        name_hash = dmHashBufferNoReverse64(name, (uint32_t)strlen(name));
                        int* first = fields.Get(name_hash);
                        // A hash collision must not reject a different key.
                        // The first indexed field precedes every later key
                        // with that hash in this object's sibling list.
                        for (int field = first ? *first : -1; field >= 0; field = m_Nodes[field].m_Next)
                        {
                            if (dmStrEq(m_Nodes[field].m_Name, name))
                            {
                                m_Valid = false;
                                break;
                            }
                        }
                        if (!m_Valid)
                        {
                            break;
                        }
                    }
                    else if (Field(id, name) != -1)
                    {
                        break;
                    }
                }
                int child = Value(depth + 1);
                if (!m_Valid)
                {
                    break;
                }
                m_Nodes[child].m_Name = name;
                m_Nodes[child].m_NameLength = name_length;
                if (previous < 0)
                {
                    m_Nodes[id].m_First = child;
                }
                else
                {
                    m_Nodes[previous].m_Next = child;
                }
                previous = child;
                if (fields.Capacity() && !fields.Get(name_hash))
                {
                    if (fields.Full())
                    {
                        uint32_t capacity = fields.Capacity();
                        fields.SetCapacity(capacity > MAX_NODE_CAPACITY / 2 ? MAX_NODE_CAPACITY : capacity * 2);
                    }
                    fields.Put(name_hash, child);
                }
                ++field_count;
                Space();
                if (m_Cursor == m_End)
                {
                    break;
                }
                if (*m_Cursor == end)
                {
                    ++m_Cursor;
                    m_Nodes[id].m_Length = (uint32_t)(m_Cursor - m_Text) - node.m_Offset;
                    return id;
                }
                if (*m_Cursor++ != ',')
                {
                    break;
                }
                Space();
            }
            m_Valid = false;
        }
        else if (c == '"')
        {
            m_Nodes[id].m_Type = TYPE_STRING;
            m_Nodes[id].m_String = ReadString(m_DecodeValues);
        }
        else if (c == '-' || (c >= '0' && c <= '9'))
        {
            char* begin = m_Cursor;
            if (*m_Cursor == '-')
            {
                ++m_Cursor;
            }
            if (m_Cursor < m_End && *m_Cursor == '0')
            {
                ++m_Cursor;
            }
            else if (m_Cursor < m_End && *m_Cursor >= '1' && *m_Cursor <= '9')
            {
                while (m_Cursor < m_End && *m_Cursor >= '0' && *m_Cursor <= '9')
                {
                    ++m_Cursor;
                }
            }
            else
            {
                m_Valid = false;
            }
            if (m_Cursor < m_End && *m_Cursor == '.')
            {
                char* digits = ++m_Cursor;
                while (m_Cursor < m_End && *m_Cursor >= '0' && *m_Cursor <= '9')
                {
                    ++m_Cursor;
                }
                if (digits == m_Cursor)
                {
                    m_Valid = false;
                }
            }
            if (m_Cursor < m_End && (*m_Cursor == 'e' || *m_Cursor == 'E'))
            {
                ++m_Cursor;
                if (m_Cursor < m_End && (*m_Cursor == '+' || *m_Cursor == '-'))
                {
                    ++m_Cursor;
                }
                char* digits = m_Cursor;
                while (m_Cursor < m_End && *m_Cursor >= '0' && *m_Cursor <= '9')
                {
                    ++m_Cursor;
                }
                if (digits == m_Cursor)
                {
                    m_Valid = false;
                }
            }
            m_Nodes[id].m_Type = TYPE_NUMBER;
            if (m_DecodeValues)
            {
                m_Nodes[id].m_Number = ReadNumber(begin, m_Cursor);
                if (!isfinite(m_Nodes[id].m_Number))
                {
                    m_Valid = false;
                }
            }
        }
        else
        {
            const char* literal = c == 't' ? "true" : c == 'f' ? "false" :
                                  "null";
            size_t      length = strlen(literal);
            if ((size_t)(m_End - m_Cursor) < length || memcmp(m_Cursor, literal, length))
            {
                m_Valid = false;
            }
            else
            {
                m_Cursor += length;
            }
            m_Nodes[id].m_Type = c == 'n' ? TYPE_NULL : TYPE_BOOL;
            m_Nodes[id].m_Number = c == 't';
        }
        m_Nodes[id].m_Length = (uint32_t)(m_Cursor - m_Text) - node.m_Offset;
        return id;
    }
    bool Document::Parse(const char* text, uint32_t size, uint32_t* error_offset, ParseMode mode)
    {
        m_Valid = false;
        m_DecodeValues = mode == PARSE_DECODE_VALUES;
        if (error_offset)
        {
            *error_offset = 0;
        }
        // The owned buffer needs one extra byte for its terminating NUL.
        if (size == UINT32_MAX)
        {
            return false;
        }
        for (uint32_t i = 0; i < size;)
        {
            // Check four blocks together to amortize the ASCII reduction.
            if (size - i >= 64)
            {
                dsimd_uint4_t a = SimdOrUint(SimdLoadUint(text + i), SimdLoadUint(text + i + 16));
                dsimd_uint4_t b = SimdOrUint(SimdLoadUint(text + i + 32), SimdLoadUint(text + i + 48));
                dsimd_uint4_t high = SimdAndUint(SimdOrUint(a, b), SimdSplatUint(0x80808080));
                if (SimdAll(SimdEqualUint(high, SimdSplatUint(0))))
                {
                    i += 64;
                    continue;
                }
            }
            uint32_t end = size - i > 16 ? i + 16 : size;
            if (size - i >= 16)
            {
                dsimd_uint4_t bytes = SimdLoadUint(text + i);
                dsimd_uint4_t high = SimdAndUint(bytes, SimdSplatUint(0x80808080));
                if (SimdAll(SimdEqualUint(high, SimdSplatUint(0))))
                {
                    i += 16;
                    continue;
                }
            }
            // Validate a mixed block once instead of reloading it for every
            // byte. A UTF-8 sequence may extend beyond the block boundary.
            do
            {
                uint32_t bytes = Utf8Bytes(text + i, size - i);
                if (!bytes)
                {
                    if (error_offset)
                    {
                        *error_offset = i;
                    }
                    return false;
                }
                i += bytes;
            } while (i < end);
        }
        m_Nodes.SetSize(0);
        if (size + 1 > m_TextCapacity)
        {
            uint32_t capacity = m_TextCapacity <= UINT32_MAX / 2 ? m_TextCapacity * 2 : size + 1;
            if (capacity < size + 1)
            {
                capacity = size + 1;
            }
            char* text_buffer = (char*)realloc(m_Text, capacity);
            if (!text_buffer)
            {
                return false;
            }
            m_Text = text_buffer;
            m_TextCapacity = capacity;
        }
        memcpy(m_Text, text, size);
        m_Text[size] = 0;
        m_Cursor = m_Text;
        m_End = m_Text + size;
        m_Valid = true;
        Value(0);
        Space();
        if (error_offset)
        {
            *error_offset = (uint32_t)(m_Cursor - m_Text);
        }
        return m_Valid && m_Cursor == m_End;
    }
    const Node& Document::Get(int node) const
    {
        static const Node missing = { TYPE_NULL, 0, { -1 }, -1, 0, 0, 0 };
        return node >= 0 && node < (int)m_Nodes.Size() ? m_Nodes[node] : missing;
    }
    int Document::Field(int object, const char* name) const
    {
        if (Get(object).m_Type != TYPE_OBJECT)
        {
            return -1;
        }
        for (int i = Get(object).m_First; i >= 0; i = Get(i).m_Next)
        {
            if (Get(i).m_Name && dmStrEq(Get(i).m_Name, name))
            {
                return i;
            }
        }
        return -1;
    }
    const char* Document::String(int node, const char* fallback) const
    {
        return m_DecodeValues && Get(node).m_Type == TYPE_STRING ? Get(node).m_String : fallback;
    }
    int Document::Integer(int node, int fallback) const
    {
        const Node& value = Get(node);
        if (!m_DecodeValues || value.m_Type != TYPE_NUMBER)
        {
            return fallback;
        }
        double n = value.m_Number;
        return n >= INT_MIN && n <= INT_MAX && floor(n) == n ? (int)n : fallback;
    }
    bool Document::UnsignedInteger(int node, uint32_t* value) const
    {
        const Node& n = Get(node);
        if (!m_DecodeValues || n.m_Type != TYPE_NUMBER || n.m_Number < 0 || n.m_Number > UINT32_MAX || floor(n.m_Number) != n.m_Number)
        {
            return false;
        }
        *value = (uint32_t)n.m_Number;
        return true;
    }
    bool Document::Boolean(int node, bool fallback) const
    {
        return Get(node).m_Type == TYPE_BOOL ? Get(node).m_Number != 0 : fallback;
    }

}
