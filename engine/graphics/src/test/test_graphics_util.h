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

#ifndef DM_GRAPHICS_TEST_UTIL_H
#define DM_GRAPHICS_TEST_UTIL_H

#include <stdint.h>

#include <dmsdk/graphics/graphics.h>

#include <graphics/graphics_ddf.h>

namespace dmGraphics
{
    // TODO: We should consider making this take a parameter struct instead
    static inline dmGraphics::ShaderDesc MakeDDFShaderDesc(
        dmGraphics::ShaderDesc::Shader* shader,
        dmGraphics::ShaderDesc::ShaderType type,
        dmGraphics::ShaderDesc::ResourceBinding* inputs, uint32_t input_count,
        dmGraphics::ShaderDesc::ResourceBinding* ubos, uint32_t ubos_count,
        dmGraphics::ShaderDesc::ResourceBinding* textures, uint32_t textures_count,
        dmGraphics::ShaderDesc::ResourceTypeInfo* types, uint32_t types_count)
    {
        dmGraphics::ShaderDesc ddf;
        memset(&ddf,0,sizeof(ddf));

        ddf.m_Shaders.m_Data = shader;
        ddf.m_Shaders.m_Count = 1;
        ddf.m_ShaderType = type;

        ddf.m_Reflection.m_Inputs.m_Data  = inputs;
        ddf.m_Reflection.m_Inputs.m_Count = input_count;

        ddf.m_Reflection.m_UniformBuffers.m_Data  = ubos;
        ddf.m_Reflection.m_UniformBuffers.m_Count = ubos_count;

        ddf.m_Reflection.m_Textures.m_Data  = textures;
        ddf.m_Reflection.m_Textures.m_Count = textures_count;

        ddf.m_Reflection.m_Types.m_Data  = types;
        ddf.m_Reflection.m_Types.m_Count = types_count;

        return ddf;
    }

    static inline dmGraphics::ShaderDesc::Shader MakeDDFShader(dmGraphics::ShaderDesc::Language language, const char* data, uint32_t data_size)
    {
        dmGraphics::ShaderDesc::Shader ddf;
        memset(&ddf,0,sizeof(ddf));
        ddf.m_Source.m_Data  = (uint8_t*)data;
        ddf.m_Source.m_Count = data_size;
        ddf.m_Language       = language;
        return ddf;
    }

    static inline void FillResourceBindingTypeIndex(dmGraphics::ShaderDesc::ResourceBinding* res, const char* name, int binding, int type_index)
    {
        res->m_Name                    = name;
        res->m_NameHash                = dmHashString64(name);
        res->m_Binding                 = binding;
        res->m_Type.m_UseTypeIndex     = true;
        res->m_Type.m_Type.m_TypeIndex = type_index;
    }

    static inline void FillResourceBindingUniformBufferTypeIndex(dmGraphics::ShaderDesc::ResourceBinding* res, const char* name, int binding, int index, uint32_t buffer_size)
    {
        FillResourceBindingTypeIndex(res, name, binding, index);
        res->m_Bindinginfo.m_BlockSize = buffer_size;
    }

    static inline void FillResourceBindingType(dmGraphics::ShaderDesc::ResourceBinding* res, const char* name, int binding, dmGraphics::ShaderDesc::ShaderDataType type)
    {
        res->m_Name                     = name;
        res->m_NameHash                 = dmHashString64(name);
        res->m_Binding                  = binding;
        res->m_Type.m_UseTypeIndex      = false;
        res->m_Type.m_Type.m_ShaderType = type;
    }

    // For simplicity, we add a single type as member here to cover most test cases
    static inline void FillShaderResourceWithSingleTypeMember(dmGraphics::ShaderDesc::ResourceTypeInfo* info, const char* name, dmGraphics::ShaderDesc::ShaderDataType type)
    {
        info->m_Name            = name;
        info->m_NameHash        = dmHashString64(name);
        info->m_Members.m_Count = 1;

        info->m_Members.m_Data = new dmGraphics::ShaderDesc::ResourceMember[1];
        memset(info->m_Members.m_Data, 0, sizeof(dmGraphics::ShaderDesc::ResourceMember));

        info->m_Members.m_Data[0].m_Name                     = name;
        info->m_Members.m_Data[0].m_NameHash                 = dmHashString64(name);
        info->m_Members.m_Data[0].m_Type.m_Type.m_ShaderType = type;
    }

    static inline void FillShaderResourceWithMembers(dmGraphics::ShaderDesc::ResourceTypeInfo* info, const char* name, dmGraphics::ShaderDesc::ResourceMember* members, uint32_t members_count)
    {
        info->m_Name            = name;
        info->m_NameHash        = dmHashString64(name);
        info->m_Members.m_Count = members_count;
        info->m_Members.m_Data  = members;
    }

    static inline void CleanupShaderResourceTypeInfos(dmGraphics::ShaderDesc::ResourceTypeInfo* infos, uint32_t count)
    {
        for (int i = 0; i < count; ++i)
        {
            delete[] infos[i].m_Members.m_Data;
        }
    }
}

#endif
