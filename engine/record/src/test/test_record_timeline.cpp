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

#define JC_TEST_IMPLEMENTATION
#include <jc_test/jc_test.h>
#include <record/record_timeline.h>

using namespace dmRecord;

// Timestamp rounding must not accumulate audio drift over long recordings.
TEST(RecordTimeline, ContiguousPackets)
{
    const uint64_t origin = 1234567890000ULL;
    const uint32_t rates[] = { 44100, 48000 };
    for (uint32_t r = 0; r < 2; ++r)
    {
        uint64_t written = 0;
        for (uint32_t i = 0; i < 100000; ++i)
        {
            uint32_t frames = i % 3 == 0 ? 127 : 480;
            AudioPacketRange range = GetAudioPacketRange(origin, origin + FramesToTime(written, rates[r]), frames, rates[r], written, UINT64_MAX);
            ASSERT_EQ(0ULL, range.m_SilenceFrames);
            ASSERT_EQ(0u, range.m_SkipFrames);
            ASSERT_EQ(frames, range.m_Frames);
            written += frames;
        }
    }
}

// Audio that begins later than video needs leading silence, including silent gaps.
TEST(RecordTimeline, LeadingSilenceAndDiscontinuity)
{
    AudioPacketRange range = GetAudioPacketRange(10000000, 12500000, 480, 48000, 0, UINT64_MAX);
    ASSERT_EQ(12000ULL, range.m_SilenceFrames);
    ASSERT_EQ(0u, range.m_SkipFrames);
    ASSERT_EQ(480u, range.m_Frames);
    range = GetAudioPacketRange(10000000, 15000000, 480, 48000, 12480, UINT64_MAX);
    ASSERT_EQ(11520ULL, range.m_SilenceFrames);
    ASSERT_EQ(480u, range.m_Frames);
}

// Clip packets straddling the epoch or already-written audio without replaying samples.
TEST(RecordTimeline, TrimEarlyAndOverlappingPackets)
{
    AudioPacketRange range = GetAudioPacketRange(10000000, 9950000, 480, 48000, 0, UINT64_MAX);
    ASSERT_EQ(240u, range.m_SkipFrames);
    ASSERT_EQ(240u, range.m_Frames);
    range = GetAudioPacketRange(10000000, 9000000, 480, 48000, 0, UINT64_MAX);
    ASSERT_EQ(0u, range.m_Frames);
    range = GetAudioPacketRange(10000000, 10000000, 480, 48000, 240, UINT64_MAX);
    ASSERT_EQ(240u, range.m_SkipFrames);
    ASSERT_EQ(240u, range.m_Frames);
    range = GetAudioPacketRange(10000000, 10000000, 480, 48000, 480, UINT64_MAX);
    ASSERT_EQ(0u, range.m_Frames);
}

// A final WASAPI packet or gap must not extend the audio past the stop boundary.
TEST(RecordTimeline, ClipAtStop)
{
    AudioPacketRange range = GetAudioPacketRange(0, 100000, 480, 48000, 480, 720);
    ASSERT_EQ(240u, range.m_Frames);
    range = GetAudioPacketRange(0, 200000, 480, 48000, 480, 720);
    ASSERT_EQ(240ULL, range.m_SilenceFrames);
    ASSERT_EQ(0u, range.m_Frames);
    range = GetAudioPacketRange(0, 200000, 480, 48000, 720, 720);
    ASSERT_EQ(0ULL, range.m_SilenceFrames);
    ASSERT_EQ(0u, range.m_Frames);
}

// Absolute frame boundaries preserve the full duration at non-divisor frame rates.
TEST(RecordTimeline, FrameDurationsDoNotDrift)
{
    uint64_t duration = 0;
    for (uint32_t i = 0; i < 60 * 3600; ++i)
    {
        duration += FramesToTime(i + 1, 60) - FramesToTime(i, 60);
    }
    ASSERT_EQ(36000000000ULL, duration);
    ASSERT_EQ(60ULL * 3600, TimeToFrames(duration, 60));
}

// Fractional microsecond deadlines must advance and skipped render ticks must not accumulate drift.
TEST(RecordTimeline, VideoCaptureDeadlines)
{
    ASSERT_EQ(16667ULL, NextVideoFrameTime(0, 60));
    ASSERT_EQ(16667ULL, NextVideoFrameTime(16666, 60));
    ASSERT_EQ(33334ULL, NextVideoFrameTime(16667, 60));
    ASSERT_EQ(50000ULL, NextVideoFrameTime(33334, 60));
    ASSERT_EQ(1016667ULL, NextVideoFrameTime(1000000, 60));
    ASSERT_EQ(2000000ULL, NextVideoFrameTime(1000000, 1));
    uint64_t time = 0;
    for (uint32_t i = 0; i < 60 * 3600; ++i)
    {
        uint64_t next = NextVideoFrameTime(time, 60);
        ASSERT_GT(next, time);
        time = next;
    }
    ASSERT_EQ(3600000000ULL, time);
}

int main(int argc, char** argv)
{
    jc_test_init(&argc, argv);
    return jc_test_run_all();
}
