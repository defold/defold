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

#include <dmsdk/extension/extension.h>
#include <dmsdk/engine/engine_event.h>
#include <stdlib.h>

int g_TypeCRegisterCount;
int g_TypeCUnregisterCount;
int g_TypeCUpdateCount;
float g_TypeCUpdateDT;
int g_TypeCEventCount;
int g_TypeCWindowCount;
int g_TypeCDeleteCount;
uint32_t g_TypeCWindowWidth;
uint32_t g_TypeCWindowHeight;
int g_TypeCUserDataValid = 1;
HEngine g_TypeCEventEngine;
EngineState g_TypeCEventState;
HWindow g_TypeCEventWindow;
static int* g_UserData;

void TestTypeCBroadcastBuiltinEvent(ExtensionParams* params, uint32_t index, HEngine engine, HWindow window)
{
    const EngineState states[] = {
        ENGINE_STATE_APP_ACTIVATE,
        ENGINE_STATE_APP_DEACTIVATE,
        ENGINE_STATE_APP_ICONIFY,
        ENGINE_STATE_APP_DEICONIFY,
        ENGINE_STATE_INITIALIZED,
        ENGINE_STATE_DELETE,
    };
    if (index == 6)
    {
        EngineWindowResizedEvent data = {
            engine, window, 640, 360
        };
        ExtensionBroadcastEvent(params, ENGINE_EVENT_WINDOW_RESIZED, &data, sizeof(data));
    }
    else
    {
        EngineStateEvent data = {
            engine, states[index]
        };
        ExtensionBroadcastEvent(params, ENGINE_STATE_EVENT, &data, sizeof(data));
    }
}

static void CheckUserData(void* data)
{
    g_TypeCUserDataValid &= data == g_UserData;
    if (data == g_UserData && data)
    {
        g_TypeCUserDataValid &= *(int*)data == 42;
    }
}

static void OnEvent(ExtensionParams* params, dmhash_t event_id, const void* data, uint32_t data_size)
{
    CheckUserData(ExtensionParamsGetUserData(params));
    ++g_TypeCEventCount;
    if (event_id == ENGINE_STATE_EVENT && data && data_size == sizeof(EngineStateEvent))
    {
        const EngineStateEvent* event = (const EngineStateEvent*)data;
        g_TypeCEventEngine = event->m_Engine;
        g_TypeCEventState = event->m_State;
        g_TypeCEventWindow = 0;
        if (event->m_State == ENGINE_STATE_DELETE)
        {
            ++g_TypeCDeleteCount;
        }
    }
    if (event_id == ENGINE_EVENT_WINDOW_RESIZED)
    {
        if (data && data_size == sizeof(EngineWindowResizedEvent))
        {
            const EngineWindowResizedEvent* resized = (const EngineWindowResizedEvent*)data;
            g_TypeCEventEngine = resized->m_Engine;
            g_TypeCEventWindow = resized->m_Window;
            ++g_TypeCWindowCount;
            g_TypeCWindowWidth = resized->m_Width;
            g_TypeCWindowHeight = resized->m_Height;
        }
    }
}

static ExtensionResult Update(ExtensionParams* params)
{
    CheckUserData(ExtensionParamsGetUserData(params));
    ++g_TypeCUpdateCount;
    return ExtensionParamsGetFloat(params, EXTENSION_PARAM_DT, &g_TypeCUpdateDT) ? EXTENSION_RESULT_OK : EXTENSION_RESULT_INIT_ERROR;
}

static ExtensionResult Register(HExtension extension)
{
    ++g_TypeCRegisterCount;
    g_TypeCUserDataValid &= ExtensionGetUserData(extension) == 0;
    g_UserData = (int*)malloc(sizeof(int));
    if (!g_UserData)
    {
        return EXTENSION_RESULT_INIT_ERROR;
    }
    *g_UserData = 42;
    ExtensionSetUserData(extension, g_UserData);
    ExtensionSetUserData(extension, 0);
    g_TypeCUserDataValid &= ExtensionGetUserData(extension) == 0;
    ExtensionSetUserData(extension, g_UserData);
    CheckUserData(ExtensionGetUserData(extension));
    ExtensionSetUpdateFn(extension, Update);
    ExtensionSetEventFn(extension, OnEvent);
    return EXTENSION_RESULT_OK;
}

static ExtensionResult Unregister(HExtension extension)
{
    CheckUserData(ExtensionGetUserData(extension));
    free(ExtensionGetUserData(extension));
    g_UserData = 0;
    ++g_TypeCUnregisterCount;
    return EXTENSION_RESULT_OK;
}

DM_DECLARE_EXTENSION(TestTypeC, Register, Unregister);
