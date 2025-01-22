// Copyright 2020-2025 The Defold Foundation
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
    int GetShaderStageFlags(ShaderDesc::ShaderType type)
    {
        switch(type)
        {
        case ShaderDesc::SHADER_TYPE_VERTEX:
            return SHADER_STAGE_FLAG_VERTEX;
        case ShaderDesc::SHADER_TYPE_FRAGMENT:
            return SHADER_STAGE_FLAG_FRAGMENT;
        case ShaderDesc::SHADER_TYPE_COMPUTE:
            return SHADER_STAGE_FLAG_COMPUTE;
        }
        return 0;
    }

    class ShaderDescBuilder
    {
    public:
        ShaderDescBuilder()
        {
            memset(this, 0, sizeof(*this));
        }

        ~ShaderDescBuilder()
        {
            for (int j = 0; j < m_Types.Size(); ++j)
            {
                delete m_Types[j].m_Members.m_Data;
            }
        }

        void AddShader(ShaderDesc::ShaderType type, ShaderDesc::Language language, const char* data, uint32_t data_size)
        {
            ShaderDesc::Shader shader = {};
            shader.m_Source.m_Data  = (uint8_t*) data;
            shader.m_Source.m_Count = data_size;
            shader.m_Language       = language;
            shader.m_ShaderType     = type;

            m_Shaders.OffsetCapacity(1);
            m_Shaders.Push(shader);

            UpdateDDFPointers();
        }

        void AddInput(ShaderDesc::ShaderType type, const char* name, int binding, ShaderDesc::ShaderDataType data_type)
        {
            ShaderDesc::ResourceBinding res = {};
            res.m_Name                     = name;
            res.m_NameHash                 = dmHashString64(name);
            res.m_Binding                  = binding;
            res.m_Type.m_UseTypeIndex      = false;
            res.m_Type.m_Type.m_ShaderType = data_type;
            res.m_StageFlags               = GetShaderStageFlags(type);

            m_Inputs.OffsetCapacity(1);
            m_Inputs.Push(res);

            UpdateDDFPointers();
        }

        void AddUniform(const char* name, int binding, int index)
        {
            ShaderDesc::ResourceBinding res = {};
            res.m_Name                    = name;
            res.m_NameHash                = dmHashString64(name);
            res.m_Binding                 = binding;
            res.m_Type.m_UseTypeIndex     = true;
            res.m_Type.m_Type.m_TypeIndex = index;

            m_UniformBuffers.OffsetCapacity(1);
            m_UniformBuffers.Push(res);

            UpdateDDFPointers();
        }

        void AddUniformBuffer(const char* name, int binding, int index, uint32_t buffer_size)
        {
            ShaderDesc::ResourceBinding res = {};
            res.m_Name                    = name;
            res.m_NameHash                = dmHashString64(name);
            res.m_Binding                 = binding;
            res.m_Type.m_UseTypeIndex     = true;
            res.m_Type.m_Type.m_TypeIndex = index;
            res.m_Bindinginfo.m_BlockSize = buffer_size;

            m_UniformBuffers.OffsetCapacity(1);
            m_UniformBuffers.Push(res);

            UpdateDDFPointers();
        }

        void AddTexture(const char* name, int binding, ShaderDesc::ShaderDataType data_type)
        {
            ShaderDesc::ResourceBinding res = {};
            res.m_Name                     = name;
            res.m_NameHash                 = dmHashString64(name);
            res.m_Binding                  = binding;
            res.m_Type.m_UseTypeIndex      = false;
            res.m_Type.m_Type.m_ShaderType = data_type;

            m_Textures.OffsetCapacity(1);
            m_Textures.Push(res);

            UpdateDDFPointers();
        }

        void AddTypeMember(const char* name, ShaderDesc::ShaderDataType data_type)
        {
            ShaderDesc::ResourceMember* member = new ShaderDesc::ResourceMember();
            member->m_Name                     = name;
            member->m_NameHash                 = dmHashString64(name);
            member->m_Type.m_Type.m_ShaderType = data_type;

            ShaderDesc::ResourceTypeInfo info = {};
            info.m_Name            = name;
            info.m_NameHash        = dmHashString64(name);
            info.m_Members.m_Data  = member;
            info.m_Members.m_Count = 1;

            m_Types.OffsetCapacity(1);
            m_Types.Push(info);

            UpdateDDFPointers();
        }

        void AddTypeMemberWithMembers(const char* name, ShaderDesc::ResourceMember* members, uint32_t member_count)
        {
            ShaderDesc::ResourceTypeInfo info = {};
            info.m_Name            = name;
            info.m_NameHash        = dmHashString64(name);

            info.m_Members.m_Data = new ShaderDesc::ResourceMember[member_count];
            info.m_Members.m_Count = member_count;
            memcpy(info.m_Members.m_Data, members, sizeof(ShaderDesc::ResourceMember) * member_count);

            m_Types.OffsetCapacity(1);
            m_Types.Push(info);

            UpdateDDFPointers();
        }

        ShaderDesc* Get()
        {
            return &m_DDF;
        }

    private:

        void UpdateDDFPointers()
        {
            m_DDF.m_Shaders.m_Data = m_Shaders.Begin();
            m_DDF.m_Shaders.m_Count = m_Shaders.Size();

            m_DDF.m_Reflection.m_Inputs.m_Data = m_Inputs.Begin();
            m_DDF.m_Reflection.m_Inputs.m_Count = m_Inputs.Size();
            m_DDF.m_Reflection.m_UniformBuffers.m_Data = m_UniformBuffers.Begin();
            m_DDF.m_Reflection.m_UniformBuffers.m_Count = m_UniformBuffers.Size();
            m_DDF.m_Reflection.m_Textures.m_Data = m_Textures.Begin();
            m_DDF.m_Reflection.m_Textures.m_Count = m_Textures.Size();
            m_DDF.m_Reflection.m_Types.m_Data = m_Types.Begin();
            m_DDF.m_Reflection.m_Types.m_Count = m_Types.Size();
        }

        dmArray<ShaderDesc::Shader>           m_Shaders;
        dmArray<ShaderDesc::ResourceBinding>  m_Inputs;
        dmArray<ShaderDesc::ResourceBinding>  m_UniformBuffers;
        dmArray<ShaderDesc::ResourceBinding>  m_Textures;
        dmArray<ShaderDesc::ResourceTypeInfo> m_Types;

        ShaderDesc m_DDF;
    };

    /*
    static inline ShaderDesc MakeDDFShaderDesc(ShaderDesc::Shader* shaders, uint32_t shader_count)
    {
        ShaderDesc ddf;
        memset(&ddf,0,sizeof(ddf));

        ddf.m_Shaders.m_Data = shaders;
        ddf.m_Shaders.m_Count = shader_count;

        ddf.m_Reflection.m_Data = new ShaderDesc::ShaderReflection[2];
        ddf.m_Reflection.m_Count = 2;
        return ddf;
    }

    static inline void AddShaderReflection(
        ShaderDesc* desc,
        ShaderDesc::ShaderType type,
        ShaderDesc::ResourceBinding* inputs, uint32_t input_count,
        ShaderDesc::ResourceBinding* ubos, uint32_t ubos_count,
        ShaderDesc::ResourceBinding* textures, uint32_t textures_count,
        ShaderDesc::ResourceTypeInfo* types, uint32_t types_count)
    {
        int index = 0;
        switch(type)
        {
        case ShaderDesc::SHADER_TYPE_VERTEX:
        case ShaderDesc::SHADER_TYPE_COMPUTE:
            index = 0;
            break;
        case ShaderDesc::SHADER_TYPE_FRAGMENT:
            index = 1;
            break;
        }

        desc->m_Reflection[index].m_Inputs.m_Data  = inputs;
        desc->m_Reflection[index].m_Inputs.m_Count = input_count;
        desc->m_Reflection[index].m_UniformBuffers.m_Data  = ubos;
        desc->m_Reflection[index].m_UniformBuffers.m_Count = ubos_count;
        desc->m_Reflection[index].m_Textures.m_Data  = textures;
        desc->m_Reflection[index].m_Textures.m_Count = textures_count;
        desc->m_Reflection[index].m_Types.m_Data  = types;
        desc->m_Reflection[index].m_Types.m_Count = types_count;
    }

    static inline ShaderDesc::Shader MakeDDFShader(ShaderDesc::Language language, ShaderDesc::ShaderType type, const char* data, uint32_t data_size)
    {
        ShaderDesc::Shader ddf;
        memset(&ddf,0,sizeof(ddf));
        ddf.m_Source.m_Data  = (uint8_t*)data;
        ddf.m_Source.m_Count = data_size;
        ddf.m_Language       = language;
        ddf.m_ShaderType     = type;
        return ddf;
    }

    static inline void FillResourceBindingTypeIndex(ShaderDesc::ResourceBinding* res, const char* name, int binding, int type_index)
    {
        res->m_Name                    = name;
        res->m_NameHash                = dmHashString64(name);
        res->m_Binding                 = binding;
        res->m_Type.m_UseTypeIndex     = true;
        res->m_Type.m_Type.m_TypeIndex = type_index;
    }

    static inline void FillResourceBindingUniformBufferTypeIndex(ShaderDesc::ResourceBinding* res, const char* name, int binding, int index, uint32_t buffer_size)
    {
        FillResourceBindingTypeIndex(res, name, binding, index);
        res->m_Bindinginfo.m_BlockSize = buffer_size;
    }

    static inline void FillResourceBindingType(ShaderDesc::ResourceBinding* res, const char* name, int binding, ShaderDesc::ShaderDataType type)
    {
        res->m_Name                     = name;
        res->m_NameHash                 = dmHashString64(name);
        res->m_Binding                  = binding;
        res->m_Type.m_UseTypeIndex      = false;
        res->m_Type.m_Type.m_ShaderType = type;
    }

    // For simplicity, we add a single type as member here to cover most test cases
    static inline void FillShaderResourceWithSingleTypeMember(ShaderDesc::ResourceTypeInfo* info, const char* name, ShaderDesc::ShaderDataType type)
    {
        info->m_Name            = name;
        info->m_NameHash        = dmHashString64(name);
        info->m_Members.m_Count = 1;

        info->m_Members.m_Data = new ShaderDesc::ResourceMember[1];
        memset(info->m_Members.m_Data, 0, sizeof(ShaderDesc::ResourceMember));

        info->m_Members.m_Data[0].m_Name                     = name;
        info->m_Members.m_Data[0].m_NameHash                 = dmHashString64(name);
        info->m_Members.m_Data[0].m_Type.m_Type.m_ShaderType = type;
    }

    static inline void FillShaderResourceWithMembers(ShaderDesc::ResourceTypeInfo* info, const char* name, ShaderDesc::ResourceMember* members, uint32_t members_count)
    {
        info->m_Name            = name;
        info->m_NameHash        = dmHashString64(name);
        info->m_Members.m_Count = members_count;
        info->m_Members.m_Data  = members;
    }

    static inline void CleanupShaderResourceTypeInfos(ShaderDesc::ResourceTypeInfo* infos, uint32_t count)
    {
        for (int i = 0; i < count; ++i)
        {
            delete[] infos[i].m_Members.m_Data;
        }
    }

    static inline void DestroyDDFShader(ShaderDesc& shader)
    {
        delete shader.m_Reflection.m_Data;
    }
    */
}

#endif
