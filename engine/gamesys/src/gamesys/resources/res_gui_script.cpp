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

#include <string.h>
#include <stdlib.h>
//#include <new>
#include "res_gui.h"
//#include <gamesys/gui_ddf.h>
#include "../components/comp_gui_private.h"
#include "gamesys.h"
#include <gameobject/res_lua.h>
#include <gameobject/lua_ddf.h>
#include <gameobject/gameobject_script_util.h>

namespace dmGameSystem
{
    struct ResGuiContexts
    {
        dmGui::HContext     m_GuiContext;
        dmScript::HContext  m_ScriptContext;
    };

    // TODO: We're missing a proper cleanup function as well! (I.e. for the games that rely heavily on "reboot")
    ResGuiContexts* g_ResGuiScriptContext = 0;
    struct ResGuiScriptContextDestructor
    {
        ~ResGuiScriptContextDestructor() {
            if (g_ResGuiScriptContext) {
                free((void*)g_ResGuiScriptContext);
                g_ResGuiScriptContext = 0;
            }
        }
    } g_ResGuiScriptContextDestructor;

    static dmResource::Result ResPreloadGuiScript(const dmResource::ResourcePreloadParams& params)
    {
        dmLuaDDF::LuaModule* lua_module = 0;
        dmDDF::Result e = dmDDF::LoadMessage<dmLuaDDF::LuaModule>(params.m_Buffer, params.m_BufferSize, &lua_module);
        if ( e != dmDDF::RESULT_OK )
            return dmResource::RESULT_FORMAT_ERROR;

        dmGameObject::PatchLuaBytecode(&lua_module->m_Source);

        uint32_t n_modules = lua_module->m_Modules.m_Count;
        for (uint32_t i = 0; i < n_modules; ++i)
        {
            dmResource::PreloadHint(params.m_HintInfo, lua_module->m_Resources[i]);
        }

        *params.m_PreloadData = lua_module;
        return dmResource::RESULT_OK;
    }

    static dmResource::Result ResCreateGuiScript(const dmResource::ResourceCreateParams& params)
    {
        ResGuiContexts* gui_context = (ResGuiContexts*) params.m_Context;
        dmLuaDDF::LuaModule* lua_module = (dmLuaDDF::LuaModule*) params.m_PreloadData;

        if (!dmGameObject::RegisterSubModules(params.m_Factory, gui_context->m_ScriptContext, lua_module))
        {
            dmDDF::FreeMessage(lua_module);
            return dmResource::RESULT_FORMAT_ERROR;
        }

        dmGui::HScript script = dmGui::NewScript(gui_context->m_GuiContext);
        dmGui::Result result = dmGui::SetScript(script, &lua_module->m_Source);
        if (result == dmGui::RESULT_OK)
        {
            params.m_Resource->m_Resource = script;
            params.m_Resource->m_ResourceSize = params.m_BufferSize - lua_module->m_Source.m_Script.m_Count;
            dmDDF::FreeMessage(lua_module);
            return dmResource::RESULT_OK;
        }
        else
        {
            dmDDF::FreeMessage(lua_module);
            return dmResource::RESULT_FORMAT_ERROR;
        }
    }

    static dmResource::Result ResDestroyGuiScript(const dmResource::ResourceDestroyParams& params)
    {
        dmGui::HScript script = (dmGui::HScript)params.m_Resource->m_Resource;
        dmGui::DeleteScript(script);
        return dmResource::RESULT_OK;
    }

    static dmResource::Result ResRecreateGuiScript(const dmResource::ResourceRecreateParams& params)
    {
        ResGuiContexts* gui_context = (ResGuiContexts*) params.m_Context;
        dmGui::HScript script = (dmGui::HScript)params.m_Resource->m_Resource;

        dmLuaDDF::LuaModule* lua_module = 0;
        dmDDF::Result e = dmDDF::LoadMessage<dmLuaDDF::LuaModule>(params.m_Buffer, params.m_BufferSize, &lua_module);
        if ( e != dmDDF::RESULT_OK ) {
            return dmResource::RESULT_FORMAT_ERROR;
        }

        dmGameObject::PatchLuaBytecode(&lua_module->m_Source);

        if (!dmGameObject::RegisterSubModules(params.m_Factory, gui_context->m_ScriptContext, lua_module))
        {
            dmDDF::FreeMessage(lua_module);
            return dmResource::RESULT_FORMAT_ERROR;
        }

        dmGui::Result result = dmGui::SetScript(script, &lua_module->m_Source);
        if (result == dmGui::RESULT_OK)
        {
            params.m_Resource->m_ResourceSize = params.m_BufferSize - lua_module->m_Source.m_Script.m_Count;
            dmDDF::FreeMessage(lua_module);
            return dmResource::RESULT_OK;
        }
        else
        {
            dmDDF::FreeMessage(lua_module);
            return dmResource::RESULT_FORMAT_ERROR;
        }
    }

    static dmResource::Result ResourceTypeGuiScript_Register(dmResource::ResourceTypeRegisterContext& ctx)
    {
        dmScript::HContext* gui_scriptc_ctx = (dmScript::HContext*)ctx.m_Contexts->Get(dmHashString64("gui_scriptc"));
        if (gui_scriptc_ctx == 0)
        {
            dmLogError("Missing resource context 'gui_scriptc' when registering resource type 'gui_scriptc'");
            return dmResource::RESULT_INVAL;
        }

        dmGui::HContext* gui_ctx = (dmGui::HContext*)ctx.m_Contexts->Get(dmHashString64("guic"));
        if (gui_ctx == 0)
        {
            dmLogError("Missing resource context 'guic' when registering resource type 'gui_scriptc'");
            return dmResource::RESULT_INVAL;
        }

        ResGuiContexts* resctx = (ResGuiContexts*)malloc(sizeof(ResGuiContexts));
        resctx->m_GuiContext = *gui_ctx;
        resctx->m_ScriptContext = *gui_scriptc_ctx;
        g_ResGuiScriptContext = resctx;

        return dmResource::RegisterType(ctx.m_Factory,
                                           ctx.m_Name,
                                           resctx, // context
                                           ResPreloadGuiScript,
                                           ResCreateGuiScript,
                                           0, // post create
                                           ResDestroyGuiScript,
                                           ResRecreateGuiScript);
    }

    static dmResource::Result ResourceTypeGuiScript_Unregister(dmResource::ResourceTypeRegisterContext& ctx)
    {
        free((void*)g_ResGuiScriptContext);
        g_ResGuiScriptContext = 0;
        return dmResource::RESULT_OK;
    }
}

DM_DECLARE_RESOURCE_TYPE(ResourceTypeGuiScript, "gui_scriptc", dmGameSystem::ResourceTypeGuiScript_Register, dmGameSystem::ResourceTypeGuiScript_Unregister);

