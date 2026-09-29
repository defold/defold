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

#include "safe_windows.h"
#include <stdint.h>
#include <time.h>

namespace dmTime
{
    void Sleep(uint32_t useconds)
    {
        // Round up so a positive sub-millisecond wait does not become Sleep(0)
        // and cause callers such as the frame pacer to poll until their deadline.
        uint32_t milliseconds = useconds / 1000 + (useconds % 1000 != 0);
        ::Sleep(milliseconds);
    }

    uint64_t GetTime()
    {
#if defined(_MSC_VER) || defined(_MSC_EXTENSIONS)
  #define DELTA_EPOCH_IN_MICROSECS  11644473600000000Ui64
#else
  #define DELTA_EPOCH_IN_MICROSECS  11644473600000000ULL
#endif
        FILETIME ft;
        uint64_t t;
        GetSystemTimeAsFileTime(&ft);

        t = ft.dwHighDateTime;
        t <<= 32;
        t |= ft.dwLowDateTime;

        t /= 10;
        t -= DELTA_EPOCH_IN_MICROSECS;
        return t;
    }

    // Convert Windows performance-counter ticks without multiplying the full
    // uptime counter by 1000000 first. That product overflows uint64_t after
    // about 21.35 days at 10 MHz, making the clock go backwards and breaking
    // elapsed-time measurements and absolute-deadline comparisons.
    // Split whole and fractional seconds to retain integer-microsecond precision.
    // frequency must be nonzero; for Windows QueryPerformanceCounter frequencies
    // the remainder's product fits in uint64_t, as must the resulting timestamp.
    uint64_t PerformanceCounterToMicroseconds(uint64_t ticks, uint64_t frequency)
    {
        uint64_t seconds = ticks / frequency;
        uint64_t remainder = ticks % frequency;
        return seconds * 1000000ULL + remainder * 1000000ULL / frequency;
    }

    static uint64_t frequency = 0;

    uint64_t GetMonotonicTime()
    {
        if (frequency == 0) {
            LARGE_INTEGER freq;
            QueryPerformanceFrequency(&freq);
            frequency = freq.QuadPart;
        }
        LARGE_INTEGER counter;
        QueryPerformanceCounter(&counter);
        return PerformanceCounterToMicroseconds((uint64_t)counter.QuadPart, frequency);
    }
}
