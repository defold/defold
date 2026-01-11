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

#include <string.h>
#include <script/script.h>

#include <dmsdk/resource/resource.h>
#include "res_lua.h"
#include "../proto/gameobject/lua_ddf.h"
#include "gameobject_script.h"
#include "gameobject_script_util.h"

namespace dmGameObject
{
    static dmResource::Result ResScriptPreload(const dmResource::ResourcePreloadParams* params)
    {
        dmLuaDDF::LuaModule* lua_module = 0;
        dmDDF::Result e = dmDDF::LoadMessage<dmLuaDDF::LuaModule>(params->m_Buffer, params->m_BufferSize, &lua_module);
        if ( e != dmDDF::RESULT_OK )
            return dmResource::RESULT_FORMAT_ERROR;

        PatchLuaBytecode(&lua_module->m_Source);

        uint32_t n_modules = lua_module->m_Modules.m_Count;
        for (uint32_t i = 0; i < n_modules; ++i)
        {
            dmResource::PreloadHint(params->m_HintInfo, lua_module->m_Resources[i]);
        }

        const char** resources = lua_module->m_PropertyResources.m_Data;
        uint32_t n_resources = lua_module->m_PropertyResources.m_Count;
        for (uint32_t i = 0; i < n_resources; ++i)
        {
            dmResource::PreloadHint(params->m_HintInfo, resources[i]);
        }

        *params->m_PreloadData = lua_module;
        return dmResource::RESULT_OK;
    }

    static dmResource::Result ResScriptCreate(const dmResource::ResourceCreateParams* params)
    {
        dmLuaDDF::LuaModule* lua_module = (dmLuaDDF::LuaModule*) params->m_PreloadData;

        dmScript::HContext script_context = (dmScript::HContext) params->m_Context;
        lua_State* L = dmScript::GetLuaState(script_context);

        if (!RegisterSubModules(params->m_Factory, script_context, lua_module))
        {
            dmDDF::FreeMessage(lua_module);
            return dmResource::RESULT_FORMAT_ERROR;
        }

        HScript script = NewScript(L, lua_module);
        if (script)
        {
            dmResource::Result res = LoadPropertyResources(params->m_Factory, lua_module->m_PropertyResources.m_Data, lua_module->m_PropertyResources.m_Count, script->m_PropertyResources);
            if(res != dmResource::RESULT_OK)
            {
                DeleteScript(script);
                return res;
            }

            ResourceDescriptorSetResource(params->m_Resource, script);
            ResourceDescriptorSetResourceSize(params->m_Resource, params->m_BufferSize - script->m_LuaModule->m_Source.m_Script.m_Count);
            return dmResource::RESULT_OK;
        }
        else
        {
            dmDDF::FreeMessage(lua_module);
            return dmResource::RESULT_FORMAT_ERROR;
        }
    }

    static dmResource::Result ResScriptDestroy(const dmResource::ResourceDestroyParams* params)
    {
        HScript script = (HScript) ResourceDescriptorGetResource(params->m_Resource);
        UnloadPropertyResources(params->m_Factory, script->m_PropertyResources);
        dmDDF::FreeMessage(script->m_LuaModule);
        DeleteScript((HScript) script);
        return dmResource::RESULT_OK;
    }

    static dmResource::Result ResScriptRecreate(const dmResource::ResourceRecreateParams* params)
    {
        HScript script = (HScript) ResourceDescriptorGetResource(params->m_Resource);

        dmLuaDDF::LuaModule* lua_module = 0;
        dmDDF::Result e = dmDDF::LoadMessage<dmLuaDDF::LuaModule>(params->m_Buffer, params->m_BufferSize, &lua_module);
        if ( e != dmDDF::RESULT_OK )
            return dmResource::RESULT_FORMAT_ERROR;

        PatchLuaBytecode(&lua_module->m_Source);

        dmScript::HContext script_context = (dmScript::HContext) params->m_Context;
        if (!RegisterSubModules(params->m_Factory, script_context, lua_module))
        {
            dmDDF::FreeMessage(lua_module);
            return dmResource::RESULT_FORMAT_ERROR;
        }

        dmLuaDDF::LuaModule* old_lua_module = script->m_LuaModule;
        bool ok = ReloadScript(script, lua_module);
        if (ok)
        {
            dmArray<void*> tmp_res;
            dmResource::Result res = LoadPropertyResources(params->m_Factory, lua_module->m_PropertyResources.m_Data, lua_module->m_PropertyResources.m_Count, tmp_res);
            if(res == dmResource::RESULT_OK)
            {
                UnloadPropertyResources(params->m_Factory, script->m_PropertyResources);
                script->m_PropertyResources.Swap(tmp_res);
            }
            dmDDF::FreeMessage(old_lua_module);
            ResourceDescriptorSetResourceSize(params->m_Resource, params->m_BufferSize - script->m_LuaModule->m_Source.m_Script.m_Count);
            return dmResource::RESULT_OK;
        }
        else
        {
            dmDDF::FreeMessage(lua_module);
            return dmResource::RESULT_FORMAT_ERROR;
        }
    }

    static ResourceResult RegisterResourceTypeScript(HResourceTypeContext ctx, HResourceType type)
    {
        // The engine.cpp creates the contexts for some of our our built in types (i.e. same context for some types)
        void* context = ResourceTypeContextGetContextByHash(ctx, ResourceTypeGetNameHash(type));
        assert(context);
        return (ResourceResult)dmResource::SetupType(ctx,
                                                    type,
                                                    context,
                                                    ResScriptPreload,
                                                    ResScriptCreate,
                                                    0,
                                                    ResScriptDestroy,
                                                    ResScriptRecreate);
    }
}

DM_DECLARE_RESOURCE_TYPE(ResourceTypeScript, "scriptc", dmGameObject::RegisterResourceTypeScript, 0);
