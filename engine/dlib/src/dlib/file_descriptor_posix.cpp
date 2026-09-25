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

#include <assert.h>
#include <poll.h>
#include <dmsdk/dlib/dalloca.h>

namespace dmFileDescriptor
{
    static void ToNativePollFDs(Poller* poller, pollfd* native_pollfds, uint32_t count)
    {
        for (uint32_t i = 0; i < count; ++i)
        {
            native_pollfds[i].fd = poller->m_Pollfds[i].m_Fd;
            native_pollfds[i].events = (short)poller->m_Pollfds[i].m_Events;
            native_pollfds[i].revents = 0;
        }
    }

    static void FromNativePollFDs(pollfd* native_pollfds, Poller* poller, uint32_t count)
    {
        for (uint32_t i = 0; i < count; ++i)
        {
            poller->m_Pollfds[i].m_REvents = native_pollfds[i].revents;
        }
    }

    int PollEventToNative(PollEvent event)
    {
        switch (event)
        {
            case EVENT_READ:
                return POLLIN;
            case EVENT_WRITE:
                return POLLOUT;
            case EVENT_ERROR:
                return POLLPRI;
            default:
                assert(false);
        }
    }

    int PollReturnEventToNative(PollEvent event)
    {
        switch (event)
        {
            case EVENT_READ:
                return POLLIN;
            case EVENT_WRITE:
                return POLLOUT;
            case EVENT_ERROR:
                return POLLHUP | POLLERR | POLLNVAL;
            default:
                assert(false);
        }
    }

    int Wait(Poller* poller, int timeout)
    {
        uint32_t count = poller->m_Pollfds.Size();
        pollfd* native_pollfds = count ? (pollfd*)dmAlloca(sizeof(pollfd) * count) : 0;
        ToNativePollFDs(poller, native_pollfds, count);
        int r = poll(native_pollfds, (nfds_t)count, timeout);
        FromNativePollFDs(native_pollfds, poller, count);
        return r;
    }
} // namespace dmFileDescriptor
