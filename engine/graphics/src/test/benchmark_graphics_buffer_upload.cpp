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


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dlib/time.h>
#include <platform/window.hpp>
#include <testmain/testmain.h>
#include "test_app_graphics.h"
#include "../dx12/graphics_dx12_private.h"
#include "../vulkan/graphics_vulkan_private.h"
#include <d3d12sdklayers.h>
#include <GL/gl.h>
#include "../opengl/win32/glext.h"
#include "buffer_upload_vert.h"
#include "buffer_upload_frag.h"
#include "buffer_upload_gl_vert.h"
#include "buffer_upload_gl_frag.h"
#include "buffer_upload_dxil_vert.h"
#include "buffer_upload_dxil_frag.h"

using namespace dmGraphics;
static const uint32_t SLOT_COUNT = 3;

static void Require(bool condition, const char* message)
{
    if (!condition)
    {
        fprintf(stderr, "Buffer upload benchmark failed: %s\n", message);
        exit(1);
    }
}

// All query results are collected after the final drain, without per-frame query waits.
struct Timers
{
    HContext                     m_Context;
    AdapterFamily                m_Backend;
    uint32_t                     m_Count;
    uint64_t*                    m_Values;
    double                       m_MicrosecondsPerTick;
    uint64_t                     m_Mask;
    VkQueryPool                  m_VulkanPool;
    PFN_vkCmdWriteTimestamp      m_VulkanWrite;
    ID3D12QueryHeap*             m_DX12Heap;
    ID3D12Resource*              m_DX12Readback;
    GLuint*                      m_GLQueries;
    PFNGLGENQUERIESPROC          m_GLGen;
    PFNGLDELETEQUERIESPROC       m_GLDelete;
    PFNGLQUERYCOUNTERPROC        m_GLCounter;
    PFNGLGETQUERYOBJECTUI64VPROC m_GLGet;

    void Init(HContext context, AdapterFamily backend, uint32_t frames)
    {
        m_Context = context;
        m_Backend = backend;
        m_Count = frames * 3;
        m_Values = (uint64_t*) calloc(m_Count, sizeof(uint64_t));
        Require(m_Values != 0, "allocate query results");
        m_Mask = UINT64_MAX;
        if (backend == ADAPTER_FAMILY_VULKAN)
        {
            VulkanContext* vk = (VulkanContext*) context;
            VkDevice device = vk->m_LogicalDevice.m_Device;
            m_VulkanWrite = (PFN_vkCmdWriteTimestamp) vkGetDeviceProcAddr(device, "vkCmdWriteTimestamp");
            Require(m_VulkanWrite != 0, "load Vulkan timestamp command");
            QueueFamily family = GetGraphicsQueueFamily(&vk->m_PhysicalDevice);
            uint32_t bits = vk->m_PhysicalDevice.m_QueueFamilyProperties[family.m_GraphicsQueueIx].timestampValidBits;
            Require(bits != 0, "Vulkan timestamp support");
            m_Mask = bits == 64 ? UINT64_MAX : (uint64_t(1) << bits) - 1;
            m_MicrosecondsPerTick = vk->m_PhysicalDevice.m_Properties.limits.timestampPeriod / 1000.0;
            VkQueryPoolCreateInfo info = {};
            info.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;
            info.queryType = VK_QUERY_TYPE_TIMESTAMP;
            info.queryCount = m_Count;
            Require(vkCreateQueryPool(device, &info, 0, &m_VulkanPool) == VK_SUCCESS, "create Vulkan query pool");
        }
        else if (backend == ADAPTER_FAMILY_DIRECTX)
        {
            DX12Context* dx = (DX12Context*) context;
            UINT64 frequency = 0;
            Require(SUCCEEDED(dx->m_CommandQueue->GetTimestampFrequency(&frequency)) && frequency, "DX12 timestamp frequency");
            m_MicrosecondsPerTick = 1000000.0 / frequency;
            D3D12_QUERY_HEAP_DESC query = {};
            query.Count = m_Count;
            query.Type = D3D12_QUERY_HEAP_TYPE_TIMESTAMP;
            Require(SUCCEEDED(dx->m_Device->CreateQueryHeap(&query, IID_PPV_ARGS(&m_DX12Heap))), "create DX12 query heap");
            D3D12_HEAP_PROPERTIES heap = {};
            heap.Type = D3D12_HEAP_TYPE_READBACK;
            D3D12_RESOURCE_DESC buffer = CD3DX12_RESOURCE_DESC::Buffer(m_Count * sizeof(uint64_t));
            Require(SUCCEEDED(dx->m_Device->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &buffer,
                                                                    D3D12_RESOURCE_STATE_COPY_DEST, 0, IID_PPV_ARGS(&m_DX12Readback))), "create DX12 query readback");
        }
        else
        {
            m_GLGen = (PFNGLGENQUERIESPROC) wglGetProcAddress("glGenQueries");
            m_GLDelete = (PFNGLDELETEQUERIESPROC) wglGetProcAddress("glDeleteQueries");
            m_GLCounter = (PFNGLQUERYCOUNTERPROC) wglGetProcAddress("glQueryCounter");
            m_GLGet = (PFNGLGETQUERYOBJECTUI64VPROC) wglGetProcAddress("glGetQueryObjectui64v");
            Require(m_GLGen && m_GLDelete && m_GLCounter && m_GLGet, "OpenGL timestamp support");
            m_GLQueries = (GLuint*) calloc(m_Count, sizeof(GLuint));
            Require(m_GLQueries != 0, "allocate OpenGL queries");
            m_GLGen(m_Count, m_GLQueries);
            m_MicrosecondsPerTick = 0.001;
        }
    }

    void Write(uint32_t frame, uint32_t point)
    {
        uint32_t query = frame * 3 + point;
        if (m_Backend == ADAPTER_FAMILY_VULKAN)
        {
            VulkanContext* vk = (VulkanContext*) m_Context;
            VkCommandBuffer cmd = vk->m_MainCommandBuffers[vk->m_CurrentFrameInFlight];
            if (point == 0)
            {
                vkCmdResetQueryPool(cmd, m_VulkanPool, query, 3);
            }

            m_VulkanWrite(cmd, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, m_VulkanPool, query);
        }
        else if (m_Backend == ADAPTER_FAMILY_DIRECTX)
        {
            DX12Context* dx = (DX12Context*) m_Context;
            dx->m_CommandList->EndQuery(m_DX12Heap, D3D12_QUERY_TYPE_TIMESTAMP, query);
            if (point == 2)
            {
                dx->m_CommandList->ResolveQueryData(m_DX12Heap, D3D12_QUERY_TYPE_TIMESTAMP,
                                                    frame * 3, 3, m_DX12Readback, frame * 3 * sizeof(uint64_t));
            }
        }
        else
        {
            m_GLCounter(m_GLQueries[query], GL_TIMESTAMP);
        }
    }

    void Drain()
    {
        if (m_Backend == ADAPTER_FAMILY_VULKAN)
        {
            Require(vkDeviceWaitIdle(((VulkanContext*) m_Context)->m_LogicalDevice.m_Device) == VK_SUCCESS, "drain Vulkan queue");
        }
        else if (m_Backend == ADAPTER_FAMILY_DIRECTX)
        {
            WaitForDX12Idle((DX12Context*) m_Context);
        }
        else
        {
            glFinish();
        }
    }

    void Collect()
    {
        if (m_Backend == ADAPTER_FAMILY_VULKAN)
        {
            Require(vkGetQueryPoolResults(((VulkanContext*) m_Context)->m_LogicalDevice.m_Device, m_VulkanPool, 0, m_Count,
                                          m_Count * sizeof(uint64_t), m_Values, sizeof(uint64_t), VK_QUERY_RESULT_64_BIT) == VK_SUCCESS, "read Vulkan queries");
        }
        else if (m_Backend == ADAPTER_FAMILY_DIRECTX)
        {
            void* data = 0;
            D3D12_RANGE read = { 0, m_Count* sizeof(uint64_t) };
            Require(SUCCEEDED(m_DX12Readback->Map(0, &read, &data)), "map DX12 query results");
            memcpy(m_Values, data, read.End);
            D3D12_RANGE written = { 0, 0 };
            m_DX12Readback->Unmap(0, &written);
        }
        else
        {
            for (uint32_t i = 0; i < m_Count; ++i)
            {
                m_GLGet(m_GLQueries[i], GL_QUERY_RESULT, &m_Values[i]);
            }

            Require(glGetError() == GL_NO_ERROR, "read OpenGL queries");
        }
    }

    double Elapsed(uint32_t frame, uint32_t begin)
    {
        return double((m_Values[frame * 3 + 2] - m_Values[frame * 3 + begin]) & m_Mask) * m_MicrosecondsPerTick;
    }

    void Delete()
    {
        if (m_VulkanPool)
        {
            vkDestroyQueryPool(((VulkanContext*) m_Context)->m_LogicalDevice.m_Device, m_VulkanPool, 0);
        }

        if (m_DX12Heap)
        {
            m_DX12Heap->Release();
        }

        if (m_DX12Readback)
        {
            m_DX12Readback->Release();
        }

        if (m_GLQueries)
        {
            m_GLDelete(m_Count, m_GLQueries);
        }

        free(m_GLQueries);
        free(m_Values);
    }

};

struct Slot
{
    HVertexBuffer m_Vertices;
    HIndexBuffer  m_Indices;
    uint32_t      m_LastUpload;
    bool          m_Initialized;
    bool          m_White;
};

struct Benchmark
{
    HWindow            m_Window;
    HContext           m_Context;
    HJobContext        m_Jobs;
    HProgram           m_Program;
    HVertexDeclaration m_Declaration;
    HRenderTarget      m_Target;
    AdapterFamily      m_Backend;
    char               m_GPU[256];
    Slot               m_Slots[SLOT_COUNT];
    Timers             m_Timers;

    void Init(const char* backend, bool verify, uint32_t frames)
    {
        m_Backend = strcmp(backend, "vulkan") == 0 ? ADAPTER_FAMILY_VULKAN :
                    strcmp(backend, "dx12") == 0 ? ADAPTER_FAMILY_DIRECTX : ADAPTER_FAMILY_OPENGL;
        Require(m_Backend != ADAPTER_FAMILY_OPENGL || strcmp(backend, "opengl") == 0, "valid backend");
        Require(InstallAdapter(m_Backend) == RESULT_OK, "install graphics backend");
        m_Window = dmPlatform::NewWindow();
        WindowCreateParams window;
        WindowCreateParamsInitialize(&window);
        window.m_Width = 64;
        window.m_Height = 64;
        window.m_Samples = 1;
        window.m_Hidden = 1;
        window.m_FocusOnShow = 0;
        window.m_Title = "Graphics buffer upload benchmark";
        window.m_GraphicsApi = m_Backend == ADAPTER_FAMILY_VULKAN ? WINDOW_GRAPHICS_API_VULKAN :
                               m_Backend == ADAPTER_FAMILY_DIRECTX ? WINDOW_GRAPHICS_API_DIRECTX : WINDOW_GRAPHICS_API_OPENGL;
        if (m_Backend == ADAPTER_FAMILY_OPENGL)
        {
            window.m_GraphicsApiVersionHint = 45;
        }

        Require(dmPlatform::OpenWindow(m_Window, window) == WINDOW_RESULT_OK, "open hidden window");
        JobSystemCreateParams jobs = {};
        jobs.m_ThreadCount = 1;
        m_Jobs = JobSystemCreate(&jobs);
        ContextParams params;
        params.m_Window = m_Window;
        params.m_Width = 64;
        params.m_Height = 64;
        params.m_JobContext = m_Jobs;
        params.m_VerifyGraphicsCalls = verify;
        params.m_UseValidationLayers = verify;
        m_Context = NewContext(params);
        Require(m_Context != 0, "create graphics context");
        SetSwapInterval(m_Context, 0);
        ShaderDesc desc = {};
        ID3DBlob* root_blob = 0;
        if (m_Backend == ADAPTER_FAMILY_VULKAN)
        {
            AddShader(&desc, ShaderDesc::LANGUAGE_SPIRV, ShaderDesc::SHADER_TYPE_VERTEX, buffer_upload_vert, buffer_upload_vert_SIZE);
            AddShader(&desc, ShaderDesc::LANGUAGE_SPIRV, ShaderDesc::SHADER_TYPE_FRAGMENT, buffer_upload_frag, buffer_upload_frag_SIZE);
            snprintf(m_GPU, sizeof(m_GPU), "%s", ((VulkanContext*) m_Context)->m_PhysicalDevice.m_Properties.deviceName);
        }
        else if (m_Backend == ADAPTER_FAMILY_DIRECTX)
        {
            AddShader(&desc, ShaderDesc::LANGUAGE_HLSL_60, ShaderDesc::SHADER_TYPE_VERTEX, buffer_upload_dxil_vert, buffer_upload_dxil_vert_SIZE);
            AddShader(&desc, ShaderDesc::LANGUAGE_HLSL_60, ShaderDesc::SHADER_TYPE_FRAGMENT, buffer_upload_dxil_frag, buffer_upload_dxil_frag_SIZE);
            D3D12_ROOT_SIGNATURE_DESC root = {};
            root.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
            Require(SUCCEEDED(D3D12SerializeRootSignature(&root, D3D_ROOT_SIGNATURE_VERSION_1, &root_blob, 0)), "serialize DX12 root signature");
            desc.m_HlslRootSignature.m_Data = (uint8_t*) root_blob->GetBufferPointer();
            desc.m_HlslRootSignature.m_Count = (uint32_t) root_blob->GetBufferSize();
            IDXGIFactory4* factory = 0;
            IDXGIAdapter1* adapter = 0;
            Require(SUCCEEDED(CreateDXGIFactory1(IID_PPV_ARGS(&factory))), "query DX12 adapter factory");
            Require(SUCCEEDED(factory->EnumAdapterByLuid(((DX12Context*) m_Context)->m_Device->GetAdapterLuid(), IID_PPV_ARGS(&adapter))), "query DX12 adapter");
            DXGI_ADAPTER_DESC1 adapter_desc = {};
            Require(SUCCEEDED(adapter->GetDesc1(&adapter_desc)), "query DX12 adapter description");
            WideCharToMultiByte(CP_UTF8, 0, adapter_desc.Description, -1, m_GPU, sizeof(m_GPU), 0, 0);
            adapter->Release();
            factory->Release();
        }
        else
        {
            AddShader(&desc, ShaderDesc::LANGUAGE_GLSL_SM330, ShaderDesc::SHADER_TYPE_VERTEX, buffer_upload_gl_vert, buffer_upload_gl_vert_SIZE);
            AddShader(&desc, ShaderDesc::LANGUAGE_GLSL_SM330, ShaderDesc::SHADER_TYPE_FRAGMENT, buffer_upload_gl_frag, buffer_upload_gl_frag_SIZE);
            snprintf(m_GPU, sizeof(m_GPU), "%s", (const char*) glGetString(GL_RENDERER));
        }

        AddShaderResource(&desc, "position", ShaderDesc::SHADER_TYPE_VEC4, 0, 0, BINDING_TYPE_INPUT, SHADER_STAGE_FLAG_VERTEX);
        AddShaderResource(&desc, "offset", ShaderDesc::SHADER_TYPE_VEC4, 1, 0, BINDING_TYPE_INPUT, SHADER_STAGE_FLAG_VERTEX);
        char error[2048] = {};
        m_Program = NewProgram(m_Context, &desc, error, sizeof(error));
        Require(m_Program != 0, error);
        DeleteShaderDesc(&desc);
        if (root_blob)
        {
            root_blob->Release();
        }

        HVertexStreamDeclaration streams = NewVertexStreamDeclaration(m_Context);
        AddVertexStream(streams, "position", 4, TYPE_FLOAT, false);
        AddVertexStream(streams, "offset", 4, TYPE_FLOAT, false);
        m_Declaration = NewVertexDeclaration(m_Context, streams);
        DeleteVertexStreamDeclaration(streams);
        RenderTargetCreationParams target = {};
        target.m_SampleCount = 1;
        target.m_ColorBufferCreationParams[0].m_Width = 64;
        target.m_ColorBufferCreationParams[0].m_Height = 64;
        target.m_ColorBufferParams[0].m_Width = 64;
        target.m_ColorBufferParams[0].m_Height = 64;
        target.m_ColorBufferParams[0].m_Format = TEXTURE_FORMAT_RGBA;
        target.m_ColorBufferLoadOps[0] = ATTACHMENT_OP_LOAD;
        target.m_ColorBufferStoreOps[0] = ATTACHMENT_OP_STORE;
        m_Target = NewRenderTarget(m_Context, BUFFER_TYPE_COLOR0_BIT, target);
        Require(m_Target != 0, "create render target");
        m_Timers.Init(m_Context, m_Backend, frames);
        for (uint32_t i = 0; i < SLOT_COUNT; ++i)
        {
            m_Slots[i].m_Vertices = NewVertexBuffer(m_Context, 0, 0, BUFFER_USAGE_STREAM_DRAW);
            m_Slots[i].m_Indices = NewIndexBuffer(m_Context, 0, 0, BUFFER_USAGE_STREAM_DRAW);
            Require(m_Slots[i].m_Vertices && m_Slots[i].m_Indices, "create vertex/index handles");
        }
    }

    void Draw(Slot& slot, uint32_t index_count, uint32_t draws, bool verify)
    {
        SetRenderTarget(m_Context, m_Target, RenderTargetBindingParams());
        Clear(m_Context, BUFFER_TYPE_COLOR0_BIT, 0, 0, 0, 255, 1, 0);
        EnableProgram(m_Context, m_Program);
        DisableState(m_Context, STATE_BLEND);
        DisableState(m_Context, STATE_DEPTH_TEST);
        DisableState(m_Context, STATE_CULL_FACE);
        SetViewport(m_Context, 0, 0, 64, 64);
        EnableVertexBuffer(m_Context, (HVertexBuffer) slot.m_Vertices, 0);
        EnableVertexDeclaration(m_Context, m_Declaration, 0, 0, m_Program);
        for (uint32_t i = 0; i < draws; ++i)
        {
            DrawElements(m_Context, PRIMITIVE_TRIANGLES, 0, index_count, TYPE_UNSIGNED_INT, (HIndexBuffer) slot.m_Indices, 1);
        }

        if (verify)
        {
            uint8_t pixels[64 * 64 * 4];
            ReadPixels(m_Context, 0, 0, 64, 64, pixels, sizeof(pixels));
            const uint8_t* pixel = pixels + (32 * 64 + 32) * 4;
            uint8_t expected = slot.m_White ? 255 : 0;
            Require(pixel[0] == expected && pixel[1] == expected && pixel[2] == expected && pixel[3] == 255,
                    "rendered vertex/index contents match the latest upload");
        }
    }


    void CheckValidation(bool verify)
    {
        if (verify && m_Backend == ADAPTER_FAMILY_DIRECTX)
        {
            ID3D12InfoQueue* info = 0;
            Require(SUCCEEDED(((DX12Context*) m_Context)->m_Device->QueryInterface(IID_PPV_ARGS(&info))), "DX12 validation enabled");
            bool valid = true;
            for (UINT64 i = 0; i < info->GetNumStoredMessages(); ++i)
            {
                SIZE_T size = 0;
                info->GetMessage(i, 0, &size);
                D3D12_MESSAGE* message = (D3D12_MESSAGE*) malloc(size);
                Require(SUCCEEDED(info->GetMessage(i, message, &size)), "read DX12 validation message");
                if (message->Severity <= D3D12_MESSAGE_SEVERITY_ERROR)
                {
                    fprintf(stderr, "DX12 validation error: %s\n", message->pDescription);
                    valid = false;
                }

                free(message);
            }

            info->Release();
            Require(valid, "DX12 validation checks");
        }
    }

    void Delete()
    {
        m_Timers.Delete();
        for (uint32_t i = 0; i < SLOT_COUNT; ++i)
        {
            DeleteVertexBuffer(m_Slots[i].m_Vertices);
            DeleteIndexBuffer(m_Slots[i].m_Indices);
        }

        DeleteVertexDeclaration(m_Declaration);
        DeleteProgram(m_Context, m_Program);
        DeleteRenderTarget(m_Context, m_Target);
        CloseWindow(m_Context);
        DeleteContext(m_Context);
        Finalize();
        JobSystemDestroy(m_Jobs);
        dmPlatform::CloseWindow(m_Window);
        dmPlatform::DeleteWindow(m_Window);
    }

};

static int CompareDouble(const void* a, const void* b)
{
    double x = *(const double*) a;
    double y = *(const double*) b;
    return (x > y) - (x < y);
}

extern "C" void dmExportedSymbols();
int main(int argc, char** argv)
{
    if (argc != 8)
    {
        fprintf(stderr, "Usage: %s vulkan|dx12|opengl vertex_KiB upload_interval draws warmup_frames measured_frames verify\n", argv[0]);
        return 1;
    }

    uint32_t vertex_count = (uint32_t) strtoul(argv[2], 0, 10) * 1024 / (8 * sizeof(float));
    vertex_count -= vertex_count % 3;
    uint32_t interval = (uint32_t) strtoul(argv[3], 0, 10);
    uint32_t draws = (uint32_t) strtoul(argv[4], 0, 10);
    uint32_t warmup = (uint32_t) strtoul(argv[5], 0, 10);
    uint32_t frames = (uint32_t) strtoul(argv[6], 0, 10);
    bool verify = atoi(argv[7]) != 0;
    bool serial = getenv("DEFOLD_BUFFER_BENCHMARK_SERIAL") != 0;
    Require(vertex_count >= 6 && frames > 0 && warmup >= SLOT_COUNT && draws > 0, "valid workload");
    uint32_t vertex_bytes = vertex_count * 8 * sizeof(float);
    uint32_t index_bytes = vertex_count * sizeof(uint32_t);
    float* vertices = (float*) calloc(vertex_count * 8, sizeof(float));
    uint32_t* indices = (uint32_t*) malloc(index_bytes);
    double* gpu_times = (double*) calloc(frames, sizeof(double));
    Require(vertices && indices && gpu_times, "allocate CPU data");
    for (uint32_t i = 0; i < vertex_count; ++i)
    {
        // All but the first triangle are clipped, keeping this vertex-fetch-bound.
        vertices[i * 8] = 4.0f + (i % 3);
        vertices[i * 8 + 1] = -1.0f + (i % 3);
        vertices[i * 8 + 2] = 0.25f;
        vertices[i * 8 + 3] = 0.5f;
        vertices[i * 8 + 6] = 0.25f;
        vertices[i * 8 + 7] = 0.5f;
        indices[i] = i;
    }


    dmExportedSymbols();
    TestMainPlatformInit();
    Benchmark benchmark = {};
    benchmark.Init(argv[1], verify, frames);
    double upload_us = 0;
    double begin_us = 0;
    uint32_t frame_slot_mask = 0;
    uint32_t uploads = 0;
    uint64_t start = 0;
    for (uint32_t frame = 0; frame < warmup + frames; ++frame)
    {
        if (frame == warmup)
        {
            benchmark.m_Timers.Drain();
            start = dmTime::GetMonotonicTime();
        }

        uint64_t begin_start = dmTime::GetMonotonicTime();
        BeginFrame(benchmark.m_Context);
        if (frame >= warmup)
        {
            begin_us += double(dmTime::GetMonotonicTime() - begin_start);
            if (benchmark.m_Backend == ADAPTER_FAMILY_VULKAN)
            {
                frame_slot_mask |= 1u << ((VulkanContext*) benchmark.m_Context)->m_CurrentFrameInFlight;
            }
            else if (benchmark.m_Backend == ADAPTER_FAMILY_DIRECTX)
            {
                frame_slot_mask |= 1u << ((DX12Context*) benchmark.m_Context)->m_CurrentFrameIndex;
            }
        }

        Slot& slot = benchmark.m_Slots[frame % SLOT_COUNT];
        if (frame >= warmup)
        {
            benchmark.m_Timers.Write(frame - warmup, 0);
        }

        if (!slot.m_Initialized || (interval && frame / interval != slot.m_LastUpload / interval))
        {
            bool visible_vertices = !interval || (frame & 1) == 0;
            bool visible_indices = !interval || (frame & 2) == 0;
            const float triangle[] = { -1, -1, 3, -1, -1, 3 };
            for (uint32_t i = 0; i < 3; ++i)
            {
                vertices[i * 8] = triangle[i * 2] + (visible_vertices ? 0.0f : 8.0f);
                vertices[i * 8 + 1] = triangle[i * 2 + 1];
                indices[i] = i + (visible_indices ? 0 : 3);
            }

            uint64_t upload_start = dmTime::GetMonotonicTime();
            BufferUsage usage = interval ? BUFFER_USAGE_STREAM_DRAW : BUFFER_USAGE_STATIC_DRAW;
            SetVertexBufferData(slot.m_Vertices, vertex_bytes, vertices, usage);
            SetIndexBufferData(slot.m_Indices, index_bytes, indices, usage);
            if (frame >= warmup)
            {
                upload_us += double(dmTime::GetMonotonicTime() - upload_start);
                ++uploads;
            }

            slot.m_White = visible_vertices && visible_indices;
            slot.m_Initialized = true;
            slot.m_LastUpload = frame;
        }

        if (frame >= warmup)
        {
            benchmark.m_Timers.Write(frame - warmup, 1);
        }

        benchmark.Draw(slot, vertex_count, draws, verify);
        if (frame >= warmup)
        {
            benchmark.m_Timers.Write(frame - warmup, 2);
        }

        DisableVertexDeclaration(benchmark.m_Context, benchmark.m_Declaration);
        DisableProgram(benchmark.m_Context);
        SetRenderTarget(benchmark.m_Context, 0, RenderTargetBindingParams());
        Flip(benchmark.m_Context);
        if (serial)
        {
            benchmark.m_Timers.Drain();
        }
    }

    benchmark.m_Timers.Drain();
    double wall_us = double(dmTime::GetMonotonicTime() - start) / frames;
    benchmark.m_Timers.Collect();
    double gpu_sum = 0;
    double draw_sum = 0;
    for (uint32_t i = 0; i < frames; ++i)
    {
        gpu_times[i] = benchmark.m_Timers.Elapsed(i, 0);
        Require(gpu_times[i] > 0, "all measured frames have GPU timestamps");
        gpu_sum += gpu_times[i];
        draw_sum += benchmark.m_Timers.Elapsed(i, 1);
    }

    qsort(gpu_times, frames, sizeof(double), CompareDouble);
    benchmark.CheckValidation(verify);
    printf("RESULT {\"mode\":\"%s\",\"gpu\":\"%s\",\"vertex_bytes\":%u,\"index_bytes\":%u,"
           "\"interval\":%u,\"draws\":%u,\"frames\":%u,\"warmup\":%u,\"verify\":%s,\"slots\":%u,"
           "\"uploads\":%u,\"upload_cpu_us\":%.3f,\"gpu_mean_us\":%.3f,\"gpu_median_us\":%.3f,"
           "\"gpu_p95_us\":%.3f,\"draw_gpu_mean_us\":%.3f,\"wall_us\":%.3f,"
           "\"serial\":%s,\"frame_slot_mask\":%u,\"begin_cpu_us\":%.3f}\n",
           argv[1], benchmark.m_GPU, vertex_bytes, index_bytes, interval, draws, frames, warmup,
           verify ? "true" : "false", SLOT_COUNT, uploads, uploads ? upload_us / uploads : 0.0,
           gpu_sum / frames, gpu_times[frames / 2], gpu_times[(frames - 1) * 95 / 100], draw_sum / frames, wall_us,
           serial ? "true" : "false", frame_slot_mask, begin_us / frames);
    benchmark.Delete();
    free(vertices);
    free(indices);
    free(gpu_times);
    return 0;
}
