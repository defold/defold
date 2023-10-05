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
