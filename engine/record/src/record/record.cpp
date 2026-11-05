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

#include "record_private.h"
#include <dmsdk/dlib/dstrings.h>
#include <dmsdk/dlib/log.h>
#include <string.h>
#include <dmsdk/dlib/condition_variable.h>
#include <dmsdk/dlib/thread.h>

namespace dmRecord
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
        Status                                  m_Status;
        Window                                  m_Window;
        void*                                   m_NativeWindow;
        dmMutex::HMutex                         m_Mutex;
        dmConditionVariable::HConditionVariable m_Condition;
        dmThread::Thread                        m_Thread;
        RecordingRequest                        m_Request;
        bool                                    m_WindowPrepared;
    };
    static RecordingOperation g_Recording;
    static uint64_t           g_OperationId;

    static void RestorePreparedWindow()
    {
        if (g_Recording.m_WindowPrepared)
        {
            g_Recording.m_WindowPrepared = false;
            RestoreWindow();
        }
    }

    static void PublishCompletedStatus(const Status* status)
    {
        if (status->m_Pending)
        {
            return;
        }
        if (!status->m_Active)
        {
            RestorePreparedWindow();
        }
        NotifyStatus(status);
    }

    static void RecordingWorker(void* arg)
    {
        RecordingOperation* operation = (RecordingOperation*)arg;
        for (;;)
        {
            RecordingRequest request;
            Status pending;
            Window window;
            {
                DM_MUTEX_SCOPED_LOCK(operation->m_Mutex);
                while (operation->m_Request == RECORDING_REQUEST_NONE)
                {
                    dmConditionVariable::Wait(operation->m_Condition, operation->m_Mutex);
                }
                request = operation->m_Request;
                operation->m_Request = RECORDING_REQUEST_NONE;
                pending = operation->m_Status;
                window = operation->m_Window;
            }
            // WinRT initialization and native recorder destruction must share a
            // thread, including shutdown during an outstanding start or stop.
            if (request == RECORDING_REQUEST_FINALIZE)
            {
                FinalizeNative();
                return;
            }
            Status result;
            if (request == RECORDING_REQUEST_STOP)
            {
                StopNative(&result);
            }
            else
            {
                StartNative(&window, pending.m_Path, pending.m_Fps, pending.m_Audio, &result);
            }
            if (!result.m_Path[0])
            {
                dmStrlCpy(result.m_Path, pending.m_Path, sizeof(result.m_Path));
            }
            result.m_Id = pending.m_Id;
            result.m_Stopping = pending.m_Stopping;
            result.m_Pending = false;
            DM_MUTEX_SCOPED_LOCK(operation->m_Mutex);
            operation->m_Status = result;
        }
    }

    void GetStatus(Status* status)
    {
        if (!g_Recording.m_Mutex)
        {
            memset(status, 0, sizeof(*status));
            status->m_Supported = IsSupported();
            if (!status->m_Supported)
            {
                dmStrlCpy(status->m_Failure, GetUnsupportedReason(), sizeof(status->m_Failure));
            }
            return;
        }
        DM_MUTEX_SCOPED_LOCK(g_Recording.m_Mutex);
        *status = g_Recording.m_Status;
    }

    void Update()
    {
        if (!g_Recording.m_Mutex)
        {
            return;
        }
        Status status;
        GetStatus(&status);
        // Preserve a completed start before native polling can replace it with
        // an automatic stop or an asynchronous capture failure.
        PublishCompletedStatus(&status);
        if (!status.m_Pending && status.m_Active)
        {
            Status current;
            GetNativeStatus(&current);
            current.m_Id = status.m_Id;
            Window window = {};
            if (current.m_Active && GetWindow(g_Recording.m_NativeWindow, &window) &&
                (window.m_Width != g_Recording.m_Window.m_Width ||
                 window.m_Height != g_Recording.m_Window.m_Height ||
                 window.m_Scale != g_Recording.m_Window.m_Scale))
            {
                current.m_Stopping = true;
            }
            {
                DM_MUTEX_SCOPED_LOCK(g_Recording.m_Mutex);
                if (current.m_Active && (current.m_Stopping || current.m_Failure[0]))
                {
                    // Finalize on the same worker that owns the native recorder.
                    dmLogWarning("Stopping video recording: %s", current.m_Failure[0] ? current.m_Failure : "capture ended or window size changed");
                    current.m_Stopping = true;
                    current.m_Pending = true;
                    g_Recording.m_Request = RECORDING_REQUEST_STOP;
                    dmConditionVariable::Signal(g_Recording.m_Condition);
                }
                g_Recording.m_Status = current;
            }
            if (!current.m_Active)
            {
                PublishCompletedStatus(&current);
            }
        }
    }

    static bool Fail(Status* status, const char* message)
    {
        dmStrlCpy(status->m_Failure, message, sizeof(status->m_Failure));
        return false;
    }

    bool Start(const StartParams* params, Status* status)
    {
        GetStatus(status);
        if (status->m_Pending || status->m_Active)
        {
            return Fail(status, "a recording operation is already active");
        }
        if (!IsSupported())
        {
            return Fail(status, GetUnsupportedReason());
        }
        if (!params->m_Path || !params->m_Path[0] || strlen(params->m_Path) >= sizeof(status->m_Path))
        {
            return Fail(status, "recording path is empty or too long");
        }
        if (params->m_Fps < 1 || params->m_Fps > 60)
        {
            return Fail(status, "fps must be between 1 and 60");
        }
        if (params->m_Audio && !IsAudioSupported())
        {
            return Fail(status, "application audio is not supported by this recording backend");
        }
        Window window = {};
        if (!params->m_Window)
        {
            return Fail(status, "recording window is unavailable");
        }
        // A caller may replace a completed operation before the next Update.
        // Preserve its receipt and restore its window before preparing another.
        PublishCompletedStatus(status);
        if (!PrepareWindow(params->m_Window, &window))
        {
            RestoreWindow();
            return Fail(status, "recording window is unavailable");
        }
        g_Recording.m_WindowPrepared = true;
        if (!g_Recording.m_Mutex)
        {
            g_Recording.m_Mutex = dmMutex::New();
            g_Recording.m_Condition = dmConditionVariable::New();
        }
        {
            DM_MUTEX_SCOPED_LOCK(g_Recording.m_Mutex);
            g_Recording.m_Window = window;
            g_Recording.m_NativeWindow = params->m_Window;
            memset(&g_Recording.m_Status, 0, sizeof(g_Recording.m_Status));
            Status* pending = &g_Recording.m_Status;
            pending->m_Id = ++g_OperationId;
            pending->m_Supported = true;
            pending->m_Pending = true;
            pending->m_Fps = params->m_Fps;
            pending->m_Audio = params->m_Audio;
            dmStrlCpy(pending->m_Path, params->m_Path, sizeof(pending->m_Path));
            g_Recording.m_Request = RECORDING_REQUEST_START;
            if (!g_Recording.m_Thread)
            {
                g_Recording.m_Thread = dmThread::New(RecordingWorker, 512 * 1024, &g_Recording, "record");
            }
            if (!g_Recording.m_Thread)
            {
                g_Recording.m_Request = RECORDING_REQUEST_NONE;
                pending->m_Pending = false;
                dmStrlCpy(pending->m_Failure, "recording worker unavailable", sizeof(pending->m_Failure));
            }
            else
            {
                dmConditionVariable::Signal(g_Recording.m_Condition);
            }
            *status = *pending;
        }
        if (!g_Recording.m_Thread)
        {
            RestorePreparedWindow();
            return false;
        }
        return true;
    }

    bool Stop(Status* status)
    {
        GetStatus(status);
        if (status->m_Pending || !status->m_Active)
        {
            return Fail(status, "no active recording is ready to stop");
        }
        // Preserve STARTED even if the caller stops before the next Update.
        PublishCompletedStatus(status);
        DM_MUTEX_SCOPED_LOCK(g_Recording.m_Mutex);
        g_Recording.m_Status.m_Pending = true;
        g_Recording.m_Status.m_Stopping = true;
        g_Recording.m_Request = RECORDING_REQUEST_STOP;
        *status = g_Recording.m_Status;
        dmConditionVariable::Signal(g_Recording.m_Condition);
        return true;
    }

    void Finalize()
    {
        // No worker calls into Lua, scene objects, or the main dispatch queue.
        // Join before the engine destroys the window and platform contexts.
        if (g_Recording.m_Thread)
        {
            {
                DM_MUTEX_SCOPED_LOCK(g_Recording.m_Mutex);
                g_Recording.m_Request = RECORDING_REQUEST_FINALIZE;
                dmConditionVariable::Signal(g_Recording.m_Condition);
            }
            dmThread::Join(g_Recording.m_Thread);
#if defined(_WIN32)
            // Win32 Join waits for termination; Detach closes the thread handle.
            dmThread::Detach(g_Recording.m_Thread);
#endif
        }
        RestorePreparedWindow();
        if (g_Recording.m_Condition)
        {
            dmConditionVariable::Delete(g_Recording.m_Condition);
        }
        if (g_Recording.m_Mutex)
        {
            dmMutex::Delete(g_Recording.m_Mutex);
        }
        memset(&g_Recording, 0, sizeof(g_Recording));
    }
} // namespace dmRecord
