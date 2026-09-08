// Copyright 2020-2026 The Defold Foundation
// Licensed under the Defold License version 1.0. See https://www.defold.com/license

#if !defined(DM_RELEASE)
#include "dap.h"
#include <dlib/dstrings.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <limits.h>

namespace dmDebugger
{
    static uint32_t Utf8Bytes(const char* text, uint32_t size)
    {
        unsigned char first = (unsigned char)text[0];
        if (first < 0x80)
            return 1;
        uint32_t count = first >= 0xf0 ? 4 : first >= 0xe0 ? 3 :
                                                             2;
        if (first < 0xc2 || first > 0xf4 || size < count)
            return 0;
        uint32_t cp = first & (0x7f >> count);
        for (uint32_t i = 1; i < count; ++i)
        {
            unsigned char c = text[i];
            if ((c & 0xc0) != 0x80)
                return 0;
            cp = (cp << 6) | (c & 63);
        }
        if ((count == 3 && cp < 0x800) || (count == 4 && cp < 0x10000) || cp > 0x10ffff || (cp >= 0xd800 && cp <= 0xdfff))
            return 0;
        return count;
    }
    Buffer::Buffer()
        : m_Valid(true)
    {
    }
    void Buffer::Clear()
    {
        m_Data.SetSize(0);
        m_Valid = true;
    }
    uint32_t Buffer::Size() const
    {
        return m_Data.Size();
    }
    const char* Buffer::Data() const
    {
        return m_Data.Size() ? m_Data.Begin() : "";
    }
    void Buffer::Consume(uint32_t size)
    {
        memmove(m_Data.Begin(), m_Data.Begin() + size, Size() - size);
        m_Data.SetSize(Size() - size);
    }
    void Buffer::Add(const char* data, uint32_t size)
    {
        if (!m_Valid || size > MAX_QUEUE_SIZE - Size())
        {
            m_Valid = false;
            return;
        }
        uint32_t old = Size();
        if (m_Data.Capacity() < old + size + 1)
            m_Data.SetCapacity(old + size + 1024);
        m_Data.SetSize(old + size);
        memcpy(m_Data.Begin() + old, data, size);
        m_Data.Begin()[Size()] = 0;
    }
    void Buffer::Add(const char* text)
    {
        Add(text, (uint32_t)strlen(text));
    }
    void Buffer::Format(const char* format, ...)
    {
        char    text[256];
        va_list args;
        va_start(args, format);
        int size = vsnprintf(text, sizeof(text), format, args);
        va_end(args);
        if (size < 0 || size >= (int)sizeof(text))
            m_Valid = false;
        else
            Add(text, size);
    }
    void Buffer::String(const char* text)
    {
        String(text ? text : "", text ? (uint32_t)strlen(text) : 0);
    }
    void Buffer::String(const char* text, uint32_t size)
    {
        Add("\"");
        for (uint32_t i = 0; i < size; ++i)
        {
            unsigned char c = text[i];
            if (c == '"' || c == '\\')
            {
                Add("\\");
                Add(text + i, 1);
            }
            else if (c == '\n')
                Add("\\n");
            else if (c == '\r')
                Add("\\r");
            else if (c == '\t')
                Add("\\t");
            else if (c < 32)
                Format("\\u%04x", c);
            else
            {
                uint32_t bytes = Utf8Bytes(text + i, size - i);
                if (bytes)
                {
                    Add(text + i, bytes);
                    i += bytes - 1;
                }
                else
                    Format("\\u%04x", c); // Lua strings may contain arbitrary bytes.
            }
        }
        Add("\"");
    }

    Json::Json()
        : m_Text(0)
        , m_Cursor(0)
        , m_End(0)
        , m_Valid(true)
    {
    }
    Json::~Json()
    {
        free(m_Text);
    }
    void Json::Space()
    {
        while (m_Cursor < m_End && (*m_Cursor == ' ' || *m_Cursor == '\n' || *m_Cursor == '\r' || *m_Cursor == '\t'))
            ++m_Cursor;
    }
    static int Hex(char c)
    {
        if (c >= '0' && c <= '9')
            return c - '0';
        if (c >= 'a' && c <= 'f')
            return c - 'a' + 10;
        if (c >= 'A' && c <= 'F')
            return c - 'A' + 10;
        return -1;
    }
    static int Unicode(char* text)
    {
        int value = 0;
        for (int i = 0; i < 4; ++i)
        {
            int n = Hex(text[i]);
            if (n < 0)
                return -1;
            value = value * 16 + n;
        }
        return value;
    }
    char* Json::ReadString()
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
            unsigned char c = *m_Cursor++;
            if (c == '"')
            {
                *out = 0;
                return begin;
            }
            if (c < 32)
                break;
            if (c != '\\')
            {
                *out++ = c;
                continue;
            }
            if (m_Cursor == m_End)
                break;
            c = *m_Cursor++;
            if (c == '"' || c == '\\' || c == '/')
                *out++ = c;
            else if (c == 'b')
                *out++ = '\b';
            else if (c == 'f')
                *out++ = '\f';
            else if (c == 'n')
                *out++ = '\n';
            else if (c == 'r')
                *out++ = '\r';
            else if (c == 't')
                *out++ = '\t';
            else if (c == 'u')
            {
                if (m_End - m_Cursor < 4)
                    break;
                int cp = Unicode(m_Cursor);
                m_Cursor += 4;
                // DAP identifiers/expressions are C strings. Reject embedded NUL.
                if (cp <= 0 || (cp >= 0xdc00 && cp <= 0xdfff))
                    break;
                if (cp >= 0xd800 && cp <= 0xdbff)
                {
                    if (m_End - m_Cursor < 6 || m_Cursor[0] != '\\' || m_Cursor[1] != 'u')
                        break;
                    int low = Unicode(m_Cursor + 2);
                    if (low < 0xdc00 || low > 0xdfff)
                        break;
                    m_Cursor += 6;
                    cp = 0x10000 + ((cp - 0xd800) << 10) + low - 0xdc00;
                }
                if (cp < 0x80)
                    *out++ = cp;
                else
                {
                    if (cp >= 0x10000)
                    {
                        *out++ = 0xf0 | (cp >> 18);
                        *out++ = 0x80 | ((cp >> 12) & 63);
                    }
                    else if (cp >= 0x800)
                        *out++ = 0xe0 | (cp >> 12);
                    else
                        *out++ = 0xc0 | (cp >> 6);
                    if (cp >= 0x800)
                        *out++ = 0x80 | ((cp >> 6) & 63);
                    *out++ = 0x80 | (cp & 63);
                }
            }
            else
                break;
        }
        m_Valid = false;
        return 0;
    }
    int Json::Value(int depth)
    {
        Space();
        if (depth > 64 || m_Cursor == m_End || m_Nodes.Size() >= 65536)
        {
            m_Valid = false;
            return -1;
        }
        JsonNode node = {};
        node.m_First = node.m_Next = -1;
        if (m_Nodes.Full())
            m_Nodes.OffsetCapacity(64);
        int id = m_Nodes.Size();
        m_Nodes.Push(node);
        char c = *m_Cursor;
        if (c == '{' || c == '[')
        {
            m_Nodes[id].m_Type = c == '{' ? JSON_OBJECT : JSON_ARRAY;
            char end = c == '{' ? '}' : ']';
            ++m_Cursor;
            Space();
            int previous = -1;
            if (m_Cursor < m_End && *m_Cursor == end)
            {
                ++m_Cursor;
                return id;
            }
            while (m_Valid && m_Cursor < m_End)
            {
                const char* name = 0;
                if (c == '{')
                {
                    name = ReadString();
                    Space();
                    if (!m_Valid || m_Cursor == m_End || *m_Cursor++ != ':')
                        break;
                    if (Field(id, name) != -1)
                        break;
                }
                int child = Value(depth + 1);
                if (!m_Valid)
                    break;
                m_Nodes[child].m_Name = name;
                if (previous < 0)
                    m_Nodes[id].m_First = child;
                else
                    m_Nodes[previous].m_Next = child;
                previous = child;
                Space();
                if (m_Cursor == m_End)
                    break;
                if (*m_Cursor == end)
                {
                    ++m_Cursor;
                    return id;
                }
                if (*m_Cursor++ != ',')
                    break;
                Space();
            }
            m_Valid = false;
        }
        else if (c == '"')
        {
            m_Nodes[id].m_Type = JSON_STRING;
            m_Nodes[id].m_String = ReadString();
        }
        else if (c == '-' || (c >= '0' && c <= '9'))
        {
            char* begin = m_Cursor;
            if (*m_Cursor == '-')
                ++m_Cursor;
            if (m_Cursor < m_End && *m_Cursor == '0')
                ++m_Cursor;
            else if (m_Cursor < m_End && *m_Cursor >= '1' && *m_Cursor <= '9')
                while (m_Cursor < m_End && *m_Cursor >= '0' && *m_Cursor <= '9')
                    ++m_Cursor;
            else
                m_Valid = false;
            if (m_Cursor < m_End && *m_Cursor == '.')
            {
                char* digits = ++m_Cursor;
                while (m_Cursor < m_End && *m_Cursor >= '0' && *m_Cursor <= '9')
                    ++m_Cursor;
                if (digits == m_Cursor)
                    m_Valid = false;
            }
            if (m_Cursor < m_End && (*m_Cursor == 'e' || *m_Cursor == 'E'))
            {
                ++m_Cursor;
                if (m_Cursor < m_End && (*m_Cursor == '+' || *m_Cursor == '-'))
                    ++m_Cursor;
                char* digits = m_Cursor;
                while (m_Cursor < m_End && *m_Cursor >= '0' && *m_Cursor <= '9')
                    ++m_Cursor;
                if (digits == m_Cursor)
                    m_Valid = false;
            }
            m_Nodes[id].m_Type = JSON_NUMBER;
            m_Nodes[id].m_Number = strtod(begin, 0);
            if (!isfinite(m_Nodes[id].m_Number))
                m_Valid = false;
        }
        else
        {
            const char* literal = c == 't' ? "true" : c == 'f' ? "false" :
                                                                 "null";
            size_t      length = strlen(literal);
            if ((size_t)(m_End - m_Cursor) < length || memcmp(m_Cursor, literal, length))
                m_Valid = false;
            else
                m_Cursor += length;
            m_Nodes[id].m_Type = c == 'n' ? JSON_NULL : JSON_BOOL;
            m_Nodes[id].m_Number = c == 't';
        }
        return id;
    }
    bool Json::Parse(const char* text, uint32_t size)
    {
        if (size > MAX_MESSAGE_SIZE)
            return false;
        for (uint32_t i = 0; i < size;)
        {
            uint32_t bytes = Utf8Bytes(text + i, size - i);
            if (!bytes)
                return false;
            i += bytes;
        }
        free(m_Text);
        m_Nodes.SetSize(0);
        m_Text = (char*)malloc(size + 1);
        memcpy(m_Text, text, size);
        m_Text[size] = 0;
        m_Cursor = m_Text;
        m_End = m_Text + size;
        m_Valid = true;
        if (m_Valid)
            Value(0);
        Space();
        return m_Valid && m_Cursor == m_End;
    }
    const JsonNode& Json::Get(int node) const
    {
        static const JsonNode missing = { JSON_NULL, 0, 0, 0, -1, -1 };
        return node >= 0 && node < (int)m_Nodes.Size() ? m_Nodes[node] : missing;
    }
    int Json::Field(int object, const char* name) const
    {
        if (Get(object).m_Type != JSON_OBJECT)
            return -1;
        for (int i = Get(object).m_First; i >= 0; i = Get(i).m_Next)
            if (Get(i).m_Name && !strcmp(Get(i).m_Name, name))
                return i;
        return -1;
    }
    const char* Json::String(int node, const char* fallback) const
    {
        return Get(node).m_Type == JSON_STRING ? Get(node).m_String : fallback;
    }
    int Json::Integer(int node, int fallback) const
    {
        double n = Get(node).m_Number;
        return Get(node).m_Type == JSON_NUMBER && n >= INT_MIN && n <= INT_MAX && floor(n) == n ? (int)n : fallback;
    }
    bool Json::Boolean(int node, bool fallback) const
    {
        return Get(node).m_Type == JSON_BOOL ? Get(node).m_Number != 0 : fallback;
    }

    int ReadFrame(const Buffer& input, uint32_t* offset, uint32_t* size)
    {
        const char* p = input.Data();
        uint32_t    end = 0;
        while (end + 3 < input.Size() && end < 4096)
        {
            if (!memcmp(p + end, "\r\n\r\n", 4))
                break;
            ++end;
        }
        if (end >= 4096)
            return -1;
        if (end + 3 >= input.Size())
            return 0;
        uint32_t length = 0;
        bool     found = false;
        for (uint32_t line = 0; line <= end;)
        {
            uint32_t stop = line;
            while (stop <= end && memcmp(p + stop, "\r\n", 2))
                ++stop;
            if (stop > end)
                return -1;
            const char* key = "Content-Length:";
            if (stop - line >= strlen(key) && !memcmp(p + line, key, strlen(key)))
            {
                if (found)
                    return -1;
                found = true;
                uint32_t n = line + (uint32_t)strlen(key);
                while (n < stop && p[n] == ' ')
                    ++n;
                if (n == stop)
                    return -1;
                for (; n < stop; ++n)
                {
                    if (p[n] < '0' || p[n] > '9')
                        return -1;
                    length = length * 10 + p[n] - '0';
                    if (length > MAX_MESSAGE_SIZE)
                        return -1;
                }
            }
            else
            {
                // Permit optional extension headers, but require a header field.
                if (!memchr(p + line, ':', stop - line))
                    return -1;
            }
            line = stop + 2;
        }
        if (!found || !length)
            return -1;
        *offset = end + 4;
        *size = length;
        return input.Size() - *offset >= length ? 1 : 0;
    }
} // namespace dmDebugger
#endif
