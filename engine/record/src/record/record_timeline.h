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

#ifndef DM_RECORD_TIMELINE_H
#define DM_RECORD_TIMELINE_H

#include <stdint.h>

namespace dmRecord
{
    // Media Foundation and WASAPI timestamps are in 100 ns units. Calculate
    // absolute sample boundaries rather than accumulating rounded durations.
    static inline uint64_t FramesToTime(uint64_t frames, uint32_t rate)
    {
        return frames / rate * 10000000 + frames % rate * 10000000 / rate;
    }

    static inline uint64_t TimeToFrames(uint64_t time, uint32_t rate)
    {
        return time / 10000000 * rate + time % 10000000 * rate / 10000000;
    }

    struct AudioPacketRange
    {
        uint64_t m_SilenceFrames;
        uint32_t m_SkipFrames;
        uint32_t m_Frames;
    };

    static inline AudioPacketRange GetAudioPacketRange(uint64_t origin, uint64_t time, uint32_t frames,
                                                       uint32_t rate, uint64_t written, uint64_t limit)
    {
        AudioPacketRange range = {};
        // Round to the nearest sample so timestamp quantization does not insert
        // or drop one sample on every packet (e.g. 44100 Hz in a 100 ns clock).
        int64_t position = time >= origin
            ? (int64_t)TimeToFrames(time - origin + 5000000 / rate, rate)
            : -(int64_t)TimeToFrames(origin - time + 5000000 / rate, rate);
        int64_t end = position + frames;
        if (end <= (int64_t)written || written >= limit)
            return range;
        if (position < (int64_t)written)
            range.m_SkipFrames = (uint32_t)((int64_t)written - position);
        else if ((uint64_t)position > written)
            range.m_SilenceFrames = ((uint64_t)position < limit ? (uint64_t)position : limit) - written;
        uint64_t begin = written + range.m_SilenceFrames;
        uint64_t clipped_end = (uint64_t)end < limit ? (uint64_t)end : limit;
        if (clipped_end > begin)
            range.m_Frames = (uint32_t)(clipped_end - begin);
        return range;
    }
}

#endif
