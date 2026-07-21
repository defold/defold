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

#include "file_descriptor.h"
#include "log.h"

#include <stdint.h>

namespace dmFileDescriptor
{
    int  PollEventToNative(PollEvent event);
    int  PollReturnEventToNative(PollEvent event);

    void PollerSetCapacity(Poller* poller, uint32_t capacity)
    {
        poller->m_Pollfds.SetCapacity(capacity);
    }

    void PollerClearEvent(Poller* poller, PollEvent event, int fd)
    {
        for (uint32_t i = 0; i < poller->m_Pollfds.Size(); ++i)
        {
            if (poller->m_Pollfds[i].m_Fd == fd)
            {
                int e = PollEventToNative(event);
                poller->m_Pollfds[i].m_Events &= ~e;
                return;
            }
        }
    }

    void PollerSetEvent(Poller* poller, PollEvent event, int fd)
    {
        int e = PollEventToNative(event);
        for (uint32_t i = 0; i < poller->m_Pollfds.Size(); ++i)
        {
            if (poller->m_Pollfds[i].m_Fd == fd)
            {
                poller->m_Pollfds[i].m_Events |= e;
                return;
            }
        }

        if (poller->m_Pollfds.Full())
        {
            poller->m_Pollfds.OffsetCapacity(4);
        }

        PollFD pfd;
        pfd.m_Fd = fd;
        pfd.m_Events = e;
        pfd.m_REvents = 0;
        poller->m_Pollfds.Push(pfd);
    }

    bool PollerHasEvent(Poller* poller, PollEvent event, int fd)
    {
        for (uint32_t i = 0; i < poller->m_Pollfds.Size(); ++i)
        {
            if (poller->m_Pollfds[i].m_Fd == fd)
            {
                int e = PollReturnEventToNative(event);
                return poller->m_Pollfds[i].m_REvents & e;
            }
        }
        return false;
    }

    void PollerReset(Poller* poller)
    {
        while (!poller->m_Pollfds.Empty())
        {
            poller->m_Pollfds.Pop();
        }
    }

    void PollerDump(Poller* poller)
    {
        dmLogInfo("poller size = %d ", poller->m_Pollfds.Size());
        for (uint32_t i = 0; i < poller->m_Pollfds.Size(); ++i)
        {
            dmLogInfo("poller i = %d fd = %d events = %d revents = %d", i, poller->m_Pollfds[i].m_Fd, poller->m_Pollfds[i].m_Events, poller->m_Pollfds[i].m_REvents);
        }
    }
} // namespace dmFileDescriptor
