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
#include <dmsdk/vectormath/cpp/vectormath_aos.h>
#include <dlib/opaque_handle_container.h>
#include <platform/platform_window.h>

namespace dmGraphics
{
    const static uint8_t MAX_FRAMES_IN_FLIGHT = 2;
    const static uint8_t MAX_FRAMEBUFFERS     = 3;

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

    struct DX12RenderTarget
    {
        int dummy;
    };

    struct DX12FrameResource
    {
        ID3D12Resource*         m_RenderTarget;
        ID3D12CommandAllocator* m_CommandAllocator;
        ID3D12Fence*            m_Fence;
        uint64_t                m_FenceValue;
    };

    struct DX12Context
    {
        DX12Context(const ContextParams& params);

        ID3D12Device*              m_Device;
        IDXGISwapChain3*           m_SwapChain;
        ID3D12CommandQueue*        m_CommandQueue;
        ID3D12DescriptorHeap*      m_RtvDescriptorHeap;
        ID3D12GraphicsCommandList* m_CommandList;
        ID3D12Debug*               m_DebugInterface;
        HANDLE                     m_FenceEvent;

        DX12FrameResource          m_FrameResources[MAX_FRAMEBUFFERS];

        dmPlatform::HWindow                m_Window;
        dmOpaqueHandleContainer<uintptr_t> m_AssetHandleContainer;
        TextureFilter                      m_DefaultTextureMinFilter;
        TextureFilter                      m_DefaultTextureMagFilter;
        uint32_t                           m_Width;
        uint32_t                           m_Height;
        uint32_t                           m_CurrentFrameIndex;
        uint32_t                           m_NumFramesInFlight    : 2;
        uint32_t                           m_VerifyGraphicsCalls  : 1;
        uint32_t                           m_UseValidationLayers  : 1;
        uint32_t                           m_PrintDeviceInfo      : 1;
    };
}

#endif // DMGRAPHICS_GRAPHICS_DEVICE_DX12_H
