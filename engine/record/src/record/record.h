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

#ifndef DM_RECORD_H
#define DM_RECORD_H

#include <stdint.h>

namespace dmRecord
{
    // One recording per process. Public operations run on the main thread;
    // capture startup, shutdown and native resource ownership stay on one worker.
    struct StartParams
    {
        StartParams();
        void*       m_Window; // NSWindow*, HWND, or Linux graphics context. Borrowed until Finalize().
        const char* m_Path;   // UTF-8 MP4 output path, copied by Start().
        uint32_t    m_Fps;    // Capture rate, 1..60. Defaults to 30.
        bool        m_Audio;  // Application audio, where supported. Defaults to false.
    };

    struct Status
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

    bool IsSupported();
    bool IsAudioSupported();
    const char* GetBackendName();
    const char* GetMinimumPlatformVersion();
    const char* GetUnsupportedReason();
    // Advance native state once per frame on the main thread, before dispatching
    // script callbacks. Handles automatic stops, window restoration and observers.
    void Update();
    // Copy the latest worker/update snapshot without advancing the recording,
    // changing the window or notifying observers.
    void GetStatus(Status* status);

    // True means the operation was accepted. Keep calling Update() each frame
    // and read GetStatus() until m_Pending clears, then inspect m_Failure and
    // m_Active / m_Finalized for the result.
    // Start assigns a new ID; Stop retains it. IDs are not reused after Finalize().
    bool Start(const StartParams* params, Status* status);
    bool Stop(Status* status);

    // Joins outstanding operations, finalizes an active MP4 and restores the window.
    // Call before destroying the native window. Does not delete the output file.
    void Finalize();
} // namespace dmRecord

#endif
