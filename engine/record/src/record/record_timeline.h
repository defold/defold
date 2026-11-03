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
    // Framebuffer capture uses microseconds. Round the next boundary up so a
    // fractional interval (e.g. 60 fps) cannot capture twice at the same tick.
    uint64_t NextVideoFrameTime(uint64_t elapsed, uint32_t fps);

    // Media Foundation and WASAPI timestamps are in 100 ns units. Calculate
    // absolute sample boundaries rather than accumulating rounded durations.
    uint64_t FramesToTime(uint64_t frames, uint32_t rate);

    uint64_t TimeToFrames(uint64_t time, uint32_t rate);

    struct AudioPacketRange
    {
        uint64_t m_SilenceFrames;
        uint32_t m_SkipFrames;
        uint32_t m_Frames;
    };

    AudioPacketRange GetAudioPacketRange(uint64_t origin, uint64_t time, uint32_t frames, uint32_t rate, uint64_t written, uint64_t limit);
} // namespace dmRecord

#endif
