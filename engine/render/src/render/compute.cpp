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

#include <dlib/log.h>
#include <dlib/dstrings.h>

#include "render.h"
#include "render_private.h"

namespace dmRender
{
    HComputeProgram NewComputeProgram(HRenderContext render_context, dmGraphics::HProgram compute_program)
    {
        if (!dmGraphics::IsContextFeatureSupported(render_context->m_GraphicsContext, dmGraphics::CONTEXT_FEATURE_COMPUTE_SHADER))
        {
            dmLogError("Compute programs are not supported on this context.");
            return 0;
        }

        ComputeProgram* program        = new ComputeProgram();
        program->m_RenderContext       = render_context;
        program->m_Program             = compute_program;
        uint32_t total_constants_count = dmGraphics::GetUniformCount(program->m_Program);

        uint32_t constants_count = 0;
        uint32_t samplers_count  = 0;
        GetProgramUniformCount(program->m_Program, total_constants_count, &constants_count, &samplers_count);
        uint32_t total_uniforms_count = constants_count + samplers_count;

        if (total_uniforms_count > 0)
        {
            program->m_NameHashToLocation.SetCapacity(total_uniforms_count, total_uniforms_count * 2);
            program->m_Constants.SetCapacity(total_uniforms_count);
        }

        if (samplers_count > 0)
        {
            program->m_Samplers.SetCapacity(samplers_count);
            for (uint32_t i = 0; i < samplers_count; ++i)
            {
                program->m_Samplers.Push(Sampler());
            }
        }

        SetProgramConstantValues(render_context->m_GraphicsContext, program->m_Program, total_constants_count, program->m_NameHashToLocation, program->m_Constants, program->m_Samplers);

        return (HComputeProgram) program;
    }

    void ApplyComputeProgramConstants(dmRender::HRenderContext render_context, HComputeProgram compute_program)
    {
        dmGraphics::HContext graphics_context           = dmRender::GetGraphicsContext(render_context);
        const dmArray<RenderConstant>& render_constants = compute_program->m_Constants;
        dmGraphics::HProgram program                    = compute_program->m_Program;
        dmGraphics::ShaderDesc::Language language       = dmGraphics::GetProgramLanguage(program);

        dmVMath::Matrix4 world_matrix;
        dmVMath::Matrix4 texture_matrix;

        for (int i = 0; i < render_constants.Size(); ++i)
        {
            const RenderConstant& material_constant      = render_constants[i];
            const HConstant constant                     = material_constant.m_Constant;
            dmGraphics::HUniformLocation location        = GetConstantLocation(constant);
            dmRenderDDF::MaterialDesc::ConstantType type = GetConstantType(constant);
            SetProgramConstant(render_context, graphics_context, world_matrix, texture_matrix, language, type, program, location, constant);
        }
    }

    void SetComputeProgramConstant(HComputeProgram compute_program, dmhash_t name_hash, Vector4* values, uint32_t count)
    {
        SetProgramRenderConstant(compute_program->m_Constants, name_hash, values, count);
    }

    void SetComputeProgramConstantType(HComputeProgram compute_program, dmhash_t name_hash, dmRenderDDF::MaterialDesc::ConstantType type)
    {
        SetProgramConstantType(compute_program->m_Constants, name_hash, type);
    }

    bool GetComputeProgramConstant(HComputeProgram compute_program, dmhash_t name_hash, HConstant& out_value)
    {
        return GetProgramConstant(compute_program->m_Constants, name_hash, out_value);
    }

    bool SetComputeProgramSampler(HComputeProgram compute_program, dmhash_t name_hash, uint32_t unit, dmGraphics::TextureWrap u_wrap, dmGraphics::TextureWrap v_wrap, dmGraphics::TextureFilter min_filter, dmGraphics::TextureFilter mag_filter, float max_anisotropy)
    {
        return SetProgramSampler(compute_program->m_Samplers, compute_program->m_NameHashToLocation, name_hash, unit, u_wrap, v_wrap, min_filter, mag_filter, max_anisotropy);
    }

    uint32_t GetComputeProgramSamplerUnit(HComputeProgram compute_program, dmhash_t name_hash)
    {
        return GetProgramSamplerUnit(compute_program->m_Samplers, name_hash);
    }

    HSampler GetComputeProgramSampler(HComputeProgram program, uint32_t unit)
    {
        return GetProgramSampler(program->m_Samplers, unit);
    }

    dmGraphics::HProgram GetComputeProgram(HComputeProgram program)
    {
        return program->m_Program;
    }

    void DeleteComputeProgram(dmRender::HRenderContext render_context, HComputeProgram program)
    {
        for (uint32_t i = 0; i < program->m_Constants.Size(); ++i)
        {
            dmRender::DeleteConstant(program->m_Constants[i].m_Constant);
        }
        delete program;
    }

    HRenderContext GetProgramRenderContext(HComputeProgram program)
    {
        return program->m_RenderContext;
    }
}
