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

    static void ResourceReloadedCallback(const dmResource::ResourceReloadedParams& params)
    {
        dmRender::HComputeProgram program       = (dmRender::HComputeProgram) params.m_UserData;
        dmRender::HRenderContext render_context = dmRender::GetProgramRenderContext(program);
        dmGraphics::HContext graphics_context   = dmRender::GetGraphicsContext(render_context);
        dmGraphics::HComputeProgram shader      = GetComputeProgramShader(program);
        dmGraphics::HProgram gfx_program        = GetComputeProgram(program);

        if (!dmGraphics::ReloadProgram(graphics_context, gfx_program, shader))
        {
            dmLogWarning("Reloading the compute program failed.");
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
            dmRender::HComputeProgram program = dmRender::NewComputeProgram(render_context, (dmGraphics::HComputeProgram) shader);

            dmResource::SResourceDescriptor desc;
            dmResource::Result res = dmResource::GetDescriptor(params.m_Factory, ddf->m_Program, &desc);

            assert(res == dmResource::RESULT_OK);
            dmRender::SetProgramUserData(program, desc.m_NameHash);

            dmResource::RegisterResourceReloadedCallback(params.m_Factory, ResourceReloadedCallback, program);

            params.m_Resource->m_Resource = (void*) program;
        }
        dmDDF::FreeMessage(ddf);
        return r;
    }

    dmResource::Result ResComputeProgramDestroy(const dmResource::ResourceDestroyParams& params)
    {
        dmRender::HRenderContext render_context = (dmRender::HRenderContext) params.m_Context;
        dmRender::HComputeProgram program = (dmRender::HComputeProgram) params.m_Resource->m_Resource;

        dmResource::UnregisterResourceReloadedCallback(params.m_Factory, ResourceReloadedCallback, program);

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
