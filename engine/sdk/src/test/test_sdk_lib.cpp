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
#include <dmsdk/engine/engine_event.h>

int g_TestAppInitCount = 0;
int g_TestAppEventCount = 0;
int g_TestCustomEventCount = 0;
uint32_t g_TestCustomEventValue = 0;
bool g_TestCustomEventDTValid = true;

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
    float before = -1.0f;
    g_TestCustomEventDTValid &= dmExtension::ParamsGetFloat(params, dmExtension::PARAM_DT, &before);
    uint32_t value = 42;
    dmExtension::BroadcastEvent(params, dmHashString64("sdk.custom_event"), &value, sizeof(value));
    float after = -1.0f;
    g_TestCustomEventDTValid &= dmExtension::ParamsGetFloat(params, dmExtension::PARAM_DT, &after) && after == before;
    return dmExtension::RESULT_OK;
}

void OnEventTest(dmExtension::Params* params, dmhash_t event_id, const void* data, uint32_t data_size)
{
    if (event_id == dmEngine::EVENT_ACTIVATEAPP)
    {
        ++g_TestAppEventCount;
    }
    else if (event_id == dmEngine::EVENT_DEACTIVATEAPP)
    {
        --g_TestAppEventCount;
    }
    else if (event_id == dmHashString64("sdk.custom_event") && data_size == sizeof(uint32_t))
    {
        ++g_TestCustomEventCount;
        g_TestCustomEventValue = *(const uint32_t*)data;
        float dt = -1.0f;
        g_TestCustomEventDTValid &= dmExtension::ParamsGetFloat(params, dmExtension::PARAM_DT, &dt) && dt == 0.0f;
    }
}

dmExtension::Result FinalizeTest(dmExtension::Params* params)
{
    return dmExtension::RESULT_OK;
}

static dmExtension::Result RegisterTest(dmExtension::HExtension extension)
{
    dmExtension::SetAppInitializeFn(extension, AppInitializeTest);
    dmExtension::SetAppFinalizeFn(extension, AppFinalizeTest);
    dmExtension::SetInitializeFn(extension, InitializeTest);
    dmExtension::SetFinalizeFn(extension, FinalizeTest);
    dmExtension::SetUpdateFn(extension, UpdateTest);
    dmExtension::SetEventFn(extension, OnEventTest);
    return dmExtension::RESULT_OK;
}

DM_DECLARE_EXTENSION(TestSdk, RegisterTest, 0);
