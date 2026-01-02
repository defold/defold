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

#include <stdlib.h>
#include <string.h>

#include "shaderc_private.h"

#include <dlib/log.h>

namespace dmShaderc
{
	const ShaderReflection* GetReflection(HShaderContext context)
    {
        return &context->m_Reflection;
    }

    static void SetResourceStageFlagsForResource(dmArray<ShaderResource>& resources, uint64_t name_hash, uint8_t stage_flags)
    {
        for (int i = 0; i < resources.Size(); ++i)
        {
            if (resources[i].m_NameHash == name_hash)
            {
                resources[i].m_StageFlags = stage_flags;
            }
        }
    }

    void SetResourceStageFlags(HShaderContext context, uint64_t name_hash, uint8_t stage_flags)
    {
        SetResourceStageFlagsForResource(context->m_Reflection.m_Inputs, name_hash, stage_flags);
        SetResourceStageFlagsForResource(context->m_Reflection.m_Outputs, name_hash, stage_flags);
        SetResourceStageFlagsForResource(context->m_Reflection.m_UniformBuffers, name_hash, stage_flags);
        SetResourceStageFlagsForResource(context->m_Reflection.m_StorageBuffers, name_hash, stage_flags);
        SetResourceStageFlagsForResource(context->m_Reflection.m_Textures, name_hash, stage_flags);
    }

    HShaderCompiler NewShaderCompiler(HShaderContext context, ShaderLanguage language)
    {
        if (language == SHADER_LANGUAGE_SPIRV)
        {
            return (HShaderCompiler) NewShaderCompilerSPIRV(context);
        }
        return (HShaderCompiler) NewShaderCompilerSPVC(context, language);
    }

    void DeleteShaderCompiler(HShaderCompiler compiler)
    {
        if (compiler->m_Language == SHADER_LANGUAGE_SPIRV)
        {
            DeleteShaderCompilerSPIRV((ShaderCompilerSPIRV*) compiler);
        }
        else
        {
            DeleteShaderCompilerSPVC((ShaderCompilerSPVC*) compiler);
        }
    }

    void SetResourceLocation(HShaderContext context, HShaderCompiler compiler, uint64_t name_hash, uint8_t location)
    {
        if (compiler->m_Language == SHADER_LANGUAGE_SPIRV)
        {
            SetResourceLocationSPIRV(context, (ShaderCompilerSPIRV*) compiler, name_hash, location);
        }
        else
        {
            SetResourceLocationSPVC(context, (ShaderCompilerSPVC*) compiler, name_hash, location);
        }
    }

    void SetResourceBinding(HShaderContext context, HShaderCompiler compiler, uint64_t name_hash, uint8_t binding)
    {
        if (compiler->m_Language == SHADER_LANGUAGE_SPIRV)
        {
            SetResourceBindingSPIRV(context, (ShaderCompilerSPIRV*) compiler, name_hash, binding);
        }
        else
        {
            SetResourceBindingSPVC(context, (ShaderCompilerSPVC*) compiler, name_hash, binding);
        }
    }

    void SetResourceSet(HShaderContext context, HShaderCompiler compiler, uint64_t name_hash, uint8_t set)
    {
        if (compiler->m_Language == SHADER_LANGUAGE_SPIRV)
        {
            SetResourceSetSPIRV(context, (ShaderCompilerSPIRV*) compiler, name_hash, set);
        }
        else
        {
            SetResourceSetSPVC(context, (ShaderCompilerSPVC*) compiler, name_hash, set);
        }
    }

    ShaderCompileResult* Compile(HShaderContext context, HShaderCompiler compiler, const ShaderCompilerOptions& options)
    {
        if (compiler->m_Language == SHADER_LANGUAGE_SPIRV)
        {
            return CompileSPIRV(context, (ShaderCompilerSPIRV*) compiler, options);
        }

#ifdef DM_BINARY_HLSL_SUPPORTED
        if (compiler->m_Language == SHADER_LANGUAGE_HLSL)
        {
            ShaderCompileResult* result_xcompiled = CompileSPVC(context, (ShaderCompilerSPVC*) compiler, options);
            ShaderCompileResult* result_hlsl = CompileRawHLSLToBinary(context, compiler, &options, result_xcompiled);

            FreeShaderCompileResult(result_xcompiled);
            return result_hlsl;
        }
#endif
        return CompileSPVC(context, (ShaderCompilerSPVC*) compiler, options);
    }

    void FreeShaderCompileResult(ShaderCompileResult* result)
    {
        result->m_Data.SetCapacity(0);
        result->m_HLSLResourceMappings.SetCapacity(0);
        free(result);
    }

    // Internal / private
    static const ShaderResource* FindShaderResource(const dmArray<ShaderResource>& resources, uint64_t name_hash)
    {
        for (int i = 0; i < resources.Size(); ++i)
        {
            if (resources[i].m_NameHash == name_hash)
                return &resources[i];
        }
        return 0;
    }

    const ShaderResource* FindShaderResourceInputOutput(HShaderContext context, uint64_t name_hash)
    {
        const ShaderResource* res_input = FindShaderResource(context->m_Reflection.m_Inputs, name_hash);
        if (res_input)
            return res_input;
        const ShaderResource* res_output = FindShaderResource(context->m_Reflection.m_Outputs, name_hash);
        if (res_output)
            return res_output;
        return 0;
    }

    const ShaderResource* FindShaderResourceUniform(HShaderContext context, uint64_t name_hash)
    {
        const ShaderResource* res_ubo = FindShaderResource(context->m_Reflection.m_UniformBuffers, name_hash);
        if (res_ubo)
            return res_ubo;
        const ShaderResource* res_ssbo = FindShaderResource(context->m_Reflection.m_StorageBuffers, name_hash);
        if (res_ssbo)
            return res_ssbo;
        const ShaderResource* res_texture = FindShaderResource(context->m_Reflection.m_Textures, name_hash);
        if (res_texture)
            return res_texture;
        return 0;
    }
}
