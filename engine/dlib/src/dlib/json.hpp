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

#ifndef DM_JSON_HPP
#define DM_JSON_HPP

#include "json_gen.hpp"

namespace dmJson
{
    // Own a C document for the duration of this scope. Handles returned by
    // Handle() are borrowed; they must not be destroyed independently.
    struct Document
    {
        explicit Document(uint32_t initial_capacity)
        : m_Document(JsonCreate(initial_capacity))
        {
        }

        ~Document()
        {
            JsonDestroy(m_Document);
        }

        bool Parse(const char* text, uint32_t size, uint32_t* error_offset, ParseMode mode)
        {
            return JsonParse(m_Document, text, size, error_offset, mode) == RESULT_OK;
        }
        HDocument Handle() const
        {
            return m_Document;
        }
        const Node& Get(int node) const
        {
            return *JsonGetNode(m_Document, node);
        }
        uint32_t NodeCount() const
        {
            return JsonGetNodeCount(m_Document);
        }
        const char* Text() const
        {
            return JsonGetText(m_Document);
        }
        const char* Name(int node) const
        {
            return JsonGetName(m_Document, node);
        }
        int Field(int object, const char* name) const
        {
            return JsonField(m_Document, object, name);
        }
        const char* String(int node, const char* fallback) const
        {
            return JsonString(m_Document, node, fallback);
        }
        int Integer(int node, int fallback) const
        {
            return JsonInteger(m_Document, node, fallback);
        }
        bool UnsignedInteger(int node, uint32_t* value) const
        {
            return JsonUnsignedInteger(m_Document, node, value);
        }
        bool Boolean(int node, bool fallback) const
        {
            return JsonBoolean(m_Document, node, fallback);
        }

        private:
        Document(const Document&);
        Document& operator=(const Document&);
        HDocument m_Document;
    };
}

#endif // DM_JSON_HPP
