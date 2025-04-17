// Copyright 2020-2025 The Defold Foundation
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

#ifndef DM_SOUND_PRIVATE_H
#define DM_SOUND_PRIVATE_H

#include <dmsdk/dlib/configfile_gen.hpp>
#include "sound.h"

namespace dmSound
{
    Result PlatformInitialize(dmConfigFile::HConfig config, const InitializeParams* params);

    Result PlatformFinalize();

    bool PlatformIsMusicPlaying(bool is_device_started, bool has_window_focus);

    bool PlatformIsAudioInterrupted();

    uint32_t GetDefaultFrameCount(uint32_t mix_rate);

    // Unit tests
    int64_t GetInternalPos(HSoundInstance);
    int32_t GetRefCount(HSoundData);

    #define DM_SOUND_USE_LEGACY_GAIN (1)     // if defined to 0 gain will be an exponential parameter rather than a linear scale
    #define DM_SOUND_USE_LEGACY_STEREO_PAN (0)  // define this to 1 to enable legacy behavior regarding panning of stereo instances

    #define SOUND_MAX_DECODE_CHANNELS (2)
    #define SOUND_MAX_MIX_CHANNELS (2)
    #define SOUND_OUTBUFFER_COUNT (6)
    #define SOUND_MAX_SPEED (5)
    #define SOUND_MAX_HISTORY (4)
    #define SOUND_MAX_FUTURE (4)
    #define SOUND_INSTANCE_STATEFRAMECOUNT (SOUND_MAX_HISTORY + SOUND_MAX_SPEED + SOUND_MAX_FUTURE)         // "max speed" is used as "extra sample count" as we can at most leave these many samples in the buffers due to fractional positions etc.

    const uint32_t RESAMPLE_FRACTION_BITS = 11; // matches number of polyphase filter bank entries (2048)

    const dmhash_t MASTER_GROUP_HASH = dmHashString64("master");
    const uint32_t GROUP_MEMORY_BUFFER_COUNT = 64;
}

#endif // #ifndef DM_SOUND_PRIVATE_H
