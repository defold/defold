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
#include <mach/mach_time.h>
#include <assert.h>

namespace dmTime
{
    // One-second chunks keep the product in uint64_t even for a 32-bit denominator.
    // Round up: a fractional tick must not turn a positive wait into a poll.
    uint64_t MicrosecondsToWaitTicks(uint32_t useconds, uint32_t numer, uint32_t denom)
    {
        assert(useconds <= 1000000 && numer && denom);
        return ((uint64_t)useconds * 1000U * denom + numer - 1) / numer;
    }

    uint32_t DeadlineWaitChunk(uint64_t remaining, uint32_t attempt, bool staged)
    {
        uint32_t chunk = remaining > 1000000 ? 1000000 : (uint32_t)remaining;
        // Reduce duration-dependent timer slack with a few earlier blocking
        // wakeups. Bound the extra wakeups and always finish with an OS wait.
        if (staged && attempt < 8 && chunk > 250)
            chunk = (chunk + 1) / 2;
        return chunk;
    }

    bool SleepUntil(uint64_t deadline, bool staged)
    {
        mach_timebase_info_data_t timebase;
        if (mach_timebase_info(&timebase) != KERN_SUCCESS)
            return false;
        uint32_t attempt = 0;
        for (;;)
        {
            // Sample Mach first: preemption between these reads can shorten a
            // wait, which the loop corrects, but must not extend the deadline.
            uint64_t ticks_now = mach_absolute_time();
            uint64_t now = GetMonotonicTime();
            if (now >= deadline)
                return true;
            uint64_t remaining = deadline - now;
            uint32_t chunk = DeadlineWaitChunk(remaining, attempt++, staged);
            uint64_t ticks = MicrosecondsToWaitTicks(chunk, timebase.numer, timebase.denom);
            // Use a duration between clock domains, never an assumed shared epoch.
            kern_return_t result = mach_wait_until(ticks_now + ticks);
            if (result != KERN_SUCCESS && result != KERN_ABORTED)
                return false;
        }
    }

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
