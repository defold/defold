#include "res_compute_program.h"

#include <render/render.h>
#include <render/compute_program_ddf.h>

namespace dmGameSystem
{
	dmResource::Result AcquireResources(dmResource::HFactory factory, dmRenderDDF::ComputeProgramDesc* ddf, void** program)
    {
        dmResource::Result factory_e;
        factory_e = dmResource::Get(factory, ddf->m_Program, program);
        if ( factory_e != dmResource::RESULT_OK)
        {
            return factory_e;
        }

        return dmResource::RESULT_OK;
    }

    dmResource::Result ResComputeProgramCreate(const dmResource::ResourceCreateParams& params)
    {
    	dmRender::HRenderContext render_context = (dmRender::HRenderContext) params.m_Context;
        dmRenderDDF::ComputeProgramDesc* ddf   = (dmRenderDDF::ComputeProgramDesc*)params.m_PreloadData;

        void* shader;
        dmResource::Result r = AcquireResources(params.m_Factory, ddf, &shader);
        if (r == dmResource::RESULT_OK)
        {
        	dmRender::HComputeProgram program = dmRender::NewComputeProgram(render_context, (dmGraphics::HComputeProgram) shader);

            dmResource::SResourceDescriptor desc;
            dmResource::Result factory_e;

            // dmResource::RegisterResourceReloadedCallback(params.m_Factory, ResourceReloadedCallback, material);

            params.m_Resource->m_Resource = (void*) program;
        }
        dmDDF::FreeMessage(ddf);
        return r;
    }

    dmResource::Result ResComputeProgramDestroy(const dmResource::ResourceDestroyParams& params)
    {
        dmRender::HRenderContext render_context = (dmRender::HRenderContext) params.m_Context;
        dmRender::HComputeProgram program = (dmRender::HComputeProgram) params.m_Resource->m_Resource;

        // dmResource::UnregisterResourceReloadedCallback(params.m_Factory, ResourceReloadedCallback, material);

        dmResource::Release(params.m_Factory, (void*) dmRender::GetComputeProgramShader(program));
        dmRender::DeleteComputeProgram(render_context, program);

    	return dmResource::RESULT_OK;
    }

    dmResource::Result ResComputeProgramRecreate(const dmResource::ResourceRecreateParams& params)
    {
    	return dmResource::RESULT_OK;
    }

    dmResource::Result ResComputeProgramPreload(const dmResource::ResourcePreloadParams& params)
    {
    	dmRenderDDF::ComputeProgramDesc* ddf;
        dmDDF::Result e = dmDDF::LoadMessage<dmRenderDDF::ComputeProgramDesc>(params.m_Buffer, params.m_BufferSize, &ddf);
        if (e != dmDDF::RESULT_OK)
        {
            return dmResource::RESULT_DDF_ERROR;
        }

        dmResource::PreloadHint(params.m_HintInfo, ddf->m_Program);
        *params.m_PreloadData = ddf;
        return dmResource::RESULT_OK;
    }
}
