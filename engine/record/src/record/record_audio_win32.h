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

#ifndef DM_RECORD_AUDIO_WIN32_H
#define DM_RECORD_AUDIO_WIN32_H

#include <windows.h>
#include <stdint.h>

namespace dmRecord
{
    const uint32_t RECORD_AUDIO_RATE = 48000;
    const uint32_t RECORD_AUDIO_CHANNELS = 2;

    struct AudioCapture;
    // Runs on the capture thread. Data is copied out of WASAPI before calling
    // the writer. A null samples pointer represents silence.
    typedef HRESULT (*AudioCaptureCallback)(void* context, const int16_t* samples, uint32_t frames, uint64_t time, bool discontinuity);

    bool IsProcessAudioCaptureSupported();
    // Initializes on its own COM thread and waits for StartAudioCapture.
    HRESULT NewAudioCapture(AudioCapture** capture, AudioCaptureCallback callback, void* context);
    void StartAudioCapture(AudioCapture* capture);
    HRESULT GetAudioCaptureResult(AudioCapture* capture);
    // Stops, drains, joins and destroys capture, including after failed startup.
    HRESULT DeleteAudioCapture(AudioCapture* capture);
}

#endif
