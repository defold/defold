
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
#include <dmsdk/dlib/shared_library.h>

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

    enum BaseType
    {
        BASE_TYPE_UNKNOWN,
        BASE_TYPE_VOID,
        BASE_TYPE_BOOLEAN,
        BASE_TYPE_INT8,
        BASE_TYPE_UINT8,
        BASE_TYPE_INT16,
        BASE_TYPE_UINT16,
        BASE_TYPE_INT32,
        BASE_TYPE_UINT32,
        BASE_TYPE_INT64,
        BASE_TYPE_UINT64,
        BASE_TYPE_ATOMIC_COUNTER,
        BASE_TYPE_FP16,
        BASE_TYPE_FP32,
        BASE_TYPE_FP64,
        BASE_TYPE_STRUCT,
        BASE_TYPE_IMAGE,
        BASE_TYPE_SAMPLED_IMAGE,
        BASE_TYPE_SAMPLER,
        BASE_TYPE_ACCELERATION_STRUCTURE,
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

    struct ResourceType
    {
        BaseType m_BaseType;
        uint32_t m_TypeIndex;
        bool     m_UseTypeIndex;
    };

    struct ResourceMember
    {
        const char*     m_Name;
        uint64_t        m_NameHash;
        ResourceType    m_Type;
        uint32_t        m_VectorSize;
        uint32_t        m_ColumnCount;
        uint32_t        m_Offset;
    };

    struct ResourceTypeInfo
    {
        const char*     m_Name;
        uint64_t        m_NameHash;
        ResourceMember* m_Members;
        uint32_t        m_MemberCount;
    };

    struct ShaderResource
    {
        const char*  m_Name;
        uint64_t     m_NameHash;
        const char*  m_InstanceName;
        uint64_t     m_InstanceNameHash;
        ResourceType m_Type;
        uint8_t      m_Location;
        uint8_t      m_Binding;
        uint8_t      m_Set;
    };

    struct ShaderReflection
    {
        dmArray<ShaderResource>   m_Inputs;
        dmArray<ShaderResource>   m_Outputs;
        dmArray<ShaderResource>   m_UniformBuffers;
        dmArray<ShaderResource>   m_Textures;
        dmArray<ResourceTypeInfo> m_Types;
    };


    // Shader context
    extern "C" DM_DLLEXPORT HShaderContext          NewShaderContext(const void* source, uint32_t source_size);
    extern "C" DM_DLLEXPORT void                    DeleteShaderContext(HShaderContext context);
    extern "C" DM_DLLEXPORT const ShaderReflection* GetReflection(HShaderContext context);

    // Compilers
    extern "C" DM_DLLEXPORT HShaderCompiler         NewShaderCompiler(HShaderContext context, ShaderLanguage language);
    extern "C" DM_DLLEXPORT void                    DeleteShaderCompiler(HShaderCompiler compiler);
    extern "C" DM_DLLEXPORT void                    SetResourceLocation(HShaderContext context, HShaderCompiler compiler, uint64_t name_hash, uint8_t location);
    extern "C" DM_DLLEXPORT void                    SetResourceBinding(HShaderContext context, HShaderCompiler compiler, uint64_t name_hash, uint8_t binding);
    extern "C" DM_DLLEXPORT void                    SetResourceSet(HShaderContext context, HShaderCompiler compiler, uint64_t name_hash, uint8_t set);
    extern "C" DM_DLLEXPORT const char*             Compile(HShaderContext context, HShaderCompiler compiler, const ShaderCompilerOptions& options);

    void                    DebugPrintReflection(const ShaderReflection* reflection);
}

#endif // DM_SHADERC_H
