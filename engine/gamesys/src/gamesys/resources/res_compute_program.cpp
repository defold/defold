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

    static void SetProgram(const char* path, dmRender::HComputeProgram program, dmRenderDDF::ComputeProgramDesc* ddf)
    {
        dmRenderDDF::ComputeProgramDesc::Constant* constants = ddf->m_Constants.m_Data;

        // save pre-set fragment constants
        for (uint32_t i = 0; i < ddf->m_Constants.m_Count; i++)
        {
            const char* name   = constants[i].m_Name;
            dmhash_t name_hash = dmHashString64(name);
            dmRender::SetComputeProgramConstant(program, name_hash, (dmVMath::Vector4*) constants[i].m_Value.m_Data, constants[i].m_Value.m_Count);

            // dmRender::SetMaterialProgramConstantType(material, name_hash, constants[i].m_Type);
            // dmRender::SetMaterialProgramConstant(material, name_hash,
            //     (dmVMath::Vector4*) constants[i].m_Value.m_Data, constants[i].m_Value.m_Count);
        }
    }

    dmResource::Result ResComputeProgramCreate(const dmResource::ResourceCreateParams& params)
    {
    	dmRender::HRenderContext render_context = (dmRender::HRenderContext) params.m_Context;
        dmRenderDDF::ComputeProgramDesc* ddf   = (dmRenderDDF::ComputeProgramDesc*)params.m_PreloadData;

        void* shader;

        dmResource::Result r = AcquireResources(params.m_Factory, ddf, &shader);
        if (r == dmResource::RESULT_OK)
        {
        	dmRender::HComputeProgram program = dmRender::NewComputeProgram(render_context, (dmGraphics::HComputeShader) shader);

            SetProgram(params.m_Filename, program, ddf);

            dmResource::SResourceDescriptor desc;
            dmResource::Result factory_e;

            params.m_Resource->m_Resource = (void*) program;
        }
        dmDDF::FreeMessage(ddf);
        return r;
    }

    dmResource::Result ResComputeProgramDestroy(const dmResource::ResourceDestroyParams& params)
    {
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

        /*
        if (!ValidateFormat(ddf))
        {
            dmDDF::FreeMessage(ddf);
            return dmResource::RESULT_FORMAT_ERROR;
        }
        */

        dmResource::PreloadHint(params.m_HintInfo, ddf->m_Program);
        *params.m_PreloadData = ddf;
        return dmResource::RESULT_OK;
    }
}
