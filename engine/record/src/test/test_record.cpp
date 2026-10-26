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

#define JC_TEST_IMPLEMENTATION
#include <jc_test/jc_test.h>
#include <dmsdk/dlib/condition_variable.h>
#include <dmsdk/dlib/dstrings.h>
#include <dmsdk/dlib/thread.h>
#include <dmsdk/dlib/time.h>
#include <string.h>

#include "record/record_private.h"

using namespace dmRecord;

// Localized recording failures must retain their UTF-8 bytes for JSON responses.
TEST(RecordingText, UnicodeMessage)
{
    const char message[] = "Error: \xc3\xa9\xd0\x96\xe6\x97\xa5\xf0\x9f\x8e\xa5";
    char output[sizeof(message)];
    CopyTextUtf8(output, sizeof(output), message);
    ASSERT_STREQ(message, output);
    CopyTextUtf8(output, sizeof(output), 0);
    ASSERT_STREQ("", output);
    output[0] = '!';
    CopyTextUtf8(output, 0, message);
    ASSERT_EQ('!', output[0]);
}

// Truncating two-, three- or four-byte characters must not corrupt JSON UTF-8.
TEST(RecordingText, TruncateAtCharacterBoundary)
{
    const char message[] = "\xc3\xa9\xe6\x97\xa5\xf0\x9f\x8e\xa5";
    const char* expected[] = {
        "", "", "\xc3\xa9", "\xc3\xa9", "\xc3\xa9", "\xc3\xa9\xe6\x97\xa5", "\xc3\xa9\xe6\x97\xa5", "\xc3\xa9\xe6\x97\xa5", "\xc3\xa9\xe6\x97\xa5", message
    };
    for (size_t size = 1; size <= sizeof(message); ++size)
    {
        char output[sizeof(message) + 1];
        memset(output, '!', sizeof(output));
        CopyTextUtf8(output, size, message);
        ASSERT_STREQ(expected[size - 1], output);
        ASSERT_EQ('!', output[size]);
    }
}

// Long macOS/Windows error descriptions must not end with a partial UTF-8 character.
TEST(RecordingText, StatusFailureLimit)
{
    Status status = {};
    char message[sizeof(status.m_Failure) + 2];
    memset(message, 'x', sizeof(message));
    memcpy(message + sizeof(status.m_Failure) - 2, "\xe6\x97\xa5", 4);
    CopyTextUtf8(status.m_Failure, sizeof(status.m_Failure), message);
    ASSERT_EQ(sizeof(status.m_Failure) - 2, strlen(status.m_Failure));
    ASSERT_EQ(0, memcmp(message, status.m_Failure, sizeof(status.m_Failure) - 2));
}

struct RecordingBackend
{
    dmMutex::HMutex                         m_Mutex;
    dmConditionVariable::HConditionVariable m_Condition;
    dmThread::TlsKey                        m_ThreadKey;
    Status                                  m_Status;
    uint32_t                                m_WindowWidth;
    uint32_t                                m_WindowHeight;
    uint32_t                                m_Starts;
    uint32_t                                m_Stops;
    uint32_t                                m_Finalizes;
    uint32_t                                m_WrongThread;
    uint32_t                                m_WindowPrepares;
    uint32_t                                m_WindowRestores;
    uint32_t                                m_EarlyRestores;
    uint32_t                                m_StatusQueries;
    uint32_t                                m_WindowQueries;
    uint32_t                                m_SupportQueries;
    uint32_t                                m_Notifications;
    bool                                    m_AllowStart;
    bool                                    m_AllowStop;
    bool                                    m_FailStart;
    bool                                    m_FailStop;
    bool                                    m_FailPrepare;
    bool                                    m_WindowPrepared;
    bool                                    m_FinalizeRequested;
    bool                                    m_Unsupported;
    bool                                    m_NoWindow;
};
static RecordingBackend g_Backend;

static void OnStatus(const Status*)
{
    ++g_Backend.m_Notifications;
}
static StatusListener g_Listener = { OnStatus, 0 };

namespace dmRecord
{
    bool IsSupported()
    {
        ++g_Backend.m_SupportQueries;
        return !g_Backend.m_Unsupported;
    }
    bool IsAudioSupported()
    {
        return false;
    }
    const char* GetUnsupportedReason()
    {
        return "unsupported test backend";
    }

    bool GetWindow(void*, Window* window)
    {
        ++g_Backend.m_WindowQueries;
        memset(window, 0, sizeof(*window));
        window->m_Handle = 1;
        window->m_Width = g_Backend.m_WindowWidth;
        window->m_Height = g_Backend.m_WindowHeight;
        window->m_Scale = 1.0;
        return !g_Backend.m_NoWindow;
    }

    bool PrepareWindow(void* native_window, Window* window)
    {
        DM_MUTEX_SCOPED_LOCK(g_Backend.m_Mutex);
        if (dmThread::GetTlsValue(g_Backend.m_ThreadKey) != &g_Backend.m_WindowPrepared)
        {
            ++g_Backend.m_WrongThread;
        }
        ++g_Backend.m_WindowPrepares;
        g_Backend.m_WindowPrepared = true;
        return !g_Backend.m_FailPrepare && GetWindow(native_window, window);
    }

    void RestoreWindow()
    {
        DM_MUTEX_SCOPED_LOCK(g_Backend.m_Mutex);
        if (dmThread::GetTlsValue(g_Backend.m_ThreadKey) != &g_Backend.m_WindowPrepared)
        {
            ++g_Backend.m_WrongThread;
        }
        if (g_Backend.m_Status.m_Active)
        {
            ++g_Backend.m_EarlyRestores;
        }
        ++g_Backend.m_WindowRestores;
        g_Backend.m_WindowPrepared = false;
    }

    void GetNativeStatus(Status* status)
    {
        DM_MUTEX_SCOPED_LOCK(g_Backend.m_Mutex);
        ++g_Backend.m_StatusQueries;
        *status = g_Backend.m_Status;
    }

    bool StartNative(const Window* window, const char* path, uint32_t fps, bool audio, Status* status)
    {
        DM_MUTEX_SCOPED_LOCK(g_Backend.m_Mutex);
        // TLS detects a replacement thread even if the OS reuses its thread ID.
        dmThread::SetTlsValue(g_Backend.m_ThreadKey, &g_Backend);
        ++g_Backend.m_Starts;
        dmConditionVariable::Broadcast(g_Backend.m_Condition);
        while (!g_Backend.m_AllowStart)
        {
            dmConditionVariable::Wait(g_Backend.m_Condition, g_Backend.m_Mutex);
        }
        memset(&g_Backend.m_Status, 0, sizeof(g_Backend.m_Status));
        g_Backend.m_Status.m_Supported = true;
        g_Backend.m_Status.m_Active = !g_Backend.m_FailStart;
        if (g_Backend.m_FailStart)
        {
            dmStrlCpy(g_Backend.m_Status.m_Failure, "start failed", sizeof(g_Backend.m_Status.m_Failure));
        }
        else
        {
            dmStrlCpy(g_Backend.m_Status.m_Path, path, sizeof(g_Backend.m_Status.m_Path));
            g_Backend.m_Status.m_Width = (uint32_t)window->m_Width;
            g_Backend.m_Status.m_Height = (uint32_t)window->m_Height;
            g_Backend.m_Status.m_Fps = fps;
            g_Backend.m_Status.m_Audio = audio;
        }
        *status = g_Backend.m_Status;
        return status->m_Active;
    }

    bool StopNative(Status* status)
    {
        DM_MUTEX_SCOPED_LOCK(g_Backend.m_Mutex);
        if (dmThread::GetTlsValue(g_Backend.m_ThreadKey) != &g_Backend)
        {
            ++g_Backend.m_WrongThread;
        }
        ++g_Backend.m_Stops;
        dmConditionVariable::Broadcast(g_Backend.m_Condition);
        while (!g_Backend.m_AllowStop)
        {
            dmConditionVariable::Wait(g_Backend.m_Condition, g_Backend.m_Mutex);
        }
        g_Backend.m_Status.m_Active = false;
        g_Backend.m_Status.m_Finalized = !g_Backend.m_FailStop && !g_Backend.m_Status.m_Failure[0];
        if (g_Backend.m_FailStop)
        {
            dmStrlCpy(g_Backend.m_Status.m_Failure, "stop failed", sizeof(g_Backend.m_Status.m_Failure));
        }
        *status = g_Backend.m_Status;
        return status->m_Finalized;
    }

    void FinalizeNative()
    {
        bool active;
        {
            DM_MUTEX_SCOPED_LOCK(g_Backend.m_Mutex);
            if (dmThread::GetTlsValue(g_Backend.m_ThreadKey) != &g_Backend)
            {
                ++g_Backend.m_WrongThread;
            }
            ++g_Backend.m_Finalizes;
            active = g_Backend.m_Status.m_Active;
        }
        if (active)
        {
            Status status;
            StopNative(&status);
        }
        dmThread::SetTlsValue(g_Backend.m_ThreadKey, 0);
    }

} // namespace dmRecord

class RecordingAsyncTest : public jc_test_base_class
{
protected:
    void SetUp()
    {
        memset(&g_Backend, 0, sizeof(g_Backend));
        g_Backend.m_Mutex = dmMutex::New();
        g_Backend.m_Condition = dmConditionVariable::New();
        g_Backend.m_ThreadKey = dmThread::AllocTls();
        dmThread::SetTlsValue(g_Backend.m_ThreadKey, &g_Backend.m_WindowPrepared);
        g_Backend.m_AllowStart = true;
        g_Backend.m_AllowStop = true;
        g_Backend.m_Status.m_Supported = true;
        g_Backend.m_WindowWidth = 320;
        g_Backend.m_WindowHeight = 240;
        AddStatusListener(&g_Listener);
    }

    void TearDown()
    {
        AllowOperations();
        Finalize();
        RemoveStatusListener(&g_Listener);
        ASSERT_EQ(g_Backend.m_WindowPrepares, g_Backend.m_WindowRestores);
        ASSERT_FALSE(g_Backend.m_WindowPrepared);
        ASSERT_EQ(0u, g_Backend.m_EarlyRestores);
        ASSERT_EQ(0u, g_Backend.m_WrongThread);
        dmThread::FreeTls(g_Backend.m_ThreadKey);
        dmConditionVariable::Delete(g_Backend.m_Condition);
        dmMutex::Delete(g_Backend.m_Mutex);
    }

    static bool Start(Status* status)
    {
        StartParams params;
        params.m_Window = (void*)1;
        params.m_Path = "recording-test.mp4";
        return dmRecord::Start(&params, status);
    }

    static void AllowOperations()
    {
        DM_MUTEX_SCOPED_LOCK(g_Backend.m_Mutex);
        g_Backend.m_AllowStart = true;
        g_Backend.m_AllowStop = true;
        dmConditionVariable::Broadcast(g_Backend.m_Condition);
    }

    static void WaitForOperation(bool stopping)
    {
        DM_MUTEX_SCOPED_LOCK(g_Backend.m_Mutex);
        while (!(stopping ? g_Backend.m_Stops : g_Backend.m_Starts))
        {
            dmConditionVariable::Wait(g_Backend.m_Condition, g_Backend.m_Mutex);
        }
    }

    static Status WaitForCompletion(bool update = true)
    {
        Status status;
        uint64_t deadline = dmTime::GetMonotonicTime() + 5000000;
        // Wait for the public completion receipt, without assuming an operation duration.
        do
        {
            GetStatus(&status);
            if (update && !status.m_Pending)
            {
                bool was_active = status.m_Active;
                Update();
                GetStatus(&status);
                // An automatic stop can finish before this read. Consume its
                // terminal receipt before checking cleanup or observers.
                if (was_active && !status.m_Pending && !status.m_Active)
                {
                    Update();
                }
            }
        } while (status.m_Pending && dmTime::GetMonotonicTime() < deadline);
        return status;
    }

    static void AllowOperationsWorker(void*)
    {
        {
            DM_MUTEX_SCOPED_LOCK(g_Backend.m_Mutex);
            while (!g_Backend.m_FinalizeRequested)
            {
                dmConditionVariable::Wait(g_Backend.m_Condition, g_Backend.m_Mutex);
            }
        }
        AllowOperations();
    }

    static void FinalizeBlockedOperation()
    {
        dmThread::Thread thread = dmThread::New(AllowOperationsWorker, 512 * 1024, 0, "recording-unblock-test");
        {
            DM_MUTEX_SCOPED_LOCK(g_Backend.m_Mutex);
            g_Backend.m_FinalizeRequested = true;
            dmConditionVariable::Broadcast(g_Backend.m_Condition);
        }
        // Window restoration belongs to the caller's main thread, even when
        // finalization must wait for a blocked native start/stop operation.
        Finalize();
        dmThread::Join(thread);
#if defined(_WIN32)
        dmThread::Detach(thread);
#endif
    }
};

// Repeated recordings retain operation receipts and balance native start/stop/cleanup on one thread.
TEST_F(RecordingAsyncTest, StartStopSameThread)
{
    uint64_t previous_id = 0;
    for (uint32_t i = 0; i < 2; ++i)
    {
        Status status;
        ASSERT_TRUE(Start(&status));
        ASSERT_TRUE(status.m_Pending);
        ASSERT_TRUE(g_Backend.m_WindowPrepared);
        ASSERT_GT(status.m_Id, previous_id);
        previous_id = status.m_Id;
        status = WaitForCompletion();
        ASSERT_FALSE(status.m_Pending);
        ASSERT_TRUE(status.m_Active);
        ASSERT_EQ(previous_id, status.m_Id);
        ASSERT_EQ(320u, status.m_Width);
        ASSERT_STREQ("recording-test.mp4", status.m_Path);
        ASSERT_TRUE(Stop(&status));
        ASSERT_TRUE(status.m_Pending);
        ASSERT_TRUE(status.m_Stopping);
        status = WaitForCompletion();
        ASSERT_FALSE(status.m_Pending);
        ASSERT_FALSE(status.m_Active);
        ASSERT_TRUE(status.m_Finalized);
        ASSERT_FALSE(g_Backend.m_WindowPrepared);
        ASSERT_EQ(previous_id, status.m_Id);
    }
    Finalize();
    ASSERT_EQ(2u, g_Backend.m_Starts);
    ASSERT_EQ(2u, g_Backend.m_Stops);
    ASSERT_EQ(1u, g_Backend.m_Finalizes);
    ASSERT_EQ(0u, g_Backend.m_WrongThread);
    Status reset;
    GetStatus(&reset);
    ASSERT_EQ(0u, reset.m_Id);
    ASSERT_FALSE(reset.m_Active);
    ASSERT_FALSE(reset.m_Finalized);
    ASSERT_STREQ("", reset.m_Path);
}

// Status reads must not poll native capture, notify observers, stop on resize or restore windows.
TEST_F(RecordingAsyncTest, GetStatusDoesNotAdvanceRecording)
{
    Status status;
    ASSERT_TRUE(Start(&status));
    status = WaitForCompletion(false);
    ASSERT_TRUE(status.m_Active);
    uint32_t notifications = g_Backend.m_Notifications;
    uint32_t support_queries = g_Backend.m_SupportQueries;
    uint32_t window_queries = g_Backend.m_WindowQueries;
    g_Backend.m_WindowWidth = 640;
    g_Backend.m_AllowStop = false;
    for (uint32_t i = 0; i < 10; ++i)
    {
        GetStatus(&status);
        ASSERT_TRUE(status.m_Active);
        ASSERT_FALSE(status.m_Pending);
    }
    ASSERT_EQ(0u, g_Backend.m_StatusQueries);
    ASSERT_EQ(notifications, g_Backend.m_Notifications);
    ASSERT_EQ(support_queries, g_Backend.m_SupportQueries);
    ASSERT_EQ(window_queries, g_Backend.m_WindowQueries);

    Update();
    GetStatus(&status);
    ASSERT_TRUE(status.m_Pending);
    ASSERT_TRUE(status.m_Stopping);
    ASSERT_EQ(1u, g_Backend.m_StatusQueries);
    ASSERT_EQ(notifications + 1, g_Backend.m_Notifications);
    WaitForOperation(true);
    AllowOperations();
    status = WaitForCompletion(false);
    ASSERT_FALSE(status.m_Pending);
    ASSERT_FALSE(status.m_Active);
    ASSERT_TRUE(status.m_Finalized);
    ASSERT_TRUE(g_Backend.m_WindowPrepared);
    for (uint32_t i = 0; i < 10; ++i)
    {
        GetStatus(&status);
    }
    ASSERT_EQ(0u, g_Backend.m_WindowRestores);
    ASSERT_EQ(notifications + 1, g_Backend.m_Notifications);
    ASSERT_EQ(1u, g_Backend.m_StatusQueries);

    Update();
    ASSERT_FALSE(g_Backend.m_WindowPrepared);
    ASSERT_EQ(1u, g_Backend.m_WindowRestores);
    ASSERT_EQ(notifications + 2, g_Backend.m_Notifications);
    GetStatus(&status);
    ASSERT_EQ(notifications + 2, g_Backend.m_Notifications);
}

// A content-size change finalizes on the owning worker and retains the original video dimensions.
TEST_F(RecordingAsyncTest, ResizeFinalizesRecording)
{
    Status status;
    ASSERT_TRUE(Start(&status));
    status = WaitForCompletion();
    ASSERT_TRUE(status.m_Active);
    uint64_t id = status.m_Id;
    g_Backend.m_WindowWidth = 640;
    status = WaitForCompletion();
    ASSERT_FALSE(status.m_Pending);
    ASSERT_FALSE(status.m_Active);
    ASSERT_TRUE(status.m_Finalized);
    ASSERT_TRUE(status.m_Stopping);
    ASSERT_FALSE(g_Backend.m_WindowPrepared);
    ASSERT_EQ(id, status.m_Id);
    ASSERT_EQ(320u, status.m_Width);
    ASSERT_EQ(240u, status.m_Height);
    ASSERT_EQ(0, status.m_Failure[0]);
    ASSERT_EQ(1u, g_Backend.m_Stops);
    ASSERT_EQ(0u, g_Backend.m_WrongThread);
}

// A native capture resize signal also schedules finalization on the owning worker.
TEST_F(RecordingAsyncTest, NativeResizeFinalizesRecording)
{
    Status status;
    ASSERT_TRUE(Start(&status));
    status = WaitForCompletion();
    ASSERT_TRUE(status.m_Active);
    {
        DM_MUTEX_SCOPED_LOCK(g_Backend.m_Mutex);
        g_Backend.m_Status.m_Stopping = true;
    }
    status = WaitForCompletion();
    ASSERT_FALSE(status.m_Pending);
    ASSERT_FALSE(status.m_Active);
    ASSERT_TRUE(status.m_Finalized);
    ASSERT_EQ(1u, g_Backend.m_Stops);
    ASSERT_EQ(0u, g_Backend.m_WrongThread);
}

// Shutdown during startup waits for the worker and cleans up on the initializing thread.
TEST_F(RecordingAsyncTest, FinalizeDuringStart)
{
    g_Backend.m_AllowStart = false;
    Status status;
    ASSERT_TRUE(Start(&status));
    WaitForOperation(false);
    ASSERT_FALSE(Start(&status));
    ASSERT_FALSE(Stop(&status));
    ASSERT_TRUE(status.m_Pending);
    FinalizeBlockedOperation();
    ASSERT_EQ(1u, g_Backend.m_Stops);
    ASSERT_EQ(1u, g_Backend.m_Finalizes);
    ASSERT_EQ(0u, g_Backend.m_WrongThread);
}

// Shutdown of an active recording must finalize native resources on the original worker.
TEST_F(RecordingAsyncTest, FinalizeWhileActive)
{
    Status status;
    ASSERT_TRUE(Start(&status));
    status = WaitForCompletion();
    ASSERT_FALSE(status.m_Pending);
    ASSERT_TRUE(status.m_Active);
    Finalize();
    ASSERT_EQ(1u, g_Backend.m_Stops);
    ASSERT_EQ(1u, g_Backend.m_Finalizes);
    ASSERT_EQ(0u, g_Backend.m_WrongThread);
}

// Shutdown during stop joins its completion without running cleanup on the caller thread.
TEST_F(RecordingAsyncTest, FinalizeDuringStop)
{
    g_Backend.m_AllowStop = false;
    Status status;
    ASSERT_TRUE(Start(&status));
    status = WaitForCompletion();
    ASSERT_FALSE(status.m_Pending);
    ASSERT_TRUE(Stop(&status));
    WaitForOperation(true);
    ASSERT_FALSE(Stop(&status));
    ASSERT_TRUE(status.m_Pending);
    FinalizeBlockedOperation();
    ASSERT_EQ(1u, g_Backend.m_Stops);
    ASSERT_EQ(1u, g_Backend.m_Finalizes);
    ASSERT_EQ(0u, g_Backend.m_WrongThread);
}

// A failed start retains its path for caller cleanup and allows retry on the same worker.
TEST_F(RecordingAsyncTest, RetryFailedStart)
{
    g_Backend.m_FailStart = true;
    Status status;
    ASSERT_TRUE(Start(&status));
    status = WaitForCompletion();
    ASSERT_FALSE(status.m_Pending);
    ASSERT_FALSE(status.m_Active);
    ASSERT_STREQ("start failed", status.m_Failure);
    ASSERT_STREQ("recording-test.mp4", status.m_Path);
    ASSERT_FALSE(g_Backend.m_WindowPrepared);
    {
        DM_MUTEX_SCOPED_LOCK(g_Backend.m_Mutex);
        g_Backend.m_FailStart = false;
    }
    ASSERT_TRUE(Start(&status));
    status = WaitForCompletion();
    ASSERT_FALSE(status.m_Pending);
    ASSERT_TRUE(status.m_Active);
    ASSERT_STREQ("", status.m_Failure);
    Finalize();
    ASSERT_EQ(2u, g_Backend.m_Starts);
    ASSERT_EQ(1u, g_Backend.m_Finalizes);
    ASSERT_EQ(0u, g_Backend.m_WrongThread);
}

// A failed window preparation must restore decorations before returning to the caller.
TEST_F(RecordingAsyncTest, FailedWindowPreparation)
{
    g_Backend.m_FailPrepare = true;
    Status status;
    ASSERT_FALSE(Start(&status));
    ASSERT_FALSE(g_Backend.m_WindowPrepared);
    ASSERT_EQ(1u, g_Backend.m_WindowRestores);
    ASSERT_EQ(0u, g_Backend.m_Starts);
}

// Decorations remain removed until a pending stop completes, including failed finalization.
TEST_F(RecordingAsyncTest, FailedStopRestoresWindow)
{
    Status status;
    ASSERT_TRUE(Start(&status));
    status = WaitForCompletion();
    ASSERT_TRUE(status.m_Active);
    g_Backend.m_AllowStop = false;
    g_Backend.m_FailStop = true;
    ASSERT_TRUE(Stop(&status));
    WaitForOperation(true);
    GetStatus(&status);
    ASSERT_TRUE(status.m_Pending);
    ASSERT_TRUE(g_Backend.m_WindowPrepared);
    ASSERT_EQ(0u, g_Backend.m_WindowRestores);
    AllowOperations();
    status = WaitForCompletion();
    ASSERT_FALSE(status.m_Active);
    ASSERT_FALSE(status.m_Finalized);
    ASSERT_STREQ("stop failed", status.m_Failure);
    ASSERT_FALSE(g_Backend.m_WindowPrepared);
    GetStatus(&status);
    ASSERT_EQ(1u, g_Backend.m_WindowRestores);
}

// Native errors without a stopping flag must finish cleanup before restoring the window.
TEST_F(RecordingAsyncTest, FailedCaptureRestoresWindow)
{
    Status status;
    ASSERT_TRUE(Start(&status));
    status = WaitForCompletion();
    ASSERT_TRUE(status.m_Active);
    {
        DM_MUTEX_SCOPED_LOCK(g_Backend.m_Mutex);
        g_Backend.m_AllowStop = false;
        dmStrlCpy(g_Backend.m_Status.m_Failure, "capture failed", sizeof(g_Backend.m_Status.m_Failure));
    }
    Update();
    GetStatus(&status);
    ASSERT_TRUE(status.m_Pending);
    ASSERT_TRUE(status.m_Stopping);
    WaitForOperation(true);
    ASSERT_TRUE(g_Backend.m_WindowPrepared);
    ASSERT_EQ(0u, g_Backend.m_WindowRestores);
    AllowOperations();
    status = WaitForCompletion();
    ASSERT_FALSE(status.m_Pending);
    ASSERT_FALSE(status.m_Active);
    ASSERT_FALSE(status.m_Finalized);
    ASSERT_STREQ("capture failed", status.m_Failure);
    ASSERT_FALSE(g_Backend.m_WindowPrepared);
    GetStatus(&status);
    ASSERT_EQ(1u, g_Backend.m_Stops);
    ASSERT_EQ(1u, g_Backend.m_WindowRestores);
}

// Finalization without a recording must not invoke native cleanup or create a worker.
TEST_F(RecordingAsyncTest, FinalizeWithoutStart)
{
    Finalize();
    ASSERT_EQ(0u, g_Backend.m_Starts);
    ASSERT_EQ(0u, g_Backend.m_Finalizes);
}

// Invalid options fail before a native operation or worker is created.
TEST_F(RecordingAsyncTest, InvalidOptions)
{
    StartParams params;
    params.m_Window = (void*)1;
    params.m_Path = "recording-test.mp4";
    Status status;
    params.m_Fps = 0;
    ASSERT_FALSE(dmRecord::Start(&params, &status));
    params.m_Fps = 61;
    ASSERT_FALSE(dmRecord::Start(&params, &status));
    params.m_Fps = 30;
    params.m_Audio = true;
    ASSERT_FALSE(dmRecord::Start(&params, &status));
    params.m_Audio = false;
    params.m_Path = "";
    ASSERT_FALSE(dmRecord::Start(&params, &status));
    char long_path[1025];
    memset(long_path, 'a', sizeof(long_path) - 1);
    long_path[sizeof(long_path) - 1] = 0;
    params.m_Path = long_path;
    ASSERT_FALSE(dmRecord::Start(&params, &status));
    params.m_Path = "recording-test.mp4";
    params.m_Window = 0;
    ASSERT_FALSE(dmRecord::Start(&params, &status));
    params.m_Window = (void*)1;
    g_Backend.m_NoWindow = true;
    ASSERT_FALSE(dmRecord::Start(&params, &status));
    g_Backend.m_NoWindow = false;
    g_Backend.m_Unsupported = true;
    ASSERT_FALSE(dmRecord::Start(&params, &status));
    ASSERT_EQ(0u, g_Backend.m_Starts);
}

int main(int argc, char** argv)
{
    jc_test_init(&argc, argv);
    return jc_test_run_all();
}
