// Copyright 2020-2023 The Defold Foundation
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
    HComputeProgram NewComputeProgram(HRenderContext render_context, dmGraphics::HComputeShader shader)
    {
        ComputeProgram* program        = new ComputeProgram();
        program->m_Shader              = shader;
        program->m_Program             = dmGraphics::NewComputeProgram(render_context->m_GraphicsContext, shader);
        uint32_t total_constants_count = dmGraphics::GetUniformCount(program->m_Program);

        const uint32_t name_buffer_size = 128;
        char name_buffer[name_buffer_size];

        dmGraphics::Type type;
        int32_t num_values       = 0; // number of Vector4
        uint32_t constants_count = 0;

        for (uint32_t i = 0; i < total_constants_count; ++i)
        {
            type = (dmGraphics::Type) -1;
            dmGraphics::GetUniformName(program->m_Program, i, name_buffer, name_buffer_size, &type, &num_values);

            if (type == dmGraphics::TYPE_FLOAT_VEC4 || type == dmGraphics::TYPE_FLOAT_MAT4)
            {
                constants_count++;
            }
        }

        if (constants_count > 0)
        {
            program->m_NameHashToLocation.SetCapacity(constants_count, constants_count * 2);
            program->m_Constants.SetCapacity(constants_count);
        }

        uint32_t default_values_capacity = 0;
        dmVMath::Vector4* default_values = 0;

        for (uint32_t i = 0; i < total_constants_count; ++i)
        {
            uint32_t name_str_length = dmGraphics::GetUniformName(program->m_Program, i, name_buffer, name_buffer_size, &type, &num_values);
            int32_t location         = dmGraphics::GetUniformLocation(program->m_Program, name_buffer);
            if (location == -1) {
                continue;
            }

            assert(name_str_length > 0);

            // For uniform arrays, OpenGL returns the name as "uniform[0]",
            // but we want to identify it as the base name instead.
            for (int j = 0; j < name_str_length; ++j)
            {
                if (name_buffer[j] == '[')
                {
                    name_buffer[j] = 0;
                    break;
                }
            }

            dmhash_t name_hash = dmHashString64(name_buffer);

            if (type == dmGraphics::TYPE_FLOAT_VEC4 || type == dmGraphics::TYPE_FLOAT_MAT4)
            {
                program->m_NameHashToLocation.Put(name_hash, location);

                HConstant render_constant = dmRender::NewConstant(name_hash);
                dmRender::SetConstantLocation(render_constant, location);

                /*
                if (type == dmGraphics::TYPE_FLOAT_MAT4)
                {
                    num_values *= 4;
                    dmRender::SetConstantType(render_constant, dmRenderDDF::MaterialDesc::CONSTANT_TYPE_USER_MATRIX4);
                }
                */

                // Set correct size of the constant (Until the shader builder provides all the default values)
                if (num_values > default_values_capacity)
                {
                    default_values_capacity = num_values;
                    delete[] default_values;
                    default_values = new dmVMath::Vector4[default_values_capacity];
                    memset(default_values, 0, default_values_capacity * sizeof(dmVMath::Vector4));
                }

                dmRender::SetConstantValues(render_constant, default_values, num_values);

                RenderConstant constant;
                constant.m_Constant = render_constant;

                if (type == dmGraphics::TYPE_FLOAT_VEC4)
                {
                    size_t original_size = strlen(name_buffer);
                    dmStrlCat(name_buffer, ".x", sizeof(name_buffer));
                    constant.m_ElementIds[0] = dmHashString64(name_buffer);
                    name_buffer[original_size] = 0;
                    dmStrlCat(name_buffer, ".y", sizeof(name_buffer));
                    constant.m_ElementIds[1] = dmHashString64(name_buffer);
                    name_buffer[original_size] = 0;
                    dmStrlCat(name_buffer, ".z", sizeof(name_buffer));
                    constant.m_ElementIds[2] = dmHashString64(name_buffer);
                    name_buffer[original_size] = 0;
                    dmStrlCat(name_buffer, ".w", sizeof(name_buffer));
                    constant.m_ElementIds[3] = dmHashString64(name_buffer);
                    name_buffer[original_size] = 0;
                }
                else
                {
                    // Clear element ids, otherwise we will compare against
                    // uninitialized values in GetMaterialProgramConstantInfo.
                    constant.m_ElementIds[0] = 0;
                    constant.m_ElementIds[1] = 0;
                    constant.m_ElementIds[2] = 0;
                    constant.m_ElementIds[3] = 0;
                }
                program->m_Constants.Push(constant);
            }
        }

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
}
