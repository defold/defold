#include "res_render_script.h"

#include <render/render.h>

#include <gameobject/lua_ddf.h>
#include <gameobject/gameobject_script_util.h>

namespace dmGameSystem
{
    dmResource::Result ResRenderScriptCreate(dmResource::HFactory factory,
            void* context,
            const void* buffer, uint32_t buffer_size,
            void* preload_data,
            dmResource::SResourceDescriptor* resource,
            const char* filename)
    {
        dmLuaDDF::LuaModule* lua_module = 0;
        dmDDF::Result e = dmDDF::LoadMessage<dmLuaDDF::LuaModule>(buffer, buffer_size, &lua_module);
        if ( e != dmDDF::RESULT_OK )
            return dmResource::RESULT_FORMAT_ERROR;

        dmRender::HRenderContext render_context = (dmRender::HRenderContext)context;
        if (!dmGameObject::RegisterSubModules(factory, dmRender::GetScriptContext(render_context), lua_module))
        {
            dmDDF::FreeMessage(lua_module);
            return dmResource::RESULT_FORMAT_ERROR;
        }

        dmRender::HRenderScript render_script = dmRender::NewRenderScript(render_context, &lua_module->m_Source);
        dmDDF::FreeMessage(lua_module);
        if (render_script)
        {
            resource->m_Resource = (void*) render_script;
            return dmResource::RESULT_OK;
        }
        else
        {
            return dmResource::RESULT_FORMAT_ERROR;
        }
    }

    dmResource::Result ResRenderScriptDestroy(dmResource::HFactory factory,
            void* context,
            dmResource::SResourceDescriptor* resource)
    {
        dmRender::HRenderContext render_context = (dmRender::HRenderContext)context;
        dmRender::DeleteRenderScript(render_context, (dmRender::HRenderScript) resource->m_Resource);
        return dmResource::RESULT_OK;
    }

    dmResource::Result ResRenderScriptRecreate(dmResource::HFactory factory,
            void* context,
            const void* buffer, uint32_t buffer_size,
            dmResource::SResourceDescriptor* resource,
            const char* filename)
    {
        dmRender::HRenderContext render_context = (dmRender::HRenderContext)context;
        dmRender::HRenderScript render_script = (dmRender::HRenderScript) resource->m_Resource;

        dmLuaDDF::LuaModule* lua_module = 0;
        dmDDF::Result e = dmDDF::LoadMessage<dmLuaDDF::LuaModule>(buffer, buffer_size, &lua_module);
        if ( e != dmDDF::RESULT_OK ) {
            return dmResource::RESULT_FORMAT_ERROR;
        }
        if (!dmGameObject::RegisterSubModules(factory, dmRender::GetScriptContext(render_context), lua_module))
        {
            dmDDF::FreeMessage(lua_module);
            return dmResource::RESULT_FORMAT_ERROR;
        }
        if (dmRender::ReloadRenderScript(render_context, render_script, &lua_module->m_Source))
        {
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
