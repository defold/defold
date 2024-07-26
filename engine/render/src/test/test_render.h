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

#ifndef DM_RENDER_TEST_RENDER_H
#define DM_RENDER_TEST_RENDER_H

#include <stdint.h>

#include <dmsdk/graphics/graphics.h>

#include <graphics/graphics_ddf.h>

static inline dmGraphics::ShaderDesc MakeDDFShaderDesc(dmGraphics::ShaderDesc::Shader* shader,
    dmGraphics::ShaderDesc::ShaderType type,
    dmGraphics::ShaderDesc::ResourceBinding* inputs, uint32_t input_count,
    dmGraphics::ShaderDesc::ResourceBinding* ubos, uint32_t ubos_count)
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

static inline void FillResourceBinding(dmGraphics::ShaderDesc::ResourceBinding* binding, const char* name, dmGraphics::ShaderDesc::ShaderDataType type)
{
    binding->m_Name                     = name;
    binding->m_NameHash                 = dmHashString64(name);
    binding->m_Type.m_Type.m_ShaderType = type;
}

#endif
