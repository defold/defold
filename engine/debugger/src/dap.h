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
#include <dlib/json.h>
#include <stdint.h>

namespace dmDebugger
{
    // DAP messages and queues have explicit bounds, including while the client
    // stops reading. No unbounded allocations are driven by Content-Length.
    const uint32_t MAX_MESSAGE_SIZE = 1024 * 1024;
    const uint32_t MAX_QUEUE_SIZE = 4 * MAX_MESSAGE_SIZE;

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

    // Returns 1 for a complete frame, 0 for incomplete input, -1 for invalid input.
    int ReadFrame(const Buffer& input, uint32_t* offset, uint32_t* size);
} // namespace dmDebugger
#endif
