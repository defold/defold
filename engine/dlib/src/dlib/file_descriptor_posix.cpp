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

#if !defined(_WIN32)

#include "file_descriptor.h"
#include <stdint.h>
#include <poll.h>
#include <assert.h>

namespace dmFileDescriptor
{
    int PollEventToNative(PollEvent event)
    {
        switch (event)
        {
            case EVENT_READ: return POLLIN;
            case EVENT_WRITE: return POLLOUT;
            case EVENT_ERROR: return POLLPRI;
            default: assert(false);
        }
    }
    int PollReturnEventToNative(PollEvent event)
    {
        switch (event)
        {
            case EVENT_READ: return POLLIN;
            case EVENT_WRITE: return POLLOUT;
            case EVENT_ERROR: return POLLHUP | POLLERR | POLLNVAL;
            default: assert(false);
        }
    }

    int Wait(Poller* poller, int timeout)
    {
        int r = poll(poller->m_Pollfds.Begin(), poller->m_Pollfds.Size(), timeout);
        return r;
    }
}

#endif
