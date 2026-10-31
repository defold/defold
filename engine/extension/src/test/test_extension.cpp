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

#include <stdlib.h>
#define JC_TEST_IMPLEMENTATION
#include <jc_test/jc_test.h>
#include "../extension.h"
#include "../extension.hpp"
#include <dmsdk/engine/engine_event.h>
#include "test_extension.h"
#include <dlib/array.h>
#include <dlib/context_registry.h>
#include <dlib/hashtable.h>
#include <dlib/log.h>
#include <dlib/static_assert.h>

extern "C"
{
#include "lua/lua.h"
#include "lua/lauxlib.h"
}

// Extension in a separate library. See comment in test_extension_lib.cpp

extern int g_TestAppInitCount;
extern int g_TestInitCount;
extern int g_TestUpdateCount;
extern float g_TestUpdateDT;
extern int g_TestAppEventCount;
extern int g_TestEventCount;
extern int g_TestNewEventCount;
extern int g_TestContextCount;
extern int g_TestRegisterCount;
extern int g_TestUnregisterCount;

extern "C" void dmExportedSymbols();
extern "C" void TestTypeCpp();
extern "C"
{
    void TestTypeCBroadcastBuiltinEvent(ExtensionParams* params, uint32_t index, HEngine engine, HWindow window);
    extern int g_TypeCRegisterCount;
    extern int g_TypeCUnregisterCount;
    extern int g_TypeCUpdateCount;
    extern float g_TypeCUpdateDT;
    extern int g_TypeCEventCount;
    extern int g_TypeCWindowCount;
    extern int g_TypeCDeleteCount;
    extern uint32_t g_TypeCWindowWidth;
    extern uint32_t g_TypeCWindowHeight;
    extern int g_TypeCUserDataValid;
    extern HEngine g_TypeCEventEngine;
    extern HWindow g_TypeCEventWindow;
}

struct Initializer {
    Initializer() {
        dmExportedSymbols();
    }
} g_SymbolInitializer;

static int g_DeprecationWarnings = 0;
static int g_SecondEventCalls = 0;
static bool g_UserDataValid = true;
static bool g_SkipUserData = false;
static bool g_DispatchDuringUpdate = false;

static void CaptureDeprecation(LogSeverity severity, const char*, const char* text)
{
    if (severity != LOG_SEVERITY_WARNING || !strstr(text, "deprecated FExtensionOnEvent"))
    {
        return;
    }
    ++g_DeprecationWarnings;
}

static dmExtension::Result SecondAppInitialize(dmExtension::AppParams* params)
{
    g_UserDataValid &= dmExtension::AppParamsGetUserData(params) == 0;
    return dmExtension::RESULT_OK;
}

static void SecondOnEvent(dmExtension::Params* params, dmhash_t, const void*, uint32_t)
{
    g_UserDataValid &= dmExtension::ParamsGetUserData(params) == 0;
    ++g_SecondEventCalls;
}

static dmExtension::Result RegisterObserver(dmExtension::HExtension extension)
{
    dmExtension::SetAppInitializeFn(extension, SecondAppInitialize);
    dmExtension::SetEventFn(extension, SecondOnEvent);
    return dmExtension::RESULT_OK;
}

DM_DECLARE_EXTENSION(TestEventObserver, RegisterObserver, 0);

struct TypeTestState
{
    int   m_Mode;
    int   m_Register;
    int   m_Unregister;
    int   m_AppInitialize;
    int   m_AppFinalize;
    int   m_Initialize;
    int   m_Finalize;
    int   m_Update;
    int   m_PreRender;
    int   m_PostRender;
    float m_DT;
    bool  m_SkipUpdate;
    bool  m_Registered;
    bool  m_AppLive;
    bool  m_EngineLive;
    bool  m_OrderValid;
    int*  m_UserData;
};
static TypeTestState g_TypeTest = {};

static void CheckTypeUserData(void* data)
{
    g_UserDataValid &= data == g_TypeTest.m_UserData;
    if (data == g_TypeTest.m_UserData && data)
    {
        g_UserDataValid &= *(int*)data == 17;
    }
}

struct EventTestState
{
    uint32_t                 m_Count;
    uint32_t                 m_WindowCount;
    uint32_t                 m_DeleteCount;
    uint32_t                 m_Width;
    uint32_t                 m_Height;
    uint32_t                 m_CustomValue;
    dmhash_t                 m_LastId;
    const void*              m_LastData;
    uint32_t                 m_LastDataSize;
    bool                     m_DispatchNested;
    dmEngine::HEngine        m_SourceEngine;
    HWindow                  m_SourceWindow;
};
static EventTestState g_EventTest = {};
static bool g_OmitEventCallback = false;
static const uint8_t g_NestedPayload[] = { 1, 2, 3 };

static void TypeEvent(dmExtension::Params* params, dmhash_t event_id, const void* data, uint32_t data_size)
{
    CheckTypeUserData(dmExtension::ParamsGetUserData(params));
    ++g_EventTest.m_Count;
    g_EventTest.m_LastId = event_id;
    g_EventTest.m_LastData = data;
    g_EventTest.m_LastDataSize = data_size;
    if (data && data_size == sizeof(dmEngine::AppEvent) &&
        (event_id == dmEngine::EVENT_APP_ACTIVATE || event_id == dmEngine::EVENT_APP_DEACTIVATE ||
         event_id == dmEngine::EVENT_APP_ICONIFY || event_id == dmEngine::EVENT_APP_DEICONIFY))
    {
        const dmEngine::AppEvent* event = (const dmEngine::AppEvent*)data;
        g_EventTest.m_SourceEngine = event->m_Engine;
        g_EventTest.m_SourceWindow = 0;
    }
    if (data && data_size == sizeof(dmEngine::LifecycleEvent) &&
        (event_id == dmEngine::EVENT_ENGINE_INITIALIZED || event_id == dmEngine::EVENT_ENGINE_DELETE))
    {
        g_EventTest.m_SourceEngine = ((const dmEngine::LifecycleEvent*)data)->m_Engine;
        g_EventTest.m_SourceWindow = 0;
    }
    if (event_id == dmEngine::EVENT_ENGINE_DELETE)
    {
        ++g_EventTest.m_DeleteCount;
    }
    if (event_id == dmEngine::EVENT_WINDOW_RESIZED)
    {
        if (data && data_size == sizeof(dmEngine::WindowResizedEvent))
        {
            const dmEngine::WindowResizedEvent* resized = (const dmEngine::WindowResizedEvent*)data;
            g_EventTest.m_SourceEngine = resized->m_Engine;
            g_EventTest.m_SourceWindow = resized->m_Window;
            ++g_EventTest.m_WindowCount;
            g_EventTest.m_Width = resized->m_Width;
            g_EventTest.m_Height = resized->m_Height;
            if (g_EventTest.m_DispatchNested)
            {
                g_EventTest.m_DispatchNested = false;
                dmExtension::BroadcastEvent(params, dmHashString64("nested_event"), g_NestedPayload, sizeof(g_NestedPayload));
                CheckTypeUserData(dmExtension::ParamsGetUserData(params));
            }
        }
    }
    if (event_id == dmHashString64("test.custom_event") && data && data_size == sizeof(uint32_t))
    {
        memcpy(&g_EventTest.m_CustomValue, data, sizeof(g_EventTest.m_CustomValue));
    }
}

enum TypeTestMode
{
    TYPE_TEST_OK,
    TYPE_TEST_REGISTER_FAIL,
    TYPE_TEST_APP_FAIL,
    TYPE_TEST_INITIALIZE_FAIL,
};

static dmExtension::Result TypeAppInitialize(dmExtension::AppParams* params)
{
    CheckTypeUserData(dmExtension::AppParamsGetUserData(params));
    ++g_TypeTest.m_AppInitialize;
    g_TypeTest.m_OrderValid &= g_TypeTest.m_Registered && g_TypeCRegisterCount > g_TypeCUnregisterCount;
    if (g_TypeTest.m_Mode == TYPE_TEST_APP_FAIL)
    {
        return dmExtension::RESULT_INIT_ERROR;
    }
    g_TypeTest.m_AppLive = true;
    return dmExtension::RESULT_OK;
}

static dmExtension::Result TypeAppFinalize(dmExtension::AppParams* params)
{
    CheckTypeUserData(dmExtension::AppParamsGetUserData(params));
    ++g_TypeTest.m_AppFinalize;
    g_TypeTest.m_OrderValid &= g_TypeTest.m_AppLive && !g_TypeTest.m_EngineLive;
    g_TypeTest.m_AppLive = false;
    return dmExtension::RESULT_OK;
}

static dmExtension::Result TypeInitialize(dmExtension::Params* params)
{
    CheckTypeUserData(dmExtension::ParamsGetUserData(params));
    ++g_TypeTest.m_Initialize;
    g_TypeTest.m_OrderValid &= g_TypeTest.m_AppLive;
    if (g_TypeTest.m_Mode == TYPE_TEST_INITIALIZE_FAIL)
    {
        return dmExtension::RESULT_INIT_ERROR;
    }
    g_TypeTest.m_EngineLive = true;
    return dmExtension::RESULT_OK;
}

static dmExtension::Result TypeFinalize(dmExtension::Params* params)
{
    CheckTypeUserData(dmExtension::ParamsGetUserData(params));
    ++g_TypeTest.m_Finalize;
    g_TypeTest.m_OrderValid &= g_TypeTest.m_EngineLive;
    g_TypeTest.m_EngineLive = false;
    return dmExtension::RESULT_OK;
}

static dmExtension::Result TypeUpdate(dmExtension::Params* params)
{
    CheckTypeUserData(dmExtension::ParamsGetUserData(params));
    if (g_DispatchDuringUpdate)
    {
        dmExtension::BroadcastEvent(params, dmHashString64("update_event"), 0, 0);
        CheckTypeUserData(dmExtension::ParamsGetUserData(params));
    }
    ++g_TypeTest.m_Update;
    g_TypeTest.m_OrderValid &= g_TypeTest.m_EngineLive;
    return dmExtension::ParamsGetFloat(params, dmExtension::PARAM_DT, &g_TypeTest.m_DT) ? dmExtension::RESULT_OK : dmExtension::RESULT_INIT_ERROR;
}

static dmExtension::Result TypePreRender(dmExtension::Params* params)
{
    CheckTypeUserData(dmExtension::ParamsGetUserData(params));
    ++g_TypeTest.m_PreRender;
    g_TypeTest.m_OrderValid &= g_TypeTest.m_AppLive;
    return dmExtension::RESULT_OK;
}

static dmExtension::Result TypePostRender(dmExtension::Params* params)
{
    CheckTypeUserData(dmExtension::ParamsGetUserData(params));
    ++g_TypeTest.m_PostRender;
    g_TypeTest.m_OrderValid &= g_TypeTest.m_AppLive;
    return dmExtension::RESULT_OK;
}

static dmExtension::Result RegisterType(dmExtension::HExtension extension)
{
    ++g_TypeTest.m_Register;
    g_UserDataValid &= dmExtension::GetUserData(extension) == 0;
    if (!g_SkipUserData)
    {
        g_TypeTest.m_UserData = new int(17);
    }
    dmExtension::SetUserData(extension, g_TypeTest.m_UserData);
    CheckTypeUserData(dmExtension::GetUserData(extension));
    if (g_TypeTest.m_Mode == TYPE_TEST_REGISTER_FAIL)
    {
        delete g_TypeTest.m_UserData;
        g_TypeTest.m_UserData = 0;
        return dmExtension::RESULT_INIT_ERROR;
    }
    g_TypeTest.m_Registered = true;
    dmExtension::SetAppInitializeFn(extension, TypeAppInitialize);
    dmExtension::SetAppFinalizeFn(extension, TypeAppFinalize);
    dmExtension::SetInitializeFn(extension, TypeInitialize);
    dmExtension::SetFinalizeFn(extension, TypeFinalize);
    if (!g_TypeTest.m_SkipUpdate)
    {
        dmExtension::SetUpdateFn(extension, TypeUpdate);
    }
    dmExtension::SetPreRenderFn(extension, TypePostRender);
    dmExtension::SetPreRenderFn(extension, 0);
    dmExtension::SetPreRenderFn(extension, TypePreRender);
    dmExtension::SetPostRenderFn(extension, TypePostRender);
    if (!g_OmitEventCallback)
    {
        dmExtension::SetEventFn(extension, TypeEvent);
    }
    return dmExtension::RESULT_OK;
}

static dmExtension::Result UnregisterType(dmExtension::HExtension extension)
{
    CheckTypeUserData(dmExtension::GetUserData(extension));
    delete (int*)dmExtension::GetUserData(extension);
    g_TypeTest.m_UserData = 0;
    ++g_TypeTest.m_Unregister;
    g_TypeTest.m_OrderValid &= g_TypeTest.m_Registered && !g_TypeTest.m_AppLive && !g_TypeTest.m_EngineLive && g_TestAppInitCount == 0;
    g_TypeTest.m_Registered = false;
    return dmExtension::RESULT_OK;
}

DM_DECLARE_EXTENSION(TestTypeCpp, RegisterType, UnregisterType);

static dmExtension::Result TypedInitialize(dmExtension::Params*)
{
    return dmExtension::RESULT_OK;
}

// C++ extension callbacks must be assignable to the C API without a function-pointer cast.
TEST(dmExtension, CallbackTypeCompatibility)
{
    FExtensionInitialize initialize = TypedInitialize;
    ASSERT_EQ(EXTENSION_RESULT_OK, initialize(0));
}

// Float lookup returns the default delta and preserves the output for unsupported parameters.
TEST(dmExtension, FloatParameterLookup)
{
    ExtensionParams params;
    ExtensionParamsInitialize(&params);

    float dt = -1.0f;
    ASSERT_TRUE(ExtensionParamsGetFloat(&params, EXTENSION_PARAM_DT, &dt));
    ASSERT_EQ(0.0f, dt);

    float value = 42.0f;
    ASSERT_FALSE(dmExtension::ParamsGetFloat(&params, (dmExtension::Param)(dmExtension::PARAM_DT + 1), &value));
    ASSERT_EQ(42.0f, value);

    ExtensionParamsFinalize(&params);
}

// Both declaration variants coexist in one library; only the legacy callback emits a deprecation warning.
TEST(dmExtension, Basic)
{
    HContextRegistry context_registry = ContextRegistryCreate();

    dmExtension::AppParams appparams;
    ExtensionAppParamsInitialize(&appparams);
    ExtensionAppParamsSetContextRegistry(&appparams, context_registry);

    int engine_context = 1337;
    ContextRegistrySet(context_registry, "engine", &engine_context);
    ASSERT_EQ((void*)&engine_context, ContextRegistryGetByHash(context_registry, dmHashString64("engine")));

    int engine_hash_context = 7331;
    ContextRegistrySetByHash(context_registry, dmHashString64("engine_hash"), &engine_hash_context);
    ASSERT_EQ((void*)&engine_hash_context, ContextRegistryGet(context_registry, "engine_hash"));

    ASSERT_EQ(0, g_TestAppInitCount);
    ASSERT_EQ(dmExtension::RESULT_OK, dmExtension::AppInitialize(&appparams));
    ASSERT_EQ(1, g_TestAppInitCount);
    ASSERT_EQ(1, g_TestRegisterCount);
    ASSERT_EQ(0, g_TestUnregisterCount);
    dmExtension::HExtensionDesc extension = dmExtension::GetFirstExtension();
    ASSERT_NE((dmExtension::HExtensionDesc)0, extension);
    uint32_t extension_count = 0;
    while (extension && extension_count < 8)
    {
        ++extension_count;
        extension = dmExtension::GetNextExtension(extension);
    }
    ASSERT_EQ(5u, extension_count);
    ASSERT_EQ((dmExtension::HExtensionDesc)0, extension);

    ASSERT_NE((void*)0, ContextRegistryGet(context_registry, "lib"));
    ASSERT_NE((void*)0, ContextRegistryGetByHash(context_registry, dmHashString64("lib")));

    ExtensionAppParamsFinalize(&appparams);

    dmExtension::Params params;
    ExtensionParamsInitialize(&params);
    ExtensionParamsSetContextRegistry(&params, context_registry);

    ASSERT_NE((void*)0, ContextRegistryGet(context_registry, "lib"));
    ASSERT_NE((void*)0, ContextRegistryGetByHash(context_registry, dmHashString64("lib")));

    ASSERT_EQ(dmExtension::RESULT_OK, dmExtension::Initialize(&params));
    ASSERT_EQ(1, g_TestInitCount);
    ASSERT_EQ(1, g_TestContextCount);
    ASSERT_NE((void*)0, ContextRegistryGet(context_registry, "init"));

    const float update_dts[] = { 1.0f / 60.0f, 0.0f, 1.0f / 30.0f, 1.0f / 120.0f, 0.1f };
    for (uint32_t i = 0; i < DM_ARRAY_SIZE(update_dts); ++i)
    {
        ASSERT_EQ(dmExtension::RESULT_OK, dmExtension::Update(&params, update_dts[i]));
        ASSERT_EQ(i+1, g_TestUpdateCount);
        ASSERT_EQ(update_dts[i], g_TestUpdateDT);
        float dt = -1.0f;
        ASSERT_TRUE(ExtensionParamsGetFloat(&params, EXTENSION_PARAM_DT, &dt));
        ASSERT_EQ(0.0f, dt);
    }
    ASSERT_EQ(dmExtension::RESULT_OK, dmExtension::Finalize(&params));
    ASSERT_EQ(0, g_TestInitCount);
    ASSERT_EQ(0, g_TestContextCount);
    ASSERT_EQ((void*)0, ContextRegistryGet(context_registry, "init"));

    dmLog::LogParams log_params;
    dmLog::LogInitialize(&log_params);
    dmLogRegisterListener(CaptureDeprecation);

    dmExtension::BroadcastEvent(&params, dmEngine::EVENT_APP_ACTIVATE, 0, 0);
    ASSERT_EQ(1, g_TestAppEventCount);
    dmExtension::BroadcastEvent(&params, dmEngine::EVENT_APP_DEACTIVATE, 0, 0);
    ASSERT_EQ(0, g_TestAppEventCount);
    ASSERT_EQ(2, g_TestEventCount);
    ASSERT_EQ(2, g_TestNewEventCount);
    dmLog::LogFinalize();
    dmLogUnregisterListener(CaptureDeprecation);
    ExtensionParamsFinalize(&params);

    dmExtension::AppParams appfinalizeparams;
    ExtensionAppParamsInitialize(&appfinalizeparams);
    ExtensionAppParamsSetContextRegistry(&appfinalizeparams, context_registry);

    dmExtension::AppFinalize(&appfinalizeparams);
    ASSERT_EQ(0, g_TestAppInitCount);
    ASSERT_EQ(1, g_TestUnregisterCount);

    // it deregistered its own context
    ASSERT_EQ((void*)0, ContextRegistryGet(context_registry, "lib"));
    ASSERT_EQ((void*)0, ContextRegistryGetByHash(context_registry, dmHashString64("lib")));

    ExtensionAppParamsFinalize(&appfinalizeparams);

    ContextRegistryDestroy(context_registry);

    ASSERT_EQ(1, g_DeprecationWarnings);
    ASSERT_EQ(2, g_SecondEventCalls);
}

static dmExtension::Result RunTypeLifecycle()
{
    HContextRegistry registry = ContextRegistryCreate();
    int engine_context = 1337;
    ContextRegistrySet(registry, "engine", &engine_context);
    dmExtension::AppParams app_params;
    dmExtension::AppParamsInitialize(&app_params);
    ExtensionAppParamsSetContextRegistry(&app_params, registry);
    dmExtension::Params params;
    dmExtension::ParamsInitialize(&params);
    ExtensionParamsSetContextRegistry(&params, registry);
    dmExtension::Result result = dmExtension::AppInitialize(&app_params);
    if (result == dmExtension::RESULT_OK)
    {
        dmExtension::PreRender(&params);
        dmExtension::Update(&params, 0.25f);
        dmExtension::Initialize(&params);
        dmExtension::Update(&params, 0.5f);
        dmExtension::PreRender(&params);
        dmExtension::PostRender(&params);
        dmExtension::Finalize(&params);
        dmExtension::Update(&params, 0.75f);
    }
    dmExtension::AppFinalize(&app_params);
    dmExtension::AppFinalize(&app_params);
    dmExtension::PreRender(&params);
    dmExtension::PostRender(&params);
    dmExtension::ParamsFinalize(&params);
    dmExtension::AppParamsFinalize(&app_params);
    ContextRegistryDestroy(registry);
    return result;
}

// C and C++ declarations coexist, callbacks reset on reboot, and unregister follows successful registration even after init failure.
TEST(dmExtension, TypeRegistrationLifecycle)
{
    // Repeated symbol registration must not corrupt the mixed legacy/new registry.
    TestTypeCpp();
    memset(&g_TypeTest, 0, sizeof(g_TypeTest));
    g_TypeTest.m_OrderValid = true;
    g_TypeCRegisterCount = g_TypeCUnregisterCount = g_TypeCUpdateCount = 0;
    ASSERT_EQ(dmExtension::RESULT_OK, RunTypeLifecycle());
    g_TypeTest.m_SkipUpdate = true;
    ASSERT_EQ(dmExtension::RESULT_OK, RunTypeLifecycle());
    ASSERT_TRUE(g_TypeTest.m_OrderValid);
    ASSERT_EQ(2, g_TypeTest.m_Register);
    ASSERT_EQ(2, g_TypeTest.m_Unregister);
    ASSERT_EQ(2, g_TypeTest.m_AppInitialize);
    ASSERT_EQ(2, g_TypeTest.m_AppFinalize);
    ASSERT_EQ(2, g_TypeTest.m_Initialize);
    ASSERT_EQ(2, g_TypeTest.m_Finalize);
    ASSERT_EQ(1, g_TypeTest.m_Update);
    ASSERT_EQ(0.5f, g_TypeTest.m_DT);
    ASSERT_EQ(4, g_TypeTest.m_PreRender);
    ASSERT_EQ(2, g_TypeTest.m_PostRender);
    ASSERT_EQ(2, g_TypeCRegisterCount);
    ASSERT_EQ(2, g_TypeCUnregisterCount);
    ASSERT_EQ(2, g_TypeCUpdateCount);
    ASSERT_EQ(0.5f, g_TypeCUpdateDT);

    for (int mode = TYPE_TEST_REGISTER_FAIL; mode <= TYPE_TEST_INITIALIZE_FAIL; ++mode)
    {
        memset(&g_TypeTest, 0, sizeof(g_TypeTest));
        g_TypeTest.m_Mode = mode;
        g_TypeTest.m_OrderValid = true;
        dmExtension::Result result = RunTypeLifecycle();
        ASSERT_EQ(mode == TYPE_TEST_INITIALIZE_FAIL ? dmExtension::RESULT_OK : dmExtension::RESULT_INIT_ERROR, result);
        ASSERT_TRUE(g_TypeTest.m_OrderValid);
        ASSERT_EQ(1, g_TypeTest.m_Register);
        ASSERT_EQ(mode != TYPE_TEST_REGISTER_FAIL ? 1 : 0, g_TypeTest.m_Unregister);
        ASSERT_EQ(mode == TYPE_TEST_INITIALIZE_FAIL ? 1 : 0, g_TypeTest.m_AppFinalize);
        ASSERT_EQ(0, g_TypeTest.m_Update);
        ASSERT_EQ(0, g_TypeTest.m_Finalize);
    }
    ASSERT_EQ(0, g_TestAppInitCount);
    ASSERT_EQ(0, g_TestContextCount);
}

struct EventTestContext
{
    HContextRegistry       m_Registry;
    int                    m_Engine;
    dmExtension::AppParams m_AppParams;
    dmExtension::Params    m_Params;
};

static void InitializeEventTest(EventTestContext* context)
{
    memset(&g_TypeTest, 0, sizeof(g_TypeTest));
    g_TypeTest.m_OrderValid = true;
    memset(&g_EventTest, 0, sizeof(g_EventTest));
    g_TypeCEventCount = g_TypeCWindowCount = g_TypeCDeleteCount = 0;
    g_OmitEventCallback = false;
    context->m_Engine = 1337;
    context->m_Registry = ContextRegistryCreate();
    ContextRegistrySet(context->m_Registry, "engine", &context->m_Engine);
    dmExtension::AppParamsInitialize(&context->m_AppParams);
    ExtensionAppParamsSetContextRegistry(&context->m_AppParams, context->m_Registry);
    dmExtension::ParamsInitialize(&context->m_Params);
    ExtensionParamsSetContextRegistry(&context->m_Params, context->m_Registry);
}

static void FinalizeEventTest(EventTestContext* context)
{
    dmExtension::Finalize(&context->m_Params);
    dmExtension::AppFinalize(&context->m_AppParams);
    dmExtension::ParamsFinalize(&context->m_Params);
    dmExtension::AppParamsFinalize(&context->m_AppParams);
    ContextRegistryDestroy(context->m_Registry);
    g_OmitEventCallback = false;
}

// IDs must be matching C/C++ constant expressions; runtime checks verify their names and source handles.
TEST(dmExtension, BuiltinEventHashes)
{
    DM_STATIC_ASSERT(ENGINE_EVENT_APP_ACTIVATE == dmEngine::EVENT_APP_ACTIVATE, Engine_Event_APP_ACTIVATE_Mismatch);
    DM_STATIC_ASSERT(ENGINE_EVENT_APP_DEACTIVATE == dmEngine::EVENT_APP_DEACTIVATE, Engine_Event_APP_DEACTIVATE_Mismatch);
    DM_STATIC_ASSERT(ENGINE_EVENT_APP_ICONIFY == dmEngine::EVENT_APP_ICONIFY, Engine_Event_APP_ICONIFY_Mismatch);
    DM_STATIC_ASSERT(ENGINE_EVENT_APP_DEICONIFY == dmEngine::EVENT_APP_DEICONIFY, Engine_Event_APP_DEICONIFY_Mismatch);
    DM_STATIC_ASSERT(ENGINE_EVENT_ENGINE_INITIALIZED == dmEngine::EVENT_ENGINE_INITIALIZED, Engine_Event_ENGINE_INITIALIZED_Mismatch);
    DM_STATIC_ASSERT(ENGINE_EVENT_ENGINE_DELETE == dmEngine::EVENT_ENGINE_DELETE, Engine_Event_ENGINE_DELETE_Mismatch);
    DM_STATIC_ASSERT(ENGINE_EVENT_WINDOW_RESIZED == dmEngine::EVENT_WINDOW_RESIZED, Engine_Event_WINDOW_RESIZED_Mismatch);
    DM_STATIC_ASSERT(ENGINE_EVENT_INPUT_ACTIONS == dmEngine::EVENT_INPUT_ACTIONS, Engine_Event_INPUT_ACTIONS_Mismatch);

    EventTestContext context;
    InitializeEventTest(&context);
    ASSERT_EQ(dmExtension::RESULT_OK, dmExtension::AppInitialize(&context.m_AppParams));
    const dmhash_t events[] = {
        dmEngine::EVENT_APP_ACTIVATE,
        dmEngine::EVENT_APP_DEACTIVATE,
        dmEngine::EVENT_APP_ICONIFY,
        dmEngine::EVENT_APP_DEICONIFY,
        dmEngine::EVENT_ENGINE_INITIALIZED,
        dmEngine::EVENT_ENGINE_DELETE,
        dmEngine::EVENT_WINDOW_RESIZED,
    };
    const char* names[] = {
        "_dm_event_app_activate",
        "_dm_event_app_deactivate",
        "_dm_event_app_iconify",
        "_dm_event_app_deiconify",
        "_dm_event_engine_initialized",
        "_dm_event_engine_delete",
        "_dm_event_window_resized",
    };
    // Compare opaque identity tokens without invoking engine or window APIs.
    dmEngine::HEngine engine = (dmEngine::HEngine)&context.m_Engine;
    HWindow window = (HWindow) & context;
    for (uint32_t i = 0; i < DM_ARRAY_SIZE(events); ++i)
    {
        EXPECT_EQ(dmHashString64(names[i]), events[i]);
        TestTypeCBroadcastBuiltinEvent(&context.m_Params, i, engine, window);
        EXPECT_EQ(events[i], g_EventTest.m_LastId);
        EXPECT_EQ(engine, g_EventTest.m_SourceEngine);
        EXPECT_EQ(engine, g_TypeCEventEngine);
        HWindow expected_window = i == 6 ? window : 0;
        EXPECT_EQ(expected_window, g_EventTest.m_SourceWindow);
        EXPECT_EQ(expected_window, g_TypeCEventWindow);
    }
    EXPECT_EQ(7u, g_EventTest.m_Count);
    EXPECT_EQ(7, g_TypeCEventCount);
    EXPECT_EQ(1u, g_EventTest.m_WindowCount);
    EXPECT_EQ(1, g_TypeCWindowCount);
    EXPECT_EQ(640u, g_EventTest.m_Width);
    EXPECT_EQ(360u, g_EventTest.m_Height);
    EXPECT_EQ(640u, g_TypeCWindowWidth);
    EXPECT_EQ(360u, g_TypeCWindowHeight);
    EXPECT_EQ(dmHashString64("_dm_event_input_actions"), dmEngine::EVENT_INPUT_ACTIONS);
    FinalizeEventTest(&context);
}

// Public broadcasts preserve arbitrary hashes and opaque payloads for C/C++ receivers.
TEST(dmExtension, PublicCustomEventBroadcast)
{
    EventTestContext context;
    InitializeEventTest(&context);
    const uint8_t bytes[] = { 1, 2, 3 };
    dmhash_t event_id = dmHashString64("test.custom_event");
    dmExtension::BroadcastEvent(&context.m_Params, event_id, bytes, sizeof(bytes));
    EXPECT_EQ(0u, g_EventTest.m_Count);
    ASSERT_EQ(dmExtension::RESULT_OK, dmExtension::AppInitialize(&context.m_AppParams));
    int fixture_count = g_TestEventCount;
    int new_fixture_count = g_TestNewEventCount;
    dmExtension::BroadcastEvent(&context.m_Params, event_id, bytes, sizeof(bytes));
    EXPECT_EQ(1u, g_EventTest.m_Count);
    EXPECT_EQ(1, g_TypeCEventCount);
    EXPECT_EQ(event_id, g_EventTest.m_LastId);
    EXPECT_EQ((const void*)bytes, g_EventTest.m_LastData);
    EXPECT_EQ((uint32_t)sizeof(bytes), g_EventTest.m_LastDataSize);
    EXPECT_EQ(0u, g_EventTest.m_CustomValue);
    uint32_t value = 0x12345678;
    dmExtension::BroadcastEvent(&context.m_Params, event_id, &value, sizeof(value));
    EXPECT_EQ(2u, g_EventTest.m_Count);
    EXPECT_EQ(2, g_TypeCEventCount);
    EXPECT_EQ(value, g_EventTest.m_CustomValue);
    EXPECT_EQ(fixture_count, g_TestEventCount);
    EXPECT_EQ(new_fixture_count + 2, g_TestNewEventCount);
    dmExtension::AppFinalize(&context.m_AppParams);
    dmExtension::BroadcastEvent(&context.m_Params, event_id, &value, sizeof(value));
    EXPECT_EQ(2u, g_EventTest.m_Count);
    EXPECT_EQ(2, g_TypeCEventCount);
    EXPECT_EQ(new_fixture_count + 2, g_TestNewEventCount);
    FinalizeEventTest(&context);
}

// Lifecycle and resize broadcasts reach all eligible generic and compatible legacy receivers.
TEST(dmExtension, GenericBroadcastDelivery)
{
    EventTestContext context;
    InitializeEventTest(&context);
    ASSERT_EQ(dmExtension::RESULT_OK, dmExtension::AppInitialize(&context.m_AppParams));
    int observer_count = g_SecondEventCalls;
    int fixture_count = g_TestEventCount;
    int new_fixture_count = g_TestNewEventCount;
    const dmhash_t ids[] = {
        dmEngine::EVENT_APP_ACTIVATE,
        dmEngine::EVENT_APP_DEACTIVATE,
        dmEngine::EVENT_APP_ICONIFY,
        dmEngine::EVENT_APP_DEICONIFY,
        dmEngine::EVENT_ENGINE_INITIALIZED,
        dmEngine::EVENT_ENGINE_DELETE,
    };
    for (uint32_t i = 0; i < DM_ARRAY_SIZE(ids); ++i)
    {
        dmExtension::BroadcastEvent(&context.m_Params, ids[i], 0, 0);
    }
    dmEngine::WindowResizedEvent window = { 0, 0, 321, 123 };
    dmExtension::BroadcastEvent(&context.m_Params, dmEngine::EVENT_WINDOW_RESIZED, &window, sizeof(window));
    EXPECT_EQ(7u, g_EventTest.m_Count);
    EXPECT_EQ(7, g_TypeCEventCount);
    EXPECT_EQ(observer_count + 7, g_SecondEventCalls);
    EXPECT_EQ(fixture_count + 6, g_TestEventCount);
    EXPECT_EQ(new_fixture_count + 7, g_TestNewEventCount);
    EXPECT_EQ(1u, g_EventTest.m_DeleteCount);
    EXPECT_EQ(1, g_TypeCDeleteCount);
    EXPECT_EQ(1u, g_EventTest.m_WindowCount);
    EXPECT_EQ(1, g_TypeCWindowCount);
    FinalizeEventTest(&context);
}

// Each extension owns its heap pointer across all callback phases; nested delivery, reboot and failures preserve isolation.
TEST(dmExtension, UserDataLifecycle)
{
    g_UserDataValid = true;
    g_TypeCUserDataValid = 1;
    for (int cycle = 0; cycle < 3; ++cycle)
    {
        EventTestContext context;
        InitializeEventTest(&context);
        g_SkipUserData = cycle == 1;
        EXPECT_EQ((void*)0, dmExtension::AppParamsGetUserData(&context.m_AppParams));
        EXPECT_EQ((void*)0, dmExtension::ParamsGetUserData(&context.m_Params));
        EXPECT_EQ(dmExtension::RESULT_OK, dmExtension::AppInitialize(&context.m_AppParams));
        EXPECT_EQ(dmExtension::RESULT_OK, dmExtension::Initialize(&context.m_Params));
        dmExtension::PreRender(&context.m_Params);
        g_DispatchDuringUpdate = true;
        dmExtension::Update(&context.m_Params, 0.1f);
        g_DispatchDuringUpdate = false;
        dmExtension::PostRender(&context.m_Params);
        g_EventTest.m_DispatchNested = true;
        dmEngine::WindowResizedEvent window = { 0, 0, 100, 50 };
        dmExtension::BroadcastEvent(&context.m_Params, dmEngine::EVENT_WINDOW_RESIZED, &window, sizeof(window));
        dmExtension::BroadcastEvent(&context.m_Params, dmEngine::EVENT_ENGINE_DELETE, 0, 0);
        EXPECT_EQ((void*)0, dmExtension::AppParamsGetUserData(&context.m_AppParams));
        EXPECT_EQ((void*)0, dmExtension::ParamsGetUserData(&context.m_Params));
        dmExtension::Finalize(&context.m_Params);
        dmExtension::AppFinalize(&context.m_AppParams);
        EXPECT_EQ((void*)0, dmExtension::AppParamsGetUserData(&context.m_AppParams));
        EXPECT_EQ((void*)0, dmExtension::ParamsGetUserData(&context.m_Params));
        EXPECT_EQ((int*)0, g_TypeTest.m_UserData);
        FinalizeEventTest(&context);
    }
    g_SkipUserData = false;

    for (int mode = TYPE_TEST_REGISTER_FAIL; mode <= TYPE_TEST_INITIALIZE_FAIL; ++mode)
    {
        EventTestContext context;
        InitializeEventTest(&context);
        g_TypeTest.m_Mode = mode;
        if (dmExtension::AppInitialize(&context.m_AppParams) == dmExtension::RESULT_OK)
        {
            dmExtension::Initialize(&context.m_Params);
        }
        dmExtension::BroadcastEvent(&context.m_Params, dmEngine::EVENT_ENGINE_DELETE, 0, 0);
        FinalizeEventTest(&context);
        EXPECT_EQ((int*)0, g_TypeTest.m_UserData);
    }
    ASSERT_TRUE(g_UserDataValid);
    ASSERT_EQ(1, g_TypeCUserDataValid);
}

// A catch-all handler receives raw arguments for both outer and nested broadcasts.
TEST(dmExtension, GenericEventCallbackArguments)
{
    EventTestContext context;
    InitializeEventTest(&context);
    EXPECT_EQ(dmExtension::RESULT_OK, dmExtension::AppInitialize(&context.m_AppParams));
    dmEngine::WindowResizedEvent data = { 0, 0, 320, 200 };
    dmExtension::BroadcastEvent(&context.m_Params, dmEngine::EVENT_WINDOW_RESIZED, &data, sizeof(data));
    EXPECT_EQ(dmEngine::EVENT_WINDOW_RESIZED, g_EventTest.m_LastId);
    EXPECT_EQ((const void*)&data, g_EventTest.m_LastData);
    EXPECT_EQ((uint32_t)sizeof(data), g_EventTest.m_LastDataSize);

    g_EventTest.m_DispatchNested = true;
    dmExtension::BroadcastEvent(&context.m_Params, dmEngine::EVENT_WINDOW_RESIZED, &data, sizeof(data));
    EXPECT_EQ(dmHashString64("nested_event"), g_EventTest.m_LastId);
    EXPECT_EQ((const void*)g_NestedPayload, g_EventTest.m_LastData);
    EXPECT_EQ((uint32_t)sizeof(g_NestedPayload), g_EventTest.m_LastDataSize);
    EXPECT_EQ(2u, g_EventTest.m_WindowCount);
    EXPECT_EQ(2, g_TypeCWindowCount);
    FinalizeEventTest(&context);
}

// Generic events keep the application lifetime, adapt all legacy ids once, and reset registration on reboot.
TEST(dmExtension, GenericEventLifecycle)
{
    EventTestContext context;
    InitializeEventTest(&context);
    dmEngine::WindowResizedEvent window = { 0, 0, 123, 45 };
    dmExtension::BroadcastEvent(&context.m_Params, dmEngine::EVENT_WINDOW_RESIZED, &window, sizeof(window));
    EXPECT_EQ(0u, g_EventTest.m_Count);
    EXPECT_EQ(dmExtension::RESULT_OK, dmExtension::AppInitialize(&context.m_AppParams));
    dmExtension::BroadcastEvent(&context.m_Params, dmEngine::EVENT_WINDOW_RESIZED, &window, sizeof(window));
    EXPECT_EQ(1u, g_EventTest.m_WindowCount);
    EXPECT_EQ(1, g_TypeCWindowCount);

    uint32_t observer_count = g_SecondEventCalls;
    int fixture_count = g_TestEventCount;
    const dmhash_t ids[] = {
        dmEngine::EVENT_APP_ACTIVATE,
        dmEngine::EVENT_APP_DEACTIVATE,
        dmEngine::EVENT_APP_ICONIFY,
        dmEngine::EVENT_APP_DEICONIFY,
        dmEngine::EVENT_ENGINE_INITIALIZED,
        dmEngine::EVENT_ENGINE_DELETE,
    };
    const char* names[] = {
        "_dm_event_app_activate",
        "_dm_event_app_deactivate",
        "_dm_event_app_iconify",
        "_dm_event_app_deiconify",
        "_dm_event_engine_initialized",
        "_dm_event_engine_delete",
    };
    for (uint32_t i = 0; i < DM_ARRAY_SIZE(ids); ++i)
    {
        dmExtension::BroadcastEvent(&context.m_Params, ids[i], 0, 0);
        EXPECT_EQ(dmHashString64(names[i]), g_EventTest.m_LastId);
        EXPECT_EQ((const void*)0, g_EventTest.m_LastData);
        EXPECT_EQ(0u, g_EventTest.m_LastDataSize);
    }
    EXPECT_EQ(observer_count + DM_ARRAY_SIZE(ids), (uint32_t)g_SecondEventCalls);
    EXPECT_EQ(fixture_count + DM_ARRAY_SIZE(ids), (uint32_t)g_TestEventCount);
    EXPECT_EQ(1u, g_EventTest.m_DeleteCount);
    EXPECT_EQ(1, g_TypeCDeleteCount);
    EXPECT_EQ(dmExtension::RESULT_OK, dmExtension::Initialize(&context.m_Params));
    dmExtension::Finalize(&context.m_Params);
    dmExtension::BroadcastEvent(&context.m_Params, dmEngine::EVENT_WINDOW_RESIZED, &window, sizeof(window));
    EXPECT_EQ(2u, g_EventTest.m_WindowCount);
    dmExtension::AppFinalize(&context.m_AppParams);
    uint32_t count = g_EventTest.m_Count;
    dmExtension::BroadcastEvent(&context.m_Params, dmEngine::EVENT_ENGINE_DELETE, 0, 0);
    EXPECT_EQ(count, g_EventTest.m_Count);

    g_OmitEventCallback = true;
    EXPECT_EQ(dmExtension::RESULT_OK, dmExtension::AppInitialize(&context.m_AppParams));
    dmExtension::BroadcastEvent(&context.m_Params, dmEngine::EVENT_WINDOW_RESIZED, &window, sizeof(window));
    EXPECT_EQ(count, g_EventTest.m_Count);
    EXPECT_EQ(3, g_TypeCWindowCount);
    FinalizeEventTest(&context);
    ASSERT_TRUE(g_TypeTest.m_OrderValid);
}

// Engine-delete reaches successful app handlers after engine init failure, but never failed registrations/apps.
TEST(dmExtension, GenericEventInitializationFailure)
{
    for (int mode = TYPE_TEST_REGISTER_FAIL; mode <= TYPE_TEST_INITIALIZE_FAIL; ++mode)
    {
        EventTestContext context;
        InitializeEventTest(&context);
        g_TypeTest.m_Mode = mode;
        dmExtension::Result result = dmExtension::AppInitialize(&context.m_AppParams);
        if (result == dmExtension::RESULT_OK)
        {
            dmExtension::Initialize(&context.m_Params);
        }
        dmExtension::BroadcastEvent(&context.m_Params, dmEngine::EVENT_ENGINE_DELETE, 0, 0);
        FinalizeEventTest(&context);
        ASSERT_EQ(mode == TYPE_TEST_INITIALIZE_FAIL ? 1u : 0u, g_EventTest.m_DeleteCount);
        ASSERT_TRUE(g_TypeTest.m_OrderValid);
    }
}

int main(int argc, char **argv)
{
    jc_test_init(&argc, argv);
    return jc_test_run_all();
}
