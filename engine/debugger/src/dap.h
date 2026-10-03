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

#ifndef DM_DEBUGGER_DAP_H
#define DM_DEBUGGER_DAP_H

#include <dlib/array.h>
#include <stdint.h>

namespace dmDebugger
{
    // DAP messages and queues have explicit bounds, including while the client
    // stops reading. No unbounded allocations are driven by Content-Length.
    const uint32_t MAX_MESSAGE_SIZE = 1024 * 1024;
    const uint32_t MAX_QUEUE_SIZE = 4 * MAX_MESSAGE_SIZE;

    uint32_t       Utf8Bytes(const char* text, uint32_t size);

    struct Buffer
    {
        dmArray<char> m_Data;
        bool          m_Valid;
        Buffer();
        void        Clear();
        uint32_t    Size() const;
        const char* Data() const;
        void        Consume(uint32_t size);
        void        Add(const char* data, uint32_t size);
        void        Add(const char* text);
        void        Format(const char* format, ...);
        void        String(const char* text);
        void        String(const char* text, uint32_t size);
    };

    enum JsonType
    {
        JSON_NULL,
        JSON_OBJECT,
        JSON_ARRAY,
        JSON_STRING,
        JSON_NUMBER,
        JSON_BOOL
    };
    struct JsonNode
    {
        JsonType    m_Type;
        const char* m_Name;
        const char* m_String;
        double      m_Number;
        int         m_First;
        int         m_Next;
    };

    // A small, bounded JSON document. Strings are decoded in the owned buffer;
    // node indices stay valid when the node array grows.
    struct Json
    {
        dmArray<JsonNode> m_Nodes;
        char*             m_Text;
        char*             m_Cursor;
        char*             m_End;
        bool              m_Valid;
        Json();
        ~Json();
        bool            Parse(const char* text, uint32_t size);
        int             Field(int object, const char* name) const;
        const JsonNode& Get(int node) const;
        const char*     String(int node, const char* fallback = 0) const;
        int             Integer(int node, int fallback = -1) const;
        bool            Boolean(int node, bool fallback = false) const;

        private:
        void  Space();
        char* ReadString();
        int   Value(int depth);
    };

    // Returns 1 for a complete frame, 0 for incomplete input, -1 for invalid input.
    int ReadFrame(const Buffer& input, uint32_t* offset, uint32_t* size);
} // namespace dmDebugger
#endif
