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

#include <string.h>
#include <assert.h>

#include <d3d12.h>
#include <d3dx12.h>
#include <D3DCompiler.h>

#include <dxgi1_6.h>

#include <dmsdk/dlib/vmath.h>

#include <dlib/array.h>
#include <dlib/dstrings.h>
#include <dlib/log.h>
#include <dlib/math.h>

#include <platform/platform_window.h>

#include <graphics/glfw/glfw_native.h>

#include "../graphics_private.h"
#include "../graphics_native.h"
#include "../graphics_adapter.h"

#include "graphics_dx12_private.h"


namespace dmGraphics
{
    static GraphicsAdapterFunctionTable DX12RegisterFunctionTable();
    static bool                         DX12IsSupported();
    static bool                         DX12Initialize(HContext _context);
    static const int8_t    g_dx12_adapter_priority = 0;
    static GraphicsAdapter g_dx12_adapter(ADAPTER_FAMILY_DIRECTX);
    static DX12Context*    g_DX12Context = 0x0;

    DM_REGISTER_GRAPHICS_ADAPTER(GraphicsAdapterDX12, &g_dx12_adapter, DX12IsSupported, DX12RegisterFunctionTable, g_dx12_adapter_priority);

    #define CHECK_HR_ERROR(result) \
    { \
        if(g_DX12Context->m_VerifyGraphicsCalls && result != S_OK) { \
            dmLogError("DX Error (%s:%d) code: %d", __FILE__, __LINE__, HRESULT_CODE(result)); \
            assert(0); \
        } \
    }

    DX12Context::DX12Context(const ContextParams& params)
    {
        memset(this, 0, sizeof(*this));
        m_NumFramesInFlight       = MAX_FRAMES_IN_FLIGHT;
        m_DefaultTextureMinFilter = params.m_DefaultTextureMinFilter;
        m_DefaultTextureMagFilter = params.m_DefaultTextureMagFilter;
        m_VerifyGraphicsCalls     = params.m_VerifyGraphicsCalls;
        m_PrintDeviceInfo         = params.m_PrintDeviceInfo;
        m_Window                  = params.m_Window;
        m_Width                   = params.m_Width;
        m_Height                  = params.m_Height;
        m_UseValidationLayers     = params.m_UseValidationLayers;

        m_TextureFormatSupport |= 1 << TEXTURE_FORMAT_LUMINANCE;
        m_TextureFormatSupport |= 1 << TEXTURE_FORMAT_LUMINANCE_ALPHA;
        m_TextureFormatSupport |= 1 << TEXTURE_FORMAT_RGB;
        m_TextureFormatSupport |= 1 << TEXTURE_FORMAT_RGBA;
        m_TextureFormatSupport |= 1 << TEXTURE_FORMAT_RGB_16BPP;
        m_TextureFormatSupport |= 1 << TEXTURE_FORMAT_RGBA_16BPP;

        assert(dmPlatform::GetWindowStateParam(m_Window, dmPlatform::WINDOW_STATE_OPENED));
    }

    static HContext DX12NewContext(const ContextParams& params)
    {
        if (!g_DX12Context)
        {
            g_DX12Context = new DX12Context(params);

            if (DX12Initialize(g_DX12Context))
            {
                return g_DX12Context;
            }

            DeleteContext(g_DX12Context);
        }
        return 0x0;
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

    static bool DX12IsSupported()
    {
        IDXGIAdapter1* adapter = CreateDeviceAdapter(CreateDXGIFactory());
        if (adapter)
        {
            adapter->Release();
            return true;
        }
        return false;
    }

    static void DX12DeleteContext(HContext _context)
    {
        assert(_context);
        if (g_DX12Context)
        {
            DX12Context* context = (DX12Context*) _context;
            delete (DX12Context*) context;
            g_DX12Context = 0x0;
        }
    }

    static void SetupMainRenderTarget(DX12Context* context)
    {
        // Initialize the dummy rendertarget for the main framebuffer
        // The m_Framebuffer construct will be rotated sequentially
        // with the framebuffer objects created per swap chain.
        DX12RenderTarget* rt = GetAssetFromContainer<DX12RenderTarget>(context->m_AssetHandleContainer, context->m_MainRenderTarget);
        assert(rt == 0x0);

        rt       = new DX12RenderTarget();
        rt->m_Id = DM_RENDERTARGET_BACKBUFFER_ID;

        context->m_MainRenderTarget = StoreAssetInContainer(context->m_AssetHandleContainer, rt, ASSET_TYPE_RENDER_TARGET);

        // rt->m_Handle.m_RenderPass  = context->m_MainRenderPass;
        // rt->m_Handle.m_Framebuffer = context->m_MainFrameBuffers[0];
        // rt->m_Extent               = context->m_SwapChain->m_ImageExtent;
        // rt->m_ColorAttachmentCount = 1;
    }

    static bool DX12Initialize(HContext _context)
    {
        DX12Context* context = (DX12Context*) _context;

        HRESULT hr = S_OK;

        // This needs to be created before the device
        if (context->m_UseValidationLayers)
        {
            hr = D3D12GetDebugInterface(IID_PPV_ARGS(&context->m_DebugInterface));
            CHECK_HR_ERROR(hr);

            context->m_DebugInterface->EnableDebugLayer(); // TODO: Release
        }

        IDXGIFactory4* factory = CreateDXGIFactory();
        IDXGIAdapter1* adapter = CreateDeviceAdapter(factory);

        hr = D3D12CreateDevice(adapter, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&context->m_Device));
        CHECK_HR_ERROR(hr);

        D3D12_COMMAND_QUEUE_DESC cmd_queue_desc = {};
        hr = context->m_Device->CreateCommandQueue(&cmd_queue_desc, IID_PPV_ARGS(&context->m_CommandQueue));
        CHECK_HR_ERROR(hr);

        // Create swapchain
        DXGI_MODE_DESC back_buffer_desc = {};
        back_buffer_desc.Width          = context->m_Width;
        back_buffer_desc.Height         = context->m_Height;
        back_buffer_desc.Format         = DXGI_FORMAT_R8G8B8A8_UNORM;

        DXGI_SAMPLE_DESC sample_desc = {};
        sample_desc.Count            = 1;

        DXGI_SWAP_CHAIN_DESC swap_chain_desc = {};
        swap_chain_desc.BufferCount          = MAX_FRAMEBUFFERS;
        swap_chain_desc.BufferDesc           = back_buffer_desc;
        swap_chain_desc.BufferUsage          = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        swap_chain_desc.SwapEffect           = DXGI_SWAP_EFFECT_FLIP_DISCARD;
        swap_chain_desc.OutputWindow         = glfwGetWindowsHWND();
        swap_chain_desc.SampleDesc           = sample_desc;
        swap_chain_desc.Windowed             = true;

        IDXGISwapChain* swap_chain_tmp = 0;
        factory->CreateSwapChain(context->m_CommandQueue, &swap_chain_desc, &swap_chain_tmp);
        context->m_SwapChain = static_cast<IDXGISwapChain3*>(swap_chain_tmp);

        // frameIndex = swapChain->GetCurrentBackBufferIndex();

        // this heap is a render target view heap
        D3D12_DESCRIPTOR_HEAP_DESC rt_view_heap_desc = {};
        rt_view_heap_desc.NumDescriptors             = MAX_FRAMEBUFFERS;
        rt_view_heap_desc.Type                       = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
        rt_view_heap_desc.Flags                      = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

        hr = context->m_Device->CreateDescriptorHeap(&rt_view_heap_desc, IID_PPV_ARGS(&context->m_RtvDescriptorHeap));
        CHECK_HR_ERROR(hr);

        context->m_RtvDescriptorSize = context->m_Device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

        // get a handle to the first descriptor in the descriptor heap. a handle is basically a pointer,
        // but we cannot literally use it like a c++ pointer.
        CD3DX12_CPU_DESCRIPTOR_HANDLE rtv_handle(context->m_RtvDescriptorHeap->GetCPUDescriptorHandleForHeapStart());

        for (int i = 0; i < MAX_FRAMEBUFFERS; i++)
        {
            // first we get the n'th buffer in the swap chain and store it in the n'th
            // position of our ID3D12Resource array
            hr = context->m_SwapChain->GetBuffer(i, IID_PPV_ARGS(&context->m_FrameResources[i].m_RenderTarget.m_Resource));
            CHECK_HR_ERROR(hr);

            // the we "create" a render target view which binds the swap chain buffer (ID3D12Resource[n]) to the rtv handle
            context->m_Device->CreateRenderTargetView(context->m_FrameResources[i].m_RenderTarget.m_Resource, NULL, rtv_handle);

            // we increment the rtv handle by the rtv descriptor size we got above
            rtv_handle.Offset(1, context->m_RtvDescriptorSize);

            hr = context->m_Device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&context->m_FrameResources[i].m_CommandAllocator));
            CHECK_HR_ERROR(hr);

            // Create the frame fence that will be signaled when we can render to this frame
            hr = context->m_Device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&context->m_FrameResources[i].m_Fence));
            CHECK_HR_ERROR(hr);

            context->m_FrameResources[i].m_FenceValue = RENDER_CONTEXT_STATE_FREE;
        }


        context->m_FenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
        if (!context->m_FenceEvent)
        {
            dmLogFatal("Unable to create fence event");
            return false;
        }

        // command buffer / command list
        // TODO: We should create one of these for every thread we have that are recording commands
        hr = context->m_Device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, context->m_FrameResources[0].m_CommandAllocator, NULL, IID_PPV_ARGS(&context->m_CommandList));
        CHECK_HR_ERROR(hr);

        context->m_CommandList->Close();

        SetupMainRenderTarget(context);

        context->m_CurrentRenderTarget = context->m_MainRenderTarget;

        if (context->m_PrintDeviceInfo)
        {
            dmLogInfo("Device: DirectX 12");
        }
        return true;
    }

    static void DX12Finalize()
    {

    }

    static void DX12CloseWindow(HContext _context)
    {
        DX12Context* context = (DX12Context*) _context;

        if (dmPlatform::GetWindowStateParam(context->m_Window, dmPlatform::WINDOW_STATE_OPENED))
        {
        }
    }

    static void DX12RunApplicationLoop(void* user_data, WindowStepMethod step_method, WindowIsRunning is_running)
    {
    }

    static dmPlatform::HWindow DX12GetWindow(HContext _context)
    {
        DX12Context* context = (DX12Context*) _context;
        return context->m_Window;
    }

    static uint32_t DX12GetDisplayDpi(HContext context)
    {
        assert(context);
        return 0;
    }

    static uint32_t DX12GetWidth(HContext _context)
    {
        DX12Context* context = (DX12Context*) _context;
        return context->m_Width;
    }

    static uint32_t DX12GetHeight(HContext _context)
    {
        DX12Context* context = (DX12Context*) _context;
        return context->m_Height;
    }

    static void DX12SetWindowSize(HContext _context, uint32_t width, uint32_t height)
    {
        assert(_context);
        DX12Context* context = (DX12Context*) _context;
        if (dmPlatform::GetWindowStateParam(context->m_Window, dmPlatform::WINDOW_STATE_OPENED))
        {
            dmPlatform::SetWindowSize(context->m_Window, width, height);
        }
    }

    static void DX12ResizeWindow(HContext _context, uint32_t width, uint32_t height)
    {
        assert(_context);
        DX12Context* context = (DX12Context*) _context;
        if (dmPlatform::GetWindowStateParam(context->m_Window, dmPlatform::WINDOW_STATE_OPENED))
        {
            dmPlatform::SetWindowSize(context->m_Window, width, height);
        }
    }

    static void DX12GetDefaultTextureFilters(HContext _context, TextureFilter& out_min_filter, TextureFilter& out_mag_filter)
    {
        DX12Context* context = (DX12Context*) _context;
        out_min_filter = context->m_DefaultTextureMinFilter;
        out_mag_filter = context->m_DefaultTextureMagFilter;
    }

    static void DX12Clear(HContext _context, uint32_t flags, uint8_t red, uint8_t green, uint8_t blue, uint8_t alpha, float depth, uint32_t stencil)
    {
        DX12Context* context = (DX12Context*) _context;

        const float r = ((float)red)/255.0f;
        const float g = ((float)green)/255.0f;
        const float b = ((float)blue)/255.0f;
        const float a = ((float)alpha)/255.0f;
        const float cc[] = { r, g, b, a };
        context->m_CommandList->ClearRenderTargetView(context->m_RtvHandle, cc, 0, NULL);
    }

    static void SyncronizeFrame(DX12Context* context)
    {
        // swap the current rtv buffer index so we draw on the correct buffer
        context->m_CurrentFrameIndex = context->m_SwapChain->GetCurrentBackBufferIndex();

        DX12FrameResource& current_frame_resource = context->m_FrameResources[context->m_CurrentFrameIndex];

        // if the current fence value is still less than "fenceValue", then we know the GPU has not finished executing
        // the command queue since it has not reached the "commandQueue->Signal(fence, fenceValue)" command

        HRESULT hr = S_OK;

        if (current_frame_resource.m_Fence->GetCompletedValue() < current_frame_resource.m_FenceValue)
        {
            // we have the fence create an event which is signaled once the fence's current value is "fenceValue"
            hr = current_frame_resource.m_Fence->SetEventOnCompletion(current_frame_resource.m_FenceValue, context->m_FenceEvent);
            CHECK_HR_ERROR(hr);

            // We will wait until the fence has triggered the event that it's current value has reached "fenceValue". once it's value
            // has reached "fenceValue", we know the command queue has finished executing
            WaitForSingleObject(context->m_FenceEvent, INFINITE);
        }

        // increment fenceValue for next frame
        current_frame_resource.m_FenceValue++;
    }

    static bool EndRenderPass(DX12Context* context)
    {
        DX12RenderTarget* current_rt = GetAssetFromContainer<DX12RenderTarget>(context->m_AssetHandleContainer, context->m_CurrentRenderTarget);

        if (!current_rt->m_IsBound)
        {
            return false;
        }

        if (current_rt->m_Id == DM_RENDERTARGET_BACKBUFFER_ID)
        {
            context->m_CommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(current_rt->m_Resource, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT));
        }

        current_rt->m_IsBound = 0;
        return true;
    }

    static void BeginRenderPass(DX12Context* context, HRenderTarget render_target)
    {
        DX12RenderTarget* current_rt = GetAssetFromContainer<DX12RenderTarget>(context->m_AssetHandleContainer, context->m_CurrentRenderTarget);
        DX12RenderTarget* rt         = GetAssetFromContainer<DX12RenderTarget>(context->m_AssetHandleContainer, render_target);

        if (current_rt->m_Id == rt->m_Id &&
            current_rt->m_IsBound)
        {
            return;
        }

        if (current_rt->m_IsBound)
        {
            EndRenderPass(context);
        }

        if (current_rt->m_Id == DM_RENDERTARGET_BACKBUFFER_ID)
        {
            context->m_CommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(current_rt->m_Resource, D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));
        }

        CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(context->m_RtvDescriptorHeap->GetCPUDescriptorHandleForHeapStart(), context->m_CurrentFrameIndex, context->m_RtvDescriptorSize);
        context->m_RtvHandle = rtvHandle;
        context->m_CommandList->OMSetRenderTargets(1, &context->m_RtvHandle, false, NULL);

        rt->m_IsBound          = 1;
        // rt->m_SubPassIndex     = 0;
        // rt->m_Scissor.extent   = rt->m_Extent;
        // rt->m_Scissor.offset.x = 0;
        // rt->m_Scissor.offset.y = 0;

        context->m_CurrentRenderTarget = render_target;
    }

    static void DX12BeginFrame(HContext _context)
    {
        DX12Context* context = (DX12Context*) _context;
        SyncronizeFrame(context);

        DX12FrameResource& current_frame_resource = context->m_FrameResources[context->m_CurrentFrameIndex];

        HRESULT hr = current_frame_resource.m_CommandAllocator->Reset();
        CHECK_HR_ERROR(hr);

        DX12RenderTarget* rt = GetAssetFromContainer<DX12RenderTarget>(context->m_AssetHandleContainer, context->m_MainRenderTarget);
        rt->m_Resource = current_frame_resource.m_RenderTarget.m_Resource;

        // Enter "record" mode
        hr = context->m_CommandList->Reset(current_frame_resource.m_CommandAllocator, NULL); // Second argument is a pipeline object (TODO)
        CHECK_HR_ERROR(hr);

        BeginRenderPass(context, context->m_MainRenderTarget);
    }

    static void DX12Flip(HContext _context)
    {
        DX12Context* context = (DX12Context*) _context;
        EndRenderPass(context);

        DX12FrameResource& current_frame_resource = context->m_FrameResources[context->m_CurrentFrameIndex];

        // Close the command list for recording
        HRESULT hr = context->m_CommandList->Close();

        // create an array of command lists (only one command list here)
        ID3D12CommandList* execute_cmd_lists[] = { context->m_CommandList };

        // execute the array of command lists
        context->m_CommandQueue->ExecuteCommandLists(DM_ARRAY_SIZE(execute_cmd_lists), execute_cmd_lists);

        hr = context->m_CommandQueue->Signal(current_frame_resource.m_Fence, current_frame_resource.m_FenceValue);
        CHECK_HR_ERROR(hr);

        hr = context->m_SwapChain->Present(0, 0);
        CHECK_HR_ERROR(hr);
    }

    static HVertexBuffer DX12NewVertexBuffer(HContext context, uint32_t size, const void* data, BufferUsage buffer_usage)
    {
        return 0;
    }

    static void DX12DeleteVertexBuffer(HVertexBuffer buffer)
    {
    }

    static void DX12SetVertexBufferData(HVertexBuffer buffer, uint32_t size, const void* data, BufferUsage buffer_usage)
    {
    }

    static void DX12SetVertexBufferSubData(HVertexBuffer buffer, uint32_t offset, uint32_t size, const void* data)
    {
    }

    static uint32_t DX12GetMaxElementsVertices(HContext context)
    {
        return 65536;
    }

    static HIndexBuffer DX12NewIndexBuffer(HContext context, uint32_t size, const void* data, BufferUsage buffer_usage)
    {
        return 0;
    }

    static void DX12DeleteIndexBuffer(HIndexBuffer buffer)
    {
    }

    static void DX12SetIndexBufferData(HIndexBuffer buffer, uint32_t size, const void* data, BufferUsage buffer_usage)
    {
    }

    static void DX12SetIndexBufferSubData(HIndexBuffer buffer, uint32_t offset, uint32_t size, const void* data)
    {
    }

    static bool DX12IsIndexBufferFormatSupported(HContext context, IndexBufferFormat format)
    {
        return true;
    }

    static uint32_t DX12GetMaxElementsIndices(HContext context)
    {
        return 65536;
    }

    static HVertexDeclaration DX12NewVertexDeclaration(HContext context, HVertexStreamDeclaration stream_declaration)
    {
        VertexDeclaration* vd = new VertexDeclaration;
        memset(vd, 0, sizeof(VertexDeclaration));

        vd->m_Stride = 0;
        for (uint32_t i=0; i<stream_declaration->m_StreamCount; i++)
        {
            vd->m_Streams[i].m_NameHash  = stream_declaration->m_Streams[i].m_NameHash;
            vd->m_Streams[i].m_Location  = -1;
            vd->m_Streams[i].m_Size      = stream_declaration->m_Streams[i].m_Size;
            vd->m_Streams[i].m_Type      = stream_declaration->m_Streams[i].m_Type;
            vd->m_Streams[i].m_Normalize = stream_declaration->m_Streams[i].m_Normalize;
            vd->m_Streams[i].m_Offset    = vd->m_Stride;

            vd->m_Stride += stream_declaration->m_Streams[i].m_Size * GetTypeSize(stream_declaration->m_Streams[i].m_Type);
        }
        vd->m_StreamCount = stream_declaration->m_StreamCount;

        return vd;
    }

    static HVertexDeclaration DX12NewVertexDeclarationStride(HContext context, HVertexStreamDeclaration stream_declaration, uint32_t stride)
    {
        HVertexDeclaration vd = DX12NewVertexDeclaration(context, stream_declaration);
        vd->m_Stride = stride;
        return vd;
    }

    static void DX12EnableVertexBuffer(HContext _context, HVertexBuffer vertex_buffer, uint32_t binding_index)
    {
    }

    static void DX12DisableVertexBuffer(HContext _context, HVertexBuffer vertex_buffer)
    {
    }

    static void DX12EnableVertexDeclaration(HContext _context, HVertexDeclaration vertex_declaration, uint32_t binding_index, HProgram program)
    {

    }

    static void DX12DisableVertexDeclaration(HContext context, HVertexDeclaration vertex_declaration)
    {
    }

    static void DX12DrawElements(HContext _context, PrimitiveType prim_type, uint32_t first, uint32_t count, Type type, HIndexBuffer index_buffer)
    {
    }

    static void DX12Draw(HContext context, PrimitiveType prim_type, uint32_t first, uint32_t count)
    {
    }

    static HComputeProgram DX12NewComputeProgram(HContext context, ShaderDesc::Shader* ddf)
    {
        return 0;
    }

    static HProgram DX12NewProgramFromCompute(HContext context, HComputeProgram compute_program)
    {
        return (HProgram) 0;
    }

    static void DX12DeleteComputeProgram(HComputeProgram prog)
    {
    }

    static bool DX12ReloadProgramCompute(HContext context, HProgram program, HComputeProgram compute_program)
    {
        return true;
    }

    static bool DX12ReloadComputeProgram(HComputeProgram prog, ShaderDesc::Shader* ddf)
    {
        return true;
    }

    static HProgram DX12NewProgram(HContext context, HVertexProgram vertex_program, HFragmentProgram fragment_program)
    {
        DX12ShaderProgram* program = new DX12ShaderProgram();
        program->m_VertexModule    = (DX12ShaderModule*) vertex_program;
        program->m_FragmentModule  = (DX12ShaderModule*) fragment_program;
        program->m_ComputeModule   = 0;
        return (HProgram) program;
    }

    static void DX12DeleteProgram(HContext context, HProgram program)
    {
    }

    static HRESULT CreateShaderModule(DX12Context* context, const char* target, void* data, uint32_t data_size, DX12ShaderModule* shader)
    {
        ID3DBlob* error_blob;
        uint32_t flags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;

        HRESULT hr = D3DCompile(data, data_size, NULL, NULL, NULL, "main", target, flags, 0, &shader->m_ShaderBlob, &error_blob);
        if (FAILED(hr))
        {
            dmLogError("%s", error_blob->GetBufferPointer());
            return hr;
        }

        /*
        D3D12_SHADER_BYTECODE vertexShaderBytecode = {};
        vertexShaderBytecode.BytecodeLength = vertexShader->GetBufferSize();
        vertexShaderBytecode.pShaderBytecode = vertexShader->GetBufferPointer();
        */

        return S_OK;
    }

    static HVertexProgram DX12NewVertexProgram(HContext _context, ShaderDesc::Shader* ddf)
    {
        DX12ShaderModule* shader = new DX12ShaderModule;
        memset(shader, 0, sizeof(*shader));
        DX12Context* context = (DX12Context*) _context;

        HRESULT hr = CreateShaderModule(context, "vs_5_0", ddf->m_Source.m_Data, ddf->m_Source.m_Count, shader);
        CHECK_HR_ERROR(hr);
        return (HVertexProgram) shader;
    }

    static HFragmentProgram DX12NewFragmentProgram(HContext _context, ShaderDesc::Shader* ddf)
    {
        DX12ShaderModule* shader = new DX12ShaderModule;
        memset(shader, 0, sizeof(*shader));
        DX12Context* context = (DX12Context*) _context;

        HRESULT hr = CreateShaderModule(context, "ps_5_0", ddf->m_Source.m_Data, ddf->m_Source.m_Count, shader);
        CHECK_HR_ERROR(hr);
        return (HVertexProgram) shader;
    }

    static bool DX12ReloadVertexProgram(HVertexProgram prog, ShaderDesc::Shader* ddf)
    {
        return 0;
    }

    static bool DX12ReloadFragmentProgram(HFragmentProgram prog, ShaderDesc::Shader* ddf)
    {
        return 0;
    }

    static void DX12DeleteVertexProgram(HVertexProgram program)
    {
    }

    static void DX12DeleteFragmentProgram(HFragmentProgram program)
    {
    }

    static ShaderDesc::Language DX12GetProgramLanguage(HProgram program)
    {
        return (ShaderDesc::Language) 0;
    }

    static ShaderDesc::Language DX12GetShaderProgramLanguage(HContext context, ShaderDesc::ShaderClass shader_class)
    {
        return ShaderDesc::LANGUAGE_HLSL;
    }

    static void DX12EnableProgram(HContext context, HProgram program)
    {
    }

    static void DX12DisableProgram(HContext context)
    {
    }

    static bool DX12ReloadProgramGraphics(HContext context, HProgram program, HVertexProgram vert_program, HFragmentProgram frag_program)
    {
        return true;
    }

    static uint32_t DX12GetAttributeCount(HProgram prog)
    {
        return 0;
    }

    static uint32_t GetElementCount(Type type)
    {
        return 0;
    }

    static void DX12GetAttribute(HProgram prog, uint32_t index, dmhash_t* name_hash, Type* type, uint32_t* element_count, uint32_t* num_values, int32_t* location)
    {
    }

    static uint32_t DX12GetUniformCount(HProgram prog)
    {
        return 0;
    }

    static uint32_t DX12GetVertexDeclarationStride(HVertexDeclaration vertex_declaration)
    {
        return 0;
    }

    static uint32_t DX12GetVertexStreamOffset(HVertexDeclaration vertex_declaration, dmhash_t name_hash)
    {
        return dmGraphics::INVALID_STREAM_OFFSET;
    }

    static uint32_t DX12GetUniformName(HProgram prog, uint32_t index, char* buffer, uint32_t buffer_size, Type* type, int32_t* size)
    {
        return 0;
    }

    static HUniformLocation DX12GetUniformLocation(HProgram prog, const char* name)
    {
        return INVALID_UNIFORM_LOCATION;
    }

    static void DX12SetViewport(HContext context, int32_t x, int32_t y, int32_t width, int32_t height)
    {
    }

    static void DX12SetConstantV4(HContext _context, const dmVMath::Vector4* data, int count, HUniformLocation base_location)
    {
    }

    static void DX12SetConstantM4(HContext _context, const dmVMath::Vector4* data, int count, HUniformLocation base_location)
    {
    }

    static void DX12SetSampler(HContext context, HUniformLocation location, int32_t unit)
    {
    }


    static HRenderTarget DX12NewRenderTarget(HContext _context, uint32_t buffer_type_flags, const RenderTargetCreationParams params)
    {
        return 0;
    }

    static void DX12DeleteRenderTarget(HRenderTarget render_target)
    {
    }

    static void DX12SetRenderTarget(HContext _context, HRenderTarget render_target, uint32_t transient_buffer_types)
    {
    }

    static HTexture DX12GetRenderTargetTexture(HRenderTarget render_target, BufferType buffer_type)
    {
        return 0;
    }

    static void DX12GetRenderTargetSize(HRenderTarget render_target, BufferType buffer_type, uint32_t& width, uint32_t& height)
    {
    }

    static void DX12SetRenderTargetSize(HRenderTarget render_target, uint32_t width, uint32_t height)
    {
    }

    static bool DX12IsTextureFormatSupported(HContext _context, TextureFormat format)
    {
        DX12Context* context = (DX12Context*) _context;
        return (context->m_TextureFormatSupport & (1 << format)) != 0;
    }

    static uint32_t DX12GetMaxTextureSize(HContext context)
    {
        return 1024;
    }

    static HTexture DX12NewTexture(HContext _context, const TextureCreationParams& params)
    {
        return 0;
    }

    static void DX12DeleteTexture(HTexture texture)
    {
    }

    static HandleResult DX12GetTextureHandle(HTexture texture, void** out_handle)
    {

        return HANDLE_RESULT_OK;
    }

    static void DX12SetTextureParams(HTexture texture, TextureFilter minfilter, TextureFilter magfilter, TextureWrap uwrap, TextureWrap vwrap, float max_anisotropy)
    {
    }

    static void DX12SetTexture(HTexture texture, const TextureParams& params)
    {

    }

    static uint32_t DX12GetTextureResourceSize(HTexture texture)
    {
        return 0;
    }

    static uint16_t DX12GetTextureWidth(HTexture texture)
    {
        return GetAssetFromContainer<DX12Texture>(g_DX12Context->m_AssetHandleContainer, texture)->m_Width;
    }

    static uint16_t DX12GetTextureHeight(HTexture texture)
    {
        return GetAssetFromContainer<DX12Texture>(g_DX12Context->m_AssetHandleContainer, texture)->m_Height;
    }

    static uint16_t DX12GetOriginalTextureWidth(HTexture texture)
    {
        return GetAssetFromContainer<DX12Texture>(g_DX12Context->m_AssetHandleContainer, texture)->m_OriginalWidth;
    }

    static uint16_t DX12GetOriginalTextureHeight(HTexture texture)
    {
        return GetAssetFromContainer<DX12Texture>(g_DX12Context->m_AssetHandleContainer, texture)->m_OriginalHeight;
    }

    static void DX12EnableTexture(HContext _context, uint32_t unit, uint8_t value_index, HTexture texture)
    {
    }

    static void DX12DisableTexture(HContext context, uint32_t unit, HTexture texture)
    {
    }

    static void DX12ReadPixels(HContext context, void* buffer, uint32_t buffer_size)
    {
    }

    static void DX12EnableState(HContext context, State state)
    {
    }

    static void DX12DisableState(HContext context, State state)
    {
    }

    static void DX12SetBlendFunc(HContext _context, BlendFactor source_factor, BlendFactor destinaton_factor)
    {
    }

    static void DX12SetColorMask(HContext context, bool red, bool green, bool blue, bool alpha)
    {
    }

    static void DX12SetDepthMask(HContext context, bool mask)
    {
    }

    static void DX12SetDepthFunc(HContext context, CompareFunc func)
    {
    }

    static void DX12SetScissor(HContext _context, int32_t x, int32_t y, int32_t width, int32_t height)
    {
    }

    static void DX12SetStencilMask(HContext context, uint32_t mask)
    {
    }

    static void DX12SetStencilFunc(HContext _context, CompareFunc func, uint32_t ref, uint32_t mask)
    {
    }

    static void DX12SetStencilOp(HContext _context, StencilOp sfail, StencilOp dpfail, StencilOp dppass)
    {
    }

    static void DX12SetStencilFuncSeparate(HContext _context, FaceType face_type, CompareFunc func, uint32_t ref, uint32_t mask)
    {
    }

    static void DX12SetStencilOpSeparate(HContext _context, FaceType face_type, StencilOp sfail, StencilOp dpfail, StencilOp dppass)
    {
    }

    static void DX12SetFaceWinding(HContext context, FaceWinding face_winding)
    {
    }

    static void DX12SetCullFace(HContext context, FaceType face_type)
    {
    }

    static void DX12SetPolygonOffset(HContext context, float factor, float units)
    {
    }

    static PipelineState DX12GetPipelineState(HContext context)
    {
        PipelineState s = {};
        return s;
    }

    static void DX12SetTextureAsync(HTexture texture, const TextureParams& params)
    {
    }

    static uint32_t DX12GetTextureStatusFlags(HTexture texture)
    {
        return TEXTURE_STATUS_OK;
    }

    static bool DX12IsExtensionSupported(HContext context, const char* extension)
    {
        return true;
    }

    static TextureType DX12GetTextureType(HTexture texture)
    {
        return GetAssetFromContainer<DX12Texture>(g_DX12Context->m_AssetHandleContainer, texture)->m_Type;
    }

    static uint32_t DX12GetNumSupportedExtensions(HContext context)
    {
        return 0;
    }

    static const char* DX12GetSupportedExtension(HContext context, uint32_t index)
    {
        return "";
    }

    static uint8_t DX12GetNumTextureHandles(HTexture texture)
    {
        return 1;
    }

    static bool DX12IsContextFeatureSupported(HContext context, ContextFeature feature)
    {
        return true;
    }

    static uint16_t DX12GetTextureDepth(HTexture texture)
    {
        return GetAssetFromContainer<DX12Texture>(g_DX12Context->m_AssetHandleContainer, texture)->m_Depth;
    }

    static uint8_t DX12GetTextureMipmapCount(HTexture texture)
    {
        return GetAssetFromContainer<DX12Texture>(g_DX12Context->m_AssetHandleContainer, texture)->m_MipMapCount;
    }

    static bool DX12IsAssetHandleValid(HContext _context, HAssetHandle asset_handle)
    {
        assert(_context);
        if (asset_handle == 0)
        {
            return false;
        }
        DX12Context* context = (DX12Context*) _context;
        AssetType type       = GetAssetType(asset_handle);
        if (type == ASSET_TYPE_TEXTURE)
        {
            return GetAssetFromContainer<DX12Texture>(context->m_AssetHandleContainer, asset_handle) != 0;
        }
        else if (type == ASSET_TYPE_RENDER_TARGET)
        {
            return GetAssetFromContainer<DX12RenderTarget>(context->m_AssetHandleContainer, asset_handle) != 0;
        }
        return false;
    }

    static GraphicsAdapterFunctionTable DX12RegisterFunctionTable()
    {
        GraphicsAdapterFunctionTable fn_table = {};
        DM_REGISTER_GRAPHICS_FUNCTION_TABLE(fn_table, DX12);
        return fn_table;
    }
}

