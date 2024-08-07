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
    static inline dmGraphics::TextureType TypeToTextureType(dmGraphics::Type type)
    {
        switch(type)
        {
            case dmGraphics::TYPE_SAMPLER_2D:       return dmGraphics::TEXTURE_TYPE_2D;
            case dmGraphics::TYPE_SAMPLER_2D_ARRAY: return dmGraphics::TEXTURE_TYPE_2D_ARRAY;
            case dmGraphics::TYPE_SAMPLER_CUBE:     return dmGraphics::TEXTURE_TYPE_CUBE_MAP;
            case dmGraphics::TYPE_IMAGE_2D:         return dmGraphics::TEXTURE_TYPE_IMAGE_2D;
            case dmGraphics::TYPE_TEXTURE_2D:       return dmGraphics::TEXTURE_TYPE_TEXTURE_2D;
            case dmGraphics::TYPE_TEXTURE_2D_ARRAY: return dmGraphics::TEXTURE_TYPE_TEXTURE_2D_ARRAY;
            case dmGraphics::TYPE_TEXTURE_CUBE:     return dmGraphics::TEXTURE_TYPE_TEXTURE_CUBE;
            case dmGraphics::TYPE_SAMPLER:          return dmGraphics::TEXTURE_TYPE_SAMPLER;
            default:break;
        }
        assert(0);
        return (dmGraphics::TextureType) -1;
    }

    static inline bool IsContextLanguageGlsl(dmGraphics::ShaderDesc::Language language)
    {
        return language == dmGraphics::ShaderDesc::LANGUAGE_GLSL_SM120 ||
               language == dmGraphics::ShaderDesc::LANGUAGE_GLSL_SM140 ||
               language == dmGraphics::ShaderDesc::LANGUAGE_GLSL_SM330 ||
               language == dmGraphics::ShaderDesc::LANGUAGE_GLSL_SM430 ||
               language == dmGraphics::ShaderDesc::LANGUAGE_GLES_SM100 ||
               language == dmGraphics::ShaderDesc::LANGUAGE_GLES_SM300;
    }

    void GetProgramUniformCount(dmGraphics::HProgram program, uint32_t total_constants_count, uint32_t* constant_count_out, uint32_t* samplers_count_out)
    {
        uint32_t constants_count = 0;
        uint32_t samplers_count  = 0;
        int32_t value_count      = 0;

        dmGraphics::Type type;
        const uint32_t buffer_size = 128;
        char buffer[buffer_size];

        for (uint32_t i = 0; i < total_constants_count; ++i)
        {
            type = (dmGraphics::Type) -1;
            dmGraphics::GetUniformName(program, i, buffer, buffer_size, &type, &value_count);

            if (type == dmGraphics::TYPE_FLOAT_VEC4 || type == dmGraphics::TYPE_FLOAT_MAT4)
            {
                constants_count++;
            }
            else if (dmGraphics::IsTypeTextureType(type))
            {
                samplers_count++;
            }
            else
            {
                dmLogWarning("Type for uniform %s is not supported (%d)", buffer, type);
            }
        }

        *constant_count_out = constants_count;
        *samplers_count_out = samplers_count;
    }

    void FillElementIds(char* buffer, uint32_t buffer_size, dmhash_t element_ids[4])
    {
        size_t original_size = strlen(buffer);
        dmStrlCat(buffer, ".x", buffer_size);
        element_ids[0] = dmHashString64(buffer);
        buffer[original_size] = 0;
        dmStrlCat(buffer, ".y", buffer_size);
        element_ids[1] = dmHashString64(buffer);
        buffer[original_size] = 0;
        dmStrlCat(buffer, ".z", buffer_size);
        element_ids[2] = dmHashString64(buffer);
        buffer[original_size] = 0;
        dmStrlCat(buffer, ".w", buffer_size);
        element_ids[3] = dmHashString64(buffer);
        buffer[original_size] = 0;
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
            dmGraphics::SetTextureParams(texture, s->m_MinFilter, s->m_MagFilter, s->m_UWrap, s->m_VWrap, s->m_MaxAnisotropy);
        }
    }

    void SetProgramConstantValues(dmGraphics::HContext graphics_context, dmGraphics::HProgram program, uint32_t total_constants_count, dmHashTable64<dmGraphics::HUniformLocation>& name_hash_to_location, dmArray<RenderConstant>& constants, dmArray<Sampler>& samplers)
    {
        dmGraphics::Type type;
        const uint32_t buffer_size = 128;
        char buffer[buffer_size];
        int32_t num_values = 0;

        uint32_t default_values_capacity = 0;
        dmVMath::Vector4* default_values = 0;
        uint32_t sampler_index = 0;

        bool program_language_glsl = IsContextLanguageGlsl(dmGraphics::GetProgramLanguage(program));

        for (uint32_t i = 0; i < total_constants_count; ++i)
        {
            uint32_t name_str_length              = dmGraphics::GetUniformName(program, i, buffer, buffer_size, &type, &num_values);
            dmGraphics::HUniformLocation location = dmGraphics::GetUniformLocation(program, buffer);

        #if 0
            dmLogInfo("Uniform[%d]: name=%s, type=%s, num_values=%d, location=%lld", i, buffer, dmGraphics::GetGraphicsTypeLiteral(type), num_values, location);
        #endif

            // DEF-2971-hotfix
            // Previously this check was an assert. In Emscripten 1.38.3 they made changes
            // to how uniforms are collected and reported back from WebGL. Simply speaking
            // in previous Emscripten versions you would get "valid" locations for uniforms
            // that wasn't used, but after the upgrade these unused uniforms will return -1
            // as location instead. The fix here is to avoid asserting on such values, but
            // not saving them in the m_Constants and m_NameHashToLocation structs.
            if (location == dmGraphics::INVALID_UNIFORM_LOCATION)
            {
                continue;
            }

            assert(name_str_length > 0);

            if (program_language_glsl)
            {
                // For uniform arrays, OpenGL returns the name as "uniform[0]",
                // but we want to identify it as the base name instead.
                for (int j = 0; j < name_str_length; ++j)
                {
                    if (buffer[j] == '[')
                    {
                        buffer[j] = 0;
                        break;
                    }
                }
            }

            dmhash_t name_hash = dmHashString64(buffer);

            // We check if we already have a constant registered for this name.
            // This will happen on NON-OPENGL context when there is a constant with the same name
            // in both the vertex and the fragment program. This forces the behavior of constants to be exactly like
            // OpenGL, where a uniform is in global scope between the shader stages.
            //
            // JG: A note here, since the materials have different vertex / fragment constant tables,
            //     we imply that you can have different constant values between them but that is not possible
            //     for OpenGL. For other adapters however, uniforms can either be bound independent or shared regardless of name.
            //     For now we unfortunately have to adhere to how GL works..
            if (!program_language_glsl && name_hash_to_location.Get(name_hash) != 0)
            {
                continue;
            }

            if (type == dmGraphics::TYPE_FLOAT_VEC4 || type == dmGraphics::TYPE_FLOAT_MAT4)
            {
                name_hash_to_location.Put(name_hash, location);

                HConstant render_constant = dmRender::NewConstant(name_hash);
                dmRender::SetConstantLocation(render_constant, location);

                if (type == dmGraphics::TYPE_FLOAT_MAT4)
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

                RenderConstant constant;
                constant.m_Constant = render_constant;

                if (type == dmGraphics::TYPE_FLOAT_VEC4)
                {
                    FillElementIds(buffer, buffer_size, constant.m_ElementIds);
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
                constants.Push(constant);
            }
            else if (dmGraphics::IsTypeTextureType(type))
            {
                name_hash_to_location.Put(name_hash, location);
                Sampler& s           = samplers[sampler_index];
                s.m_UnitValueCount   = num_values;
                s.m_Type             = TypeToTextureType(type);
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
                if (program_language == dmGraphics::ShaderDesc::LANGUAGE_SPIRV)
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
                if (program_language == dmGraphics::ShaderDesc::LANGUAGE_SPIRV)
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
                if (program_language == dmGraphics::ShaderDesc::LANGUAGE_SPIRV)
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
