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
        uint32_t samplers_count  = 0;
        GetProgramUniformCount(program->m_Program, total_constants_count, &constants_count, &samplers_count);

        if (constants_count > 0)
        {
            program->m_NameHashToLocation.SetCapacity(constants_count, constants_count * 2);
            program->m_Constants.SetCapacity(constants_count);
        }

        if (samplers_count > 0)
        {
            program->m_Samplers.SetCapacity(samplers_count);
            for (uint32_t i = 0; i < samplers_count; ++i)
            {
                program->m_Samplers.Push(Sampler());
            }
        }

        SetMaterialConstantValues(render_context->m_GraphicsContext, program->m_Program, total_constants_count, program->m_NameHashToLocation, program->m_Constants, program->m_Samplers);

        return (HComputeProgram) program;
    }

    void ApplyComputeProgramConstants(dmRender::HRenderContext render_context, HComputeProgram compute_program)
    {
        dmGraphics::HContext graphics_context           = dmRender::GetGraphicsContext(render_context);
        const dmArray<RenderConstant>& render_constants = compute_program->m_Constants;

        for (int i = 0; i < render_constants.Size(); ++i)
        {
            const RenderConstant& render_constant = render_constants[i];
            const HConstant constant              = render_constant.m_Constant;
            int32_t location                      = GetConstantLocation(constant);

            uint32_t num_values;
            dmVMath::Vector4* values = GetConstantValues(constant, &num_values);
            dmGraphics::SetConstantV4(graphics_context, values, num_values, location);

            // dmRenderDDF::MaterialDesc::ConstantType type = GetConstantType(constant);
        }
    }

    static inline int32_t FindConstantIndex(HComputeProgram program, dmhash_t name_hash)
    {
        dmArray<RenderConstant>& constants = program->m_Constants;
        for (int32_t i = 0; i < (int32_t) constants.Size(); ++i)
        {
            if (GetConstantName(constants[i].m_Constant) == name_hash)
            {
                return i;
            }
        }
        return -1;
    }

    void SetComputeProgramConstant(HComputeProgram program, dmhash_t name_hash, Vector4* values, uint32_t count)
    {
        int32_t index = FindConstantIndex(program, name_hash);
        if (index < 0)
        {
            return;
        }

        RenderConstant& mc = program->m_Constants[index];

        uint32_t num_default_values;
        dmVMath::Vector4* constant_values = dmRender::GetConstantValues(mc.m_Constant, &num_default_values);

        count = dmMath::Min(count, num_default_values);

        // we musn't set less values than are already registered with the program
        // so we write to the previous buffer
        memcpy(constant_values, values, count * sizeof(dmVMath::Vector4));
    }

    int32_t GetComputeProgramSamplerIndex(HComputeProgram program, dmhash_t name_hash)
    {
        for (int i = 0; i < program->m_Samplers.Size(); ++i)
        {
            if (program->m_Samplers[i].m_NameHash == name_hash)
            {
                return i;
            }
        }
        return -1;
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
