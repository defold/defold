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

#ifndef DMGRAPHICS_GRAPHICS_DEVICE_METAL_H
#define DMGRAPHICS_GRAPHICS_DEVICE_METAL_H

#include <dispatch/dispatch.h>
#include <Metal.hpp>
#include <cstring>

#include <dmsdk/dlib/atomic.h>

#include <dlib/hashtable.h>
#include <dlib/opaque_handle_container.h>

namespace dmGraphics
{
    struct MetalContext;
    struct MetalPipeline;
    struct ResourceToDestroy;

    typedef dmHashTable64<MetalPipeline> PipelineCache;
    typedef dmArray<ResourceToDestroy>   ResourcesToDestroyList;

#if defined(DM_PLATFORM_MACOS)
    const static uint8_t  MAX_FRAMES_IN_FLIGHT     = 2; // Keep two frames in flight on macOS for better CPU/GPU overlap
#else
    const static uint8_t  MAX_FRAMES_IN_FLIGHT     = 1;
#endif
    const static uint16_t MAX_ENCODER_RESOURCE_CACHE = 256;
    const static uint8_t  MAX_VERTEX_BUFFER_SLOTS    = MAX_BINDINGS_PER_SET_COUNT + MAX_VERTEX_BUFFERS;
    const static uint32_t UNIFORM_BUFFER_ALIGNMENT = 256;
    const static uint32_t STORAGE_BUFFER_ALIGNMENT = 16;

    enum MetalResourceType
    {
        RESOURCE_TYPE_DEVICE_BUFFER  = 0,
        RESOURCE_TYPE_TEXTURE        = 1,
        RESOURCE_TYPE_PROGRAM        = 2,
        RESOURCE_TYPE_RENDER_TARGET  = 3,
        RESOURCE_TYPE_COMMAND_BUFFER = 4,
    };

    struct MetalViewport
    {
        uint16_t m_X;
        uint16_t m_Y;
        uint16_t m_W;
        uint16_t m_H;
    };

    struct MetalPipeline
    {
        MTL::RenderPipelineState*  m_RenderPipelineState;
        MTL::DepthStencilState*    m_DepthStencilState;
        MTL::ComputePipelineState* m_ComputePipelineState;
    };

    struct ResourceToDestroy
    {
        union
        {
            MTL::Buffer*  m_DeviceBuffer;
            MTL::Texture* m_Texture;
        };
        MetalResourceType m_ResourceType;
    };

    struct MetalDeviceBuffer
    {
        MTL::Buffer*     m_Buffer;
        MTL::StorageMode m_StorageMode;
        uint32_t         m_Size;
        uint8_t          m_Destroyed;

        const static MetalResourceType GetType()
        {
            return RESOURCE_TYPE_DEVICE_BUFFER;
        }
    };

    struct MetalUniformBuffer
    {
        UniformBuffer     m_BaseUniformBuffer;
        MetalDeviceBuffer m_DeviceBuffer;
    };

    struct MetalConstantScratchBuffer
    {
        MetalDeviceBuffer m_DeviceBuffer;
        uint32_t          m_MappedDataCursor;

        void EnsureSize(const MetalContext* context, uint32_t size);

        inline bool CanAllocate(uint32_t size) { return size <= (m_DeviceBuffer.m_Size - m_MappedDataCursor); }
        inline void Rewind() { m_MappedDataCursor = 0; }
        inline void Advance(uint32_t size) { m_MappedDataCursor += size; }
    };

    struct MetalArgumentBinding
    {
        MTL::Buffer* m_Buffer;
        uint32_t     m_Offset;
    };

    struct MetalArgumentBufferPool
    {
        dmArray<MetalConstantScratchBuffer> m_ScratchBufferPool;
        uint32_t                            m_ScratchBufferIndex;
        uint32_t                            m_SizePerBuffer;

        void                        Initialize(const MetalContext* context, uint32_t size_per_buffer);
        void                        AddBuffer(const MetalContext* context);
        MetalConstantScratchBuffer* Allocate(const MetalContext* context, uint32_t size);
        MetalArgumentBinding        Bind(const MetalContext* context, MTL::ArgumentEncoder* encode);

        inline MetalConstantScratchBuffer* Get() { return &m_ScratchBufferPool[m_ScratchBufferIndex]; }
        inline void Rewind()
        {
            m_ScratchBufferIndex = 0;

            uint32_t count = m_ScratchBufferPool.Size();
            for (int i = 0; i < count; ++i)
            {
                m_ScratchBufferPool[i].Rewind();
            }
        }
    };

    struct MetalTextureSampler
    {
        MTL::SamplerState* m_Sampler;
        TextureFilter      m_MinFilter;
        TextureFilter      m_MagFilter;
        TextureWrap        m_AddressModeU;
        TextureWrap        m_AddressModeV;
        float              m_MaxAnisotropy;
        float              m_MinLod;
        float              m_MaxLod;
    };

    struct MetalTexture
    {
        Texture            m_Base;
        MTL::Texture*      m_Texture;
        MTL::ResourceUsage m_Usage;
        uint16_t           m_TextureSamplerIndex : 10;
        uint32_t           m_Destroyed           : 1;
        uint8_t            m_LayerCount;

        const static MetalResourceType GetType()
        {
            return RESOURCE_TYPE_TEXTURE;
        }
    };

    struct MetalRenderTarget
    {
        MetalRenderTarget(const uint32_t rtId)
        : m_DepthClearValue(1.0f)
        , m_StencilClearValue(0)
        , m_Id(rtId)
        , m_Destroyed(0)
        , m_IsBound(0)
        , m_HasPendingClearColor(0)
        , m_HasPendingClearDepth(0)
        , m_HasPendingClearStencil(0)
        , m_ColorAttachmentCount(0)
        {
            memset(m_ColorBufferLoadOps, 0, sizeof(m_ColorBufferLoadOps));
            memset(m_ColorBufferStoreOps, 0, sizeof(m_ColorBufferStoreOps));
            memset(m_ColorAttachmentClearValue, 0, sizeof(m_ColorAttachmentClearValue));
            memset(m_ColorTextureParams, 0, sizeof(m_ColorTextureParams));
            memset(&m_DepthStencilTextureParams, 0, sizeof(m_DepthStencilTextureParams));
            memset(m_TextureColor, 0, sizeof(m_TextureColor));
            m_TextureDepthStencil = 0;
            memset(m_ColorFormat, 0, sizeof(m_ColorFormat));
            m_DepthStencilFormat = MTL::PixelFormatInvalid;
        }

        AttachmentOp          m_ColorBufferLoadOps[MAX_BUFFER_COLOR_ATTACHMENTS];
        AttachmentOp          m_ColorBufferStoreOps[MAX_BUFFER_COLOR_ATTACHMENTS];
        float                 m_ColorAttachmentClearValue[MAX_BUFFER_COLOR_ATTACHMENTS][4];
        float                 m_DepthClearValue;
        uint32_t              m_StencilClearValue;
        TextureParams         m_ColorTextureParams[MAX_BUFFER_COLOR_ATTACHMENTS];
        TextureParams         m_DepthStencilTextureParams;

        HTexture              m_TextureColor[MAX_BUFFER_COLOR_ATTACHMENTS];
        HTexture              m_TextureDepthStencil;

        MTL::PixelFormat      m_ColorFormat[MAX_BUFFER_COLOR_ATTACHMENTS];
        MTL::PixelFormat      m_DepthStencilFormat;

        const uint16_t m_Id;
        uint32_t       m_Destroyed            : 1;
        uint32_t       m_IsBound              : 1;
        uint32_t       m_HasPendingClearColor : 1;
        uint32_t       m_HasPendingClearDepth : 1;
        uint32_t       m_HasPendingClearStencil : 1;
        uint32_t       m_ColorAttachmentCount : 4;
    };

    struct MetalStorageBufferBinding
    {
        HStorageBuffer m_Buffer;
        uint32_t       m_BufferOffset;
    };

    struct MetalShaderModule
    {
        MTL::Function* m_Function;
        MTL::Library*  m_Library;
        uint64_t       m_Hash;
    };

    struct MetalProgram
    {
        Program               m_BaseProgram;
        MetalShaderModule*    m_VertexModule;
        MetalShaderModule*    m_FragmentModule;
        MetalShaderModule*    m_ComputeModule;
        MTL::ArgumentEncoder* m_ArgumentEncoders[MAX_SET_COUNT];
        MetalArgumentBinding  m_ArgumentBufferBindings[MAX_SET_COUNT];

        uint32_t              m_ResourceToMslIndex[MAX_SET_COUNT][MAX_BINDINGS_PER_SET_COUNT];
        uint32_t              m_WorkGroupSize[3]; // x,y,z
        uint8_t*              m_UniformData;
        uint64_t              m_Hash;
        uint32_t              m_UniformDataSizeAligned;
        uint16_t              m_UniformBufferCount;
        uint16_t              m_StorageBufferCount;
        uint16_t              m_TextureSamplerCount;
    };

    struct MetalFrameResource
    {
        ResourcesToDestroyList*    m_ResourcesToDestroy;
        MetalConstantScratchBuffer m_ConstantScratchBuffer;
        MetalArgumentBufferPool    m_ArgumentBufferPool;
        MTL::CommandBuffer*        m_CommandBuffer;
        CA::MetalDrawable*         m_Drawable;
        NS::AutoreleasePool*       m_AutoReleasePool;
        MTL::RenderPassDescriptor* m_RenderPassDescriptor;
        MTL::RenderCommandEncoder* m_RenderCommandEncoder;
        MTL::Texture*              m_MSAAColorTexture;
        MTL::Texture*              m_MSAADepthTexture;
        uint8_t                    m_InFlight;
    };

    struct MetalClearData
    {
        struct CacheKey
        {
            uint32_t         m_ColorAttachmentCount;
            uint32_t         m_ColorWriteMaskBits;
            MTL::PixelFormat m_ColorFormats[MAX_BUFFER_COLOR_ATTACHMENTS];
            MTL::PixelFormat m_DepthStencilFormat;
            uint8_t          m_ClearColor   : 1;
            uint8_t          m_ClearDepth   : 1;
            uint8_t          m_ClearStencil : 1;
            uint8_t          m_SampleCount  : 4;
        };

        struct ClearShader
        {
            MTL::Function* m_VsFunction;
            MTL::Function* m_FsFunction;
            uint8_t        m_ClearColor   : 1;
            uint8_t        m_ClearDepth   : 1;
            uint8_t        m_ClearStencil : 1;
        };

        PipelineCache        m_PipelineCache;
        dmArray<ClearShader> m_ClearShaderPermutations;
    };

    struct MetalContext
    {
        MetalContext(const ContextParams& params);

        GraphicsContext                    m_BaseContext;
        NSView*                            m_View;
        CAMetalLayer*                      m_Layer;
        MetalFrameResource                 m_FrameResources[MAX_FRAMES_IN_FLIGHT];
        MTL::Device*                       m_Device;
        MTL::CommandQueue*                 m_CommandQueue;
        PipelineState                      m_PipelineState;
        PipelineCache                      m_PipelineCache;
        dmArray<MetalTextureSampler>       m_TextureSamplers;
        HTexture                           m_TextureUnits[DM_MAX_TEXTURE_UNITS];
        VertexDeclaration                  m_MainVertexDeclaration[MAX_VERTEX_BUFFERS];
        MetalViewport                      m_MainViewport;
        HRenderTarget                      m_MainRenderTarget;
        MTL::Texture*                      m_MainDepthStencilTexture;
        MetalClearData                     m_ClearData;
        dmArray<VertexDeclaration::Stream> m_MainVertexDeclarationStreams[MAX_VERTEX_BUFFERS];

        // Async process resources
        HJobContext                        m_JobContext;
        SetTextureAsyncState               m_SetTextureAsyncState;
        dispatch_semaphore_t               m_FrameBoundarySemaphore;

        // Per-frame render state
        MetalDeviceBuffer*                 m_CurrentVertexBuffer[MAX_VERTEX_BUFFERS];
        VertexDeclaration*                 m_CurrentVertexDeclaration[MAX_VERTEX_BUFFERS];
        uint32_t                           m_CurrentVertexBufferOffset[MAX_VERTEX_BUFFERS];
        MetalStorageBufferBinding          m_CurrentStorageBuffers[MAX_STORAGE_BUFFERS];
        MetalUniformBuffer*                m_CurrentUniformBuffers[MAX_SET_COUNT][MAX_BINDINGS_PER_SET_COUNT];
        MetalProgram*                      m_CurrentProgram;
        MetalPipeline*                     m_CurrentPipeline;
        HRenderTarget                      m_CurrentRenderTarget;
        MTL::Buffer*                       m_CurrentVertexArgumentBuffer[MAX_SET_COUNT];
        uint32_t                           m_CurrentVertexArgumentBufferOffset[MAX_SET_COUNT];
        MTL::Buffer*                       m_CurrentFragmentArgumentBuffer[MAX_SET_COUNT];
        uint32_t                           m_CurrentFragmentArgumentBufferOffset[MAX_SET_COUNT];
        MTL::Buffer*                       m_CurrentVertexBufferBindings[MAX_VERTEX_BUFFER_SLOTS];
        uint32_t                           m_CurrentVertexBufferBindingOffsets[MAX_VERTEX_BUFFER_SLOTS];
        MTL::Resource*                     m_RenderUsedResources[MAX_ENCODER_RESOURCE_CACHE];
        MTL::ResourceUsage                 m_RenderUsedResourceUsage[MAX_ENCODER_RESOURCE_CACHE];
        MTL::Resource*                     m_ComputeUsedResources[MAX_ENCODER_RESOURCE_CACHE];
        MTL::ResourceUsage                 m_ComputeUsedResourceUsage[MAX_ENCODER_RESOURCE_CACHE];
        uint16_t                           m_RenderUsedResourceCount;
        uint16_t                           m_ComputeUsedResourceCount;

        MetalTexture*                      m_DefaultTexture2D;
        MetalTexture*                      m_DefaultTexture2DArray;
        MetalTexture*                      m_DefaultTextureCubeMap;
        MetalTexture*                      m_DefaultTexture2D32UI;
        MetalTexture*                      m_DefaultStorageImage2D;

        uint32_t                           m_MSAASampleCount         : 8;
        uint32_t                           m_CurrentFrameInFlight    : 2;
        uint32_t                           m_NumFramesInFlight       : 2;
        uint32_t                           m_RenderTargetBound       : 1;
        uint32_t                           m_MainRTBegunThisFrame    : 1;
        uint32_t                           m_ViewportChanged         : 1;
        uint32_t                           m_CullFaceChanged         : 1;
        uint32_t                           m_FrameBegun              : 1;
        uint32_t                           m_ASTCSupport             : 1;
        // See OpenGL backend: separate flag for ASTC array textures
        uint32_t                           m_ASTCArrayTextureSupport : 1;
        uint32_t                           m_AsyncProcessingSupport  : 1;
    };
}

#endif // DMGRAPHICS_GRAPHICS_DEVICE_METAL_H
