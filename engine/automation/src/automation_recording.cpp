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

#include "automation_recording.h"

#if (defined(DM_AUTOMATION_HEADLESS) || (!defined(DM_PLATFORM_MACOS) && !defined(DM_PLATFORM_WINDOWS)))

#include <string.h>

namespace dmAutomation
{
    static void UnsupportedStatus(RecordingStatus* status)
    {
        memset(status, 0, sizeof(*status));
        status->m_Supported = false;
        strcpy(status->m_Failure, NativeRecordingUnsupportedReason());
    }

    bool IsNativeRecordingSupported()
    {
        return false;
    }

    bool IsNativeRecordingAudioSupported()
    {
        return false;
    }

    const char* NativeRecordingBackendName()
    {
        return "unsupported";
    }

    const char* NativeRecordingMinimumPlatformVersion()
    {
        return "";
    }

    const char* NativeRecordingUnsupportedReason()
    {
        return "native video recording is available on supported macOS and Windows desktop runtimes";
    }

    void GetNativeRecordingStatus(RecordingStatus* status)
    {
        UnsupportedStatus(status);
    }

    bool GetRecordingWindow(RecordingWindow*) { return false; }

    bool StartNativeRecording(const RecordingWindow*, const char*, uint32_t, uint32_t, uint32_t, bool, RecordingStatus* status)
    {
        UnsupportedStatus(status);
        return false;
    }

    bool StopNativeRecording(RecordingStatus* status)
    {
        UnsupportedStatus(status);
        return false;
    }

    void FinalizeNativeRecording()
    {
    }
}

#endif
