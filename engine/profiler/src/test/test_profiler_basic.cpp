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

static void ProfileScopes(void* arg)
{
    ProfileResult* result = (ProfileResult*)arg;
    uint64_t name_hash = 0;
    *result = ProfileScopeBegin("thread_scope", &name_hash);
    if (*result == PROFILE_RESULT_OK)
    {
        *result = ProfileScopeEnd("thread_scope", name_hash);
    }
}

static void ProfileThread(void* arg)
{
    ProfileResult* result = (ProfileResult*)arg;
    *result = ProfileThreadEntered("ShortLived");
    if (*result == PROFILE_RESULT_OK)
    {
        ProfileScopes(arg);
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

// Verifies automatic thread names and completed sample delivery before each
// worker's pool and name are reclaimed, guarding against the growth in #13704.
TEST_F(ProfilerBasicTest, ReclaimsExitedThreads)
{
    uint64_t main_name_hash = 0;
    ASSERT_EQ(PROFILE_RESULT_OK, ProfileScopeBegin("main_scope", &main_name_hash));
    uint32_t thread_count = g_ProfileContext->m_ThreadData.Size();
    uint32_t name_count = g_ProfileContext->m_ThreadNames.Size();
    for (uint32_t i = 0; i < 32; ++i)
    {
        ProfileResult result = PROFILE_RESULT_NOT_INITIALIZED;
        dmThread::Thread thread = dmThread::New(ProfileScopes, 0x80000, &result, "ShortLived");
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
    ProfileThreadExited();
    return 0;
}
#else
static void* NativeProfileThread(void* arg)
{
    ProfileThread(arg);
    ProfileThreadExited();
    return 0;
}
#endif

// Verifies explicit cleanup for threads created outside dmThread, guarding
// against native extension threads retaining profiler pools and names.
TEST_F(ProfilerBasicTest, ExternalThreadsUseExplicitLifecycle)
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

// Verifies that the proxy's exit notification after profiler shutdown safely
// skips already-released data, guarding against access to the destroyed context.
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

// Verifies lazy entry, repeated exit and re-entry on an existing thread,
// guarding against eager sample pools, stale tokens and double cleanup.
TEST_F(ProfilerBasicTest, ExplicitThreadLifecycle)
{
    dmProfiler::SetSampleTreeCallback(0, 0);
    ASSERT_EQ(PROFILE_RESULT_OK, ProfileThreadEntered("ExplicitThread"));
    int32_t thread_id = GetThreadId();
    ASSERT_STREQ("ExplicitThread", GetThreadNameNoLock(thread_id));
    ASSERT_EQ(0u, g_ProfileContext->m_ThreadData.Size());

    uint64_t name_hash = 0;
    ASSERT_EQ(PROFILE_RESULT_OK, ProfileScopeBegin("explicit_scope", &name_hash));
    ASSERT_EQ(PROFILE_RESULT_OK, ProfileScopeEnd("explicit_scope", name_hash));
    ASSERT_EQ(1u, g_ProfileContext->m_ThreadData.Size());
    ASSERT_EQ(PROFILE_RESULT_OK, ProfileThreadExited());
    ASSERT_EQ((void*)0, dmThread::GetTlsValue(g_TlsKey));
    ASSERT_EQ(0u, g_ProfileContext->m_ThreadData.Size());
    ASSERT_EQ(0u, g_ProfileContext->m_ThreadNames.Size());
    ASSERT_EQ(PROFILE_RESULT_OK, ProfileThreadExited());

    ASSERT_EQ(PROFILE_RESULT_OK, ProfileThreadEntered("ReenteredThread"));
    ASSERT_STREQ("ReenteredThread", GetThreadNameNoLock(GetThreadId()));
    ASSERT_EQ(0u, g_ProfileContext->m_ThreadData.Size());
    ASSERT_EQ(PROFILE_RESULT_OK, ProfileScopeBegin("explicit_scope", &name_hash));
    ASSERT_EQ(PROFILE_RESULT_OK, ProfileScopeEnd("explicit_scope", name_hash));
    ASSERT_EQ(PROFILE_RESULT_OK, ProfileThreadExited());
    ASSERT_EQ(0u, g_ProfileContext->m_ThreadData.Size());
    ASSERT_EQ(0u, g_ProfileContext->m_ThreadNames.Size());
}

int main(int argc, char** argv)
{
    jc_test_init(&argc, argv);
    return jc_test_run_all();
}
