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

#include <dlib/dstrings.h>
#include <dlib/log.h>
#include <dlib/hashtable.h>
#include <dlib/static_assert.h>

#include <dmsdk/dlib/profile.h>
#include <dmsdk/extension/extension.h>
#include "extension.hpp"

struct ExtensionParamsImpl
{
    dmHashTable64<void*> m_Contexts;
};

static void EnsureSize(dmHashTable64<void*>* tbl)
{
    if (tbl->Full())
    {
        tbl->OffsetCapacity(4);
    }
}

static int SetContext(dmHashTable64<void*>* contexts, dmhash_t name_hash, void* context)
{
    assert(contexts);
    EnsureSize(contexts);

    if (context)
        contexts->Put(name_hash, context);
    else
    {
        void** pvalue = contexts->Get(name_hash);
        if (pvalue)
            contexts->Erase(name_hash);
    }
    return 0;
}

static int SetContext(dmHashTable64<void*>* contexts, const char* name, void* context)
{
    return SetContext(contexts, dmHashString64(name), context);
}

static void* GetContext(dmHashTable64<void*>* contexts, dmhash_t name_hash)
{
    void** pcontext = contexts->Get(name_hash);
    if (pcontext != 0)
    {
        return *pcontext;
    }
    return 0;
}


#if defined(__cplusplus)
extern "C" {
#endif

typedef struct ExtensionDesc
{
    const struct ExtensionDesc* m_Next;
    char                        m_Name[16];
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
    dmStrlCpy(desc->m_Name, name, sizeof(desc->m_Name));
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

void ExtensionAppParamsInitialize(ExtensionAppParams* app_params)
{
    memset(app_params, 0, sizeof(*app_params));
    app_params->m_Impl = new ExtensionParamsImpl;
    memset(app_params->m_Impl, 0, sizeof(*app_params->m_Impl));
}

void ExtensionAppParamsFinalize(ExtensionAppParams* app_params)
{
    delete app_params->m_Impl;
    app_params->m_Impl = 0;
}

void ExtensionParamsInitialize(ExtensionParams* params)
{
    memset(params, 0, sizeof(*params));
    params->m_Impl = new ExtensionParamsImpl;
    memset(params->m_Impl, 0, sizeof(*params->m_Impl));
}

void ExtensionParamsFinalize(ExtensionParams* params)
{
    delete params->m_Impl;
    params->m_Impl = 0;
}

int ExtensionAppParamsSetContext(ExtensionAppParams* params, const char* name, void* context)
{
    return SetContext(&params->m_Impl->m_Contexts, name, context);
}

void* ExtensionAppParamsGetContext(ExtensionAppParams* params, dmhash_t name_hash)
{
    return GetContext(&params->m_Impl->m_Contexts, name_hash);
}

void* ExtensionAppParamsGetContextByName(ExtensionAppParams* params, const char* name)
{
    return GetContext(&params->m_Impl->m_Contexts, dmHashString64(name));
}

ExtensionAppExitCode ExtensionAppParamsGetAppExitCode(ExtensionAppParams* app_params)
{
    return app_params->m_ExitStatus;
}

int ExtensionParamsSetContext(ExtensionParams* params, const char* name, void* context)
{
    return SetContext(&params->m_Impl->m_Contexts, name, context);
}

void* ExtensionParamsGetContext(ExtensionParams* params, dmhash_t name_hash)
{
    return GetContext(&params->m_Impl->m_Contexts, name_hash);
}

void* ExtensionParamsGetContextByName(ExtensionParams* params, const char* name)
{
    return GetContext(&params->m_Impl->m_Contexts, dmHashString64(name));
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
                DM_PROFILE_DYN(ed->m_Name, 0);
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

    AppExitCode AppParamsGetAppExitCode(AppParams * app_params)
    {
        return (AppExitCode)ExtensionAppParamsGetAppExitCode(app_params);
    }

}
