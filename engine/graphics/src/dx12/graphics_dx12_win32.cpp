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

#include "graphics.h"
#include "dlib/dstrings.h"
#include "graphics_dx12_private.h"
#include <platform/platform_window_win32.h>

namespace dmGraphics
{

extern DX12Context* g_DX12Context; // For CHECK_HR_ERROR

static void SetupDX12Context(const ContextParams& params, DX12Context* context)
{
    memset(context, 0, sizeof(*context));
    context->m_NumFramesInFlight       = MAX_FRAMES_IN_FLIGHT;
    context->m_DefaultTextureMinFilter = params.m_DefaultTextureMinFilter;
    context->m_DefaultTextureMagFilter = params.m_DefaultTextureMagFilter;
    context->m_VerifyGraphicsCalls     = params.m_VerifyGraphicsCalls;
    context->m_PrintDeviceInfo         = params.m_PrintDeviceInfo;
    context->m_Window                  = params.m_Window;
    context->m_Width                   = params.m_Width;
    context->m_Height                  = params.m_Height;
    context->m_UseValidationLayers     = params.m_UseValidationLayers;

    context->m_TextureFormatSupport |= 1 << TEXTURE_FORMAT_LUMINANCE;
    context->m_TextureFormatSupport |= 1 << TEXTURE_FORMAT_LUMINANCE_ALPHA;
    context->m_TextureFormatSupport |= 1 << TEXTURE_FORMAT_RGB;
    context->m_TextureFormatSupport |= 1 << TEXTURE_FORMAT_RGBA;
    context->m_TextureFormatSupport |= 1 << TEXTURE_FORMAT_RGB_16BPP;
    context->m_TextureFormatSupport |= 1 << TEXTURE_FORMAT_RGBA_16BPP;
}

static IDXGIFactory4* CreateDXGIFactory()
{
    IDXGIFactory4* factory;
    HRESULT hr = CreateDXGIFactory1(IID_PPV_ARGS(&factory));
    if (FAILED(hr))
    {
        return 0;
    }
    return factory;
}

static IDXGIAdapter1* CreateDeviceAdapter(IDXGIFactory4* dxgiFactory)
{
    IDXGIAdapter1* adapter = 0;
    int adapterIndex = 0;

    // find first hardware gpu that supports d3d 12
    while (dxgiFactory->EnumAdapters1(adapterIndex, &adapter) != DXGI_ERROR_NOT_FOUND)
    {
        DXGI_ADAPTER_DESC1 desc;
        adapter->GetDesc1(&desc);

        if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
        {
            adapterIndex++;
            continue;
        }

        // we want a device that is compatible with direct3d 12 (feature level 11 or higher)
        HRESULT hr = D3D12CreateDevice(adapter, D3D_FEATURE_LEVEL_11_0, _uuidof(ID3D12Device), NULL);
        if (SUCCEEDED(hr))
        {
            break;
        }

        adapterIndex++;
    }

    return adapter;
}

static void SetupSampleDesc(DXGI_SAMPLE_DESC* sample_desc)
{
    // Note: These must be 1 and 0 - for MSAA we will render to an offscreen texture that is multisampled
    sample_desc->Count   = 1;
    sample_desc->Quality = 0;
}

DX12Context* DX12NativeCreate(const struct ContextParams& params)
{
    DX12Context* context = new DX12Context;
    g_DX12Context = context; // for CHECK_HR_ERROR to work

    SetupDX12Context(params, context);

    HRESULT hr = S_OK;

    context->m_UseValidationLayers = true;

    // This needs to be created before the device
    if (context->m_UseValidationLayers)
    {
        hr = D3D12GetDebugInterface(IID_PPV_ARGS(&context->m_DebugInterface));
        CHECK_HR_ERROR(hr);

        context->m_DebugInterface->EnableDebugLayer();
        context->m_DebugInterface->Release();
    }

    IDXGIFactory4* factory = CreateDXGIFactory();
    IDXGIAdapter1* adapter = CreateDeviceAdapter(factory);

    hr = D3D12CreateDevice(adapter, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&context->m_Device));
    CHECK_HR_ERROR(hr);

    D3D12_COMMAND_QUEUE_DESC cmd_queue_desc = {};
    hr = context->m_Device->CreateCommandQueue(&cmd_queue_desc, IID_PPV_ARGS(&context->m_CommandQueue));
    CHECK_HR_ERROR(hr);

    uint32_t window_width = dmPlatform::GetWindowWidth(context->m_Window);
    uint32_t window_height = dmPlatform::GetWindowHeight(context->m_Window);

    // Create swapchain
    DXGI_MODE_DESC back_buffer_desc = {};
    back_buffer_desc.Width          = window_width;
    back_buffer_desc.Height         = window_height;
    back_buffer_desc.Format         = DXGI_FORMAT_R8G8B8A8_UNORM;

    DXGI_SAMPLE_DESC sample_desc = {};
    SetupSampleDesc(&sample_desc);

    DXGI_SWAP_CHAIN_DESC swap_chain_desc = {};
    swap_chain_desc.BufferCount          = MAX_FRAMEBUFFERS;
    swap_chain_desc.BufferDesc           = back_buffer_desc;
    swap_chain_desc.BufferUsage          = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swap_chain_desc.SwapEffect           = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    swap_chain_desc.OutputWindow         = GetWindowsHWND(context->m_Window);
    swap_chain_desc.SampleDesc           = sample_desc;
    swap_chain_desc.Windowed             = true;

    IDXGISwapChain* swap_chain_tmp = 0;
    factory->CreateSwapChain(context->m_CommandQueue, &swap_chain_desc, &swap_chain_tmp);
    context->m_SwapChain = static_cast<IDXGISwapChain3*>(swap_chain_tmp);

    return context;
}

DXGI_FORMAT DX12GetBackBufferFormat()
{
    return DXGI_FORMAT_R8G8B8A8_UNORM;
}


bool DX12NativeInitialize(DX12Context* context)
{
    HRESULT hr = S_OK;

    for (int i = 0; i < MAX_FRAMEBUFFERS; i++)
    {
        // first we get the n'th buffer in the swap chain and store it in the n'th
        // position of our ID3D12Resource array
        hr = context->m_SwapChain->GetBuffer(i, IID_PPV_ARGS(&context->m_FrameResources[i].m_RenderTarget.m_Resource));
        CHECK_HR_ERROR(hr);
    }

    DXGI_SAMPLE_DESC sample_desc = {};
    SetupSampleDesc(&sample_desc);

    SetupMainRenderTarget(context, sample_desc);

    if (context->m_UseValidationLayers)
    {
        // We install a custom message filter here to avoid the spam that occurs when issuing clear commands
        // that contains colors that ARE NOT the same as the values that was used when the RT was created.
        // This will be slower, but we would have to change the API to fix it..
        ID3D12InfoQueue* infoQueue = nullptr;
        if (SUCCEEDED(context->m_Device->QueryInterface(IID_PPV_ARGS(&infoQueue))))
        {
            // Define the warning to suppress
            D3D12_MESSAGE_ID messageId = D3D12_MESSAGE_ID_CLEARRENDERTARGETVIEW_MISMATCHINGCLEARVALUE;

            // Set up a filter to ignore the warning
            D3D12_MESSAGE_SEVERITY severities[] = { D3D12_MESSAGE_SEVERITY_WARNING };
            D3D12_MESSAGE_ID denyIds[] = { messageId };

            D3D12_INFO_QUEUE_FILTER filter = {};
            filter.DenyList.NumSeverities = _countof(severities);
            filter.DenyList.pSeverityList = severities;
            filter.DenyList.NumIDs = _countof(denyIds);
            filter.DenyList.pIDList = denyIds;

            // Apply the filter
            infoQueue->PushStorageFilter(&filter);
            infoQueue->Release();
        }
    }

    return true;
}

void DX12NativeDestroy(DX12Context* context)
{
    for (uint8_t i=0; i < DM_ARRAY_SIZE(context->m_FrameResources); i++)
    {
        FlushResourcesToDestroy(context->m_FrameResources[i]);
    }

    delete context;
}

bool DX12IsSupported()
{
    IDXGIAdapter1* adapter = CreateDeviceAdapter(CreateDXGIFactory());
    if (adapter)
    {
        adapter->Release();
        return true;
    }
    return false;
}

void DX12NativeBeginFrame(DX12Context* context)
{
    // swap the current rtv buffer index so we draw on the correct buffer
    context->m_CurrentFrameIndex = context->m_SwapChain->GetCurrentBackBufferIndex();
    SyncronizeFrame(context);
}

void DX12NativeEndFrame(DX12Context* context)
{
    HRESULT hr = context->m_SwapChain->Present(0, 0);
    CHECK_HR_ERROR(hr);
}

}
