#include "res_fragment_program.h"
#include <graphics/graphics.h>

namespace dmGameSystem
{
    static dmResource::Result AcquireResources(dmGraphics::HContext context, dmResource::HFactory factory, dmGraphics::ShaderDesc* ddf, dmGraphics::HVertexProgram* program)
    {
        uint32_t shader_data_len;
        dmGraphics::ShaderDesc::Shader* shader_ddf =  dmGraphics::GetShaderProgramData(context, ddf);
        if (shader_ddf == 0x0)
        {
            return dmResource::RESULT_FORMAT_ERROR;
        }
        dmGraphics::HFragmentProgram prog = dmGraphics::NewFragmentProgram(context, shader_ddf);
        if (prog == 0)
        {
            return dmResource::RESULT_FORMAT_ERROR;
        }
        *program = prog;

        return dmResource::RESULT_OK;
    }

    dmResource::Result ResFragmentProgramPreload(const dmResource::ResourcePreloadParams& params)
    {
        dmGraphics::ShaderDesc* ddf;
        dmDDF::Result e = dmDDF::LoadMessage(params.m_Buffer, params.m_BufferSize, &ddf);
        if (e != dmDDF::RESULT_OK)
        {
            return dmResource::RESULT_DDF_ERROR;
        }
        *params.m_PreloadData = ddf;
        return dmResource::RESULT_OK;
    }

    dmResource::Result ResFragmentProgramCreate(const dmResource::ResourceCreateParams& params)
    {
        dmGraphics::ShaderDesc* ddf = (dmGraphics::ShaderDesc*)params.m_PreloadData;
        dmGraphics::HVertexProgram resource = 0x0;
        dmResource::Result r = AcquireResources((dmGraphics::HContext) params.m_Context, params.m_Factory, ddf, &resource);
        if (r == dmResource::RESULT_OK)
        {
            params.m_Resource->m_Resource = (void*) resource;
        }
        dmDDF::FreeMessage(ddf);
        return r;
    }

    dmResource::Result ResFragmentProgramDestroy(const dmResource::ResourceDestroyParams& params)
    {
        dmGraphics::HVertexProgram resource = (dmGraphics::HVertexProgram)params.m_Resource->m_Resource;
        dmGraphics::DeleteFragmentProgram(resource);
        return dmResource::RESULT_OK;
    }

    dmResource::Result ResFragmentProgramRecreate(const dmResource::ResourceRecreateParams& params)
    {
        dmGraphics::HVertexProgram resource = (dmGraphics::HVertexProgram)params.m_Resource->m_Resource;
        if (resource == 0)
        {
            return dmResource::RESULT_FORMAT_ERROR;
        }

        dmGraphics::ShaderDesc* ddf;
        dmDDF::Result e = dmDDF::LoadMessage(params.m_Buffer, params.m_BufferSize, &ddf);
        if (e != dmDDF::RESULT_OK)
        {
            return dmResource::RESULT_FORMAT_ERROR;
        }

        dmResource::Result res = dmResource::RESULT_OK;
        uint32_t shader_data_len;
        dmGraphics::ShaderDesc::Shader* shader_ddf =  dmGraphics::GetShaderProgramData((dmGraphics::HContext)params.m_Context, ddf);
        if (shader_ddf == 0x0)
        {
            res = dmResource::RESULT_FORMAT_ERROR;
        }
        else if(!dmGraphics::ReloadFragmentProgram(resource, shader_ddf))
        {
            res = dmResource::RESULT_FORMAT_ERROR;
        }

        dmDDF::FreeMessage(ddf);
        return res;
    }
}
