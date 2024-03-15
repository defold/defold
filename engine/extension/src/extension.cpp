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

#include <dlib/log.h>
#include <dlib/static_assert.h>
#include "extension.h"

#if defined(__cplusplus)
extern "C" {
#endif

void dmExtensionAppParams_Init(dmExtensionAppParams* params)
{
    memset(params, 0, sizeof(*params));
}

void dmExtensionParams_Init(dmExtensionParams* params)
{
    memset(params, 0, sizeof(*params));
}

void dmExtensionRegister(struct dmExtensionDesc* desc,
    uint32_t desc_size,
    const char *name,
    FExtensionAppInit       app_init,
    FExtensionAppFinalize   app_finalize,
    FExtensionInitialize    initialize,
    FExtensionFinalize      finalize,
    FExtensionUpdate        update,
    FExtensionOnEvent       on_event)
{
    DM_STATIC_ASSERT(dmExtension::m_ExtensionDescBufferSize >= sizeof(dmExtensionDesc), Invalid_Struct_Size);

    DM_STATIC_ASSERT(sizeof(dmExtensionDesc) == sizeof(dmExtension::Desc), Invalid_Struct_Size);
    DM_STATIC_ASSERT(offsetof(dmExtensionDesc, m_Next) == offsetof(dmExtension::Desc, m_Next), Misaligned_Structs);
    DM_STATIC_ASSERT(offsetof(dmExtensionDesc, m_Name) == offsetof(dmExtension::Desc, m_Name), Misaligned_Structs);
    DM_STATIC_ASSERT(offsetof(dmExtensionDesc, AppInitialize) == offsetof(dmExtension::Desc, AppInitialize), Misaligned_Structs);
    DM_STATIC_ASSERT(offsetof(dmExtensionDesc, AppFinalize) == offsetof(dmExtension::Desc, AppFinalize), Misaligned_Structs);
    DM_STATIC_ASSERT(offsetof(dmExtensionDesc, Initialize) == offsetof(dmExtension::Desc, Initialize), Misaligned_Structs);
    DM_STATIC_ASSERT(offsetof(dmExtensionDesc, Finalize) == offsetof(dmExtension::Desc, Finalize), Misaligned_Structs);
    DM_STATIC_ASSERT(offsetof(dmExtensionDesc, Update) == offsetof(dmExtension::Desc, Update), Misaligned_Structs);
    DM_STATIC_ASSERT(offsetof(dmExtensionDesc, OnEvent) == offsetof(dmExtension::Desc, OnEvent), Misaligned_Structs);
    DM_STATIC_ASSERT(offsetof(dmExtensionDesc, PreRender) == offsetof(dmExtension::Desc, PreRender), Misaligned_Structs);
    DM_STATIC_ASSERT(offsetof(dmExtensionDesc, PostRender) == offsetof(dmExtension::Desc, PostRender), Misaligned_Structs);
    DM_STATIC_ASSERT(offsetof(dmExtensionDesc, m_AppInitialized) == offsetof(dmExtension::Desc, m_AppInitialized), Misaligned_Structs);

    DM_STATIC_ASSERT(offsetof(dmExtensionEvent, m_Event) == offsetof(dmExtension::Event, m_Event), Misaligned_Structs);

    DM_STATIC_ASSERT((int)dmExtension::EVENT_ID_ACTIVATEAPP == (int)DM_EXTENSION_EVENT_ID_ACTIVATEAPP, Enum_Mismatch);
    DM_STATIC_ASSERT((int)dmExtension::EVENT_ID_DEACTIVATEAPP == (int)DM_EXTENSION_EVENT_ID_DEACTIVATEAPP, Enum_Mismatch);
    DM_STATIC_ASSERT((int)dmExtension::EVENT_ID_ICONIFYAPP == (int)DM_EXTENSION_EVENT_ID_ICONIFYAPP, Enum_Mismatch);
    DM_STATIC_ASSERT((int)dmExtension::EVENT_ID_DEICONIFYAPP == (int)DM_EXTENSION_EVENT_ID_DEICONIFYAPP, Enum_Mismatch);

    dmExtension::Register(
            (dmExtension::Desc*)desc,
            desc_size,
            name,
            (dmExtension::Result (*)(dmExtension::AppParams*))app_init,
            (dmExtension::Result (*)(dmExtension::AppParams*))app_finalize,
            (dmExtension::Result (*)(dmExtension::Params*))initialize,
            (dmExtension::Result (*)(dmExtension::Params*))finalize,
            (dmExtension::Result (*)(dmExtension::Params*))update,
            (void (*)(dmExtension::Params*, const dmExtension::Event*))on_event);
}

int dmExtensionRegisterCallback(dmExtensionCallbackType callback_type, dmExtensionFCallback func)
{
    return (int)dmExtension::RegisterCallback((dmExtension::CallbackType)callback_type, (dmExtension::extension_callback_t)func);
}

#if defined(__cplusplus)
} // extern "C"
#endif

namespace dmExtension
{
    dmExtension::Desc* g_FirstExtension = 0;
    dmExtension::Desc* g_CurrentExtension = 0;


    void Register(struct Desc* desc,
        uint32_t desc_size,
        const char *name,
        Result (*app_init)(AppParams*),
        Result (*app_finalize)(AppParams*),
        Result (*initialize)(Params*),
        Result (*finalize)(Params*),
        Result (*update)(Params*),
        void   (*on_event)(Params*, const Event*))
    {
        DM_STATIC_ASSERT(dmExtension::m_ExtensionDescBufferSize >= sizeof(struct Desc), Invalid_Struct_Size);
        desc->m_Name = name;
        desc->AppInitialize = app_init;
        desc->AppFinalize = app_finalize;
        desc->Initialize = initialize;
        desc->Finalize = finalize;
        desc->Update = update;
        desc->OnEvent = on_event;
        desc->m_Next = g_FirstExtension;

        desc->PreRender = 0x0;
        desc->PostRender = 0x0;
        g_FirstExtension = desc;
    }

    const Desc* GetFirstExtension()
    {
        return g_FirstExtension;
    }

    Result AppInitialize(AppParams* params)
    {
        dmExtension::Desc* ed = (dmExtension::Desc*) dmExtension::GetFirstExtension();
        uint32_t i = 0;
        Result ret = RESULT_OK;
        while (ed) {
            if (ed->AppInitialize) {
                g_CurrentExtension = ed;
                dmExtension::Result r = (dmExtension::Result)ed->AppInitialize(params);
                if (r != dmExtension::RESULT_OK) {
                    dmLogError("Failed to initialize (app-level) extension: %s", ed->m_Name);
                    ret = r;
                    break;
                } else {
                    ed->m_AppInitialized = true;
                }
            }
            ++i;
            ed = (dmExtension::Desc*) ed->m_Next;
        }

        g_CurrentExtension = 0x0;

        if (ret != RESULT_OK) {
            ed = (dmExtension::Desc*) dmExtension::GetFirstExtension();
            uint32_t i = 0;
            uint32_t n = i;
            while (ed && i < n) {
                if (ed->AppFinalize) {
                    dmExtension::Result r = (dmExtension::Result)ed->AppFinalize(params);
                    if (r != dmExtension::RESULT_OK) {
                        dmLogError("Failed to initialize (app-level) extension: %s", ed->m_Name);
                    }
                }
                ++i;
                ed = (dmExtension::Desc*) ed->m_Next;
            }
        }

        return ret;
    }

    void PreRender(Params* params)
    {
        dmExtension::Desc* ed = (dmExtension::Desc*) dmExtension::GetFirstExtension();
        while (ed) {
            if (ed->PreRender && ed->m_AppInitialized) {
                ed->PreRender(params);
            }
            ed = (dmExtension::Desc*) ed->m_Next;
        }
    }

    void PostRender(Params* params)
    {
        dmExtension::Desc* ed = (dmExtension::Desc*) dmExtension::GetFirstExtension();
        while (ed) {
            if (ed->PostRender && ed->m_AppInitialized) {
                ed->PostRender(params);
            }
            ed = (dmExtension::Desc*) ed->m_Next;
        }
    }

    bool RegisterCallback(CallbackType callback_type, extension_callback_t func)
    {
        if (!g_CurrentExtension) {
            dmLogError("Cannot call dmExtension::RegisterCallback outside of AppInitialize");
            return false;
        }

        switch (callback_type)
        {
            case dmExtension::CALLBACK_PRE_RENDER:
                g_CurrentExtension->PreRender = func;
                break;
            case dmExtension::CALLBACK_POST_RENDER:
                g_CurrentExtension->PostRender = func;
                break;
            default:
                return false;
        }

        return true;
    }

    Result AppFinalize(AppParams* params)
    {
        dmExtension::Desc* ed = (dmExtension::Desc*) dmExtension::GetFirstExtension();
        while (ed) {
            if (ed->AppFinalize && ed->m_AppInitialized) {
                ed->m_AppInitialized = false;
                dmExtension::Result r = (dmExtension::Result)ed->AppFinalize(params);
                if (r != dmExtension::RESULT_OK) {
                    dmLogError("Failed to finalize (app-level) extension: %s", ed->m_Name);
                }
            }
            ed = (dmExtension::Desc*) ed->m_Next;
        }

        return RESULT_OK;
    }


    void DispatchEvent(Params* params, const Event* event)
    {
        dmExtension::Desc* ed = (dmExtension::Desc*) dmExtension::GetFirstExtension();
        while (ed) {
            if (ed->OnEvent && ed->m_AppInitialized) {
                ed->OnEvent(params, event);
            }
            ed = (dmExtension::Desc*) ed->m_Next;
        }
    }

}
