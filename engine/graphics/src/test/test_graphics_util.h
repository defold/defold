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
                delete[] m_Types[j].m_Members.m_Data;
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
            ShaderDesc::ResourceTypeInfo info = {};
            info.m_Name            = name;
            info.m_NameHash        = dmHashString64(name);
            info.m_Members.m_Data  = new ShaderDesc::ResourceMember[1];
            info.m_Members.m_Count = 1;
            memset(info.m_Members.m_Data, 0, sizeof(info.m_Members.m_Data[0])*info.m_Members.m_Count);

            ShaderDesc::ResourceMember* member = &info.m_Members.m_Data[0];
            member->m_Name                     = name;
            member->m_NameHash                 = dmHashString64(name);
            member->m_Type.m_Type.m_ShaderType = data_type;

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
}

#endif
