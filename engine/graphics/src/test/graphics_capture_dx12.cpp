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

#include <dlib/dstrings.h>
#include <shaderc.h>
#include "test_app_graphics.h"

using namespace dmGraphics;

struct CaptureDX12Stage
{
    dmShaderc::ShaderCompileResult*          m_Result;
    dmArray<ShaderDesc::HLSLResourceMapping> m_Bindings;
    dmArray<ShaderDesc::HLSLShaderOutput>    m_Outputs;

    CaptureDX12Stage() : m_Result(0)
    {
    }

    ~CaptureDX12Stage()
    {
        if (m_Result)
        {
            dmShaderc::FreeShaderCompileResult(m_Result);
        }
    }

};

static bool CompileStage(const ShaderDesc::Shader& source, dmShaderc::ShaderStage stage, CaptureDX12Stage& output, char* error, uint32_t error_size)
{
    dmShaderc::HShaderContext context = dmShaderc::NewShaderContext(stage, source.m_Source.m_Data, source.m_Source.m_Count);
    dmShaderc::HShaderCompiler compiler = dmShaderc::NewShaderCompiler(context, dmShaderc::SHADER_LANGUAGE_HLSL);
    dmShaderc::ShaderCompilerOptions options;
    options.m_Version = 60;
    output.m_Result = dmShaderc::Compile(context, compiler, options);
    bool success = output.m_Result && !output.m_Result->m_LastError[0] && !output.m_Result->m_Data.Empty();
    if (!success)
    {
        dmSnPrintf(error, error_size, "Failed to compile the DX12 %s shader: %s",
                   stage == dmShaderc::SHADER_STAGE_VERTEX ? "vertex" : "fragment",
                   output.m_Result && output.m_Result->m_LastError[0] ? output.m_Result->m_LastError : "Empty shader output");
    }

    dmShaderc::DeleteShaderCompiler(compiler);
    dmShaderc::DeleteShaderContext(context);
    return success;
}

// The native merge takes a contiguous array and patches the bytecode and root parameter indices in place.
static void SwapMergeData(dmShaderc::ShaderCompileResult& a, dmShaderc::ShaderCompileResult& b)
{
    a.m_Data.Swap(b.m_Data);
    a.m_HLSLRootSignature.Swap(b.m_HLSLRootSignature);
    a.m_HLSLResourceMappings.Swap(b.m_HLSLResourceMappings);
}

static void SetShaderData(CaptureDX12Stage& stage, ShaderDesc::Shader* shader)
{
    dmShaderc::ShaderCompileResult& result = *stage.m_Result;
    stage.m_Bindings.EnsureSize(result.m_HLSLResourceMappings.Size());
    for (uint32_t i = 0; i < stage.m_Bindings.Size(); ++i)
    {
        const dmShaderc::HLSLResourceMapping& src = result.m_HLSLResourceMappings[i];
        ShaderDesc::HLSLResourceMapping& dst = stage.m_Bindings[i];
        dst.m_NameHash = src.m_NameHash;
        dst.m_RootParameterIndex = src.m_RootParameterIndex;
        dst.m_Set = src.m_ShaderResourceSet;
        dst.m_Binding = src.m_ShaderResourceBinding;
    }

    stage.m_Outputs.EnsureSize(result.m_HLSLShaderOutputs.Size());
    for (uint32_t i = 0; i < stage.m_Outputs.Size(); ++i)
    {
        const dmShaderc::HLSLShaderOutput& src = result.m_HLSLShaderOutputs[i];
        ShaderDesc::HLSLShaderOutput& dst = stage.m_Outputs[i];
        dst.m_Location = src.m_Location;
        dst.m_ComponentCount = src.m_ComponentCount;
        switch (src.m_Type)
        {
            case dmShaderc::BASE_TYPE_UINT32:
            {
                dst.m_ComponentType = ShaderDesc::HLSLShaderOutput::COMPONENT_TYPE_UINT32;
                break;
            }
            case dmShaderc::BASE_TYPE_INT32:
            {
                dst.m_ComponentType = ShaderDesc::HLSLShaderOutput::COMPONENT_TYPE_SINT32;
                break;
            }
            default:
            {
                dst.m_ComponentType = ShaderDesc::HLSLShaderOutput::COMPONENT_TYPE_FLOAT32;
                break;
            }
        }
    }

    shader->m_Language = ShaderDesc::LANGUAGE_HLSL_60;
    shader->m_Source.m_Data = result.m_Data.Begin();
    shader->m_Source.m_Count = result.m_Data.Size();
    shader->m_HlslResourceMapping.m_Data = stage.m_Bindings.Begin();
    shader->m_HlslResourceMapping.m_Count = stage.m_Bindings.Size();
    shader->m_HlslShaderOutputs.m_Data = stage.m_Outputs.Begin();
    shader->m_HlslShaderOutputs.m_Count = stage.m_Outputs.Size();
}

HProgram NewDX12CaptureProgram(HContext context, const ShaderDesc& spirv, char* error, uint32_t error_size)
{
    CaptureDX12Stage stages[2];
    if (!CompileStage(spirv.m_Shaders[0], dmShaderc::SHADER_STAGE_VERTEX, stages[0], error, error_size) ||
        !CompileStage(spirv.m_Shaders[1], dmShaderc::SHADER_STAGE_FRAGMENT, stages[1], error, error_size))
    {
        return 0;
    }

    dmShaderc::ShaderCompileResult inputs[2];
    SwapMergeData(inputs[0], *stages[0].m_Result);
    SwapMergeData(inputs[1], *stages[1].m_Result);
    dmShaderc::HLSLRootSignature* root = dmShaderc::HLSLMergeRootSignatures(inputs, 2);
    SwapMergeData(inputs[0], *stages[0].m_Result);
    SwapMergeData(inputs[1], *stages[1].m_Result);

    HProgram program = 0;
    if (root->m_LastError[0])
    {
        dmSnPrintf(error, error_size, "Failed to merge the DX12 root signatures: %s", root->m_LastError);
    }
    else
    {
        ShaderDesc::Shader shaders[2] = {};
        ShaderDesc desc = {};
        desc.m_Reflection = spirv.m_Reflection;
        desc.m_Shaders.m_Data = shaders;
        desc.m_Shaders.m_Count = 2;
        desc.m_HlslRootSignature.m_Data = root->m_HLSLRootSignature.Begin();
        desc.m_HlslRootSignature.m_Count = root->m_HLSLRootSignature.Size();
        for (uint32_t i = 0; i < 2; ++i)
        {
            shaders[i].m_ShaderType = spirv.m_Shaders[i].m_ShaderType;
            SetShaderData(stages[i], &shaders[i]);
        }

        program = NewProgram(context, &desc, error, error_size);
    }

    delete root;
    return program;
}
