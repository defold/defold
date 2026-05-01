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


// Extension in a separate library. See comment in test_sdk_lib.cpp
extern int g_TestAppInitCount;
extern int g_TestAppEventCount;

TEST(testSdk, Basic)
{
    dmExtension::AppParams appparams;
    ASSERT_EQ(0, g_TestAppInitCount);
    ASSERT_EQ(dmExtension::RESULT_OK, dmExtension::AppInitialize(&appparams));
    ASSERT_EQ(1, g_TestAppInitCount);
    ASSERT_EQ(0, dmExtension::GetNextExtension(dmExtension::GetFirstExtension()));

    dmExtension::Params params;
    dmExtension::Event event;
    event.m_Event = (ExtensionEventID)dmExtension::EVENT_ID_ACTIVATEAPP;
    dmExtension::DispatchEvent(&params, &event);
    ASSERT_EQ(1, g_TestAppEventCount);
    event.m_Event = (ExtensionEventID)dmExtension::EVENT_ID_DEACTIVATEAPP;
    dmExtension::DispatchEvent(&params, &event);
    ASSERT_EQ(0, g_TestAppEventCount);

    dmExtension::AppFinalize(&appparams);
    ASSERT_EQ(0, g_TestAppInitCount);
}

int main(int argc, char **argv)
{
    jc_test_init(&argc, argv);
    return jc_test_run_all();
}
