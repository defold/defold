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

#pragma once

#include <stdint.h>

namespace dmAutomation
{
    struct RecordingWindow
    {
        uintptr_t m_Handle;
        double m_X, m_Y, m_Width, m_Height, m_Scale;
    };

    struct RecordingStatus
    {
        uint64_t m_Id;
        bool     m_Pending;
        bool     m_Stopping;
        bool     m_Supported;
        bool     m_Active;
        bool     m_Finalized;
        bool     m_Audio;
        uint32_t m_Width;
        uint32_t m_Height;
        uint32_t m_Fps;
        uint64_t m_StartedWallTime;
        uint64_t m_StartedMonotonicTime;
        uint64_t m_StoppedWallTime;
        uint64_t m_StoppedMonotonicTime;
        char     m_Path[1024];
        char     m_Failure[512];
    };

    bool IsNativeRecordingSupported();
    bool IsNativeRecordingAudioSupported();
    const char* NativeRecordingBackendName();
    const char* NativeRecordingMinimumPlatformVersion();
    const char* NativeRecordingUnsupportedReason();
    void GetNativeRecordingStatus(RecordingStatus* status);
    bool GetRecordingWindow(RecordingWindow* window);
    bool StartNativeRecording(const RecordingWindow* window, const char* path, uint32_t width, uint32_t height,
                              uint32_t fps, bool audio, RecordingStatus* status);
    bool StopNativeRecording(RecordingStatus* status);
    void FinalizeNativeRecording();
    void PollRecording();
    void GetRecordingStatus(RecordingStatus* status);
    bool StartRecording(const char* path, uint32_t width, uint32_t height, uint32_t fps, bool audio, RecordingStatus* status);
    bool StopRecording(RecordingStatus* status);
    void FinalizeRecording();
}
