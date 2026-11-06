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

#ifndef DM_JSON_H
#define DM_JSON_H

#include <dlib/array.h>
#include <stdint.h>

namespace dmJson
{
    const uint32_t MAX_DOCUMENT_SIZE = 1024 * 1024;
    const uint32_t MAX_NODES = 65536;

    // Length of the next valid UTF-8 sequence, or zero. Input must be nonempty.
    uint32_t Utf8Bytes(const char* text, uint32_t size);

    typedef void (*FWrite)(void* context, const char* text, uint32_t size);
    // Emit a quoted JSON string. Invalid UTF-8 bytes are escaped as U+00xx.
    void WriteString(const char* text, uint32_t size, FWrite write, void* context);

    enum Type
    {
        TYPE_NULL,
        TYPE_OBJECT,
        TYPE_ARRAY,
        TYPE_STRING,
        TYPE_NUMBER,
        TYPE_BOOL
    };
    enum ParseMode
    {
        PARSE_DECODE_VALUES,
        // Validate string and number values without decoding them. Their encoded
        // spans remain in m_Text; String/Integer/UnsignedInteger return fallbacks.
        // Object keys are always decoded and must not contain embedded NUL.
        PARSE_PRESERVE_VALUES
    };
    struct Node
    {
        Type        m_Type;
        const char* m_Name;
        // Only the member selected by m_Type is valid. Containers use m_First.
        union
        {
            int         m_First;
            const char* m_String;
            double      m_Number;
        };
        int         m_Next;
        // Encoded value span in the original input, before string decoding.
        uint32_t    m_Offset;
        uint32_t    m_Length;
        // Encoded field-name length, excluding quotes. m_Name retains its source offset.
        uint32_t    m_NameLength;
    };

    // A small, bounded JSON document. Strings are decoded in the owned buffer;
    // text and node storage are reused between parses. Node indices stay valid
    // when the node array grows.
    struct Document
    {
        dmArray<Node>     m_Nodes;
        char*             m_Text;
        char*             m_Cursor;
        char*             m_End;
        uint32_t          m_TextCapacity; // Allocated bytes, including the terminating NUL.
        bool              m_Valid;
        bool              m_DecodeValues;
        // Zero capacity defers node allocation until parsing the first value.
        Document(uint32_t initial_capacity = 32);
        ~Document();
        // Parsing a new document invalidates previous node indices and string pointers.
        bool            Parse(const char* text, uint32_t size, uint32_t* error_offset = 0, ParseMode mode = PARSE_DECODE_VALUES);
        int             Field(int object, const char* name) const;
        const Node&     Get(int node) const;
        const char*     String(int node, const char* fallback = 0) const;
        int             Integer(int node, int fallback = -1) const;
        bool            UnsignedInteger(int node, uint32_t* value) const;
        bool            Boolean(int node, bool fallback = false) const;

        private:
        void  Space();
        char* ReadString(bool decode);
        int   Value(int depth);
    };

}

#endif
