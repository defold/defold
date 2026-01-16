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

#ifndef DM_GRAPHICS_TEST_APP_GRAPHICS_H
#define DM_GRAPHICS_TEST_APP_GRAPHICS_H

#include "../graphics.h"
#include "../graphics_private.h"

enum BindingType
{
    BINDING_TYPE_INPUT,
    BINDING_TYPE_OUTPUT,
    BINDING_TYPE_TEXTURE,
    BINDING_TYPE_UNIFORM_BUFFER,
    BINDING_TYPE_STORAGE_BUFFER,
};

static inline void AddShader(dmGraphics::ShaderDesc* desc, dmGraphics::ShaderDesc::Language language, dmGraphics::ShaderDesc::ShaderType shader_type, uint8_t* source, int source_size)
{
    if (desc->m_Shaders.m_Data == 0)
    {
        desc->m_Shaders.m_Data = (dmGraphics::ShaderDesc::Shader*) malloc(sizeof(dmGraphics::ShaderDesc::Shader) * (desc->m_Shaders.m_Count + 1));
    }
    else
    {
        desc->m_Shaders.m_Data = (dmGraphics::ShaderDesc::Shader*) realloc(desc->m_Shaders.m_Data, sizeof(dmGraphics::ShaderDesc::Shader) * (desc->m_Shaders.m_Count + 1));
    }

    dmGraphics::ShaderDesc::Shader* shader = desc->m_Shaders.m_Data + desc->m_Shaders.m_Count;
    memset(shader, 0, sizeof(dmGraphics::ShaderDesc::Shader));
    desc->m_Shaders.m_Count++;

    shader->m_Language       = language;
    shader->m_ShaderType     = shader_type;
    shader->m_Source.m_Data  = (uint8_t*) source;
    shader->m_Source.m_Count = source_size;
}

static inline void AddShaderResource(dmGraphics::ShaderDesc* desc, const char* name, dmGraphics::ShaderDesc::ShaderDataType shader_type, int type_index, uint32_t binding, uint32_t set, BindingType binding_type, uint8_t stage_flags)
{
    dmGraphics::ShaderDesc::ResourceBinding** data = 0;
    uint32_t* count = 0;

    switch(binding_type)
    {
    case BINDING_TYPE_INPUT:
        data = &desc->m_Reflection.m_Inputs.m_Data;
        count = &desc->m_Reflection.m_Inputs.m_Count;
        break;
    case BINDING_TYPE_OUTPUT:
        data = &desc->m_Reflection.m_Outputs.m_Data;
        count = &desc->m_Reflection.m_Outputs.m_Count;
        break;
    case BINDING_TYPE_TEXTURE:
        data = &desc->m_Reflection.m_Textures.m_Data;
        count = &desc->m_Reflection.m_Textures.m_Count;
        break;
    case BINDING_TYPE_UNIFORM_BUFFER:
        data = &desc->m_Reflection.m_UniformBuffers.m_Data;
        count = &desc->m_Reflection.m_UniformBuffers.m_Count;
        break;
    case BINDING_TYPE_STORAGE_BUFFER:
        data = &desc->m_Reflection.m_StorageBuffers.m_Data;
        count = &desc->m_Reflection.m_StorageBuffers.m_Count;
        break;
    }

    if (*count == 0)
    {
        *data = (dmGraphics::ShaderDesc::ResourceBinding*) malloc(sizeof(dmGraphics::ShaderDesc::ResourceBinding));
    }
    else
    {
        *data = (dmGraphics::ShaderDesc::ResourceBinding*) realloc(data, sizeof(dmGraphics::ShaderDesc::ResourceBinding) * (*count + 1));
    }

    dmGraphics::ShaderDesc::ResourceBinding* res = *data + *count;
    memset(res, 0, sizeof(dmGraphics::ShaderDesc::ResourceBinding));

    *count = *count + 1;

    res->m_Name                     = name;
    res->m_NameHash                 = dmHashString64(name);
    res->m_StageFlags               = stage_flags;
    res->m_Binding                  = binding;
    res->m_Set                      = set;
    res->m_Type.m_Type.m_ShaderType = shader_type;

    if (type_index != -1)
    {
        res->m_Type.m_Type.m_TypeIndex = type_index;
        res->m_Type.m_UseTypeIndex     = 1;
    }
}

static inline void AddShaderResource(dmGraphics::ShaderDesc* desc, const char* name, dmGraphics::ShaderDesc::ShaderDataType shader_type, uint32_t binding, uint32_t set, BindingType binding_type, uint8_t stage_flags)
{
    AddShaderResource(desc, name, shader_type, -1, binding, set, binding_type, stage_flags);
}

static inline void AddShaderResource(dmGraphics::ShaderDesc* desc, const char* name, int type_index, uint32_t binding, uint32_t set, BindingType binding_type, uint8_t stage_flags)
{
    AddShaderResource(desc, name, (dmGraphics::ShaderDesc::ShaderDataType) -1, type_index, binding, set, binding_type, stage_flags);
}

static inline dmGraphics::ShaderDesc::ResourceTypeInfo* AddShaderType(dmGraphics::ShaderDesc* desc, const char* name)
{
    if (desc->m_Reflection.m_Types.m_Data == 0)
    {
        desc->m_Reflection.m_Types.m_Data = (dmGraphics::ShaderDesc::ResourceTypeInfo*) malloc(sizeof(dmGraphics::ShaderDesc::ResourceTypeInfo) * (desc->m_Reflection.m_Types.m_Count + 1));
    }
    else
    {
        desc->m_Reflection.m_Types.m_Data = (dmGraphics::ShaderDesc::ResourceTypeInfo*) realloc(desc->m_Reflection.m_Types.m_Data, sizeof(dmGraphics::ShaderDesc::ResourceTypeInfo) * (desc->m_Reflection.m_Types.m_Count + 1));
    }

    dmGraphics::ShaderDesc::ResourceTypeInfo* type_info = desc->m_Reflection.m_Types.m_Data + desc->m_Reflection.m_Types.m_Count;
    memset(type_info, 0, sizeof(dmGraphics::ShaderDesc::ResourceTypeInfo));
    desc->m_Reflection.m_Types.m_Count++;

    type_info->m_Name = name;
    type_info->m_NameHash = dmHashString64(name);
    return type_info;
}

static inline void AddShaderTypeMember(dmGraphics::ShaderDesc* desc, dmGraphics::ShaderDesc::ResourceTypeInfo* type_info, const char* name, dmGraphics::ShaderDesc::ShaderDataType type, int type_index, int offset, int element_count)
{
    if (type_info->m_Members.m_Data == 0)
    {
        type_info->m_Members.m_Data = (dmGraphics::ShaderDesc::ResourceMember*) malloc(sizeof(dmGraphics::ShaderDesc::ResourceMember));

    }
    else
    {
        type_info->m_Members.m_Data = (dmGraphics::ShaderDesc::ResourceMember*) realloc(type_info->m_Members.m_Data, sizeof(dmGraphics::ShaderDesc::ResourceMember) * (type_info->m_Members.m_Count + 1));
    }
    dmGraphics::ShaderDesc::ResourceMember* member = type_info->m_Members.m_Data + type_info->m_Members.m_Count;
    memset(member, 0, sizeof(dmGraphics::ShaderDesc::ResourceMember));
    type_info->m_Members.m_Count++;

    member->m_Name = name;
    member->m_NameHash = dmHashString64(name);
    member->m_Offset = offset;
    member->m_ElementCount = element_count;
    member->m_Type.m_Type.m_ShaderType = type;

    if (type_index != -1)
    {
        member->m_Type.m_Type.m_TypeIndex = type_index;
        member->m_Type.m_UseTypeIndex = 1;
    }
}

static inline void AddShaderTypeMember(dmGraphics::ShaderDesc* desc, dmGraphics::ShaderDesc::ResourceTypeInfo* type_info, const char* name, dmGraphics::ShaderDesc::ShaderDataType type, int offset, int element_count)
{
    AddShaderTypeMember(desc, type_info, name, type, -1, offset, element_count);
}

static inline void AddShaderTypeMember(dmGraphics::ShaderDesc* desc, dmGraphics::ShaderDesc::ResourceTypeInfo* type_info, const char* name, int type_index, int offset, int element_count)
{
    AddShaderTypeMember(desc, type_info, name, (dmGraphics::ShaderDesc::ShaderDataType) -1, type_index, offset, element_count);
}

static inline void DeleteShaderDesc(dmGraphics::ShaderDesc* desc)
{
#define FREE_IF_SIZE_NOT_ZERO(x) if (x.m_Count > 0) free(x.m_Data);
    FREE_IF_SIZE_NOT_ZERO(desc->m_Reflection.m_Inputs);
    FREE_IF_SIZE_NOT_ZERO(desc->m_Reflection.m_Textures);
    FREE_IF_SIZE_NOT_ZERO(desc->m_Reflection.m_Outputs);
    FREE_IF_SIZE_NOT_ZERO(desc->m_Shaders);
    FREE_IF_SIZE_NOT_ZERO(desc->m_Reflection.m_UniformBuffers);
    FREE_IF_SIZE_NOT_ZERO(desc->m_Reflection.m_StorageBuffers);
    FREE_IF_SIZE_NOT_ZERO(desc->m_Reflection.m_Types);
#undef FREE_IF_SIZE_NOT_ZERO

    free(desc);
}

#endif // DM_GRAPHICS_TEST_APP_GRAPHICS_H
