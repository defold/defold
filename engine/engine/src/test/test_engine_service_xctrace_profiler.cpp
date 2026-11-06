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

#include <dlib/atomic.h>
#include <dlib/mutex.h>
#include <dlib/thread.h>
#include <os/signpost.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct NativeEvent
{
    os_signpost_id_t m_Id;
    uintptr_t        m_Thread;
    uint32_t         m_Type;
    const char*      m_Name;
    char             m_Text[256];
};

static NativeEvent      g_Events[2048];
static uint32_t         g_EventCount;
static int32_atomic_t   g_NextId;
static dmMutex::HMutex  g_EventMutex;
static bool             g_Recording;

static os_signpost_id_t TestSignpostId(os_log_t)
{
    return g_Recording ? (os_signpost_id_t)dmAtomicIncrement32(&g_NextId) + 1 : OS_SIGNPOST_ID_NULL;
}

static bool TestSignpostEnabled(os_log_t)
{
    return g_Recording;
}

static void TestSignpost(uint32_t type, os_signpost_id_t id, const char* name, const char* format, ...)
{
    if (!g_Recording || id == OS_SIGNPOST_ID_NULL)
        return;
    DM_MUTEX_SCOPED_LOCK(g_EventMutex);
    assert(g_EventCount < sizeof(g_Events) / sizeof(g_Events[0]));
    NativeEvent* event = &g_Events[g_EventCount++];
    event->m_Id = id;
    event->m_Thread = dmThread::GetCurrentThread();
    event->m_Type = type;
    event->m_Name = name;

    // Preserve the payload while removing os_log's printf privacy annotations.
    char     printf_format[256];
    uint32_t length = 0;
    for (const char* cursor = format; *cursor; ++cursor)
    {
        if (strncmp(cursor, "{public}", 8) == 0)
            cursor += 7;
        else
            printf_format[length++] = *cursor;
    }
    printf_format[length] = 0;
    va_list args;
    va_start(args, format);
    vsnprintf(event->m_Text, sizeof(event->m_Text), printf_format, args);
    va_end(args);
}

// Exercise the real listener through Profile* APIs, replacing only the OS sink.
// This lets tests toggle recording without an Instruments installation or sleeps.
#define os_signpost_id_generate TestSignpostId
#define os_signpost_enabled TestSignpostEnabled
#undef os_signpost_interval_begin
#undef os_signpost_interval_end
#undef os_signpost_event_emit
#define os_signpost_interval_begin(log, id, name, ...) ((void)(log), TestSignpost(1, id, name, "" __VA_ARGS__))
#define os_signpost_interval_end(log, id, name, ...) ((void)(log), TestSignpost(2, id, name, "" __VA_ARGS__))
#define os_signpost_event_emit(log, id, name, ...) ((void)(log), TestSignpost(3, id, name, "" __VA_ARGS__))
#include "../engine_service_xctrace_profiler_darwin.cpp"

DM_PROPERTY_GROUP(XcTraceProfilerTest, "", 0);
DM_PROPERTY_BOOL(NativeBoolCounter, false, 0, "", XcTraceProfilerTest);
DM_PROPERTY_S32(NativeIntCounter, -1, 0, "", XcTraceProfilerTest);
DM_PROPERTY_F32(NativeFloatCounter, 0.5f, 0, "", XcTraceProfilerTest);
DM_PROPERTY_U32(NativeFrameCounter, 7, PROFILE_PROPERTY_FRAME_RESET, "", XcTraceProfilerTest);
DM_PROPERTY_S64(NativeSignedCounter, 0, 0, "", XcTraceProfilerTest);
DM_PROPERTY_U64(NativeUnsignedCounter, 0, 0, "", XcTraceProfilerTest);
DM_PROPERTY_F64(NativeRealCounter, 0, 0, "", XcTraceProfilerTest);

class NativeProfilerTest : public jc_test_base_class
{
    public:
    char* m_PreviousEnable;

    void  SetUp()
    {
        const char* enable = getenv("DM_PROFILE_NATIVE_ENABLE");
        m_PreviousEnable = enable ? strdup(enable) : 0;
        ASSERT_EQ(0, setenv("DM_PROFILE_NATIVE_ENABLE", "1", 1));
        g_EventCount = 0;
        g_NextId = 0;
        g_Recording = true;
        g_EventMutex = dmMutex::New();
        ProfileInitialize();
        dmEngineService::InitializeXcTraceProfiler();
    }

    void TearDown()
    {
        ProfileFinalize();
        dmMutex::Delete(g_EventMutex);
        if (m_PreviousEnable)
        {
            ASSERT_EQ(0, setenv("DM_PROFILE_NATIVE_ENABLE", m_PreviousEnable, 1));
            free(m_PreviousEnable);
        }
        else
        {
            ASSERT_EQ(0, unsetenv("DM_PROFILE_NATIVE_ENABLE"));
        }
    }

    const NativeEvent* FindEvent(const char* name, const char* text, uint32_t occurrence = 0)
    {
        for (uint32_t i = 0; i < g_EventCount; ++i)
        {
            if (strcmp(g_Events[i].m_Name, name) == 0 && strcmp(g_Events[i].m_Text, text) == 0 && occurrence-- == 0)
                return &g_Events[i];
        }
        return 0;
    }
};

// Registration after ProfileInitialize receives early scopes and the existing property table.
TEST_F(NativeProfilerTest, Startup)
{
    ProfileScopeBegin("startup.before_config", 0);
    ProfileScopeEnd("startup.before_config", 0);
    ASSERT_EQ(2u, g_EventCount);
    ASSERT_STREQ("startup.before_config", g_Events[0].m_Text);
    ASSERT_EQ(1u, g_Events[0].m_Type);
    ASSERT_EQ(2u, g_Events[1].m_Type);
    ASSERT_EQ(g_Events[0].m_Id, g_Events[1].m_Id);
    ProfileFrameEnd(ProfileFrameBegin());
    ASSERT_NE((const NativeEvent*)0, FindEvent("Counter", "NativeFrameCounter = 7"));
}

// With no environment override, startup capture follows the build feature.
TEST_F(NativeProfilerTest, BuildDefault)
{
    ProfileFinalize();
    ASSERT_EQ(0, unsetenv("DM_PROFILE_NATIVE_ENABLE"));
    ProfileInitialize();
    dmEngineService::InitializeXcTraceProfiler();
    g_EventCount = 0;
    ProfileScopeBegin("startup.build_default", 0);
    ProfileScopeEnd("startup.build_default", 0);
#if defined(DM_PROFILE_NATIVE_ENABLED)
    ASSERT_EQ(2u, g_EventCount);
#else
    ASSERT_EQ(0u, g_EventCount);
#endif
}

// Environment overrides apply to each session, including enabling after a disabled session.
TEST_F(NativeProfilerTest, EnvironmentOverride)
{
    const char* values[] = { "0", "1", "", "true", "1" };
    uint32_t    expected[] = { 0, 2, 0, 0, 2 };
    for (uint32_t i = 0; i < sizeof(values) / sizeof(values[0]); ++i)
    {
        ProfileFinalize();
        ASSERT_EQ(0, setenv("DM_PROFILE_NATIVE_ENABLE", values[i], 1));
        ProfileInitialize();
        dmEngineService::InitializeXcTraceProfiler();
        g_EventCount = 0;
        ProfileScopeBegin("startup.environment", 0);
        ProfileScopeEnd("startup.environment", 0);
        ASSERT_EQ(expected[i], g_EventCount);
        if (!expected[i])
        {
            ProfileRegisterPropertyU32("NativeDisabledCounter", "", 0, 0, 0);
            ProfileFrameEnd(ProfileFrameBegin());
            ASSERT_EQ(0u, g_EventCount);
        }
    }
}

// Environment changes cannot interrupt scopes or enable tracing halfway through a session.
TEST_F(NativeProfilerTest, EnablementIsFixedForSession)
{
    ProfileScopeBegin("enabled session", 0);
    ASSERT_EQ(0, setenv("DM_PROFILE_NATIVE_ENABLE", "0", 1));
    ProfileScopeEnd("enabled session", 0);
    ASSERT_EQ(2u, g_EventCount);
    ASSERT_EQ(g_Events[0].m_Id, g_Events[1].m_Id);

    ProfileFinalize();
    ProfileInitialize();
    dmEngineService::InitializeXcTraceProfiler();
    g_EventCount = 0;
    ASSERT_EQ(0, setenv("DM_PROFILE_NATIVE_ENABLE", "1", 1));
    ProfileScopeBegin("disabled session", 0);
    ProfileScopeEnd("disabled session", 0);
    ProfileFrameEnd(ProfileFrameBegin());
    ASSERT_EQ(0u, g_EventCount);
}

// An HTTP capture can activate a launch-disabled XcTrace profiler, replay properties and balance new scopes.
TEST_F(NativeProfilerTest, RuntimeActivation)
{
    ProfileFinalize();
    setenv("DM_PROFILE_NATIVE_ENABLE", "0", 1);
    ProfileInitialize();
    dmEngineService::InitializeXcTraceProfiler();
    g_EventCount = 0;
    ProfileIdx runtime = ProfileRegisterPropertyU32("NativeBeforeActivation", "", 19, 0, 0);
    ProfileScopeBegin("before-activation", 0);
    ASSERT_TRUE(dmEngineService::EnableXcTraceProfiler());
    HProfile frame = ProfileFrameBegin();
    ProfileScopeBegin("after-activation", 0);
    ProfileScopeEnd("after-activation", 0);
    ProfileScopeEnd("before-activation", 0);
    ProfileFrameEnd(frame);
    ASSERT_NE((const NativeEvent*)0, FindEvent("Scope", "after-activation"));
    ASSERT_EQ((const NativeEvent*)0, FindEvent("Scope", "before-activation"));
    ASSERT_NE((const NativeEvent*)0, FindEvent("Counter", "NativeFrameCounter = 7"));
    ASSERT_NE((const NativeEvent*)0, FindEvent("Counter", "NativeBeforeActivation = 19"));
    ProfilePropertySetU32(runtime, 23);
    ProfileFrameEnd(ProfileFrameBegin());
    ASSERT_NE((const NativeEvent*)0, FindEvent("Counter", "NativeBeforeActivation = 23"));
    ASSERT_TRUE(dmEngineService::EnableXcTraceProfiler());
}

// Startup resource text must reach the native sink before any frame completes.
TEST_F(NativeProfilerTest, StartupResourceText)
{
    const char* record = "INFO:RESOURCE: /some/resource.texturec took 12.300ms (pre: 4.500ms, read: 1.200ms, create: 3.000ms, post: 0.100ms [2 calls], queue: 0.500ms, wait: 3.000ms, result: OK)";
    DM_PROFILE_TEXT("%s", record);
    ASSERT_NE((void*)0, FindEvent("Log", record));
    ProfileFrameEnd(ProfileFrameBegin());
    ASSERT_EQ((void*)0, FindEvent("Log", record, 1));
}

// Recursive and deeply nested scopes retain unique IDs and reverse closing order.
TEST_F(NativeProfilerTest, RecursiveScopes)
{
    for (uint32_t i = 0; i < 96; ++i)
        ProfileScopeBegin("/same/resource.texturec", 0);
    for (uint32_t i = 0; i < 96; ++i)
        ProfileScopeEnd("/same/resource.texturec", 0);
    ASSERT_EQ(192u, g_EventCount);
    for (uint32_t i = 0; i < 96; ++i)
    {
        ASSERT_EQ(g_Events[i].m_Id, g_Events[191 - i].m_Id);
        ASSERT_NE(OS_SIGNPOST_ID_NULL, g_Events[i].m_Id);
        for (uint32_t j = 0; j < i; ++j)
            ASSERT_NE(g_Events[i].m_Id, g_Events[j].m_Id);
    }
}

// Attaching a recorder inside a disabled scope must not emit that scope's unmatched end.
TEST_F(NativeProfilerTest, RecordingStartsInsideScope)
{
    g_Recording = false;
    ProfileScopeBegin("not recorded", 0);
    g_Recording = true;
    ProfileScopeBegin("recorded", 0);
    ProfileScopeEnd("recorded", 0);
    ProfileScopeEnd("not recorded", 0);
    ASSERT_EQ(2u, g_EventCount);
    ASSERT_STREQ("recorded", g_Events[0].m_Text);
    ASSERT_EQ(g_Events[0].m_Id, g_Events[1].m_Id);
}

// Resource names and scope IDs survive frame boundaries; frames have their own intervals.
TEST_F(NativeProfilerTest, ScopeAcrossFrames)
{
    HProfile frame = ProfileFrameBegin();
    ProfileScopeBegin("/some/resource.collectionc", 0);
    ProfileFrameEnd(frame);
    frame = ProfileFrameBegin();
    ProfileScopeEnd("/some/resource.collectionc", 0);
    ProfileFrameEnd(frame);
    const NativeEvent* scope = FindEvent("Scope", "/some/resource.collectionc");
    ASSERT_NE((void*)0, scope);
    bool     closed = false;
    uint32_t frames = 0;
    for (uint32_t i = 0; i < g_EventCount; ++i)
    {
        closed |= g_Events[i].m_Type == 2 && g_Events[i].m_Id == scope->m_Id;
        frames += g_Events[i].m_Type == 1 && strcmp(g_Events[i].m_Name, "Frame") == 0;
    }
    ASSERT_TRUE(closed);
    ASSERT_EQ(2u, frames);
}

static void NativeWorker(void*)
{
    for (uint32_t i = 0; i < 16; ++i)
    {
        ProfileScopeBegin("worker", 0);
        ProfileScopeBegin("worker", 0);
        ProfileScopeEnd("worker", 0);
        ProfileScopeEnd("worker", 0);
    }
}

// Identically named scopes on different threads cannot close each other's intervals.
TEST_F(NativeProfilerTest, WorkerThreads)
{
    dmThread::Thread threads[2];
    threads[0] = dmThread::New(NativeWorker, 0x80000, 0, "Native A");
    threads[1] = dmThread::New(NativeWorker, 0x80000, 0, "Native B");
    dmThread::Join(threads[0]);
    dmThread::Join(threads[1]);
    ASSERT_NE((void*)0, FindEvent("Thread Name", "Native A"));
    ASSERT_NE((void*)0, FindEvent("Thread Name", "Native B"));
    uint32_t begins = 0;
    for (uint32_t i = 0; i < g_EventCount; ++i)
    {
        if (g_Events[i].m_Type != 1)
            continue;
        ++begins;
        uint32_t ends = 0;
        for (uint32_t j = i + 1; j < g_EventCount; ++j)
        {
            if (g_Events[j].m_Type == 2 && g_Events[j].m_Id == g_Events[i].m_Id)
            {
                ++ends;
                ASSERT_EQ(g_Events[i].m_Thread, g_Events[j].m_Thread);
            }
        }
        ASSERT_EQ(1u, ends);
    }
    ASSERT_EQ(64u, begins);
}

// Counter snapshots preserve 64-bit values and reset frame counters even without recording.
TEST_F(NativeProfilerTest, CountersAndLogs)
{
    g_Recording = false;
    DM_PROPERTY_ADD_U32(NativeFrameCounter, 100);
    ProfileFrameEnd(ProfileFrameBegin());
    g_Recording = true;
    DM_PROPERTY_SET_S64(NativeSignedCounter, INT64_MIN);
    DM_PROPERTY_SET_U64(NativeUnsignedCounter, UINT64_MAX);
    DM_PROPERTY_SET_F64(NativeRealCounter, 1.25);
    ProfileLogText("resource %s", "/example.texturec");
    ProfileFrameEnd(ProfileFrameBegin());
    ASSERT_NE((void*)0, FindEvent("Counter", "NativeFrameCounter = 7"));
    ASSERT_NE((void*)0, FindEvent("Counter", "NativeSignedCounter = -9223372036854775808"));
    ASSERT_NE((void*)0, FindEvent("Counter", "NativeUnsignedCounter = 18446744073709551615"));
    ASSERT_NE((void*)0, FindEvent("Counter", "NativeRealCounter = 1.250000"));
    ASSERT_NE((void*)0, FindEvent("Log", "resource /example.texturec"));
}

// Runtime properties participate in snapshots, and group reset restores all descendant defaults.
TEST_F(NativeProfilerTest, RuntimePropertiesAndGroupReset)
{
    ProfileIdx runtime = ProfileRegisterPropertyU32("NativeRuntimeCounter", "", 23, 0, XcTraceProfilerTest);
    DM_PROPERTY_SET_BOOL(NativeBoolCounter, true);
    DM_PROPERTY_ADD_S32(NativeIntCounter, -41);
    DM_PROPERTY_ADD_F32(NativeFloatCounter, 0.75f);
    ProfilePropertyAddU32(runtime, 2);
    ProfileFrameEnd(ProfileFrameBegin());
    ASSERT_NE((void*)0, FindEvent("Counter", "NativeBoolCounter = 1"));
    ASSERT_NE((void*)0, FindEvent("Counter", "NativeIntCounter = -42"));
    ASSERT_NE((void*)0, FindEvent("Counter", "NativeFloatCounter = 1.250000"));
    ASSERT_NE((void*)0, FindEvent("Counter", "NativeRuntimeCounter = 25"));

    g_EventCount = 0;
    dmEngineService::g_NativeListener.m_PropertyReset(dmEngineService::g_NativeListener.m_Ctx, *XcTraceProfilerTest());
    ProfileFrameEnd(ProfileFrameBegin());
    ASSERT_NE((void*)0, FindEvent("Counter", "NativeBoolCounter = 0"));
    ASSERT_NE((void*)0, FindEvent("Counter", "NativeIntCounter = -1"));
    ASSERT_NE((void*)0, FindEvent("Counter", "NativeFloatCounter = 0.500000"));
    ASSERT_NE((void*)0, FindEvent("Counter", "NativeRuntimeCounter = 23"));
}

// Repeated initialization does not duplicate listeners, and finalization clears per-thread state.
TEST_F(NativeProfilerTest, Reinitialize)
{
    dmEngineService::InitializeXcTraceProfiler();
    ProfileScopeBegin("old session", 0);
    ProfileFinalize();
    ProfileFrameBegin(); // The native listener has been removed.
    ProfileInitialize();
    dmEngineService::InitializeXcTraceProfiler();
    g_EventCount = 0;
    ProfileScopeEnd("old session", 0);
    ProfileScopeBegin("new session", 0);
    ProfileScopeEnd("new session", 0);
    ASSERT_EQ(2u, g_EventCount);
    ASSERT_STREQ("new session", g_Events[0].m_Text);
    ASSERT_EQ(g_Events[0].m_Id, g_Events[1].m_Id);
}

int main(int argc, char** argv)
{
    jc_test_init(&argc, argv);
    return jc_test_run_all();
}
