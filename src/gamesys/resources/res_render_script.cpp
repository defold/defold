#include "res_render_script.h"

#include <render/render_script.h>

namespace dmGameSystem
{
    dmResource::CreateResult ResRenderScriptCreate(dmResource::HFactory factory,
            void* context,
            const void* buffer, uint32_t buffer_size,
            dmResource::SResourceDescriptor* resource,
            const char* filename)
    {
        dmRender::HRenderScript render_script = dmRender::NewRenderScript(buffer, buffer_size, filename);
        if (render_script)
        {
            resource->m_Resource = (void*) render_script;
            return dmResource::CREATE_RESULT_OK;
        }
        else
        {
            return dmResource::CREATE_RESULT_UNKNOWN;
        }
    }

    dmResource::CreateResult ResRenderScriptDestroy(dmResource::HFactory factory,
            void* context,
            dmResource::SResourceDescriptor* resource)
    {
        dmRender::DeleteRenderScript((dmRender::HRenderScript) resource->m_Resource);
        return dmResource::CREATE_RESULT_OK;
    }

    dmResource::CreateResult ResRenderScriptRecreate(dmResource::HFactory factory,
            void* context,
            const void* buffer, uint32_t buffer_size,
            dmResource::SResourceDescriptor* resource,
            const char* filename)
    {
        dmRender::HRenderScript render_script = (dmRender::HRenderScript) resource->m_Resource;
        if (dmRender::ReloadRenderScript(render_script, buffer, buffer_size, filename))
        {
            return dmResource::CREATE_RESULT_OK;
        }
        else
        {
            return dmResource::CREATE_RESULT_UNKNOWN;
        }
    }
}
