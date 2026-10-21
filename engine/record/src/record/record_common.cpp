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

#include "record_private.h"

#include <string.h>
#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

namespace dmRecord
{
    static StatusListener* g_Listeners;

    void AddStatusListener(StatusListener* listener)
    {
        listener->m_Next = g_Listeners;
        g_Listeners = listener;
    }

    void RemoveStatusListener(StatusListener* listener)
    {
        for (StatusListener** current = &g_Listeners; *current; current = &(*current)->m_Next)
        {
            if (*current == listener)
            {
                *current = listener->m_Next;
                listener->m_Next = 0;
                break;
            }
        }
    }

    void NotifyStatus(const Status* status)
    {
        if (!status->m_Pending)
        {
            for (StatusListener* listener = g_Listeners; listener; listener = listener->m_Next)
            {
                listener->m_Callback(status);
            }
        }
    }

    void CopyTextUtf8(char* output, size_t output_size, const char* text)
    {
        if (!output_size)
        {
            return;
        }
        text = text ? text : "";
        size_t length = strlen(text);
        if (length >= output_size)
        {
            length = output_size - 1;
            while (length && ((unsigned char)text[length] & 0xc0) == 0x80)
            {
                --length;
            }
        }
        memcpy(output, text, length);
        output[length] = 0;
    }

    bool HostPathToUtf8(const char* path, char* output, uint32_t output_size)
    {
        if (!output_size)
        {
            return false;
        }
        output[0] = 0;
#if defined(_WIN32)
        wchar_t wide[1024];
        if (!MultiByteToWideChar(CP_ACP, MB_ERR_INVALID_CHARS, path, -1, wide, sizeof(wide) / sizeof(wide[0])))
        {
            return false;
        }
        if (!WideCharToMultiByte(CP_UTF8, 0, wide, -1, output, output_size, 0, 0))
        {
            output[0] = 0;
            return false;
        }
#else
        size_t size = strlen(path) + 1;
        if (size > output_size)
        {
            return false;
        }
        memcpy(output, path, size);
#endif
        return true;
    }

    StartParams::StartParams()
        : m_Window(0)
        , m_Path(0)
        , m_Fps(30)
        , m_Audio(false)
    {
    }
} // namespace dmRecord
