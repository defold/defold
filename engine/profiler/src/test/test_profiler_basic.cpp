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

class ProfilerBasicMemoryTest : public ProfilerBasicTest
{
    public:
    void SetUp()
    {
        ProfilerBasicTest::SetUp();
        m_PreviousLimit = g_MaxSampleCount;
        g_MaxSampleCount = 4096;
        dmProfiler::SetSampleTreeCallback(0, 0);
    }

    void TearDown()
    {
        g_MaxSampleCount = m_PreviousLimit;
        ProfilerBasicTest::TearDown();
    }

    ThreadData* GetThreadData()
    {
        return GetOrCreateThreadData(g_ProfileContext, GetThreadId());
    }

    uint32_t m_PreviousLimit;
};

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

// Verifies that small trees allocate and reuse one sample page, guarding against
// reserving the full configured sample limit for every lightly used thread.
TEST_F(ProfilerBasicMemoryTest, SmallTreesReuseOnePage)
{
    ThreadData* td = GetThreadData();
    ASSERT_EQ(0u, td->m_SamplePages.Size());
    uint64_t name_hash = 0;
    ASSERT_EQ(PROFILE_RESULT_OK, ProfileScopeBegin("small_tree", &name_hash));
    ASSERT_EQ(1u, td->m_SamplePages.Size());
    ASSERT_EQ(1u, td->m_SampleCount);
    Sample* page = td->m_SamplePages[0];
    ASSERT_EQ(PROFILE_RESULT_OK, ProfileScopeEnd("small_tree", name_hash));
    ASSERT_EQ(0u, td->m_SampleCount);

    ASSERT_EQ(PROFILE_RESULT_OK, ProfileScopeBegin("small_tree", &name_hash));
    ASSERT_EQ(1u, td->m_SamplePages.Size());
    ASSERT_EQ(page, td->m_SamplePages[0]);
    ASSERT_EQ(1u, td->m_CurrentSample->m_CallCount);
    ASSERT_EQ(PROFILE_RESULT_OK, ProfileScopeEnd("small_tree", name_hash));
}

// Verifies pointer stability, sibling order and repeated-scope aggregation across
// page-directory growth, guarding against moved samples or broken child links.
TEST_F(ProfilerBasicMemoryTest, WideTreePreservesOrderAndAggregation)
{
    uint64_t root_hash = 0;
    ASSERT_EQ(PROFILE_RESULT_OK, ProfileScopeBegin("wide_tree", &root_hash));
    ThreadData* td = GetThreadData();
    Sample* root = td->m_CurrentSample;
    Sample* first_child = 0;
    const uint32_t child_count = g_SamplePageSize * 4 + 17;
    for (uint32_t i = 0; i < child_count; ++i)
    {
        char name[32];
        snprintf(name, sizeof(name), "child_%u", i);
        uint64_t name_hash = 0;
        for (uint32_t call = 0; call < 2; ++call)
        {
            ASSERT_EQ(PROFILE_RESULT_OK, ProfileScopeBegin(name, &name_hash));
            if (i == 0)
            {
                first_child = td->m_CurrentSample;
            }
            ASSERT_EQ(PROFILE_RESULT_OK, ProfileScopeEnd(name, name_hash));
        }
    }
    ASSERT_EQ(root, td->m_CurrentSample);
    ASSERT_EQ(first_child, root->m_FirstChild);
    ASSERT_EQ(child_count + 1, td->m_SampleCount);
    ASSERT_EQ(5u, td->m_SamplePages.Size());

    dmProfiler::SampleIterator iter;
    dmProfiler::SampleIterateChildren(root, &iter);
    for (uint32_t i = 0; i < child_count; ++i)
    {
        ASSERT_TRUE(dmProfiler::SampleIterateNext(&iter));
        char name[32];
        snprintf(name, sizeof(name), "child_%u", i);
        ASSERT_STREQ(name, dmProfiler::SampleGetName(iter.m_Sample));
        ASSERT_EQ(2u, dmProfiler::SampleGetCallCount(iter.m_Sample));
    }
    ASSERT_FALSE(dmProfiler::SampleIterateNext(&iter));
    ASSERT_EQ(PROFILE_RESULT_OK, ProfileScopeEnd("wide_tree", root_hash));
    ASSERT_EQ(0u, td->m_SampleCount);
}

// Verifies that parent links remain valid across page growth and all nested
// scopes close normally, guarding against corruption of the active scope stack.
TEST_F(ProfilerBasicMemoryTest, NestedScopesCrossPages)
{
    const uint32_t depth = g_SamplePageSize * 2 + 17;
    uint64_t name_hash = 0;
    for (uint32_t i = 0; i < depth; ++i)
    {
        ASSERT_EQ(PROFILE_RESULT_OK, ProfileScopeBegin("nested", &name_hash));
    }
    ThreadData* td = GetThreadData();
    ASSERT_EQ(3u, td->m_SamplePages.Size());
    Sample* sample = td->m_CurrentSample;
    for (uint32_t i = 0; i < depth; ++i)
    {
        ASSERT_NE(&td->m_Root, sample);
        sample = sample->m_Parent;
    }
    ASSERT_EQ(&td->m_Root, sample);
    for (uint32_t i = 0; i < depth; ++i)
    {
        ASSERT_EQ(PROFILE_RESULT_OK, ProfileScopeEnd("nested", name_hash));
    }
    ASSERT_EQ(&td->m_Root, td->m_CurrentSample);
    ASSERT_EQ(0u, td->m_SampleCount);
    ASSERT_EQ(0u, td->m_OverflowDepth);
}

// Verifies a partial final page and balanced overflow while retaining the
// thread's original limit when the default changes during pool growth.
TEST_F(ProfilerBasicMemoryTest, PreservesLimitAndOverflowAcrossPages)
{
    const uint32_t limit = g_SamplePageSize + 3;
    g_MaxSampleCount = limit;
    uint64_t name_hash = 0;
    ASSERT_EQ(PROFILE_RESULT_OK, ProfileScopeBegin("limited", &name_hash));
    ThreadData* td = GetThreadData();
    g_MaxSampleCount = 1;
    for (uint32_t i = 1; i < limit; ++i)
    {
        ASSERT_EQ(PROFILE_RESULT_OK, ProfileScopeBegin("limited", &name_hash));
    }
    ASSERT_EQ(limit, td->m_MaxSampleCount);
    ASSERT_EQ(limit, td->m_SampleCount);
    ASSERT_EQ(2u, td->m_SamplePages.Size());
    Sample* current = td->m_CurrentSample;
    ASSERT_EQ(PROFILE_RESULT_OUT_OF_SAMPLES, ProfileScopeBegin("limited", &name_hash));
    ASSERT_EQ(PROFILE_RESULT_OUT_OF_SAMPLES, ProfileScopeBegin("limited", &name_hash));
    ASSERT_EQ(PROFILE_RESULT_OK, ProfileScopeEnd("limited", name_hash));
    ASSERT_EQ(PROFILE_RESULT_OK, ProfileScopeEnd("limited", name_hash));
    ASSERT_EQ(current, td->m_CurrentSample);
    ASSERT_EQ(0u, td->m_OverflowDepth);
    for (uint32_t i = 0; i < limit; ++i)
    {
        ASSERT_EQ(PROFILE_RESULT_OK, ProfileScopeEnd("limited", name_hash));
    }
    ASSERT_EQ(0u, td->m_SampleCount);
    ASSERT_EQ(PROFILE_RESULT_OK, ProfileScopeBegin("limited", &name_hash));
    ASSERT_EQ(PROFILE_RESULT_OK, ProfileScopeEnd("limited", name_hash));
    ASSERT_EQ(2u, td->m_SamplePages.Size());
}

// Verifies the existing zero-limit behavior without allocating sample pages,
// guarding against rounding the configured maximum up to a whole page.
TEST_F(ProfilerBasicMemoryTest, ZeroLimitDoesNotAllocate)
{
    g_MaxSampleCount = 0;
    uint64_t name_hash = 0;
    ASSERT_EQ(PROFILE_RESULT_OUT_OF_SAMPLES, ProfileScopeBegin("disabled", &name_hash));
    ThreadData* td = GetThreadData();
    ASSERT_EQ(0u, td->m_SamplePages.Size());
    ASSERT_EQ(PROFILE_RESULT_OK, ProfileScopeEnd("disabled", name_hash));
    ASSERT_EQ(0u, td->m_OverflowDepth);
}

int main(int argc, char** argv)
{
    jc_test_init(&argc, argv);
    return jc_test_run_all();
}
