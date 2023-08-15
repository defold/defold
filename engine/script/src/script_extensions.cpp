// Copyright 2020-2023 The Defold Foundation
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

#include "script_extensions.h"

#include "script.h"

#include <extension/extension.h>

extern "C"
{
#include <lua/lualib.h>
}

namespace dmScript
{
    #define DM_SCRIPT_MAX_EXTENSIONS (sizeof(uint32_t) * 8 * 16)
    #define BIT_INDEX(b) ((b) / sizeof(uint32_t))
    #define BIT_OFFSET(b) ((b) % sizeof(uint32_t))

    const char* EXTENSIONS_CONTEXT_NAME = "__extensions_context__";

    struct ExtensionsData
    {
        HContext    m_Context;
        uint32_t    m_InitializedExtensions[DM_SCRIPT_MAX_EXTENSIONS / (8 * sizeof(uint32_t))];
        int         m_Ref;
    };

    static void InternalInitializeExtensions(HContext context)
    {
        lua_State* L = GetLuaState(context);
        DM_LUA_STACK_CHECK(L, 0);

        ExtensionsData* extension_data = (ExtensionsData*)lua_newuserdata(L, sizeof(ExtensionsData));
        memset(extension_data->m_InitializedExtensions, 0, sizeof(extension_data->m_InitializedExtensions));
        extension_data->m_Ref = LUA_NOREF;
        extension_data->m_Context = context;

        // [-1] extension_data
        lua_pushvalue(L, -1);
        // [-2] extension_data
        // [-1] extension_data
        extension_data->m_Ref = Ref(L, LUA_REGISTRYINDEX);
        // [-1] extension_data
        lua_pushstring(L, EXTENSIONS_CONTEXT_NAME);
        // [-2] extension_data
        // [-1] EXTENSIONS_CONTEXT_NAME

        lua_insert(L, -2);
        // [-2] EXTENSIONS_CONTEXT_NAME
        // [-1] extension_data
        
        SetContextValue(extension_data->m_Context);

        const dmExtension::Desc* ed = dmExtension::GetFirstExtension();
        uint32_t i = 0;
        while (ed) {
            if (ed->Initialize)
            {
                dmExtension::Params p;
                p.m_ConfigFile = GetConfigFile(context);
                p.m_ResourceFactory = GetResourceFactory(context);
                p.m_L = L;
                dmExtension::Result r = ed->Initialize(&p);
                if (r == dmExtension::RESULT_OK) {
                    extension_data->m_InitializedExtensions[BIT_INDEX(i)] |= 1 << BIT_OFFSET(i);
                } else {
                    dmLogError("Failed to initialize extension: %s", ed->m_Name);
                }
            }
            ++i;
            ed = ed->m_Next;
        }
    }

    static ExtensionsData* GetExtensionData(HContext context)
    {
        lua_State* L = GetLuaState(context);
        DM_LUA_STACK_CHECK(L, 0);
        lua_pushstring(L, EXTENSIONS_CONTEXT_NAME);
        GetContextValue(context);

        ExtensionsData* extension_data = (ExtensionsData*)lua_touserdata(L, -1);
        lua_pop(L, 1);
        return extension_data;
    }

    static void InternalUpdateExtensions(HContext context)
    {
        lua_State* L = GetLuaState(context);
        DM_LUA_STACK_CHECK(L, 0);

        ExtensionsData* extension_data = GetExtensionData(context);
        if (extension_data == 0x0)
        {
            return;
        }

        const dmExtension::Desc* ed = dmExtension::GetFirstExtension();
        uint32_t i = 0;
        while (ed) {
            if (ed->Update)
            {
                dmExtension::Params p;
                p.m_ConfigFile = GetConfigFile(context);
                p.m_ResourceFactory = GetResourceFactory(context);
                p.m_L = L;
                if (extension_data->m_InitializedExtensions[BIT_INDEX(i)] & (1 << BIT_OFFSET(i))) {
                    dmExtension::Result r = ed->Update(&p);
                    if (r != dmExtension::RESULT_OK) {
                        dmLogError("Failed to update extension: %s", ed->m_Name);
                    }
                }
            }
            ++i;
            ed = ed->m_Next;
        }
    }

    static void InternalFinalizeExtensions(HContext context)
    {
        lua_State* L = GetLuaState(context);
        DM_LUA_STACK_CHECK(L, 0);

        ExtensionsData* extension_data = GetExtensionData(context);
        if (extension_data == 0x0)
        {
            return;
        }
        const dmExtension::Desc* ed = dmExtension::GetFirstExtension();
        uint32_t i = 0;
        while (ed) {
            if (ed->Finalize)
            {
                dmExtension::Params p;
                p.m_ConfigFile = GetConfigFile(context);
                p.m_ResourceFactory = GetResourceFactory(context);
                p.m_L = L;
                if (extension_data->m_InitializedExtensions[BIT_INDEX(i)] & (1 << BIT_OFFSET(i))) {
                    dmExtension::Result r = ed->Finalize(&p);
                    if (r != dmExtension::RESULT_OK) {
                        dmLogError("Failed to finalize extension: %s", ed->m_Name);
                    }
                }
            }
            ++i;
            ed = ed->m_Next;
        }
        Unref(L, LUA_REGISTRYINDEX, extension_data->m_Ref);
        extension_data->m_Ref = LUA_NOREF;
        memset(extension_data->m_InitializedExtensions, 0, sizeof(extension_data->m_InitializedExtensions));
    }

    void InitializeExtensions(HContext context)
    {
        static ScriptExtension sl;
        sl.Initialize = InternalInitializeExtensions;
        sl.Update = InternalUpdateExtensions;
        sl.Finalize = InternalFinalizeExtensions;
        sl.NewScriptWorld = 0x0;
        sl.DeleteScriptWorld = 0x0;
        sl.UpdateScriptWorld = 0x0;
        sl.FixedUpdateScriptWorld = 0x0;
        sl.InitializeScriptInstance = 0x0;
        sl.FinalizeScriptInstance = 0x0;
        RegisterScriptExtension(context, &sl);
    }
}
