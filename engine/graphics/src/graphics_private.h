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
    const static uint8_t DM_RENDERTARGET_BACKBUFFER_ID = 0;

    // In OpenGL, there is a single global resource identifier between
    // fragment and vertex uniforms for a single program. In Vulkan,
    // a uniform can be present in both shaders so we have to keep track
    // of this ourselves. Because of this we pack resource locations
    // for uniforms in a single base register with 15 bits
    // per shader location. If uniform is not found, we return -1 as usual.
    #define UNIFORM_LOCATION_MAX                ((uint64_t) 0xFFFF)
    #define UNIFORM_LOCATION_GET_VS(loc)        (loc & UNIFORM_LOCATION_MAX)
    #define UNIFORM_LOCATION_GET_VS_MEMBER(loc) ((loc & (UNIFORM_LOCATION_MAX << 16)) >> 16)
    #define UNIFORM_LOCATION_GET_FS(loc)        ((loc & (UNIFORM_LOCATION_MAX << 32)) >> 32)
    #define UNIFORM_LOCATION_GET_FS_MEMBER(loc) ((loc & (UNIFORM_LOCATION_MAX << 48)) >> 48)

    // Fence values to indicate frame ready or in-use state
    enum RenderContextState
    {
        RENDER_CONTEXT_STATE_FREE = 0,
        RENDER_CONTEXT_STATE_IN_USE = 1,
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
        VertexStream m_Streams[MAX_VERTEX_STREAM_COUNT];
        uint8_t      m_StreamCount;
    };

    struct VertexDeclaration
    {
        struct Stream
        {
            dmhash_t m_NameHash;
            int16_t  m_Location;
            uint16_t m_Size;
            uint16_t m_Offset;
            Type     m_Type;
            bool     m_Normalize;
        };

        Stream             m_Streams[MAX_VERTEX_STREAM_COUNT];
        dmhash_t           m_PipelineHash; // Vulkan
        uint16_t           m_StreamCount;
        uint16_t           m_Stride;
        VertexStepFunction m_StepFunction;
        HProgram           m_BoundForProgram;     // OpenGL
        uint32_t           m_ModificationVersion; // OpenGL
    };

    struct UniformBlockMember
    {
        char*                      m_Name;
        uint64_t                   m_NameHash;
        ShaderDesc::ShaderDataType m_Type;
        uint32_t                   m_Offset;
        uint16_t                   m_ElementCount;
    };

    struct ShaderResourceBinding
    {
        char*                       m_Name;
        uint64_t                    m_NameHash;
        ShaderDesc::ShaderDataType  m_Type;
        dmArray<UniformBlockMember> m_BlockMembers;
        uint32_t                    m_DataSize;
        uint16_t                    m_ElementCount;
        uint16_t                    m_Set;
        uint16_t                    m_Binding;
        union
        {
            uint16_t               m_UniformDataIndex;
            uint16_t               m_TextureUnit;
        };
    };

    struct SetTextureAsyncParams
    {
        HTexture      m_Texture;
        TextureParams m_Params;
    };

    struct SetTextureAsyncState
    {
        dmMutex::HMutex                m_Mutex;
        dmArray<SetTextureAsyncParams> m_Params;
        dmIndexPool16                  m_Indices;
        dmArray<HTexture>              m_PostDeleteTextures;
    };

    uint32_t             GetTextureFormatBitsPerPixel(TextureFormat format); // Gets the bits per pixel from uncompressed formats
    uint32_t             GetGraphicsTypeDataSize(Type type);
    const char*          GetGraphicsTypeLiteral(Type type);
    void                 InstallAdapterVendor();
    PipelineState        GetDefaultPipelineState();
    Type                 GetGraphicsTypeFromShaderDataType(ShaderDesc::ShaderDataType shader_type);
    void                 SetForceFragmentReloadFail(bool should_fail);
    void                 SetForceVertexReloadFail(bool should_fail);
    void                 SetPipelineStateValue(PipelineState& pipeline_state, State state, uint8_t value);
    bool                 IsTextureFormatCompressed(TextureFormat format);
    bool                 IsUniformTextureSampler(ShaderDesc::ShaderDataType uniform_type);
    void                 RepackRGBToRGBA(uint32_t num_pixels, uint8_t* rgb, uint8_t* rgba);
    const char*          TextureFormatToString(TextureFormat format);
    bool                 GetUniformIndices(const dmArray<ShaderResourceBinding>& uniforms, dmhash_t name_hash, uint64_t* index_out, uint64_t* index_member_out);
    ShaderDesc::Language GetShaderProgramLanguage(HContext context);
    uint32_t             GetShaderTypeSize(ShaderDesc::ShaderDataType type);
    Type                 ShaderDataTypeToGraphicsType(ShaderDesc::ShaderDataType shader_type);

    void                  InitializeSetTextureAsyncState(SetTextureAsyncState& state);
    void                  ResetSetTextureAsyncState(SetTextureAsyncState& state);
    SetTextureAsyncParams GetSetTextureAsyncParams(SetTextureAsyncState& state, uint16_t index);
    uint16_t              PushSetTextureAsyncState(SetTextureAsyncState& state, HTexture texture, TextureParams params);
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
    void     ResetDrawCount();
    uint64_t GetDrawCount();
    void     GetTextureFilters(HContext context, uint32_t unit, TextureFilter& min_filter, TextureFilter& mag_filter);
    void     EnableVertexDeclaration(HContext _context, HVertexDeclaration vertex_declaration, uint32_t binding_index);
    void     SetOverrideShaderLanguage(HContext context, ShaderDesc::ShaderClass shader_class, ShaderDesc::Language language);
}

#endif // #ifndef DM_GRAPHICS_PRIVATE_H
