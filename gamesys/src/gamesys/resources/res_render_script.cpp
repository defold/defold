#include "res_render_script.h"

#include <render/render.h>

namespace dmGameSystem
{
    dmResource::Result ResRenderScriptCreate(dmResource::HFactory factory,
            void* context,
            const void* buffer, uint32_t buffer_size,
            dmResource::SResourceDescriptor* resource,
            const char* filename)
    {
        dmRender::HRenderContext render_context = (dmRender::HRenderContext)context;
        dmRender::HRenderScript render_script = dmRender::NewRenderScript(render_context, buffer, buffer_size, filename);
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
        if (dmRender::ReloadRenderScript(render_context, render_script, buffer, buffer_size, filename))
        {
            return dmResource::RESULT_OK;
        }
        else
        {
            return dmResource::RESULT_FORMAT_ERROR;
        }
    }
}
