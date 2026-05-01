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

#ifndef SHADERC_PRIVATE_H
#define SHADERC_PRIVATE_H

#include "shaderc.h"

#include <spirv/spirv_cross_c.h>

namespace dmShaderc
{
    static const uint8_t HLSL_NUM_WORKGROUPS_SET = 0;

    struct ShaderContext
    {
        uint8_t*          m_ShaderCode;
        uint32_t          m_ShaderCodeSize;
        spvc_context      m_SPVCContext;
        spvc_parsed_ir    m_ParsedIR;
        spvc_compiler     m_CompilerNone;
        spvc_resources    m_Resources;
        SpvExecutionModel m_ExecutionModel;
        ShaderReflection  m_Reflection;
        ShaderStage       m_Stage;
    };

    struct ShaderCompiler
    {
        ShaderLanguage m_Language;
    };

    struct ShaderCompilerSPVC
    {
        ShaderCompiler m_BaseCompiler;
        spvc_compiler  m_SPVCCompiler;
    };

    struct ShaderCompilerSPIRV
    {
        struct RemapResourceEntry
        {
            uint64_t m_NameHash;

            union Value
            {
                uint8_t m_Location;
                uint8_t m_Binding;
                uint8_t m_Set;
                uint8_t m_Value;
            };

            enum Type
            {
                TYPE_LOCATION,
                TYPE_BINDING,
                TYPE_SET,
            };

            Type     m_Type;
            uint32_t m_Id;
            Value    m_OldValue;
            Value    m_NewValue;
        };

        ShaderCompiler              m_BaseCompiler;
        dmArray<RemapResourceEntry> m_RemapResources;
        uint8_t*                    m_SPIRVCode;
        uint32_t                    m_SPIRVCodeSize;
    };

    ShaderCompilerSPIRV* NewShaderCompilerSPIRV(HShaderContext context);
    void                 DeleteShaderCompilerSPIRV(ShaderCompilerSPIRV* compiler);
    void                 SetResourceLocationSPIRV(HShaderContext context, ShaderCompilerSPIRV* compiler, uint64_t name_hash, uint8_t location);
    void                 SetResourceBindingSPIRV(HShaderContext context, ShaderCompilerSPIRV* compiler, uint64_t name_hash, uint8_t binding);
    void                 SetResourceSetSPIRV(HShaderContext context, ShaderCompilerSPIRV* compiler, uint64_t name_hash, uint8_t set);
    ShaderCompileResult* CompileSPIRV(HShaderContext context, ShaderCompilerSPIRV* compiler, const ShaderCompilerOptions& options);

    ShaderCompilerSPVC*  NewShaderCompilerSPVC(HShaderContext context, ShaderLanguage language);
    void                 DeleteShaderCompilerSPVC(ShaderCompilerSPVC* compiler);
    void                 SetResourceLocationSPVC(HShaderContext context, ShaderCompilerSPVC* compiler, uint64_t name_hash, uint8_t location);
    void                 SetResourceBindingSPVC(HShaderContext context, ShaderCompilerSPVC* compiler, uint64_t name_hash, uint8_t binding);
    void                 SetResourceSetSPVC(HShaderContext context, ShaderCompilerSPVC* compiler, uint64_t name_hash, uint8_t set);
    ShaderCompileResult* CompileSPVC(HShaderContext context, ShaderCompilerSPVC* compiler, const ShaderCompilerOptions& options);

    // Extra functions
    const ShaderResource* FindShaderResourceInputOutput(HShaderContext context, uint64_t name_hash);
    const ShaderResource* FindShaderResourceUniform(HShaderContext context, uint64_t name_hash);

    // Used for building HLSL resource bindings
    struct CombinedSampler
    {
        const char* m_CombinedName;
        uint8_t     m_CombinedId;
        const char* m_ImageName;
        uint8_t     m_ImageId;
        const char* m_SamplerName;
        uint8_t     m_SamplerId;
    };

    void GetCombinedSamplerMapSPIRV(HShaderContext context, ShaderCompilerSPVC* compiler, dmArray<CombinedSampler>& samplers);

    ShaderCompileResult* CompileRawHLSLToBinary(HShaderContext context, HShaderCompiler compiler, const ShaderCompilerOptions* options, ShaderCompileResult* raw_hlsl);
}

#endif
