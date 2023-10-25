// Copyright 2020-2023 The Defold Foundation
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
#include "graphics.h"

namespace dmGraphics
{
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

    uint32_t        GetTextureFormatBitsPerPixel(TextureFormat format); // Gets the bits per pixel from uncompressed formats
    uint32_t        GetGraphicsTypeDataSize(Type type);
    const char*     GetGraphicsTypeLiteral(Type type);
    void            InstallAdapterVendor();
    PipelineState   GetDefaultPipelineState();
    Type            GetGraphicsTypeFromShaderDataType(ShaderDesc::ShaderDataType shader_type);
    void            SetForceFragmentReloadFail(bool should_fail);
    void            SetForceVertexReloadFail(bool should_fail);
    void            SetPipelineStateValue(PipelineState& pipeline_state, State state, uint8_t value);
    bool            IsTextureFormatCompressed(TextureFormat format);
    bool            IsUniformTextureSampler(ShaderDesc::ShaderDataType uniform_type);
    void            RepackRGBToRGBA(uint32_t num_pixels, uint8_t* rgb, uint8_t* rgba);
    const char*     TextureFormatToString(TextureFormat format);
    bool            GetUniformIndices(const dmArray<ShaderResourceBinding>& uniforms, dmhash_t name_hash, uint64_t* index_out, uint64_t* index_member_out);

    static inline uint32_t GetShaderTypeSize(ShaderDesc::ShaderDataType type)
    {
        const uint8_t conversion_table[] = {
            0,  // SHADER_TYPE_UNKNOWN
            4,  // SHADER_TYPE_INT
            4,  // SHADER_TYPE_UINT
            4,  // SHADER_TYPE_FLOAT
            8,  // SHADER_TYPE_VEC2
            12, // SHADER_TYPE_VEC3
            16, // SHADER_TYPE_VEC4
            16, // SHADER_TYPE_MAT2
            36, // SHADER_TYPE_MAT3
            64, // SHADER_TYPE_MAT4
            4,  // SHADER_TYPE_SAMPLER2D
            4,  // SHADER_TYPE_SAMPLER3D
            4,  // SHADER_TYPE_SAMPLER_CUBE
            4,  // SHADER_TYPE_SAMPLER_ARRAY_2D
        };

        assert(((int) type) < DM_ARRAY_SIZE(conversion_table));

        return conversion_table[type];
    }

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

    // These functions are used for engine tests, but are available as experimental functions as well
    void* MapVertexBuffer(HVertexBuffer buffer, BufferAccess access);
    bool  UnmapVertexBuffer(HVertexBuffer buffer);
    void* MapIndexBuffer(HIndexBuffer buffer, BufferAccess access);
    bool  UnmapIndexBuffer(HIndexBuffer buffer);

    // Test functions:
    uint64_t GetDrawCount();
}

#endif // #ifndef DM_GRAPHICS_PRIVATE_H
