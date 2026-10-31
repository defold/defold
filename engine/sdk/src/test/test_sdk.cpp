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
#include <testmain/testmain.h>
#define JC_TEST_IMPLEMENTATION
#include <jc_test/jc_test.h>
#include <extension/extension.hpp>
#include <dmsdk/engine/engine_event.h>


// Extension in a separate library. See comment in test_sdk_lib.cpp
extern int g_TestAppInitCount;
extern int g_TestAppEventCount;
extern int g_TestCustomEventCount;
extern uint32_t g_TestCustomEventValue;
extern bool g_TestCustomEventDTValid;
extern "C" void dmExportedSymbols();

// Installed SDK broadcasts preserve arbitrary payloads and restore Update dt after nested event callbacks.
TEST(testSdk, Basic)
{
    dmExtension::AppParams appparams;
    dmExtension::AppParamsInitialize(&appparams);
    ASSERT_EQ(0, g_TestAppInitCount);
    ASSERT_EQ(dmExtension::RESULT_OK, dmExtension::AppInitialize(&appparams));
    ASSERT_EQ(1, g_TestAppInitCount);
    ASSERT_EQ(0, dmExtension::GetNextExtension(dmExtension::GetFirstExtension()));

    dmExtension::Params params;
    dmExtension::ParamsInitialize(&params);
    dmEngine::AppEvent event = { 0, 0 };
    dmExtension::BroadcastEvent(&params, dmEngine::EVENT_APP_ACTIVATE, &event, sizeof(event));
    ASSERT_EQ(1, g_TestAppEventCount);
    dmExtension::BroadcastEvent(&params, dmEngine::EVENT_APP_DEACTIVATE, &event, sizeof(event));
    ASSERT_EQ(0, g_TestAppEventCount);

    ASSERT_EQ(dmExtension::RESULT_OK, dmExtension::Initialize(&params));
    ASSERT_EQ(dmExtension::RESULT_OK, dmExtension::Update(&params, 0.1f));
    ASSERT_EQ(1, g_TestCustomEventCount);
    ASSERT_EQ(42u, g_TestCustomEventValue);
    ASSERT_TRUE(g_TestCustomEventDTValid);
    uint32_t value = 99;
    dmExtension::BroadcastEvent(&params, dmHashString64("sdk.custom_event"), &value, sizeof(value));
    ASSERT_EQ(2, g_TestCustomEventCount);
    ASSERT_EQ(value, g_TestCustomEventValue);
    ASSERT_TRUE(g_TestCustomEventDTValid);
    ASSERT_EQ(dmExtension::RESULT_OK, dmExtension::Finalize(&params));

    dmExtension::AppFinalize(&appparams);
    dmExtension::ParamsFinalize(&params);
    dmExtension::AppParamsFinalize(&appparams);
    ASSERT_EQ(0, g_TestAppInitCount);
}

int main(int argc, char **argv)
{
    dmExportedSymbols();
    jc_test_init(&argc, argv);
    return jc_test_run_all();
}
