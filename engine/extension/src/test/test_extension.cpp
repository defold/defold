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
#include "../extension.hpp"
#include "test_extension.h"
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
extern int g_TestAppEventCount;

extern "C" void TestExt();

struct Initializer {
    Initializer() {
        TestExt();
    }
} g_SymbolInitializer;

TEST(dmExtension, Basic)
{
    dmExtension::AppParams appparams;
    ExtensionAppParamsInitialize(&appparams);

    int engine_context = 1337;
    ExtensionAppParamsSetContext(&appparams, "engine", &engine_context);

    ASSERT_EQ(0, g_TestAppInitCount);
    ASSERT_EQ(dmExtension::RESULT_OK, dmExtension::AppInitialize(&appparams));
    ASSERT_EQ(1, g_TestAppInitCount);
    dmExtension::HExtension extension = dmExtension::GetFirstExtension();
    ASSERT_NE((dmExtension::HExtension)0, extension);
    ASSERT_EQ((dmExtension::HExtension)0, dmExtension::GetNextExtension(extension));

    ASSERT_NE((void*)0, ExtensionAppParamsGetContextByName(&appparams, "lib"));
    ASSERT_NE((void*)0, ExtensionAppParamsGetContext(&appparams, dmHashString64("lib")));

    dmExtension::Params params;
    ExtensionParamsInitialize(&params);

    ASSERT_EQ(dmExtension::RESULT_OK, dmExtension::Initialize(&params));
    ASSERT_EQ(1, g_TestInitCount);

    for (int i = 0; i < 5; ++i)
    {
        ASSERT_EQ(dmExtension::RESULT_OK, dmExtension::Update(&params));
        ASSERT_EQ(i+1, g_TestUpdateCount);

    }
    ASSERT_EQ(dmExtension::RESULT_OK, dmExtension::Finalize(&params));
    ASSERT_EQ(0, g_TestInitCount);

    ExtensionParamsFinalize(&params);

    dmExtension::Event event;
    event.m_Event = (ExtensionEventID)dmExtension::EVENT_ID_ACTIVATEAPP;
    dmExtension::DispatchEvent(&params, &event);
    ASSERT_EQ(1, g_TestAppEventCount);
    event.m_Event = (ExtensionEventID)dmExtension::EVENT_ID_DEACTIVATEAPP;
    dmExtension::DispatchEvent(&params, &event);
    ASSERT_EQ(0, g_TestAppEventCount);

    dmExtension::AppFinalize(&appparams);
    ASSERT_EQ(0, g_TestAppInitCount);

    // it deregistered its own context
    ASSERT_EQ((void*)0, ExtensionAppParamsGetContextByName(&appparams, "lib"));
    ASSERT_EQ((void*)0, ExtensionAppParamsGetContext(&appparams, dmHashString64("lib")));

    ExtensionAppParamsFinalize(&appparams);
}

int main(int argc, char **argv)
{
    jc_test_init(&argc, argv);
    return jc_test_run_all();
}
