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


// Extension created in a separate lib in order to
// test potential problems with incorrect non-sdk includes from sdk files.
// ONLY c/c++ std includes or dmsdk includes are allowed!
// Attempting other includes will fail!

// Include generated sdk.h files including all sdk include files.
#include <dmsdk/sdk.h>

int g_TestAppInitCount = 0;
int g_TestAppEventCount = 0;

dmExtension::Result AppInitializeTest(dmExtension::AppParams* params)
{
    g_TestAppInitCount++;
    return dmExtension::RESULT_OK;
}

dmExtension::Result AppFinalizeTest(dmExtension::AppParams* params)
{
    g_TestAppInitCount--;
    return dmExtension::RESULT_OK;
}

dmExtension::Result InitializeTest(dmExtension::Params* params)
{
    return dmExtension::RESULT_OK;
}

dmExtension::Result UpdateTest(dmExtension::Params* params)
{
    return dmExtension::RESULT_OK;
}

void OnEventTest(dmExtension::Params* params, const dmExtension::Event* event)
{
    if( event->m_Event == dmExtension::EVENT_ID_ACTIVATEAPP )
        ++g_TestAppEventCount;
    else if(event->m_Event == dmExtension::EVENT_ID_DEACTIVATEAPP)
        --g_TestAppEventCount;
}

dmExtension::Result FinalizeTest(dmExtension::Params* params)
{
    return dmExtension::RESULT_OK;
}

DM_DECLARE_EXTENSION(TestSdk, "test", AppInitializeTest, AppFinalizeTest, InitializeTest, UpdateTest, OnEventTest, FinalizeTest);

