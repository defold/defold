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

#include <dmsdk/extension/extension.h>
#include <dlib/context_registry.h>
#include <extension/extension.h>
#include "test_engine_api.h"

typedef struct EngineApi
{
    HEngine            m_Engine;
    HContextRegistry   m_Registry;
    HContextRegistry   m_EmptyRegistry;
    ExtensionParams    m_Params;
    ExtensionAppParams m_AppParams;
} EngineApi;

JC_TEST_FIXTURE_SETUP(EngineApi)
{
    ExtensionParamsInitialize(&jc_test_fixture->m_Params);
    ExtensionAppParamsInitialize(&jc_test_fixture->m_AppParams);
    jc_test_fixture->m_Engine = TestEngineNew(&jc_test_fixture->m_Registry);
    jc_test_fixture->m_EmptyRegistry = ContextRegistryCreate();
}

JC_TEST_FIXTURE_TEARDOWN(EngineApi)
{
    ExtensionAppParamsFinalize(&jc_test_fixture->m_AppParams);
    ExtensionParamsFinalize(&jc_test_fixture->m_Params);
    ContextRegistryDestroy(jc_test_fixture->m_EmptyRegistry);
    TestEngineDelete(jc_test_fixture->m_Engine);
}

// The C window accessor follows the engine from construction to successful initialization.
TEST_F(EngineApi, WindowAccessor)
{
    ASSERT_EQ((HWindow)0, EngineGetWindow(jc_test_fixture->m_Engine));

    HWindow window = 0;
    ASSERT_TRUE(TestEngineInit(jc_test_fixture->m_Engine, &window));
    ASSERT_NE((HWindow)0, window);
    ASSERT_EQ(window, EngineGetWindow(jc_test_fixture->m_Engine));
}

// C update parameters support standalone, missing-entry and engine-owned registries before initialization.
TEST_F(EngineApi, ExtensionParamsContext)
{
    ExtensionParams* params = &jc_test_fixture->m_Params;
    ASSERT_EQ((HContextRegistry)0, ExtensionParamsGetContextRegistry(params));

    ExtensionParamsSetContextRegistry(params, jc_test_fixture->m_EmptyRegistry);
    HContextRegistry registry = ExtensionParamsGetContextRegistry(params);
    ASSERT_EQ(jc_test_fixture->m_EmptyRegistry, registry);
    ASSERT_EQ((void*)0, ContextRegistryGet(registry, ENGINE_CONTEXT_NAME));
    ASSERT_EQ((void*)0, ContextRegistryGetByHash(registry, dmHashString64(ENGINE_CONTEXT_NAME)));

    ExtensionParamsSetContextRegistry(params, jc_test_fixture->m_Registry);
    registry = ExtensionParamsGetContextRegistry(params);
    ASSERT_EQ(jc_test_fixture->m_Registry, registry);
    ASSERT_EQ(jc_test_fixture->m_Engine, (HEngine)ContextRegistryGet(registry, ENGINE_CONTEXT_NAME));
    ASSERT_EQ(jc_test_fixture->m_Engine, (HEngine)ContextRegistryGetByHash(registry, dmHashString64(ENGINE_CONTEXT_NAME)));
}

// C application parameters expose the same borrowed engine registry, including standalone and missing entries.
TEST_F(EngineApi, ExtensionAppParamsContext)
{
    ExtensionAppParams* params = &jc_test_fixture->m_AppParams;
    ASSERT_EQ((HContextRegistry)0, ExtensionAppParamsGetContextRegistry(params));

    ExtensionAppParamsSetContextRegistry(params, jc_test_fixture->m_EmptyRegistry);
    HContextRegistry registry = ExtensionAppParamsGetContextRegistry(params);
    ASSERT_EQ(jc_test_fixture->m_EmptyRegistry, registry);
    ASSERT_EQ((void*)0, ContextRegistryGet(registry, ENGINE_CONTEXT_NAME));
    ASSERT_EQ((void*)0, ContextRegistryGetByHash(registry, dmHashString64(ENGINE_CONTEXT_NAME)));

    ExtensionAppParamsSetContextRegistry(params, jc_test_fixture->m_Registry);
    registry = ExtensionAppParamsGetContextRegistry(params);
    ASSERT_EQ(jc_test_fixture->m_Registry, registry);
    ASSERT_EQ(jc_test_fixture->m_Engine, (HEngine)ContextRegistryGet(registry, ENGINE_CONTEXT_NAME));
    ASSERT_EQ(jc_test_fixture->m_Engine, (HEngine)ContextRegistryGetByHash(registry, dmHashString64(ENGINE_CONTEXT_NAME)));
}

int TestEngineApiRunTests(int argc, char** argv)
{
    jc_test_init(&argc, argv);
    return jc_test_run_all();
}
