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

#ifndef DM_JSON_LEXER_H
#define DM_JSON_LEXER_H
#include <stdint.h>
#include <math.h>
#include <stdlib.h>
#include <simd/simd.h>
#include "json_number.h"

namespace dmJson
{
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

    static int Unicode(char* text)
    {
        static const unsigned char hex[256] = {
            255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
            255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
            255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
            0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 255, 255, 255, 255, 255, 255,
            255, 10, 11, 12, 13, 14, 15, 255, 255, 255, 255, 255, 255, 255, 255, 255,
            255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
            255, 10, 11, 12, 13, 14, 15, 255, 255, 255, 255, 255, 255, 255, 255, 255,
            255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
            255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
            255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
            255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
            255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
            255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
            255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
            255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
            255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
        };
        uint32_t a = hex[(unsigned char)text[0]];
        uint32_t b = hex[(unsigned char)text[1]];
        uint32_t c = hex[(unsigned char)text[2]];
        uint32_t d = hex[(unsigned char)text[3]];
        return (a | b | c | d) & 128 ? -1 : (int)((a << 12) | (b << 8) | (c << 4) | d);
    }
    struct StringBuffer
    {
        char*    m_Data;
        uint32_t m_Capacity;
        bool     m_Failed;
        StringBuffer() : m_Data(0), m_Capacity(0), m_Failed(false)
        {
        }
        ~StringBuffer()
        {
            free(m_Data);
        }
        bool Reserve(uint32_t size)
        {
            if (size <= m_Capacity)
            {
                return true;
            }
            uint32_t capacity = m_Capacity <= UINT32_MAX / 2 ? m_Capacity * 2 : size;
            if (capacity < 256)
            {
                capacity = 256;
            }
            if (capacity < size)
            {
                capacity = size;
            }
            char* data = (char*)realloc(m_Data, capacity);
            if (!data)
            {
                m_Failed = true;
                return false;
            }
            m_Data = data;
            m_Capacity = capacity;
            return true;
        }
    };

    struct Lexer
    {
        char*    m_Cursor;
        char*    m_End;
        uint32_t m_StringLength;
        bool     m_Valid;
        bool     m_Escaped;
        bool     m_AllowNul;
        bool     m_AllowControl;
        bool     m_RejectOverflow;
        Lexer(char* cursor, char* end) : m_Cursor(cursor), m_End(end), m_StringLength(0), m_Valid(true), m_Escaped(false), m_AllowNul(false), m_AllowControl(false), m_RejectOverflow(true)
        {
        }
        void Space()
        {
            while (m_Cursor < m_End && (*m_Cursor == ' ' || *m_Cursor == '\n' || *m_Cursor == '\r' || *m_Cursor == '\t'))
            {
                ++m_Cursor;
            }
        }
        // Specialize validation, in-place decoding and read-only buffered
        // decoding so their mode checks stay outside the string loop.
        template <bool decode, bool buffered>
        char* ReadStringInternal(StringBuffer* buffer)
        {
            char* cursor = m_Cursor;
            if (cursor == m_End || *cursor++ != '"')
            {
                m_Cursor = cursor;
                m_Valid = false;
                return 0;
            }
            m_Escaped = false;
            char* begin = cursor;
            char* out = begin;
            char* limit = m_End;
            while (cursor < m_End)
            {
                uint32_t plain_bytes = 0;
                if (decode && buffered && m_Escaped)
                {
                    if (cursor >= limit)
                    {
                        uint32_t used = (uint32_t)(out - buffer->m_Data);
                        if (used > UINT32_MAX - 4 || !buffer->Reserve(used + 4))
                        {
                            break;
                        }
                        out = buffer->m_Data + used;
                        // Decoding never expands the input. Three spare bytes
                        // cover a four-byte escape beginning just before limit.
                        uint32_t available = buffer->m_Capacity - used - 3;
                        uint32_t remaining = (uint32_t)(m_End - cursor);
                        limit = cursor + (available < remaining ? available : remaining);
                    }
                    while (plain_bytes < 8 && cursor < limit)
                    {
                        unsigned char c = (unsigned char)*cursor;
                        if (c < 32 || c == '"' || c == '\\')
                        {
                            break;
                        }
                        *out++ = c;
                        ++cursor;
                        ++plain_bytes;
                    }
                    if (plain_bytes == 8)
                    {
                        while (limit - cursor >= 16 && PlainStringBlock(cursor, false))
                        {
                            memcpy(out, cursor, 16);
                            out += 16;
                            cursor += 16;
                        }
                    }
                    if (cursor == limit)
                    {
                        continue;
                    }
                }
                else
                {
                    char* run = cursor;
                    while (plain_bytes < 8 && cursor < m_End)
                    {
                        unsigned char c = (unsigned char)*cursor;
                        if (c < 32 || c == '"' || c == '\\')
                        {
                            break;
                        }
                        ++cursor;
                        ++plain_bytes;
                    }
                    if (plain_bytes == 8)
                    {
                        while (m_End - cursor >= 16 && PlainStringBlock(cursor, false))
                        {
                            cursor += 16;
                        }
                    }
                    uint32_t length = (uint32_t)(cursor - run);
                    if (decode)
                    {
                        if (out != run && length)
                        {
                            memmove(out, run, length);
                        }
                        out += length;
                    }
                }
                if (plain_bytes == 8)
                {
                    continue;
                }
                if (cursor == m_End)
                {
                    break;
                }
                unsigned char c = *cursor++;
                if (c == '"')
                {
                    char* result = decode && buffered && m_Escaped ? buffer->m_Data : begin;
                    m_StringLength = (uint32_t)((decode ? out : cursor - 1) - result);
                    if (decode && !buffered)
                    {
                        *out = 0;
                    }
                    m_Cursor = cursor;
                    return result;
                }
                if (c < 32 || cursor == m_End)
                {
                    if (c && c < 32 && m_AllowControl)
                    {
                        if (decode)
                        {
                            // Escaped strings have four spare output bytes;
                            // otherwise this byte is already in the input.
                            if (out != cursor - 1)
                            {
                                *out = c;
                            }
                            ++out;
                        }
                        continue;
                    }
                    break;
                }
                if (decode && buffered && !m_Escaped)
                {
                    uint32_t prefix = (uint32_t)(out - begin);
                    if (prefix > UINT32_MAX - 4 || !buffer->Reserve(prefix + 4))
                    {
                        break;
                    }
                    memcpy(buffer->m_Data, begin, prefix);
                    out = buffer->m_Data + prefix;
                    limit = cursor;
                }
                m_Escaped = true;
                c = *cursor++;
                if (c == 'u')
                {
                decode_unicode:
                    if (m_End - cursor < 4)
                    {
                        break;
                    }
                    int cp = Unicode(cursor);
                    cursor += 4;
                    // Decoded JSON values are C strings. Reject embedded NUL.
                    if (cp < 0 || (decode && !m_AllowNul && cp == 0) || (cp >= 0xdc00 && cp <= 0xdfff))
                    {
                        break;
                    }
                    if (cp >= 0xd800 && cp <= 0xdbff)
                    {
                        if (m_End - cursor < 6 || cursor[0] != '\\' || cursor[1] != 'u')
                        {
                            break;
                        }
                        int low = Unicode(cursor + 2);
                        if (low < 0xdc00 || low > 0xdfff)
                        {
                            break;
                        }
                        cursor += 6;
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
                    // Consecutive Unicode escapes need no ordinary-text scan.
                    if (m_End - cursor >= 2 && cursor[0] == '\\' && cursor[1] == 'u')
                    {
                        if (buffered && cursor >= limit)
                        {
                            continue;
                        }
                        cursor += 2;
                        goto decode_unicode;
                    }
                    continue;
                }
                static const unsigned char simple[256] = {
                    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                    0, 0, 34, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 47,
                    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 92, 0, 0, 0,
                    0, 0, 8, 0, 0, 0, 12, 0, 0, 0, 0, 0, 0, 0, 10, 0,
                    0, 0, 13, 0, 9, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                };
                c = simple[c];
                if (!c)
                {
                    break;
                }
                if (decode)
                {
                    *out++ = c;
                }
            }
            m_Cursor = cursor;
            m_Valid = false;
            return 0;
        }
        char* ReadString(bool decode, StringBuffer* buffer = 0)
        {
            if (!decode)
            {
                return ReadStringInternal<false, false>(0);
            }
            if (buffer)
            {
                return ReadStringInternal<true, true>(buffer);
            }
            return ReadStringInternal<true, false>(0);
        }
        // Lua retains leading zeros, a leading plus, and incomplete fractions.
        // Document callers use the strict grammar by default.
        double ReadNumber(bool decode, bool relaxed = false)
        {
            double value = 0;
            char* begin = m_Cursor;
            uint64_t coefficient = 0;
            if (*m_Cursor == '-' || (relaxed && *m_Cursor == '+'))
            {
                ++m_Cursor;
            }
            char* integer_begin = m_Cursor;
            if (!relaxed && m_Cursor < m_End && *m_Cursor == '0')
            {
                ++m_Cursor;
            }
            else if (m_Cursor < m_End && *m_Cursor >= (relaxed ? '0' : '1') && *m_Cursor <= '9')
            {
                while (m_Cursor < m_End && *m_Cursor >= '0' && *m_Cursor <= '9')
                {
                    // Unsigned wrap is defined; only <=19 digits use this accumulator.
                    coefficient = coefficient * 10 + (uint32_t)(*m_Cursor - '0');
                    ++m_Cursor;
                }
            }
            else if (!relaxed || m_Cursor == m_End || *m_Cursor != '.')
            {
                m_Valid = false;
            }
            uint32_t digits = (uint32_t)(m_Cursor - integer_begin);
            int64_t exponent = 0;
            if (m_Cursor < m_End && *m_Cursor == '.')
            {
                char* fraction = ++m_Cursor;
                while (m_Cursor < m_End && *m_Cursor >= '0' && *m_Cursor <= '9')
                {
                    coefficient = coefficient * 10 + (uint32_t)(*m_Cursor - '0');
                    ++m_Cursor;
                }
                if (fraction == m_Cursor && (!relaxed || !digits))
                {
                    m_Valid = false;
                }
                uint32_t fraction_digits = (uint32_t)(m_Cursor - fraction);
                digits += fraction_digits;
                exponent = -(int64_t)fraction_digits;
            }
            if (m_Cursor < m_End && (*m_Cursor == 'e' || *m_Cursor == 'E'))
            {
                char* marker = m_Cursor;
                ++m_Cursor;
                bool negative_exponent = m_Cursor < m_End && *m_Cursor == '-';
                if (m_Cursor < m_End && (*m_Cursor == '+' || *m_Cursor == '-'))
                {
                    ++m_Cursor;
                }
                char* exponent_begin = m_Cursor;
                int64_t explicit_exponent = 0;
                while (m_Cursor < m_End && *m_Cursor >= '0' && *m_Cursor <= '9')
                {
                    // Larger than any possible mantissa length, without signed overflow.
                    if (explicit_exponent < INT64_C(10000000000))
                    {
                        explicit_exponent = explicit_exponent * 10 + (*m_Cursor - '0');
                    }
                    ++m_Cursor;
                }
                if (exponent_begin == m_Cursor)
                {
                    if (relaxed)
                    {
                        m_Cursor = marker;
                    }
                    else
                    {
                        m_Valid = false;
                    }
                }
                exponent += negative_exponent ? -explicit_exponent : explicit_exponent;
            }
            if (decode && m_Valid)
            {
                if (digits <= 19 && exponent == 0)
                {
                    value = (double)coefficient;
                    value = *begin == '-' ? -value : value;
                }
                else
                {
                    value = ReadDecimal(*begin == '+' ? begin + 1 : begin, m_Cursor, coefficient, digits, exponent);
                }
                if (m_RejectOverflow && !isfinite(value))
                {
                    m_Valid = false;
                }
            }
            return value;
        }
    };
}
#endif
