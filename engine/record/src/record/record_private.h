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

#ifndef DM_RECORD_PRIVATE_H
#define DM_RECORD_PRIVATE_H

#include "record.h"
#include <stddef.h>

namespace dmRecord
{
    // Main-thread observers snapshot completed states before another caller can
    // replace the current operation. Observers must not call back into the recorder.
    struct StatusListener
    {
        void (*m_Callback)(const Status* status);
        StatusListener* m_Next;
    };
    void AddStatusListener(StatusListener* listener);
    void RemoveStatusListener(StatusListener* listener);
    void NotifyStatus(const Status* status);

    // Copy valid UTF-8 without leaving a partial character when the buffer fills.
    void CopyTextUtf8(char* output, size_t output_size, const char* text);

    // dmSys/Lua paths use the ANSI code page on Windows; native recording paths use UTF-8.
    bool HostPathToUtf8(const char* path, char* output, uint32_t output_size);

#if defined(DM_PLATFORM_MACOS)
    // Prepare the native output path without removing directories or their contents.
    bool PrepareOutputFile(const char* path, Status* status);
#endif

    struct Window
    {
        uintptr_t m_Handle;
        double    m_X, m_Y, m_Width, m_Height, m_Scale;
    };

    // Snapshot window geometry on the main thread before handing it to the worker.
    bool GetWindow(void* native_window, Window* window);
    // Prepare the window before taking that snapshot and restore it after capture.
    // Both hooks run on the main thread, including failure and shutdown cleanup.
    bool PrepareWindow(void* native_window, Window* window);
    void RestoreWindow();
    void GetNativeStatus(Status* status);
    bool StartNative(const Window* window, const char* path, uint32_t fps, bool audio, Status* status);
    bool StopNative(Status* status);
    void FinalizeNative();

    // Linux framebuffer capture, called on the render thread before Flip().
    // The null backend does nothing, including in headless Linux engines.
    void CaptureFrame();
} // namespace dmRecord

#endif
