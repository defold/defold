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
#include <d3d12shader.h>
#include <d3dcompiler.h>
#include <d3dx12.h>  // Optional, for helpers

#include <dxgi1_6.h>

#include <dmsdk/dlib/vmath.h>

#include <dlib/array.h>
#include <dlib/dstrings.h>
#include <dlib/log.h>
#include <dlib/math.h>
#include <dlib/profile.h>

#include <platform/platform_window.h>

#include <platform/platform_window_win32.h>

#include "../graphics_private.h"
#include "../graphics_native.h"
#include "../graphics_adapter.h"

#include "graphics_dx12_private.h"

DM_PROPERTY_EXTERN(rmtp_DrawCalls);
DM_PROPERTY_EXTERN(rmtp_DispatchCalls);

namespace dmGraphics
{
    static GraphicsAdapterFunctionTable DX12RegisterFunctionTable();
    static bool                         DX12IsSupported();
    static HContext                     DX12GetContext();
    static bool                         DX12Initialize(HContext _context);
    static GraphicsAdapter g_dx12_adapter(ADAPTER_FAMILY_DIRECTX);
    static DX12Context*    g_DX12Context = 0x0;

    DM_REGISTER_GRAPHICS_ADAPTER(GraphicsAdapterDX12, &g_dx12_adapter, DX12IsSupported, DX12RegisterFunctionTable, DX12GetContext, ADAPTER_FAMILY_PRIORITY_DIRECTX);

    static int16_t CreateTextureSampler(DX12Context* context, TextureFilter minfilter, TextureFilter magfilter, TextureWrap uwrap, TextureWrap vwrap, uint8_t maxLod, float max_anisotropy);
    static void    FlushResourcesToDestroy(DX12FrameResource& current_frame_resource);

    static const dmhash_t HASH_SPIRV_Cross_NumWorkgroups = dmHashString64("SPIRV_Cross_NumWorkgroups");

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

    static HContext DX12GetContext()
    {
        return g_DX12Context;
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

            for (uint8_t i=0; i < DM_ARRAY_SIZE(context->m_FrameResources); i++)
            {
                FlushResourcesToDestroy(context->m_FrameResources[i]);
            }

            delete (DX12Context*) context;
            g_DX12Context = 0x0;
        }
    }

    static void SetupMainRenderTarget(DX12Context* context, DXGI_SAMPLE_DESC sample_desc)
    {
        // Initialize the dummy rendertarget for the main framebuffer
        // The m_Framebuffer construct will be rotated sequentially
        // with the framebuffer objects created per swap chain.
        DX12RenderTarget* rt = GetAssetFromContainer<DX12RenderTarget>(context->m_AssetHandleContainer, context->m_MainRenderTarget);
        assert(rt == 0x0);

        rt               = new DX12RenderTarget();
        rt->m_Id         = DM_RENDERTARGET_BACKBUFFER_ID;
        rt->m_Format     = DXGI_FORMAT_R8G8B8A8_UNORM;
        rt->m_SampleDesc = sample_desc;

        context->m_MainRenderTarget    = StoreAssetInContainer(context->m_AssetHandleContainer, rt, ASSET_TYPE_RENDER_TARGET);
        context->m_CurrentRenderTarget = context->m_MainRenderTarget;
    }

    void DX12ScratchBuffer::Initialize(DX12Context* context, uint32_t frame_index)
    {
        m_FrameIndex = frame_index;

        // Initialize constant buffer heap
        {
            uint32_t pool_block_count = MAX_BLOCK_SIZE / BLOCK_STEP_SIZE;
            m_MemoryPools.SetCapacity(pool_block_count);
            m_MemoryPools.SetSize(pool_block_count);

            for (int i = 0; i < m_MemoryPools.Size(); ++i)
            {
                m_MemoryPools[i].m_BlockSize        = (i+1) * BLOCK_STEP_SIZE;
                m_MemoryPools[i].m_DescriptorCursor = 0;
                m_MemoryPools[i].m_MemoryCursor     = 0;

                D3D12_DESCRIPTOR_HEAP_DESC heap_Desc = {};
                heap_Desc.NumDescriptors             = DESCRIPTORS_PER_POOL;
                heap_Desc.Flags                      = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
                heap_Desc.Type                       = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;

                HRESULT hr = context->m_Device->CreateDescriptorHeap(&heap_Desc, IID_PPV_ARGS(&m_MemoryPools[i].m_DescriptorHeap));
                CHECK_HR_ERROR(hr);

                const uint32_t memory_heap_alignment = 1024 * 64;
                const uint32_t memory_heap_size      = DM_ALIGN(DESCRIPTORS_PER_POOL * m_MemoryPools[i].m_BlockSize, memory_heap_alignment);

                hr = context->m_Device->CreateCommittedResource(
                    &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD), // this heap will be used to upload the constant buffer data
                    D3D12_HEAP_FLAG_NONE,                             // no flags
                    &CD3DX12_RESOURCE_DESC::Buffer(memory_heap_size), // size of the resource heap. Must be a multiple of 64KB for single-textures and constant buffers
                    D3D12_RESOURCE_STATE_GENERIC_READ,                // will be data that is read from so we keep it in the generic read state
                    NULL,                                             // we do not have use an optimized clear value for constant buffers
                    IID_PPV_ARGS(&m_MemoryPools[i].m_MemoryHeap));
                CHECK_HR_ERROR(hr);

                for (int j = 0; j < DESCRIPTORS_PER_POOL; ++j)
                {
                    D3D12_CONSTANT_BUFFER_VIEW_DESC view_desc = {};
                    view_desc.BufferLocation = m_MemoryPools[i].m_MemoryHeap->GetGPUVirtualAddress() + j * m_MemoryPools[i].m_BlockSize;
                    view_desc.SizeInBytes    = m_MemoryPools[i].m_BlockSize;

                    CD3DX12_CPU_DESCRIPTOR_HANDLE view_handle(m_MemoryPools[i].m_DescriptorHeap->GetCPUDescriptorHandleForHeapStart(), j, context->m_Device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV));
                    context->m_Device->CreateConstantBufferView(&view_desc, view_handle);
                }

                hr = m_MemoryPools[i].m_MemoryHeap->Map(0, 0, &m_MemoryPools[i].m_MappedDataPtr);
                CHECK_HR_ERROR(hr);
            }
        }
    }

    void* DX12ScratchBuffer::AllocateConstantBuffer(DX12Context* context, DX12PipelineType pipeline_type, uint32_t buffer_index, uint32_t non_aligned_byte_size)
    {
        assert(non_aligned_byte_size < MAX_BLOCK_SIZE);
        uint32_t pool_index     = non_aligned_byte_size / BLOCK_STEP_SIZE;
        uint32_t memory_cursor  = m_MemoryPools[pool_index].m_MemoryCursor;
        uint8_t* base_ptr       = ((uint8_t*) m_MemoryPools[pool_index].m_MappedDataPtr) + memory_cursor;

        if (pipeline_type == PIPELINE_TYPE_GRAPHICS)
        {
            context->m_CommandList->SetGraphicsRootConstantBufferView(buffer_index, m_MemoryPools[pool_index].m_MemoryHeap->GetGPUVirtualAddress() + memory_cursor);
        }
        else
        {
            context->m_CommandList->SetComputeRootConstantBufferView(buffer_index, m_MemoryPools[pool_index].m_MemoryHeap->GetGPUVirtualAddress() + memory_cursor);
        }

        m_MemoryPools[pool_index].m_MemoryCursor += m_MemoryPools[pool_index].m_BlockSize;
        m_MemoryPools[pool_index].m_DescriptorCursor++;

    #if 0
        dmLogInfo("AllocateConstantBuffer: ptr: %p, frame: %d, pool: %d, descriptor: %d, offset: %d", base_ptr, m_FrameIndex, pool_index, m_MemoryPools[pool_index].m_DescriptorCursor, cursor);
    #endif

        return (void*) base_ptr;
    }

    void DX12ScratchBuffer::AllocateSampler(DX12Context* context, DX12PipelineType pipeline_type, const DX12TextureSampler& sampler, uint32_t sampler_index)
    {
        CD3DX12_GPU_DESCRIPTOR_HANDLE handle_sampler(context->m_SamplerPool.m_DescriptorHeap->GetGPUDescriptorHandleForHeapStart(), sampler.m_DescriptorOffset);
        if (pipeline_type == PIPELINE_TYPE_GRAPHICS)
        {
            context->m_CommandList->SetGraphicsRootDescriptorTable(sampler_index, handle_sampler);
        }
        else
        {
            context->m_CommandList->SetComputeRootDescriptorTable(sampler_index, handle_sampler);
        }
    }

    void DX12ScratchBuffer::AllocateTexture2D(DX12Context* context, DX12PipelineType pipeline_type, DX12Texture* texture, uint32_t texture_index)
    {
        D3D12_SHADER_RESOURCE_VIEW_DESC view_desc = {};
        view_desc.Format                          = texture->m_ResourceDesc.Format;
        view_desc.ViewDimension                   = D3D12_SRV_DIMENSION_TEXTURE2D;
        view_desc.Shader4ComponentMapping         = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        view_desc.Texture2D.MipLevels             = texture->m_MipMapCount;

        if (texture->m_Type == TEXTURE_TYPE_2D_ARRAY)
        {
            view_desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DARRAY;
            view_desc.Texture2DArray.MostDetailedMip = 0;
            view_desc.Texture2DArray.MipLevels       = texture->m_MipMapCount;
            view_desc.Texture2DArray.FirstArraySlice = 0;  // Start from the first slice
            view_desc.Texture2DArray.ArraySize       = texture->m_LayerCount;  // Number of slices
            view_desc.Texture2DArray.PlaneSlice      = 0;  // This is generally 0 for 2D arrays (1D textures have planes)
        }
        else if (texture->m_Type == TEXTURE_TYPE_CUBE_MAP)
        {
            view_desc.ViewDimension                   = D3D12_SRV_DIMENSION_TEXTURECUBE;
            view_desc.TextureCube.MipLevels           = texture->m_MipMapCount;
            view_desc.TextureCube.MostDetailedMip     = 0;
            view_desc.TextureCube.ResourceMinLODClamp = 0.0f;
        }

        uint32_t desc_size   = context->m_Device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
        uint32_t desc_offset = desc_size * m_MemoryPools[0].m_DescriptorCursor;

        CD3DX12_CPU_DESCRIPTOR_HANDLE  view_desc_handle(
            m_MemoryPools[0].m_DescriptorHeap->GetCPUDescriptorHandleForHeapStart(),
            m_MemoryPools[0].m_DescriptorCursor,
            desc_size);

        context->m_Device->CreateShaderResourceView(texture->m_Resource, &view_desc, view_desc_handle);
        m_MemoryPools[0].m_DescriptorCursor++;

        CD3DX12_GPU_DESCRIPTOR_HANDLE handle_texture(m_MemoryPools[0].m_DescriptorHeap->GetGPUDescriptorHandleForHeapStart(), desc_offset);

        if (pipeline_type == PIPELINE_TYPE_GRAPHICS)
        {
            context->m_CommandList->SetGraphicsRootDescriptorTable(texture_index, handle_texture);
        }
        else
        {
            context->m_CommandList->SetComputeRootDescriptorTable(texture_index, handle_texture);
        }
    }

    void DX12ScratchBuffer::Reset(DX12Context* context)
    {
        for (int i = 0; i < m_MemoryPools.Size(); ++i)
        {
            m_MemoryPools[i].m_DescriptorCursor = 0;
            m_MemoryPools[i].m_MemoryCursor     = 0;
        }
    }

    // Can we bind this at start of frame?
    void DX12ScratchBuffer::Bind(DX12Context* context)
    {
        // TODO: multiple heaps needs to be bound here
        ID3D12DescriptorHeap* heaps[] = {
            m_MemoryPools[0].m_DescriptorHeap
        };
        context->m_CommandList->SetDescriptorHeaps(DM_ARRAY_SIZE(heaps), heaps);
    }

    static DXGI_FORMAT GetDXGIFormatFromTextureFormat(TextureFormat format)
    {
        switch (format)
        {
            case TEXTURE_FORMAT_LUMINANCE:               return DXGI_FORMAT_R8_UNORM;
            case TEXTURE_FORMAT_LUMINANCE_ALPHA:         return DXGI_FORMAT_R8G8_UNORM;
            case TEXTURE_FORMAT_RGB:                     return DXGI_FORMAT_R8G8B8A8_UNORM; // No native RGB8
            case TEXTURE_FORMAT_RGBA:                    return DXGI_FORMAT_R8G8B8A8_UNORM;
            case TEXTURE_FORMAT_RGB_16BPP:               return DXGI_FORMAT_B5G6R5_UNORM;
            case TEXTURE_FORMAT_RGBA_16BPP:              return DXGI_FORMAT_B5G5R5A1_UNORM;
            case TEXTURE_FORMAT_DEPTH:                   return DXGI_FORMAT_D32_FLOAT;
            case TEXTURE_FORMAT_STENCIL:                 return DXGI_FORMAT_D24_UNORM_S8_UINT;

            // Compressed formats
            case TEXTURE_FORMAT_RGB_PVRTC_2BPPV1:        return DXGI_FORMAT_UNKNOWN; // Not supported in DX
            case TEXTURE_FORMAT_RGB_PVRTC_4BPPV1:        return DXGI_FORMAT_UNKNOWN;
            case TEXTURE_FORMAT_RGBA_PVRTC_2BPPV1:       return DXGI_FORMAT_UNKNOWN;
            case TEXTURE_FORMAT_RGBA_PVRTC_4BPPV1:       return DXGI_FORMAT_UNKNOWN;
            case TEXTURE_FORMAT_RGB_ETC1:                return DXGI_FORMAT_UNKNOWN;
            case TEXTURE_FORMAT_R_ETC2:                  return DXGI_FORMAT_UNKNOWN;
            case TEXTURE_FORMAT_RG_ETC2:                 return DXGI_FORMAT_UNKNOWN;
            case TEXTURE_FORMAT_RGBA_ETC2:               return DXGI_FORMAT_UNKNOWN;
            case TEXTURE_FORMAT_RGBA_ASTC_4X4:           return DXGI_FORMAT_UNKNOWN;

            case TEXTURE_FORMAT_RGB_BC1:                 return DXGI_FORMAT_BC1_UNORM;
            case TEXTURE_FORMAT_RGBA_BC3:                return DXGI_FORMAT_BC3_UNORM;
            case TEXTURE_FORMAT_R_BC4:                   return DXGI_FORMAT_BC4_UNORM; // Or BC4_SNORM
            case TEXTURE_FORMAT_RG_BC5:                  return DXGI_FORMAT_BC5_UNORM; // Or BC5_SNORM
            case TEXTURE_FORMAT_RGBA_BC7:                return DXGI_FORMAT_BC7_UNORM;

            // Floating-point formats
            case TEXTURE_FORMAT_RGB16F:                  return DXGI_FORMAT_R16G16B16A16_FLOAT; // No native RGB16F
            case TEXTURE_FORMAT_RGB32F:                  return DXGI_FORMAT_R32G32B32_FLOAT;
            case TEXTURE_FORMAT_RGBA16F:                 return DXGI_FORMAT_R16G16B16A16_FLOAT;
            case TEXTURE_FORMAT_RGBA32F:                 return DXGI_FORMAT_R32G32B32A32_FLOAT;
            case TEXTURE_FORMAT_R16F:                    return DXGI_FORMAT_R16_FLOAT;
            case TEXTURE_FORMAT_RG16F:                   return DXGI_FORMAT_R16G16_FLOAT;
            case TEXTURE_FORMAT_R32F:                    return DXGI_FORMAT_R32_FLOAT;
            case TEXTURE_FORMAT_RG32F:                   return DXGI_FORMAT_R32G32_FLOAT;

            // Integer formats
            case TEXTURE_FORMAT_RGBA32UI:                return DXGI_FORMAT_R32G32B32A32_UINT;
            case TEXTURE_FORMAT_BGRA8U:                  return DXGI_FORMAT_B8G8R8A8_UNORM;
            case TEXTURE_FORMAT_R32UI:                   return DXGI_FORMAT_R32_UINT;
        }

        assert(0);

        return (DXGI_FORMAT) -1;
    }

    static void SetupSupportedTextureFormats(DX12Context* context)
    {
        // TODO: Compression formats!

        // Same as vulkan (we should merge checks for types somehow, so we make sure to test all formats)
        TextureFormat texture_formats[] = { TEXTURE_FORMAT_LUMINANCE,
                                            TEXTURE_FORMAT_LUMINANCE_ALPHA,
                                            TEXTURE_FORMAT_RGBA,

                                            // Float formats
                                            TEXTURE_FORMAT_RGB16F,
                                            TEXTURE_FORMAT_RGB32F,
                                            TEXTURE_FORMAT_RGBA16F,
                                            TEXTURE_FORMAT_RGBA32F,
                                            TEXTURE_FORMAT_R16F,
                                            TEXTURE_FORMAT_RG16F,
                                            TEXTURE_FORMAT_R32F,
                                            TEXTURE_FORMAT_RG32F,

                                            // Misc formats
                                            TEXTURE_FORMAT_RGBA32UI,
                                            TEXTURE_FORMAT_R32UI,
                                            TEXTURE_FORMAT_RGB_16BPP,
                                            TEXTURE_FORMAT_RGBA_16BPP,
                                        };

        // RGB isn't supported as a texture format, but we still need to supply it to the engine
        // Later when a texture is created, we will convert it internally to RGBA.
        context->m_TextureFormatSupport |= 1 << TEXTURE_FORMAT_RGB;

        for (uint32_t i = 0; i < DM_ARRAY_SIZE(texture_formats); ++i)
        {
            TextureFormat texture_format = texture_formats[i];
            DXGI_FORMAT dxgi_format      = GetDXGIFormatFromTextureFormat(texture_format);

            D3D12_FEATURE_DATA_FORMAT_SUPPORT query = {};
            query.Format = dxgi_format;

            HRESULT hr = context->m_Device->CheckFeatureSupport(D3D12_FEATURE_FORMAT_SUPPORT, &query, sizeof(query) );
            if (SUCCEEDED(hr))
            {
                // TODO: Check for more fine-grained support, i.e "query.Support1 & D3D12_FORMAT_SUPPORT1_TEXTURE2D"
                context->m_TextureFormatSupport |= 1 << texture_format;
            }
        }
    }

    static bool MultiSamplingCountSupported(ID3D12Device* device, DXGI_FORMAT format, uint32_t requested_count)
    {
        D3D12_FEATURE_DATA_MULTISAMPLE_QUALITY_LEVELS query = {};
        query.Format           = format;
        query.SampleCount      = requested_count;
        query.Flags            = D3D12_MULTISAMPLE_QUALITY_LEVELS_FLAG_NONE;
        query.NumQualityLevels = 0;

        HRESULT hr = device->CheckFeatureSupport(D3D12_FEATURE_MULTISAMPLE_QUALITY_LEVELS, &query, sizeof(query));
        return SUCCEEDED(hr) && query.NumQualityLevels > 0;
    }

    static uint32_t GetClosestMultiSamplingCount(ID3D12Device* device, DXGI_FORMAT format, uint32_t requested_count)
    {
        if (MultiSamplingCountSupported(device, format, requested_count))
            return requested_count;

        // Same sample counts as vulkan
        static const uint32_t sample_counts[] = { 64, 32, 16, 8, 4, 2, 1 };

        for (uint32_t i=0; i < DM_ARRAY_SIZE(sample_counts); i++)
        {
            if (sample_counts[i] > requested_count)
                continue;

            if (MultiSamplingCountSupported(device, format, sample_counts[i]))
            {
                return sample_counts[i];
            }
        }

        return 1;
    }

    static bool DX12Initialize(HContext _context)
    {
        DX12Context* context = (DX12Context*) _context;

        HRESULT hr = S_OK;

        bool has_debug_layer = false;

        // This needs to be created before the device
        // if (context->m_UseValidationLayers)
        if (true)
        {
            hr = D3D12GetDebugInterface(IID_PPV_ARGS(&context->m_DebugInterface));
            CHECK_HR_ERROR(hr);

            context->m_DebugInterface->EnableDebugLayer();
            context->m_DebugInterface->Release();

            has_debug_layer = true;
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

        DXGI_FORMAT color_format = DXGI_FORMAT_R8G8B8A8_UNORM;
        DXGI_FORMAT depth_format = DXGI_FORMAT_D24_UNORM_S8_UINT;

        // Create swapchain
        DXGI_MODE_DESC back_buffer_desc = {};
        back_buffer_desc.Width          = window_width;
        back_buffer_desc.Height         = window_height;
        back_buffer_desc.Format         = color_format;

        // Note: These must be 1 and 0 - for MSAA we will render to an offscreen texture that is multisampled
        DXGI_SAMPLE_DESC sample_desc = {};
        sample_desc.Count            = 1;
        sample_desc.Quality          = 0;

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

        context->m_MSAASampleCount = GetClosestMultiSamplingCount(context->m_Device, color_format, dmPlatform::GetWindowStateParam(context->m_Window, dmPlatform::WINDOW_STATE_SAMPLE_COUNT));

        SetupSupportedTextureFormats(context);

        ////// MOVE THIS?
        D3D12_DESCRIPTOR_HEAP_DESC sampler_heap_desc = {};
        sampler_heap_desc.NumDescriptors             = 128; // TODO, I don't know if this is a good value, the sampler pool should be fully dynamic I think
        sampler_heap_desc.Flags                      = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
        sampler_heap_desc.Type                       = D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER;
        hr = context->m_Device->CreateDescriptorHeap(&sampler_heap_desc, IID_PPV_ARGS(&context->m_SamplerPool.m_DescriptorHeap));
        CHECK_HR_ERROR(hr);

        // this heap is a render target view heap, we use this heap for the swapchain render targets only. The other render targets will use another heap for this
        D3D12_DESCRIPTOR_HEAP_DESC rt_view_heap_desc = {};
        rt_view_heap_desc.NumDescriptors             = MAX_FRAMEBUFFERS * (context->m_MSAASampleCount > 1 ? 2 : 1);
        rt_view_heap_desc.Type                       = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
        rt_view_heap_desc.Flags                      = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

        hr = context->m_Device->CreateDescriptorHeap(&rt_view_heap_desc, IID_PPV_ARGS(&context->m_RtvDescriptorHeap));
        CHECK_HR_ERROR(hr);

        context->m_RtvDescriptorSize = context->m_Device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

        // Depth/stencil heap
        D3D12_DESCRIPTOR_HEAP_DESC dsv_heap_desc = {};
        dsv_heap_desc.NumDescriptors             = MAX_FRAMEBUFFERS;
        dsv_heap_desc.Type                       = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
        dsv_heap_desc.Flags                      = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

        hr = context->m_Device->CreateDescriptorHeap(&dsv_heap_desc, IID_PPV_ARGS(&context->m_DsvDescriptorHeap));
        CHECK_HR_ERROR(hr);

        context->m_DsvDescriptorSize = context->m_Device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);

        // get a handle to the first descriptor in the descriptor heap. a handle is basically a pointer,
        // but we cannot literally use it like a c++ pointer.
        CD3DX12_CPU_DESCRIPTOR_HANDLE rtv_handle(context->m_RtvDescriptorHeap->GetCPUDescriptorHandleForHeapStart());

        TextureCreationParams texture_create_params;
        texture_create_params.m_Width          = window_width;
        texture_create_params.m_Height         = window_height;
        texture_create_params.m_OriginalWidth  = window_width;
        texture_create_params.m_OriginalHeight = window_height;

        for (int i = 0; i < MAX_FRAMEBUFFERS; i++)
        {
            context->m_FrameResources[i].m_TextureColor        = NewTexture(_context, texture_create_params);
            context->m_FrameResources[i].m_TextureDepthStencil = NewTexture(_context, texture_create_params);
            DX12Texture* texture_color = GetAssetFromContainer<DX12Texture>(context->m_AssetHandleContainer, context->m_FrameResources[i].m_TextureColor);
            DX12Texture* texture_depth = GetAssetFromContainer<DX12Texture>(context->m_AssetHandleContainer, context->m_FrameResources[i].m_TextureDepthStencil);

            // first we get the n'th buffer in the swap chain and store it in the n'th
            // position of our ID3D12Resource array
            hr = context->m_SwapChain->GetBuffer(i, IID_PPV_ARGS(&texture_color->m_Resource));
            CHECK_HR_ERROR(hr);

            // we "create" a render target view which binds the swap chain buffer (ID3D12Resource[n]) to the rtv handle
            context->m_Device->CreateRenderTargetView(texture_color->m_Resource, NULL, rtv_handle);
            rtv_handle.Offset(1, context->m_RtvDescriptorSize);


            // If the project is using multisampling, we need to create a secondary RT that we render into.
            // This will later be resolved into the swap chain RT before presenting (via EndRenderPass).
            if (context->m_MSAASampleCount > 1)
            {
                D3D12_RESOURCE_DESC msaa_desc = {};
                msaa_desc.Dimension           = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
                msaa_desc.Width               = window_width;
                msaa_desc.Height              = window_height;
                msaa_desc.DepthOrArraySize    = 1;
                msaa_desc.MipLevels           = 1;
                msaa_desc.Format              = color_format;
                msaa_desc.SampleDesc.Count    = context->m_MSAASampleCount;
                msaa_desc.SampleDesc.Quality  = 0;
                msaa_desc.Layout              = D3D12_TEXTURE_LAYOUT_UNKNOWN;
                msaa_desc.Flags               = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

                D3D12_CLEAR_VALUE clear_value = {};
                clear_value.Format            = color_format;
                clear_value.Color[0]          = 0.0f;
                clear_value.Color[1]          = 0.0f;
                clear_value.Color[2]          = 0.0f;
                clear_value.Color[3]          = 1.0f;

                hr = context->m_Device->CreateCommittedResource(
                    &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
                    D3D12_HEAP_FLAG_NONE,
                    &msaa_desc,
                    D3D12_RESOURCE_STATE_RENDER_TARGET,
                    &clear_value,
                    IID_PPV_ARGS(&context->m_FrameResources[i].m_MsaaRenderTarget)
                );
                CHECK_HR_ERROR(hr);

                context->m_Device->CreateRenderTargetView(context->m_FrameResources[i].m_MsaaRenderTarget, NULL, rtv_handle);
                rtv_handle.Offset(1, context->m_RtvDescriptorSize);
            }

            // Create the depth/stencil attachment(s)
            D3D12_RESOURCE_DESC depth_desc = {};
            depth_desc.Dimension           = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
            depth_desc.Width               = window_width;
            depth_desc.Height              = window_height;
            depth_desc.DepthOrArraySize    = 1;
            depth_desc.MipLevels           = 1;
            depth_desc.Format              = depth_format;
            depth_desc.SampleDesc.Count    = context->m_MSAASampleCount;
            depth_desc.SampleDesc.Quality  = 0;
            depth_desc.Layout              = D3D12_TEXTURE_LAYOUT_UNKNOWN;
            depth_desc.Flags               = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

            D3D12_CLEAR_VALUE depth_clear    = {};
            depth_clear.Format               = depth_format;
            depth_clear.DepthStencil.Depth   = 1.0f;
            depth_clear.DepthStencil.Stencil = 0;

            hr = context->m_Device->CreateCommittedResource(
                &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
                D3D12_HEAP_FLAG_NONE,
                &depth_desc,
                D3D12_RESOURCE_STATE_DEPTH_WRITE,
                &depth_clear,
                IID_PPV_ARGS(&texture_depth->m_Resource)
            );
            CHECK_HR_ERROR(hr);

            // Create depth stencil view
            CD3DX12_CPU_DESCRIPTOR_HANDLE dsv_handle(context->m_DsvDescriptorHeap->GetCPUDescriptorHandleForHeapStart(), i, context->m_DsvDescriptorSize);

            D3D12_DEPTH_STENCIL_VIEW_DESC dsv_desc = {};
            dsv_desc.Format        = depth_format;
            dsv_desc.ViewDimension = (context->m_MSAASampleCount > 1) ? D3D12_DSV_DIMENSION_TEXTURE2DMS : D3D12_DSV_DIMENSION_TEXTURE2D;
            dsv_desc.Flags         = D3D12_DSV_FLAG_NONE;

            context->m_Device->CreateDepthStencilView(texture_depth->m_Resource, &dsv_desc, dsv_handle);

            // Create the rest of the per-frame resources
            hr = context->m_Device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&context->m_FrameResources[i].m_CommandAllocator));
            CHECK_HR_ERROR(hr);

            // Create the frame fence that will be signaled when we can render to this frame
            hr = context->m_Device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&context->m_FrameResources[i].m_Fence));
            CHECK_HR_ERROR(hr);

            context->m_FrameResources[i].m_FenceValue = RENDER_CONTEXT_STATE_FREE;
            context->m_FrameResources[i].m_ScratchBuffer.Initialize(context, i);
        }


        context->m_FenceEvent = CreateEvent(NULL, false, false, NULL);
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

        SetupMainRenderTarget(context, sample_desc);

        context->m_PipelineState = GetDefaultPipelineState();

        CreateTextureSampler(context, TEXTURE_FILTER_LINEAR, TEXTURE_FILTER_LINEAR, TEXTURE_WRAP_REPEAT, TEXTURE_WRAP_REPEAT, 1, 1.0f);

        if (context->m_PrintDeviceInfo)
        {
            dmLogInfo("Device: DirectX 12");
        }

        if (has_debug_layer)
        {
            // We install a custom message filter here to avoid the spam that occurs when issuing clear commands
            // that contains colors that ARE NOT the same as the values that was used when the RT was created.
            // This will be slower, but we would have to change the API to fix it..
            ID3D12InfoQueue* infoQueue = NULL;
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
        DX12RenderTarget* current_rt = GetAssetFromContainer<DX12RenderTarget>(context->m_AssetHandleContainer, context->m_CurrentRenderTarget);

        const float r = ((float)red) / 255.0f;
        const float g = ((float)green) / 255.0f;
        const float b = ((float)blue) / 255.0f;
        const float a = ((float)alpha) / 255.0f;
        const float clearColor[] = { r, g, b, a };

        bool has_depth_stencil_texture = current_rt->m_Id == DM_RENDERTARGET_BACKBUFFER_ID; // || current_rt->m_TextureDepthStencil;
        bool clear_depth_stencil       = flags & (BUFFER_TYPE_DEPTH_BIT | BUFFER_TYPE_STENCIL_BIT);

        if (flags & BUFFER_TYPE_COLOR0_BIT)
        {
            context->m_CommandList->ClearRenderTargetView(context->m_RtvHandle, clearColor, 0, NULL);
        }

        // Clear depth / stencil
        if (has_depth_stencil_texture && clear_depth_stencil)
        {
            D3D12_CLEAR_FLAGS clear_depth_stencil_flags = (D3D12_CLEAR_FLAGS) 0;

            if (flags & BUFFER_TYPE_DEPTH_BIT)
            {
                clear_depth_stencil_flags |= D3D12_CLEAR_FLAG_DEPTH;
            }

            if (flags & BUFFER_TYPE_STENCIL_BIT)
            {
                clear_depth_stencil_flags |= D3D12_CLEAR_FLAG_STENCIL;
            }

            // Clear the depth and stencil buffer
            context->m_CommandList->ClearDepthStencilView(
                context->m_DsvHandle,
                clear_depth_stencil_flags,
                depth,
                (UINT8) stencil,
                0,
                NULL);
        }
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
            return false;

        if (current_rt->m_Id == DM_RENDERTARGET_BACKBUFFER_ID)
        {
            // NOTE: We rotate the swap chain textures into the RT at the beginning of the frame
            DX12Texture* texture_color         = GetAssetFromContainer<DX12Texture>(context->m_AssetHandleContainer, current_rt->m_TextureColor[0]);
            ID3D12Resource* color              = texture_color->m_Resource;

            if (context->m_MSAASampleCount > 1)
            {
                ID3D12Resource* msaaRT = context->m_FrameResources[context->m_CurrentFrameIndex].m_MsaaRenderTarget;

                // Transition targets into dest and source (TODO: We should track the state of the resources, or actually _all_ resources)
                context->m_CommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(color, D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RESOLVE_DEST));
                context->m_CommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(msaaRT, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_RESOLVE_SOURCE));

                // Resolve from MSAA to swapchain
                context->m_CommandList->ResolveSubresource(
                    color, 0,
                    msaaRT, 0,
                    DXGI_FORMAT_R8G8B8A8_UNORM
                );

                // Transition resources back to previous state (to be used for next frame)
                context->m_CommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(color, D3D12_RESOURCE_STATE_RESOLVE_DEST, D3D12_RESOURCE_STATE_PRESENT));
                context->m_CommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(msaaRT, D3D12_RESOURCE_STATE_RESOLVE_SOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET));
            }
            else
            {
                // No MSAA: just transition swapchain RT to PRESENT
                context->m_CommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(color, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT));
            }

            DX12Texture* texture_depth_stencil = GetAssetFromContainer<DX12Texture>(context->m_AssetHandleContainer, current_rt->m_TextureDepthStencil);

            // Regardless of MSAA count (no resolve needed for depth target I think?), we need to transition backbuffer DSV to COMMON
            if (texture_depth_stencil && texture_depth_stencil->m_Resource)
            {
                context->m_CommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(texture_depth_stencil->m_Resource, D3D12_RESOURCE_STATE_DEPTH_WRITE, D3D12_RESOURCE_STATE_COMMON));
            }
        }
        else
        {
            // Transition custom render target's depth/stencil back to COMMON
            if (current_rt->m_TextureDepthStencil)
            {
                DX12Texture* texture_depth_stencil = GetAssetFromContainer<DX12Texture>(context->m_AssetHandleContainer, current_rt->m_TextureDepthStencil);
                context->m_CommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(texture_depth_stencil->m_Resource, D3D12_RESOURCE_STATE_DEPTH_WRITE, D3D12_RESOURCE_STATE_COMMON));
            }
        }

        current_rt->m_IsBound = 0;
        return true;
    }

    static void BeginRenderPass(DX12Context* context, HRenderTarget render_target)
    {
        DX12RenderTarget* current_rt = GetAssetFromContainer<DX12RenderTarget>(context->m_AssetHandleContainer, context->m_CurrentRenderTarget);
        DX12RenderTarget* rt         = GetAssetFromContainer<DX12RenderTarget>(context->m_AssetHandleContainer, render_target);

        if (current_rt->m_Id == rt->m_Id && current_rt->m_IsBound)
            return;

        if (current_rt->m_IsBound)
            EndRenderPass(context);

        ID3D12DescriptorHeap* rtv_heap = NULL;
        uint32_t num_attachments = 0;

        CD3DX12_CPU_DESCRIPTOR_HANDLE rtv_handle;

        if (rt->m_Id == DM_RENDERTARGET_BACKBUFFER_ID)
        {
            num_attachments = 1;
            rtv_heap = context->m_RtvDescriptorHeap;

            if (context->m_MSAASampleCount > 1)
            {
                // MSAA: bind the MSAA RT for current frame
                uint32_t rtv_index = context->m_CurrentFrameIndex * 2 + 1;

                ID3D12Resource* msaaRT = context->m_FrameResources[context->m_CurrentFrameIndex].m_MsaaRenderTarget;
                rtv_handle = CD3DX12_CPU_DESCRIPTOR_HANDLE(
                    context->m_RtvDescriptorHeap->GetCPUDescriptorHandleForHeapStart(),
                    rtv_index,
                    context->m_RtvDescriptorSize
                );
            }
            else
            {
                DX12Texture* texture_color = GetAssetFromContainer<DX12Texture>(context->m_AssetHandleContainer, current_rt->m_TextureColor[0]);

                // No MSAA: use the regular swapchain RT
                rtv_handle = CD3DX12_CPU_DESCRIPTOR_HANDLE(
                    context->m_RtvDescriptorHeap->GetCPUDescriptorHandleForHeapStart(),
                    context->m_CurrentFrameIndex,
                    context->m_RtvDescriptorSize
                );

                context->m_CommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(texture_color->m_Resource, D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));
            }
        }
        else
        {
            rtv_heap = rt->m_ColorAttachmentDescriptorHeap;
            rtv_handle = rt->m_ColorAttachmentDescriptorHeap->GetCPUDescriptorHandleForHeapStart();

            for (int i = 0; i < MAX_BUFFER_COLOR_ATTACHMENTS; ++i)
            {
                if (rt->m_TextureColor[i])
                {
                    DX12Texture* attachment = GetAssetFromContainer<DX12Texture>(context->m_AssetHandleContainer, rt->m_TextureColor[i]);

                    if (attachment->m_ResourceStates[0] != D3D12_RESOURCE_STATE_RENDER_TARGET)
                    {
                        // Transition the first mipmap into a render target
                        context->m_CommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(attachment->m_Resource, attachment->m_ResourceStates[0], D3D12_RESOURCE_STATE_RENDER_TARGET));
                        attachment->m_ResourceStates[0] = D3D12_RESOURCE_STATE_RENDER_TARGET;
                    }
                    num_attachments++;
                }
            }
        }

        // Setup DSV if available
        D3D12_CPU_DESCRIPTOR_HANDLE* dsv_handle_ptr = NULL;
        CD3DX12_CPU_DESCRIPTOR_HANDLE dsv_handle;

        if (rt->m_Id == DM_RENDERTARGET_BACKBUFFER_ID)
        {
            dsv_handle = CD3DX12_CPU_DESCRIPTOR_HANDLE(
                context->m_DsvDescriptorHeap->GetCPUDescriptorHandleForHeapStart(),
                context->m_CurrentFrameIndex,
                context->m_DsvDescriptorSize
            );
            dsv_handle_ptr = &dsv_handle;
        }
        else if (rt->m_DepthStencilDescriptorHeap)
        {
            dsv_handle     = CD3DX12_CPU_DESCRIPTOR_HANDLE(rt->m_DepthStencilDescriptorHeap->GetCPUDescriptorHandleForHeapStart());
            dsv_handle_ptr = &dsv_handle;

            DX12Texture* texture_depth_stencil = GetAssetFromContainer<DX12Texture>(context->m_AssetHandleContainer, rt->m_TextureDepthStencil);

            if (texture_depth_stencil && texture_depth_stencil->m_Resource)
            {
                context->m_CommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(texture_depth_stencil->m_Resource, D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_DEPTH_WRITE));
            }
        }

        context->m_RtvHandle = rtv_handle;
        context->m_DsvHandle = dsv_handle;

        // Bind render target(s) and optional depth-stencil
        context->m_CommandList->OMSetRenderTargets(1, &context->m_RtvHandle, FALSE, dsv_handle_ptr);

        rt->m_IsBound = 1;
        context->m_CurrentRenderTarget = render_target;
    }

    template <typename T>
    static void DestroyResourceDeferred(DX12FrameResource& current_frame_resource, T* resource) 
    {
        if (resource == 0x0 || resource->m_Resource == 0x0)
        {
            return;
        }

        if (current_frame_resource.m_ResourcesToDestroy.Full())
        {
            current_frame_resource.m_ResourcesToDestroy.OffsetCapacity(8);
        }

        current_frame_resource.m_ResourcesToDestroy.Push(resource->m_Resource);
        resource->m_Destroyed = 1;
        resource->m_Resource = 0;
    }

    static void FlushResourcesToDestroy(DX12FrameResource& current_frame_resource)
    {
        if (current_frame_resource.m_ResourcesToDestroy.Size() > 0)
        {
            for (uint32_t i = 0; i < current_frame_resource.m_ResourcesToDestroy.Size(); ++i)
            {
                current_frame_resource.m_ResourcesToDestroy[i]->Release();
            }
            current_frame_resource.m_ResourcesToDestroy.SetSize(0);
        }
    }

    static void DX12BeginFrame(HContext _context)
    {
        DX12Context* context = (DX12Context*) _context;
        SyncronizeFrame(context);

        DX12FrameResource& current_frame_resource = context->m_FrameResources[context->m_CurrentFrameIndex];

        HRESULT hr = current_frame_resource.m_CommandAllocator->Reset();
        CHECK_HR_ERROR(hr);

        DX12RenderTarget* rt = GetAssetFromContainer<DX12RenderTarget>(context->m_AssetHandleContainer, context->m_MainRenderTarget);
        rt->m_TextureColor[0] = current_frame_resource.m_TextureColor;
        rt->m_TextureDepthStencil = current_frame_resource.m_TextureDepthStencil;

        FlushResourcesToDestroy(current_frame_resource);

        // Enter "record" mode
        hr = context->m_CommandList->Reset(current_frame_resource.m_CommandAllocator, NULL); // Second argument is a pipeline object (TODO)
        CHECK_HR_ERROR(hr);

        current_frame_resource.m_ScratchBuffer.Reset(context);

        context->m_FrameBegun = 1;

        ID3D12DescriptorHeap* heaps[] = {
            context->m_SamplerPool.m_DescriptorHeap,
            current_frame_resource.m_ScratchBuffer.m_MemoryPools[0].m_DescriptorHeap
        };

        context->m_CommandList->SetDescriptorHeaps(DM_ARRAY_SIZE(heaps), heaps);

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

        context->m_FrameBegun = 0;
    }

    static inline bool IsRenderTargetbound(DX12Context* context, HRenderTarget rt)
    {
        DX12RenderTarget* current_rt = GetAssetFromContainer<DX12RenderTarget>(context->m_AssetHandleContainer, rt);
        return current_rt ? current_rt->m_IsBound : 0;
    }

    static inline uint32_t GetPitchFromMipMap(uint32_t pitch, uint8_t mipmap)
    {
        for (uint32_t i = 0; i < mipmap; ++i)
        {
            pitch /= 2;
        }
        return pitch;
    }

    static void CopyTextureDataMipmapLevel(const TextureParams& params, TextureFormat format_dst, TextureFormat format_src,
        const D3D12_PLACED_SUBRESOURCE_FOOTPRINT* layouts, const uint32_t* num_rows, uint32_t array_count,
        uint32_t mipmap_count, const uint32_t* slice_row_pitch, const uint8_t* pixels, uint8_t* upload_data, uint32_t mipmap)
    {
        const uint8_t bpp_dst = GetTextureFormatBitsPerPixel(format_dst) / 8;
        const uint8_t bpp_src = GetTextureFormatBitsPerPixel(format_src) / 8;
        const uint64_t srcRowPitch = slice_row_pitch[0];  // Assuming only one mip being uploaded
        const uint32_t copy_width = params.m_Width;
        const uint32_t copy_height = params.m_Height;
        const uint32_t copy_x = params.m_X;
        const uint32_t copy_y = params.m_Y;

        for (uint32_t array = 0; array < array_count; ++array)
        {
            const D3D12_PLACED_SUBRESOURCE_FOOTPRINT& layout = layouts[array];
            const uint64_t dstRowPitch = layout.Footprint.RowPitch;
            const uint32_t depth = layout.Footprint.Depth;

            uint8_t* dst = upload_data + layout.Offset;
            const uint8_t* src = pixels + array * copy_height * srcRowPitch * depth;

            for (uint32_t z = 0; z < depth; ++z)
            {
                for (uint32_t row = 0; row < copy_height; ++row)
                {
                    const uint8_t* src_row = src + (z * copy_height + row) * srcRowPitch;
                    uint8_t* dst_row = dst + (z * copy_height + row) * dstRowPitch;

                    memcpy(dst_row, src_row, copy_width * bpp_dst);
                }
            }
        }
    }

    static inline void CreateOneTimeCommandList(ID3D12Device* device, DX12OneTimeCommandList* cmd_list)
    {
        HRESULT hr = device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&cmd_list->m_CommandAllocator));
        CHECK_HR_ERROR(hr);

        hr = device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, cmd_list->m_CommandAllocator, NULL, IID_PPV_ARGS(&cmd_list->m_CommandList));
        CHECK_HR_ERROR(hr);

        hr = device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&cmd_list->m_Fence));
        CHECK_HR_ERROR(hr);
    }

    static inline void ResetOneTimeCommandList(DX12OneTimeCommandList* cmd_list)
    {
        cmd_list->m_Fence->Release();
        cmd_list->m_CommandAllocator->Reset();
        cmd_list->m_CommandList->Reset(cmd_list->m_CommandAllocator, 0);
    }

    static inline void SyncronizeOneTimeCommandList(ID3D12Device* device, ID3D12CommandQueue* queue, DX12OneTimeCommandList* cmd_list)
    {
        UINT64 fence_value = FENCE_VALUE_SYNCRONIZE_UPLOAD;
        HANDLE fence_event = CreateEvent(NULL, FALSE, FALSE, NULL);
        device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&cmd_list->m_Fence));

        queue->Signal(cmd_list->m_Fence, fence_value);
        if (cmd_list->m_Fence->GetCompletedValue() != fence_value)
        {
            cmd_list->m_Fence->SetEventOnCompletion(fence_value, fence_event);
            WaitForSingleObject(fence_event, INFINITE);
        }
        CloseHandle(fence_event);
    }

    static void TextureBufferUploadHelper(DX12Context* context, DX12Texture* texture, TextureFormat format_dst, TextureFormat format_src, const TextureParams& params, uint8_t* pixels)
    {
        const uint32_t target_mip        = params.m_MipMap;
        const uint16_t tex_layer_count   = dmMath::Max(texture->m_LayerCount, (uint16_t) params.m_LayerCount);
        const uint32_t subresource_count = tex_layer_count; // only one mip, full array

        D3D12_PLACED_SUBRESOURCE_FOOTPRINT fp[16] = {};
        uint32_t num_rows[16];
        uint64_t row_size_in_bytes[16];
        uint64_t total_upload_size = 0;

        // Calculate offset/footprint per array slice
        for (uint32_t array = 0; array < tex_layer_count; ++array)
        {
            const uint32_t subresource = D3D12CalcSubresource(target_mip, array, 0, texture->m_MipMapCount, tex_layer_count);
            
            D3D12_PLACED_SUBRESOURCE_FOOTPRINT layout = {};
            UINT rows = 0;
            UINT64 rowSize = 0;
            UINT64 size = 0;
            
            context->m_Device->GetCopyableFootprints(
                &texture->m_ResourceDesc, subresource, 1,
                total_upload_size, &layout, &rows, &rowSize, &size);
            
            fp[array] = layout;
            num_rows[array] = rows;
            row_size_in_bytes[array] = rowSize;
            total_upload_size += size;
        }

        // Create upload heap
        D3D12_RESOURCE_DESC upload_desc = {};
        upload_desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        upload_desc.Alignment = D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT;
        upload_desc.Width = total_upload_size;
        upload_desc.Height = 1;
        upload_desc.DepthOrArraySize = 1;
        upload_desc.MipLevels = 1;
        upload_desc.Format = DXGI_FORMAT_UNKNOWN;
        upload_desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
        upload_desc.SampleDesc.Count = 1;

        ID3D12Resource* upload_heap;
        HRESULT hr = context->m_Device->CreateCommittedResource(
            &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
            D3D12_HEAP_FLAG_NONE,
            &upload_desc,
            D3D12_RESOURCE_STATE_GENERIC_READ,
            NULL,
            IID_PPV_ARGS(&upload_heap));
        CHECK_HR_ERROR(hr);

        uint8_t* upload_data = NULL;
        hr = upload_heap->Map(0, NULL, (void**)&upload_data);
        CHECK_HR_ERROR(hr);

        // Compute only the target mip’s row pitch
        uint8_t bpp_src = GetTextureFormatBitsPerPixel(format_src) / 8;
        uint32_t slice_row_pitch[1] = { params.m_Width * bpp_src };

        // Copy only the selected mip level's data
        CopyTextureDataMipmapLevel(params, format_dst, format_src, fp, num_rows, tex_layer_count,
            texture->m_MipMapCount, slice_row_pitch, pixels, upload_data, target_mip);

        ID3D12GraphicsCommandList* cmd_list = context->m_CommandList;
        DX12OneTimeCommandList one_time_cmd_list = {};
        if (!context->m_FrameBegun)
        {
            CreateOneTimeCommandList(context->m_Device, &one_time_cmd_list);
            cmd_list = one_time_cmd_list.m_CommandList;
        }

        // Transition resource state if needed
        if (texture->m_ResourceStates[target_mip] != D3D12_RESOURCE_STATE_COPY_DEST)
        {
            cmd_list->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(
                texture->m_Resource, texture->m_ResourceStates[target_mip],
                D3D12_RESOURCE_STATE_COPY_DEST, target_mip));
            texture->m_ResourceStates[target_mip] = D3D12_RESOURCE_STATE_COPY_DEST;
        }

        // Copy per array slice
        for (uint32_t array = 0; array < texture->m_LayerCount; ++array)
        {
            const uint32_t subresourceIndex = D3D12CalcSubresource(target_mip, array, 0, texture->m_MipMapCount, texture->m_LayerCount);

            D3D12_TEXTURE_COPY_LOCATION copy_dst = {};
            copy_dst.pResource                   = texture->m_Resource;
            copy_dst.Type                        = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
            copy_dst.SubresourceIndex            = subresourceIndex;

            D3D12_TEXTURE_COPY_LOCATION copy_src = {};
            copy_src.pResource                   = upload_heap;
            copy_src.Type                        = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
            copy_src.PlacedFootprint             = fp[array];  // One footprint per array slice

            // This box represents the region of the "source" data that we should copy
            D3D12_BOX box = {};
            box.left      = 0;
            box.top       = 0;
            box.front     = 0;
            box.right     = params.m_Width;
            box.bottom    = params.m_Height;
            box.back      = 1;

            cmd_list->CopyTextureRegion(&copy_dst, params.m_X, params.m_Y, 0, &copy_src, &box);
        }

        // Transition back
        if (texture->m_ResourceStates[target_mip] != D3D12_RESOURCE_STATE_GENERIC_READ)
        {
            cmd_list->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(
                texture->m_Resource, texture->m_ResourceStates[target_mip],
                D3D12_RESOURCE_STATE_GENERIC_READ, target_mip));
            texture->m_ResourceStates[target_mip] = D3D12_RESOURCE_STATE_GENERIC_READ;
        }

        // Submit command list if needed
        if (!context->m_FrameBegun)
        {
            cmd_list->Close();
            ID3D12CommandList* execute_cmd_lists[] = { cmd_list };
            context->m_CommandQueue->ExecuteCommandLists(DM_ARRAY_SIZE(execute_cmd_lists), execute_cmd_lists);

            SyncronizeOneTimeCommandList(context->m_Device, context->m_CommandQueue, &one_time_cmd_list);
            ResetOneTimeCommandList(&one_time_cmd_list);
        }
    }

    static void CreateDeviceBuffer(DX12Context* context, DX12DeviceBuffer* device_buffer, uint32_t size)
    {
        assert(device_buffer->m_Resource == 0x0);

        // create default heap
        // default heap is memory on the GPU. Only the GPU has access to this memory
        // To get data into this heap, we will have to upload the data using
        // an upload heap
        HRESULT hr = context->m_Device->CreateCommittedResource(
            &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT), // a default heap
            D3D12_HEAP_FLAG_NONE,                              // no flags
            &CD3DX12_RESOURCE_DESC::Buffer(size),              // resource description for a buffer
            D3D12_RESOURCE_STATE_COMMON,                       // we will start this heap in the copy destination state since we will copy data from the upload heap to this heap
            NULL,                                              // optimized clear value must be null for this type of resource. used for render targets and depth/stencil buffers
            IID_PPV_ARGS(&device_buffer->m_Resource));
        CHECK_HR_ERROR(hr);

        device_buffer->m_Resource->SetName(L"Vertex Buffer Resource Heap");
    }

    static void DeviceBufferUploadHelper(DX12Context* context, DX12DeviceBuffer* device_buffer, const void* data, uint32_t data_size)
    {
        if (data == 0 || data_size == 0)
            return;

        if (device_buffer->m_Destroyed || device_buffer->m_Resource == 0x0)
        {
            CreateDeviceBuffer(context, device_buffer, data_size);
        }

        // create upload heap
        // upload heaps are used to upload data to the GPU. CPU can write to it, GPU can read from it
        // We will upload the vertex buffer using this heap to the default heap
        ID3D12Resource* upload_heap;
        HRESULT hr = context->m_Device->CreateCommittedResource(
            &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),   // upload heap
            D3D12_HEAP_FLAG_NONE,                               // no flags
            &CD3DX12_RESOURCE_DESC::Buffer(data_size),          // resource description for a buffer
            D3D12_RESOURCE_STATE_GENERIC_READ,                  // GPU will read from this buffer and copy its contents to the default heap
            NULL,
            IID_PPV_ARGS(&upload_heap));
        CHECK_HR_ERROR(hr);

        upload_heap->SetName(L"Vertex Buffer Upload Resource Heap");

        D3D12_SUBRESOURCE_DATA res_data = {};
        res_data.pData      = data;
        res_data.RowPitch   = data_size;
        res_data.SlicePitch = data_size;

        ID3D12GraphicsCommandList* cmd_list = context->m_CommandList;

        DX12OneTimeCommandList one_time_cmd_list = {};
        if (!context->m_FrameBegun)
        {
            CreateOneTimeCommandList(context->m_Device, &one_time_cmd_list);
            cmd_list = one_time_cmd_list.m_CommandList;
        }

        UpdateSubresources(cmd_list, device_buffer->m_Resource, upload_heap, 0, 0, 1, &res_data);

        // transition the vertex buffer data from copy destination state to vertex buffer state
        cmd_list->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(device_buffer->m_Resource, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER));

        if (!context->m_FrameBegun)
        {
            cmd_list->Close();
            ID3D12CommandList* execute_cmd_lists[] = { cmd_list };
            context->m_CommandQueue->ExecuteCommandLists(DM_ARRAY_SIZE(execute_cmd_lists), execute_cmd_lists);

            SyncronizeOneTimeCommandList(context->m_Device, context->m_CommandQueue, &one_time_cmd_list);

            ResetOneTimeCommandList(&one_time_cmd_list);
        }

        device_buffer->m_DataSize = data_size;

        // TODO: Release the heap buffer deferred(?)
        // DeviceBuffer wrapped_heap_buffer();
    }

    static void CreateConstantBuffer(DX12Context* context, DX12DeviceBuffer* buffer, uint32_t size)
    {
        uint32_t aligned_size = DM_ALIGN(size, 256);

        HRESULT hr = context->m_Device->CreateCommittedResource(
            &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
            D3D12_HEAP_FLAG_NONE,
            &CD3DX12_RESOURCE_DESC::Buffer(aligned_size),
            D3D12_RESOURCE_STATE_GENERIC_READ,
            NULL,
            IID_PPV_ARGS(&buffer->m_Resource));
        CHECK_HR_ERROR(hr);

        buffer->m_Resource->Map(0, NULL, (void**)&buffer->m_MappedDataPtr);
        buffer->m_DataSize = aligned_size;
    }

    static HUniformBuffer DX12NewUniformBuffer(HContext _context, const UniformBufferLayout& layout)
    {
        DX12Context* context = (DX12Context*) _context;
        DX12UniformBuffer* ubo = new DX12UniformBuffer();
        ubo->m_BaseUniformBuffer.m_Layout       = layout;
        ubo->m_BaseUniformBuffer.m_BoundSet     = UNUSED_BINDING_OR_SET;
        ubo->m_BaseUniformBuffer.m_BoundBinding = UNUSED_BINDING_OR_SET;

        CreateConstantBuffer(context, &ubo->m_DeviceBuffer, layout.m_Size);

        return (HUniformBuffer) ubo;
    }

    static void DX12SetUniformBuffer(HContext _context, HUniformBuffer uniform_buffer, uint32_t offset, uint32_t size, const void* data)
    {
        DX12UniformBuffer* ubo = (DX12UniformBuffer*)uniform_buffer;
        assert(offset + size <= ubo->m_BaseUniformBuffer.m_Layout.m_Size);
        memcpy(ubo->m_DeviceBuffer.m_MappedDataPtr + offset, data, size);
    }

    static void DX12DisableUniformBuffer(HContext _context, HUniformBuffer uniform_buffer)
    {
        DX12Context* context = (DX12Context*)_context;
        DX12UniformBuffer* ubo = (DX12UniformBuffer*) uniform_buffer;

        if (ubo->m_BaseUniformBuffer.m_BoundSet == UNUSED_BINDING_OR_SET || ubo->m_BaseUniformBuffer.m_BoundBinding == UNUSED_BINDING_OR_SET)
        {
            return;
        }

        if (context->m_CurrentUniformBuffers[ubo->m_BaseUniformBuffer.m_BoundSet][ubo->m_BaseUniformBuffer.m_BoundBinding] == ubo)
        {
            context->m_CurrentUniformBuffers[ubo->m_BaseUniformBuffer.m_BoundSet][ubo->m_BaseUniformBuffer.m_BoundBinding] = 0;
        }

        ubo->m_BaseUniformBuffer.m_BoundSet     = UNUSED_BINDING_OR_SET;
        ubo->m_BaseUniformBuffer.m_BoundBinding = UNUSED_BINDING_OR_SET;
    }

    static void DX12EnableUniformBuffer(HContext _context, HUniformBuffer uniform_buffer, uint32_t binding, uint32_t set)
    {
        DX12Context* context = (DX12Context*) _context;
        DX12UniformBuffer* ubo = (DX12UniformBuffer*) uniform_buffer;

        ubo->m_BaseUniformBuffer.m_BoundBinding = binding;
        ubo->m_BaseUniformBuffer.m_BoundSet     = set;

        if (context->m_CurrentUniformBuffers[set][binding])
        {
            DX12DisableUniformBuffer(context, (HUniformBuffer) context->m_CurrentUniformBuffers[set][binding]);
        }

        context->m_CurrentUniformBuffers[set][binding] = ubo;
    }

    static void DX12DeleteUniformBuffer(HContext _context, HUniformBuffer uniform_buffer)
    {
        DX12Context* context = (DX12Context*) _context;
        DX12UniformBuffer* ubo = (DX12UniformBuffer*) uniform_buffer;

        DX12DisableUniformBuffer(_context, uniform_buffer);
        DestroyResourceDeferred(context->m_FrameResources[context->m_CurrentFrameIndex], &ubo->m_DeviceBuffer);

        delete ubo;
    }

    static HVertexBuffer DX12NewVertexBuffer(HContext _context, uint32_t size, const void* data, BufferUsage buffer_usage)
    {
        DX12Context* context        = (DX12Context*) _context;
        DX12VertexBuffer* vx_buffer = new DX12VertexBuffer();

        if (size > 0)
        {
            DeviceBufferUploadHelper(context, &vx_buffer->m_DeviceBuffer, data, size);
        }

        return (HVertexBuffer) vx_buffer;
    }

    static void DX12DeleteVertexBuffer(HVertexBuffer buffer)
    {
        DX12VertexBuffer* buffer_ptr = (DX12VertexBuffer*) buffer;
        DestroyResourceDeferred(g_DX12Context->m_FrameResources[g_DX12Context->m_CurrentFrameIndex], &buffer_ptr->m_DeviceBuffer);
    }

    static void DX12SetVertexBufferData(HVertexBuffer _buffer, uint32_t size, const void* data, BufferUsage buffer_usage)
    {
        DM_PROFILE(__FUNCTION__);

        if (size == 0)
        {
            return;
        }

        DX12VertexBuffer* vx_buffer = (DX12VertexBuffer*) _buffer;
        //if (size != vx_buffer->m_DeviceBuffer.m_DataSize)
        {
            DestroyResourceDeferred(g_DX12Context->m_FrameResources[g_DX12Context->m_CurrentFrameIndex], &vx_buffer->m_DeviceBuffer);
        }

        DeviceBufferUploadHelper(g_DX12Context, &vx_buffer->m_DeviceBuffer, data, size);
    }

    static void DX12SetVertexBufferSubData(HVertexBuffer buffer, uint32_t offset, uint32_t size, const void* data)
    {
        assert(0);
    }

    static uint32_t DX12GetVertexBufferSize(HVertexBuffer buffer)
    {
        if (!buffer)
        {
            return 0;
        }
        DX12VertexBuffer* buffer_ptr = (DX12VertexBuffer*) buffer;
        return buffer_ptr->m_DeviceBuffer.m_DataSize;
    }

    static uint32_t DX12GetMaxElementsVertices(HContext context)
    {
        return 65536;
    }

    static HIndexBuffer DX12NewIndexBuffer(HContext _context, uint32_t size, const void* data, BufferUsage buffer_usage)
    {
        DX12Context* context       = (DX12Context*) _context;
        DX12IndexBuffer* ix_buffer = new DX12IndexBuffer();

        if (size > 0)
        {
            DeviceBufferUploadHelper(context, &ix_buffer->m_DeviceBuffer, data, size);
        }

        return (HIndexBuffer) ix_buffer;
    }

    static void DX12DeleteIndexBuffer(HIndexBuffer buffer)
    {
        DX12IndexBuffer* buffer_ptr = (DX12IndexBuffer*) buffer;
        DestroyResourceDeferred(g_DX12Context->m_FrameResources[g_DX12Context->m_CurrentFrameIndex], &buffer_ptr->m_DeviceBuffer);
    }

    static void DX12SetIndexBufferData(HIndexBuffer buffer, uint32_t size, const void* data, BufferUsage buffer_usage)
    {
        DM_PROFILE(__FUNCTION__);

        if (size == 0)
        {
            return;
        }

        DX12IndexBuffer* ix_buffer = (DX12IndexBuffer*) buffer;
        //if (ix_buffer->m_DeviceBuffer.m_Resource == 0x0)
        //{
        //    CreateDeviceBuffer(g_DX12Context, &ix_buffer->m_DeviceBuffer, size);
        //}

        DestroyResourceDeferred(g_DX12Context->m_FrameResources[g_DX12Context->m_CurrentFrameIndex], &ix_buffer->m_DeviceBuffer);

        DeviceBufferUploadHelper(g_DX12Context, &ix_buffer->m_DeviceBuffer, data, size);
    }

    static void DX12SetIndexBufferSubData(HIndexBuffer buffer, uint32_t offset, uint32_t size, const void* data)
    {
        assert(0);
    }

    static uint32_t DX12GetIndexBufferSize(HIndexBuffer buffer)
    {
        if (!buffer)
        {
            return 0;
        }
        DX12IndexBuffer* buffer_ptr = (DX12IndexBuffer*) buffer;
        return buffer_ptr->m_DeviceBuffer.m_DataSize;
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
        DX12Context* context                          = (DX12Context*) _context;
        context->m_CurrentVertexBuffer[binding_index] = (DX12VertexBuffer*) vertex_buffer;
    }

    static void DX12DisableVertexBuffer(HContext _context, HVertexBuffer vertex_buffer)
    {
        DX12Context* context = (DX12Context*) _context;
        for (int i = 0; i < MAX_VERTEX_BUFFERS; ++i)
        {
            if (context->m_CurrentVertexBuffer[i] == (DX12VertexBuffer*) vertex_buffer)
                context->m_CurrentVertexBuffer[i] = 0;
        }
    }

    static void DX12EnableVertexDeclaration(HContext _context, HVertexDeclaration vertex_declaration, uint32_t binding_index, uint32_t base_offset, HProgram program)
    {
        DX12Context* context            = (DX12Context*) _context;
        DX12ShaderProgram* program_ptr  = (DX12ShaderProgram*) program;
        DX12ShaderModule* vertex_shader = program_ptr->m_VertexModule;

        context->m_MainVertexDeclaration[binding_index]                = {};
        context->m_MainVertexDeclaration[binding_index].m_Stride       = vertex_declaration->m_Stride;
        context->m_MainVertexDeclaration[binding_index].m_StepFunction = vertex_declaration->m_StepFunction;
        context->m_MainVertexDeclaration[binding_index].m_PipelineHash = vertex_declaration->m_PipelineHash;

        context->m_CurrentVertexDeclaration[binding_index]             = &context->m_MainVertexDeclaration[binding_index];

        // TODO
        // context->m_CurrentVertexBufferOffset[binding_index]            = base_offset;

        uint32_t stream_ix = 0;
        uint32_t num_inputs = program_ptr->m_BaseProgram.m_ShaderMeta.m_Inputs.Size();

        for (int i = 0; i < vertex_declaration->m_StreamCount; ++i)
        {
            for (int j = 0; j < num_inputs; ++j)
            {
                ShaderResourceBinding& input = program_ptr->m_BaseProgram.m_ShaderMeta.m_Inputs[j];

                if (input.m_StageFlags & SHADER_STAGE_FLAG_VERTEX && input.m_NameHash == vertex_declaration->m_Streams[i].m_NameHash)
                {
                    VertexDeclaration::Stream& stream = context->m_MainVertexDeclaration[binding_index].m_Streams[stream_ix];
                    stream.m_NameHash  = input.m_NameHash;
                    stream.m_Location  = input.m_Binding;
                    stream.m_Type      = vertex_declaration->m_Streams[i].m_Type;
                    stream.m_Offset    = vertex_declaration->m_Streams[i].m_Offset;
                    stream.m_Size      = vertex_declaration->m_Streams[i].m_Size;
                    stream.m_Normalize = vertex_declaration->m_Streams[i].m_Normalize;
                    stream_ix++;

                    context->m_MainVertexDeclaration[binding_index].m_StreamCount++;
                    break;
                }
            }
        }
    }

    static void DX12DisableVertexDeclaration(HContext _context, HVertexDeclaration vertex_declaration)
    {
        DX12Context* context = (DX12Context*) _context;
        for (int i = 0; i < MAX_VERTEX_BUFFERS; ++i)
        {
            if (context->m_CurrentVertexDeclaration[i] == vertex_declaration)
                context->m_CurrentVertexDeclaration[i] = 0;
        }
    }

    static inline D3D_PRIMITIVE_TOPOLOGY GetPrimitiveTopology(PrimitiveType prim_type)
    {
        switch(prim_type)
        {
            case PRIMITIVE_LINES:          return D3D_PRIMITIVE_TOPOLOGY_LINELIST;
            case PRIMITIVE_TRIANGLES:      return D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
            case PRIMITIVE_TRIANGLE_STRIP: return D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP;
            default:break;
        }
        return D3D_PRIMITIVE_TOPOLOGY_UNDEFINED;
    }

    static inline DXGI_FORMAT GetDXGIFormat(Type type, uint16_t size, bool normalized)
    {
        /*
        // undefined formats:
        // DXGI_FORMAT_R8G8B8_SNORM
        // DXGI_FORMAT_R8G8B8_SINT
        // DXGI_FORMAT_R8G8B8_UNORM
        // DXGI_FORMAT_R8G8B8_UINT
        // DXGI_FORMAT_R16G16B16_SNORM
        // DXGI_FORMAT_R16G16B16_SINT
        // DXGI_FORMAT_R16G16B16_UNORM
        // DXGI_FORMAT_R16G16B16_UINT
        */
        if (type == TYPE_FLOAT)
        {
            switch(size)
            {
                case 1: return DXGI_FORMAT_R32_FLOAT;
                case 2: return DXGI_FORMAT_R32G32_FLOAT;
                case 3: return DXGI_FORMAT_R32G32B32_FLOAT;
                case 4: return DXGI_FORMAT_R32G32B32A32_FLOAT;
                default:break;
            }
        }
        else if (type == TYPE_INT)
        {
            switch(size)
            {
                case 1: return DXGI_FORMAT_R32_SINT;
                case 2: return DXGI_FORMAT_R32G32_SINT;
                case 3: return DXGI_FORMAT_R32G32B32_SINT;
                case 4: return DXGI_FORMAT_R32G32B32A32_SINT;
                default:break;
            }
        }
        else if (type == TYPE_UNSIGNED_INT)
        {
            switch(size)
            {
                case 1: return DXGI_FORMAT_R32_UINT;
                case 2: return DXGI_FORMAT_R32G32_UINT;
                case 3: return DXGI_FORMAT_R32G32B32_UINT;
                case 4: return DXGI_FORMAT_R32G32B32A32_UINT;
                default:break;
            }
        }
        else if (type == TYPE_BYTE)
        {
            switch(size)
            {
                case 1: return normalized ? DXGI_FORMAT_R8_SNORM : DXGI_FORMAT_R8_SINT;
                case 2: return normalized ? DXGI_FORMAT_R8G8_SNORM : DXGI_FORMAT_R8G8_SINT;
                // case 3: return normalized ? DXGI_FORMAT_R8G8B8_SNORM : DXGI_FORMAT_R8G8B8_SINT;
                case 4: return normalized ? DXGI_FORMAT_R8G8B8A8_SNORM : DXGI_FORMAT_R8G8B8A8_SINT;
                default:break;
            }
        }
        else if (type == TYPE_UNSIGNED_BYTE)
        {
            switch(size)
            {
                case 1: return normalized ? DXGI_FORMAT_R8_UNORM : DXGI_FORMAT_R8_UINT;
                case 2: return normalized ? DXGI_FORMAT_R8G8_UNORM : DXGI_FORMAT_R8G8_UINT;
                // case 3: return normalized ? DXGI_FORMAT_R8G8B8_UNORM : DXGI_FORMAT_R8G8B8_UINT;
                case 4: return normalized ? DXGI_FORMAT_R8G8B8A8_UNORM : DXGI_FORMAT_R8G8B8A8_UINT;
                default:break;
            }
        }
        else if (type == TYPE_SHORT)
        {
            switch(size)
            {
                case 1: return normalized ? DXGI_FORMAT_R16_SNORM : DXGI_FORMAT_R16_SINT;
                case 2: return normalized ? DXGI_FORMAT_R16G16_SNORM : DXGI_FORMAT_R16G16_SINT;
                //case 3: return normalized ? DXGI_FORMAT_R16G16B16_SNORM : DXGI_FORMAT_R16G16B16_SINT;
                case 4: return normalized ? DXGI_FORMAT_R16G16B16A16_SNORM : DXGI_FORMAT_R16G16B16A16_SINT;
                default:break;
            }
        }
        else if (type == TYPE_UNSIGNED_SHORT)
        {
            switch(size)
            {
                case 1: return normalized ? DXGI_FORMAT_R16_UNORM : DXGI_FORMAT_R16_UINT;
                case 2: return normalized ? DXGI_FORMAT_R16G16_UNORM : DXGI_FORMAT_R16G16_UINT;
                //case 3: return normalized ? DXGI_FORMAT_R16G16B16_UNORM : DXGI_FORMAT_R16G16B16_UINT;
                case 4: return normalized ? DXGI_FORMAT_R16G16B16A16_UNORM : DXGI_FORMAT_R16G16B16A16_UINT;
                default:break;
            }
        }
        else if (type == TYPE_FLOAT_MAT4)
        {
            return DXGI_FORMAT_R32_FLOAT;
        }
        else if (type == TYPE_FLOAT_VEC4)
        {
            return DXGI_FORMAT_R32G32B32A32_FLOAT;
        }

        assert(0 && "Unable to deduce type from dmGraphics::Type");
        return DXGI_FORMAT_UNKNOWN;
    }

    static inline D3D12_CULL_MODE GetCullMode(const PipelineState& state)
    {
        if (state.m_CullFaceEnabled)
        {
            if (state.m_CullFaceType == FACE_TYPE_BACK)
                return D3D12_CULL_MODE_BACK;
            else if (state.m_CullFaceType == FACE_TYPE_FRONT)
                return D3D12_CULL_MODE_FRONT;
            // FRONT_AND_BACK not supported
        }
        return D3D12_CULL_MODE_NONE;
    }

    static inline D3D12_COMPARISON_FUNC GetDepthTestFunc(const PipelineState& state)
    {
        if (state.m_DepthTestEnabled)
        {
            switch(state.m_DepthTestFunc)
            {
                case COMPARE_FUNC_NEVER:    return D3D12_COMPARISON_FUNC_NEVER;
                case COMPARE_FUNC_LESS:     return D3D12_COMPARISON_FUNC_LESS;
                case COMPARE_FUNC_LEQUAL:   return D3D12_COMPARISON_FUNC_LESS_EQUAL;
                case COMPARE_FUNC_GREATER:  return D3D12_COMPARISON_FUNC_GREATER;
                case COMPARE_FUNC_GEQUAL:   return D3D12_COMPARISON_FUNC_GREATER_EQUAL;
                case COMPARE_FUNC_EQUAL:    return D3D12_COMPARISON_FUNC_EQUAL;
                case COMPARE_FUNC_NOTEQUAL: return D3D12_COMPARISON_FUNC_NOT_EQUAL;
                case COMPARE_FUNC_ALWAYS:   return D3D12_COMPARISON_FUNC_ALWAYS;
                default:break;
            }
        }
        return D3D12_COMPARISON_FUNC_NONE;
    }

    static inline D3D12_BLEND GetBlendFactor(BlendFactor factor)
    {
        switch(factor)
        {
            case BLEND_FACTOR_ZERO:                 return D3D12_BLEND_ZERO;
            case BLEND_FACTOR_ONE:                  return D3D12_BLEND_ONE;
            case BLEND_FACTOR_SRC_COLOR:            return D3D12_BLEND_SRC_COLOR;
            case BLEND_FACTOR_ONE_MINUS_SRC_COLOR:  return D3D12_BLEND_INV_SRC_COLOR;
            case BLEND_FACTOR_DST_COLOR:            return D3D12_BLEND_DEST_COLOR;
            case BLEND_FACTOR_ONE_MINUS_DST_COLOR:  return D3D12_BLEND_INV_DEST_COLOR;
            case BLEND_FACTOR_SRC_ALPHA:            return D3D12_BLEND_SRC_ALPHA;
            case BLEND_FACTOR_ONE_MINUS_SRC_ALPHA:  return D3D12_BLEND_INV_SRC_ALPHA;
            case BLEND_FACTOR_DST_ALPHA:            return D3D12_BLEND_DEST_ALPHA;
            case BLEND_FACTOR_ONE_MINUS_DST_ALPHA:  return D3D12_BLEND_INV_DEST_ALPHA;
            case BLEND_FACTOR_SRC_ALPHA_SATURATE:   return D3D12_BLEND_SRC_ALPHA_SAT;

            // No idea about these.
            // case BLEND_FACTOR_CONSTANT_COLOR:           return ;
            // case BLEND_FACTOR_ONE_MINUS_CONSTANT_COLOR: return ;
            // case BLEND_FACTOR_CONSTANT_ALPHA:           return ;
            // case BLEND_FACTOR_ONE_MINUS_CONSTANT_ALPHA: return ;
            default: break;
        }
        return D3D12_BLEND_ZERO;
    }

    static inline void WriteConstantData(uint32_t offset, uint8_t* uniform_data_ptr, uint8_t* data_ptr, uint32_t data_size)
    {
        memcpy(&uniform_data_ptr[offset], data_ptr, data_size);
    }

    static void CreateComputePipeline(DX12Context* context, DX12Pipeline* pipeline)
    {
        assert(context->m_CurrentProgram && context->m_CurrentProgram->m_ComputeModule);

        D3D12_COMPUTE_PIPELINE_STATE_DESC psoDesc = {};
        psoDesc.pRootSignature = context->m_CurrentProgram->m_RootSignature;

        psoDesc.CS.pShaderBytecode = context->m_CurrentProgram->m_ComputeModule->m_ShaderBlob->GetBufferPointer();
        psoDesc.CS.BytecodeLength  = context->m_CurrentProgram->m_ComputeModule->m_ShaderBlob->GetBufferSize();

        psoDesc.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;

        HRESULT hr = context->m_Device->CreateComputePipelineState(&psoDesc, IID_PPV_ARGS(pipeline));
        CHECK_HR_ERROR(hr);
    }

    static DX12Pipeline* GetOrCreateComputePipeline(DX12Context* context)
    {
        HashState64 pipeline_hash_state;
        dmHashInit64(&pipeline_hash_state, false);
        dmHashUpdateBuffer64(&pipeline_hash_state, &context->m_CurrentProgram->m_Hash, sizeof(context->m_CurrentProgram->m_Hash));

        uint64_t pipeline_hash = dmHashFinal64(&pipeline_hash_state);
        DX12Pipeline* cached_pipeline = context->m_PipelineCache.Get(pipeline_hash);

        if (!cached_pipeline)
        {
            if (context->m_PipelineCache.Full())
            {
                context->m_PipelineCache.SetCapacity(32, context->m_PipelineCache.Capacity() + 4);
            }

            context->m_PipelineCache.Put(pipeline_hash, {});
            cached_pipeline = context->m_PipelineCache.Get(pipeline_hash);

            CreateComputePipeline(context, cached_pipeline);

            dmLogDebug("Created new DX12 Compute Pipeline with hash %llu", (unsigned long long) pipeline_hash);
        }

        return cached_pipeline;
    }

    static void CreatePipeline(DX12Context* context, DX12RenderTarget* rt, DX12Pipeline* pipeline)
    {
        D3D12_SHADER_BYTECODE vs_byte_code = {};
        vs_byte_code.BytecodeLength        = context->m_CurrentProgram->m_VertexModule->m_ShaderBlob->GetBufferSize();
        vs_byte_code.pShaderBytecode       = context->m_CurrentProgram->m_VertexModule->m_ShaderBlob->GetBufferPointer();

        D3D12_SHADER_BYTECODE fs_byte_code = {};
        fs_byte_code.BytecodeLength        = context->m_CurrentProgram->m_FragmentModule->m_ShaderBlob->GetBufferSize();
        fs_byte_code.pShaderBytecode       = context->m_CurrentProgram->m_FragmentModule->m_ShaderBlob->GetBufferPointer();

        uint32_t stream_count = 0;
        D3D12_INPUT_ELEMENT_DESC input_layout[MAX_VERTEX_STREAM_COUNT] = {};

        for (int i = 0; i < MAX_VERTEX_BUFFERS; ++i)
        {
            if (context->m_CurrentVertexDeclaration[i])
            {
                for (int j = 0; j < context->m_CurrentVertexDeclaration[i]->m_StreamCount; ++j)
                {
                    VertexDeclaration::Stream& stream = context->m_CurrentVertexDeclaration[i]->m_Streams[j];
                    D3D12_INPUT_ELEMENT_DESC& desc    = input_layout[stream_count];

                    desc.SemanticName         = "TEXCOORD";
                    desc.SemanticIndex        = stream.m_Location;
                    desc.Format               = GetDXGIFormat(stream.m_Type, stream.m_Size, stream.m_Normalize);
                    desc.InputSlot            = i;
                    desc.AlignedByteOffset    = stream.m_Offset;
                    desc.InputSlotClass       = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;
                    desc.InstanceDataStepRate = 0;

                    stream_count++;
                }
            }
        }

        D3D12_INPUT_LAYOUT_DESC inputLayoutDesc = {};
        inputLayoutDesc.NumElements             = stream_count;
        inputLayoutDesc.pInputElementDescs      = input_layout;

        CD3DX12_RASTERIZER_DESC rasterizerState = CD3DX12_RASTERIZER_DESC(
            D3D12_FILL_MODE_SOLID,
            GetCullMode(context->m_PipelineState),
            true,                                       // FrontCounterClockwise
            D3D12_DEFAULT_DEPTH_BIAS,
            D3D12_DEFAULT_DEPTH_BIAS_CLAMP,
            D3D12_DEFAULT_SLOPE_SCALED_DEPTH_BIAS,
            true,                                       // DepthClipEnable
            false,                                      // MultisampleEnable
            false,                                      // AntialiasedLineEnable: TODO
            0,                                          // forcedSampleCount
            D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF); // conservativeRaster

        D3D12_DEPTH_STENCIL_DESC depthStencilDesc = {};
        depthStencilDesc.DepthEnable      = context->m_PipelineState.m_DepthTestEnabled;
        depthStencilDesc.DepthWriteMask   = (D3D12_DEPTH_WRITE_MASK) context->m_PipelineState.m_WriteDepth; // D3D12_DEPTH_WRITE_MASK_ZERO or D3D12_DEPTH_WRITE_MASK_ALL
        depthStencilDesc.DepthFunc        = GetDepthTestFunc(context->m_PipelineState);
        depthStencilDesc.StencilEnable    = context->m_PipelineState.m_StencilEnabled;
        depthStencilDesc.StencilReadMask  = D3D12_DEFAULT_STENCIL_READ_MASK; // TODO
        depthStencilDesc.StencilWriteMask = D3D12_DEFAULT_STENCIL_WRITE_MASK; // TODO

        // TODO
        const D3D12_DEPTH_STENCILOP_DESC defaultStencilOp = { D3D12_STENCIL_OP_KEEP, D3D12_STENCIL_OP_KEEP, D3D12_STENCIL_OP_KEEP, D3D12_COMPARISON_FUNC_ALWAYS };
        depthStencilDesc.FrontFace = defaultStencilOp;
        depthStencilDesc.BackFace  = defaultStencilOp;

        D3D12_BLEND_DESC blendDesc = {};
        blendDesc.AlphaToCoverageEnable                 = false;
        blendDesc.IndependentBlendEnable                = false;
        blendDesc.RenderTarget[0].BlendEnable           = context->m_PipelineState.m_BlendEnabled;
        blendDesc.RenderTarget[0].LogicOpEnable         = false;
        blendDesc.RenderTarget[0].SrcBlend              = GetBlendFactor((BlendFactor) context->m_PipelineState.m_BlendSrcFactor);
        blendDesc.RenderTarget[0].DestBlend             = GetBlendFactor((BlendFactor) context->m_PipelineState.m_BlendDstFactor);
        blendDesc.RenderTarget[0].BlendOp               = D3D12_BLEND_OP_ADD;
        blendDesc.RenderTarget[0].SrcBlendAlpha         = GetBlendFactor((BlendFactor) context->m_PipelineState.m_BlendSrcFactor);
        blendDesc.RenderTarget[0].DestBlendAlpha        = GetBlendFactor((BlendFactor) context->m_PipelineState.m_BlendDstFactor);
        blendDesc.RenderTarget[0].BlendOpAlpha          = D3D12_BLEND_OP_ADD;
        blendDesc.RenderTarget[0].LogicOp               = D3D12_LOGIC_OP_NOOP;
        blendDesc.RenderTarget[0].RenderTargetWriteMask = context->m_PipelineState.m_WriteColorMask;

        D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {}; // a structure to define a pso
        psoDesc.InputLayout           = inputLayoutDesc; // the structure describing our input layout
        psoDesc.pRootSignature        = context->m_CurrentProgram->m_RootSignature;
        psoDesc.VS                    = vs_byte_code;
        psoDesc.PS                    = fs_byte_code;
        psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE; // SHould we support points?
        psoDesc.RTVFormats[0]         = rt->m_Format;
        psoDesc.SampleDesc            = rt->m_SampleDesc; // must be the same sample description as the swapchain and depth/stencil buffer
        psoDesc.SampleMask            = UINT_MAX; // TODO: sample mask has to do with multi-sampling. 0xffffffff means point sampling is done
        psoDesc.RasterizerState       = rasterizerState; // TODO? rasterizerState;
        psoDesc.BlendState            = blendDesc; // TODO
        psoDesc.DepthStencilState     = depthStencilDesc;
        psoDesc.NumRenderTargets      = 1; // TODO
        psoDesc.DSVFormat             = DXGI_FORMAT_D24_UNORM_S8_UINT;

        if (rt->m_Id == DM_RENDERTARGET_BACKBUFFER_ID && context->m_MSAASampleCount > 1)
        {
            psoDesc.RasterizerState.MultisampleEnable = true;
            psoDesc.SampleDesc.Count                  = context->m_MSAASampleCount;
            psoDesc.SampleDesc.Quality                = 0;
        }

        HRESULT hr = context->m_Device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(pipeline));
        CHECK_HR_ERROR(hr);
    }

    static DX12Pipeline* GetOrCreatePipeline(DX12Context* context, DX12RenderTarget* current_rt, uint32_t num_vx_decls)
    {
        HashState64 pipeline_hash_state;
        dmHashInit64(&pipeline_hash_state, false);
        dmHashUpdateBuffer64(&pipeline_hash_state, &context->m_CurrentProgram->m_Hash,          sizeof(context->m_CurrentProgram->m_Hash));
        dmHashUpdateBuffer64(&pipeline_hash_state, &context->m_PipelineState,                   sizeof(context->m_PipelineState));
        dmHashUpdateBuffer64(&pipeline_hash_state, &current_rt->m_Id,                           sizeof(current_rt->m_Id));
        dmHashUpdateBuffer64(&pipeline_hash_state, &context->m_CurrentProgram->m_RootSignature, sizeof(context->m_CurrentProgram->m_RootSignature));
        // dmHashUpdateBuffer64(&pipeline_hash_state, &vk_sample_count,  sizeof(vk_sample_count));

        for (int i = 0; i < num_vx_decls; ++i)
        {
            dmHashUpdateBuffer64(&pipeline_hash_state, &context->m_CurrentVertexDeclaration[i]->m_PipelineHash, sizeof(context->m_CurrentVertexDeclaration[i]->m_PipelineHash));
            dmHashUpdateBuffer64(&pipeline_hash_state, &context->m_CurrentVertexDeclaration[i]->m_StepFunction, sizeof(context->m_CurrentVertexDeclaration[i]->m_StepFunction));
        }

        dmhash_t pipeline_hash = dmHashFinal64(&pipeline_hash_state);
        DX12Pipeline* cached_pipeline = context->m_PipelineCache.Get(pipeline_hash);

        if (!cached_pipeline)
        {
            if (context->m_PipelineCache.Full())
            {
                context->m_PipelineCache.SetCapacity(32, context->m_PipelineCache.Capacity() + 4);
            }

            context->m_PipelineCache.Put(pipeline_hash, {});
            cached_pipeline = context->m_PipelineCache.Get(pipeline_hash);
            CreatePipeline(context, current_rt, cached_pipeline);

            dmLogDebug("Created new DX12 Pipeline with hash %llu", (unsigned long long) pipeline_hash);
        }

        return cached_pipeline;
    }

    static inline void SetViewportAndScissorHelper(DX12Context* context, int32_t x, int32_t y, int32_t width, int32_t height)
    {
        D3D12_VIEWPORT viewport;
        D3D12_RECT scissor;

        viewport.TopLeftX = (float) x;
        viewport.TopLeftY = (float) y;
        viewport.Width    = (float) width;
        viewport.Height   = (float) height;
        viewport.MinDepth = 0.0f;
        viewport.MaxDepth = 1.0f;

        scissor.left   = (float) x;
        scissor.top    = (float) y;
        scissor.right  = (float) width;
        scissor.bottom = (float) height;

        context->m_CommandList->RSSetViewports(1, &viewport);
        context->m_CommandList->RSSetScissorRects(1, &scissor);
    }

    static void CommitUniforms(DX12Context* context, DX12FrameResource& frame_resources, DX12PipelineType pipeline_type)
    {
        DX12ShaderProgram* program = context->m_CurrentProgram;

        uint32_t texture_unit_start = program->m_UniformBufferCount;
        uint32_t ubo_ix = 0;

        for (int i = 0; i < program->m_RootSignatureResources.Size(); ++i)
        {
            DX12ResourceBinding& dx12_res = program->m_RootSignatureResources[i];
            ProgramResourceBinding& pgm_res = program->m_BaseProgram.m_ResourceBindings[dx12_res.m_Set][dx12_res.m_Binding];

            switch(pgm_res.m_Res->m_BindingFamily)
            {
                case BINDING_FAMILY_TEXTURE:
                {
                    DX12Texture* texture = GetAssetFromContainer<DX12Texture>(context->m_AssetHandleContainer, context->m_CurrentTextures[pgm_res.m_TextureUnit]);

                    if (pgm_res.m_Res->m_Type.m_ShaderType == ShaderDesc::SHADER_TYPE_SAMPLER)
                    {
                        const DX12TextureSampler& sampler = context->m_TextureSamplers[texture->m_TextureSamplerIndex];
                        frame_resources.m_ScratchBuffer.AllocateSampler(context, pipeline_type, sampler, i);
                    }
                    else
                    {
                        frame_resources.m_ScratchBuffer.AllocateTexture2D(context, pipeline_type, texture, i);

                        // Transition all mipmaps into pixel read state
                        for (int i = 0; i < texture->m_MipMapCount; ++i)
                        {
                            if (texture->m_ResourceStates[i] != D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE)
                            {
                                context->m_CommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(texture->m_Resource, texture->m_ResourceStates[i], D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, (UINT) i));
                                texture->m_ResourceStates[i] = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
                            }
                        }
                    }
                } break;
                case BINDING_FAMILY_STORAGE_BUFFER:
                {
                    assert(0);
                } break;
                case BINDING_FAMILY_UNIFORM_BUFFER:
                {
                    DX12UniformBuffer* bound_ubo = context->m_CurrentUniformBuffers[dx12_res.m_Set][dx12_res.m_Binding];

                    if (bound_ubo)
                    {
                        UniformBufferLayout* pgm_layout = (UniformBufferLayout*) pgm_res.m_BindingUserData;
                        if (bound_ubo->m_BaseUniformBuffer.m_Layout.m_Hash != pgm_layout->m_Hash)
                        {
                            dmLogWarning("Uniform buffer with hash %d has an incompatible layout with the currently bound program at the shader binding '%s' (hash=%d)",
                                bound_ubo->m_BaseUniformBuffer.m_Layout.m_Hash,
                                pgm_res.m_Res->m_Name,
                                pgm_layout->m_Hash);

                            // Fallback to the scratch buffer uniform setup
                            bound_ubo = 0;
                        }
                    }

                    if (bound_ubo)
                    {
                        D3D12_GPU_VIRTUAL_ADDRESS gpu_addr = bound_ubo->m_DeviceBuffer.m_Resource->GetGPUVirtualAddress();

                        if (pipeline_type == PIPELINE_TYPE_GRAPHICS)
                        {
                            context->m_CommandList->SetGraphicsRootConstantBufferView(i, gpu_addr);
                        }
                        else
                        {
                            context->m_CommandList->SetComputeRootConstantBufferView(i, gpu_addr);
                        }
                    }
                    else
                    {
                        const uint32_t uniform_size_nonalign = pgm_res.m_Res->m_BindingInfo.m_BlockSize;
                        void* gpu_mapped_memory = frame_resources.m_ScratchBuffer.AllocateConstantBuffer(context, pipeline_type, i, uniform_size_nonalign);
                        memcpy(gpu_mapped_memory, &program->m_UniformData[pgm_res.m_UniformBufferOffset], uniform_size_nonalign);
                    }
                } break;
                case BINDING_FAMILY_GENERIC:
                default: continue;
            }
        }
    }

    static void DrawSetup(DX12Context* context, PrimitiveType prim_type)
    {
        assert(context->m_CurrentProgram);

        DX12FrameResource& frame_resources = context->m_FrameResources[context->m_CurrentFrameIndex];

        DX12RenderTarget* current_rt = GetAssetFromContainer<DX12RenderTarget>(context->m_AssetHandleContainer, context->m_CurrentRenderTarget);

        D3D12_VERTEX_BUFFER_VIEW vx_buffer_views[MAX_VERTEX_BUFFERS];
        uint32_t num_vx_buffers = 0;

        for (int i = 0; i < MAX_VERTEX_BUFFERS; ++i)
        {
            if (context->m_CurrentVertexBuffer[i] && context->m_CurrentVertexDeclaration[i])
            {
                vx_buffer_views[num_vx_buffers].BufferLocation = context->m_CurrentVertexBuffer[i]->m_DeviceBuffer.m_Resource->GetGPUVirtualAddress();
                vx_buffer_views[num_vx_buffers].SizeInBytes    = context->m_CurrentVertexBuffer[i]->m_DeviceBuffer.m_DataSize;
                vx_buffer_views[num_vx_buffers].StrideInBytes  = context->m_CurrentVertexDeclaration[i]->m_Stride;
                num_vx_buffers++;
            }
        }

        // Update the viewport
        if (context->m_ViewportChanged)
        {
            DX12Viewport& vp = context->m_CurrentViewport;
            SetViewportAndScissorHelper(context, vp.m_X, vp.m_Y, vp.m_W, vp.m_H);
            context->m_ViewportChanged = 0;
        }

        DX12Pipeline* pipeline = GetOrCreatePipeline(context, current_rt, num_vx_buffers);
        context->m_CommandList->SetGraphicsRootSignature(context->m_CurrentProgram->m_RootSignature);
        context->m_CommandList->SetPipelineState(*pipeline);
        context->m_CommandList->IASetPrimitiveTopology(GetPrimitiveTopology(prim_type));
        context->m_CommandList->IASetVertexBuffers(0, num_vx_buffers, vx_buffer_views); // set the vertex buffer (using the vertex buffer view)

        // frame_resources.m_ScratchBuffer.Bind(context);
        CommitUniforms(context, frame_resources, PIPELINE_TYPE_GRAPHICS);
    }

    static void DrawSetupCompute(DX12Context* context, uint32_t group_count_x, uint32_t group_count_y, uint32_t group_count_z)
    {
        DX12FrameResource& frame_resources = context->m_FrameResources[context->m_CurrentFrameIndex];

        DX12Pipeline* pipeline = GetOrCreateComputePipeline(context);
        context->m_CommandList->SetComputeRootSignature(context->m_CurrentProgram->m_RootSignature);
        context->m_CommandList->SetPipelineState(*pipeline);

        // gl_NumWorkGroups is a cbuffer in HLSL, so we need to put the group counts in that special buffer first.
        if (context->m_CurrentProgram->m_NumWorkGroupsResourceIndex != 0xff)
        {
            uint32_t data[] = { group_count_x, group_count_y, group_count_z };
            DX12ResourceBinding& workgroup_res = context->m_CurrentProgram->m_RootSignatureResources[context->m_CurrentProgram->m_NumWorkGroupsResourceIndex];
            ProgramResourceBinding& pgm_res = context->m_CurrentProgram->m_BaseProgram.m_ResourceBindings[workgroup_res.m_Set][workgroup_res.m_Binding];
            WriteConstantData(pgm_res.m_UniformBufferOffset, context->m_CurrentProgram->m_UniformData, (uint8_t*) data, sizeof(data));
        }

        CommitUniforms(context, frame_resources, PIPELINE_TYPE_COMPUTE);
    }

    static void DX12DrawElements(HContext _context, PrimitiveType prim_type, uint32_t first, uint32_t count, Type type, HIndexBuffer index_buffer, uint32_t instance_count)
    {
        DM_PROFILE(__FUNCTION__);
        DM_PROPERTY_ADD_U32(rmtp_DrawCalls, 1);

        DX12Context* context = (DX12Context*) _context;
        DrawSetup(context, prim_type);

        DX12IndexBuffer* ix_buffer   = (DX12IndexBuffer*) index_buffer;
        D3D12_INDEX_BUFFER_VIEW view = {};
        view.BufferLocation          = ix_buffer->m_DeviceBuffer.m_Resource->GetGPUVirtualAddress();
        view.SizeInBytes             = ix_buffer->m_DeviceBuffer.m_DataSize;
        view.Format                  = type == dmGraphics::TYPE_UNSIGNED_SHORT ? DXGI_FORMAT_R16_UINT : DXGI_FORMAT_R32_UINT;
        uint32_t index_offset        = first / (type == TYPE_UNSIGNED_SHORT ? 2 : 4);

        context->m_CommandList->IASetIndexBuffer(&view);
        context->m_CommandList->DrawIndexedInstanced(count, dmMath::Max((uint32_t) 1, instance_count), index_offset, 0, 0); // draw first quad
    }

    static void DX12Draw(HContext _context, PrimitiveType prim_type, uint32_t first, uint32_t count, uint32_t instance_count)
    {
        DM_PROFILE(__FUNCTION__);
        DM_PROPERTY_ADD_U32(rmtp_DrawCalls, 1);

        DX12Context* context = (DX12Context*) _context;
        DrawSetup(context, prim_type);

        context->m_CommandList->DrawInstanced(count, dmMath::Max((uint32_t) 1, instance_count), first, 0);
    }

    static void DX12DispatchCompute(HContext _context, uint32_t group_count_x, uint32_t group_count_y, uint32_t group_count_z)
    {
        DM_PROFILE(__FUNCTION__);
        DM_PROPERTY_ADD_U32(rmtp_DispatchCalls, 1);

         DX12Context* context = (DX12Context*) _context;

        // From graphics_vulkan.cpp
        if (IsRenderTargetbound(context, context->m_CurrentRenderTarget))
        {
            EndRenderPass(context);
        }

        DrawSetupCompute(context, group_count_x, group_count_y, group_count_z);

        context->m_CommandList->Dispatch(group_count_x, group_count_y, group_count_z);
    }

    static D3D12_SHADER_VISIBILITY GetShaderVisibilityFromStage(uint8_t stage_flag)
    {
        if (stage_flag & SHADER_STAGE_FLAG_VERTEX && stage_flag & SHADER_STAGE_FLAG_FRAGMENT)
        {
            return D3D12_SHADER_VISIBILITY_ALL;
        }
        else if (stage_flag & SHADER_STAGE_FLAG_VERTEX)
        {
            return D3D12_SHADER_VISIBILITY_VERTEX;
        }
        else if (stage_flag & SHADER_STAGE_FLAG_FRAGMENT)
        {
            return D3D12_SHADER_VISIBILITY_PIXEL;
        }
        else if (stage_flag & SHADER_STAGE_FLAG_COMPUTE)
        {
            return D3D12_SHADER_VISIBILITY_ALL;
        }
        return D3D12_SHADER_VISIBILITY_ALL;
    }

    static void ResolveSamplerTextureUnits(DX12ShaderProgram* program, const dmArray<ShaderResourceBinding>& texture_resources)
    {
        for (int i = 0; i < texture_resources.Size(); ++i)
        {
            const ShaderResourceBinding& shader_res = texture_resources[i];
            assert(shader_res.m_BindingFamily == BINDING_FAMILY_TEXTURE);

            ProgramResourceBinding& shader_pgm_res = program->m_BaseProgram.m_ResourceBindings[shader_res.m_Set][shader_res.m_Binding];

            if (shader_res.m_Type.m_ShaderType == ShaderDesc::SHADER_TYPE_SAMPLER)
            {
                const ShaderResourceBinding& texture_shader_res = texture_resources[shader_pgm_res.m_Res->m_BindingInfo.m_SamplerTextureIndex];
                const ProgramResourceBinding& texture_pgm_res   = program->m_BaseProgram.m_ResourceBindings[texture_shader_res.m_Set][texture_shader_res.m_Binding];
                shader_pgm_res.m_TextureUnit                    = texture_pgm_res.m_TextureUnit;
            #if 0 // Debug
                dmLogInfo("Resolving sampler at %d, %d to texture unit %d", shader_res.m_Set, shader_res.m_Binding, shader_pgm_res.m_TextureUnit);
            #endif
            }
        }
    }

    static void CreateProgramResourceBindings(DX12ShaderProgram* program, DX12ShaderModule* vertex_module, DX12ShaderModule* fragment_module, DX12ShaderModule* compute_module)
    {
        ResourceBindingDesc bindings[MAX_SET_COUNT][MAX_BINDINGS_PER_SET_COUNT] = {};

        uint32_t ubo_alignment = UNIFORM_BUFFERS_ALIGNMENT;
        uint32_t ssbo_alignment = 0; // TODO

        ProgramResourceBindingsInfo binding_info = {};
        FillProgramResourceBindings(&program->m_BaseProgram, bindings, ubo_alignment, ssbo_alignment, binding_info);

        // Each module must resolve samplers individually since there is no contextual information across modules (currently)
        ResolveSamplerTextureUnits(program, program->m_BaseProgram.m_ShaderMeta.m_Textures);

        program->m_BaseProgram.m_MaxSet     = binding_info.m_MaxSet;
        program->m_BaseProgram.m_MaxBinding = binding_info.m_MaxBinding;

        program->m_UniformData = new uint8_t[binding_info.m_UniformDataSize];
        memset(program->m_UniformData, 0, binding_info.m_UniformDataSize);

        program->m_UniformDataSizeAligned = binding_info.m_UniformDataSizeAligned;
        program->m_UniformBufferCount     = binding_info.m_UniformBufferCount;
        program->m_StorageBufferCount     = binding_info.m_StorageBufferCount;
        program->m_TextureSamplerCount    = binding_info.m_TextureCount + binding_info.m_SamplerCount;
        program->m_TotalResourcesCount    = binding_info.m_UniformBufferCount + binding_info.m_TextureCount + binding_info.m_SamplerCount + binding_info.m_StorageBufferCount; // num actual descriptors

        BuildUniforms(&program->m_BaseProgram);
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


        hr = D3DGetBlobPart(shader->m_ShaderBlob->GetBufferPointer(), shader->m_ShaderBlob->GetBufferSize(), D3D_BLOB_ROOT_SIGNATURE, 0, &shader->m_RootSignatureBlob);
        if (FAILED(hr))
        {
            // TODO: Extract reflection and generate root signature that way. There is no embedded root signature (which can happen when content has been built on non-windows platforms)
        }

        HashState64 shader_hash_state;
        dmHashInit64(&shader_hash_state, false);
        dmHashUpdateBuffer64(&shader_hash_state, data, data_size);
        shader->m_Hash = dmHashFinal64(&shader_hash_state);

        return S_OK;
    }

    static HRESULT ConcatenateRootSignatures(ID3DBlob* blob_a, ID3DBlob* blob_b, ID3DBlob** out_merged_blob)
    {
        if (!blob_a || !blob_b || !out_merged_blob)
        {
            return E_INVALIDARG;
        }

        ID3D12RootSignatureDeserializer* deserializer_a = NULL;
        ID3D12RootSignatureDeserializer* deserializer_b = NULL;

        HRESULT hr = D3D12CreateRootSignatureDeserializer(blob_a->GetBufferPointer(), blob_a->GetBufferSize(), IID_PPV_ARGS(&deserializer_a));
        if (FAILED(hr))
        {
            return hr;
        }

        hr = D3D12CreateRootSignatureDeserializer(blob_b->GetBufferPointer(), blob_b->GetBufferSize(), IID_PPV_ARGS(&deserializer_b));
        if (FAILED(hr))
        {
            deserializer_a->Release();
            return hr;
        }

        const D3D12_ROOT_SIGNATURE_DESC* desc_a = deserializer_a->GetRootSignatureDesc();
        const D3D12_ROOT_SIGNATURE_DESC* desc_b = deserializer_b->GetRootSignatureDesc();

        // Allocate combined arrays
        UINT total_params = desc_a->NumParameters + desc_b->NumParameters;
        UINT total_samplers = desc_a->NumStaticSamplers + desc_b->NumStaticSamplers;

        D3D12_ROOT_PARAMETER* root_params = (D3D12_ROOT_PARAMETER*) calloc(total_params, sizeof(D3D12_ROOT_PARAMETER));
        D3D12_STATIC_SAMPLER_DESC* static_samplers = NULL;
        
        if (total_samplers > 0)
        {
            static_samplers = (D3D12_STATIC_SAMPLER_DESC*) calloc(total_samplers, sizeof(D3D12_STATIC_SAMPLER_DESC));
        }

        // Copy root parameters
        memcpy(root_params, desc_a->pParameters, sizeof(D3D12_ROOT_PARAMETER) * desc_a->NumParameters);
        memcpy(root_params + desc_a->NumParameters, desc_b->pParameters, sizeof(D3D12_ROOT_PARAMETER) * desc_b->NumParameters);

        // Copy static samplers if any
        if (static_samplers)
        {
            memcpy(static_samplers, desc_a->pStaticSamplers, sizeof(D3D12_STATIC_SAMPLER_DESC) * desc_a->NumStaticSamplers);
            memcpy(static_samplers + desc_a->NumStaticSamplers, desc_b->pStaticSamplers, sizeof(D3D12_STATIC_SAMPLER_DESC) * desc_b->NumStaticSamplers);
        }

        D3D12_ROOT_SIGNATURE_DESC merged_desc;
        merged_desc.NumParameters     = total_params;
        merged_desc.pParameters       = root_params;
        merged_desc.NumStaticSamplers = total_samplers;
        merged_desc.pStaticSamplers   = static_samplers;
        merged_desc.Flags             = desc_a->Flags | desc_b->Flags | D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

        ID3DBlob* error_blob = NULL;
        hr = D3D12SerializeRootSignature(&merged_desc, D3D_ROOT_SIGNATURE_VERSION_1, out_merged_blob, &error_blob);
        if (FAILED(hr))
        {
            if (error_blob)
            {
                dmLogError("%s", error_blob->GetBufferPointer());
                error_blob->Release();
            }
        }

        // Cleanup
        deserializer_a->Release();
        deserializer_b->Release();
        free(root_params);
        if (static_samplers)
        {
            free(static_samplers);
        }

        return hr;
    }

    static void CreateRootSignatureResourceBindings(DX12ShaderProgram* program, ShaderDesc::Shader** shaders, uint32_t shader_count)
    {
        uint32_t num_bindings = 0;

        for (int i = 0; i < shader_count; ++i)
        {
            num_bindings += shaders[i]->m_HlslResourceMapping.m_Count;
        }

        program->m_RootSignatureResources.SetCapacity(num_bindings);
        program->m_RootSignatureResources.SetSize(num_bindings);

        uint32_t offset = 0;

        for (int i = 0; i < shader_count; ++i)
        {
            for (int j = 0; j < shaders[i]->m_HlslResourceMapping.m_Count; ++j)
            {
                uint32_t index = offset + j;
                program->m_RootSignatureResources[index].m_NameHash = shaders[i]->m_HlslResourceMapping[j].m_NameHash;
                program->m_RootSignatureResources[index].m_Binding  = shaders[i]->m_HlslResourceMapping[j].m_Binding;
                program->m_RootSignatureResources[index].m_Set      = shaders[i]->m_HlslResourceMapping[j].m_Set;

                if (program->m_RootSignatureResources[index].m_NameHash == HASH_SPIRV_Cross_NumWorkgroups)
                {
                    program->m_NumWorkGroupsResourceIndex = index;
                }
            }

            offset += shaders[i]->m_HlslResourceMapping.m_Count;
        }
    }

    static HProgram DX12NewProgram(HContext _context, ShaderDesc* ddf, char* error_buffer, uint32_t error_buffer_size)
    {
        ShaderDesc::Shader* ddf_vp = 0x0;
        ShaderDesc::Shader* ddf_fp = 0x0;
        ShaderDesc::Shader* ddf_cp = 0x0;

        if (!GetShaderProgram(_context, ddf, &ddf_vp, &ddf_fp, &ddf_cp))
        {
            return 0;
        }

        DX12Context* context = (DX12Context*) _context;
        DX12ShaderProgram* program = new DX12ShaderProgram();
        program->m_NumWorkGroupsResourceIndex = 0xff; // 0xff == unused

        CreateShaderMeta(&ddf->m_Reflection, &program->m_BaseProgram.m_ShaderMeta);

        HashState64 program_hash;
        dmHashInit64(&program_hash, false);

        if (ddf_cp)
        {
            program->m_ComputeModule = new DX12ShaderModule();
            memset(program->m_ComputeModule, 0, sizeof(DX12ShaderModule));

            HRESULT hr = CreateShaderModule(context, "cs_5_1", ddf_cp->m_Source.m_Data, ddf_cp->m_Source.m_Count, program->m_ComputeModule);
            CHECK_HR_ERROR(hr);

            CreateProgramResourceBindings(program, NULL, NULL, program->m_ComputeModule);

            CreateRootSignatureResourceBindings(program, &ddf_cp, 1);

            // Extract the embedded root signature blob
            hr = context->m_Device->CreateRootSignature(0, program->m_ComputeModule->m_ShaderBlob->GetBufferPointer(), program->m_ComputeModule->m_ShaderBlob->GetBufferSize(), IID_PPV_ARGS(&program->m_RootSignature));
            CHECK_HR_ERROR(hr);

            // Create the hash
            dmHashUpdateBuffer64(&program_hash, &program->m_ComputeModule->m_Hash, sizeof(program->m_ComputeModule->m_Hash));
        }
        else
        {
            program->m_VertexModule = new DX12ShaderModule;
            program->m_FragmentModule = new DX12ShaderModule;
            memset(program->m_VertexModule, 0, sizeof(DX12ShaderModule));
            memset(program->m_FragmentModule, 0, sizeof(DX12ShaderModule));

            HRESULT hr = CreateShaderModule(context, "vs_5_1", ddf_vp->m_Source.m_Data, ddf_vp->m_Source.m_Count, program->m_VertexModule);
            CHECK_HR_ERROR(hr);

            hr = CreateShaderModule(context, "ps_5_1", ddf_fp->m_Source.m_Data, ddf_fp->m_Source.m_Count, program->m_FragmentModule);
            CHECK_HR_ERROR(hr);

            CreateProgramResourceBindings(program, program->m_VertexModule, program->m_FragmentModule, NULL);

            ShaderDesc::Shader* shaders[] = { ddf_vp, ddf_fp };
            CreateRootSignatureResourceBindings(program, shaders, 2);

            ID3DBlob* merged_signature_blob = 0;

            // We can only use a single root signature, so we have to concat them (TODO: can we do this offline?)
            hr = ConcatenateRootSignatures(program->m_VertexModule->m_RootSignatureBlob, program->m_FragmentModule->m_RootSignatureBlob, &merged_signature_blob);
            CHECK_HR_ERROR(hr);

            hr = context->m_Device->CreateRootSignature(0, merged_signature_blob->GetBufferPointer(), merged_signature_blob->GetBufferSize(), IID_PPV_ARGS(&program->m_RootSignature));
            CHECK_HR_ERROR(hr);

            // Create the hash
            for (uint32_t i=0; i < program->m_BaseProgram.m_ShaderMeta.m_Inputs.Size(); i++)
            {
                dmHashUpdateBuffer64(&program_hash, &program->m_BaseProgram.m_ShaderMeta.m_Inputs[i].m_Binding, sizeof(program->m_BaseProgram.m_ShaderMeta.m_Inputs[i].m_Binding));
            }

            dmHashUpdateBuffer64(&program_hash, &program->m_VertexModule->m_Hash, sizeof(program->m_VertexModule->m_Hash));
            dmHashUpdateBuffer64(&program_hash, &program->m_FragmentModule->m_Hash, sizeof(program->m_FragmentModule->m_Hash));
        }

        program->m_Hash = dmHashFinal64(&program_hash);

        return (HProgram) program;
    }

    static void DX12DeleteProgram(HContext context, HProgram program)
    {
    }

    static bool DX12IsShaderLanguageSupported(HContext context, ShaderDesc::Language language, ShaderDesc::ShaderType shader_type)
    {
        return language == ShaderDesc::LANGUAGE_HLSL_51 || language == ShaderDesc::LANGUAGE_HLSL_50;
    }

    static ShaderDesc::Language DX12GetProgramLanguage(HProgram program)
    {
        return ShaderDesc::LANGUAGE_HLSL_51;
    }

    static void DX12EnableProgram(HContext context, HProgram program)
    {
        ((DX12Context*) context)->m_CurrentProgram = (DX12ShaderProgram*) program;
    }

    static void DX12DisableProgram(HContext context)
    {
        ((DX12Context*) context)->m_CurrentProgram = 0;
    }

    static bool DX12ReloadProgram(HContext _context, HProgram _program, ShaderDesc* ddf)
    {
        return true;
    }

    static uint32_t DX12GetAttributeCount(HProgram prog)
    {
        DX12ShaderProgram* program = (DX12ShaderProgram*) prog;
        uint32_t num_vx_inputs = 0;
        for (int i = 0; i < program->m_BaseProgram.m_ShaderMeta.m_Inputs.Size(); ++i)
        {
            if (program->m_BaseProgram.m_ShaderMeta.m_Inputs[i].m_StageFlags & SHADER_STAGE_FLAG_VERTEX)
            {
                num_vx_inputs++;
            }
        }
        return num_vx_inputs;
    }

    static void DX12GetAttribute(HProgram prog, uint32_t index, dmhash_t* name_hash, Type* type, uint32_t* element_count, uint32_t* num_values, int32_t* location)
    {
        DX12ShaderProgram* program = (DX12ShaderProgram*) prog;
        uint32_t input_ix = 0;
        for (int i = 0; i < program->m_BaseProgram.m_ShaderMeta.m_Inputs.Size(); ++i)
        {
            if (program->m_BaseProgram.m_ShaderMeta.m_Inputs[i].m_StageFlags & SHADER_STAGE_FLAG_VERTEX)
            {
                if (input_ix == index)
                {
                    ShaderResourceBinding& attr = program->m_BaseProgram.m_ShaderMeta.m_Inputs[i];
                    *name_hash                  = attr.m_NameHash;
                    *type                       = ShaderDataTypeToGraphicsType(attr.m_Type.m_ShaderType);
                    *num_values                 = 1;
                    *location                   = attr.m_Binding;
                    *element_count              = GetShaderTypeSize(attr.m_Type.m_ShaderType) / sizeof(float);
                }
                input_ix++;
            }
        }
    }

    static void DX12SetConstantV4(HContext _context, const dmVMath::Vector4* data, int count, HUniformLocation base_location)
    {
        DX12Context* context = (DX12Context*) _context;
        assert(context->m_CurrentProgram);
        assert(base_location != INVALID_UNIFORM_LOCATION);

        DX12ShaderProgram* program = (DX12ShaderProgram*) context->m_CurrentProgram;
        uint32_t set               = UNIFORM_LOCATION_GET_OP0(base_location);
        uint32_t binding           = UNIFORM_LOCATION_GET_OP1(base_location);
        uint32_t buffer_offset     = UNIFORM_LOCATION_GET_OP2(base_location);
        assert(!(set == UNIFORM_LOCATION_MAX && binding == UNIFORM_LOCATION_MAX));

        ProgramResourceBinding& pgm_res = program->m_BaseProgram.m_ResourceBindings[set][binding];

        uint32_t offset = pgm_res.m_UniformBufferOffset + buffer_offset;
        WriteConstantData(offset, program->m_UniformData, (uint8_t*) data, sizeof(dmVMath::Vector4) * count);
    }

    static void DX12SetConstantM4(HContext _context, const dmVMath::Vector4* data, int count, HUniformLocation base_location)
    {
        DX12Context* context = (DX12Context*) _context;
        assert(context->m_CurrentProgram);
        assert(base_location != INVALID_UNIFORM_LOCATION);

        DX12ShaderProgram* program = (DX12ShaderProgram*) context->m_CurrentProgram;
        uint32_t set            = UNIFORM_LOCATION_GET_OP0(base_location);
        uint32_t binding        = UNIFORM_LOCATION_GET_OP1(base_location);
        uint32_t buffer_offset  = UNIFORM_LOCATION_GET_OP2(base_location);
        assert(!(set == UNIFORM_LOCATION_MAX && binding == UNIFORM_LOCATION_MAX));

        ProgramResourceBinding& pgm_res = program->m_BaseProgram.m_ResourceBindings[set][binding];

        uint32_t offset = pgm_res.m_UniformBufferOffset + buffer_offset;
        WriteConstantData(offset, program->m_UniformData, (uint8_t*) data, sizeof(dmVMath::Vector4) * 4 * count);
     }

    static void DX12SetSampler(HContext _context, HUniformLocation location, int32_t unit)
    {
        DX12Context* context = (DX12Context*) _context;
        assert(context->m_CurrentProgram);
        assert(location != INVALID_UNIFORM_LOCATION);

        DX12ShaderProgram* program = (DX12ShaderProgram*) context->m_CurrentProgram;
        uint32_t set         = UNIFORM_LOCATION_GET_OP0(location);
        uint32_t binding     = UNIFORM_LOCATION_GET_OP1(location);
        assert(!(set == UNIFORM_LOCATION_MAX && binding == UNIFORM_LOCATION_MAX));

        assert(program->m_BaseProgram.m_ResourceBindings[set][binding].m_Res);
        program->m_BaseProgram.m_ResourceBindings[set][binding].m_TextureUnit = unit;
    }

    static HRenderTarget DX12NewRenderTarget(HContext _context, uint32_t buffer_type_flags, const RenderTargetCreationParams params)
    {
        DX12Context* context = (DX12Context*) _context;
        DX12RenderTarget* rt = new DX12RenderTarget();
        rt->m_Id             = GetNextRenderTargetId();

        memcpy(rt->m_ColorTextureParams, params.m_ColorBufferParams, sizeof(TextureParams) * MAX_BUFFER_COLOR_ATTACHMENTS);

        const BufferType color_buffer_flags[] = {
            BUFFER_TYPE_COLOR0_BIT,
            BUFFER_TYPE_COLOR1_BIT,
            BUFFER_TYPE_COLOR2_BIT,
            BUFFER_TYPE_COLOR3_BIT,
        };

        DX12Texture* color_attachments[MAX_BUFFER_COLOR_ATTACHMENTS] = {};
        DX12Texture* depth_stencil_attachment = 0;

        HRESULT hr;

        uint32_t color_attachment_count = 0;
        for (int i = 0; i < MAX_BUFFER_COLOR_ATTACHMENTS; ++i)
        {
            const BufferType buffer_type = color_buffer_flags[i];
            if (buffer_type_flags & buffer_type)
            {
                TextureParams& color_buffer_params = rt->m_ColorTextureParams[i];
                HTexture new_texture_color_handle  = NewTexture(_context, params.m_ColorBufferCreationParams[i]);
                DX12Texture* new_texture_color     = GetAssetFromContainer<DX12Texture>(context->m_AssetHandleContainer, new_texture_color_handle);

                color_attachments[color_attachment_count] = new_texture_color;
                rt->m_TextureColor[color_attachment_count] = new_texture_color_handle;

                if (color_buffer_params.m_Format == TEXTURE_FORMAT_RGB)
                {
                    color_buffer_params.m_Format = TEXTURE_FORMAT_RGBA;
                }

                DXGI_FORMAT dxgi_format = GetDXGIFormatFromTextureFormat(color_buffer_params.m_Format);

                D3D12_RESOURCE_DESC texture_desc = {};
                texture_desc.Dimension           = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
                texture_desc.Width               = color_buffer_params.m_Width;
                texture_desc.Height              = color_buffer_params.m_Height;
                texture_desc.DepthOrArraySize    = 1;
                texture_desc.MipLevels           = 1;
                texture_desc.Format              = dxgi_format;
                texture_desc.SampleDesc.Count    = 1; // No MSAA
                texture_desc.SampleDesc.Quality  = 0;
                texture_desc.Layout              = D3D12_TEXTURE_LAYOUT_UNKNOWN;
                texture_desc.Flags               = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

                D3D12_CLEAR_VALUE clear_value = {};
                clear_value.Format            = dxgi_format;

                hr = context->m_Device->CreateCommittedResource(
                    &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
                    D3D12_HEAP_FLAG_NONE,
                    &texture_desc,
                    D3D12_RESOURCE_STATE_COMMON,
                    &clear_value,
                    IID_PPV_ARGS(&new_texture_color->m_Resource)
                );
                CHECK_HR_ERROR(hr);

                // Initial state (no mipmaps)
                new_texture_color->m_ResourceStates[0] = D3D12_RESOURCE_STATE_COMMON;

                color_attachment_count++;
            }
        }

        uint8_t has_depth         = buffer_type_flags & dmGraphics::BUFFER_TYPE_DEPTH_BIT;
        uint8_t has_stencil       = buffer_type_flags & dmGraphics::BUFFER_TYPE_STENCIL_BIT;
        uint8_t has_depth_stencil = has_depth || has_stencil;

        // Create attachments
        if (color_attachment_count)
        {
            // this heap is a render target view heap
            D3D12_DESCRIPTOR_HEAP_DESC rt_view_heap_desc = {};
            rt_view_heap_desc.NumDescriptors             = color_attachment_count;
            rt_view_heap_desc.Type                       = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
            rt_view_heap_desc.Flags                      = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

            hr = context->m_Device->CreateDescriptorHeap(&rt_view_heap_desc, IID_PPV_ARGS(&rt->m_ColorAttachmentDescriptorHeap));
            CHECK_HR_ERROR(hr);

            D3D12_CPU_DESCRIPTOR_HANDLE rtv_handle = rt->m_ColorAttachmentDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
            UINT rtv_descriptor_size               = context->m_Device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

            for (int i = 0; i < color_attachment_count; ++i)
            {
                context->m_Device->CreateRenderTargetView(color_attachments[i]->m_Resource, NULL, rtv_handle);
                rtv_handle.ptr += rtv_descriptor_size; // Move to the next descriptor
            }
        }

        if (has_depth_stencil)
        {

            const TextureCreationParams& stencil_depth_create_params = has_depth ? params.m_DepthBufferCreationParams : params.m_StencilBufferCreationParams;

            HTexture texture_depth_stencil         = NewTexture(_context, stencil_depth_create_params);
            DX12Texture* texture_depth_stencil_ptr = GetAssetFromContainer<DX12Texture>(context->m_AssetHandleContainer, texture_depth_stencil);

            DXGI_FORMAT ds_format = DXGI_FORMAT_D24_UNORM_S8_UINT;

            if (has_depth && !has_stencil)
            {
                ds_format = DXGI_FORMAT_D32_FLOAT;
            }

            D3D12_RESOURCE_DESC ds_desc = {};
            ds_desc.Dimension           = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
            ds_desc.Width               = stencil_depth_create_params.m_Width;
            ds_desc.Height              = stencil_depth_create_params.m_Height;
            ds_desc.DepthOrArraySize    = 1;
            ds_desc.MipLevels           = 1;
            ds_desc.Format              = ds_format;
            ds_desc.SampleDesc.Count    = 1;
            ds_desc.SampleDesc.Quality  = 0;
            ds_desc.Layout              = D3D12_TEXTURE_LAYOUT_UNKNOWN;
            ds_desc.Flags               = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

            D3D12_CLEAR_VALUE clear_value    = {};
            clear_value.Format               = ds_format;
            clear_value.DepthStencil.Depth   = 1.0f;
            clear_value.DepthStencil.Stencil = 0;

            hr = context->m_Device->CreateCommittedResource(
                &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
                D3D12_HEAP_FLAG_NONE,
                &ds_desc,
                D3D12_RESOURCE_STATE_DEPTH_WRITE, // Initial state
                &clear_value,
                IID_PPV_ARGS(&texture_depth_stencil_ptr->m_Resource)
            );
            CHECK_HR_ERROR(hr);

            // Create DSV descriptor heap
            D3D12_DESCRIPTOR_HEAP_DESC dsv_heap_desc = {};
            dsv_heap_desc.NumDescriptors             = 1;
            dsv_heap_desc.Type                       = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
            dsv_heap_desc.Flags                      = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

            hr = context->m_Device->CreateDescriptorHeap(&dsv_heap_desc, IID_PPV_ARGS(&rt->m_DepthStencilDescriptorHeap));
            CHECK_HR_ERROR(hr);

            D3D12_DEPTH_STENCIL_VIEW_DESC dsv_desc = {};
            dsv_desc.Format              = ds_format;
            dsv_desc.ViewDimension       = D3D12_DSV_DIMENSION_TEXTURE2D;
            dsv_desc.Flags               = D3D12_DSV_FLAG_NONE;

            context->m_Device->CreateDepthStencilView(texture_depth_stencil_ptr->m_Resource, &dsv_desc, rt->m_DepthStencilDescriptorHeap->GetCPUDescriptorHandleForHeapStart());
        }

        return StoreAssetInContainer(context->m_AssetHandleContainer, rt, ASSET_TYPE_RENDER_TARGET);
    }

    static void DX12DeleteRenderTarget(HContext context, HRenderTarget render_target)
    {
    }

    static void DX12SetRenderTarget(HContext _context, HRenderTarget render_target, uint32_t transient_buffer_types)
    {
        (void) transient_buffer_types;
        DX12Context* context = (DX12Context*) _context;
        context->m_ViewportChanged = 1;
        BeginRenderPass(context, render_target != 0x0 ? render_target : context->m_MainRenderTarget);
    }

    static HTexture DX12GetRenderTargetTexture(HContext context, HRenderTarget render_target, BufferType buffer_type)
    {
        DX12RenderTarget* rt = GetAssetFromContainer<DX12RenderTarget>(g_DX12Context->m_AssetHandleContainer, render_target);

        if (IsColorBufferType(buffer_type))
        {
            return rt->m_TextureColor[GetBufferTypeIndex(buffer_type)];
        }
        else if (buffer_type == BUFFER_TYPE_DEPTH_BIT || buffer_type == BUFFER_TYPE_STENCIL_BIT)
        {
            return rt->m_TextureDepthStencil;
        }
        return 0;
    }

    static void DX12GetRenderTargetSize(HContext context, HRenderTarget render_target, BufferType buffer_type, uint32_t& width, uint32_t& height)
    {
        DX12RenderTarget* rt = GetAssetFromContainer<DX12RenderTarget>(g_DX12Context->m_AssetHandleContainer, render_target);
        TextureParams* params = 0;

        if (IsColorBufferType(buffer_type))
        {
            uint32_t i = GetBufferTypeIndex(buffer_type);
            assert(i < MAX_BUFFER_COLOR_ATTACHMENTS);
            params = &rt->m_ColorTextureParams[i];
        }
        else if (buffer_type == BUFFER_TYPE_DEPTH_BIT || buffer_type == BUFFER_TYPE_STENCIL_BIT)
        {
            params = &rt->m_DepthStencilTextureParams;
        }
        else
        {
            assert(0);
        }

        width  = params->m_Width;
        height = params->m_Height;
    }

    static void DX12SetRenderTargetSize(HContext context, HRenderTarget render_target, uint32_t width, uint32_t height)
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
        DX12Context* context = (DX12Context*) _context;
        DX12Texture* tex = new DX12Texture;
        memset(tex, 0, sizeof(DX12Texture));

        tex->m_Type        = params.m_Type;
        tex->m_Width       = params.m_Width;
        tex->m_Height      = params.m_Height;
        tex->m_Depth       = dmMath::Max((uint16_t)1, params.m_Depth);
        tex->m_LayerCount  = dmMath::Max((uint16_t)1, (uint16_t) params.m_LayerCount);
        tex->m_MipMapCount = params.m_MipMapCount;
        tex->m_PageCount   = params.m_LayerCount;

        // tex->m_UsageFlags  = GetVulkanUsageFromHints(params.m_UsageHintBits);

        if (params.m_OriginalWidth == 0)
        {
            tex->m_OriginalWidth  = params.m_Width;
            tex->m_OriginalHeight = params.m_Height;
            tex->m_OriginalDepth  = params.m_Depth;
        }
        else
        {
            tex->m_OriginalWidth  = params.m_OriginalWidth;
            tex->m_OriginalHeight = params.m_OriginalHeight;
            tex->m_OriginalDepth  = params.m_OriginalDepth;
        }
        return StoreAssetInContainer(context->m_AssetHandleContainer, tex, ASSET_TYPE_TEXTURE);
    }

    static void DX12DeleteTexture(HContext context, HTexture texture)
    {
    }

    static HandleResult DX12GetTextureHandle(HTexture texture, void** out_handle)
    {

        return HANDLE_RESULT_OK;
    }

    static int16_t GetTextureSamplerIndex(DX12Context* context, TextureFilter minfilter, TextureFilter magfilter, TextureWrap uwrap, TextureWrap vwrap, uint8_t maxLod, float max_anisotropy)
    {
        if (minfilter == TEXTURE_FILTER_DEFAULT)
            minfilter = context->m_DefaultTextureMinFilter;
        if (magfilter == TEXTURE_FILTER_DEFAULT)
            magfilter = context->m_DefaultTextureMagFilter;

        for (uint32_t i=0; i < context->m_TextureSamplers.Size(); i++)
        {
            const DX12TextureSampler& sampler = context->m_TextureSamplers[i];
            if (sampler.m_MagFilter     == magfilter &&
                sampler.m_MinFilter     == minfilter &&
                sampler.m_AddressModeU  == uwrap     &&
                sampler.m_AddressModeV  == vwrap     &&
                sampler.m_MaxLod        == maxLod    &&
                sampler.m_MaxAnisotropy == max_anisotropy)
            {
                return (uint8_t) i;
            }
        }

        return -1;
    }

    static inline float GetMaxAnisotrophyClamped(float max_anisotropy_requested)
    {
        return dmMath::Min(max_anisotropy_requested, 32.0f); // TODO: What's the max limit here?
    }

    static inline D3D12_FILTER GetSamplerFilter(TextureFilter minfilter, TextureFilter magfilter)
    {
        if (magfilter == TEXTURE_FILTER_NEAREST)
        {
            if (minfilter == TEXTURE_FILTER_NEAREST || minfilter == TEXTURE_FILTER_NEAREST_MIPMAP_NEAREST)
            {
                return D3D12_FILTER_MIN_MAG_MIP_POINT;
            }
            else if (minfilter == TEXTURE_FILTER_LINEAR)
            {
                return D3D12_FILTER_MIN_LINEAR_MAG_MIP_POINT;
            }
            else if (minfilter == TEXTURE_FILTER_LINEAR_MIPMAP_NEAREST)
            {
                return D3D12_FILTER_MIN_LINEAR_MAG_MIP_POINT;
            }
            else if (minfilter == TEXTURE_FILTER_LINEAR_MIPMAP_LINEAR)
            {
                return D3D12_FILTER_MIN_LINEAR_MAG_POINT_MIP_LINEAR;
            }
        }
        else if (magfilter == TEXTURE_FILTER_LINEAR)
        {
            if (minfilter == TEXTURE_FILTER_NEAREST || minfilter == TEXTURE_FILTER_NEAREST_MIPMAP_NEAREST)
            {
                return D3D12_FILTER_MIN_POINT_MAG_LINEAR_MIP_POINT;
            }
            else if (minfilter == TEXTURE_FILTER_LINEAR || minfilter == TEXTURE_FILTER_LINEAR_MIPMAP_NEAREST)
            {
                return D3D12_FILTER_MIN_MAG_LINEAR_MIP_POINT;
            }
            else if (minfilter == TEXTURE_FILTER_LINEAR_MIPMAP_LINEAR)
            {
                return D3D12_FILTER_MIN_MAG_MIP_LINEAR;
            }
        }

        assert(0);
        return D3D12_FILTER_MIN_MAG_MIP_POINT;
    }

    static inline D3D12_TEXTURE_ADDRESS_MODE GetAddressMode(TextureWrap mode)
    {
        switch(mode)
        {
            case TEXTURE_WRAP_CLAMP_TO_BORDER:  return D3D12_TEXTURE_ADDRESS_MODE_BORDER;
            case TEXTURE_WRAP_CLAMP_TO_EDGE:    return D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
            case TEXTURE_WRAP_MIRRORED_REPEAT:  return D3D12_TEXTURE_ADDRESS_MODE_MIRROR;
            case TEXTURE_WRAP_REPEAT:           return D3D12_TEXTURE_ADDRESS_MODE_WRAP;
            default:break;
        }
        assert(0);
        return D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    }

    static int16_t CreateTextureSampler(DX12Context* context, TextureFilter minfilter, TextureFilter magfilter, TextureWrap uwrap, TextureWrap vwrap, uint8_t maxLod, float max_anisotropy)
    {
        DX12TextureSampler new_sampler  = {};
        new_sampler.m_MinFilter     = minfilter;
        new_sampler.m_MagFilter     = magfilter;
        new_sampler.m_AddressModeU  = uwrap;
        new_sampler.m_AddressModeV  = vwrap;
        new_sampler.m_MaxLod        = maxLod;
        new_sampler.m_MaxAnisotropy = max_anisotropy;

        uint32_t sampler_index = context->m_TextureSamplers.Size();
        if (context->m_TextureSamplers.Full())
        {
            context->m_TextureSamplers.OffsetCapacity(1);
        }

        D3D12_SAMPLER_DESC desc  = {};
        desc.Filter              = GetSamplerFilter(minfilter, magfilter);
        desc.AddressU            = GetAddressMode(uwrap);
        desc.AddressV            = GetAddressMode(vwrap);
        desc.AddressW            = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
        desc.MinLOD              = 0;
        desc.MaxLOD              = D3D12_FLOAT32_MAX;
        desc.MipLODBias          = 0.0f;
        desc.MaxAnisotropy       = max_anisotropy;
        desc.ComparisonFunc      = D3D12_COMPARISON_FUNC_ALWAYS;

        CD3DX12_CPU_DESCRIPTOR_HANDLE  desc_handle(context->m_SamplerPool.m_DescriptorHeap->GetCPUDescriptorHandleForHeapStart(), sampler_index, context->m_Device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER));
        context->m_Device->CreateSampler(&desc, desc_handle);
        context->m_SamplerPool.m_DescriptorCursor++;

        new_sampler.m_DescriptorOffset = sampler_index * context->m_Device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER);

        context->m_TextureSamplers.Push(new_sampler);
        return (int16_t) sampler_index;
    }

    static void DX12SetTextureParamsInternal(DX12Context* context, DX12Texture* texture, TextureFilter minfilter, TextureFilter magfilter, TextureWrap uwrap, TextureWrap vwrap, float max_anisotropy)
    {
        const DX12TextureSampler& sampler = context->m_TextureSamplers[texture->m_TextureSamplerIndex];
        float anisotropy_clamped = GetMaxAnisotrophyClamped(max_anisotropy);

        if (sampler.m_MinFilter     != minfilter              ||
            sampler.m_MagFilter     != magfilter              ||
            sampler.m_AddressModeU  != uwrap                  ||
            sampler.m_AddressModeV  != vwrap                  ||
            sampler.m_MaxLod        != texture->m_MipMapCount ||
            sampler.m_MaxAnisotropy != anisotropy_clamped)
        {
            int16_t sampler_index = GetTextureSamplerIndex(context, minfilter, magfilter, uwrap, vwrap, texture->m_MipMapCount, anisotropy_clamped);
            if (sampler_index < 0)
            {
                sampler_index = CreateTextureSampler(context, minfilter, magfilter, uwrap, vwrap, texture->m_MipMapCount, anisotropy_clamped);
            }
            texture->m_TextureSamplerIndex = sampler_index;
        }
    }

    static void DX12SetTextureParams(HContext context, HTexture texture, TextureFilter minfilter, TextureFilter magfilter, TextureWrap uwrap, TextureWrap vwrap, float max_anisotropy)
    {
        DX12Texture* tex = GetAssetFromContainer<DX12Texture>(g_DX12Context->m_AssetHandleContainer, texture);
        DX12SetTextureParamsInternal(g_DX12Context, tex, minfilter, magfilter, uwrap, vwrap, max_anisotropy);
    }

    static void DX12SetTexture(HContext context, HTexture texture, const TextureParams& params)
    {
        // Same as graphics_opengl.cpp
        switch (params.m_Format)
        {
            case TEXTURE_FORMAT_DEPTH:
            case TEXTURE_FORMAT_STENCIL:
                dmLogError("Unable to upload texture data, unsupported type (%s).", TextureFormatToString(params.m_Format));
                return;
            default:break;
        }

        DX12Texture* tex             = GetAssetFromContainer<DX12Texture>(g_DX12Context->m_AssetHandleContainer, texture);
        TextureFormat format_orig    = params.m_Format;
        TextureFormat format_actual  = params.m_Format;
        void* tex_data_ptr           = (void*) params.m_Data;
        uint32_t depth_or_array_size = dmMath::Max(1U, dmMath::Max((uint32_t) params.m_Depth, (uint32_t) params.m_LayerCount));
        uint32_t tex_data_size       = params.m_DataSize;
        DXGI_FORMAT dxgi_format      = GetDXGIFormatFromTextureFormat(format_orig);

        if (tex->m_MipMapCount == 1 && params.m_MipMap > 0)
        {
            return;
        }

        tex->m_MipMapCount = dmMath::Max(tex->m_MipMapCount, (uint16_t)(params.m_MipMap+1));

        // Note: There's no 8 bit RGB format, we have to expand this to four channels
        // TODO: Can we use R11G11B10 somehow?
        if (format_orig == TEXTURE_FORMAT_RGB)
        {
            format_actual = TEXTURE_FORMAT_RGBA;
            dxgi_format   = GetDXGIFormatFromTextureFormat(format_actual);

            uint32_t data_pixel_count = params.m_Width * params.m_Height * depth_or_array_size;
            uint8_t bpp_new           = 4;
            uint8_t* data_new         = new uint8_t[data_pixel_count * bpp_new];

            RepackRGBToRGBA(data_pixel_count, (uint8_t*) tex_data_ptr, data_new);
            tex_data_ptr  = data_new;
        }

        if (!tex->m_Resource)
        {
            D3D12_RESOURCE_DESC desc = {};
            desc.Format              = dxgi_format;
            desc.Width               = params.m_Width;
            desc.Height              = params.m_Height;
            desc.Flags               = D3D12_RESOURCE_FLAG_NONE;
            desc.DepthOrArraySize    = depth_or_array_size;
            desc.MipLevels           = tex->m_MipMapCount;
            desc.SampleDesc.Count    = 1;
            desc.SampleDesc.Quality  = 0;
            desc.Dimension           = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
            desc.Layout              = D3D12_TEXTURE_LAYOUT_UNKNOWN;
            desc.Alignment           = 0;

            CD3DX12_HEAP_PROPERTIES heap(D3D12_HEAP_TYPE_DEFAULT);

            HRESULT hr = g_DX12Context->m_Device->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &desc, D3D12_RESOURCE_STATE_COPY_DEST, NULL, IID_PPV_ARGS(&tex->m_Resource));
            CHECK_HR_ERROR(hr);

            for (int i = 0; i < tex->m_MipMapCount; ++i)
            {
                tex->m_ResourceStates[i] = D3D12_RESOURCE_STATE_COPY_DEST;
            }

            tex->m_ResourceDesc = desc;
        }

        TextureBufferUploadHelper(g_DX12Context, tex, format_actual, format_actual, params, (uint8_t*) tex_data_ptr);

        DX12SetTextureParamsInternal(g_DX12Context, tex, params.m_MinFilter, params.m_MagFilter, params.m_UWrap, params.m_VWrap, 1.0f);

        if (format_orig == TEXTURE_FORMAT_RGB)
        {
            delete[] (uint8_t*)tex_data_ptr;
        }
    }

    static uint32_t DX12GetTextureResourceSize(HContext context, HTexture texture)
    {
        return 0;
    }

    static uint16_t DX12GetTextureWidth(HContext context, HTexture texture)
    {
        return GetAssetFromContainer<DX12Texture>(g_DX12Context->m_AssetHandleContainer, texture)->m_Width;
    }

    static uint16_t DX12GetTextureHeight(HContext context, HTexture texture)
    {
        return GetAssetFromContainer<DX12Texture>(g_DX12Context->m_AssetHandleContainer, texture)->m_Height;
    }

    static uint16_t DX12GetOriginalTextureWidth(HContext context, HTexture texture)
    {
        return GetAssetFromContainer<DX12Texture>(g_DX12Context->m_AssetHandleContainer, texture)->m_OriginalWidth;
    }

    static uint16_t DX12GetOriginalTextureHeight(HContext context, HTexture texture)
    {
        return GetAssetFromContainer<DX12Texture>(g_DX12Context->m_AssetHandleContainer, texture)->m_OriginalHeight;
    }

    static void DX12EnableTexture(HContext _context, uint32_t unit, uint8_t value_index, HTexture texture)
    {
        assert(unit < DM_MAX_TEXTURE_UNITS);
        g_DX12Context->m_CurrentTextures[unit] = texture;
    }

    static void DX12DisableTexture(HContext context, uint32_t unit, HTexture texture)
    {
        assert(unit < DM_MAX_TEXTURE_UNITS);
        g_DX12Context->m_CurrentTextures[unit] = 0x0;
    }

    static void DX12ReadPixels(HContext context, int32_t x, int32_t y, uint32_t width, uint32_t height, void* buffer, uint32_t buffer_size)
    {
    }

    static void DX12SetViewport(HContext _context, int32_t x, int32_t y, int32_t width, int32_t height)
    {
        DX12Context* context = (DX12Context*) _context;

        DX12Viewport& viewport = context->m_CurrentViewport;
        viewport.m_X = (uint16_t) x;
        viewport.m_Y = (uint16_t) y;
        viewport.m_W = (uint16_t) width;
        viewport.m_H = (uint16_t) height;
        context->m_ViewportChanged = 1;
    }

    static void DX12GetViewport(HContext context, int32_t* x, int32_t* y, uint32_t* width, uint32_t* height)
    {
        DX12Context* _context = (DX12Context*) context;

        const DX12Viewport& viewport = _context->m_CurrentViewport;
        *x = viewport.m_X, *y = viewport.m_Y, *width = viewport.m_W, *height = viewport.m_H;
    }

    static void DX12EnableState(HContext context, State state)
    {
        SetPipelineStateValue(g_DX12Context->m_PipelineState, state, 1);
    }

    static void DX12DisableState(HContext context, State state)
    {
        SetPipelineStateValue(g_DX12Context->m_PipelineState, state, 0);
    }

    static void DX12SetBlendFunc(HContext _context, BlendFactor source_factor, BlendFactor destinaton_factor)
    {
        g_DX12Context->m_PipelineState.m_BlendSrcFactor = source_factor;
        g_DX12Context->m_PipelineState.m_BlendDstFactor = destinaton_factor;
    }

    static void DX12SetColorMask(HContext context, bool red, bool green, bool blue, bool alpha)
    {
        assert(context);
        uint8_t write_mask = red   ? DM_GRAPHICS_STATE_WRITE_R : 0;
        write_mask        |= green ? DM_GRAPHICS_STATE_WRITE_G : 0;
        write_mask        |= blue  ? DM_GRAPHICS_STATE_WRITE_B : 0;
        write_mask        |= alpha ? DM_GRAPHICS_STATE_WRITE_A : 0;

        g_DX12Context->m_PipelineState.m_WriteColorMask = write_mask;
    }

    static void DX12SetDepthMask(HContext context, bool enable_mask)
    {
        g_DX12Context->m_PipelineState.m_WriteDepth = enable_mask;
    }

    static void DX12SetDepthFunc(HContext context, CompareFunc func)
    {
        g_DX12Context->m_PipelineState.m_DepthTestFunc = func;
    }

    static void DX12SetScissor(HContext _context, int32_t x, int32_t y, int32_t width, int32_t height)
    {
    }

    static void DX12SetStencilMask(HContext context, uint32_t mask)
    {
        g_DX12Context->m_PipelineState.m_StencilWriteMask = mask;
    }

    static void DX12SetStencilFunc(HContext _context, CompareFunc func, uint32_t ref, uint32_t mask)
    {
        g_DX12Context->m_PipelineState.m_StencilFrontTestFunc = (uint8_t) func;
        g_DX12Context->m_PipelineState.m_StencilBackTestFunc  = (uint8_t) func;
        g_DX12Context->m_PipelineState.m_StencilReference     = (uint8_t) ref;
        g_DX12Context->m_PipelineState.m_StencilCompareMask   = (uint8_t) mask;
    }

    static void DX12SetStencilOp(HContext _context, StencilOp sfail, StencilOp dpfail, StencilOp dppass)
    {
        g_DX12Context->m_PipelineState.m_StencilFrontOpFail      = sfail;
        g_DX12Context->m_PipelineState.m_StencilFrontOpDepthFail = dpfail;
        g_DX12Context->m_PipelineState.m_StencilFrontOpPass      = dppass;
        g_DX12Context->m_PipelineState.m_StencilBackOpFail       = sfail;
        g_DX12Context->m_PipelineState.m_StencilBackOpDepthFail  = dpfail;
        g_DX12Context->m_PipelineState.m_StencilBackOpPass       = dppass;
    }

    static void DX12SetStencilFuncSeparate(HContext _context, FaceType face_type, CompareFunc func, uint32_t ref, uint32_t mask)
    {
        if (face_type == FACE_TYPE_BACK)
        {
            g_DX12Context->m_PipelineState.m_StencilBackTestFunc  = (uint8_t) func;
        }
        else
        {
            g_DX12Context->m_PipelineState.m_StencilFrontTestFunc = (uint8_t) func;
        }
        g_DX12Context->m_PipelineState.m_StencilReference     = (uint8_t) ref;
        g_DX12Context->m_PipelineState.m_StencilCompareMask   = (uint8_t) mask;
    }

    static void DX12SetStencilOpSeparate(HContext _context, FaceType face_type, StencilOp sfail, StencilOp dpfail, StencilOp dppass)
    {
        if (face_type == FACE_TYPE_BACK)
        {
            g_DX12Context->m_PipelineState.m_StencilBackOpFail       = sfail;
            g_DX12Context->m_PipelineState.m_StencilBackOpDepthFail  = dpfail;
            g_DX12Context->m_PipelineState.m_StencilBackOpPass       = dppass;
        }
        else
        {
            g_DX12Context->m_PipelineState.m_StencilFrontOpFail      = sfail;
            g_DX12Context->m_PipelineState.m_StencilFrontOpDepthFail = dpfail;
            g_DX12Context->m_PipelineState.m_StencilFrontOpPass      = dppass;
        }
    }

    static void DX12SetFaceWinding(HContext context, FaceWinding face_winding)
    {
        // TODO: Add this to the DX12 pipeline handle aswell, for now it's a NOP
    }

    static void DX12SetCullFace(HContext context, FaceType face_type)
    {
        g_DX12Context->m_PipelineState.m_CullFaceType = face_type;
        g_DX12Context->m_CullFaceChanged              = true;
    }

    static void DX12SetPolygonOffset(HContext context, float factor, float units)
    {
        // TODO: Add this to the DX12 pipeline handle aswell, for now it's a NOP
    }

    static PipelineState DX12GetPipelineState(HContext context)
    {
        return ((DX12Context*) context)->m_PipelineState;
    }

    static void DX12SetTextureAsync(HContext context, HTexture texture, const TextureParams& params, SetTextureAsyncCallback callback, void* user_data)
    {
        SetTexture(context, texture, params);
        if (callback)
        {
            callback(texture, user_data);
        }
    }

    static uint32_t DX12GetTextureStatusFlags(HContext context, HTexture texture)
    {
        return TEXTURE_STATUS_OK;
    }

    static bool DX12IsExtensionSupported(HContext context, const char* extension)
    {
        return true;
    }

    static TextureType DX12GetTextureType(HContext context, HTexture texture)
    {
        return GetAssetFromContainer<DX12Texture>(g_DX12Context->m_AssetHandleContainer, texture)->m_Type;
    }

    static uint32_t DX12GetTextureUsageHintFlags(HContext context, HTexture texture)
    {
        return 0;
        // return GetAssetFromContainer<DX12Texture>(g_DX12Context->m_AssetHandleContainer, texture)->m_UsageHintFlags;
    }

    static uint8_t DX12GetTexturePageCount(HTexture texture)
    {
        // TODO: mutex is missed?
        // ScopedLock lock(g_DX12Context->m_AssetHandleContainerMutex);
        DX12Texture* tex = GetAssetFromContainer<DX12Texture>(g_DX12Context->m_AssetHandleContainer, texture);
        return tex ? tex->m_PageCount : 0;

    }

    static uint32_t DX12GetNumSupportedExtensions(HContext context)
    {
        return 0;
    }

    static const char* DX12GetSupportedExtension(HContext context, uint32_t index)
    {
        return "";
    }

    static uint8_t DX12GetNumTextureHandles(HContext context, HTexture texture)
    {
        return 1;
    }

    static bool DX12IsContextFeatureSupported(HContext context, ContextFeature feature)
    {
        return true;
    }

    static uint16_t DX12GetTextureDepth(HContext context, HTexture texture)
    {
        return GetAssetFromContainer<DX12Texture>(g_DX12Context->m_AssetHandleContainer, texture)->m_Depth;
    }

    static uint8_t DX12GetTextureMipmapCount(HContext context, HTexture texture)
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

    static void DX12InvalidateGraphicsHandles(HContext context)
    {
        // NOP
    }

    static GraphicsAdapterFunctionTable DX12RegisterFunctionTable()
    {
        GraphicsAdapterFunctionTable fn_table = {};
        DM_REGISTER_GRAPHICS_FUNCTION_TABLE(fn_table, DX12);
        return fn_table;
    }
}

