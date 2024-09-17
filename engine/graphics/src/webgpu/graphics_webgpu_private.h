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

#ifndef GRAPHICS_DEVICE_WEBGPU
#define GRAPHICS_DEVICE_WEBGPU

#include <vector>

#include <dlib/hashtable.h>
#include <dmsdk/dlib/vmath.h>
#include <dlib/opaque_handle_container.h>

#include "../graphics_private.h"

#ifdef __EMSCRIPTEN__
# include <webgpu/webgpu.h>
#endif

namespace dmGraphics
{
    const static uint32_t MAX_TEXTURE_COUNT  = 32;
    const static uint8_t MAX_VERTEX_BUFFERS = 2;

    struct WebGPUBuffer
    {
        WebGPUBuffer(WGPUBufferUsageFlags usage) : m_Usage(usage) { }

        WGPUBuffer                      m_Buffer = NULL;
        const WGPUBufferUsageFlags      m_Usage;

        size_t                          m_Size = 0;
        size_t                          m_Used = 0;
    };

    struct WebGPUUniformBuffer
    {
        struct Alloc
        {
            WGPUBuffer    m_Buffer = NULL;
            size_t        m_Used = 0;
            size_t        m_Size = 0;
        };
        std::vector<Alloc*> m_Allocs;
        size_t              m_Alloc = 0;
    };

    struct WebGPUShaderModule
    {
        WGPUShaderModule                m_Module = NULL;

        ShaderMeta                      m_ShaderMeta;
        uint64_t                        m_Hash;
    };

    struct WebGPUProgram
    {
        WebGPUProgram() {
            memset(this, 0, sizeof(*this));
        }

        WebGPUShaderModule*             m_VertexModule;
        WebGPUShaderModule*             m_FragmentModule;
        WebGPUShaderModule*             m_ComputeModule;

        WGPUBindGroupLayout             m_BindGroupLayouts[MAX_SET_COUNT];
        WGPUBindGroup                   m_BindGroups[MAX_SET_COUNT];
        WGPUPipelineLayout              m_PipelineLayout;

        uint64_t                        m_Hash;
        uint8_t*                        m_UniformData;
        ProgramResourceBinding          m_ResourceBindings[MAX_SET_COUNT][MAX_BINDINGS_PER_SET_COUNT];

        uint32_t                        m_UniformDataSizeAligned;
        uint16_t                        m_UniformBufferCount;
        uint16_t                        m_StorageBufferCount;
        uint16_t                        m_TextureSamplerCount;
        uint16_t                        m_TotalResourcesCount;
        uint16_t                        m_TotalUniformCount;

        uint8_t                         m_MaxSet;
        uint8_t                         m_MaxBinding;
        uint8_t                         m_Destroyed : 1;
    };

    struct WebGPUTexture
    {
        WebGPUTexture() {
            memset(this, 0, sizeof(*this));
            m_Type                = TEXTURE_TYPE_2D;
            m_GraphicsFormat      = TEXTURE_FORMAT_RGBA;
            m_Format              = WGPUTextureFormat_Undefined;
            m_UsageFlags          = WGPUTextureUsage_None;
            m_Texture             = NULL;
            m_TextureView         = NULL;
            m_Sampler             = NULL;
        }

        WGPUTexture       m_Texture;
        WGPUTextureView   m_TextureView;
        WGPUSampler       m_Sampler;
        TextureType       m_Type;
        TextureFormat     m_GraphicsFormat;
        WGPUTextureFormat m_Format;
        WGPUTextureUsageFlags m_UsageFlags;
        uint32_t          m_Width;
        uint32_t          m_Height;
        uint16_t          m_Depth;
        uint32_t          m_OriginalWidth;
        uint32_t          m_OriginalHeight;
        uint16_t          m_MipMapCount         : 5;
        uint16_t          m_TextureSamplerIndex : 10;
        uint32_t          m_Destroyed           : 1;
        uint32_t          m_UsageHintFlags      : 8;
    };

    struct WebGPURenderTarget
    {
        WebGPURenderTarget() {
            memset(this, 0, sizeof(*this));
        }

        AttachmentOp   m_ColorBufferLoadOps[MAX_BUFFER_COLOR_ATTACHMENTS];
        AttachmentOp   m_ColorBufferStoreOps[MAX_BUFFER_COLOR_ATTACHMENTS];
        float          m_ColorBufferClearValue[MAX_BUFFER_COLOR_ATTACHMENTS][4];

        HTexture       m_TextureResolve[MAX_BUFFER_COLOR_ATTACHMENTS];
        HTexture       m_TextureColor[MAX_BUFFER_COLOR_ATTACHMENTS];
        HTexture       m_TextureDepthStencil;

        uint32_t       m_Width;
        uint32_t       m_Height;
        float          m_Scissor[4];
        uint8_t        m_Multisample;
        uint32_t       m_ColorBufferCount : 7;
    };

    struct WebGPUComputePass
    {
        WGPUComputePassEncoder m_Encoder;
        WGPUComputePipeline    m_Pipeline;
    };

    struct WebGPURenderPass
    {
        WebGPURenderTarget*    m_Target;
        WGPURenderPassEncoder  m_Encoder;
        WGPURenderPipeline     m_Pipeline;
        WGPUBindGroup          m_BindGroups[MAX_SET_COUNT];
        WebGPUBuffer*          m_VertexBuffers[MAX_VERTEX_BUFFERS];
        WebGPUBuffer*          m_IndexBuffer;
    };

    struct WebGPUTextureSampler
    {
        WGPUSampler   m_Sampler;
        TextureFilter m_MinFilter;
        TextureFilter m_MagFilter;
        TextureWrap   m_AddressModeU;
        TextureWrap   m_AddressModeV;
        float         m_MaxAnisotropy;
        uint8_t       m_MaxLod;
    };

    struct WebGPUContext
    {
        WebGPUContext(const ContextParams& params);
        ~WebGPUContext();
        bool initialize(const ContextParams& params);

        static void instanceRequestAdapterCallback(WGPURequestAdapterStatus status, WGPUAdapter adapterArg, const char *message, void *userdata);
        static void requestDeviceCallback(WGPURequestDeviceStatus status, WGPUDevice deviceArg, const char *message, void *userdata);

        dmPlatform::HWindow                m_Window;
        dmOpaqueHandleContainer<uintptr_t> m_AssetHandleContainer;

        TextureFilter                      m_DefaultTextureMinFilter;
        TextureFilter                      m_DefaultTextureMagFilter;
        WebGPUTexture*                     m_DefaultTexture2D;
        WebGPUTexture*                     m_DefaultTexture2DArray;
        WebGPUTexture*                     m_DefaultTextureCubeMap;
        WebGPUTexture*                     m_DefaultTexture2D32UI;
        WebGPUTexture*                     m_DefaultStorageImage2D;

        uint32_t                           m_OriginalWidth;
        uint32_t                           m_OriginalHeight;
        uint32_t                           m_Width;
        uint32_t                           m_Height;
        int32_t                            m_ScissorRect[4];
        int32_t                            m_ViewportRect[4];
        uint64_t                           m_TextureFormatSupport;
        VertexDeclaration                  m_VertexDeclaration[MAX_VERTEX_BUFFERS];
        WebGPURenderTarget*                m_MainRenderTarget;

        uint32_t                           m_PrintDeviceInfo : 1;
        uint32_t                           m_ContextFeatures : 3;
        uint32_t                           m_ViewportChanged : 1;

        bool                               m_InitComplete;
        WGPUInstance                       m_Instance;
        WGPUAdapter                        m_Adapter;
        WGPUSupportedLimits                m_AdapterLimits;
        WGPUDevice                         m_Device;
        WGPUSupportedLimits                m_DeviceLimits;
        WGPUQueue                          m_Queue;
        WGPUSurface                        m_Surface;
        WGPUTextureFormat                  m_Format;
        WGPUSwapChain                      m_SwapChain;

        WGPUCommandEncoder                 m_CommandEncoder;
        dmHashTable64<WGPURenderPipeline>  m_RenderPipelineCache;
        dmHashTable64<WGPUComputePipeline> m_ComputePipelineCache;
        dmHashTable64<WGPUBindGroup>       m_BindGroupCache;
        dmHashTable64<WGPUSampler>         m_SamplerCache;

        // Current state
        PipelineState                      m_CurrentPipelineState;
        WebGPURenderPass                   m_CurrentRenderPass;
        WebGPUComputePass                  m_CurrentComputePass;
        WebGPUUniformBuffer                m_CurrentUniforms;
        WebGPUProgram*                     m_CurrentProgram;
        WebGPURenderTarget*                m_CurrentRenderTarget;
        WebGPUBuffer*                      m_CurrentVertexBuffers[MAX_VERTEX_BUFFERS];
        WebGPUTexture*                     m_CurrentTextureUnits[MAX_TEXTURE_COUNT];
        VertexDeclaration*                 m_CurrentVertexDeclaration[MAX_VERTEX_BUFFERS];
        //StorageBufferBinding             m_CurrentStorageBuffers[MAX_STORAGE_BUFFERS];

    };
}

#endif // __GRAPHICS_DEVICE_WEBGPU__
