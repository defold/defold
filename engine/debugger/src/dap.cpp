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

#include "dap.h"
#include <dlib/dstrings.h>
#include <dlib/json.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <limits.h>

namespace dmDebugger
{
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
    static void WriteJsonString(void* context, const char* text, uint32_t size)
    {
        ((Buffer*)context)->Add(text, size);
    }

    void Buffer::String(const char* text, uint32_t size)
    {
        dmJson::WriteString(text, size, WriteJsonString, this);
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
