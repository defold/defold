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
#include <dlib/opaque_handle_container.h>
#include "graphics.h"

namespace dmGraphics
{
    const static uint8_t MAX_VERTEX_STREAM_COUNT = 8;

    // Decorated asset handle with 32 bits meta | 32 bits opaque handle
    typedef uint64_t HAssetHandle;

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

    PipelineState GetDefaultPipelineState();
    void          SetPipelineStateValue(PipelineState& pipeline_state, State state, uint8_t value);
    uint64_t      GetDrawCount();
    void          SetForceFragmentReloadFail(bool should_fail);
    void          SetForceVertexReloadFail(bool should_fail);
    bool          IsTextureFormatCompressed(TextureFormat format);

    // Test functions:
    void* MapVertexBuffer(HVertexBuffer buffer, BufferAccess access);
    bool  UnmapVertexBuffer(HVertexBuffer buffer);
    void* MapIndexBuffer(HIndexBuffer buffer, BufferAccess access);
    bool  UnmapIndexBuffer(HIndexBuffer buffer);
    bool  IsAssetHandleValid(HContext context, HAssetHandle asset_handle);
    // <- end test functions

    static inline HAssetHandle MakeAssetHandle(HOpaqueHandle opaque_handle, AssetType asset_type)
    {
        assert(asset_type != ASSET_TYPE_NONE && "Invalid asset type");
        return ((uint64_t) asset_type) << 32 | opaque_handle;
    }

    static inline AssetType GetAssetType(HAssetHandle asset_handle)
    {
        return (AssetType) (asset_handle >> 32);
    }

    static inline HOpaqueHandle GetOpaqueHandle(HAssetHandle asset_handle)
    {
        return (HOpaqueHandle) asset_handle & 0xFFFFFFFF;
    }

    template <typename T>
    static inline HAssetHandle StoreAssetInContainer(dmOpaqueHandleContainer<uintptr_t>& container, T* asset, AssetType type)
    {
        if (container.Full())
        {
            container.Allocate(8);
        }
        HOpaqueHandle opaque_handle = container.Put((uintptr_t*) asset);
        return MakeAssetHandle(opaque_handle, type);
    }

    template <typename T>
    static inline T* GetAssetFromContainer(dmOpaqueHandleContainer<uintptr_t>& container, HAssetHandle asset_handle)
    {
        HOpaqueHandle opaque_handle = GetOpaqueHandle(asset_handle);
        return (T*) container.Get(opaque_handle);
    }
}

#endif // #ifndef DM_GRAPHICS_PRIVATE_H
