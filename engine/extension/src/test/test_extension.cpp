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
#include "test_extension.h"
#include <dlib/array.h>
#include <dlib/context_registry.h>
#include <dlib/hashtable.h>

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
extern int g_TestContextCount;

extern "C" void TestExt();

struct Initializer {
    Initializer() {
        TestExt();
    }
} g_SymbolInitializer;

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
    dmExtension::HExtension extension = dmExtension::GetFirstExtension();
    ASSERT_NE((dmExtension::HExtension)0, extension);
    ASSERT_EQ((dmExtension::HExtension)0, dmExtension::GetNextExtension(extension));

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

    dmExtension::Event event;
    event.m_Event = (ExtensionEventID)dmExtension::EVENT_ID_ACTIVATEAPP;
    dmExtension::DispatchEvent(&params, &event);
    ASSERT_EQ(1, g_TestAppEventCount);
    event.m_Event = (ExtensionEventID)dmExtension::EVENT_ID_DEACTIVATEAPP;
    dmExtension::DispatchEvent(&params, &event);
    ASSERT_EQ(0, g_TestAppEventCount);

    ExtensionParamsFinalize(&params);

    dmExtension::AppParams appfinalizeparams;
    ExtensionAppParamsInitialize(&appfinalizeparams);
    ExtensionAppParamsSetContextRegistry(&appfinalizeparams, context_registry);

    dmExtension::AppFinalize(&appfinalizeparams);
    ASSERT_EQ(0, g_TestAppInitCount);

    // it deregistered its own context
    ASSERT_EQ((void*)0, ContextRegistryGet(context_registry, "lib"));
    ASSERT_EQ((void*)0, ContextRegistryGetByHash(context_registry, dmHashString64("lib")));

    ExtensionAppParamsFinalize(&appfinalizeparams);

    ContextRegistryDestroy(context_registry);
}

int main(int argc, char **argv)
{
    jc_test_init(&argc, argv);
    return jc_test_run_all();
}
