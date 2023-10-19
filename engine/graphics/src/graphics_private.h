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
    static const uint32_t MAX_SUBPASSES            = 4;
    static const uint32_t MAX_SUBPASS_DEPENDENCIES = 4;

    enum VertexStepFunction
    {
        VERTEX_STEP_VERTEX,
        VERTEX_STEP_INSTANCE,
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
        uint8_t            m_StreamCount;
    };

    static const uint8_t SUBPASS_EXTERNAL = -1;

    struct RenderPassDependency
    {
        uint8_t m_Src;
        uint8_t m_Dst;
    };

    struct RenderPassDescriptor
    {
        uint8_t* m_ColorAttachmentIndices;
        uint8_t  m_ColorAttachmentIndicesCount;
        uint8_t* m_DepthStencilAttachmentIndex;

        uint8_t* m_InputAttachmentIndices;
        uint8_t  m_InputAttachmentIndicesCount;
    };

    struct CreateRenderPassParams
    {
        RenderPassDescriptor m_SubPasses[MAX_SUBPASSES];
        RenderPassDependency m_Dependencies[MAX_SUBPASS_DEPENDENCIES];
        uint8_t              m_SubPassCount;
        uint8_t              m_DependencyCount;
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

    // Experimental only functions:
    void     CopyBufferToTexture(HContext context, HVertexBuffer buffer, HTexture texture, const TextureParams& params);
    void     SetRenderTargetAttachments(HContext context, HRenderTarget render_target, HTexture* color_attachments, uint32_t num_color_attachments, const AttachmentOp* color_attachment_load_ops, const AttachmentOp* color_attachment_store_ops, HTexture depth_stencil_attachment);
    void     SetConstantBuffer(HContext context, HVertexBuffer buffer, HUniformLocation base_location);
    HTexture GetActiveSwapChainTexture(HContext _context);
    void     DrawElementsInstanced(HContext context, PrimitiveType prim_type, uint32_t first, uint32_t count, uint32_t instance_count, uint32_t base_instance, Type type, HIndexBuffer index_buffer);
    void     SetVertexDeclarationStepFunction(HContext context, HVertexDeclaration vertex_declaration, VertexStepFunction step_function);
    void     Draw(HContext _context, PrimitiveType prim_type, uint32_t first, uint32_t count, uint32_t base_instance);
    void     CreateRenderPass(HContext _context, HRenderTarget render_target, const CreateRenderPassParams& params);
    void     NextRenderPass(HContext context, HRenderTarget render_target);

    // Test only functions:
    uint64_t GetDrawCount();

    // Both experimental + tests only:
    void* MapVertexBuffer(HContext context, HVertexBuffer buffer, BufferAccess access);
    bool  UnmapVertexBuffer(HContext context, HVertexBuffer buffer);
    void* MapIndexBuffer(HContext context, HIndexBuffer buffer, BufferAccess access);
    bool  UnmapIndexBuffer(HContext context, HIndexBuffer buffer);
}

#endif // #ifndef DM_GRAPHICS_PRIVATE_H
