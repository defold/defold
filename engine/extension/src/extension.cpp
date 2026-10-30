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

#include <dlib/context_registry.h>
#include <dlib/array.h>
#include <dlib/dstrings.h>
#include <dlib/log.h>
#include <dlib/static_assert.h>

#include <dmsdk/dlib/profile.h>
#include <dmsdk/extension/extension.h>
#include <dmsdk/engine/engine_event.h>
#include "extension.hpp"

struct ExtensionParamsImpl
{
    HContextRegistry m_ContextRegistry;
    void*            m_UserData;
    float            m_DT;

    ExtensionParamsImpl()
    : m_ContextRegistry(0)
    , m_UserData(0)
    , m_DT(0.0f)
    {
    }
};

#if defined(__cplusplus)
extern "C" {
#endif

typedef struct ExtensionDesc
{
    const struct ExtensionDesc* m_Next;
    // New declarations borrow their full name through m_FullName, leaving the legacy
    // name storage available for user data within the original 128-byte buffer.
    union
    {
        char  m_Name[16];
        void* m_UserData;
    };
    FExtensionAppInitialize     m_AppInitialize;
    FExtensionAppFinalize       m_AppFinalize;
    FExtensionInitialize        m_Initialize;
    FExtensionFinalize          m_Finalize;
    FExtensionUpdate            m_Update;
    FExtensionOnEvent           m_OnEvent;
    FExtensionCallback          m_PreRender;
    FExtensionCallback          m_PostRender;
    FExtensionEventCallback     m_EventCallback;
    FExtensionRegister          m_Register;
    FExtensionUnregister        m_Unregister;
    const char*                 m_FullName;
    uint8_t                     m_AppInitialized:1;
    uint8_t                     m_Initialized:1;
    uint8_t                     m_Registered : 1;
    uint8_t                     m_EventDeprecationLogged : 1;
} ExtensionDesc;

static ExtensionDesc* g_FirstExtension = 0;
static const ExtensionDesc* g_CurrentExtension = 0;

static const char* GetExtensionName(const ExtensionDesc* desc)
{
    return desc->m_FullName ? desc->m_FullName : desc->m_Name;
}

static bool RegisterDescriptor(ExtensionDesc* desc, const char* name)
{
    DM_STATIC_ASSERT(ExtensionDescBufferSize >= sizeof(ExtensionDesc), Invalid_Struct_Size);
    for (const ExtensionDesc* first = g_FirstExtension; first; first = first->m_Next)
    {
        if (first == desc || strcmp(name, GetExtensionName(first)) == 0)
        {
            dmLogError("Extension %s is already registered!", name);
            return false;
        }
    }

    memset(desc, 0, sizeof(*desc));
    dmStrlCpy(desc->m_Name, name, sizeof(desc->m_Name));
    desc->m_Next = g_FirstExtension;
    g_FirstExtension = desc;
    return true;
}

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
    ExtensionDesc* desc = (ExtensionDesc*)_desc;
    if (!RegisterDescriptor(desc, name))
    {
        return;
    }

    desc->m_AppInitialize = app_init;
    desc->m_AppFinalize = app_finalize;
    desc->m_Initialize = initialize;
    desc->m_Finalize = finalize;
    desc->m_Update = update;
    desc->m_OnEvent = on_event;
}

void ExtensionRegisterDesc(void* _desc, uint32_t desc_size, const char* name, FExtensionRegister register_fn, FExtensionUnregister unregister_fn)
{
    ExtensionDesc* desc = (ExtensionDesc*)_desc;
    if (!RegisterDescriptor(desc, name))
    {
        return;
    }
    desc->m_FullName = name;
    desc->m_UserData = 0;
    desc->m_Register = register_fn;
    desc->m_Unregister = unregister_fn;
}

bool ExtensionRegisterCallback(ExtensionCallbackType callback_type, FExtensionCallback func)
{
    return dmExtension::RegisterCallback((dmExtension::CallbackType)callback_type, (dmExtension::FCallback)func);
}

void ExtensionSetUserData(HExtension extension, void* user_data)
{
    extension->m_UserData = user_data;
}

void* ExtensionGetUserData(HExtension extension)
{
    return extension->m_UserData;
}

void ExtensionSetAppInitializeFn(HExtension extension, FExtensionAppInitialize fn)
{
    extension->m_AppInitialize = fn;
}

void ExtensionSetAppFinalizeFn(HExtension extension, FExtensionAppFinalize fn)
{
    extension->m_AppFinalize = fn;
}

void ExtensionSetInitializeFn(HExtension extension, FExtensionInitialize fn)
{
    extension->m_Initialize = fn;
}

void ExtensionSetFinalizeFn(HExtension extension, FExtensionFinalize fn)
{
    extension->m_Finalize = fn;
}

void ExtensionSetUpdateFn(HExtension extension, FExtensionUpdate fn)
{
    extension->m_Update = fn;
}

void ExtensionSetPreRenderFn(HExtension extension, FExtensionCallback fn)
{
    extension->m_PreRender = fn;
}

void ExtensionSetPostRenderFn(HExtension extension, FExtensionCallback fn)
{
    extension->m_PostRender = fn;
}

void ExtensionSetEventFn(HExtension extension, FExtensionEventCallback fn)
{
    extension->m_EventCallback = fn;
}

void ExtensionBroadcastEvent(ExtensionParams* params, dmhash_t event_id, const void* data, uint32_t data_size)
{
    dmExtension::BroadcastEvent(params, event_id, data, data_size);
}

void ExtensionAppParamsInitialize(ExtensionAppParams* app_params)
{
    memset(app_params, 0, sizeof(*app_params));
    app_params->m_Impl = new ExtensionParamsImpl;
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
}

void ExtensionParamsFinalize(ExtensionParams* params)
{
    delete params->m_Impl;
    params->m_Impl = 0;
}

void ExtensionAppParamsSetContextRegistry(ExtensionAppParams* params, HContextRegistry context_registry)
{
    params->m_Impl->m_ContextRegistry = context_registry;
}

HContextRegistry ExtensionAppParamsGetContextRegistry(ExtensionAppParams* params)
{
    return params->m_Impl->m_ContextRegistry;
}

void ExtensionParamsSetContextRegistry(ExtensionParams* params, HContextRegistry context_registry)
{
    params->m_Impl->m_ContextRegistry = context_registry;
}

HContextRegistry ExtensionParamsGetContextRegistry(ExtensionParams* params)
{
    return params->m_Impl->m_ContextRegistry;
}

void* ExtensionAppParamsGetUserData(const ExtensionAppParams* params)
{
    return params->m_Impl->m_UserData;
}

void* ExtensionParamsGetUserData(const ExtensionParams* params)
{
    return params->m_Impl->m_UserData;
}

bool ExtensionParamsGetFloat(const ExtensionParams* params, ExtensionParam param, float* out_value)
{
    if (param != EXTENSION_PARAM_DT)
        return false;

    *out_value = params->m_Impl->m_DT;
    return true;
}

int ExtensionAppParamsSetContext(ExtensionAppParams* params, const char* name, void* context)
{
    return ContextRegistrySet(ExtensionAppParamsGetContextRegistry(params), name, context);
}

void* ExtensionAppParamsGetContext(ExtensionAppParams* params, dmhash_t name_hash)
{
    return ContextRegistryGetByHash(ExtensionAppParamsGetContextRegistry(params), name_hash);
}

void* ExtensionAppParamsGetContextByName(ExtensionAppParams* params, const char* name)
{
    return ContextRegistryGet(ExtensionAppParamsGetContextRegistry(params), name);
}

ExtensionAppExitCode ExtensionAppParamsGetAppExitCode(ExtensionAppParams* app_params)
{
    return app_params->m_ExitStatus;
}

int ExtensionParamsSetContext(ExtensionParams* params, const char* name, void* context)
{
    return ContextRegistrySet(ExtensionParamsGetContextRegistry(params), name, context);
}

void* ExtensionParamsGetContext(ExtensionParams* params, dmhash_t name_hash)
{
    return ContextRegistryGetByHash(ExtensionParamsGetContextRegistry(params), name_hash);
}

void* ExtensionParamsGetContextByName(ExtensionParams* params, const char* name)
{
    return ContextRegistryGet(ExtensionParamsGetContextRegistry(params), name);
}



#if defined(__cplusplus)
} // extern "C"
#endif

namespace dmExtension
{
    void AppParamsInitialize(AppParams* params)
    {
        ExtensionAppParamsInitialize(params);
    }

    void AppParamsFinalize(AppParams* params)
    {
        ExtensionAppParamsFinalize(params);
    }

    void ParamsInitialize(Params* params)
    {
        ExtensionParamsInitialize(params);
    }

    void ParamsFinalize(Params* params)
    {
        ExtensionParamsFinalize(params);
    }

    // A parameter struct is reused for different extensions. Restore its previous
    // value so nested delivery using the same parameters preserves its caller.
    struct ScopedUserData
    {
        ExtensionParamsImpl* m_Params;
        void*                m_Previous;

        ScopedUserData(ExtensionParamsImpl* params, const ExtensionDesc* extension)
        : m_Params(params)
        , m_Previous(params->m_UserData)
        {
            params->m_UserData = extension->m_FullName ? extension->m_UserData : 0;
        }

        ~ScopedUserData()
        {
            m_Params->m_UserData = m_Previous;
        }
    };

    void* AppParamsGetUserData(const AppParams* params)
    {
        return ExtensionAppParamsGetUserData(params);
    }

    void* ParamsGetUserData(const Params* params)
    {
        return ExtensionParamsGetUserData(params);
    }

    void SetUserData(HExtension extension, void* user_data)
    {
        ExtensionSetUserData(extension, user_data);
    }

    void* GetUserData(HExtension extension)
    {
        return ExtensionGetUserData(extension);
    }

    bool ParamsGetFloat(const Params* params, Param param, float* out_value)
    {
        return ExtensionParamsGetFloat(params, (ExtensionParam)param, out_value);
    }

    void SetAppInitializeFn(HExtension extension, FExtensionAppInitialize fn)
    {
        ExtensionSetAppInitializeFn(extension, fn);
    }

    void SetAppFinalizeFn(HExtension extension, FExtensionAppFinalize fn)
    {
        ExtensionSetAppFinalizeFn(extension, fn);
    }

    void SetInitializeFn(HExtension extension, FExtensionInitialize fn)
    {
        ExtensionSetInitializeFn(extension, fn);
    }

    void SetFinalizeFn(HExtension extension, FExtensionFinalize fn)
    {
        ExtensionSetFinalizeFn(extension, fn);
    }

    void SetUpdateFn(HExtension extension, FExtensionUpdate fn)
    {
        ExtensionSetUpdateFn(extension, fn);
    }

    void SetPreRenderFn(HExtension extension, FCallback fn)
    {
        ExtensionSetPreRenderFn(extension, fn);
    }

    void SetPostRenderFn(HExtension extension, FCallback fn)
    {
        ExtensionSetPostRenderFn(extension, fn);
    }

    void SetEventFn(HExtension extension, FEventCallback fn)
    {
        ExtensionSetEventFn(extension, fn);
    }

    HExtensionDesc GetFirstExtension()
    {
        return g_FirstExtension;
    }

    HExtensionDesc GetNextExtension(HExtensionDesc extension)
    {
        return extension->m_Next;
    }

    Result AppInitialize(AppParams* params)
    {
        const ExtensionDesc* ed = dmExtension::GetFirstExtension();
        Result ret = RESULT_OK;
        // Configure every extension before any extension starts its application lifecycle.
        while (ed)
        {
            if (ed->m_Register)
            {
                ExtensionDesc* extension = (ExtensionDesc*)ed;
                extension->m_AppInitialize = 0;
                extension->m_AppFinalize = 0;
                extension->m_Initialize = 0;
                extension->m_Finalize = 0;
                extension->m_Update = 0;
                extension->m_PreRender = 0;
                extension->m_PostRender = 0;
                extension->m_EventCallback = 0;
                extension->m_UserData = 0;
                ExtensionResult r = ed->m_Register(extension);
                if (r != EXTENSION_RESULT_OK)
                {
                    extension->m_UserData = 0;
                    dmLogError("Failed to register extension: %s", GetExtensionName(ed));
                    return (Result)r;
                }
                extension->m_Registered = true;
            }
            ed = dmExtension::GetNextExtension(ed);
        }
        ed = dmExtension::GetFirstExtension();
        while (ed) {
            if (ed->m_AppInitialize) {
                ScopedUserData user_data(params->m_Impl, ed);
                g_CurrentExtension = ed;
                ExtensionResult r = ed->m_AppInitialize(params);
                if (r != EXTENSION_RESULT_OK) {
                    dmLogError("Failed to initialize (app-level) extension: %s", GetExtensionName(ed));
                    ret = (dmExtension::Result)r;
                    break;
                } else {
                    ((ExtensionDesc*)ed)->m_AppInitialized = true;
                }
            }
            else if (ed->m_Registered)
            {
                ((ExtensionDesc*)ed)->m_AppInitialized = true;
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
                    ScopedUserData user_data(params->m_Impl, ed);
                    ExtensionResult r = ed->m_AppFinalize(params);
                    if (r != EXTENSION_RESULT_OK) {
                        dmLogError("Failed to initialize (app-level) extension: %s", GetExtensionName(ed));
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
                ScopedUserData user_data(params->m_Impl, ed);
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
                ScopedUserData user_data(params->m_Impl, ed);
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
                ScopedUserData user_data(params->m_Impl, ed);
                ExtensionResult r = ed->m_Initialize(params);
                if (r == EXTENSION_RESULT_OK) {
                    ((ExtensionDesc*)ed)->m_Initialized = true;
                } else {
                    dmLogError("Failed to initialize extension: %s", GetExtensionName(ed));
                }
            }
            else if (ed->m_Registered && ed->m_AppInitialized)
            {
                ((ExtensionDesc*)ed)->m_Initialized = true;
            }
            ed = ed->m_Next;
        }
        return RESULT_OK;
    }

    Result Finalize(Params* params)
    {
        const ExtensionDesc* ed = dmExtension::GetFirstExtension();
        while (ed) {
            bool initialized = ed->m_Initialized;
            ((ExtensionDesc*)ed)->m_Initialized = false;
            if (ed->m_Finalize && initialized)
            {
                ScopedUserData user_data(params->m_Impl, ed);
                ExtensionResult r = ed->m_Finalize(params);
                if (r != EXTENSION_RESULT_OK) {
                    dmLogError("Failed to finalize extension: %s", GetExtensionName(ed));
                }
            }
            ed = dmExtension::GetNextExtension(ed);
        }
        return RESULT_OK;
    }

    Result Update(Params* params, float dt)
    {
        params->m_Impl->m_DT = dt;
        const ExtensionDesc* ed = dmExtension::GetFirstExtension();
        while (ed) {
            if (ed->m_Update && ed->m_Initialized)
            {
                ScopedUserData user_data(params->m_Impl, ed);
                DM_PROFILE_DYN(GetExtensionName(ed), 0);
                ExtensionResult r = ed->m_Update(params);
                if (r != EXTENSION_RESULT_OK) {
                    dmLogError("Failed to update extension: %s", GetExtensionName(ed));
                }
            }
            ed = dmExtension::GetNextExtension(ed);
        }
        params->m_Impl->m_DT = 0.0f;
        return RESULT_OK;
    }

    Result AppFinalize(AppParams* params)
    {
        const ExtensionDesc* ed = dmExtension::GetFirstExtension();
        while (ed) {
            bool initialized = ed->m_AppInitialized;
            ((ExtensionDesc*)ed)->m_AppInitialized = false;
            if (ed->m_AppFinalize && initialized)
            {
                ScopedUserData user_data(params->m_Impl, ed);
                ExtensionResult r = ed->m_AppFinalize(params);
                if (r != EXTENSION_RESULT_OK) {
                    dmLogError("Failed to finalize (app-level) extension: %s", GetExtensionName(ed));
                }
            }
            ed = dmExtension::GetNextExtension(ed);
        }
        // Registration-owned state outlives all application finalizers.
        ed = dmExtension::GetFirstExtension();
        while (ed)
        {
            if (ed->m_Registered)
            {
                ((ExtensionDesc*)ed)->m_Registered = false;
                if (ed->m_Unregister)
                {
                    ExtensionResult r = ed->m_Unregister((HExtension)ed);
                    if (r != EXTENSION_RESULT_OK)
                    {
                        dmLogError("Failed to unregister extension: %s", GetExtensionName(ed));
                    }
                }
                ((ExtensionDesc*)ed)->m_UserData = 0;
            }
            ed = dmExtension::GetNextExtension(ed);
        }
        return RESULT_OK;
    }

    struct EngineEventMapping
    {
        EventID  m_LegacyId;
        dmhash_t m_Id;
    };

    static const EngineEventMapping g_EngineEvents[] = {
        { EVENT_ID_ACTIVATEAPP, dmEngine::EVENT_ACTIVATEAPP },
        { EVENT_ID_DEACTIVATEAPP, dmEngine::EVENT_DEACTIVATEAPP },
        { EVENT_ID_ICONIFYAPP, dmEngine::EVENT_ICONIFYAPP },
        { EVENT_ID_DEICONIFYAPP, dmEngine::EVENT_DEICONIFYAPP },
        { EVENT_ID_ENGINE_INITIALIZED, dmEngine::EVENT_ENGINE_INITIALIZED },
        { EVENT_ID_ENGINE_DELETE, dmEngine::EVENT_ENGINE_DELETE },
    };

    void BroadcastEvent(Params* params, dmhash_t event_id, const void* data, uint32_t data_size)
    {
        float previous_dt = params->m_Impl->m_DT;
        params->m_Impl->m_DT = 0.0f;
        Event legacy_event;
        bool has_legacy_event = false;
        for (uint32_t i = 0; i < DM_ARRAY_SIZE(g_EngineEvents); ++i)
        {
            if (g_EngineEvents[i].m_Id == event_id)
            {
                legacy_event.m_Event = (ExtensionEventID)g_EngineEvents[i].m_LegacyId;
                has_legacy_event = true;
                break;
            }
        }

        const ExtensionDesc* ed = dmExtension::GetFirstExtension();
        while (ed) {
            if (!ed->m_AppInitialized)
            {
                ed = dmExtension::GetNextExtension(ed);
                continue;
            }
            ScopedUserData user_data(params->m_Impl, ed);
            if (ed->m_EventCallback)
            {
                ed->m_EventCallback(params, event_id, data, data_size);
            }

            if (ed->m_OnEvent && has_legacy_event)
            {
                if (!ed->m_EventDeprecationLogged)
                {
                    ((ExtensionDesc*)ed)->m_EventDeprecationLogged = true;
                    dmLogWarning("Extension '%s' uses deprecated FExtensionOnEvent and ExtensionEvent.", GetExtensionName(ed));
                }
                ed->m_OnEvent(params, &legacy_event);
            }
            ed = dmExtension::GetNextExtension(ed);
        }
        params->m_Impl->m_DT = previous_dt;
    }

    AppExitCode AppParamsGetAppExitCode(AppParams * app_params)
    {
        return (AppExitCode)ExtensionAppParamsGetAppExitCode(app_params);
    }

}
