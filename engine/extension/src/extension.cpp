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
#include <dmsdk/extension/extension.h>
#include "extension.h"

#if defined(__cplusplus)
extern "C" {
#endif

typedef struct ExtensionDesc
{
    const struct ExtensionDesc* m_Next;
    const char*                 m_Name;
    FExtensionAppInitialize     m_AppInitialize;
    FExtensionAppFinalize       m_AppFinalize;
    FExtensionInitialize        m_Initialize;
    FExtensionFinalize          m_Finalize;
    FExtensionUpdate            m_Update;
    FExtensionOnEvent           m_OnEvent;
    FExtensionCallback          m_PreRender;
    FExtensionCallback          m_PostRender;
    uint8_t                     m_AppInitialized:1;
    uint8_t                     m_Initialized:1;
} ExtensionDesc;

static ExtensionDesc* g_FirstExtension = 0;
static const ExtensionDesc* g_CurrentExtension = 0;

void ExtensionRegister(void* _desc,
    uint32_t desc_size,
    const char *name,
    FExtensionAppInitialize app_init,
    FExtensionAppFinalize   app_finalize,
    FExtensionInitialize    initialize,
    FExtensionFinalize      finalize,
    FExtensionUpdate        update,
    FExtensionOnEvent       on_event)
{
    DM_STATIC_ASSERT(ExtensionDescBufferSize >= sizeof(ExtensionDesc), Invalid_Struct_Size);

    ExtensionDesc* desc = (ExtensionDesc*)_desc;
    memset(desc, 0, sizeof(ExtensionDesc));
    desc->m_Name = name;
    desc->m_AppInitialize = app_init;
    desc->m_AppFinalize = app_finalize;
    desc->m_Initialize = initialize;
    desc->m_Finalize = finalize;
    desc->m_Update = update;
    desc->m_OnEvent = on_event;
    desc->m_Next = g_FirstExtension;

    const ExtensionDesc* first = g_FirstExtension;
    while(first) {
        if (strcmp(name, first->m_Name) == 0)
        {
            dmLogError("Extension %s is already registered!", name);
            return;
        }
        first = first->m_Next;
    }

    g_FirstExtension = desc;
}

bool ExtensionRegisterCallback(ExtensionCallbackType callback_type, FExtensionCallback func)
{
    return dmExtension::RegisterCallback((dmExtension::CallbackType)callback_type, (dmExtension::FCallback)func);
}

#if defined(__cplusplus)
} // extern "C"
#endif

namespace dmExtension
{
    HExtension GetFirstExtension()
    {
        return g_FirstExtension;
    }

    HExtension GetNextExtension(HExtension extension)
    {
        return extension->m_Next;
    }

    Result AppInitialize(AppParams* params)
    {
        const ExtensionDesc* ed = dmExtension::GetFirstExtension();
        Result ret = RESULT_OK;
        while (ed) {
            if (ed->m_AppInitialize) {
                g_CurrentExtension = ed;
                ExtensionResult r = ed->m_AppInitialize(params);
                if (r != EXTENSION_RESULT_OK) {
                    dmLogError("Failed to initialize (app-level) extension: %s", ed->m_Name);
                    ret = (dmExtension::Result)r;
                    break;
                } else {
                    ((ExtensionDesc*)ed)->m_AppInitialized = true;
                }
            }
            ed = dmExtension::GetNextExtension(ed);
        }

        g_CurrentExtension = 0x0;

        if (ret != RESULT_OK) {
            ed = dmExtension::GetFirstExtension();
            uint32_t i = 0;
            uint32_t n = i;
            while (ed && i < n) {
                if (ed->m_AppFinalize) {
                    ExtensionResult r = ed->m_AppFinalize(params);
                    if (r != EXTENSION_RESULT_OK) {
                        dmLogError("Failed to initialize (app-level) extension: %s", ed->m_Name);
                    }
                }
                ++i;
                ed = dmExtension::GetNextExtension(ed);
            }
        }

        return ret;
    }

    void PreRender(Params* params)
    {
        const ExtensionDesc* ed = dmExtension::GetFirstExtension();
        while (ed) {
            if (ed->m_PreRender && ed->m_AppInitialized) {
                ed->m_PreRender(params);
            }
            ed = dmExtension::GetNextExtension(ed);
        }
    }

    void PostRender(Params* params)
    {
        const ExtensionDesc* ed = dmExtension::GetFirstExtension();
        while (ed) {
            if (ed->m_PostRender && ed->m_AppInitialized) {
                ed->m_PostRender(params);
            }
            ed = dmExtension::GetNextExtension(ed);
        }
    }

    bool RegisterCallback(CallbackType callback_type, FExtensionCallback func)
    {
        if (!g_CurrentExtension) {
            dmLogError("Cannot call dmExtension::RegisterCallback outside of AppInitialize");
            return false;
        }

        switch (callback_type)
        {
            case dmExtension::CALLBACK_PRE_RENDER:
                ((ExtensionDesc*)g_CurrentExtension)->m_PreRender = func;
                break;
            case dmExtension::CALLBACK_POST_RENDER:
                ((ExtensionDesc*)g_CurrentExtension)->m_PostRender = func;
                break;
            default:
                return false;
        }

        return true;
    }

    Result Initialize(Params* params)
    {
        const ExtensionDesc* ed = dmExtension::GetFirstExtension();
        while (ed) {
            if (ed->m_Initialize)
            {
                ExtensionResult r = ed->m_Initialize(params);
                if (r == EXTENSION_RESULT_OK) {
                    ((ExtensionDesc*)ed)->m_Initialized = true;
                } else {
                    dmLogError("Failed to initialize extension: %s", ed->m_Name);
                }
            }
            ed = ed->m_Next;
        }
        return RESULT_OK;
    }

    Result Finalize(Params* params)
    {
        const ExtensionDesc* ed = dmExtension::GetFirstExtension();
        while (ed) {
            if (ed->m_Finalize && ed->m_Initialized) {
                ((ExtensionDesc*)ed)->m_Initialized = false;
                ExtensionResult r = ed->m_Finalize(params);
                if (r != EXTENSION_RESULT_OK) {
                    dmLogError("Failed to finalize extension: %s", ed->m_Name);
                }
            }
            ed = dmExtension::GetNextExtension(ed);
        }
        return RESULT_OK;
    }

    Result Update(Params* params)
    {
        const ExtensionDesc* ed = dmExtension::GetFirstExtension();
        while (ed) {
            if (ed->m_Update && ed->m_Initialized)
            {
                ExtensionResult r = ed->m_Update(params);
                if (r != EXTENSION_RESULT_OK) {
                    dmLogError("Failed to update extension: %s", ed->m_Name);
                }
            }
            ed = dmExtension::GetNextExtension(ed);
        }
        return RESULT_OK;
    }

    Result AppFinalize(AppParams* params)
    {
        const ExtensionDesc* ed = dmExtension::GetFirstExtension();
        while (ed) {
            if (ed->m_AppFinalize && ed->m_AppInitialized) {
                ((ExtensionDesc*)ed)->m_AppInitialized = false;
                ExtensionResult r = ed->m_AppFinalize(params);
                if (r != EXTENSION_RESULT_OK) {
                    dmLogError("Failed to finalize (app-level) extension: %s", ed->m_Name);
                }
            }
            ed = dmExtension::GetNextExtension(ed);
        }
        return RESULT_OK;
    }

    void DispatchEvent(Params* params, const Event* event)
    {
        const ExtensionDesc* ed = dmExtension::GetFirstExtension();
        while (ed) {
            if (ed->m_OnEvent && ed->m_AppInitialized) {
                ed->m_OnEvent(params, event);
            }
            ed = dmExtension::GetNextExtension(ed);
        }
    }

}
