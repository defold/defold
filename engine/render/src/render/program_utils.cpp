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
    static inline bool IsUniformTypeSupported(dmGraphics::Type type)
    {
        return type == dmGraphics::TYPE_FLOAT_VEC4 || type == dmGraphics::TYPE_FLOAT_MAT4 || dmGraphics::IsTypeTextureType(type) || type == dmGraphics::TYPE_SAMPLER;
    }

    void FillElementIds(const char* name, char* buffer, uint32_t buffer_size, dmhash_t element_ids[4])
    {
        dmSnPrintf(buffer, buffer_size, "%s.x", name);
        element_ids[0] = dmHashString64(buffer);
        dmSnPrintf(buffer, buffer_size, "%s.y", name);
        element_ids[1] = dmHashString64(buffer);
        dmSnPrintf(buffer, buffer_size, "%s.z", name);
        element_ids[2] = dmHashString64(buffer);
        dmSnPrintf(buffer, buffer_size, "%s.w", name);
        element_ids[3] = dmHashString64(buffer);
    }

    int32_t GetProgramSamplerIndex(const dmArray<Sampler>& samplers, dmhash_t name_hash)
    {
        uint32_t num_samplers = samplers.Size();
        for (int i = 0; i < num_samplers; ++i)
        {
            if (samplers[i].m_NameHash == name_hash)
            {
                return i;
            }
        }
        return -1;
    }

    static inline const RenderConstant* GetRenderConstant(const dmArray<RenderConstant>& constants, dmhash_t name_hash)
    {
        uint32_t n = constants.Size();
        for (uint32_t i = 0; i < n; ++i)
        {
            if (GetConstantName(constants[i].m_Constant) == name_hash)
            {
                return &constants[i];
            }
        }
        return 0;
    }

    void SetProgramRenderConstant(const dmArray<RenderConstant>& constants, dmhash_t name_hash, const dmVMath::Vector4* values, uint32_t count)
    {
        const RenderConstant* rc = GetRenderConstant(constants, name_hash);
        if (!rc)
            return;

        uint32_t num_default_values;
        dmVMath::Vector4* constant_values = dmRender::GetConstantValues(rc->m_Constant, &num_default_values);

        // we cannot set more values than are already registered with the program
        if (num_default_values < count)
        {
            count = num_default_values;
        }

        // we musn't set less values than are already registered with the program
        // so we write to the previous buffer
        memcpy(constant_values, values, count * sizeof(dmVMath::Vector4));
    }

    void SetProgramConstantType(const dmArray<RenderConstant>& constants, dmhash_t name_hash, dmRenderDDF::MaterialDesc::ConstantType type)
    {
        const RenderConstant* rc = GetRenderConstant(constants, name_hash);
        if (rc)
        {
            SetConstantType(rc->m_Constant, type);
        }
    }

    bool GetProgramConstant(const dmArray<RenderConstant>& constants, dmhash_t name_hash, HConstant& out_value)
    {
        const RenderConstant* rc = GetRenderConstant(constants, name_hash);
        if (!rc)
        {
            return false;
        }
        out_value = rc->m_Constant;
        return true;
    }

    bool SetProgramSampler(dmArray<Sampler>& samplers, dmHashTable64<dmGraphics::HUniformLocation>& name_hash_to_location, dmhash_t name_hash, uint32_t unit, dmGraphics::TextureWrap u_wrap, dmGraphics::TextureWrap v_wrap, dmGraphics::TextureFilter min_filter, dmGraphics::TextureFilter mag_filter, float max_anisotropy)
    {
        if (unit < samplers.Size() && name_hash != 0)
        {
            dmGraphics::HUniformLocation* location = name_hash_to_location.Get(name_hash);
            if (location)
            {
                Sampler& s        = samplers[unit];
                s.m_NameHash      = name_hash;
                s.m_Location      = *location;
                s.m_UWrap         = u_wrap;
                s.m_VWrap         = v_wrap;
                s.m_MinFilter     = min_filter;
                s.m_MagFilter     = mag_filter;
                s.m_MaxAnisotropy = max_anisotropy;
                return true;
            }
        }
        return false;
    }

    uint32_t GetProgramSamplerUnit(const dmArray<Sampler>& samplers, dmhash_t name_hash)
    {
        uint32_t num_samplers = samplers.Size();
        for (uint32_t i = 0; i < num_samplers; ++i)
        {
            if (samplers[i].m_NameHash == name_hash)
            {
                return i;
            }
        }
        return INVALID_SAMPLER_UNIT;
    }

    HSampler GetProgramSampler(const dmArray<Sampler>& samplers, uint32_t unit)
    {
        if (unit < samplers.Size())
        {
            return (HSampler) &samplers[unit];
        }
        return 0x0;
    }

    void ApplyProgramSampler(dmRender::HRenderContext render_context, HSampler sampler, uint8_t unit, dmGraphics::HTexture texture)
    {
        if (!sampler)
        {
            return;
        }

        Sampler* s = (Sampler*) sampler;
        dmGraphics::HContext graphics_context = dmRender::GetGraphicsContext(render_context);

        if (s->m_Location != -1)
        {
            dmGraphics::SetSampler(graphics_context, s->m_Location, unit);
            dmGraphics::SetTextureParams(graphics_context, texture, s->m_MinFilter, s->m_MagFilter, s->m_UWrap, s->m_VWrap, s->m_MaxAnisotropy);
        }
    }

    void GetProgramUniformCount(dmGraphics::HProgram program, uint32_t total_constants_count, uint32_t* constant_count_out, uint32_t* samplers_count_out)
    {
        uint32_t constants_count = 0;
        uint32_t samplers_count  = 0;

        for (uint32_t i = 0; i < total_constants_count; ++i)
        {
            dmGraphics::Uniform uniform_desc = {};
            dmGraphics::GetUniform(program, i, &uniform_desc);

            if (!IsUniformTypeSupported(uniform_desc.m_Type))
            {
                dmLogWarning("Type for uniform %s is not supported (%d)", uniform_desc.m_Name, uniform_desc.m_Type);
                continue;
            }

            if (uniform_desc.m_Type == dmGraphics::TYPE_FLOAT_VEC4 || uniform_desc.m_Type == dmGraphics::TYPE_FLOAT_MAT4)
            {
                constants_count++;
            }
            else if (dmGraphics::IsTypeTextureType(uniform_desc.m_Type))
            {
                samplers_count++;
            }
            else if (uniform_desc.m_Type == dmGraphics::TYPE_SAMPLER)
            {
                // ignore samplers for now
            }
        }

        *constant_count_out = constants_count;
        *samplers_count_out = samplers_count;
    }

    static Constant* FindConstant(dmArray<RenderConstant>& constants, dmhash_t name_hash)
    {
        uint32_t n = constants.Size();
        for (uint32_t i = 0; i < n; ++i)
        {
            RenderConstant* constant = &constants[i];
            if (constant->m_Constant->m_NameHash == name_hash)
                return constant->m_Constant;
        }
        return 0;
    }

    void SetProgramConstantValues(dmGraphics::HContext graphics_context, dmGraphics::HProgram program, uint32_t total_constants_count, dmHashTable64<dmGraphics::HUniformLocation>& name_hash_to_location, dmArray<RenderConstant>& constants, dmArray<Sampler>& samplers)
    {
        const uint32_t buffer_size = 128;
        char buffer[buffer_size];

        uint32_t default_values_capacity = 0;
        dmVMath::Vector4* default_values = 0;
        uint32_t sampler_index = 0;

        for (uint32_t i = 0; i < total_constants_count; ++i)
        {
            dmGraphics::Uniform uniform_desc;
            dmGraphics::GetUniform(program, i, &uniform_desc);

        #if 0
            dmLogInfo("Uniform[%d]: name=%s, type=%s, num_values=%d, location=%lld",
                i, uniform_desc.m_Name, dmGraphics::GetGraphicsTypeLiteral(uniform_desc.m_Type),
                uniform_desc.m_Count, uniform_desc.m_Location);
        #endif

            // DEF-2971-hotfix
            // Previously this check was an assert. In Emscripten 1.38.3 they made changes
            // to how uniforms are collected and reported back from WebGL. Simply speaking
            // in previous Emscripten versions you would get "valid" locations for uniforms
            // that wasn't used, but after the upgrade these unused uniforms will return -1
            // as location instead. The fix here is to avoid asserting on such values, but
            // not saving them in the m_Constants and m_NameHashToLocation structs.
            if (uniform_desc.m_Location == dmGraphics::INVALID_UNIFORM_LOCATION || !IsUniformTypeSupported(uniform_desc.m_Type))
            {
                continue;
            }

            name_hash_to_location.Put(uniform_desc.m_NameHash, uniform_desc.m_Location);

            uint32_t num_values = uniform_desc.m_Count;

            if (uniform_desc.m_Type == dmGraphics::TYPE_FLOAT_VEC4 || uniform_desc.m_Type == dmGraphics::TYPE_FLOAT_MAT4)
            {
                HConstant render_constant = dmRender::NewConstant(uniform_desc.m_NameHash);

                dmRender::SetConstantLocation(render_constant, uniform_desc.m_Location);

                // We are about to add a duplicate. Make sure to reuse the data
                HConstant prev_constant = FindConstant(constants, uniform_desc.m_NameHash);
                if (prev_constant != 0)
                {
                    uint32_t prev_num_values;
                    dmVMath::Vector4* prev_values = GetConstantValues(prev_constant, &prev_num_values);

                    dmRender::SetConstantValuesRef(render_constant, prev_values, prev_num_values);
                    dmRender::SetConstantType(render_constant, GetConstantType(prev_constant));
                }
                else
                {
                    if (uniform_desc.m_Type == dmGraphics::TYPE_FLOAT_MAT4)
                    {
                        num_values *= 4;
                        dmRender::SetConstantType(render_constant, dmRenderDDF::MaterialDesc::CONSTANT_TYPE_USER_MATRIX4);
                    }

                    // Set correct size of the constant (Until the shader builder provides all the default values)
                    if (num_values > default_values_capacity)
                    {
                        default_values_capacity = num_values;
                        delete[] default_values;
                        default_values = new dmVMath::Vector4[default_values_capacity];
                        memset(default_values, 0, default_values_capacity * sizeof(dmVMath::Vector4));
                    }

                    dmRender::SetConstantValues(render_constant, default_values, num_values);
                }

                RenderConstant constant;
                constant.m_Constant = render_constant;

                if (uniform_desc.m_Type == dmGraphics::TYPE_FLOAT_VEC4)
                {
                    FillElementIds(uniform_desc.m_Name, buffer, buffer_size, constant.m_ElementIdsName);
                }
                else
                {
                    // Clear element ids, otherwise we will compare against
                    // uninitialized values in GetMaterialProgramConstantInfo.
                    memset(constant.m_ElementIdsName, 0, sizeof(constant.m_ElementIdsName));
                }
                constants.Push(constant);
            }
            else if (dmGraphics::IsTypeTextureType(uniform_desc.m_Type))
            {
                Sampler& s         = samplers[sampler_index];
                s.m_UnitValueCount = num_values;
                s.m_Type           = TypeToTextureType(uniform_desc.m_Type);
                sampler_index++;
            }
        }

        delete[] default_values;
    }

    void SetProgramConstant(dmRender::HRenderContext render_context, dmGraphics::HContext graphics_context, const dmVMath::Matrix4& world_matrix, const dmVMath::Matrix4& texture_matrix, dmGraphics::ShaderDesc::Language program_language, dmRenderDDF::MaterialDesc::ConstantType type, dmGraphics::HProgram program, dmGraphics::HUniformLocation location, HConstant constant)
    {
        switch (type)
        {
            case dmRenderDDF::MaterialDesc::CONSTANT_TYPE_USER:
            {
                uint32_t num_values;
                dmVMath::Vector4* values = GetConstantValues(constant, &num_values);
                dmGraphics::SetConstantV4(graphics_context, values, num_values, location);
                break;
            }
            case dmRenderDDF::MaterialDesc::CONSTANT_TYPE_USER_MATRIX4:
            {
                uint32_t num_values;
                dmVMath::Vector4* values = GetConstantValues(constant, &num_values);
                dmGraphics::SetConstantM4(graphics_context, values, num_values / 4, location);
                break;
            }
            case dmRenderDDF::MaterialDesc::CONSTANT_TYPE_VIEWPROJ:
            {
                if (program_language == dmGraphics::ShaderDesc::LANGUAGE_SPIRV ||
                    program_language == dmGraphics::ShaderDesc::LANGUAGE_WGSL ||
                    program_language == dmGraphics::ShaderDesc::LANGUAGE_HLSL_51 ||
                    program_language == dmGraphics::ShaderDesc::LANGUAGE_HLSL_50)
                {
                    Matrix4 ndc_matrix = Matrix4::identity();
                    ndc_matrix.setElem(2, 2, 0.5f );
                    ndc_matrix.setElem(3, 2, 0.5f );
                    const Matrix4 view_projection = ndc_matrix * render_context->m_ViewProj;
                    dmGraphics::SetConstantM4(graphics_context, (Vector4*)&view_projection, 1, location);
                }
                else
                {
                    dmGraphics::SetConstantM4(graphics_context, (Vector4*)&render_context->m_ViewProj, 1, location);
                }
                break;
            }
            case dmRenderDDF::MaterialDesc::CONSTANT_TYPE_WORLD:
            {
                dmGraphics::SetConstantM4(graphics_context, (Vector4*)&world_matrix, 1, location);
                break;
            }
            case dmRenderDDF::MaterialDesc::CONSTANT_TYPE_TEXTURE:
            {
                dmGraphics::SetConstantM4(graphics_context, (Vector4*)&texture_matrix, 1, location);
                break;
            }
            case dmRenderDDF::MaterialDesc::CONSTANT_TYPE_VIEW:
            {
                dmGraphics::SetConstantM4(graphics_context, (Vector4*)&render_context->m_View, 1, location);
                break;
            }
            case dmRenderDDF::MaterialDesc::CONSTANT_TYPE_PROJECTION:
            {
                // Vulkan NDC is [0..1] for z, so we must transform
                // the projection before setting the constant.
                if (program_language == dmGraphics::ShaderDesc::LANGUAGE_SPIRV ||
                    program_language == dmGraphics::ShaderDesc::LANGUAGE_WGSL ||
                    program_language == dmGraphics::ShaderDesc::LANGUAGE_HLSL_51 ||
                    program_language == dmGraphics::ShaderDesc::LANGUAGE_HLSL_50)
                {
                    Matrix4 ndc_matrix = Matrix4::identity();
                    ndc_matrix.setElem(2, 2, 0.5f );
                    ndc_matrix.setElem(3, 2, 0.5f );
                    const Matrix4 proj = ndc_matrix * render_context->m_Projection;
                    dmGraphics::SetConstantM4(graphics_context, (Vector4*)&proj, 1, location);
                }
                else
                {
                    dmGraphics::SetConstantM4(graphics_context, (Vector4*)&render_context->m_Projection, 1, location);
                }
                break;
            }
            case dmRenderDDF::MaterialDesc::CONSTANT_TYPE_NORMAL:
            {
                {
                    // normalT = transp(inv(view * world))
                    Matrix4 normalT = render_context->m_View * world_matrix;
                    // The world transform might include non-uniform scaling, which breaks the orthogonality of the combined model-view transform
                    // It is always affine however
                    normalT = affineInverse(normalT);
                    normalT = transpose(normalT);
                    dmGraphics::SetConstantM4(graphics_context, (Vector4*)&normalT, 1, location);
                }
                break;
            }
            case dmRenderDDF::MaterialDesc::CONSTANT_TYPE_WORLDVIEW:
            {
                {
                    Matrix4 world_view = render_context->m_View * world_matrix;
                    dmGraphics::SetConstantM4(graphics_context, (Vector4*)&world_view, 1, location);
                }
                break;
            }
            case dmRenderDDF::MaterialDesc::CONSTANT_TYPE_WORLDVIEWPROJ:
            {
                if (program_language == dmGraphics::ShaderDesc::LANGUAGE_SPIRV ||
                    program_language == dmGraphics::ShaderDesc::LANGUAGE_WGSL ||
                    program_language == dmGraphics::ShaderDesc::LANGUAGE_HLSL_51 ||
                    program_language == dmGraphics::ShaderDesc::LANGUAGE_HLSL_50)
                {
                    Matrix4 ndc_matrix = Matrix4::identity();
                    ndc_matrix.setElem(2, 2, 0.5f );
                    ndc_matrix.setElem(3, 2, 0.5f );
                    const Matrix4 world_view_projection = ndc_matrix * render_context->m_ViewProj * world_matrix;
                    dmGraphics::SetConstantM4(graphics_context, (Vector4*)&world_view_projection, 1, location);
                }
                else
                {
                    const Matrix4 world_view_projection = render_context->m_ViewProj * world_matrix;
                    dmGraphics::SetConstantM4(graphics_context, (Vector4*)&world_view_projection, 1, location);
                }
                break;
            }
        }
    }

}
