
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
        SHADER_LANGUAGE_NONE  = 0,
        SHADER_LANGUAGE_GLSL  = 1,
        SHADER_LANGUAGE_HLSL  = 2,
        SHADER_LANGUAGE_MSL   = 3,
        SHADER_LANGUAGE_SPIRV = 4,
    };

    enum ShaderStage
    {
        SHADER_STAGE_VERTEX   = 1,
        SHADER_STAGE_FRAGMENT = 2,
        SHADER_STAGE_COMPUTE  = 4,
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

    enum DimensionType
    {
        DIMENSION_TYPE_1D,
        DIMENSION_TYPE_2D,
        DIMENSION_TYPE_3D,
        DIMENSION_TYPE_CUBE,
        DIMENSION_TYPE_RECT,
        DIMENSION_TYPE_BUFFER,
        DIMENSION_TYPE_SUBPASS_DATA,
    };

    enum ImageStorageType
    {
        IMAGE_STORAGE_TYPE_UNKNOWN,
        IMAGE_STORAGE_TYPE_RGBA32F,
        IMAGE_STORAGE_TYPE_RGBA16F,
        IMAGE_STORAGE_TYPE_R32F,
        IMAGE_STORAGE_TYPE_RGBA8,
        IMAGE_STORAGE_TYPE_RGBA8_SNORM,
        IMAGE_STORAGE_TYPE_RG32F,
        IMAGE_STORAGE_TYPE_RG16F,
        IMAGE_STORAGE_TYPE_R11F_G11F_B10F,
        IMAGE_STORAGE_TYPE_R16F,
        IMAGE_STORAGE_TYPE_RGBA16,
        IMAGE_STORAGE_TYPE_RGB10A2,
        IMAGE_STORAGE_TYPE_RG16,
        IMAGE_STORAGE_TYPE_RG8,
        IMAGE_STORAGE_TYPE_R16,
        IMAGE_STORAGE_TYPE_R8,
        IMAGE_STORAGE_TYPE_RGBA16_SNORM,
        IMAGE_STORAGE_TYPE_RG16_SNORM,
        IMAGE_STORAGE_TYPE_RG8_SNORM,
        IMAGE_STORAGE_TYPE_R16_SNORM,
        IMAGE_STORAGE_TYPE_R8_SNORM,
        IMAGE_STORAGE_TYPE_RGBA32I,
        IMAGE_STORAGE_TYPE_RGBA16I,
        IMAGE_STORAGE_TYPE_RGBA8I,
        IMAGE_STORAGE_TYPE_R32I,
        IMAGE_STORAGE_TYPE_RG32I,
        IMAGE_STORAGE_TYPE_RG16I,
        IMAGE_STORAGE_TYPE_RG8I,
        IMAGE_STORAGE_TYPE_R16I,
        IMAGE_STORAGE_TYPE_R8I,
        IMAGE_STORAGE_TYPE_RGBA32UI,
        IMAGE_STORAGE_TYPE_RGBA16UI,
        IMAGE_STORAGE_TYPE_RGBA8UI,
        IMAGE_STORAGE_TYPE_R32UI,
        IMAGE_STORAGE_TYPE_RGb10a2UI,
        IMAGE_STORAGE_TYPE_RG32UI,
        IMAGE_STORAGE_TYPE_RG16UI,
        IMAGE_STORAGE_TYPE_RG8UI,
        IMAGE_STORAGE_TYPE_R16UI,
        IMAGE_STORAGE_TYPE_R8UI,
        IMAGE_STORAGE_TYPE_R64UI,
        IMAGE_STORAGE_TYPE_R64I,
    };

    enum ImageAccessQualifier
    {
        IMAGE_ACCESS_QUALIFIER_READ_ONLY,
        IMAGE_ACCESS_QUALIFIER_WRITE_ONLY,
        IMAGE_ACCESS_QUALIFIER_READ_WRITE,
    };

    struct ShaderCompilerOptions
    {
        ShaderCompilerOptions()
        : m_Version(330)
        , m_EntryPoint("main")
        , m_RemoveUnusedVariables(true)
        , m_No420PackExtension(true)
        , m_GlslEmitUboAsPlainUniforms(true)
        , m_GlslEs(false)
        {}

        uint32_t    m_Version;
        const char* m_EntryPoint;

        uint8_t     m_RemoveUnusedVariables      : 1;
        uint8_t     m_No420PackExtension         : 1;
        uint8_t     m_GlslEmitUboAsPlainUniforms : 1;
        uint8_t     m_GlslEs                     : 1;
    };

    struct ResourceType
    {
        BaseType             m_BaseType;
        DimensionType        m_DimensionType;
        ImageStorageType     m_ImageStorageType;
        ImageAccessQualifier m_ImageAccessQualifier;
        BaseType             m_ImageBaseType;
        uint32_t             m_TypeIndex;
        uint32_t             m_VectorSize;
        uint32_t             m_ColumnCount;
        uint32_t             m_ArraySize;
        bool                 m_UseTypeIndex;
        bool                 m_ImageIsArrayed;
        bool                 m_ImageIsStorage;
    };

    struct ResourceMember
    {
        const char*     m_Name;
        uint64_t        m_NameHash;
        ResourceType    m_Type;
        uint32_t        m_Offset;
    };

    struct ResourceTypeInfo
    {
        const char*             m_Name;
        uint64_t                m_NameHash;
        dmArray<ResourceMember> m_Members;
    };

    struct ShaderResource
    {
        const char*  m_Name;
        uint64_t     m_NameHash;
        const char*  m_InstanceName;
        uint64_t     m_InstanceNameHash;
        ResourceType m_Type;
        uint32_t     m_Id;
        uint32_t     m_BlockSize;
        uint8_t      m_Location;
        uint8_t      m_Binding;
        uint8_t      m_Set;
        uint8_t      m_StageFlags;
    };

    struct ShaderReflection
    {
        dmArray<ShaderResource>   m_Inputs;
        dmArray<ShaderResource>   m_Outputs;
        dmArray<ShaderResource>   m_UniformBuffers;
        dmArray<ShaderResource>   m_StorageBuffers;
        dmArray<ShaderResource>   m_Textures;
        dmArray<ResourceTypeInfo> m_Types;
    };

    struct HLSLResourceMapping
    {
        const char* m_Name;
        uint64_t    m_NameHash;

        // These point to a resource from one of the ShaderReflection list
        uint8_t     m_ShaderResourceSet;
        uint8_t     m_ShaderResourceBinding;
    };

    struct ShaderCompileResult
    {
        dmArray<uint8_t> m_Data;
        const char*      m_LastError;

        // In case of compiling HLSL, we generate a separate reflection structure
        // that embeds a list of resources (called root signature) and their HLSL bind points (registers)
        // This must match resource bind points in the engine, so we need to output that information here.
        dmArray<HLSLResourceMapping> m_HLSLResourceMappings;
        // When compiling compute shaders for HLSL, we need to store a reference to the
        // manufactured gl_NumWorkGroups constant buffer that was generated.
        // The value will be set to 0xFF otherwise.
        uint8_t                    m_HLSLNumWorkGroupsId;
    };

    // Shader context
    extern "C" DM_DLLEXPORT HShaderContext          NewShaderContext(ShaderStage stage, const void* source, uint32_t source_size);
    extern "C" DM_DLLEXPORT void                    DeleteShaderContext(HShaderContext context);

    // Reflection
    extern "C" DM_DLLEXPORT const ShaderReflection* GetReflection(HShaderContext context);
    extern "C" DM_DLLEXPORT void                    SetResourceStageFlags(HShaderContext context, uint64_t name_hash, uint8_t stage_flags);

    // Compilers
    extern "C" DM_DLLEXPORT HShaderCompiler         NewShaderCompiler(HShaderContext context, ShaderLanguage language);
    extern "C" DM_DLLEXPORT void                    DeleteShaderCompiler(HShaderCompiler compiler);
    extern "C" DM_DLLEXPORT void                    SetResourceLocation(HShaderContext context, HShaderCompiler compiler, uint64_t name_hash, uint8_t location);
    extern "C" DM_DLLEXPORT void                    SetResourceBinding(HShaderContext context, HShaderCompiler compiler, uint64_t name_hash, uint8_t binding);
    extern "C" DM_DLLEXPORT void                    SetResourceSet(HShaderContext context, HShaderCompiler compiler, uint64_t name_hash, uint8_t set);
    extern "C" DM_DLLEXPORT ShaderCompileResult*    Compile(HShaderContext context, HShaderCompiler compiler, const ShaderCompilerOptions& options);
    extern "C" DM_DLLEXPORT void                    FreeShaderCompileResult(ShaderCompileResult* result);

    void DebugPrintReflection(const ShaderReflection* reflection);
}

#endif // DM_SHADERC_H
