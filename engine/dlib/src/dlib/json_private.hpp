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

#ifndef DM_JSON_PRIVATE_HPP
#define DM_JSON_PRIVATE_HPP

#include "json.h"
#include <dlib/array.h>

// Parser storage is private to the implementation and allocation tests.
struct JsonDocument
{
    dmArray<JsonNode> m_Nodes;
    char*             m_Text;
    char*             m_Cursor;
    char*             m_End;
    uint32_t          m_TextCapacity;
    bool              m_Valid;
    bool              m_DecodeValues;

    JsonDocument(uint32_t initial_capacity);
    ~JsonDocument();
    JsonResult      Parse(const char* text, uint32_t size, uint32_t* error_offset, JsonParseMode mode);
    const JsonNode& Get(int node) const;
    const char*     Name(int node) const;
    int             Field(int object, const char* name) const;
    const char*     String(int node, const char* fallback) const;
    int             Integer(int node, int fallback) const;
    bool            UnsignedInteger(int node, uint32_t* value) const;
    bool            Boolean(int node, bool fallback) const;

    private:
    // Cursors enter Value() after whitespace. Text bounds stay fixed while
    // recursive values may grow node storage.
    char* ReadString(bool decode, char*& cursor, char* limit);
    int   Value(int depth, char*& cursor, char* limit, char* text);
    template <bool object>
    int   Container(int depth, int id, uint32_t offset, char*& cursor, char* limit, char* text);
};

#endif // DM_JSON_PRIVATE_HPP
