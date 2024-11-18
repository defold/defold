
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

#ifndef DM_SHADERC_H
#define DM_SHADERC_H

#include <stdint.h>

#include <dmsdk/dlib/array.h>
#include <dmsdk/dlib/hash.h>

namespace dmShaderc
{
    typedef struct ShaderContext* HShaderContext;
    typedef struct ShaderCompiler* HShaderCompiler;

    enum ShaderLanguage
    {
        SHADER_LANGUAGE_NONE = 0,
        SHADER_LANGUAGE_GLSL = 1,
        SHADER_LANGUAGE_HLSL = 2,
    };

    enum ShaderStage
    {
        SHADER_STAGE_VERTEX   = 1,
        SHADER_STAGE_FRAGMENT = 2,
        SHADER_STAGE_COMPUTE  = 3,
    };

    struct ShaderResource
    {
        const char* m_Name;
        dmhash_t    m_NameHash;
        const char* m_InstanceName;
        dmhash_t    m_InstanceNameHash;
        uint8_t     m_Location;
        uint8_t     m_Binding;
        uint8_t     m_Set;
    };

    struct ShaderCompilerOptions
    {
        ShaderCompilerOptions()
        : m_Version(330)
        , m_EntryPoint("main")
        , m_Stage(SHADER_STAGE_VERTEX)
        , m_RemoveUnusedVariables(true)
        , m_No420PackExtension(true)
        , m_GlslEmitUboAsPlainUniforms(true)
        , m_GlslEs(false)
        {}

        uint32_t    m_Version;
        const char* m_EntryPoint;
        ShaderStage m_Stage;

        uint8_t     m_RemoveUnusedVariables      : 1;
        uint8_t     m_No420PackExtension         : 1;
        uint8_t     m_GlslEmitUboAsPlainUniforms : 1;
        uint8_t     m_GlslEs                     : 1;
    };

    struct ShaderReflection
    {
        dmArray<ShaderResource> m_Inputs;
        dmArray<ShaderResource> m_Outputs;
        dmArray<ShaderResource> m_UniformBuffers;
    };

    HShaderContext          NewShaderContext(const void* source, uint32_t source_size);
    void                    DeleteShaderContext(HShaderContext context);

    const ShaderReflection* GetReflection(HShaderContext context);

    HShaderCompiler         NewShaderCompiler(HShaderContext context, ShaderLanguage language);
    void                    DeleteShaderCompiler(HShaderCompiler compiler);

    void                    SetLocation(HShaderContext context, HShaderCompiler compiler, dmhash_t name_hash, uint8_t location);
    const char*             Compile(HShaderContext context, HShaderCompiler compiler, const ShaderCompilerOptions& options);

    void                    DebugPrintReflection(const ShaderReflection* reflection);
}

#endif // DM_SHADERC_H
