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

#include "res_shader_program.h"
#include <graphics/graphics.h>
#include <dlib/log.h>

namespace dmGameSystem
{
    static dmResource::Result AcquireResources(dmGraphics::HContext context, dmResource::HFactory factory, const char* filename, dmGraphics::ShaderDesc* ddf, dmGraphics::HProgram* program)
    {
        char error_buffer[1024] = {};
        dmGraphics::HProgram prog = dmGraphics::NewProgram(context, ddf, error_buffer, sizeof(error_buffer));
        if (prog == 0)
        {
            dmLogError("Failed to create shader program '%s': %s", filename, error_buffer);
            return dmResource::RESULT_FORMAT_ERROR;
        }
        *program = prog;

        return dmResource::RESULT_OK;
    }

    dmResource::Result ResShaderProgramPreload(const dmResource::ResourcePreloadParams* params)
    {
        dmGraphics::ShaderDesc* ddf;
        dmDDF::Result e = dmDDF::LoadMessage(params->m_Buffer, params->m_BufferSize, &ddf);
        if (e != dmDDF::RESULT_OK)
        {
            return dmResource::RESULT_DDF_ERROR;
        }
        *params->m_PreloadData = ddf;
        return dmResource::RESULT_OK;
    }

    dmResource::Result ResShaderProgramCreate(const dmResource::ResourceCreateParams* params)
    {
        dmGraphics::ShaderDesc* ddf = (dmGraphics::ShaderDesc*)params->m_PreloadData;
        dmGraphics::HProgram resource = 0x0;
        dmResource::Result r = AcquireResources((dmGraphics::HContext) params->m_Context, params->m_Factory, params->m_Filename, ddf, &resource);
        if (r == dmResource::RESULT_OK)
        {
            dmResource::SetResource(params->m_Resource, (void*) resource);
        }
        dmDDF::FreeMessage(ddf);
        return r;
    }

    dmResource::Result ResShaderProgramDestroy(const dmResource::ResourceDestroyParams* params)
    {
        dmGraphics::HProgram resource = (dmGraphics::HProgram) dmResource::GetResource(params->m_Resource);
        dmGraphics::DeleteProgram((dmGraphics::HContext) params->m_Context, resource);
        return dmResource::RESULT_OK;
    }

    dmResource::Result ResShaderProgramRecreate(const dmResource::ResourceRecreateParams* params)
    {
        dmGraphics::HProgram resource = (dmGraphics::HProgram)dmResource::GetResource(params->m_Resource);
        if (resource == 0)
        {
            return dmResource::RESULT_FORMAT_ERROR;
        }

        dmGraphics::ShaderDesc* ddf;
        dmDDF::Result e = dmDDF::LoadMessage(params->m_Buffer, params->m_BufferSize, &ddf);
        if (e != dmDDF::RESULT_OK)
        {
            return dmResource::RESULT_FORMAT_ERROR;
        }

        dmResource::Result res = dmResource::RESULT_OK;
        if(!dmGraphics::ReloadProgram((dmGraphics::HContext) params->m_Context, resource, ddf))
        {
            res = dmResource::RESULT_FORMAT_ERROR;
        }

        dmDDF::FreeMessage(ddf);
        return res;
    }
}
