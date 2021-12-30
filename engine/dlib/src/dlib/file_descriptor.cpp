// Copyright 2021 The Defold Foundation
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
#include <assert.h>
#include <poll.h>

namespace dmFileDescriptor
{
    void PollerSetCapacity(Poller* poller, uint32_t capacity)
    {
        poller->m_Pollfds.SetCapacity(capacity);
    }

    int PollEventToNative(PollEvent event)
    {
        switch (event)
        {
            case EVENT_READ:
                return POLLIN;
            case EVENT_WRITE:
                return POLLOUT;
            default:
            case EVENT_ERROR:
                return POLLPRI;
        }
    }

    void PollerClearEvent(Poller* poller, PollEvent event, int fd)
    {
        for (uint32_t i = 0; i < poller->m_Pollfds.Size(); ++i)
        {
            pollfd* pfd = &poller->m_Pollfds[i];
            if (pfd->fd == fd)
            {
                int e = PollEventToNative(event);
                pfd->events &= ~e;
                return;
            }
        }
    }

    void PollerSetEvent(Poller* poller, PollEvent event, int fd)
    {
        int e = PollEventToNative(event);
        for (uint32_t i = 0; i < poller->m_Pollfds.Size(); ++i)
        {
            pollfd* pfd = &poller->m_Pollfds[i];
            if (pfd->fd == fd)
            {
                pfd->events |= e;
                // dmLogInfo("PollerSetEvent() existing fd = %d event = %d e = %d p = %p", fd, event, e, pfd)
                return;
            }
        }

        if (poller->m_Pollfds.Full())
        {
            poller->m_Pollfds.OffsetCapacity(4);
        }

        pollfd pfd;
        pfd.fd = fd;
        pfd.events = e;
        poller->m_Pollfds.Push(pfd);
        // dmLogInfo("PollerSetEvent() new fd = %d event = %d e = %d", fd, event, e)
    }

    bool PollerHasEvent(Poller* poller, PollEvent event, int fd)
    {
        for (uint32_t i = 0; i < poller->m_Pollfds.Size(); ++i)
        {
            pollfd* pfd = &poller->m_Pollfds[i];
            if (pfd->fd == fd)
            {
                // dmLogInfo("PollerHasEvent() existing fd = %d event = %d p = %p", fd, event, pfd)
                if (event == EVENT_READ)
                {
                    return pfd->revents & POLLIN;
                }
                else if (event == EVENT_WRITE)
                {
                    return pfd->revents & POLLOUT;
                }
                else if (event == EVENT_ERROR)
                {
                    return pfd->revents & (POLLHUP + POLLERR + POLLNVAL);
                }
                return false;
            }
        }
        return false;
    }

    void PollerReset(Poller* poller)
    {
        // dmLogInfo("PollerReset");
        while (!poller->m_Pollfds.Empty())
        {
            poller->m_Pollfds.Pop();
        }
    }

    int Wait(Poller* poller, int timeout)
    {

        int r;

        if (timeout != 0)
        {
            dmLogInfo("Wait poll() size = %d timeout = %d", poller->m_Pollfds.Size(), timeout);
            for (uint32_t i = 0; i < poller->m_Pollfds.Size(); ++i)
            {
                pollfd* v = &poller->m_Pollfds[i];
                dmLogInfo("Wait poll() i = %d fd = %d events = %d", i, v->fd, v->events);
            }
        }

        // r = poll(selector->m_Pollfd, 1, timeout);
        r = poll(poller->m_Pollfds.Begin(), poller->m_Pollfds.Size(), timeout);

        if (timeout != 0)
        {
            dmLogInfo("Wait poll() result r = %d", r);
            for (uint32_t i = 0; i < poller->m_Pollfds.Size(); ++i)
            {
                pollfd* v = &poller->m_Pollfds[i];
                dmLogInfo("Wait poll() i = %d fd = %d revents = %d", i, v->fd, v->revents);
            }
        }

        // dmLogInfo("Select got result r = %d fd = %d revents = %d", r, selector->m_Pollfd[0].fd, selector->m_Pollfd[0].revents);

        // dmLogInfo("Wait r = %d", r);
        return r;
    }
}
