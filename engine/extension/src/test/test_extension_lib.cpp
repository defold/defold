// Copyright 2020-2024 The Defold Foundation
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
#include "../extension.h"
#include "test_extension.h"

extern "C"
{
#include "lua/lua.h"
#include "lua/lauxlib.h"
}

// Extension created in a separate lib in order to
// test potential problems with dead stripping of symbols

int g_TestAppInitCount = 0;
int g_TestAppEventCount = 0;

static dmExtension::Result AppInitializeTest(dmExtension::AppParams* params)
{
    g_TestAppInitCount++;
    return dmExtension::RESULT_OK;
}

static dmExtension::Result AppFinalizeTest(dmExtension::AppParams* params)
{
    g_TestAppInitCount--;
    return dmExtension::RESULT_OK;
}

static dmExtension::Result InitializeTest(dmExtension::Params* params)
{
    return dmExtension::RESULT_OK;
}

static dmExtension::Result UpdateTest(dmExtension::Params* params)
{
    return dmExtension::RESULT_OK;
}

void OnEventTest(dmExtension::Params* params, const dmExtension::Event* event)
{
    if( event->m_Event == EXTENSION_EVENT_ID_ACTIVATEAPP )
        ++g_TestAppEventCount;
    else if(event->m_Event == EXTENSION_EVENT_ID_DEACTIVATEAPP)
        --g_TestAppEventCount;
}

static dmExtension::Result FinalizeTest(dmExtension::Params* params)
{
    return dmExtension::RESULT_OK;
}

DM_DECLARE_EXTENSION(TestExt, "test", AppInitializeTest, AppFinalizeTest, InitializeTest, UpdateTest, OnEventTest, FinalizeTest);
