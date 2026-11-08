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
#include <dlib/utf8.h>
#include <simd/simd.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <limits.h>
#include "json_lexer.h"

namespace dmJson
{
    // dmArray allocates with a uint32_t byte count. Node indices also fit in int.
    static const uint32_t MAX_NODE_CAPACITY = UINT32_MAX / sizeof(Node);

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
                bytes = dmUtf8::ValidateSequence(text + i, size - i);
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
    char* Document::ReadString(bool decode)
    {
        Lexer lexer(m_Cursor, m_End);
        char* result = lexer.ReadString(decode);
        m_Cursor = lexer.m_Cursor;
        m_Valid = lexer.m_Valid;
        return decode ? result : 0;
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
        node.m_Container.m_First = node.m_Next = -1;
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
                        for (int field = m_Nodes[id].m_Container.m_First; field >= 0; field = m_Nodes[field].m_Next)
                        {
                            const char* key = m_Nodes[field].Name(m_Text);
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
                            if (dmStrEq(m_Nodes[field].Name(m_Text), name))
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
                m_Nodes[child].m_NameOffset = name ? (uint32_t)(name - m_Text) : 0;
                m_Nodes[child].m_NameLength = name_length;
                if (previous < 0)
                {
                    m_Nodes[id].m_Container.m_First = child;
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
                    m_Nodes[id].m_Container.m_Count = field_count;
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
            Lexer lexer(m_Cursor, m_End);
            m_Nodes[id].m_Type = TYPE_NUMBER;
            m_Nodes[id].m_Number = lexer.ReadNumber(m_DecodeValues);
            m_Cursor = lexer.m_Cursor;
            m_Valid = lexer.m_Valid;
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
        uint32_t valid_bytes = dmUtf8::Validate(text, size);
        if (valid_bytes != size)
        {
            if (error_offset)
            {
                *error_offset = valid_bytes;
            }
            return false;
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
        static const Node missing = { TYPE_NULL, 0, { { -1, 0 } }, -1, 0, 0, 0 };
        return node >= 0 && node < (int)m_Nodes.Size() ? m_Nodes[node] : missing;
    }
    int Document::Field(int object, const char* name) const
    {
        if (Get(object).m_Type != TYPE_OBJECT)
        {
            return -1;
        }
        for (int i = Get(object).m_Container.m_First; i >= 0; i = Get(i).m_Next)
        {
            if (Get(i).Name(m_Text) && dmStrEq(Get(i).Name(m_Text), name))
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
