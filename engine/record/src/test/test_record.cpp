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

struct RecordingBackend
{
    dmMutex::HMutex                         m_Mutex;
    dmConditionVariable::HConditionVariable m_Condition;
    dmThread::TlsKey                        m_ThreadKey;
    Status                                  m_Status;
    uint32_t                                m_Starts;
    uint32_t                                m_Stops;
    uint32_t                                m_Finalizes;
    uint32_t                                m_WrongThread;
    bool                                    m_AllowStart;
    bool                                    m_AllowStop;
    bool                                    m_FailStart;
    bool                                    m_FinalizeRequested;
    bool                                    m_Unsupported;
    bool                                    m_NoWindow;
};
static RecordingBackend g_Backend;

namespace dmRecord
{
    bool IsSupported()
    {
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
        memset(window, 0, sizeof(*window));
        window->m_Handle = 1;
        return !g_Backend.m_NoWindow;
    }

    void GetNativeStatus(Status* status)
    {
        dmMutex::ScopedLock lock(g_Backend.m_Mutex);
        *status = g_Backend.m_Status;
    }

    bool StartNative(const Window*, const char* path, uint32_t width, uint32_t height, uint32_t fps, bool audio, Status* status)
    {
        dmMutex::ScopedLock lock(g_Backend.m_Mutex);
        // TLS detects a replacement thread even if the OS reuses its thread ID.
        dmThread::SetTlsValue(g_Backend.m_ThreadKey, &g_Backend);
        ++g_Backend.m_Starts;
        dmConditionVariable::Broadcast(g_Backend.m_Condition);
        while (!g_Backend.m_AllowStart)
            dmConditionVariable::Wait(g_Backend.m_Condition, g_Backend.m_Mutex);
        memset(&g_Backend.m_Status, 0, sizeof(g_Backend.m_Status));
        g_Backend.m_Status.m_Supported = true;
        g_Backend.m_Status.m_Active = !g_Backend.m_FailStart;
        if (g_Backend.m_FailStart)
            dmStrlCpy(g_Backend.m_Status.m_Failure, "start failed", sizeof(g_Backend.m_Status.m_Failure));
        else
        {
            dmStrlCpy(g_Backend.m_Status.m_Path, path, sizeof(g_Backend.m_Status.m_Path));
            g_Backend.m_Status.m_Width = width;
            g_Backend.m_Status.m_Height = height;
            g_Backend.m_Status.m_Fps = fps;
            g_Backend.m_Status.m_Audio = audio;
        }
        *status = g_Backend.m_Status;
        return status->m_Active;
    }

    bool StopNative(Status* status)
    {
        dmMutex::ScopedLock lock(g_Backend.m_Mutex);
        if (dmThread::GetTlsValue(g_Backend.m_ThreadKey) != &g_Backend)
            ++g_Backend.m_WrongThread;
        ++g_Backend.m_Stops;
        dmConditionVariable::Broadcast(g_Backend.m_Condition);
        while (!g_Backend.m_AllowStop)
            dmConditionVariable::Wait(g_Backend.m_Condition, g_Backend.m_Mutex);
        g_Backend.m_Status.m_Active = false;
        g_Backend.m_Status.m_Finalized = true;
        *status = g_Backend.m_Status;
        return true;
    }

    void FinalizeNative()
    {
        bool active;
        {
            dmMutex::ScopedLock lock(g_Backend.m_Mutex);
            if (dmThread::GetTlsValue(g_Backend.m_ThreadKey) != &g_Backend)
                ++g_Backend.m_WrongThread;
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
        g_Backend.m_AllowStart = true;
        g_Backend.m_AllowStop = true;
        g_Backend.m_Status.m_Supported = true;
    }

    void TearDown()
    {
        AllowOperations();
        Finalize();
        dmThread::FreeTls(g_Backend.m_ThreadKey);
        dmConditionVariable::Delete(g_Backend.m_Condition);
        dmMutex::Delete(g_Backend.m_Mutex);
    }

    static bool Start(Status* status)
    {
        StartParams params;
        params.m_Window = (void*)1;
        params.m_Path = "recording-test.mp4";
        params.m_Width = 320;
        params.m_Height = 240;
        return dmRecord::Start(&params, status);
    }

    static void AllowOperations()
    {
        dmMutex::ScopedLock lock(g_Backend.m_Mutex);
        g_Backend.m_AllowStart = true;
        g_Backend.m_AllowStop = true;
        dmConditionVariable::Broadcast(g_Backend.m_Condition);
    }

    static void WaitForOperation(bool stopping)
    {
        dmMutex::ScopedLock lock(g_Backend.m_Mutex);
        while (!(stopping ? g_Backend.m_Stops : g_Backend.m_Starts))
            dmConditionVariable::Wait(g_Backend.m_Condition, g_Backend.m_Mutex);
    }

    static Status WaitForCompletion()
    {
        Status   status;
        uint64_t deadline = dmTime::GetMonotonicTime() + 5000000;
        // Wait for the public completion receipt, without assuming an operation duration.
        do
        {
            GetStatus(&status);
        } while (status.m_Pending && dmTime::GetMonotonicTime() < deadline);
        return status;
    }

    static void FinalizeWorker(void*)
    {
        {
            dmMutex::ScopedLock lock(g_Backend.m_Mutex);
            g_Backend.m_FinalizeRequested = true;
            dmConditionVariable::Broadcast(g_Backend.m_Condition);
        }
        Finalize();
    }

    static void FinalizeBlockedOperation()
    {
        dmThread::Thread thread = dmThread::New(FinalizeWorker, 512 * 1024, 0, "recording-finalize-test");
        {
            dmMutex::ScopedLock lock(g_Backend.m_Mutex);
            while (!g_Backend.m_FinalizeRequested)
                dmConditionVariable::Wait(g_Backend.m_Condition, g_Backend.m_Mutex);
        }
        AllowOperations();
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
    {
        dmMutex::ScopedLock lock(g_Backend.m_Mutex);
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
    params.m_Width = 320;
    ASSERT_FALSE(dmRecord::Start(&params, &status));
    params.m_Height = 16385;
    ASSERT_FALSE(dmRecord::Start(&params, &status));
    params.m_Height = 240;
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
