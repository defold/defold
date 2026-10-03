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

#include "automation_private.h"
#include "automation_recording.h"
#include "automation_artifact.h"
#include <dmsdk/dlib/thread.h>

namespace dmAutomation
{
    struct RecordingOperation
    {
        RecordingStatus m_Status;
        RecordingWindow m_Window;
        dmMutex::HMutex m_Mutex;
        dmThread::Thread m_Thread;
        bool m_Done;
    };
    static RecordingOperation g_Recording;
    static uint64_t g_OperationId;

    static void RecordingWorker(void* arg)
    {
        RecordingOperation* operation = (RecordingOperation*)arg;
        RecordingStatus result;
        if (operation->m_Status.m_Stopping)
            StopNativeRecording(&result);
        else
            StartNativeRecording(&operation->m_Window, operation->m_Status.m_Path,
                operation->m_Status.m_Width, operation->m_Status.m_Height,
                operation->m_Status.m_Fps, operation->m_Status.m_Audio, &result);
        dmMutex::ScopedLock lock(operation->m_Mutex);
        if (!result.m_Path[0])
            dmStrlCpy(result.m_Path, operation->m_Status.m_Path, sizeof(result.m_Path));
        result.m_Id = operation->m_Status.m_Id;
        result.m_Stopping = operation->m_Status.m_Stopping;
        result.m_Pending = false;
        operation->m_Status = result;
        operation->m_Done = true;
    }

    void GetRecordingStatus(RecordingStatus* status)
    {
        if (!g_Recording.m_Mutex)
        {
            GetNativeRecordingStatus(status);
            return;
        }
        bool done;
        {
            dmMutex::ScopedLock lock(g_Recording.m_Mutex);
            *status = g_Recording.m_Status;
            done = g_Recording.m_Done;
        }
        if (done && g_Recording.m_Thread)
        {
            dmThread::Join(g_Recording.m_Thread);
            g_Recording.m_Thread = 0;
        }
        if (!status->m_Pending && status->m_Active)
        {
            RecordingStatus current;
            GetNativeRecordingStatus(&current);
            current.m_Id = status->m_Id;
            current.m_Stopping = status->m_Stopping;
            g_Recording.m_Status = current;
            *status = current;
        }
        if (!status->m_Pending && !status->m_Active && !status->m_Finalized && status->m_Failure[0])
            ReleaseCapturePath(status->m_Path);
    }

    void PollRecording()
    {
        if (!g_Recording.m_Mutex) return;
        RecordingStatus status;
        GetRecordingStatus(&status);
    }

    bool StartRecording(const char* path, uint32_t width, uint32_t height, uint32_t fps, bool audio, RecordingStatus* status)
    {
        GetRecordingStatus(status);
        if (status->m_Pending || status->m_Active) return false;
        if (!g_Recording.m_Mutex) g_Recording.m_Mutex = dmMutex::New();
        if (!GetRecordingWindow(&g_Recording.m_Window)) return false;
        memset(&g_Recording.m_Status, 0, sizeof(g_Recording.m_Status));
        RecordingStatus* pending = &g_Recording.m_Status;
        pending->m_Id = ++g_OperationId;
        pending->m_Supported = true;
        pending->m_Pending = true;
        pending->m_Width = width;
        pending->m_Height = height;
        pending->m_Fps = fps;
        pending->m_Audio = audio;
        dmStrlCpy(pending->m_Path, path, sizeof(pending->m_Path));
        g_Recording.m_Done = false;
        *status = *pending;
        g_Recording.m_Thread = dmThread::New(RecordingWorker, 512 * 1024, &g_Recording, "automation-record");
        if (!g_Recording.m_Thread)
        {
            g_Recording.m_Status.m_Pending = false;
            dmStrlCpy(g_Recording.m_Status.m_Failure, "recording worker unavailable", sizeof(g_Recording.m_Status.m_Failure));
            *status = g_Recording.m_Status;
            return false;
        }
        return true;
    }

    bool StopRecording(RecordingStatus* status)
    {
        GetRecordingStatus(status);
        if (status->m_Pending || !status->m_Active) return false;
        g_Recording.m_Status.m_Pending = true;
        g_Recording.m_Status.m_Stopping = true;
        g_Recording.m_Done = false;
        *status = g_Recording.m_Status;
        g_Recording.m_Thread = dmThread::New(RecordingWorker, 512 * 1024, &g_Recording, "automation-record");
        if (!g_Recording.m_Thread)
        {
            g_Recording.m_Status.m_Pending = false;
            dmStrlCpy(g_Recording.m_Status.m_Failure, "recording worker unavailable", sizeof(g_Recording.m_Status.m_Failure));
            *status = g_Recording.m_Status;
            return false;
        }
        return true;
    }

    void FinalizeRecording()
    {
        // No worker calls into Lua, scene objects, or the main dispatch queue.
        // Join before the engine destroys the window and platform contexts.
        if (g_Recording.m_Thread) dmThread::Join(g_Recording.m_Thread);
        FinalizeNativeRecording();
        if (g_Recording.m_Mutex) dmMutex::Delete(g_Recording.m_Mutex);
        memset(&g_Recording, 0, sizeof(g_Recording));
    }
}
