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

#ifndef DMGRAPHICS_GRAPHICS_DEVICE_DX12_H
#define DMGRAPHICS_GRAPHICS_DEVICE_DX12_H

#include <dlib/math.h>
#include <dlib/mutex.h>
#include <dlib/hashtable.h>
#include <dlib/opaque_handle_container.h>

#include <dmsdk/vectormath/cpp/vectormath_aos.h>

#include <platform/platform_window.h>

namespace dmGraphics
{
    const static uint8_t MAX_FRAMES_IN_FLIGHT = 2;
    const static uint8_t MAX_FRAMEBUFFERS     = 3;
    const static uint8_t MAX_VERTEX_BUFFERS   = 2;

    typedef ID3D12PipelineState*          DX12Pipeline;
    typedef dmHashTable64<DX12Pipeline>   DX12PipelineCache;

    struct DX12Texture
    {
        TextureType       m_Type;
        uint16_t          m_Width;
        uint16_t          m_Height;
        uint16_t          m_Depth;
        uint16_t          m_OriginalWidth;
        uint16_t          m_OriginalHeight;
        uint16_t          m_MipMapCount         : 5;
    };

    struct DX12VertexBuffer
    {
        ID3D12Resource*          m_Resource;
        D3D12_VERTEX_BUFFER_VIEW m_View;
        uint32_t                 m_DataSize;
    };

    struct DX12ShaderModule
    {
        ID3DBlob*                      m_ShaderBlob;
        dmArray<ShaderResourceBinding> m_Uniforms;
        dmArray<ShaderResourceBinding> m_Inputs;
        uint16_t                       m_UniformBufferCount;
        uint16_t                       m_TextureSamplerCount;
        uint16_t                       m_TotalUniformCount;
    };

    struct DX12Viewport
    {
        uint16_t m_X;
        uint16_t m_Y;
        uint16_t m_W;
        uint16_t m_H;
    };

    struct DX12ShaderProgram
    {
        DX12ShaderModule* m_VertexModule;
        DX12ShaderModule* m_FragmentModule;
        DX12ShaderModule* m_ComputeModule;
        uint16_t          m_TotalUniformCount;
    };

    struct DX12RenderTarget
    {
        ID3D12Resource*  m_Resource;
        DXGI_FORMAT      m_Format;
        DXGI_SAMPLE_DESC m_SampleDesc;
        uint16_t         m_Id;
        uint32_t         m_IsBound : 1;
    };

    struct DX12FrameResource
    {
        DX12RenderTarget        m_RenderTarget;
        ID3D12CommandAllocator* m_CommandAllocator;
        ID3D12Fence*            m_Fence;
        uint64_t                m_FenceValue;
    };

    struct DX12Context
    {
        DX12Context(const ContextParams& params);

        ID3D12Device*                      m_Device;
        IDXGISwapChain3*                   m_SwapChain;
        ID3D12CommandQueue*                m_CommandQueue;
        ID3D12DescriptorHeap*              m_RtvDescriptorHeap;
        ID3D12GraphicsCommandList*         m_CommandList;
        ID3D12Debug*                       m_DebugInterface;
        ID3D12RootSignature*               m_RootSignature;
        HANDLE                             m_FenceEvent;
        DX12FrameResource                  m_FrameResources[MAX_FRAMEBUFFERS];
        CD3DX12_CPU_DESCRIPTOR_HANDLE      m_RtvHandle;

        dmPlatform::HWindow                m_Window;
        dmOpaqueHandleContainer<uintptr_t> m_AssetHandleContainer;
        DX12PipelineCache                  m_PipelineCache;
        PipelineState                      m_PipelineState;

        HRenderTarget                      m_MainRenderTarget;
        VertexDeclaration                  m_MainVertexDeclaration[MAX_VERTEX_BUFFERS];

        HRenderTarget                      m_CurrentRenderTarget;
        DX12ShaderProgram*                 m_CurrentProgram;
        DX12Pipeline*                      m_CurrentPipeline;
        DX12VertexBuffer*                  m_CurrentVertexBuffer[MAX_VERTEX_BUFFERS];
        VertexDeclaration*                 m_CurrentVertexDeclaration[MAX_VERTEX_BUFFERS];
        DX12Viewport                       m_CurrentViewport;

        TextureFilter                      m_DefaultTextureMinFilter;
        TextureFilter                      m_DefaultTextureMagFilter;
        uint64_t                           m_TextureFormatSupport;
        uint32_t                           m_Width;
        uint32_t                           m_Height;
        uint32_t                           m_CurrentFrameIndex;
        uint32_t                           m_RtvDescriptorSize;
        uint32_t                           m_NumFramesInFlight    : 2;
        uint32_t                           m_FrameBegun           : 1;
        uint32_t                           m_CullFaceChanged      : 1;
        uint32_t                           m_ViewportChanged      : 1;
        uint32_t                           m_VerifyGraphicsCalls  : 1;
        uint32_t                           m_UseValidationLayers  : 1;
        uint32_t                           m_PrintDeviceInfo      : 1;
    };
}

#endif // DMGRAPHICS_GRAPHICS_DEVICE_DX12_H
