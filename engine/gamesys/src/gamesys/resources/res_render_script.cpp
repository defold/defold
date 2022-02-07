// Copyright 2020 The Defold Foundation
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

#include "res_render_script.h"

#include <render/render.h>

#include <gameobject/lua_ddf.h>
#include <gameobject/gameobject_script_util.h>

namespace dmGameSystem
{
    dmResource::Result ResRenderScriptCreate(const dmResource::ResourceCreateParams& params)
    {
        dmLuaDDF::LuaModule* lua_module = 0;
        dmDDF::Result e = dmDDF::LoadMessage<dmLuaDDF::LuaModule>(params.m_Buffer, params.m_BufferSize, &lua_module);
        if ( e != dmDDF::RESULT_OK )
            return dmResource::RESULT_FORMAT_ERROR;

        dmRender::HRenderContext render_context = (dmRender::HRenderContext) params.m_Context;
        if (!dmGameObject::RegisterSubModules(params.m_Factory, dmRender::GetScriptContext(render_context), lua_module))
        {
            dmDDF::FreeMessage(lua_module);
            return dmResource::RESULT_FORMAT_ERROR;
        }

        dmRender::HRenderScript render_script = dmRender::NewRenderScript(render_context, &lua_module->m_Source);
        params.m_Resource->m_ResourceSize = params.m_BufferSize - lua_module->m_Source.m_Script.m_Count;
        dmDDF::FreeMessage(lua_module);
        if (render_script)
        {
            params.m_Resource->m_Resource = (void*) render_script;
            return dmResource::RESULT_OK;
        }
        else
        {
            return dmResource::RESULT_FORMAT_ERROR;
        }
    }

    dmResource::Result ResRenderScriptDestroy(const dmResource::ResourceDestroyParams& params)
    {
        dmRender::HRenderContext render_context = (dmRender::HRenderContext) params.m_Context;
        dmRender::DeleteRenderScript(render_context, (dmRender::HRenderScript) params.m_Resource->m_Resource);
        return dmResource::RESULT_OK;
    }

    dmResource::Result ResRenderScriptRecreate(const dmResource::ResourceRecreateParams& params)
    {
        dmRender::HRenderContext render_context = (dmRender::HRenderContext) params.m_Context;
        dmRender::HRenderScript render_script = (dmRender::HRenderScript) params.m_Resource->m_Resource;

        dmLuaDDF::LuaModule* lua_module = 0;
        dmDDF::Result e = dmDDF::LoadMessage<dmLuaDDF::LuaModule>(params.m_Buffer, params.m_BufferSize, &lua_module);
        if ( e != dmDDF::RESULT_OK ) {
            return dmResource::RESULT_FORMAT_ERROR;
        }
        if (!dmGameObject::RegisterSubModules(params.m_Factory, dmRender::GetScriptContext(render_context), lua_module))
        {
            dmDDF::FreeMessage(lua_module);
            return dmResource::RESULT_FORMAT_ERROR;
        }
        if (dmRender::ReloadRenderScript(render_context, render_script, &lua_module->m_Source))
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
}
