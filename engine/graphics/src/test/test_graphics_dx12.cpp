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
#include <dlib/array.h>
#include <dlib/dstrings.h>
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

static void ReleasePipeline(void*, const uint64_t*, DX12Pipeline* pipeline)
{
    (*pipeline)->Release();
}

static bool IsTestAdapterSupported()
{
    return true;
}

static void RequireHR(HRESULT hr)
{
    ASSERT_EQ(S_OK, hr);
}

// Build real DXBC and serialized signatures, then load via the public program API.
struct TestProgram
{
    ShaderDesc m_Desc;
    // Own the compiled blobs referenced by the descriptor until the test finishes.
    dmArray<ID3DBlob*> m_Blobs;

    TestProgram()
    {
        memset(&m_Desc, 0, sizeof(m_Desc));
        m_Blobs.SetCapacity(4);
    }

    ~TestProgram()
    {
        for (uint32_t i = 0; i < m_Desc.m_Shaders.m_Count; ++i)
            free(m_Desc.m_Shaders[i].m_HlslResourceMapping.m_Data);
        DeleteShaderDesc(&m_Desc);
        for (uint32_t i = 0; i < m_Blobs.Size(); ++i)
            m_Blobs[i]->Release();
    }

    void AddShaderStage(const char* source, ShaderDesc::ShaderType stage)
    {
        ComPtr<ID3DBlob> code, errors;
        const char* profile = "ps_5_1";
        if (stage == ShaderDesc::SHADER_TYPE_COMPUTE)
            profile = "cs_5_1";
        else if (stage == ShaderDesc::SHADER_TYPE_VERTEX)
            profile = "vs_5_1";
        HRESULT hr = D3DCompile(source, strlen(source), 0, 0, 0, "main", profile, 0, 0, &code, &errors);
        if (FAILED(hr) && errors)
            printf("%s\n", (const char*)errors->GetBufferPointer());
        RequireHR(hr);
        AddShader(&m_Desc, ShaderDesc::LANGUAGE_HLSL_51, stage, (uint8_t*)code->GetBufferPointer(), (uint32_t)code->GetBufferSize());
        if (m_Blobs.Full())
            m_Blobs.OffsetCapacity(4);
        m_Blobs.Push(code.Detach());
    }

    void SetRootSignature(const char* text)
    {
        ComPtr<ID3DBlob> root, errors;
        const char* prefix = "#define main \"";
        const char* suffix = "\"\n";
        dmArray<char> source;
        source.SetCapacity((uint32_t)(strlen(prefix) + strlen(text) + strlen(suffix) + 1));
        source.SetSize(source.Capacity());
        dmSnPrintf(source.Begin(), source.Size(), "%s%s%s", prefix, text, suffix);
        RequireHR(D3DCompile(source.Begin(), source.Size() - 1, 0, 0, 0, "main", "rootsig_1_0", 0, 0, &root, &errors));
        m_Desc.m_HlslRootSignature.m_Data = (uint8_t*)root->GetBufferPointer();
        m_Desc.m_HlslRootSignature.m_Count = (uint32_t)root->GetBufferSize();
        if (m_Blobs.Full())
            m_Blobs.OffsetCapacity(4);
        m_Blobs.Push(root.Detach());
    }

    void AddBinding(const char* name, uint32_t binding, uint32_t root_index, ShaderDesc::ShaderDataType type, BindingType family, uint32_t shader_index = 0)
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

    HProgram CreateProgram(HContext context)
    {
        char error[1024] = {};
        HProgram program = NewProgram(context, &m_Desc, error, sizeof(error));
        if (!program)
            printf("Program load: %s\n", error);
        return program;
    }
};

static const char* FULLSCREEN_VS =
    "float4 main(uint id:SV_VertexID):SV_Position {float2 p[3]={float2(-1,-1),float2(-1,3),float2(3,-1)};"
    "return float4(p[id],0.5,1);"
    "}";

struct DX12Test : public jc_test_base_class
{
    DX12Context* m_Context;
    ComPtr<ID3D12InfoQueue> m_Info;
    ComPtr<ID3D12CommandAllocator> m_Allocator;
    ComPtr<ID3D12Fence> m_Fence;
    uint64_t m_FenceValue;
    dmArray<HTexture> m_Textures;

    HContext GetContext()
    {
        return (HContext)m_Context;
    }

    DX12FrameResource& GetFrame()
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
        D3D12_MESSAGE_ID clear_warnings[] = {
            D3D12_MESSAGE_ID_CLEARRENDERTARGETVIEW_MISMATCHINGCLEARVALUE,
            D3D12_MESSAGE_ID_CLEARDEPTHSTENCILVIEW_MISMATCHINGCLEARVALUE,
        };
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
        GetFrame().m_CommandAllocator = m_Allocator.Get();
        m_Context->m_FrameBegun = 1;
        m_Context->m_BaseContext.m_DefaultTextureMinFilter = TEXTURE_FILTER_NEAREST;
        m_Context->m_BaseContext.m_DefaultTextureMagFilter = TEXTURE_FILTER_NEAREST;
        m_Context->m_PipelineState = GetDefaultPipelineState();
        GetFrame().m_ScratchBuffer.Initialize(m_Context, 0);
        // Deliberately small to exercise the uniform fallback as well as ring allocations.
        GetFrame().m_UploadRing.Initialize(m_Context, 256 * 300);
        CreateTextureSampler(m_Context, TEXTURE_FILTER_NEAREST, TEXTURE_FILTER_NEAREST, TEXTURE_WRAP_CLAMP_TO_EDGE, TEXTURE_WRAP_CLAMP_TO_EDGE, TEXTURE_WRAP_CLAMP_TO_EDGE, 1, 1);
    }

    void SubmitAndWait()
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
        SubmitAndWait();
        for (uint32_t i = 0; i < m_Textures.Size(); ++i)
            DeleteTexture(GetContext(), m_Textures[i]);
        FlushResourcesToDestroy(GetFrame());
        GetFrame().m_ScratchBuffer.Destroy();
        GetFrame().m_UploadRing.Destroy();
        m_Context->m_PipelineCache.Iterate<void>(ReleasePipeline, 0);
        m_Context->m_CommandList->Close();
        m_Context->m_CommandList->Release();
        m_Context->m_CommandQueue->Release();
        m_Allocator.Reset();
        m_Fence.Reset();
        for (UINT64 i = 0; i < m_Info->GetNumStoredMessages(); ++i)
        {
            SIZE_T size = 0;
            m_Info->GetMessage(i, 0, &size);
            dmArray<uint8_t> buffer;
            buffer.SetCapacity((uint32_t)size);
            buffer.SetSize(buffer.Capacity());
            memset(buffer.Begin(), 0, buffer.Size());
            D3D12_MESSAGE* message = (D3D12_MESSAGE*)buffer.Begin();
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

    HTexture CreateTestTexture(TextureType type, uint16_t width, uint16_t height, uint16_t layers, uint8_t mips)
    {
        const bool is_volume = type == TEXTURE_TYPE_3D || type == TEXTURE_TYPE_IMAGE_3D;
        TextureCreationParams params;
        params.m_Type = type;
        params.m_Width = width;
        params.m_Height = height;
        // Large arrays use the depth field because the layer-count field is only 8 bits.
        params.m_LayerCount = is_volume || layers > 255 ? 1 : layers;
        params.m_Depth = is_volume || layers > 255 ? layers : 1;
        params.m_MipMapCount = mips;
        HTexture texture = NewTexture(GetContext(), params);
        if (m_Textures.Full())
            m_Textures.OffsetCapacity(8);
        m_Textures.Push(texture);
        return texture;
    }

    DX12Texture* GetTextureData(HTexture texture)
    {
        return GetAssetFromContainer<DX12Texture>(m_Context->m_BaseContext.m_AssetHandleContainer, texture);
    }

    HRenderTarget CreateTestRenderTarget(uint32_t width, uint32_t height, uint32_t flags = BUFFER_TYPE_COLOR0_BIT, uint32_t samples = 1)
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
        return NewRenderTarget(GetContext(), flags, params);
    }

    void ReadTexture(HTexture texture, uint32_t mip, uint32_t layer, dmArray<uint8_t>& bytes)
    {
        DX12Texture* tex = GetTextureData(texture);
        const uint32_t subresource = D3D12CalcSubresource(mip, layer, 0, tex->m_ResourceDesc.MipLevels, tex->m_ResourceDesc.DepthOrArraySize);
        D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint;
        UINT rows;
        UINT64 row_bytes, size;
        m_Context->m_Device->GetCopyableFootprints(&tex->m_ResourceDesc, subresource, 1, 0, &footprint, &rows, &row_bytes, &size);
        ComPtr<ID3D12Resource> readback;
        RequireHR(m_Context->m_Device->CreateCommittedResource(
            &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_READBACK),
            D3D12_HEAP_FLAG_NONE,
            &CD3DX12_RESOURCE_DESC::Buffer(size),
            D3D12_RESOURCE_STATE_COPY_DEST,
            0,
            IID_PPV_ARGS(&readback)));
        TransitionTexture(m_Context->m_CommandList, tex, D3D12_RESOURCE_STATE_COPY_SOURCE, subresource, 1);
        CD3DX12_TEXTURE_COPY_LOCATION src(tex->m_Resource, subresource), dst(readback.Get(), footprint);
        m_Context->m_CommandList->CopyTextureRegion(&dst, 0, 0, 0, &src, 0);
        SubmitAndWait();
        uint8_t* mapped;
        RequireHR(readback->Map(0, 0, (void**)&mapped));
        // Strip DX12 row padding from the copied subresource.
        const uint32_t byte_count = (uint32_t)(row_bytes * rows * footprint.Footprint.Depth);
        bytes.SetSize(0);
        bytes.SetCapacity(byte_count);
        bytes.SetSize(byte_count);
        for (UINT row = 0; row < rows * footprint.Footprint.Depth; ++row)
            memcpy(bytes.Begin() + row * row_bytes, mapped + footprint.Offset + row * footprint.Footprint.RowPitch, (size_t)row_bytes);
        readback->Unmap(0, 0);
    }
};

// Verifies array-layer uploads and partial RGB conversion preserve untouched pixels.
TEST_F(DX12Test, ArrayUploadsAndPartialRGBUpdates)
{
    HTexture texture = CreateTestTexture(TEXTURE_TYPE_2D_ARRAY, 8, 8, 20, 3);
    for (uint32_t mip = 0; mip < 3; ++mip)
    {
        const uint32_t width = 8 >> mip;
        dmArray<uint8_t> pixels;
        pixels.SetCapacity(width * width * 3 * 20);
        pixels.SetSize(pixels.Capacity());
        memset(pixels.Begin(), 0, pixels.Size());
        for (uint32_t layer = 0; layer < 20; ++layer)
            memset(pixels.Begin() + layer * width * width * 3, 10 + layer + mip * 30, width * width * 3);
        TextureParams params;
        params.m_Format = TEXTURE_FORMAT_RGB;
        params.m_Data = pixels.Begin();
        params.m_DataSize = width * width * 3;
        params.m_Width = params.m_Height = width;
        params.m_LayerCount = 20;
        params.m_MipMap = mip;
        SetTexture(GetContext(), texture, params);
    }
    ASSERT_EQ(60u, GetTextureData(texture)->m_ResourceStates.Size());
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
    SetTexture(GetContext(), texture, params);
    for (uint32_t mip = 0; mip < 3; ++mip)
        for (uint32_t layer = 0; layer < 20; ++layer)
        {
            const uint32_t width = 8 >> mip;
            dmArray<uint8_t> bytes;
            ReadTexture(texture, mip, layer, bytes);
            for (uint32_t y = 0; y < width; ++y)
                for (uint32_t x = 0; x < width; ++x)
                {
                    const bool is_patch = mip == 1 && layer >= 17 && layer <= 18 &&
                                          x >= 1 && x < 3 && y >= 1 && y < 3;
                    const uint8_t expected = is_patch ? 201 : 10 + layer + mip * 30;
                    ASSERT_EQ(expected, bytes[(y * width + x) * 4]);
                    ASSERT_EQ(255, bytes[(y * width + x) * 4 + 3]);
                }
        }
    TransitionTexture(m_Context->m_CommandList, GetTextureData(texture), (D3D12_RESOURCE_STATES)(D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE));
}

// Verifies maximum array layers and cubemap mip uploads use the correct subresources.
TEST_F(DX12Test, MaximumArrayLayersAndCubemapMips)
{
    HTexture array = CreateTestTexture(TEXTURE_TYPE_2D_ARRAY, 1, 1, 2048, 1);
    dmArray<uint8_t> pixels;
    pixels.SetCapacity(2048 * 4);
    pixels.SetSize(pixels.Capacity());
    memset(pixels.Begin(), 77, pixels.Size());
    TextureParams params;
    params.m_Format = TEXTURE_FORMAT_RGBA;
    params.m_Data = pixels.Begin();
    params.m_DataSize = 4;
    params.m_Width = params.m_Height = 1;
    SetTexture(GetContext(), array, params);
    ASSERT_EQ(2048u, GetTextureData(array)->m_ResourceStates.Size());
    dmArray<uint8_t> bytes;
    ReadTexture(array, 0, 2047, bytes);
    ASSERT_EQ(77, bytes[0]);
    HTexture cube = CreateTestTexture(TEXTURE_TYPE_CUBE_MAP, 8, 8, 1, 4);
    for (uint32_t mip = 0; mip < 4; ++mip)
    {
        const uint32_t width = 8 >> mip;
        pixels.SetSize(0);
        pixels.SetCapacity(width * width * 4 * 6);
        pixels.SetSize(pixels.Capacity());
        memset(pixels.Begin(), 30 + mip, pixels.Size());
        params.m_Data = pixels.Begin();
        params.m_DataSize = width * width * 4;
        params.m_Width = params.m_Height = width;
        params.m_MipMap = mip;
        SetTexture(GetContext(), cube, params);
    }
    ASSERT_EQ(24u, GetTextureData(cube)->m_ResourceStates.Size());
    ReadTexture(cube, 3, 5, bytes);
    ASSERT_EQ(33, bytes[0]);
}

// Verifies compressed array updates preserve blocks outside the updated region.
TEST_F(DX12Test, CompressedArraySubUpdatePreservesOtherBlocks)
{
    HTexture texture = CreateTestTexture(TEXTURE_TYPE_2D_ARRAY, 16, 16, 19, 2);
    for (uint32_t mip = 0; mip < 2; ++mip)
    {
        const uint32_t width = 16 >> mip;
        const uint32_t slice_size = (width / 4) * (width / 4) * 8;
        dmArray<uint8_t> pixels;
        pixels.SetCapacity(slice_size * 19);
        pixels.SetSize(pixels.Capacity());
        memset(pixels.Begin(), 0, pixels.Size());
        for (uint32_t layer = 0; layer < 19; ++layer)
            memset(pixels.Begin() + layer * slice_size, layer + mip * 30, slice_size);
        TextureParams params;
        params.m_Format = TEXTURE_FORMAT_RGB_BC1;
        params.m_Data = pixels.Begin();
        params.m_DataSize = slice_size;
        params.m_Width = params.m_Height = width;
        params.m_LayerCount = 19;
        params.m_MipMap = mip;
        SetTexture(GetContext(), texture, params);
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
    SetTexture(GetContext(), texture, params);
    dmArray<uint8_t> bytes;
    ReadTexture(texture, 1, 18, bytes);
    ASSERT_EQ(32u, bytes.Size());
    for (uint32_t i = 0; i < bytes.Size(); ++i)
        ASSERT_EQ(i >= 24 ? 201 : 48, bytes[i]);
    ReadTexture(texture, 1, 17, bytes);
    ASSERT_EQ(47, bytes[0]);
    ReadTexture(texture, 0, 18, bytes);
    ASSERT_EQ(18, bytes[0]);
}

// Verifies stencil operations and separate face settings produce the expected pipeline state.
TEST_F(DX12Test, StencilStateAndSeparateFaces)
{
    SetStencilMask(GetContext(), 0x53);
    SetStencilFuncSeparate(GetContext(), FACE_TYPE_FRONT, COMPARE_FUNC_EQUAL, 0x37, 0x71);
    SetStencilFuncSeparate(GetContext(), FACE_TYPE_BACK, COMPARE_FUNC_NOTEQUAL, 0x37, 0x71);
    SetStencilOpSeparate(GetContext(), FACE_TYPE_FRONT, STENCIL_OP_INCR, STENCIL_OP_DECR_WRAP, STENCIL_OP_REPLACE);
    SetStencilOpSeparate(GetContext(), FACE_TYPE_BACK, STENCIL_OP_INCR_WRAP, STENCIL_OP_DECR, STENCIL_OP_INVERT);
    EnableState(GetContext(), STATE_STENCIL_TEST);
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
    SetStencilFuncSeparate(GetContext(), FACE_TYPE_FRONT_AND_BACK, COMPARE_FUNC_ALWAYS, 0, 0xff);
    SetStencilOpSeparate(GetContext(), FACE_TYPE_FRONT_AND_BACK, STENCIL_OP_KEEP, STENCIL_OP_ZERO, STENCIL_OP_INVERT);
    desc = GetDepthStencilState(m_Context->m_PipelineState);
    ASSERT_EQ(0, memcmp(&desc.FrontFace, &desc.BackFace, sizeof(desc.FrontFace)));
}

// Verifies descriptor page rollover and uniform upload overflow preserve earlier draw data.
TEST_F(DX12Test, DescriptorPagesAndUniformOverflowPreserveDrawData)
{
    const uint32_t count = 600;
    HTexture texture = CreateTestTexture(TEXTURE_TYPE_2D, 1, 1, 1, 1);
    uint8_t red[] = { 255, 0, 0, 255 };
    TextureParams tex_params;
    tex_params.m_Format = TEXTURE_FORMAT_RGBA;
    tex_params.m_Data = red;
    tex_params.m_DataSize = sizeof(red);
    tex_params.m_Width = tex_params.m_Height = 1;
    SetTexture(GetContext(), texture, tex_params);
    TransitionTexture(m_Context->m_CommandList, GetTextureData(texture), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);

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
    RequireHR(m_Context->m_Device->CreateCommittedResource(
        &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
        D3D12_HEAP_FLAG_NONE,
        &CD3DX12_RESOURCE_DESC::Buffer(count * 16, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS),
        D3D12_RESOURCE_STATE_COMMON,
        0,
        IID_PPV_ARGS(&output)));
    RequireHR(m_Context->m_Device->CreateCommittedResource(
        &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_READBACK),
        D3D12_HEAP_FLAG_NONE,
        &CD3DX12_RESOURCE_DESC::Buffer(count * 16),
        D3D12_RESOURCE_STATE_COPY_DEST,
        0,
        IID_PPV_ARGS(&readback)));

    D3D12_RESOURCE_BARRIER initial_barrier = CD3DX12_RESOURCE_BARRIER::Transition(output.Get(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    m_Context->m_CommandList->ResourceBarrier(1, &initial_barrier);
    for (uint32_t frame = 0; frame < 2; ++frame)
    {
        m_Context->m_CommandList->SetComputeRootSignature(root.Get());
        m_Context->m_CommandList->SetPipelineState(pso.Get());
        m_Context->m_CommandList->SetComputeRootUnorderedAccessView(3, output->GetGPUVirtualAddress());
        for (uint32_t i = 0; i < count; ++i)
        {
            ASSERT_TRUE(GetFrame().m_ScratchBuffer.Prepare(m_Context, 1, 1));
            GetFrame().m_ScratchBuffer.AllocateTexture2D(m_Context, PIPELINE_TYPE_COMPUTE, GetTextureData(texture), 0);
            GetFrame().m_ScratchBuffer.AllocateSampler(m_Context, PIPELINE_TYPE_COMPUTE, m_Context->m_TextureSamplers[0], 1);
            struct Constants
            {
                uint32_t index;
                float value;
            } constants = { i, (float)(i + frame) };
            void* data = GetFrame().m_ScratchBuffer.AllocateConstantBuffer(m_Context, PIPELINE_TYPE_COMPUTE, 2, sizeof(constants));
            ASSERT_NE((void*)0, data);
            memcpy(data, &constants, sizeof(constants));
            m_Context->m_CommandList->Dispatch(1, 1, 1);
        }
        ASSERT_EQ(3u, GetFrame().m_ScratchBuffer.m_DescriptorPools.Size());
        ASSERT_GT(GetFrame().m_ResourcesToDestroy.Size(), 0u);
        D3D12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(output.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COPY_SOURCE);
        m_Context->m_CommandList->ResourceBarrier(1, &barrier);
        m_Context->m_CommandList->CopyResource(readback.Get(), output.Get());
        SubmitAndWait();
        float* values;
        RequireHR(readback->Map(0, 0, (void**)&values));
        for (uint32_t i = 0; i < count; ++i)
        {
            ASSERT_EQ((float)(i + frame + 1), values[i * 4]);
            ASSERT_EQ((float)(i + frame), values[i * 4 + 1]);
        }
        readback->Unmap(0, 0);
        // Reuse only after SubmitAndWait has waited for the fence. No page allocations on the second frame.
        FlushResourcesToDestroy(GetFrame());
        GetFrame().m_ScratchBuffer.Reset(m_Context);
        GetFrame().m_UploadRing.Reset();
        if (frame == 0)
        {
            barrier = CD3DX12_RESOURCE_BARRIER::Transition(output.Get(), D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
            m_Context->m_CommandList->ResourceBarrier(1, &barrier);
        }
    }
    // CPU sampler caching must not write beyond a fixed heap or truncate indices at 1024.
    for (uint32_t i = 1; i <= 1100; ++i)
        CreateTextureSampler(m_Context, TEXTURE_FILTER_NEAREST, TEXTURE_FILTER_NEAREST, TEXTURE_WRAP_CLAMP_TO_EDGE, TEXTURE_WRAP_CLAMP_TO_EDGE, TEXTURE_WRAP_CLAMP_TO_EDGE, 1, 1.0f + i / 128.0f);
    SetTextureParams(GetContext(), texture, TEXTURE_FILTER_NEAREST, TEXTURE_FILTER_NEAREST, TEXTURE_WRAP_CLAMP_TO_EDGE, TEXTURE_WRAP_CLAMP_TO_EDGE, TEXTURE_WRAP_CLAMP_TO_EDGE, 1.0f + 1100 / 128.0f);
    ASSERT_GT(GetTextureData(texture)->m_TextureSamplerIndex, 1024u);
    ASSERT_EQ(m_Context->m_TextureSamplers.Size() - 1, GetTextureData(texture)->m_TextureSamplerIndex);
}

// Verifies masked stencil references clip rendered pixels correctly.
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
    HRenderTarget target = NewRenderTarget(GetContext(), BUFFER_TYPE_COLOR0_BIT | BUFFER_TYPE_DEPTH_BIT | BUFFER_TYPE_STENCIL_BIT, params);
    m_Context->m_CurrentRenderTarget = target;
    m_Context->m_MainRenderTarget = target;
    SetRenderTarget(GetContext(), target, RenderTargetBindingParams());
    HTexture color = GetRenderTargetTexture(GetContext(), target, BUFFER_TYPE_COLOR0_BIT);
    DX12RenderTarget* rt = GetAssetFromContainer<DX12RenderTarget>(m_Context->m_BaseContext.m_AssetHandleContainer, target);
    ASSERT_EQ(2u, GetTextureData(rt->m_Base.m_TextureDepthStencil)->m_ResourceStates.Size());
    Clear(GetContext(), BUFFER_TYPE_COLOR0_BIT | BUFFER_TYPE_DEPTH_BIT | BUFFER_TYPE_STENCIL_BIT, 0, 0, 0, 0, 1, 0);
    const char* vs = FULLSCREEN_VS;
    const char* ps = "float4 main():SV_Target {return float4(1,0,0,1);}";
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
    DisableState(GetContext(), STATE_DEPTH_TEST);
    DisableState(GetContext(), STATE_CULL_FACE);
    EnableState(GetContext(), STATE_STENCIL_TEST);
    SetColorMask(GetContext(), false, false, false, false);
    SetStencilMask(GetContext(), 0x0f);
    SetStencilFunc(GetContext(), COMPARE_FUNC_ALWAYS, 0x35, 0xff);
    SetStencilOp(GetContext(), STENCIL_OP_KEEP, STENCIL_OP_KEEP, STENCIL_OP_REPLACE);
    D3D12_RECT scissor = { 0, 0, 2, 4 };
    m_Context->m_CommandList->RSSetScissorRects(1, &scissor);
    Draw(GetContext(), PRIMITIVE_TRIANGLES, 0, 3, 1);
    SetColorMask(GetContext(), true, true, true, true);
    SetStencilMask(GetContext(), 0);
    SetStencilFunc(GetContext(), COMPARE_FUNC_EQUAL, 0xa5, 0x0f);
    SetStencilOp(GetContext(), STENCIL_OP_KEEP, STENCIL_OP_KEEP, STENCIL_OP_KEEP);
    scissor.right = 4;
    m_Context->m_CommandList->RSSetScissorRects(1, &scissor);
    Draw(GetContext(), PRIMITIVE_TRIANGLES, 0, 3, 1);
    dmArray<uint8_t> pixels;
    ReadTexture(color, 0, 0, pixels);
    for (uint32_t y = 0; y < 4; ++y)
        for (uint32_t x = 0; x < 4; ++x)
            ASSERT_EQ(x < 2 ? 255 : 0, pixels[(y * 4 + x) * 4]);
    m_Context->m_CurrentProgram = 0;
    DeleteRenderTarget(GetContext(), target);
}

// Verifies volume mip uploads and Z-slice updates preserve untouched voxels.
TEST_F(DX12Test, VolumeMipUploadsAndZSubUpdates)
{
    HTexture texture = CreateTestTexture(TEXTURE_TYPE_3D, 8, 8, 4, 3);
    TextureParams params;
    params.m_Format = TEXTURE_FORMAT_RGB;
    for (uint32_t mip = 0; mip < 3; ++mip)
    {
        params.m_MipMap = mip;
        params.m_Width = params.m_Height = 8 >> mip;
        params.m_Depth = dmMath::Max(1u, 4u >> mip);
        dmArray<uint8_t> data;
        data.SetCapacity(params.m_Width * params.m_Height * params.m_Depth * 3);
        data.SetSize(data.Capacity());
        memset(data.Begin(), 20 + mip, data.Size());
        params.m_Data = data.Begin();
        SetTexture(GetContext(), texture, params);
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
    SetTexture(GetContext(), texture, params);
    ASSERT_EQ(D3D12_RESOURCE_DIMENSION_TEXTURE3D, GetTextureData(texture)->m_ResourceDesc.Dimension);
    ASSERT_EQ(3u, GetTextureData(texture)->m_ResourceStates.Size());
    for (uint32_t mip = 0; mip < 3; ++mip)
    {
        dmArray<uint8_t> pixels;
        ReadTexture(texture, mip, 0, pixels);
        const uint32_t width = 8 >> mip;
        const uint32_t depth = dmMath::Max(1u, 4u >> mip);
        for (uint32_t z = 0; z < depth; ++z)
            for (uint32_t y = 0; y < width; ++y)
                for (uint32_t x = 0; x < width; ++x)
                {
                    const bool is_patch = mip == 1 && z == 1 && x >= 1 && x < 3 && y >= 1 && y < 3;
                    const uint8_t expected = is_patch ? 99 : 20 + mip;
                    ASSERT_EQ(expected, pixels[((z * width + y) * width + x) * 4]);
                }
    }
    GetFrame().m_ScratchBuffer.Prepare(m_Context, 1, 0);
    // View creation is also exercised by the compute sampling test below.
}

// Verifies resizing a bound target restores its pass before a clear, guarding against stale RTV/DSV handles.
TEST_F(DX12Test, BoundTargetResizeRestoresClearAttachments)
{
    HRenderTarget other = CreateTestRenderTarget(1, 1);
    for (uint32_t samples = 1; samples <= 4; samples *= 4)
    {
        HRenderTarget target = CreateTestRenderTarget(4, 4, BUFFER_TYPE_COLOR0_BIT | BUFFER_TYPE_DEPTH_BIT, samples);
        SetRenderTarget(GetContext(), target, RenderTargetBindingParams());
        DX12RenderTarget* rt = GetAssetFromContainer<DX12RenderTarget>(m_Context->m_BaseContext.m_AssetHandleContainer, target);
        for (uint32_t size = 2; size < 7; ++size)
        {
            SetRenderTargetSize(GetContext(), target, size, size);
            ASSERT_TRUE(rt->m_Base.m_IsBound);
            ASSERT_EQ(target, m_Context->m_CurrentRenderTarget);
            ASSERT_EQ(D3D12_RESOURCE_STATE_DEPTH_WRITE, GetTextureData(rt->m_Base.m_TextureDepthStencil)->m_ResourceStates[0]);
            Clear(GetContext(), BUFFER_TYPE_COLOR0_BIT | BUFFER_TYPE_DEPTH_BIT, 0, 0, 255, 255, 0.25f, 0);
            uint8_t pixel[4] = {};
            ReadPixels(GetContext(), 1, 1, 1, 1, pixel, sizeof(pixel));
            ASSERT_EQ(255, pixel[0]);
            ASSERT_EQ(0, pixel[2]);
        }
        SetRenderTarget(GetContext(), other, RenderTargetBindingParams());
        SubmitAndWait();
        DeleteRenderTarget(GetContext(), target);
    }
    DeleteRenderTarget(GetContext(), other);
}

// Verifies MRT shaders can draw to a single color attachment without DX12 validation errors.
TEST_F(DX12Test, MRTShaderWithSingleBoundAttachment)
{
    HRenderTarget target = CreateTestRenderTarget(4, 4);
    SetRenderTarget(GetContext(), target, RenderTargetBindingParams());
    SetViewport(GetContext(), 0, 0, 4, 4);
    DisableState(GetContext(), STATE_CULL_FACE);
    DisableState(GetContext(), STATE_DEPTH_TEST);
    TestProgram shader;
    shader.AddShaderStage(FULLSCREEN_VS, ShaderDesc::SHADER_TYPE_VERTEX);
    shader.AddShaderStage(
        "struct Out {float4 color:SV_Target0;"
        "float4 extra:SV_Target1;"
        "};"
        " Out main(){Out o;"
        "o.color=float4(1,0,0,1);"
        "o.extra=float4(0,1,0,1);"
        "return o;"
        "}", ShaderDesc::SHADER_TYPE_FRAGMENT);
    shader.SetRootSignature("RootFlags(ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT)");
    HProgram program = shader.CreateProgram(GetContext());
    ASSERT_NE((HProgram)0, program);
    EnableProgram(GetContext(), program);
    Draw(GetContext(), PRIMITIVE_TRIANGLES, 0, 3, 1);
    uint8_t pixel[4] = {};
    ReadPixels(GetContext(), 1, 1, 1, 1, pixel, sizeof(pixel));
    ASSERT_EQ(255, pixel[2]);
    ASSERT_EQ(0, pixel[1]);
    SubmitAndWait();
    DisableProgram(GetContext());
    DeleteProgram(GetContext(), program);
    DeleteRenderTarget(GetContext(), target);
}

// Verifies multisample MRT clears, resize and public readback preserve attachment colors.
TEST_F(DX12Test, MultisampleMRTClearResizeAndPublicReadback)
{
    HRenderTarget target = CreateTestRenderTarget(4, 4, BUFFER_TYPE_COLOR0_BIT | BUFFER_TYPE_COLOR1_BIT | BUFFER_TYPE_DEPTH_BIT, 4);
    HRenderTarget other = CreateTestRenderTarget(1, 1);
    SetRenderTarget(GetContext(), target, RenderTargetBindingParams());
    DX12RenderTarget* rt = GetAssetFromContainer<DX12RenderTarget>(m_Context->m_BaseContext.m_AssetHandleContainer, target);
    ASSERT_EQ(4u, rt->m_SampleDesc.Count);
    ASSERT_EQ(4u, GetTextureData(rt->m_Base.m_TextureDepthStencil)->m_ResourceDesc.SampleDesc.Count);
    Clear(GetContext(), BUFFER_TYPE_COLOR0_BIT, 255, 0, 0, 255, 1, 0);
    Clear(GetContext(), BUFFER_TYPE_COLOR1_BIT | BUFFER_TYPE_DEPTH_BIT, 0, 255, 0, 255, 1, 0);
    uint8_t pixels[4 * 4 * 4] = {};
    ReadPixels(GetContext(), 0, 0, 4, 4, pixels, sizeof(pixels));
    for (uint32_t i = 0; i < 16; ++i)
    {
        ASSERT_EQ(255, pixels[i * 4 + 2]);
        ASSERT_EQ(0, pixels[i * 4 + 1]);
    }
    SetRenderTarget(GetContext(), other, RenderTargetBindingParams());
    dmArray<uint8_t> green;
    ReadTexture(rt->m_Base.m_TextureColor[1], 0, 0, green);
    for (uint32_t i = 0; i < 16; ++i)
        ASSERT_EQ(255, green[i * 4 + 1]);
    for (uint32_t size = 2; size < 7; ++size)
    {
        SetRenderTargetSize(GetContext(), target, size, size);
        SetRenderTarget(GetContext(), target, RenderTargetBindingParams());
        Clear(GetContext(), BUFFER_TYPE_COLOR0_BIT, 0, 0, 255, 255, 1, 0);
        ReadPixels(GetContext(), 1, 1, 1, 1, pixels, sizeof(pixels));
        ASSERT_EQ(255, pixels[0]);
        ASSERT_EQ(0, pixels[2]);
        SetRenderTarget(GetContext(), other, RenderTargetBindingParams());
    }
    SubmitAndWait();
    DeleteRenderTarget(GetContext(), target);
    DeleteRenderTarget(GetContext(), other);
}

// Verifies shader reload, line rendering, depth bias and scissor survive pipeline changes.
TEST_F(DX12Test, ShaderReloadLinesBiasAndScissor)
{
    HRenderTarget target = CreateTestRenderTarget(4, 4, BUFFER_TYPE_COLOR0_BIT | BUFFER_TYPE_DEPTH_BIT);
    SetRenderTarget(GetContext(), target, RenderTargetBindingParams());
    SetViewport(GetContext(), 0, 0, 4, 4);
    DisableState(GetContext(), STATE_CULL_FACE);
    DisableState(GetContext(), STATE_DEPTH_TEST);
    TestProgram red, green;
    red.AddShaderStage(FULLSCREEN_VS, ShaderDesc::SHADER_TYPE_VERTEX);
    red.AddShaderStage("float4 main():SV_Target{return float4(1,0,0,1);}", ShaderDesc::SHADER_TYPE_FRAGMENT);
    red.SetRootSignature("RootFlags(ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT)");
    green.AddShaderStage(FULLSCREEN_VS, ShaderDesc::SHADER_TYPE_VERTEX);
    green.AddShaderStage("float4 main():SV_Target{return float4(0,1,0,1);}", ShaderDesc::SHADER_TYPE_FRAGMENT);
    green.SetRootSignature("RootFlags(ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT)");
    HProgram program = red.CreateProgram(GetContext());
    ASSERT_NE((HProgram)0, program);
    EnableProgram(GetContext(), program);
    Clear(GetContext(), BUFFER_TYPE_COLOR0_BIT, 0, 0, 0, 0, 1, 0);
    SetScissor(GetContext(), 0, 0, 2, 4);
    EnableState(GetContext(), STATE_SCISSOR_TEST);
    Draw(GetContext(), PRIMITIVE_TRIANGLES, 0, 3, 1);
    uint8_t pixels[64] = {};
    ReadPixels(GetContext(), 0, 0, 4, 4, pixels, sizeof(pixels));
    for (uint32_t i = 0; i < 16; ++i)
        ASSERT_EQ(i % 4 < 2 ? 255 : 0, pixels[i * 4 + 2]);
    char error[512] = {};
    ASSERT_TRUE(ReloadProgram(GetContext(), program, &green.m_Desc, error, sizeof(error)));
    // Readback breaks and restores the render pass, retaining the scissor rectangle.
    Draw(GetContext(), PRIMITIVE_TRIANGLES, 0, 3, 1);
    ReadPixels(GetContext(), 0, 0, 4, 4, pixels, sizeof(pixels));
    for (uint32_t i = 0; i < 16; ++i)
        ASSERT_EQ(i % 4 < 2 ? 255 : 0, pixels[i * 4 + 1]);
    DisableState(GetContext(), STATE_SCISSOR_TEST);
    Draw(GetContext(), PRIMITIVE_TRIANGLES, 0, 3, 1);
    ReadPixels(GetContext(), 0, 0, 4, 4, pixels, sizeof(pixels));
    for (uint32_t i = 0; i < 16; ++i)
        ASSERT_EQ(255, pixels[i * 4 + 1]);
    ShaderDesc invalid = {};
    ASSERT_FALSE(ReloadProgram(GetContext(), program, &invalid, error, sizeof(error)));
    Draw(GetContext(), PRIMITIVE_TRIANGLES, 0, 3, 1);
    Draw(GetContext(), PRIMITIVE_LINES, 0, 2, 1);
    EnableState(GetContext(), STATE_POLYGON_OFFSET_FILL);
    SetPolygonOffset(GetContext(), 2.0f, 3.0f);
    Draw(GetContext(), PRIMITIVE_TRIANGLES, 0, 3, 1);
    ASSERT_GE(m_Context->m_PipelineCache.Size(), 4u);
    SubmitAndWait();
    DeleteProgram(GetContext(), program);
    DeleteRenderTarget(GetContext(), target);
}

// Verifies front/back culling matches the engine convention across render targets.
TEST_F(DX12Test, FrontAndBackCulling)
{
    HRenderTarget target = CreateTestRenderTarget(8, 8);
    SetRenderTarget(GetContext(), target, RenderTargetBindingParams());
    SetViewport(GetContext(), 0, 0, 8, 8);
    DisableState(GetContext(), STATE_DEPTH_TEST);
    TestProgram shader;
    shader.AddShaderStage(FULLSCREEN_VS, ShaderDesc::SHADER_TYPE_VERTEX);
    shader.AddShaderStage("float4 main():SV_Target{return float4(1,0,0,1);}", ShaderDesc::SHADER_TYPE_FRAGMENT);
    shader.SetRootSignature("RootFlags(ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT)");
    HProgram program = shader.CreateProgram(GetContext());
    ASSERT_NE((HProgram)0, program);
    EnableProgram(GetContext(), program);
    const uint16_t indices[] = { 0, 1, 2 };
    HIndexBuffer index_buffer = NewIndexBuffer(GetContext(), sizeof(indices), indices, BUFFER_USAGE_STATIC_DRAW);
    const PrimitiveType triangles[] = { PRIMITIVE_TRIANGLES, PRIMITIVE_TRIANGLE_STRIP };
    const FaceWinding windings[] = { FACE_WINDING_CW, FACE_WINDING_CCW };
    uint8_t pixels[8 * 8 * 4] = {};
    SetCullFace(GetContext(), FACE_TYPE_FRONT_AND_BACK);
    EnableState(GetContext(), STATE_CULL_FACE);
    for (uint32_t primitive_index = 0; primitive_index < DM_ARRAY_SIZE(triangles); ++primitive_index)
    {
        for (uint32_t winding_index = 0; winding_index < DM_ARRAY_SIZE(windings); ++winding_index)
        {
            SetFaceWinding(GetContext(), windings[winding_index]);
            for (uint32_t indexed = 0; indexed < 2; ++indexed)
            {
                Clear(GetContext(), BUFFER_TYPE_COLOR0_BIT, 0, 0, 255, 255, 1, 0);
                if (indexed)
                    DrawElements(GetContext(), triangles[primitive_index], 0, 3, TYPE_UNSIGNED_SHORT, index_buffer, 2);
                else
                    Draw(GetContext(), triangles[primitive_index], 0, 3, 2);
                ReadPixels(GetContext(), 0, 0, 8, 8, pixels, sizeof(pixels));
                for (uint32_t i = 0; i < 64; ++i)
                {
                    ASSERT_EQ(255, pixels[i * 4]);
                    ASSERT_EQ(0, pixels[i * 4 + 2]);
                }
            }
        }
    }

    // Disabling culling must restore a user's scissor without a pass break or viewport change.
    SetScissor(GetContext(), 0, 0, 4, 8);
    EnableState(GetContext(), STATE_SCISSOR_TEST);
    Draw(GetContext(), PRIMITIVE_TRIANGLES, 0, 3, 1);
    DisableState(GetContext(), STATE_CULL_FACE);
    Draw(GetContext(), PRIMITIVE_TRIANGLES, 0, 3, 1);
    ReadPixels(GetContext(), 0, 0, 8, 8, pixels, sizeof(pixels));
    for (uint32_t i = 0; i < 64; ++i)
        ASSERT_EQ(i % 8 < 4 ? 255 : 0, pixels[i * 4 + 2]);

    // Switching from both faces to a single face must also restore rasterization.
    Clear(GetContext(), BUFFER_TYPE_COLOR0_BIT, 0, 0, 255, 255, 1, 0);
    EnableState(GetContext(), STATE_CULL_FACE);
    Draw(GetContext(), PRIMITIVE_TRIANGLES, 0, 3, 1);
    SetCullFace(GetContext(), FACE_TYPE_FRONT);
    Draw(GetContext(), PRIMITIVE_TRIANGLES, 0, 3, 1);
    SetCullFace(GetContext(), FACE_TYPE_BACK);
    Draw(GetContext(), PRIMITIVE_TRIANGLES, 0, 3, 1);
    ReadPixels(GetContext(), 0, 0, 8, 8, pixels, sizeof(pixels));
    for (uint32_t i = 0; i < 64; ++i)
        ASSERT_EQ(i % 8 < 4 ? 255 : 0, pixels[i * 4 + 2]);

    // Lines have no faces. Compare against unculled lines, immediately after a culled triangle.
    DisableState(GetContext(), STATE_SCISSOR_TEST);
    DisableState(GetContext(), STATE_CULL_FACE);
    Clear(GetContext(), BUFFER_TYPE_COLOR0_BIT, 0, 0, 255, 255, 1, 0);
    TestProgram line_shader;
    line_shader.AddShaderStage(
        "float4 main(uint id:SV_VertexID):SV_Position {float2 p[2]={float2(-0.75,-0.5),float2(0.75,0.5)};"
        "return float4(p[id],0.5,1);"
        "}", ShaderDesc::SHADER_TYPE_VERTEX);
    line_shader.AddShaderStage("float4 main():SV_Target{return float4(1,0,0,1);}", ShaderDesc::SHADER_TYPE_FRAGMENT);
    line_shader.SetRootSignature("RootFlags(ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT)");
    HProgram line_program = line_shader.CreateProgram(GetContext());
    ASSERT_NE((HProgram)0, line_program);
    EnableProgram(GetContext(), line_program);
    Draw(GetContext(), PRIMITIVE_LINES, 0, 2, 1);
    uint8_t line_pixels[sizeof(pixels)] = {};
    ReadPixels(GetContext(), 0, 0, 8, 8, line_pixels, sizeof(line_pixels));
    uint32_t red_pixels = 0;
    for (uint32_t i = 0; i < 64; ++i)
        red_pixels += line_pixels[i * 4 + 2] == 255;
    ASSERT_GT(red_pixels, 0u);
    SetCullFace(GetContext(), FACE_TYPE_FRONT_AND_BACK);
    EnableState(GetContext(), STATE_CULL_FACE);
    for (uint32_t indexed = 0; indexed < 2; ++indexed)
    {
        Clear(GetContext(), BUFFER_TYPE_COLOR0_BIT, 0, 0, 255, 255, 1, 0);
        EnableProgram(GetContext(), program);
        Draw(GetContext(), PRIMITIVE_TRIANGLE_STRIP, 0, 3, 1);
        EnableProgram(GetContext(), line_program);
        if (indexed)
            DrawElements(GetContext(), PRIMITIVE_LINES, 0, 2, TYPE_UNSIGNED_SHORT, index_buffer, 1);
        else
            Draw(GetContext(), PRIMITIVE_LINES, 0, 2, 1);
        ReadPixels(GetContext(), 0, 0, 8, 8, pixels, sizeof(pixels));
        ASSERT_EQ(0, memcmp(line_pixels, pixels, sizeof(pixels)));
    }
    SubmitAndWait();
    DeleteIndexBuffer(index_buffer);
    DeleteProgram(GetContext(), line_program);
    DeleteProgram(GetContext(), program);
    DeleteRenderTarget(GetContext(), target);
}

// Verifies culling both faces preserves vertex shader storage writes.
TEST_F(DX12Test, FrontAndBackCullingPreservesVertexWrites)
{
    HTexture written = CreateTestTexture(TEXTURE_TYPE_IMAGE_2D, 3, 1, 1, 1);
    uint32_t zero[3] = {};
    TextureParams params;
    params.m_Format = TEXTURE_FORMAT_R32UI;
    params.m_Width = 3;
    params.m_Height = 1;
    params.m_Data = zero;
    SetTexture(GetContext(), written, params);
    HRenderTarget target = CreateTestRenderTarget(4, 4);
    SetRenderTarget(GetContext(), target, RenderTargetBindingParams());
    SetViewport(GetContext(), 0, 0, 4, 4);
    DisableState(GetContext(), STATE_DEPTH_TEST);
    TestProgram shader;
    shader.AddShaderStage(
        "RWTexture2D<uint> written:register(u0);"
        " float4 main(uint id:SV_VertexID):SV_Position {written[uint2(id,0)]=id+1;"
        "float2 p[3]={float2(-1,-1),float2(-1,3),float2(3,-1)};"
        "return float4(p[id],0.5,1);"
        "}", ShaderDesc::SHADER_TYPE_VERTEX);
    shader.AddShaderStage("float4 main():SV_Target{return float4(1,0,0,1);}", ShaderDesc::SHADER_TYPE_FRAGMENT);
    shader.SetRootSignature("DescriptorTable(UAV(u0), visibility=SHADER_VISIBILITY_VERTEX), RootFlags(ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT)");
    shader.AddBinding("written", 0, 0, ShaderDesc::SHADER_TYPE_UIMAGE2D, BINDING_TYPE_TEXTURE);
    HProgram program = shader.CreateProgram(GetContext());
    ASSERT_NE((HProgram)0, program);
    EnableTexture(GetContext(), 0, 0, written);
    EnableProgram(GetContext(), program);
    SetCullFace(GetContext(), FACE_TYPE_FRONT_AND_BACK);
    EnableState(GetContext(), STATE_CULL_FACE);
    Clear(GetContext(), BUFFER_TYPE_COLOR0_BIT, 0, 0, 0, 255, 1, 0);
    Draw(GetContext(), PRIMITIVE_TRIANGLES, 0, 3, 1);
    uint8_t pixels[4 * 4 * 4] = {};
    ReadPixels(GetContext(), 0, 0, 4, 4, pixels, sizeof(pixels));
    for (uint32_t i = 0; i < 16; ++i)
        ASSERT_EQ(0, pixels[i * 4 + 2]);
    dmArray<uint8_t> values;
    ReadTexture(written, 0, 0, values);
    for (uint32_t i = 0; i < 3; ++i)
        ASSERT_EQ(i + 1, ((uint32_t*)values.Begin())[i]);
    DeleteProgram(GetContext(), program);
    DeleteRenderTarget(GetContext(), target);
}

// Verifies dependent image dispatches and compute-to-draw transitions synchronize writes.
TEST_F(DX12Test, DependentImageDispatchesAndComputeToDraw)
{
    HTexture texture = CreateTestTexture(TEXTURE_TYPE_IMAGE_2D, 4, 4, 1, 1);
    uint32_t zero[16] = {};
    TextureParams params;
    params.m_Format = TEXTURE_FORMAT_R32UI;
    params.m_Width = params.m_Height = 4;
    params.m_Data = zero;
    SetTexture(GetContext(), texture, params);
    TestProgram compute;
    compute.AddShaderStage(
        "RWTexture2D<uint> image:register(u0);"
        " [numthreads(4,4,1)] void main(uint3 p:SV_DispatchThreadID){image[p.xy]=image[p.xy]+1;"
        "}", ShaderDesc::SHADER_TYPE_COMPUTE);
    compute.SetRootSignature("DescriptorTable(UAV(u0))");
    compute.AddBinding("image", 0, 0, ShaderDesc::SHADER_TYPE_UIMAGE2D, BINDING_TYPE_TEXTURE);
    HProgram compute_program = compute.CreateProgram(GetContext());
    ASSERT_NE((HProgram)0, compute_program);
    EnableProgram(GetContext(), compute_program);
    EnableTexture(GetContext(), 0, 0, texture);
    for (uint32_t i = 0; i < 3; ++i)
        DispatchCompute(GetContext(), 1, 1, 1);
    uint32_t changed = 8;
    params.m_Data = &changed;
    params.m_Width = params.m_Height = 1;
    params.m_SubUpdate = 1;
    SetTexture(GetContext(), texture, params);
    DispatchCompute(GetContext(), 1, 1, 1);
    TestProgram draw;
    draw.AddShaderStage(FULLSCREEN_VS, ShaderDesc::SHADER_TYPE_VERTEX);
    draw.AddShaderStage(
        "Texture2D<uint> image:register(t0);"
        " float4 main(float4 p:SV_Position):SV_Target {uint v=image.Load(int3(p.xy,0));"
        "return float4(v/10.0,0.9,0,1);"
        "}", ShaderDesc::SHADER_TYPE_FRAGMENT);
    draw.SetRootSignature("DescriptorTable(SRV(t0)), RootFlags(ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT)");
    draw.AddBinding("image", 0, 0, ShaderDesc::SHADER_TYPE_UTEXTURE2D, BINDING_TYPE_TEXTURE, 1);
    HProgram draw_program = draw.CreateProgram(GetContext());
    ASSERT_NE((HProgram)0, draw_program);
    HRenderTarget target = CreateTestRenderTarget(4, 4);
    SetRenderTarget(GetContext(), target, RenderTargetBindingParams());
    SetViewport(GetContext(), 0, 0, 4, 4);
    DisableState(GetContext(), STATE_CULL_FACE);
    DisableState(GetContext(), STATE_DEPTH_TEST);
    EnableProgram(GetContext(), draw_program);
    Draw(GetContext(), PRIMITIVE_TRIANGLES, 0, 3, 1);
    uint8_t pixels[64] = {};
    ReadPixels(GetContext(), 0, 0, 4, 4, pixels, sizeof(pixels));
    for (uint32_t i = 0; i < 16; ++i)
    {
        ASSERT_NEAR(i == 0 ? 230 : 102, pixels[i * 4 + 2], 1);
        ASSERT_NEAR(230, pixels[i * 4 + 1], 1);
    }
    SubmitAndWait();
    DeleteProgram(GetContext(), compute_program);
    DeleteProgram(GetContext(), draw_program);
    DeleteRenderTarget(GetContext(), target);
}

// Verifies volume sampling views and writable volume images address the correct voxels.
TEST_F(DX12Test, VolumeViewsAndWritableVolume)
{
    HTexture input = CreateTestTexture(TEXTURE_TYPE_3D, 4, 4, 4, 2);
    HTexture output = CreateTestTexture(TEXTURE_TYPE_IMAGE_3D, 2, 2, 2, 1);
    TextureParams params;
    params.m_Format = TEXTURE_FORMAT_R32UI;
    params.m_Width = params.m_Height = params.m_Depth = 4;
    uint32_t base[64] = {};
    params.m_Data = base;
    SetTexture(GetContext(), input, params);
    uint32_t mip[] = { 3, 3, 3, 3, 7, 7, 7, 7 };
    params.m_Data = mip;
    params.m_MipMap = 1;
    params.m_Width = params.m_Height = params.m_Depth = 2;
    SetTexture(GetContext(), input, params);
    params.m_MipMap = 0;
    params.m_Data = base;
    SetTexture(GetContext(), output, params);
    TestProgram compute;
    compute.AddShaderStage(
        "Texture3D<uint> source:register(t0);"
        " RWTexture3D<uint> target:register(u1);"
        " [numthreads(2,2,2)] void main(uint3 p:SV_DispatchThreadID){target[p]=source.Load(int4(p,1))*2;"
        "}", ShaderDesc::SHADER_TYPE_COMPUTE);
    compute.SetRootSignature("DescriptorTable(SRV(t0)), DescriptorTable(UAV(u1))");
    compute.AddBinding("source", 0, 0, ShaderDesc::SHADER_TYPE_UTEXTURE3D, BINDING_TYPE_TEXTURE);
    compute.AddBinding("target", 1, 1, ShaderDesc::SHADER_TYPE_UIMAGE3D, BINDING_TYPE_TEXTURE);
    HProgram program = compute.CreateProgram(GetContext());
    ASSERT_NE((HProgram)0, program);
    EnableTexture(GetContext(), 0, 0, input);
    EnableTexture(GetContext(), 1, 0, output);
    EnableProgram(GetContext(), program);
    for (uint32_t frame = 0; frame < 2; ++frame)
    {
        DispatchCompute(GetContext(), 1, 1, 1);
        dmArray<uint8_t> pixels;
        ReadTexture(output, 0, 0, pixels);
        for (uint32_t i = 0; i < 8; ++i)
            ASSERT_EQ(i < 4 ? 6 : 14, ((uint32_t*)pixels.Begin())[i]);
    }
    DeleteProgram(GetContext(), program);
}

// Verifies depth sampling and resized cubemap targets preserve per-face data.
TEST_F(DX12Test, DepthSamplingAndCubemapTargetResize)
{
    HRenderTarget depth = CreateTestRenderTarget(4, 4, BUFFER_TYPE_DEPTH_BIT);
    HRenderTarget color = CreateTestRenderTarget(4, 4, BUFFER_TYPE_COLOR0_BIT, 4);
    DX12RenderTarget* color_rt = GetAssetFromContainer<DX12RenderTarget>(m_Context->m_BaseContext.m_AssetHandleContainer, color);
    ASSERT_EQ(4u, color_rt->m_SampleDesc.Count);
    SetRenderTarget(GetContext(), depth, RenderTargetBindingParams());
    Clear(GetContext(), BUFFER_TYPE_DEPTH_BIT, 0, 0, 0, 0, 0.25f, 0);
    SetRenderTarget(GetContext(), color, RenderTargetBindingParams());
    TestProgram draw;
    draw.AddShaderStage(FULLSCREEN_VS, ShaderDesc::SHADER_TYPE_VERTEX);
    draw.AddShaderStage(
        "Texture2D<float> depth:register(t0);"
        " float4 main(float4 p:SV_Position):SV_Target {return float4(depth.Load(int3(p.xy,0)),0,0,1);"
        "}", ShaderDesc::SHADER_TYPE_FRAGMENT);
    draw.SetRootSignature("DescriptorTable(SRV(t0)), RootFlags(ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT)");
    draw.AddBinding("depth", 0, 0, ShaderDesc::SHADER_TYPE_TEXTURE2D, BINDING_TYPE_TEXTURE, 1);
    HProgram program = draw.CreateProgram(GetContext());
    ASSERT_NE((HProgram)0, program);
    EnableTexture(GetContext(), 0, 0, GetRenderTargetTexture(GetContext(), depth, BUFFER_TYPE_DEPTH_BIT));
    EnableProgram(GetContext(), program);
    SetViewport(GetContext(), 0, 0, 4, 4);
    DisableState(GetContext(), STATE_CULL_FACE);
    DisableState(GetContext(), STATE_DEPTH_TEST);
    Draw(GetContext(), PRIMITIVE_TRIANGLES, 0, 3, 1);
    uint8_t pixels[64];
    ReadPixels(GetContext(), 0, 0, 4, 4, pixels, sizeof(pixels));
    for (uint32_t i = 0; i < 16; ++i)
        ASSERT_NEAR(64, pixels[i * 4 + 2], 1);
    RenderTargetCreationParams params;
    params.m_TextureType = TEXTURE_TYPE_CUBE_MAP;
    params.m_ColorBufferCreationParams[0].m_Type = TEXTURE_TYPE_CUBE_MAP;
    params.m_ColorBufferCreationParams[0].m_Width = params.m_ColorBufferCreationParams[0].m_Height = 4;
    params.m_ColorBufferParams[0].m_Width = params.m_ColorBufferParams[0].m_Height = 4;
    params.m_ColorBufferParams[0].m_Format = TEXTURE_FORMAT_RGBA;
    HRenderTarget cube = NewRenderTarget(GetContext(), BUFFER_TYPE_COLOR0_BIT, params);
    for (uint32_t size = 4; size <= 8; size += 4)
    {
        if (size == 8)
            SetRenderTargetSize(GetContext(), cube, size, size);
        for (uint32_t face = 0; face < 6; ++face)
        {
            RenderTargetBindingParams binding;
            binding.m_CubeMapFace = (CubeMapFace)face;
            SetRenderTarget(GetContext(), cube, binding);
            Clear(GetContext(), BUFFER_TYPE_COLOR0_BIT, 20 + face, 0, 0, 255, 1, 0);
        }
        SetRenderTarget(GetContext(), color, RenderTargetBindingParams());
        for (uint32_t face = 0; face < 6; ++face)
        {
            dmArray<uint8_t> bytes;
            ReadTexture(GetRenderTargetTexture(GetContext(), cube, BUFFER_TYPE_COLOR0_BIT), 0, face, bytes);
            for (uint32_t i = 0; i < size * size; ++i)
                ASSERT_EQ(20 + face, bytes[i * 4]);
        }
    }
    DeleteProgram(GetContext(), program);
    DeleteRenderTarget(GetContext(), cube);
    DeleteRenderTarget(GetContext(), color);
    DeleteRenderTarget(GetContext(), depth);
}

// Verifies upload and pipeline reuse workloads complete without validation errors.
TEST_F(DX12Test, UploadAndPipelineBenchmark)
{
    // Opt in after correctness tests. Report CPU submission cost; this is not a GPU throughput measurement.
    if (!getenv("DEFOLD_DX12_BENCHMARK"))
        return;
    HRenderTarget target = CreateTestRenderTarget(4, 4);
    SetRenderTarget(GetContext(), target, RenderTargetBindingParams());
    TestProgram shader;
    shader.AddShaderStage(FULLSCREEN_VS, ShaderDesc::SHADER_TYPE_VERTEX);
    shader.AddShaderStage("float4 main():SV_Target{return float4(1,0,0,1);}", ShaderDesc::SHADER_TYPE_FRAGMENT);
    shader.SetRootSignature("RootFlags(ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT)");
    HProgram program = shader.CreateProgram(GetContext());
    EnableProgram(GetContext(), program);
    SetViewport(GetContext(), 0, 0, 4, 4);
    DisableState(GetContext(), STATE_DEPTH_TEST);
    DisableState(GetContext(), STATE_CULL_FACE);
    uint64_t start = dmTime::GetMonotonicTime();
    Draw(GetContext(), PRIMITIVE_TRIANGLES, 0, 3, 1);
    uint64_t cold = dmTime::GetMonotonicTime() - start;
    float vertices[16] = {};
    HVertexBuffer buffer = NewVertexBuffer(GetContext(), sizeof(vertices), vertices, BUFFER_USAGE_DYNAMIC_DRAW);
    const uint32_t count = 1000;
    start = dmTime::GetMonotonicTime();
    for (uint32_t i = 0; i < count; ++i)
        SetVertexBufferData(buffer, sizeof(vertices), vertices, BUFFER_USAGE_DYNAMIC_DRAW);
    uint64_t uploads = dmTime::GetMonotonicTime() - start;
    start = dmTime::GetMonotonicTime();
    for (uint32_t i = 0; i < count; ++i)
        Draw(GetContext(), PRIMITIVE_TRIANGLES, 0, 3, 1);
    uint64_t draws = dmTime::GetMonotonicTime() - start;
    printf("DX12 WARP CPU benchmark: cold draw=%llu us, %u uploads=%llu us, %u cached draws=%llu us\n", (unsigned long long)cold, count, (unsigned long long)uploads, count, (unsigned long long)draws);
    SubmitAndWait();
    DeleteVertexBuffer(buffer);
    DeleteProgram(GetContext(), program);
    DeleteRenderTarget(GetContext(), target);
}

// Verifies sampler modes and supported format capabilities match DX12 settings.
TEST_F(DX12Test, SamplerModesAndFormatCapabilities)
{
    SetupSupportedTextureFormats(m_Context);
    ASSERT_TRUE(IsTextureFormatSupported(GetContext(), TEXTURE_FORMAT_RGB_BC1));
    ASSERT_FALSE(IsTextureFormatSupported(GetContext(), TEXTURE_FORMAT_RGBA_ASTC_4X4));
    ASSERT_FALSE(IsExtensionSupported(GetContext(), "invented-extension"));
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
    GetRegisteredAdapter(0)->m_IsSupportedCb = IsTestAdapterSupported;
    if (!InstallAdapter(ADAPTER_FAMILY_DIRECTX))
        return 1;
    jc_test_init(&argc, argv);
    return jc_test_run_all();
}
