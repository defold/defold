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

#include <stdint.h>
#define JC_TEST_IMPLEMENTATION
#include <jc_test/jc_test.h>
#include "dlib/time.h"

TEST(dmTime, Sleep)
{
    dmTime::Sleep(1);
}

#if defined(_WIN32)
namespace dmTime
{
    // Internal Windows conversion, declared here only for regression tests.
    uint64_t PerformanceCounterToMicroseconds(uint64_t ticks, uint64_t frequency);
}

TEST(dmTime, PerformanceCounterConversionAcrossOverflowBoundary)
{
    const uint64_t frequency = 10000000ULL;
    // The old ticks * 1000000 multiplication first overflows at this counter
    // value. At 10 MHz, ten ticks are exactly one microsecond.
    const uint64_t boundary = UINT64_MAX / 1000000ULL + 1;
    uint64_t previous = dmTime::PerformanceCounterToMicroseconds(boundary - 20, frequency);
    for (uint64_t ticks = boundary - 19; ticks <= boundary + 20; ++ticks)
    {
        uint64_t time = dmTime::PerformanceCounterToMicroseconds(ticks, frequency);
        ASSERT_EQ(ticks / 10, time);
        ASSERT_GE(time, previous);
        previous = time;
    }
}

TEST(dmTime, PerformanceCounterConversionPrecisionAndLongUptime)
{
    ASSERT_EQ(0ULL, dmTime::PerformanceCounterToMicroseconds(0, 10000000));
    ASSERT_EQ(0ULL, dmTime::PerformanceCounterToMicroseconds(9, 10000000));
    ASSERT_EQ(1ULL, dmTime::PerformanceCounterToMicroseconds(10, 10000000));
    // Fractional microseconds are truncated even when the frequency is not an
    // integer multiple of one MHz.
    ASSERT_EQ(39506172ULL, dmTime::PerformanceCounterToMicroseconds(123456789, 3125000));
    // One year of uptime, and the largest signed Windows counter at 10 MHz.
    ASSERT_EQ(31536000000000ULL, dmTime::PerformanceCounterToMicroseconds(315360000000000ULL, 10000000));
    ASSERT_EQ(922337203685477580ULL, dmTime::PerformanceCounterToMicroseconds(9223372036854775807ULL, 10000000));
}

TEST(dmTime, SleepSubMillisecond)
{
    // Verify that a positive sub-millisecond duration blocks instead of being
    // truncated to Sleep(0), which would make deadline loops busy-wait.
    uint64_t start = dmTime::GetMonotonicTime();
    dmTime::Sleep(500);
    uint64_t elapsed = dmTime::GetMonotonicTime() - start;
    ASSERT_GE(elapsed, 400U);
}
#endif

#if !defined(GITHUB_CI)
TEST(dmTime, GetTime)
{
    uint64_t start = dmTime::GetTime();
    dmTime::Sleep(200000);
    uint64_t end = dmTime::GetTime();
    ASSERT_NEAR((double) 200000, (double) (end-start), (double) 40000);
}
#endif

TEST(dmTime, GetMonotonicTime)
{
    uint64_t prev = dmTime::GetMonotonicTime();
    for (int i = 0; i < 1000; ++i)
    {
        uint64_t next = dmTime::GetMonotonicTime();
        ASSERT_LE(prev, next);
        prev = next;
    }
}

int main(int argc, char **argv)
{
    jc_test_init(&argc, argv);
    return jc_test_run_all();
}
