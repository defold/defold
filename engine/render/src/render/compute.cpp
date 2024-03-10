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

#include <dlib/log.h>
#include <dlib/dstrings.h>

#include "render.h"
#include "render_private.h"

namespace dmRender
{
    HComputeProgram NewComputeProgram(HRenderContext render_context, dmGraphics::HComputeProgram shader)
    {
        if (!dmGraphics::IsContextFeatureSupported(render_context->m_GraphicsContext, dmGraphics::CONTEXT_FEATURE_COMPUTE_SHADER))
        {
            dmLogError("Compute programs are not supported on this context.");
            return 0;
        }

        ComputeProgram* program        = new ComputeProgram();
        program->m_RenderContext       = render_context;
        program->m_Shader              = shader;
        program->m_Program             = dmGraphics::NewProgram(render_context->m_GraphicsContext, shader);
        uint32_t total_constants_count = dmGraphics::GetUniformCount(program->m_Program);

        uint32_t constants_count = 0;
        uint32_t sampler_count   = 0;
        GetProgramUniformCount(program->m_Program, total_constants_count, &constants_count, &sampler_count);

        if (constants_count > 0)
        {
            program->m_NameHashToLocation.SetCapacity(constants_count, constants_count * 2);
            program->m_Constants.SetCapacity(constants_count);
        }

        dmArray<Sampler> samplers;
        SetMaterialConstantValues(render_context->m_GraphicsContext, program->m_Program, total_constants_count, program->m_NameHashToLocation, program->m_Constants, samplers);

        return (HComputeProgram) program;
    }

    dmGraphics::HComputeProgram GetComputeProgramShader(HComputeProgram program)
    {
        return program->m_Shader;
    }

    dmGraphics::HProgram GetComputeProgram(HComputeProgram program)
    {
        return program->m_Program;
    }

    void DeleteComputeProgram(dmRender::HRenderContext render_context, HComputeProgram program)
    {
        dmGraphics::HContext graphics_context = dmRender::GetGraphicsContext(render_context);
        dmGraphics::DeleteProgram(graphics_context, program->m_Program);
        delete program;
    }

    HRenderContext GetProgramRenderContext(HComputeProgram program)
    {
        return program->m_RenderContext;
    }

    uint64_t GetProgramUserData(HComputeProgram program)
    {
        return program->m_UserData;
    }

    void SetProgramUserData(HComputeProgram program, uint64_t user_data)
    {
        program->m_UserData = user_data;
    }
}
