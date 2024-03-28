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
            else if (type == dmGraphics::TYPE_SAMPLER_2D || type == dmGraphics::TYPE_SAMPLER_CUBE || type == dmGraphics::TYPE_SAMPLER_2D_ARRAY)
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

    static inline bool IsContextLanguageGlsl(dmGraphics::ShaderDesc::Language language)
    {
        return language == dmGraphics::ShaderDesc::LANGUAGE_GLSL_SM120 ||
               language == dmGraphics::ShaderDesc::LANGUAGE_GLSL_SM140 ||
               language == dmGraphics::ShaderDesc::LANGUAGE_GLSL_SM430 ||
               language == dmGraphics::ShaderDesc::LANGUAGE_GLES_SM100 ||
               language == dmGraphics::ShaderDesc::LANGUAGE_GLES_SM300;
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

    void SetMaterialConstantValues(dmGraphics::HContext graphics_context, dmGraphics::HProgram program, uint32_t total_constants_count, dmHashTable64<dmGraphics::HUniformLocation>& name_hash_to_location, dmArray<RenderConstant>& constants, dmArray<Sampler>& samplers)
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
            if (location == dmGraphics::INVALID_UNIFORM_LOCATION) {
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
                } else {
                    // Clear element ids, otherwise we will compare against
                    // uninitialized values in GetMaterialProgramConstantInfo.
                    constant.m_ElementIds[0] = 0;
                    constant.m_ElementIds[1] = 0;
                    constant.m_ElementIds[2] = 0;
                    constant.m_ElementIds[3] = 0;
                }
                constants.Push(constant);
            }
            else if (type == dmGraphics::TYPE_SAMPLER_2D || type == dmGraphics::TYPE_SAMPLER_CUBE || type == dmGraphics::TYPE_SAMPLER_2D_ARRAY)
            {
                name_hash_to_location.Put(name_hash, location);
                Sampler& s           = samplers[sampler_index];
                s.m_UnitValueCount   = num_values;

                switch(type)
                {
                    case dmGraphics::TYPE_SAMPLER_2D:
                        s.m_Type = dmGraphics::TEXTURE_TYPE_2D;
                        break;
                    case dmGraphics::TYPE_SAMPLER_2D_ARRAY:
                        s.m_Type = dmGraphics::TEXTURE_TYPE_2D_ARRAY;
                        break;
                    case dmGraphics::TYPE_SAMPLER_CUBE:
                        s.m_Type = dmGraphics::TEXTURE_TYPE_CUBE_MAP;
                        break;
                    default: assert(0);
                }
                sampler_index++;
            }
        }

        delete[] default_values;
    }
}
