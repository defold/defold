// Copyright 2020-2026 The Defold Foundation
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

#define JC_TEST_IMPLEMENTATION
#include <jc_test/jc_test.h>
#include <vector>
#include <string>
#include <dlib/time.h>
#include <wrl/client.h>
#include "graphics_adapter.h"
#include "test_app_graphics.h"
#include "dx12/graphics_dx12_private.h"

using Microsoft::WRL::ComPtr;
using namespace dmGraphics;
namespace dmGraphics
{
    extern DX12Context* g_DX12Context;
}
extern "C" void GraphicsAdapterDX12();

static void     RequireHR(HRESULT hr)
{
    ASSERT_EQ(S_OK, hr);
}

class DX12Test : public jc_test_base_class
{
    public:
    DX12Context*                   m_Context;
    ComPtr<ID3D12InfoQueue>        m_Info;
    ComPtr<ID3D12CommandAllocator> m_Allocator;
    ComPtr<ID3D12Fence>            m_Fence;
    uint64_t                       m_FenceValue;
    std::vector<HTexture>          m_Textures;

    HContext                       Context()
    {
        return (HContext)m_Context;
    }
    DX12FrameResource& Frame()
    {
        return m_Context->m_FrameResources[0];
    }

    void SetUp() override
    {
        ComPtr<ID3D12Debug> debug;
        RequireHR(D3D12GetDebugInterface(IID_PPV_ARGS(&debug)));
        debug->EnableDebugLayer();
        ComPtr<IDXGIFactory4> factory;
        RequireHR(CreateDXGIFactory1(IID_PPV_ARGS(&factory)));
        ComPtr<IDXGIAdapter> warp;
        RequireHR(factory->EnumWarpAdapter(IID_PPV_ARGS(&warp)));
        m_Context = new DX12Context();
        g_DX12Context = m_Context;
        RequireHR(D3D12CreateDevice(warp.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&m_Context->m_Device)));
        RequireHR(m_Context->m_Device->QueryInterface(IID_PPV_ARGS(&m_Info)));
        D3D12_MESSAGE_ID        clear_warnings[] = { D3D12_MESSAGE_ID_CLEARRENDERTARGETVIEW_MISMATCHINGCLEARVALUE,
                                                     D3D12_MESSAGE_ID_CLEARDEPTHSTENCILVIEW_MISMATCHINGCLEARVALUE };
        D3D12_INFO_QUEUE_FILTER filter = {};
        filter.DenyList.NumIDs = DM_ARRAY_SIZE(clear_warnings);
        filter.DenyList.pIDList = clear_warnings;
        RequireHR(m_Info->PushStorageFilter(&filter));
        D3D12_COMMAND_QUEUE_DESC queue = {};
        RequireHR(m_Context->m_Device->CreateCommandQueue(&queue, IID_PPV_ARGS(&m_Context->m_CommandQueue)));
        RequireHR(m_Context->m_Device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&m_Allocator)));
        RequireHR(m_Context->m_Device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, m_Allocator.Get(), 0, IID_PPV_ARGS(&m_Context->m_CommandList)));
        RequireHR(m_Context->m_Device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_Fence)));
        m_FenceValue = 0;
        Frame().m_CommandAllocator = m_Allocator.Get();
        m_Context->m_FrameBegun = 1;
        m_Context->m_BaseContext.m_DefaultTextureMinFilter = TEXTURE_FILTER_NEAREST;
        m_Context->m_BaseContext.m_DefaultTextureMagFilter = TEXTURE_FILTER_NEAREST;
        m_Context->m_PipelineState = GetDefaultPipelineState();
        Frame().m_ScratchBuffer.Initialize(m_Context, 0);
        // Deliberately small to exercise the uniform fallback as well as ring allocations.
        Frame().m_UploadRing.Initialize(m_Context, 256 * 300);
        CreateTextureSampler(m_Context, TEXTURE_FILTER_NEAREST, TEXTURE_FILTER_NEAREST, TEXTURE_WRAP_CLAMP_TO_EDGE, TEXTURE_WRAP_CLAMP_TO_EDGE, TEXTURE_WRAP_CLAMP_TO_EDGE, 1, 1);
    }

    void Submit()
    {
        RequireHR(m_Context->m_CommandList->Close());
        ID3D12CommandList* lists[] = { m_Context->m_CommandList };
        m_Context->m_CommandQueue->ExecuteCommandLists(1, lists);
        RequireHR(m_Context->m_CommandQueue->Signal(m_Fence.Get(), ++m_FenceValue));
        HANDLE event = CreateEvent(0, FALSE, FALSE, 0);
        RequireHR(m_Fence->SetEventOnCompletion(m_FenceValue, event));
        ASSERT_EQ(WAIT_OBJECT_0, WaitForSingleObject(event, 10000));
        CloseHandle(event);
        RequireHR(m_Allocator->Reset());
        RequireHR(m_Context->m_CommandList->Reset(m_Allocator.Get(), 0));
        RenderTarget* target = GetAssetFromContainer<RenderTarget>(m_Context->m_BaseContext.m_AssetHandleContainer, m_Context->m_CurrentRenderTarget);
        if (target)
            target->m_IsBound = 0;
        m_Context->m_ViewportChanged = 1;
    }

    void TearDown() override
    {
        Submit();
        for (HTexture texture : m_Textures)
            DeleteTexture(Context(), texture);
        FlushResourcesToDestroy(Frame());
        Frame().m_ScratchBuffer.Destroy();
        Frame().m_UploadRing.Destroy();
        m_Context->m_PipelineCache.Iterate<void>(+[](void*, const uint64_t*, DX12Pipeline* pipeline) { (*pipeline)->Release(); }, 0);
        m_Context->m_CommandList->Close();
        m_Context->m_CommandList->Release();
        m_Context->m_CommandQueue->Release();
        m_Allocator.Reset();
        m_Fence.Reset();
        for (UINT64 i = 0; i < m_Info->GetNumStoredMessages(); ++i)
        {
            SIZE_T size = 0;
            m_Info->GetMessage(i, 0, &size);
            std::vector<uint8_t> buffer(size);
            D3D12_MESSAGE*       message = (D3D12_MESSAGE*)buffer.data();
            m_Info->GetMessage(i, message, &size);
            if (message->Severity <= D3D12_MESSAGE_SEVERITY_WARNING)
            {
                printf("DX12 validation: %s\n", message->pDescription);
                ASSERT_GT((int)message->Severity, (int)D3D12_MESSAGE_SEVERITY_WARNING);
            }
        }
        m_Info.Reset();
        m_Context->m_Device->Release();
        delete m_Context;
        g_DX12Context = 0;
    }

    HTexture Texture(TextureType type, uint16_t width, uint16_t height, uint16_t layers, uint8_t mips)
    {
        TextureCreationParams params;
        params.m_Type = type;
        params.m_Width = width;
        params.m_Height = height;
        params.m_LayerCount = layers > 255 ? 1 : layers;
        params.m_Depth = layers > 255 || type == TEXTURE_TYPE_3D || type == TEXTURE_TYPE_IMAGE_3D ? layers : 1;
        if (type == TEXTURE_TYPE_3D || type == TEXTURE_TYPE_IMAGE_3D)
            params.m_LayerCount = 1;
        params.m_MipMapCount = mips;
        HTexture texture = NewTexture(Context(), params);
        m_Textures.push_back(texture);
        return texture;
    }

    DX12Texture* TextureData(HTexture texture)
    {
        return GetAssetFromContainer<DX12Texture>(m_Context->m_BaseContext.m_AssetHandleContainer, texture);
    }

    HRenderTarget Target(uint32_t width, uint32_t height, uint32_t flags = BUFFER_TYPE_COLOR0_BIT, uint32_t samples = 1)
    {
        RenderTargetCreationParams params;
        params.m_SampleCount = samples;
        for (uint32_t i = 0; i < MAX_BUFFER_COLOR_ATTACHMENTS; ++i)
        {
            params.m_ColorBufferCreationParams[i].m_Width = params.m_ColorBufferParams[i].m_Width = width;
            params.m_ColorBufferCreationParams[i].m_Height = params.m_ColorBufferParams[i].m_Height = height;
            params.m_ColorBufferParams[i].m_Format = TEXTURE_FORMAT_RGBA;
        }
        params.m_DepthBufferCreationParams.m_Width = params.m_DepthBufferParams.m_Width = width;
        params.m_DepthBufferCreationParams.m_Height = params.m_DepthBufferParams.m_Height = height;
        params.m_DepthBufferParams.m_Format = TEXTURE_FORMAT_DEPTH;
        return NewRenderTarget(Context(), flags, params);
    }

    std::vector<uint8_t> ReadTexture(HTexture texture, uint32_t mip, uint32_t layer)
    {
        DX12Texture*                       tex = TextureData(texture);
        const uint32_t                     subresource = D3D12CalcSubresource(mip, layer, 0, tex->m_ResourceDesc.MipLevels, tex->m_ResourceDesc.DepthOrArraySize);
        D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint;
        UINT                               rows;
        UINT64                             row_bytes, size;
        m_Context->m_Device->GetCopyableFootprints(&tex->m_ResourceDesc, subresource, 1, 0, &footprint, &rows, &row_bytes, &size);
        ComPtr<ID3D12Resource> readback;
        RequireHR(m_Context->m_Device->CreateCommittedResource(&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_READBACK), D3D12_HEAP_FLAG_NONE, &CD3DX12_RESOURCE_DESC::Buffer(size), D3D12_RESOURCE_STATE_COPY_DEST, 0, IID_PPV_ARGS(&readback)));
        TransitionTexture(m_Context->m_CommandList, tex, D3D12_RESOURCE_STATE_COPY_SOURCE, subresource, 1);
        CD3DX12_TEXTURE_COPY_LOCATION src(tex->m_Resource, subresource), dst(readback.Get(), footprint);
        m_Context->m_CommandList->CopyTextureRegion(&dst, 0, 0, 0, &src, 0);
        Submit();
        uint8_t* mapped;
        RequireHR(readback->Map(0, 0, (void**)&mapped));
        std::vector<uint8_t> bytes((size_t)row_bytes * rows * footprint.Footprint.Depth);
        for (UINT row = 0; row < rows * footprint.Footprint.Depth; ++row)
            memcpy(bytes.data() + row * row_bytes, mapped + footprint.Offset + row * footprint.Footprint.RowPitch, (size_t)row_bytes);
        readback->Unmap(0, 0);
        return bytes;
    }
};

TEST_F(DX12Test, ArrayUploadsAndPartialRGBUpdates)
{
    HTexture texture = Texture(TEXTURE_TYPE_2D_ARRAY, 8, 8, 20, 3);
    for (uint32_t mip = 0; mip < 3; ++mip)
    {
        const uint32_t       width = 8 >> mip;
        std::vector<uint8_t> pixels(width * width * 3 * 20);
        for (uint32_t layer = 0; layer < 20; ++layer)
            memset(pixels.data() + layer * width * width * 3, 10 + layer + mip * 30, width * width * 3);
        TextureParams params;
        params.m_Format = TEXTURE_FORMAT_RGB;
        params.m_Data = pixels.data();
        params.m_DataSize = width * width * 3;
        params.m_Width = params.m_Height = width;
        params.m_LayerCount = 20;
        params.m_MipMap = mip;
        SetTexture(Context(), texture, params);
    }
    ASSERT_EQ(60u, TextureData(texture)->m_ResourceStates.Size());
    // A source containing only two 2x2 slices must not be expanded as a full 20-layer texture.
    uint8_t patch[2 * 2 * 3 * 2];
    memset(patch, 201, sizeof(patch));
    TextureParams params;
    params.m_Format = TEXTURE_FORMAT_RGB;
    params.m_SubUpdate = 1;
    params.m_MipMap = 1;
    params.m_Slice = 17;
    params.m_LayerCount = 2;
    params.m_X = params.m_Y = 1;
    params.m_Width = params.m_Height = 2;
    params.m_Data = patch;
    params.m_DataSize = sizeof(patch) / 2;
    SetTexture(Context(), texture, params);
    for (uint32_t mip = 0; mip < 3; ++mip)
        for (uint32_t layer = 0; layer < 20; ++layer)
        {
            const uint32_t       width = 8 >> mip;
            std::vector<uint8_t> bytes = ReadTexture(texture, mip, layer);
            for (uint32_t y = 0; y < width; ++y)
                for (uint32_t x = 0; x < width; ++x)
                {
                    uint8_t expected = mip == 1 && layer >= 17 && layer <= 18 && x >= 1 && x < 3 && y >= 1 && y < 3 ? 201 : 10 + layer + mip * 30;
                    ASSERT_EQ(expected, bytes[(y * width + x) * 4]);
                    ASSERT_EQ(255, bytes[(y * width + x) * 4 + 3]);
                }
        }
    TransitionTexture(m_Context->m_CommandList, TextureData(texture), (D3D12_RESOURCE_STATES)(D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE));
}

TEST_F(DX12Test, MaximumArrayLayersAndCubemapMips)
{
    HTexture             array = Texture(TEXTURE_TYPE_2D_ARRAY, 1, 1, 2048, 1);
    std::vector<uint8_t> pixels(2048 * 4, 77);
    TextureParams        params;
    params.m_Format = TEXTURE_FORMAT_RGBA;
    params.m_Data = pixels.data();
    params.m_DataSize = 4;
    params.m_Width = params.m_Height = 1;
    SetTexture(Context(), array, params);
    ASSERT_EQ(2048u, TextureData(array)->m_ResourceStates.Size());
    ASSERT_EQ(77, ReadTexture(array, 0, 2047)[0]);
    HTexture cube = Texture(TEXTURE_TYPE_CUBE_MAP, 8, 8, 1, 4);
    for (uint32_t mip = 0; mip < 4; ++mip)
    {
        const uint32_t width = 8 >> mip;
        pixels.assign(width * width * 4 * 6, 30 + mip);
        params.m_Data = pixels.data();
        params.m_DataSize = width * width * 4;
        params.m_Width = params.m_Height = width;
        params.m_MipMap = mip;
        SetTexture(Context(), cube, params);
    }
    ASSERT_EQ(24u, TextureData(cube)->m_ResourceStates.Size());
    ASSERT_EQ(33, ReadTexture(cube, 3, 5)[0]);
}

TEST_F(DX12Test, CompressedArraySubUpdatePreservesOtherBlocks)
{
    HTexture texture = Texture(TEXTURE_TYPE_2D_ARRAY, 16, 16, 19, 2);
    for (uint32_t mip = 0; mip < 2; ++mip)
    {
        const uint32_t       width = 16 >> mip;
        const uint32_t       slice_size = (width / 4) * (width / 4) * 8;
        std::vector<uint8_t> pixels(slice_size * 19);
        for (uint32_t layer = 0; layer < 19; ++layer)
            memset(pixels.data() + layer * slice_size, layer + mip * 30, slice_size);
        TextureParams params;
        params.m_Format = TEXTURE_FORMAT_RGB_BC1;
        params.m_Data = pixels.data();
        params.m_DataSize = slice_size;
        params.m_Width = params.m_Height = width;
        params.m_LayerCount = 19;
        params.m_MipMap = mip;
        SetTexture(Context(), texture, params);
    }
    uint8_t block[8];
    memset(block, 201, sizeof(block));
    TextureParams params;
    params.m_Format = TEXTURE_FORMAT_RGB_BC1;
    params.m_SubUpdate = 1;
    params.m_MipMap = 1;
    params.m_Slice = 18;
    params.m_LayerCount = 1;
    params.m_X = params.m_Y = 4;
    params.m_Width = params.m_Height = 4;
    params.m_Data = block;
    params.m_DataSize = sizeof(block);
    SetTexture(Context(), texture, params);
    std::vector<uint8_t> bytes = ReadTexture(texture, 1, 18);
    ASSERT_EQ(32u, bytes.size());
    for (uint32_t i = 0; i < bytes.size(); ++i)
        ASSERT_EQ(i >= 24 ? 201 : 48, bytes[i]);
    ASSERT_EQ(47, ReadTexture(texture, 1, 17)[0]);
    ASSERT_EQ(18, ReadTexture(texture, 0, 18)[0]);
}

TEST_F(DX12Test, StencilStateAndSeparateFaces)
{
    SetStencilMask(Context(), 0x53);
    SetStencilFuncSeparate(Context(), FACE_TYPE_FRONT, COMPARE_FUNC_EQUAL, 0x37, 0x71);
    SetStencilFuncSeparate(Context(), FACE_TYPE_BACK, COMPARE_FUNC_NOTEQUAL, 0x37, 0x71);
    SetStencilOpSeparate(Context(), FACE_TYPE_FRONT, STENCIL_OP_INCR, STENCIL_OP_DECR_WRAP, STENCIL_OP_REPLACE);
    SetStencilOpSeparate(Context(), FACE_TYPE_BACK, STENCIL_OP_INCR_WRAP, STENCIL_OP_DECR, STENCIL_OP_INVERT);
    EnableState(Context(), STATE_STENCIL_TEST);
    D3D12_DEPTH_STENCIL_DESC desc = GetDepthStencilState(m_Context->m_PipelineState);
    ASSERT_TRUE(desc.StencilEnable);
    ASSERT_EQ(0x53, desc.StencilWriteMask);
    ASSERT_EQ(0x71, desc.StencilReadMask);
    ASSERT_EQ(D3D12_COMPARISON_FUNC_EQUAL, desc.FrontFace.StencilFunc);
    ASSERT_EQ(D3D12_COMPARISON_FUNC_NOT_EQUAL, desc.BackFace.StencilFunc);
    ASSERT_EQ(D3D12_STENCIL_OP_INCR_SAT, desc.FrontFace.StencilFailOp);
    ASSERT_EQ(D3D12_STENCIL_OP_DECR, desc.FrontFace.StencilDepthFailOp);
    ASSERT_EQ(D3D12_STENCIL_OP_REPLACE, desc.FrontFace.StencilPassOp);
    ASSERT_EQ(D3D12_STENCIL_OP_INCR, desc.BackFace.StencilFailOp);
    ASSERT_EQ(D3D12_STENCIL_OP_DECR_SAT, desc.BackFace.StencilDepthFailOp);
    ASSERT_EQ(D3D12_STENCIL_OP_INVERT, desc.BackFace.StencilPassOp);
    SetStencilFuncSeparate(Context(), FACE_TYPE_FRONT_AND_BACK, COMPARE_FUNC_ALWAYS, 0, 0xff);
    SetStencilOpSeparate(Context(), FACE_TYPE_FRONT_AND_BACK, STENCIL_OP_KEEP, STENCIL_OP_ZERO, STENCIL_OP_INVERT);
    desc = GetDepthStencilState(m_Context->m_PipelineState);
    ASSERT_EQ(0, memcmp(&desc.FrontFace, &desc.BackFace, sizeof(desc.FrontFace)));
}

TEST_F(DX12Test, DescriptorPagesAndUniformOverflowPreserveDrawData)
{
    const uint32_t count = 600;
    HTexture       texture = Texture(TEXTURE_TYPE_2D, 1, 1, 1, 1);
    uint8_t        red[] = { 255, 0, 0, 255 };
    TextureParams  tex_params;
    tex_params.m_Format = TEXTURE_FORMAT_RGBA;
    tex_params.m_Data = red;
    tex_params.m_DataSize = sizeof(red);
    tex_params.m_Width = tex_params.m_Height = 1;
    SetTexture(Context(), texture, tex_params);
    TransitionTexture(m_Context->m_CommandList, TextureData(texture), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);

    const char* source =
    "Texture2D<float4> tex:register(t0); SamplerState smp:register(s0);"
    "cbuffer Values:register(b0){uint index;float value;} RWStructuredBuffer<float4> output:register(u0);"
    "[numthreads(1,1,1)] void main(){output[index]=tex.SampleLevel(smp,float2(0.5,0.5),0)+value;}";
    ComPtr<ID3DBlob> shader, errors, signature;
    RequireHR(D3DCompile(source, strlen(source), 0, 0, 0, "main", "cs_5_1", 0, 0, &shader, &errors));
    D3D12_DESCRIPTOR_RANGE ranges[2] = {};
    ranges[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    ranges[0].NumDescriptors = 1;
    ranges[1].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER;
    ranges[1].NumDescriptors = 1;
    D3D12_ROOT_PARAMETER roots[4] = {};
    for (uint32_t i = 0; i < 2; ++i)
    {
        roots[i].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
        roots[i].DescriptorTable.NumDescriptorRanges = 1;
        roots[i].DescriptorTable.pDescriptorRanges = &ranges[i];
    }
    roots[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    roots[3].ParameterType = D3D12_ROOT_PARAMETER_TYPE_UAV;
    D3D12_ROOT_SIGNATURE_DESC desc = {};
    desc.NumParameters = 4;
    desc.pParameters = roots;
    RequireHR(D3D12SerializeRootSignature(&desc, D3D_ROOT_SIGNATURE_VERSION_1, &signature, &errors));
    ComPtr<ID3D12RootSignature> root;
    RequireHR(m_Context->m_Device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&root)));
    D3D12_COMPUTE_PIPELINE_STATE_DESC pso_desc = {};
    pso_desc.pRootSignature = root.Get();
    pso_desc.CS = { shader->GetBufferPointer(), shader->GetBufferSize() };
    ComPtr<ID3D12PipelineState> pso;
    RequireHR(m_Context->m_Device->CreateComputePipelineState(&pso_desc, IID_PPV_ARGS(&pso)));
    ComPtr<ID3D12Resource> output, readback;
    RequireHR(m_Context->m_Device->CreateCommittedResource(&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT), D3D12_HEAP_FLAG_NONE, &CD3DX12_RESOURCE_DESC::Buffer(count * 16, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS), D3D12_RESOURCE_STATE_COMMON, 0, IID_PPV_ARGS(&output)));
    RequireHR(m_Context->m_Device->CreateCommittedResource(&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_READBACK), D3D12_HEAP_FLAG_NONE, &CD3DX12_RESOURCE_DESC::Buffer(count * 16), D3D12_RESOURCE_STATE_COPY_DEST, 0, IID_PPV_ARGS(&readback)));

    D3D12_RESOURCE_BARRIER initial_barrier = CD3DX12_RESOURCE_BARRIER::Transition(output.Get(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    m_Context->m_CommandList->ResourceBarrier(1, &initial_barrier);
    for (uint32_t frame = 0; frame < 2; ++frame)
    {
        m_Context->m_CommandList->SetComputeRootSignature(root.Get());
        m_Context->m_CommandList->SetPipelineState(pso.Get());
        m_Context->m_CommandList->SetComputeRootUnorderedAccessView(3, output->GetGPUVirtualAddress());
        for (uint32_t i = 0; i < count; ++i)
        {
            ASSERT_TRUE(Frame().m_ScratchBuffer.Prepare(m_Context, 1, 1));
            Frame().m_ScratchBuffer.AllocateTexture2D(m_Context, PIPELINE_TYPE_COMPUTE, TextureData(texture), 0);
            Frame().m_ScratchBuffer.AllocateSampler(m_Context, PIPELINE_TYPE_COMPUTE, m_Context->m_TextureSamplers[0], 1);
            struct Constants
            {
                uint32_t index;
                float    value;
            } constants = { i, (float)(i + frame) };
            void* data = Frame().m_ScratchBuffer.AllocateConstantBuffer(m_Context, PIPELINE_TYPE_COMPUTE, 2, sizeof(constants));
            ASSERT_NE((void*)0, data);
            memcpy(data, &constants, sizeof(constants));
            m_Context->m_CommandList->Dispatch(1, 1, 1);
        }
        ASSERT_EQ(3u, Frame().m_ScratchBuffer.m_DescriptorPools.Size());
        ASSERT_GT(Frame().m_ResourcesToDestroy.Size(), 0u);
        D3D12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(output.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COPY_SOURCE);
        m_Context->m_CommandList->ResourceBarrier(1, &barrier);
        m_Context->m_CommandList->CopyResource(readback.Get(), output.Get());
        Submit();
        float* values;
        RequireHR(readback->Map(0, 0, (void**)&values));
        for (uint32_t i = 0; i < count; ++i)
        {
            ASSERT_EQ((float)(i + frame + 1), values[i * 4]);
            ASSERT_EQ((float)(i + frame), values[i * 4 + 1]);
        }
        readback->Unmap(0, 0);
        // Reuse only after Submit has waited for the fence. No page allocations on the second frame.
        FlushResourcesToDestroy(Frame());
        Frame().m_ScratchBuffer.Reset(m_Context);
        Frame().m_UploadRing.Reset();
        if (frame == 0)
        {
            barrier = CD3DX12_RESOURCE_BARRIER::Transition(output.Get(), D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
            m_Context->m_CommandList->ResourceBarrier(1, &barrier);
        }
    }
    // CPU sampler caching must not write beyond a fixed heap or truncate indices at 1024.
    for (uint32_t i = 1; i <= 1100; ++i)
        CreateTextureSampler(m_Context, TEXTURE_FILTER_NEAREST, TEXTURE_FILTER_NEAREST, TEXTURE_WRAP_CLAMP_TO_EDGE, TEXTURE_WRAP_CLAMP_TO_EDGE, TEXTURE_WRAP_CLAMP_TO_EDGE, 1, 1.0f + i / 128.0f);
    SetTextureParams(Context(), texture, TEXTURE_FILTER_NEAREST, TEXTURE_FILTER_NEAREST, TEXTURE_WRAP_CLAMP_TO_EDGE, TEXTURE_WRAP_CLAMP_TO_EDGE, TEXTURE_WRAP_CLAMP_TO_EDGE, 1.0f + 1100 / 128.0f);
    ASSERT_GT(TextureData(texture)->m_TextureSamplerIndex, 1024u);
    ASSERT_EQ(m_Context->m_TextureSamplers.Size() - 1, TextureData(texture)->m_TextureSamplerIndex);
}

TEST_F(DX12Test, StencilClipsPixelsWithMaskedReference)
{
    RenderTargetCreationParams params;
    params.m_ColorBufferCreationParams[0].m_Width = params.m_ColorBufferCreationParams[0].m_Height = 4;
    params.m_DepthBufferCreationParams.m_Width = params.m_DepthBufferCreationParams.m_Height = 4;
    params.m_StencilBufferCreationParams = params.m_DepthBufferCreationParams;
    params.m_ColorBufferParams[0].m_Format = TEXTURE_FORMAT_RGBA;
    params.m_ColorBufferParams[0].m_Width = params.m_ColorBufferParams[0].m_Height = 4;
    params.m_DepthBufferParams.m_Format = TEXTURE_FORMAT_DEPTH;
    params.m_DepthBufferParams.m_Width = params.m_DepthBufferParams.m_Height = 4;
    params.m_StencilBufferParams = params.m_DepthBufferParams;
    params.m_StencilBufferParams.m_Format = TEXTURE_FORMAT_STENCIL;
    HRenderTarget target = NewRenderTarget(Context(), BUFFER_TYPE_COLOR0_BIT | BUFFER_TYPE_DEPTH_BIT | BUFFER_TYPE_STENCIL_BIT, params);
    m_Context->m_CurrentRenderTarget = target;
    m_Context->m_MainRenderTarget = target;
    SetRenderTarget(Context(), target, RenderTargetBindingParams());
    HTexture          color = GetRenderTargetTexture(Context(), target, BUFFER_TYPE_COLOR0_BIT);
    DX12RenderTarget* rt = GetAssetFromContainer<DX12RenderTarget>(m_Context->m_BaseContext.m_AssetHandleContainer, target);
    ASSERT_EQ(2u, TextureData(rt->m_Base.m_TextureDepthStencil)->m_ResourceStates.Size());
    Clear(Context(), BUFFER_TYPE_COLOR0_BIT | BUFFER_TYPE_DEPTH_BIT | BUFFER_TYPE_STENCIL_BIT, 0, 0, 0, 0, 1, 0);
    const char*      vs = "float4 main(uint id:SV_VertexID):SV_Position {float2 p[3]={float2(-1,-1),float2(-1,3),float2(3,-1)};return float4(p[id],0.5,1);}";
    const char*      ps = "float4 main():SV_Target {return float4(1,0,0,1);}";
    ComPtr<ID3DBlob> vertex, fragment, errors, signature;
    RequireHR(D3DCompile(vs, strlen(vs), 0, 0, 0, "main", "vs_5_1", 0, 0, &vertex, &errors));
    RequireHR(D3DCompile(ps, strlen(ps), 0, 0, 0, "main", "ps_5_1", 0, 0, &fragment, &errors));
    D3D12_ROOT_SIGNATURE_DESC desc = {};
    desc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
    RequireHR(D3D12SerializeRootSignature(&desc, D3D_ROOT_SIGNATURE_VERSION_1, &signature, &errors));
    ComPtr<ID3D12RootSignature> root;
    RequireHR(m_Context->m_Device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&root)));
    DX12ShaderModule vertex_module = {}, fragment_module = {};
    vertex_module.m_Data = vertex->GetBufferPointer();
    vertex_module.m_DataSize = (uint32_t)vertex->GetBufferSize();
    fragment_module.m_Data = fragment->GetBufferPointer();
    fragment_module.m_DataSize = (uint32_t)fragment->GetBufferSize();
    DX12ShaderProgram program = {};
    program.m_VertexModule = &vertex_module;
    program.m_FragmentModule = &fragment_module;
    program.m_RootSignature = root.Get();
    m_Context->m_CurrentProgram = &program;
    // Isolate stencil from the backend's offscreen viewport transform.
    D3D12_VIEWPORT viewport = { 0, 0, 4, 4, 0, 1 };
    m_Context->m_CommandList->RSSetViewports(1, &viewport);
    m_Context->m_ViewportChanged = 0;
    DisableState(Context(), STATE_DEPTH_TEST);
    DisableState(Context(), STATE_CULL_FACE);
    EnableState(Context(), STATE_STENCIL_TEST);
    SetColorMask(Context(), false, false, false, false);
    SetStencilMask(Context(), 0x0f);
    SetStencilFunc(Context(), COMPARE_FUNC_ALWAYS, 0x35, 0xff);
    SetStencilOp(Context(), STENCIL_OP_KEEP, STENCIL_OP_KEEP, STENCIL_OP_REPLACE);
    D3D12_RECT scissor = { 0, 0, 2, 4 };
    m_Context->m_CommandList->RSSetScissorRects(1, &scissor);
    Draw(Context(), PRIMITIVE_TRIANGLES, 0, 3, 1);
    SetColorMask(Context(), true, true, true, true);
    SetStencilMask(Context(), 0);
    SetStencilFunc(Context(), COMPARE_FUNC_EQUAL, 0xa5, 0x0f);
    SetStencilOp(Context(), STENCIL_OP_KEEP, STENCIL_OP_KEEP, STENCIL_OP_KEEP);
    scissor.right = 4;
    m_Context->m_CommandList->RSSetScissorRects(1, &scissor);
    Draw(Context(), PRIMITIVE_TRIANGLES, 0, 3, 1);
    std::vector<uint8_t> pixels = ReadTexture(color, 0, 0);
    for (uint32_t y = 0; y < 4; ++y)
        for (uint32_t x = 0; x < 4; ++x)
            ASSERT_EQ(x < 2 ? 255 : 0, pixels[(y * 4 + x) * 4]);
    m_Context->m_CurrentProgram = 0;
    DeleteRenderTarget(Context(), target);
}

// Build real DXBC and serialized signatures, then load via the public program API.
struct TestProgram
{
    ShaderDesc                    m_Desc = {};
    std::vector<ComPtr<ID3DBlob>> m_Blobs;
    ~TestProgram()
    {
        for (uint32_t i = 0; i < m_Desc.m_Shaders.m_Count; ++i)
            free(m_Desc.m_Shaders[i].m_HlslResourceMapping.m_Data);
        DeleteShaderDesc(&m_Desc);
    }
    void Shader(const char* source, ShaderDesc::ShaderType stage)
    {
        ComPtr<ID3DBlob> code, errors;
        HRESULT          hr = D3DCompile(source, strlen(source), 0, 0, 0, "main", stage == ShaderDesc::SHADER_TYPE_COMPUTE ? "cs_5_1" : stage == ShaderDesc::SHADER_TYPE_VERTEX ? "vs_5_1" :
                                                                                                                                                                                  "ps_5_1",
                                0,
                                0,
                                &code,
                                &errors);
        if (FAILED(hr) && errors)
            printf("%s\n", (const char*)errors->GetBufferPointer());
        RequireHR(hr);
        AddShader(&m_Desc, ShaderDesc::LANGUAGE_HLSL_51, stage, (uint8_t*)code->GetBufferPointer(), (uint32_t)code->GetBufferSize());
        m_Blobs.push_back(code);
    }
    void Root(const char* text)
    {
        ComPtr<ID3DBlob>  root, errors;
        const std::string source = std::string("#define main \"") + text + "\"\n";
        RequireHR(D3DCompile(source.c_str(), source.size(), 0, 0, 0, "main", "rootsig_1_0", 0, 0, &root, &errors));
        m_Desc.m_HlslRootSignature.m_Data = (uint8_t*)root->GetBufferPointer();
        m_Desc.m_HlslRootSignature.m_Count = (uint32_t)root->GetBufferSize();
        m_Blobs.push_back(root);
    }
    void Binding(const char* name, uint32_t binding, uint32_t root_index, ShaderDesc::ShaderDataType type, BindingType family, uint32_t shader_index = 0)
    {
        AddShaderResource(&m_Desc, name, type, binding, 0, family);
        ShaderDesc::Shader& shader = m_Desc.m_Shaders[shader_index];
        shader.m_HlslResourceMapping.m_Data = (ShaderDesc::HLSLResourceMapping*)realloc(shader.m_HlslResourceMapping.m_Data,
                                                                                        sizeof(ShaderDesc::HLSLResourceMapping) * (shader.m_HlslResourceMapping.m_Count + 1));
        ShaderDesc::HLSLResourceMapping& map = shader.m_HlslResourceMapping.m_Data[shader.m_HlslResourceMapping.m_Count++];
        memset(&map, 0, sizeof(map));
        map.m_NameHash = dmHashString64(name);
        map.m_Binding = binding;
        map.m_RootParameterIndex = root_index;
    }
    HProgram Load(HContext context)
    {
        char     error[1024] = {};
        HProgram program = NewProgram(context, &m_Desc, error, sizeof(error));
        if (!program)
            printf("Program load: %s\n", error);
        return program;
    }
};

static const char* FULLSCREEN_VS = "float4 main(uint id:SV_VertexID):SV_Position {float2 p[3]={float2(-1,-1),float2(-1,3),float2(3,-1)};return float4(p[id],0.5,1);}";

TEST_F(DX12Test, VolumeMipUploadsAndZSubUpdates)
{
    HTexture      texture = Texture(TEXTURE_TYPE_3D, 8, 8, 4, 3);
    TextureParams params;
    params.m_Format = TEXTURE_FORMAT_RGB;
    for (uint32_t mip = 0; mip < 3; ++mip)
    {
        params.m_MipMap = mip;
        params.m_Width = params.m_Height = 8 >> mip;
        params.m_Depth = dmMath::Max(1u, 4u >> mip);
        std::vector<uint8_t> data(params.m_Width * params.m_Height * params.m_Depth * 3, 20 + mip);
        params.m_Data = data.data();
        SetTexture(Context(), texture, params);
    }
    params.m_MipMap = 1;
    params.m_Width = params.m_Height = 2;
    params.m_Depth = 1;
    params.m_Z = 1;
    params.m_X = params.m_Y = 1;
    params.m_SubUpdate = 1;
    uint8_t patch[12];
    memset(patch, 99, sizeof(patch));
    params.m_Data = patch;
    SetTexture(Context(), texture, params);
    ASSERT_EQ(D3D12_RESOURCE_DIMENSION_TEXTURE3D, TextureData(texture)->m_ResourceDesc.Dimension);
    ASSERT_EQ(3u, TextureData(texture)->m_ResourceStates.Size());
    for (uint32_t mip = 0; mip < 3; ++mip)
    {
        auto     pixels = ReadTexture(texture, mip, 0);
        uint32_t w = 8 >> mip, depth = dmMath::Max(1u, 4u >> mip);
        for (uint32_t z = 0; z < depth; ++z)
            for (uint32_t y = 0; y < w; ++y)
                for (uint32_t x = 0; x < w; ++x)
                    ASSERT_EQ(mip == 1 && z == 1 && x >= 1 && x < 3 && y >= 1 && y < 3 ? 99 : 20 + mip, pixels[((z * w + y) * w + x) * 4]);
    }
    Frame().m_ScratchBuffer.Prepare(m_Context, 1, 0);
    // View creation is also exercised by the compute sampling test below.
}

TEST_F(DX12Test, MultisampleMRTClearResizeAndPublicReadback)
{
    HRenderTarget target = Target(4, 4, BUFFER_TYPE_COLOR0_BIT | BUFFER_TYPE_COLOR1_BIT | BUFFER_TYPE_DEPTH_BIT, 4);
    HRenderTarget other = Target(1, 1);
    SetRenderTarget(Context(), target, RenderTargetBindingParams());
    auto rt = GetAssetFromContainer<DX12RenderTarget>(m_Context->m_BaseContext.m_AssetHandleContainer, target);
    ASSERT_EQ(4u, rt->m_SampleDesc.Count);
    ASSERT_EQ(4u, TextureData(rt->m_Base.m_TextureDepthStencil)->m_ResourceDesc.SampleDesc.Count);
    Clear(Context(), BUFFER_TYPE_COLOR0_BIT, 255, 0, 0, 255, 1, 0);
    Clear(Context(), BUFFER_TYPE_COLOR1_BIT | BUFFER_TYPE_DEPTH_BIT, 0, 255, 0, 255, 1, 0);
    uint8_t pixels[4 * 4 * 4] = {};
    ReadPixels(Context(), 0, 0, 4, 4, pixels, sizeof(pixels));
    for (uint32_t i = 0; i < 16; ++i)
    {
        ASSERT_EQ(255, pixels[i * 4 + 2]);
        ASSERT_EQ(0, pixels[i * 4 + 1]);
    }
    SetRenderTarget(Context(), other, RenderTargetBindingParams());
    auto green = ReadTexture(rt->m_Base.m_TextureColor[1], 0, 0);
    for (uint32_t i = 0; i < 16; ++i)
        ASSERT_EQ(255, green[i * 4 + 1]);
    for (uint32_t size = 2; size < 7; ++size)
    {
        SetRenderTargetSize(Context(), target, size, size);
        SetRenderTarget(Context(), target, RenderTargetBindingParams());
        Clear(Context(), BUFFER_TYPE_COLOR0_BIT, 0, 0, 255, 255, 1, 0);
        ReadPixels(Context(), 1, 1, 1, 1, pixels, sizeof(pixels));
        ASSERT_EQ(255, pixels[0]);
        ASSERT_EQ(0, pixels[2]);
        SetRenderTarget(Context(), other, RenderTargetBindingParams());
    }
    Submit();
    DeleteRenderTarget(Context(), target);
    DeleteRenderTarget(Context(), other);
}

TEST_F(DX12Test, ShaderReloadLinesBiasAndScissor)
{
    HRenderTarget target = Target(4, 4, BUFFER_TYPE_COLOR0_BIT | BUFFER_TYPE_DEPTH_BIT);
    SetRenderTarget(Context(), target, RenderTargetBindingParams());
    SetViewport(Context(), 0, 0, 4, 4);
    DisableState(Context(), STATE_CULL_FACE);
    DisableState(Context(), STATE_DEPTH_TEST);
    TestProgram red, green;
    red.Shader(FULLSCREEN_VS, ShaderDesc::SHADER_TYPE_VERTEX);
    red.Shader("float4 main():SV_Target{return float4(1,0,0,1);}", ShaderDesc::SHADER_TYPE_FRAGMENT);
    red.Root("RootFlags(ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT)");
    green.Shader(FULLSCREEN_VS, ShaderDesc::SHADER_TYPE_VERTEX);
    green.Shader("float4 main():SV_Target{return float4(0,1,0,1);}", ShaderDesc::SHADER_TYPE_FRAGMENT);
    green.Root("RootFlags(ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT)");
    HProgram program = red.Load(Context());
    ASSERT_NE((HProgram)0, program);
    EnableProgram(Context(), program);
    Clear(Context(), BUFFER_TYPE_COLOR0_BIT, 0, 0, 0, 0, 1, 0);
    SetScissor(Context(), 0, 0, 2, 4);
    EnableState(Context(), STATE_SCISSOR_TEST);
    Draw(Context(), PRIMITIVE_TRIANGLES, 0, 3, 1);
    uint8_t pixels[64] = {};
    ReadPixels(Context(), 0, 0, 4, 4, pixels, sizeof(pixels));
    for (uint32_t i = 0; i < 16; ++i)
        ASSERT_EQ(i % 4 < 2 ? 255 : 0, pixels[i * 4 + 2]);
    char error[512] = {};
    ASSERT_TRUE(ReloadProgram(Context(), program, &green.m_Desc, error, sizeof(error)));
    // Readback breaks and restores the render pass, retaining the scissor rectangle.
    Draw(Context(), PRIMITIVE_TRIANGLES, 0, 3, 1);
    ReadPixels(Context(), 0, 0, 4, 4, pixels, sizeof(pixels));
    for (uint32_t i = 0; i < 16; ++i)
        ASSERT_EQ(i % 4 < 2 ? 255 : 0, pixels[i * 4 + 1]);
    DisableState(Context(), STATE_SCISSOR_TEST);
    Draw(Context(), PRIMITIVE_TRIANGLES, 0, 3, 1);
    ReadPixels(Context(), 0, 0, 4, 4, pixels, sizeof(pixels));
    for (uint32_t i = 0; i < 16; ++i)
        ASSERT_EQ(255, pixels[i * 4 + 1]);
    ShaderDesc invalid = {};
    ASSERT_FALSE(ReloadProgram(Context(), program, &invalid, error, sizeof(error)));
    Draw(Context(), PRIMITIVE_TRIANGLES, 0, 3, 1);
    Draw(Context(), PRIMITIVE_LINES, 0, 2, 1);
    EnableState(Context(), STATE_POLYGON_OFFSET_FILL);
    SetPolygonOffset(Context(), 2.0f, 3.0f);
    Draw(Context(), PRIMITIVE_TRIANGLES, 0, 3, 1);
    ASSERT_GE(m_Context->m_PipelineCache.Size(), 4u);
    Submit();
    DeleteProgram(Context(), program);
    DeleteRenderTarget(Context(), target);
}

TEST_F(DX12Test, DependentImageDispatchesAndComputeToDraw)
{
    HTexture      texture = Texture(TEXTURE_TYPE_IMAGE_2D, 4, 4, 1, 1);
    uint32_t      zero[16] = {};
    TextureParams params;
    params.m_Format = TEXTURE_FORMAT_R32UI;
    params.m_Width = params.m_Height = 4;
    params.m_Data = zero;
    SetTexture(Context(), texture, params);
    TestProgram compute;
    compute.Shader("RWTexture2D<uint> image:register(u0); [numthreads(4,4,1)] void main(uint3 p:SV_DispatchThreadID){image[p.xy]=image[p.xy]+1;}", ShaderDesc::SHADER_TYPE_COMPUTE);
    compute.Root("DescriptorTable(UAV(u0))");
    compute.Binding("image", 0, 0, ShaderDesc::SHADER_TYPE_UIMAGE2D, BINDING_TYPE_TEXTURE);
    HProgram cp = compute.Load(Context());
    ASSERT_NE((HProgram)0, cp);
    EnableProgram(Context(), cp);
    EnableTexture(Context(), 0, 0, texture);
    for (uint32_t i = 0; i < 3; ++i)
        DispatchCompute(Context(), 1, 1, 1);
    uint32_t changed = 8;
    params.m_Data = &changed;
    params.m_Width = params.m_Height = 1;
    params.m_SubUpdate = 1;
    SetTexture(Context(), texture, params);
    DispatchCompute(Context(), 1, 1, 1);
    TestProgram draw;
    draw.Shader(FULLSCREEN_VS, ShaderDesc::SHADER_TYPE_VERTEX);
    draw.Shader("Texture2D<uint> image:register(t0); float4 main(float4 p:SV_Position):SV_Target {uint v=image.Load(int3(p.xy,0));return float4(v/10.0,0.9,0,1);}", ShaderDesc::SHADER_TYPE_FRAGMENT);
    draw.Root("DescriptorTable(SRV(t0)), RootFlags(ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT)");
    draw.Binding("image", 0, 0, ShaderDesc::SHADER_TYPE_UTEXTURE2D, BINDING_TYPE_TEXTURE, 1);
    HProgram dp = draw.Load(Context());
    ASSERT_NE((HProgram)0, dp);
    HRenderTarget target = Target(4, 4);
    SetRenderTarget(Context(), target, RenderTargetBindingParams());
    SetViewport(Context(), 0, 0, 4, 4);
    DisableState(Context(), STATE_CULL_FACE);
    DisableState(Context(), STATE_DEPTH_TEST);
    EnableProgram(Context(), dp);
    Draw(Context(), PRIMITIVE_TRIANGLES, 0, 3, 1);
    uint8_t pixels[64] = {};
    ReadPixels(Context(), 0, 0, 4, 4, pixels, sizeof(pixels));
    for (uint32_t i = 0; i < 16; ++i)
    {
        ASSERT_NEAR(i == 0 ? 230 : 102, pixels[i * 4 + 2], 1);
        ASSERT_NEAR(230, pixels[i * 4 + 1], 1);
    }
    Submit();
    DeleteProgram(Context(), cp);
    DeleteProgram(Context(), dp);
    DeleteRenderTarget(Context(), target);
}

TEST_F(DX12Test, VolumeViewsAndWritableVolume)
{
    HTexture      input = Texture(TEXTURE_TYPE_3D, 4, 4, 4, 2);
    HTexture      output = Texture(TEXTURE_TYPE_IMAGE_3D, 2, 2, 2, 1);
    TextureParams params;
    params.m_Format = TEXTURE_FORMAT_R32UI;
    params.m_Width = params.m_Height = params.m_Depth = 4;
    uint32_t base[64] = {};
    params.m_Data = base;
    SetTexture(Context(), input, params);
    uint32_t mip[] = { 3, 3, 3, 3, 7, 7, 7, 7 };
    params.m_Data = mip;
    params.m_MipMap = 1;
    params.m_Width = params.m_Height = params.m_Depth = 2;
    SetTexture(Context(), input, params);
    params.m_MipMap = 0;
    params.m_Data = base;
    SetTexture(Context(), output, params);
    TestProgram compute;
    compute.Shader("Texture3D<uint> source:register(t0); RWTexture3D<uint> target:register(u1); [numthreads(2,2,2)] void main(uint3 p:SV_DispatchThreadID){target[p]=source.Load(int4(p,1))*2;}", ShaderDesc::SHADER_TYPE_COMPUTE);
    compute.Root("DescriptorTable(SRV(t0)), DescriptorTable(UAV(u1))");
    compute.Binding("source", 0, 0, ShaderDesc::SHADER_TYPE_UTEXTURE3D, BINDING_TYPE_TEXTURE);
    compute.Binding("target", 1, 1, ShaderDesc::SHADER_TYPE_UIMAGE3D, BINDING_TYPE_TEXTURE);
    HProgram program = compute.Load(Context());
    ASSERT_NE((HProgram)0, program);
    EnableTexture(Context(), 0, 0, input);
    EnableTexture(Context(), 1, 0, output);
    EnableProgram(Context(), program);
    for (uint32_t frame = 0; frame < 2; ++frame)
    {
        DispatchCompute(Context(), 1, 1, 1);
        auto pixels = ReadTexture(output, 0, 0);
        for (uint32_t i = 0; i < 8; ++i)
            ASSERT_EQ(i < 4 ? 6 : 14, ((uint32_t*)pixels.data())[i]);
    }
    DeleteProgram(Context(), program);
}

TEST_F(DX12Test, DepthSamplingAndCubemapTargetResize)
{
    HRenderTarget depth = Target(4, 4, BUFFER_TYPE_DEPTH_BIT);
    HRenderTarget color = Target(4, 4, BUFFER_TYPE_COLOR0_BIT, 4);
    auto          color_rt = GetAssetFromContainer<DX12RenderTarget>(m_Context->m_BaseContext.m_AssetHandleContainer, color);
    ASSERT_EQ(4u, color_rt->m_SampleDesc.Count);
    SetRenderTarget(Context(), depth, RenderTargetBindingParams());
    Clear(Context(), BUFFER_TYPE_DEPTH_BIT, 0, 0, 0, 0, 0.25f, 0);
    SetRenderTarget(Context(), color, RenderTargetBindingParams());
    TestProgram draw;
    draw.Shader(FULLSCREEN_VS, ShaderDesc::SHADER_TYPE_VERTEX);
    draw.Shader("Texture2D<float> depth:register(t0); float4 main(float4 p:SV_Position):SV_Target {return float4(depth.Load(int3(p.xy,0)),0,0,1);}", ShaderDesc::SHADER_TYPE_FRAGMENT);
    draw.Root("DescriptorTable(SRV(t0)), RootFlags(ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT)");
    draw.Binding("depth", 0, 0, ShaderDesc::SHADER_TYPE_TEXTURE2D, BINDING_TYPE_TEXTURE, 1);
    HProgram program = draw.Load(Context());
    ASSERT_NE((HProgram)0, program);
    EnableTexture(Context(), 0, 0, GetRenderTargetTexture(Context(), depth, BUFFER_TYPE_DEPTH_BIT));
    EnableProgram(Context(), program);
    SetViewport(Context(), 0, 0, 4, 4);
    DisableState(Context(), STATE_CULL_FACE);
    DisableState(Context(), STATE_DEPTH_TEST);
    Draw(Context(), PRIMITIVE_TRIANGLES, 0, 3, 1);
    uint8_t pixels[64];
    ReadPixels(Context(), 0, 0, 4, 4, pixels, sizeof(pixels));
    for (uint32_t i = 0; i < 16; ++i)
        ASSERT_NEAR(64, pixels[i * 4 + 2], 1);
    RenderTargetCreationParams params;
    params.m_TextureType = TEXTURE_TYPE_CUBE_MAP;
    params.m_ColorBufferCreationParams[0].m_Type = TEXTURE_TYPE_CUBE_MAP;
    params.m_ColorBufferCreationParams[0].m_Width = params.m_ColorBufferCreationParams[0].m_Height = 4;
    params.m_ColorBufferParams[0].m_Width = params.m_ColorBufferParams[0].m_Height = 4;
    params.m_ColorBufferParams[0].m_Format = TEXTURE_FORMAT_RGBA;
    HRenderTarget cube = NewRenderTarget(Context(), BUFFER_TYPE_COLOR0_BIT, params);
    for (uint32_t size = 4; size <= 8; size += 4)
    {
        if (size == 8)
            SetRenderTargetSize(Context(), cube, size, size);
        for (uint32_t face = 0; face < 6; ++face)
        {
            RenderTargetBindingParams binding;
            binding.m_CubeMapFace = (CubeMapFace)face;
            SetRenderTarget(Context(), cube, binding);
            Clear(Context(), BUFFER_TYPE_COLOR0_BIT, 20 + face, 0, 0, 255, 1, 0);
        }
        SetRenderTarget(Context(), color, RenderTargetBindingParams());
        for (uint32_t face = 0; face < 6; ++face)
        {
            auto bytes = ReadTexture(GetRenderTargetTexture(Context(), cube, BUFFER_TYPE_COLOR0_BIT), 0, face);
            for (uint32_t i = 0; i < size * size; ++i)
                ASSERT_EQ(20 + face, bytes[i * 4]);
        }
    }
    DeleteProgram(Context(), program);
    DeleteRenderTarget(Context(), cube);
    DeleteRenderTarget(Context(), color);
    DeleteRenderTarget(Context(), depth);
}

TEST_F(DX12Test, UploadAndPipelineBenchmark)
{
    // Opt in after correctness tests. Report CPU submission cost; this is not a GPU throughput measurement.
    if (!getenv("DEFOLD_DX12_BENCHMARK"))
        return;
    HRenderTarget target = Target(4, 4);
    SetRenderTarget(Context(), target, RenderTargetBindingParams());
    TestProgram shader;
    shader.Shader(FULLSCREEN_VS, ShaderDesc::SHADER_TYPE_VERTEX);
    shader.Shader("float4 main():SV_Target{return float4(1,0,0,1);}", ShaderDesc::SHADER_TYPE_FRAGMENT);
    shader.Root("RootFlags(ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT)");
    HProgram program = shader.Load(Context());
    EnableProgram(Context(), program);
    SetViewport(Context(), 0, 0, 4, 4);
    DisableState(Context(), STATE_DEPTH_TEST);
    DisableState(Context(), STATE_CULL_FACE);
    uint64_t start = dmTime::GetMonotonicTime();
    Draw(Context(), PRIMITIVE_TRIANGLES, 0, 3, 1);
    uint64_t       cold = dmTime::GetMonotonicTime() - start;
    float          vertices[16] = {};
    HVertexBuffer  buffer = NewVertexBuffer(Context(), sizeof(vertices), vertices, BUFFER_USAGE_DYNAMIC_DRAW);
    const uint32_t count = 1000;
    start = dmTime::GetMonotonicTime();
    for (uint32_t i = 0; i < count; ++i)
        SetVertexBufferData(buffer, sizeof(vertices), vertices, BUFFER_USAGE_DYNAMIC_DRAW);
    uint64_t uploads = dmTime::GetMonotonicTime() - start;
    start = dmTime::GetMonotonicTime();
    for (uint32_t i = 0; i < count; ++i)
        Draw(Context(), PRIMITIVE_TRIANGLES, 0, 3, 1);
    uint64_t draws = dmTime::GetMonotonicTime() - start;
    printf("DX12 WARP CPU benchmark: cold draw=%llu us, %u uploads=%llu us, %u cached draws=%llu us\n", (unsigned long long)cold, count, (unsigned long long)uploads, count, (unsigned long long)draws);
    Submit();
    DeleteVertexBuffer(buffer);
    DeleteProgram(Context(), program);
    DeleteRenderTarget(Context(), target);
}

TEST_F(DX12Test, SamplerModesAndFormatCapabilities)
{
    SetupSupportedTextureFormats(m_Context);
    ASSERT_TRUE(IsTextureFormatSupported(Context(), TEXTURE_FORMAT_RGB_BC1));
    ASSERT_FALSE(IsTextureFormatSupported(Context(), TEXTURE_FORMAT_RGBA_ASTC_4X4));
    ASSERT_FALSE(IsExtensionSupported(Context(), "invented-extension"));
    int32_t point = CreateTextureSampler(m_Context, TEXTURE_FILTER_NEAREST_MIPMAP_LINEAR, TEXTURE_FILTER_NEAREST, TEXTURE_WRAP_REPEAT, TEXTURE_WRAP_REPEAT, TEXTURE_WRAP_REPEAT, 4, 1);
    ASSERT_EQ(D3D12_FILTER_MIN_MAG_POINT_MIP_LINEAR, m_Context->m_TextureSamplers[point].m_Desc.Filter);
    int32_t linear = CreateTextureSampler(m_Context, TEXTURE_FILTER_NEAREST_MIPMAP_LINEAR, TEXTURE_FILTER_LINEAR, TEXTURE_WRAP_REPEAT, TEXTURE_WRAP_REPEAT, TEXTURE_WRAP_REPEAT, 4, 1);
    ASSERT_EQ(D3D12_FILTER_MIN_POINT_MAG_MIP_LINEAR, m_Context->m_TextureSamplers[linear].m_Desc.Filter);
    int32_t aniso = CreateTextureSampler(m_Context, TEXTURE_FILTER_LINEAR_MIPMAP_LINEAR, TEXTURE_FILTER_LINEAR, TEXTURE_WRAP_REPEAT, TEXTURE_WRAP_REPEAT, TEXTURE_WRAP_REPEAT, 4, 8);
    ASSERT_EQ(D3D12_FILTER_ANISOTROPIC, m_Context->m_TextureSamplers[aniso].m_Desc.Filter);
}

int main(int argc, char** argv)
{
    GraphicsAdapterDX12();
    // The fixture supplies WARP; do not require a hardware adapter or a window.
    GetRegisteredAdapter(0)->m_IsSupportedCb = []() { return true; };
    if (!InstallAdapter(ADAPTER_FAMILY_DIRECTX))
        return 1;
    jc_test_init(&argc, argv);
    return jc_test_run_all();
}
