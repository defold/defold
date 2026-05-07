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

#include "time.h"

#include <sys/time.h>
#include <unistd.h>

namespace dmTime
{
    void Sleep(uint32_t useconds)
    {
        usleep(useconds);
    }

    uint64_t GetTime()
    {
        timeval tv;
        gettimeofday(&tv, 0);
        return ((uint64_t) tv.tv_sec) * 1000000U + tv.tv_usec;
    }

    uint64_t GetMonotonicTime()
    {
        uint64_t nanoseconds = clock_gettime_nsec_np(CLOCK_UPTIME_RAW);
        return nanoseconds / 1000U;
    }
}
