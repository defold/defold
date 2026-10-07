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
#include <dmsdk/dlib/condition_variable.h>
#include <dmsdk/dlib/thread.h>

namespace dmAutomation
{
    enum RecordingRequest
    {
        RECORDING_REQUEST_NONE,
        RECORDING_REQUEST_START,
        RECORDING_REQUEST_STOP,
        RECORDING_REQUEST_FINALIZE
    };

    struct RecordingOperation
    {
        RecordingStatus m_Status;
        RecordingWindow m_Window;
        dmMutex::HMutex m_Mutex;
        dmConditionVariable::HConditionVariable m_Condition;
        dmThread::Thread m_Thread;
        RecordingRequest m_Request;
    };
    static RecordingOperation g_Recording;
    static uint64_t g_OperationId;

    static void RecordingWorker(void* arg)
    {
        RecordingOperation* operation = (RecordingOperation*)arg;
        for (;;)
        {
            RecordingRequest request;
            RecordingStatus pending;
            RecordingWindow window;
            {
                dmMutex::ScopedLock lock(operation->m_Mutex);
                while (operation->m_Request == RECORDING_REQUEST_NONE)
                    dmConditionVariable::Wait(operation->m_Condition, operation->m_Mutex);
                request = operation->m_Request;
                operation->m_Request = RECORDING_REQUEST_NONE;
                pending = operation->m_Status;
                window = operation->m_Window;
            }
            // WinRT initialization and native recorder destruction must share a
            // thread, including shutdown during an outstanding start or stop.
            if (request == RECORDING_REQUEST_FINALIZE)
            {
                FinalizeNativeRecording();
                return;
            }
            RecordingStatus result;
            if (request == RECORDING_REQUEST_STOP)
                StopNativeRecording(&result);
            else
                StartNativeRecording(&window, pending.m_Path, pending.m_Width, pending.m_Height,
                                     pending.m_Fps, pending.m_Audio, &result);
            if (!result.m_Path[0])
                dmStrlCpy(result.m_Path, pending.m_Path, sizeof(result.m_Path));
            result.m_Id = pending.m_Id;
            result.m_Stopping = pending.m_Stopping;
            result.m_Pending = false;
            dmMutex::ScopedLock lock(operation->m_Mutex);
            operation->m_Status = result;
        }
    }

    void GetRecordingStatus(RecordingStatus* status)
    {
        if (!g_Recording.m_Mutex)
        {
            GetNativeRecordingStatus(status);
            return;
        }
        {
            dmMutex::ScopedLock lock(g_Recording.m_Mutex);
            *status = g_Recording.m_Status;
        }
        if (!status->m_Pending && status->m_Active)
        {
            RecordingStatus current;
            GetNativeRecordingStatus(&current);
            current.m_Id = status->m_Id;
            current.m_Stopping = status->m_Stopping;
            dmMutex::ScopedLock lock(g_Recording.m_Mutex);
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
        RecordingWindow window;
        if (!GetRecordingWindow(&window)) return false;
        if (!g_Recording.m_Mutex)
        {
            g_Recording.m_Mutex = dmMutex::New();
            g_Recording.m_Condition = dmConditionVariable::New();
        }
        dmMutex::ScopedLock lock(g_Recording.m_Mutex);
        g_Recording.m_Window = window;
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
        g_Recording.m_Request = RECORDING_REQUEST_START;
        *status = *pending;
        if (!g_Recording.m_Thread)
            g_Recording.m_Thread = dmThread::New(RecordingWorker, 512 * 1024, &g_Recording, "automation-record");
        if (!g_Recording.m_Thread)
        {
            g_Recording.m_Request = RECORDING_REQUEST_NONE;
            g_Recording.m_Status.m_Pending = false;
            dmStrlCpy(g_Recording.m_Status.m_Failure, "recording worker unavailable", sizeof(g_Recording.m_Status.m_Failure));
            *status = g_Recording.m_Status;
            return false;
        }
        dmConditionVariable::Signal(g_Recording.m_Condition);
        return true;
    }

    bool StopRecording(RecordingStatus* status)
    {
        GetRecordingStatus(status);
        if (status->m_Pending || !status->m_Active) return false;
        dmMutex::ScopedLock lock(g_Recording.m_Mutex);
        g_Recording.m_Status.m_Pending = true;
        g_Recording.m_Status.m_Stopping = true;
        g_Recording.m_Request = RECORDING_REQUEST_STOP;
        *status = g_Recording.m_Status;
        dmConditionVariable::Signal(g_Recording.m_Condition);
        return true;
    }

    void FinalizeRecording()
    {
        // No worker calls into Lua, scene objects, or the main dispatch queue.
        // Join before the engine destroys the window and platform contexts.
        if (g_Recording.m_Thread)
        {
            {
                dmMutex::ScopedLock lock(g_Recording.m_Mutex);
                g_Recording.m_Request = RECORDING_REQUEST_FINALIZE;
                dmConditionVariable::Signal(g_Recording.m_Condition);
            }
            dmThread::Join(g_Recording.m_Thread);
#if defined(_WIN32)
            // Win32 Join waits for termination; Detach closes the thread handle.
            dmThread::Detach(g_Recording.m_Thread);
#endif
        }
        if (g_Recording.m_Condition) dmConditionVariable::Delete(g_Recording.m_Condition);
        if (g_Recording.m_Mutex) dmMutex::Delete(g_Recording.m_Mutex);
        memset(&g_Recording, 0, sizeof(g_Recording));
    }
}
