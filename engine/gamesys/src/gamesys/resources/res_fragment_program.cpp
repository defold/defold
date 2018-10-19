#include "res_fragment_program.h"
#include <dlib/log.h>

namespace dmGameSystem
{

    dmResource::Result AcquireResources(dmGraphics::HContext context, dmResource::HFactory factory, FragmentProgramResource* resource, const char* filename)
    {
        uint32_t shader_data_len;
        void* shader_data =  dmGraphics::GetShaderProgramData(context, resource->m_ShaderDDF, shader_data_len);
        dmGraphics::HFragmentProgram prog = dmGraphics::NewFragmentProgram(context, shader_data, shader_data_len);
        if (prog == 0 )
            return dmResource::RESULT_FORMAT_ERROR;
        resource->m_Program = prog;

        return dmResource::RESULT_OK;
    }

    static void ReleaseResources(dmResource::HFactory factory, FragmentProgramResource* resource)
    {
        if(resource->m_Program != 0x0)
        {
            dmGraphics::DeleteFragmentProgram((dmGraphics::HFragmentProgram) resource->m_Program);
            resource->m_Program = 0;
        }
        if (resource->m_ShaderDDF != 0x0)
        {
            dmDDF::FreeMessage(resource->m_ShaderDDF);
            resource->m_ShaderDDF = 0;
        }
    }

    dmResource::Result ResFragmentProgramPreload(const dmResource::ResourcePreloadParams& params)
    {
        dmGraphics::ShaderDesc* shader_desc;
        dmDDF::Result e = dmDDF::LoadMessage(params.m_Buffer, params.m_BufferSize, &dmGraphics_ShaderDesc_DESCRIPTOR, (void**) &shader_desc);
        if (e != dmDDF::RESULT_OK)
        {
            return dmResource::RESULT_DDF_ERROR;
        }
        *params.m_PreloadData = shader_desc;
        return dmResource::RESULT_OK;
    }

    dmResource::Result ResFragmentProgramCreate(const dmResource::ResourceCreateParams& params)
    {
        FragmentProgramResource* resource = new FragmentProgramResource();
        memset(resource, 0, sizeof(FragmentProgramResource));
        resource->m_ShaderDDF = (dmGraphics::ShaderDesc*) params.m_PreloadData;
        dmResource::Result r = AcquireResources((dmGraphics::HContext) params.m_Context, params.m_Factory, resource, params.m_Filename);
        if (r == dmResource::RESULT_OK)
        {
            params.m_Resource->m_Resource = (void*) resource;
        }
        else
        {
            ReleaseResources(params.m_Factory, resource);
            delete resource;
        }
        return r;
    }

    dmResource::Result ResFragmentProgramDestroy(const dmResource::ResourceDestroyParams& params)
    {
        FragmentProgramResource* resource = (FragmentProgramResource*)params.m_Resource->m_Resource;
        ReleaseResources(params.m_Factory, resource);
        delete resource;
        return dmResource::RESULT_OK;
    }

    dmResource::Result ResFragmentProgramRecreate(const dmResource::ResourceRecreateParams& params)
    {
        FragmentProgramResource* resource = (FragmentProgramResource*)params.m_Resource->m_Resource;
        if (resource == 0 || resource->m_Program == 0 )
        {
            return dmResource::RESULT_FORMAT_ERROR;
        }

        dmGraphics::ShaderDesc* shader_desc;
        dmDDF::Result e = dmDDF::LoadMessage(params.m_Buffer, params.m_BufferSize, &dmGraphics_ShaderDesc_DESCRIPTOR, (void**) &shader_desc);
        if (e != dmDDF::RESULT_OK)
        {
            return dmResource::RESULT_FORMAT_ERROR;
        }

        uint32_t shader_data_len;
        void* shader_data =  dmGraphics::GetShaderProgramData((dmGraphics::HContext) params.m_Context, shader_desc, shader_data_len);
        if(!dmGraphics::ReloadFragmentProgram(resource->m_Program, shader_data, shader_data_len))
        {
            return dmResource::RESULT_FORMAT_ERROR;
        }

        dmDDF::FreeMessage(resource->m_ShaderDDF);
        resource->m_ShaderDDF = shader_desc;
        return dmResource::RESULT_OK;
    }
}
