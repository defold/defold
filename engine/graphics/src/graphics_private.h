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

#ifndef DM_GRAPHICS_PRIVATE_H
#define DM_GRAPHICS_PRIVATE_H

#include <stdint.h>
#include <dlib/mutex.h>
#include <dlib/index_pool.h>
#include "graphics.h"

namespace dmGraphics
{
    #define UNIFORM_LOCATION_MAX                ((uint64_t) 0xFFFF)
    #define UNIFORM_LOCATION_GET_OP0(loc) (loc & UNIFORM_LOCATION_MAX)
    #define UNIFORM_LOCATION_GET_OP1(loc) ((loc & (UNIFORM_LOCATION_MAX << 16)) >> 16)
    #define UNIFORM_LOCATION_GET_OP2(loc) ((loc & (UNIFORM_LOCATION_MAX << 32)) >> 32)
    #define UNIFORM_LOCATION_GET_OP3(loc) ((loc & (UNIFORM_LOCATION_MAX << 48)) >> 48)

    struct ProgramResourceBinding;
    struct ShaderResourceMember;

    struct CreateUniformLeafMembersCallbackParams
    {
        // Temporary char*, don't count on them existing outside of the callback
        char*                         m_CanonicalName;
        char*                         m_Namespace;
        char*                         m_InstanceName;
        const ProgramResourceBinding* m_Resource;
        const ShaderResourceMember*   m_Member;
        uint32_t                      m_BaseOffset;
    };

    typedef void (*IterateUniformsCallback)(const CreateUniformLeafMembersCallbackParams& params, void* user_data);

    const static uint8_t MAX_BINDINGS_PER_SET_COUNT = 32;
    const static uint8_t MAX_SET_COUNT              = 4;
    const static uint8_t MAX_STORAGE_BUFFERS        = 4;
    const static uint8_t MAX_VERTEX_BUFFERS         = 3;

    enum ShaderStageFlag
    {
        SHADER_STAGE_FLAG_VERTEX   = 0x1,
        SHADER_STAGE_FLAG_FRAGMENT = 0x2,
        SHADER_STAGE_FLAG_COMPUTE  = 0x4,
    };

    struct VertexStream
    {
        dmhash_t m_NameHash;
        uint32_t m_Stream;
        uint32_t m_Size;
        Type     m_Type;
        bool     m_Normalize;
    };

    struct VertexStreamDeclaration
    {
        VertexStream       m_Streams[MAX_VERTEX_STREAM_COUNT];
        VertexStepFunction m_StepFunction;
        uint8_t            m_StreamCount;
    };

    struct ShaderResourceType
    {
        union
        {
            dmGraphics::ShaderDesc::ShaderDataType m_ShaderType;
            uint32_t                               m_TypeIndex;
        };
        uint8_t m_UseTypeIndex : 1;
    };

    struct ShaderResourceMember
    {
        char*                       m_Name;
        dmhash_t                    m_NameHash;
        ShaderResourceType          m_Type;
        uint32_t                    m_ElementCount;
        uint16_t                    m_Offset;
    };

    struct ShaderResourceTypeInfo
    {
        char*                         m_Name;
        dmhash_t                      m_NameHash;
        dmArray<ShaderResourceMember> m_Members;
    };

    struct ShaderResourceBinding
    {
        enum BindingFamily
        {
            BINDING_FAMILY_GENERIC        = 0,
            BINDING_FAMILY_UNIFORM_BUFFER = 1,
            BINDING_FAMILY_STORAGE_BUFFER = 2,
            BINDING_FAMILY_TEXTURE        = 3,
        };

        union BindingInfo
        {
            uint16_t m_BlockSize;
            uint16_t m_SamplerTextureIndex;
        };

        char*              m_Name;
        dmhash_t           m_NameHash;
        char*              m_InstanceName;
        dmhash_t           m_InstanceNameHash;
        ShaderResourceType m_Type;
        BindingFamily      m_BindingFamily;
        uint16_t           m_Set;
        uint16_t           m_Binding;
        uint16_t           m_ElementCount;
        BindingInfo        m_BindingInfo;
    };

    struct ShaderMeta
    {
        dmArray<ShaderResourceBinding>  m_UniformBuffers;
        dmArray<ShaderResourceBinding>  m_StorageBuffers;
        dmArray<ShaderResourceBinding>  m_Textures;
        dmArray<ShaderResourceBinding>  m_Inputs;
        dmArray<ShaderResourceTypeInfo> m_TypeInfos;
    };

    struct SetTextureAsyncParams
    {
        HTexture                m_Texture;
        TextureParams           m_Params;
        SetTextureAsyncCallback m_Callback;
        void*                   m_UserData;
    };

    struct SetTextureAsyncState
    {
        dmMutex::HMutex                m_Mutex;
        dmArray<SetTextureAsyncParams> m_Params;
        dmIndexPool16                  m_Indices;
        dmArray<HTexture>              m_PostDeleteTextures;
    };

    struct ProgramResourceBindingsInfo
    {
        uint32_t m_UniformBufferCount;
        uint32_t m_StorageBufferCount;
        uint32_t m_TextureCount;
        uint32_t m_SamplerCount;
        uint32_t m_UniformDataSize;
        uint32_t m_UniformDataSizeAligned;
        uint32_t m_MaxSet;
        uint32_t m_MaxBinding;
    };

    struct ResourceBindingDesc
    {
        uint16_t m_Binding;
        uint8_t  m_Taken;
    };

    struct ProgramResourceBinding
    {
        ShaderResourceBinding*           m_Res;
        dmArray<ShaderResourceTypeInfo>* m_TypeInfos;
        uint32_t                         m_DataOffset;

        union
        {
            uint16_t m_DynamicOffsetIndex;
            uint16_t m_TextureUnit;
            uint16_t m_StorageBufferUnit;
        };

        uint8_t m_StageFlags;
    };

    struct Program
    {
        ProgramResourceBinding m_ResourceBindings[MAX_SET_COUNT][MAX_BINDINGS_PER_SET_COUNT];
        dmArray<Uniform>       m_Uniforms;
        uint8_t                m_MaxSet;
        uint8_t                m_MaxBinding;
    };

    struct ProgramResourceBindingIterator
    {
        const Program* m_Program;
        uint32_t       m_CurrentSet;
        uint32_t       m_CurrentBinding;

        ProgramResourceBindingIterator(const Program* pgm)
        : m_Program(pgm)
        , m_CurrentSet(0)
        , m_CurrentBinding(0)
        {}

        const ProgramResourceBinding* Next()
        {
            for (; m_CurrentSet < m_Program->m_MaxSet; ++m_CurrentSet)
            {
                for (; m_CurrentBinding < m_Program->m_MaxBinding; ++m_CurrentBinding)
                {
                    if (m_Program->m_ResourceBindings[m_CurrentSet][m_CurrentBinding].m_Res != 0x0)
                    {
                        const ProgramResourceBinding* res = &m_Program->m_ResourceBindings[m_CurrentSet][m_CurrentBinding];
                        m_CurrentBinding++;
                        return res;
                    }
                }
                m_CurrentBinding = 0;  // Reset binding index when moving to the next set
            }
            return 0x0;
        }

        void Reset()
        {
            m_CurrentSet = 0;
            m_CurrentBinding = 0;
        }
    };

    HContext             GetInstalledContext();
    uint32_t             GetTextureFormatBitsPerPixel(TextureFormat format); // Gets the bits per pixel from uncompressed formats
    uint32_t             GetGraphicsTypeDataSize(Type type);
    void                 InstallAdapterVendor();
    PipelineState        GetDefaultPipelineState();
    Type                 GetGraphicsTypeFromShaderDataType(ShaderDesc::ShaderDataType shader_type);
    void                 SetForceFragmentReloadFail(bool should_fail);
    void                 SetForceVertexReloadFail(bool should_fail);
    void                 SetPipelineStateValue(PipelineState& pipeline_state, State state, uint8_t value);
    bool                 IsTextureFormatCompressed(TextureFormat format);
    bool                 IsUniformTextureSampler(ShaderDesc::ShaderDataType uniform_type);
    bool                 IsUniformStorageBuffer(ShaderDesc::ShaderDataType uniform_type);
    void                 RepackRGBToRGBA(uint32_t num_pixels, uint8_t* rgb, uint8_t* rgba);
    bool                 IsTextureFormatASTC(TextureFormat format);
    const char*          TextureFormatToString(TextureFormat format);
    ShaderDesc::Language GetShaderProgramLanguage(HContext context);
    uint32_t             GetShaderTypeSize(ShaderDesc::ShaderDataType type);
    Type                 ShaderDataTypeToGraphicsType(ShaderDesc::ShaderDataType shader_type);
    ShaderDesc::Shader*  GetShaderProgram(HContext context, ShaderDesc* shader_desc);

    void                 CreateShaderMeta(ShaderDesc::ShaderReflection* ddf, ShaderMeta* meta);
    void                 DestroyShaderMeta(ShaderMeta& meta);
    bool                 GetUniformIndices(const dmArray<ShaderResourceBinding>& uniforms, dmhash_t name_hash, uint64_t* index_out, uint64_t* index_member_out);
    uint32_t             CountShaderResourceLeafMembers(const dmArray<ShaderResourceTypeInfo>& type_infos, ShaderResourceType type, uint32_t count = 0);
    void                 BuildUniforms(Program* program);
    void                 IterateUniforms(Program* program, bool prepend_instance_name, IterateUniformsCallback callback, void* user_data);

    void FillProgramResourceBindings(Program& program,
        dmArray<ShaderResourceBinding>&       resources,
        dmArray<ShaderResourceTypeInfo>&      stage_type_infos,
        ResourceBindingDesc                   bindings[MAX_SET_COUNT][MAX_BINDINGS_PER_SET_COUNT],
        uint32_t                              ubo_alignment,
        uint32_t                              ssbo_alignment,
        ShaderStageFlag                       stage_flag,
        ProgramResourceBindingsInfo&          info);

    void FillProgramResourceBindings(
        Program*                     program,
        ShaderMeta*                  meta,
        ResourceBindingDesc          bindings[MAX_SET_COUNT][MAX_BINDINGS_PER_SET_COUNT],
        uint32_t                     ubo_alignment,
        uint32_t                     ssbo_alignment,
        ShaderStageFlag              stage_flag,
        ProgramResourceBindingsInfo& info);

    void                  InitializeSetTextureAsyncState(SetTextureAsyncState& state);
    void                  ResetSetTextureAsyncState(SetTextureAsyncState& state);
    SetTextureAsyncParams GetSetTextureAsyncParams(SetTextureAsyncState& state, uint16_t index);
    uint16_t              PushSetTextureAsyncState(SetTextureAsyncState& state, HTexture texture, TextureParams params, SetTextureAsyncCallback callback, void* user_data);
    void                  ReturnSetTextureAsyncIndex(SetTextureAsyncState& state, uint16_t index);
    void                  PushSetTextureAsyncDeleteTexture(SetTextureAsyncState& state, HTexture texture);

    static inline void ClearTextureParamsData(TextureParams& params)
    {
        params.m_Data     = 0x0;
        params.m_DataSize = 0;
    }

    template <typename T>
    static inline HAssetHandle StoreAssetInContainer(dmOpaqueHandleContainer<uintptr_t>& container, T* asset, AssetType type)
    {
        if (container.Full())
        {
            container.Allocate(8);
        }
        HOpaqueHandle opaque_handle = container.Put((uintptr_t*) asset);
        HAssetHandle asset_handle   = MakeAssetHandle(opaque_handle, type);
        return asset_handle;
    }

    template <typename T>
    static inline T* GetAssetFromContainer(dmOpaqueHandleContainer<uintptr_t>& container, HAssetHandle asset_handle)
    {
        assert(asset_handle <= MAX_ASSET_HANDLE_VALUE);
        HOpaqueHandle opaque_handle = GetOpaqueHandle(asset_handle);
        return (T*) container.Get(opaque_handle);
    }

    // Test only functions:
    void             ResetDrawCount();
    uint64_t         GetDrawCount();
    void             GetTextureFilters(HContext context, uint32_t unit, TextureFilter& min_filter, TextureFilter& mag_filter);
    void             EnableVertexDeclaration(HContext _context, HVertexDeclaration vertex_declaration, uint32_t binding_index);
    void             SetOverrideShaderLanguage(HContext context, ShaderDesc::ShaderType shader_class, ShaderDesc::Language language);
    const Uniform*   GetUniform(HProgram prog, dmhash_t name_hash);
}

#endif // #ifndef DM_GRAPHICS_PRIVATE_H
