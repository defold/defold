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

#include <dlib/condition_variable.h>

#if defined(_WIN32)
#include <dmsdk/dlib/safe_windows.h>
#else
#include <pthread.h>
#endif

static void FinishProcessExitRegression();

// Run after the profiler's static initializer is destroyed, so an exit callback
// remains active throughout any profiler cleanup performed by static destructors.
struct ProcessExitRegression
{
    ~ProcessExitRegression()
    {
        FinishProcessExitRegression();
    }
};

static ProcessExitRegression g_ProcessExitRegression;

// Inspect retained allocations without adding a production API for the tests.
#include "../basic/profiler_basic.cpp"

struct CapturedSamples
{
    uint32_t m_Count;
    bool     m_Valid;
};

static void CaptureSample(void* ctx, const char* thread_name, dmProfiler::HSample sample)
{
    CapturedSamples* captured = (CapturedSamples*)ctx;
    ++captured->m_Count;
    captured->m_Valid = captured->m_Valid && strcmp(thread_name, "ShortLived") == 0 && strcmp(dmProfiler::SampleGetName(sample), "thread_scope") == 0;
}

static void ProfileThread(void* arg)
{
    ProfileResult* result = (ProfileResult*)arg;
    *result = ProfileSetThreadName("ShortLived");
    if (*result != PROFILE_RESULT_OK)
    {
        return;
    }
    uint64_t name_hash = 0;
    *result = ProfileScopeBegin("thread_scope", &name_hash);
    if (*result == PROFILE_RESULT_OK)
    {
        *result = ProfileScopeEnd("thread_scope", name_hash);
    }
}

static void NameThread(void* arg)
{
    *(ProfileResult*)arg = ProfileSetThreadName("NameOnly");
}

class ProfilerBasicTest : public jc_test_base_class
{
    public:
    void SetUp()
    {
        ProfileInitialize();
        ProfileRegisterProfiler(g_ProfilerName, &g_Listener);
        m_Samples.m_Count = 0;
        m_Samples.m_Valid = true;
        dmProfiler::SetSampleTreeCallback(&m_Samples, CaptureSample);
    }

    void TearDown()
    {
        dmProfiler::SetSampleTreeCallback(0, 0);
        if (ProfileIsInitialized())
        {
            ProfileFinalize();
        }
        ProfileUnregisterProfiler(g_ProfilerName);
    }

    CapturedSamples m_Samples;
};

// Verifies that completed samples are delivered before each worker's pool and
// name are reclaimed, guarding against the per-load growth reported in #13704.
TEST_F(ProfilerBasicTest, ReclaimsExitedThreads)
{
    uint64_t main_name_hash = 0;
    ASSERT_EQ(PROFILE_RESULT_OK, ProfileScopeBegin("main_scope", &main_name_hash));
    uint32_t thread_count = g_ProfileContext->m_ThreadData.Size();
    uint32_t name_count = g_ProfileContext->m_ThreadNames.Size();
    for (uint32_t i = 0; i < 32; ++i)
    {
        ProfileResult result = PROFILE_RESULT_NOT_INITIALIZED;
        dmThread::Thread thread = dmThread::New(ProfileThread, 0x80000, &result, "profiler_test");
        dmThread::Join(thread);

        ASSERT_EQ(PROFILE_RESULT_OK, result);
        ASSERT_EQ(thread_count, g_ProfileContext->m_ThreadData.Size());
        ASSERT_EQ(name_count, g_ProfileContext->m_ThreadNames.Size());
    }
    ASSERT_EQ(32u, m_Samples.m_Count);
    ASSERT_TRUE(m_Samples.m_Valid);
    dmProfiler::SetSampleTreeCallback(0, 0);
    ASSERT_EQ(PROFILE_RESULT_OK, ProfileScopeEnd("main_scope", main_name_hash));
}

// Verifies that naming a thread without opening a scope does not retain its
// name after exit, guarding against cleanup being tied only to sample allocation.
TEST_F(ProfilerBasicTest, ReclaimsNamedThreadsWithoutSamples)
{
    uint32_t thread_count = g_ProfileContext->m_ThreadData.Size();
    uint32_t name_count = g_ProfileContext->m_ThreadNames.Size();
    ProfileResult result = PROFILE_RESULT_NOT_INITIALIZED;
    dmThread::Thread thread = dmThread::New(NameThread, 0x80000, &result, "name_only");
    dmThread::Join(thread);

    ASSERT_EQ(PROFILE_RESULT_OK, result);
    ASSERT_EQ(thread_count, g_ProfileContext->m_ThreadData.Size());
    ASSERT_EQ(name_count, g_ProfileContext->m_ThreadNames.Size());
    ASSERT_EQ(0u, m_Samples.m_Count);
}

#if defined(_WIN32)
static DWORD WINAPI NativeProfileThread(void* arg)
{
    ProfileThread(arg);
    return 0;
}
#else
static void* NativeProfileThread(void* arg)
{
    ProfileThread(arg);
    return 0;
}
#endif

// Verifies exit cleanup for threads created outside dmThread, guarding against
// native extension threads retaining profiler pools and names indefinitely.
TEST_F(ProfilerBasicTest, ReclaimsNativeThreads)
{
    uint32_t thread_count = g_ProfileContext->m_ThreadData.Size();
    uint32_t name_count = g_ProfileContext->m_ThreadNames.Size();
    ProfileResult result = PROFILE_RESULT_NOT_INITIALIZED;
#if defined(_WIN32)
    HANDLE thread = CreateThread(0, 0, NativeProfileThread, &result, 0, 0);
    ASSERT_NE((HANDLE)0, thread);
    ASSERT_EQ((DWORD)WAIT_OBJECT_0, WaitForSingleObject(thread, INFINITE));
    ASSERT_TRUE(CloseHandle(thread));
#else
    pthread_t thread;
    ASSERT_EQ(0, pthread_create(&thread, 0, NativeProfileThread, &result));
    ASSERT_EQ(0, pthread_join(thread, 0));
#endif
    ASSERT_EQ(PROFILE_RESULT_OK, result);
    ASSERT_EQ(thread_count, g_ProfileContext->m_ThreadData.Size());
    ASSERT_EQ(name_count, g_ProfileContext->m_ThreadNames.Size());
    ASSERT_EQ(1u, m_Samples.m_Count);
    ASSERT_TRUE(m_Samples.m_Valid);
}

struct WaitingThreadContext
{
    dmMutex::HMutex                         m_Mutex;
    dmConditionVariable::HConditionVariable m_Changed;
    ProfileResult                           m_Result;
    bool                                    m_Ready;
    bool                                    m_Exit;
    bool                                    m_ProfileAfterWait;
};

static void WaitingThread(void* arg)
{
    WaitingThreadContext* context = (WaitingThreadContext*)arg;
    ProfileThread(&context->m_Result);
    {
        DM_MUTEX_SCOPED_LOCK(context->m_Mutex);
        context->m_Ready = true;
        dmConditionVariable::Signal(context->m_Changed);
        while (!context->m_Exit)
        {
            dmConditionVariable::Wait(context->m_Changed, context->m_Mutex);
        }
    }
    if (context->m_ProfileAfterWait)
    {
        ProfileThread(&context->m_Result);
    }
}

// Verifies that an exit callback after profiler shutdown can safely skip the
// already-released data, guarding against use of the destroyed profiler mutex.
TEST_F(ProfilerBasicTest, ThreadExitsAfterProfilerShutdown)
{
    WaitingThreadContext context = {};
    context.m_Mutex = dmMutex::New();
    context.m_Changed = dmConditionVariable::New();
    dmThread::Thread thread = dmThread::New(WaitingThread, 0x80000, &context, "waiting_profiler");
    {
        DM_MUTEX_SCOPED_LOCK(context.m_Mutex);
        while (!context.m_Ready)
        {
            dmConditionVariable::Wait(context.m_Changed, context.m_Mutex);
        }
    }

    ProfileFinalize();
    {
        DM_MUTEX_SCOPED_LOCK(context.m_Mutex);
        context.m_Exit = true;
        dmConditionVariable::Signal(context.m_Changed);
    }
    dmThread::Join(thread);
    dmConditionVariable::Delete(context.m_Changed);
    dmMutex::Delete(context.m_Mutex);

    ASSERT_EQ(PROFILE_RESULT_OK, context.m_Result);
    ASSERT_EQ((ProfileContext*)0, g_ProfileContext);
    ASSERT_EQ(1u, m_Samples.m_Count);
    ASSERT_TRUE(m_Samples.m_Valid);
}

// Verifies that a thread can register fresh samples after profiler restart and
// reclaim only its current records on exit, guarding against stale TLS state.
TEST_F(ProfilerBasicTest, ThreadSurvivesProfilerRestart)
{
    WaitingThreadContext context = {};
    context.m_Mutex = dmMutex::New();
    context.m_Changed = dmConditionVariable::New();
    context.m_ProfileAfterWait = true;
    dmThread::Thread thread = dmThread::New(WaitingThread, 0x80000, &context, "restarting_profiler");
    {
        DM_MUTEX_SCOPED_LOCK(context.m_Mutex);
        while (!context.m_Ready)
        {
            dmConditionVariable::Wait(context.m_Changed, context.m_Mutex);
        }
    }

    ProfileFinalize();
    ProfileInitialize();
    uint32_t thread_count = g_ProfileContext->m_ThreadData.Size();
    uint32_t name_count = g_ProfileContext->m_ThreadNames.Size();
    {
        DM_MUTEX_SCOPED_LOCK(context.m_Mutex);
        context.m_Exit = true;
        dmConditionVariable::Signal(context.m_Changed);
    }
    dmThread::Join(thread);
    dmConditionVariable::Delete(context.m_Changed);
    dmMutex::Delete(context.m_Mutex);

    ASSERT_EQ(PROFILE_RESULT_OK, context.m_Result);
    ASSERT_EQ(thread_count, g_ProfileContext->m_ThreadData.Size());
    ASSERT_EQ(name_count, g_ProfileContext->m_ThreadNames.Size());
    ASSERT_EQ(2u, m_Samples.m_Count);
    ASSERT_TRUE(m_Samples.m_Valid);
}

enum ProcessExitPhase
{
    PROCESS_EXIT_INITIAL,
    PROCESS_EXIT_READY,
    PROCESS_EXIT_RELEASED,
    PROCESS_EXIT_CALLBACK,
    PROCESS_EXIT_COMPLETE
};

struct ProcessExitContext
{
    dmMutex::HMutex                         m_Mutex;
    dmConditionVariable::HConditionVariable m_Changed;
    dmThread::Thread                        m_Thread;
    dmThread::TlsKey                        m_Key;
    void*                                   m_ProfilerThreadId;
    ProcessExitPhase                        m_Phase;
};

static ProcessExitContext g_ProcessExitContext = {};

static void DM_THREAD_TLS_CALLBACK ProcessExitCallback(void* data)
{
    ProcessExitContext* context = (ProcessExitContext*)data;
    // Hold the same lock as OnThreadExit while process-static destruction runs.
    DM_MUTEX_SCOPED_LOCK(g_Lock);
    DM_MUTEX_SCOPED_LOCK(context->m_Mutex);
    context->m_Phase = PROCESS_EXIT_CALLBACK;
    dmConditionVariable::Signal(context->m_Changed);
    while (context->m_Phase != PROCESS_EXIT_COMPLETE)
    {
        dmConditionVariable::Wait(context->m_Changed, context->m_Mutex);
    }
    OnThreadExit(context->m_ProfilerThreadId);
}

static void ProcessExitWorker(void* arg)
{
    ProcessExitContext* context = (ProcessExitContext*)arg;
    ProfileResult result = PROFILE_RESULT_NOT_INITIALIZED;
    ProfileThread(&result);
    assert(result == PROFILE_RESULT_OK);
    context->m_ProfilerThreadId = dmThread::GetTlsValue(g_TlsKey);
    dmThread::SetTlsValue(g_TlsKey, 0);
    dmThread::SetTlsValue(context->m_Key, context);

    DM_MUTEX_SCOPED_LOCK(context->m_Mutex);
    context->m_Phase = PROCESS_EXIT_READY;
    dmConditionVariable::Signal(context->m_Changed);
    while (context->m_Phase != PROCESS_EXIT_RELEASED)
    {
        dmConditionVariable::Wait(context->m_Changed, context->m_Mutex);
    }
}

// Verify that process-static destruction preserves the cleanup lock while a TLS
// callback is active. Joining happens in the earlier-declared static destructor.
static void StartProcessExitRegression()
{
    ProfileInitialize();
    ProfileRegisterProfiler(g_ProfilerName, &g_Listener);
    ProcessExitContext* context = &g_ProcessExitContext;
    context->m_Mutex = dmMutex::New();
    context->m_Changed = dmConditionVariable::New();
    context->m_Key = dmThread::AllocTls(ProcessExitCallback);
    context->m_Thread = dmThread::New(ProcessExitWorker, 0x80000, context, "process_exit_profiler");
    {
        DM_MUTEX_SCOPED_LOCK(context->m_Mutex);
        while (context->m_Phase != PROCESS_EXIT_READY)
        {
            dmConditionVariable::Wait(context->m_Changed, context->m_Mutex);
        }
    }

    ProfileFinalize();
    ProfileUnregisterProfiler(g_ProfilerName);
    DM_MUTEX_SCOPED_LOCK(context->m_Mutex);
    context->m_Phase = PROCESS_EXIT_RELEASED;
    dmConditionVariable::Signal(context->m_Changed);
    while (context->m_Phase != PROCESS_EXIT_CALLBACK)
    {
        dmConditionVariable::Wait(context->m_Changed, context->m_Mutex);
    }
}

static void FinishProcessExitRegression()
{
    ProcessExitContext* context = &g_ProcessExitContext;
    if (!context->m_Mutex)
    {
        return;
    }
    {
        DM_MUTEX_SCOPED_LOCK(context->m_Mutex);
        context->m_Phase = PROCESS_EXIT_COMPLETE;
        dmConditionVariable::Signal(context->m_Changed);
    }
    dmThread::Join(context->m_Thread);
    dmThread::FreeTls(context->m_Key);
    dmConditionVariable::Delete(context->m_Changed);
    dmMutex::Delete(context->m_Mutex);
}

int main(int argc, char** argv)
{
    jc_test_init(&argc, argv);
    int result = jc_test_run_all();
    if (result == 0)
    {
        StartProcessExitRegression();
    }
    return result;
}
