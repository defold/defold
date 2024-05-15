// Copyright 2020-2024 The Defold Foundation
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

#include "sound.h"
#include "sound_private.h"

namespace dmSound
{
    Result PlatformInitialize(dmConfigFile::HConfig config,
            const InitializeParams* params)
    {
        return RESULT_OK;
    }

    Result PlatformFinalize()
    {
        return RESULT_OK;
    }

    bool PlatformIsMusicPlaying(bool is_device_started, bool has_window_focus)
    {
        (void)is_device_started;
        (void)has_window_focus;
        return false;
    }

    bool PlatformIsAudioInterrupted()
    {
        return false;
    }
}
