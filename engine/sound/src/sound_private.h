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

#ifndef DM_SOUND_PRIVATE_H
#define DM_SOUND_PRIVATE_H

#include <dmsdk/dlib/configfile_gen.hpp>
#include "sound.h"

#if defined(__EMSCRIPTEN__) && !defined(DM_NO_THREAD_SUPPORT)
#include <emscripten.h>
#define DM_SOUND_WASM_SUPPORT_THREADS 1
#endif

namespace dmSound
{
    Result PlatformInitialize(dmConfigFile::HConfig config, const InitializeParams* params);

    Result PlatformFinalize();

    bool PlatformIsMusicPlaying(bool is_device_started, bool has_window_focus);

    bool PlatformIsAudioInterrupted();

    uint32_t GetDefaultFrameCount(uint32_t mix_rate);

    /*
     * @param pan [type: float] Range [0, 1]. 0.5 == center panning, 0 == full left, 1 == full right
     * @param left_scale [type: float*] (out)Range [0, 1]
     * @param right_scale [type: float*] (out)Range [0, 1]
     */
    void GetPanScale(float pan, float* left_scale, float* right_scale);

    // Unit tests
    int64_t GetInternalPos(HSoundInstance);
    int32_t GetRefCount(HSoundData);

    #define SOUND_MAX_DECODE_CHANNELS (2)
    #define SOUND_MAX_MIX_CHANNELS (2)
    #define SOUND_OUTBUFFER_COUNT (3)
    #define SOUND_OUTBUFFER_COUNT_NO_THREADS (4)
    #define SOUND_OUTBUFFER_MAX_COUNT (6)
    #define SOUND_MAX_SPEED (5)
    #define SOUND_MAX_HISTORY (4)
    #define SOUND_MAX_FUTURE (4)
    #define SOUND_INSTANCE_STATEFRAMECOUNT (SOUND_MAX_HISTORY + SOUND_MAX_SPEED + SOUND_MAX_FUTURE)         // "max speed" is used as "extra sample count" as we can at most leave these many samples in the buffers due to fractional positions etc.

    const uint32_t RESAMPLE_FRACTION_BITS = 11; // matches number of polyphase filter bank entries (2048)

    const dmhash_t MASTER_GROUP_HASH = dmHashString64("master");
    const uint32_t GROUP_MEMORY_BUFFER_COUNT = 64;
}

#endif // #ifndef DM_SOUND_PRIVATE_H
