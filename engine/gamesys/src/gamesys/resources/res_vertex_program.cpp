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

#include "res_vertex_program.h"
#include <graphics/graphics.h>

namespace dmGameSystem
{
    static dmResource::Result AcquireResources(dmGraphics::HContext context, dmResource::HFactory factory, dmGraphics::ShaderDesc* ddf, dmGraphics::HVertexProgram* program)
    {
        dmGraphics::ShaderDesc::Shader* shader =  dmGraphics::GetShaderProgram(context, ddf);
        if (shader == 0x0)
        {
            return dmResource::RESULT_FORMAT_ERROR;
        }
        dmGraphics::HVertexProgram prog = dmGraphics::NewVertexProgram(context, shader);
        if (prog == 0)
        {
            return dmResource::RESULT_FORMAT_ERROR;
        }
        *program = prog;

        return dmResource::RESULT_OK;
    }

    dmResource::Result ResVertexProgramPreload(const dmResource::ResourcePreloadParams& params)
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

    dmResource::Result ResVertexProgramCreate(const dmResource::ResourceCreateParams& params)
    {
        dmGraphics::ShaderDesc* ddf = (dmGraphics::ShaderDesc*)params.m_PreloadData;
        dmGraphics::HVertexProgram resource = 0x0;
        dmResource::Result r = AcquireResources((dmGraphics::HContext) params.m_Context, params.m_Factory, ddf, &resource);
        dmDDF::FreeMessage(ddf);
        if (r == dmResource::RESULT_OK)
        {
            params.m_Resource->m_Resource = (void*) resource;
        }
        return r;
    }

    dmResource::Result ResVertexProgramDestroy(const dmResource::ResourceDestroyParams& params)
    {
        dmGraphics::HVertexProgram resource = (dmGraphics::HVertexProgram)params.m_Resource->m_Resource;
        dmGraphics::DeleteVertexProgram(resource);
        return dmResource::RESULT_OK;
    }

    dmResource::Result ResVertexProgramRecreate(const dmResource::ResourceRecreateParams& params)
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
        dmGraphics::ShaderDesc::Shader* shader =  dmGraphics::GetShaderProgram((dmGraphics::HContext)params.m_Context, ddf);
        if (shader == 0x0)
        {
            res = dmResource::RESULT_FORMAT_ERROR;
        }
        else if(!dmGraphics::ReloadVertexProgram(resource, shader))
        {
            res = dmResource::RESULT_FORMAT_ERROR;
        }

        dmDDF::FreeMessage(ddf);
        return res;
    }

}
