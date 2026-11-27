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
#include "../vulkan/graphics_vulkan_private.h"
#include "buffer_upload_vert.h"
#include "buffer_upload_frag.h"

using namespace dmGraphics;

static void Require(bool condition, const char* message)
{
    if (!condition)
    {
        fprintf(stderr, "Buffer upload benchmark failed: %s\n", message);
        exit(1);
    }
}

static void Check(VkResult result)
{
    if (result != VK_SUCCESS)
    {
        fprintf(stderr, "Buffer upload benchmark failed: VkResult %d\n", result);
        exit(1);
    }
}

enum UploadMode
{
    MODE_HOST, MODE_BAR, MODE_STAGED
};

struct Slot
{
    DeviceBuffer*          m_Vertices;
    DeviceBuffer*          m_Indices;
    DeviceBufferAllocation m_Staging;
    uint32_t               m_LastFrame;
    uint32_t               m_LastUpload;
    bool                   m_Initialized;
    bool                   m_PendingQuery;
    bool                   m_White;
};

struct Benchmark
{
    HWindow                          m_Window;
    HContext                         m_Context;
    HJobContext                      m_Jobs;
    HProgram                         m_Program;
    HVertexDeclaration               m_Declaration;
    HRenderTarget                    m_Target;
    VulkanContext*                   m_Vulkan;
    VkQueryPool                      m_Queries;
    PFN_vkCmdWriteTimestamp          m_WriteTimestamp;
    VkPhysicalDeviceMemoryProperties m_Memory;
    uint64_t                         m_TimestampMask;
    Slot                             m_Slots[DM_MAX_FRAMES_IN_FLIGHT];

    void Init(bool verify)
    {
        Require(InstallAdapter(ADAPTER_FAMILY_VULKAN) == RESULT_OK, "install Vulkan");
        m_Window = dmPlatform::NewWindow();
        WindowCreateParams window;
        WindowCreateParamsInitialize(&window);
        window.m_Width = 64;
        window.m_Height = 64;
        window.m_Samples = 1;
        window.m_Hidden = 1;
        window.m_FocusOnShow = 0;
        window.m_Title = "Vulkan buffer upload benchmark";
        window.m_GraphicsApi = WINDOW_GRAPHICS_API_VULKAN;
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
        Require(m_Context != 0, "create context");
        m_Vulkan = (VulkanContext*) m_Context;
        SetSwapInterval(m_Context, 0);
        VkDevice device = m_Vulkan->m_LogicalDevice.m_Device;
        m_WriteTimestamp = (PFN_vkCmdWriteTimestamp) vkGetDeviceProcAddr(device, "vkCmdWriteTimestamp");
        Require(m_WriteTimestamp != 0, "load timestamp command");
        QueueFamily family = GetGraphicsQueueFamily(&m_Vulkan->m_PhysicalDevice);
        uint32_t bits = m_Vulkan->m_PhysicalDevice.m_QueueFamilyProperties[family.m_GraphicsQueueIx].timestampValidBits;
        Require(bits > 0, "graphics queue supports timestamps");
        m_TimestampMask = bits == 64 ? UINT64_MAX : (uint64_t(1) << bits) - 1;
        vkGetPhysicalDeviceMemoryProperties(m_Vulkan->m_PhysicalDevice.m_Device, &m_Memory);
        VkQueryPoolCreateInfo query = {};
        query.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;
        query.queryType = VK_QUERY_TYPE_TIMESTAMP;
        query.queryCount = 2 * DM_MAX_FRAMES_IN_FLIGHT;
        Check(vkCreateQueryPool(device, &query, 0, &m_Queries));

        ShaderDesc desc = {};
        AddShader(&desc, ShaderDesc::LANGUAGE_SPIRV, ShaderDesc::SHADER_TYPE_VERTEX, buffer_upload_vert, buffer_upload_vert_SIZE);
        AddShader(&desc, ShaderDesc::LANGUAGE_SPIRV, ShaderDesc::SHADER_TYPE_FRAGMENT, buffer_upload_frag, buffer_upload_frag_SIZE);
        AddShaderResource(&desc, "position", ShaderDesc::SHADER_TYPE_VEC4, 0, 0, BINDING_TYPE_INPUT, SHADER_STAGE_FLAG_VERTEX);
        AddShaderResource(&desc, "offset", ShaderDesc::SHADER_TYPE_VEC4, 1, 0, BINDING_TYPE_INPUT, SHADER_STAGE_FLAG_VERTEX);
        char error[2048] = {};
        m_Program = NewProgram(m_Context, &desc, error, sizeof(error));
        Require(m_Program != 0, error);
        DeleteShaderDesc(&desc);
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
        Require(m_Target != 0, "create target");
    }

    DeviceBuffer* CreateBuffer(uint32_t size, VkBufferUsageFlags usage, UploadMode mode)
    {
        const VkMemoryPropertyFlags host = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
        const VkMemoryPropertyFlags local = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
        VkMemoryPropertyFlags required = mode == MODE_STAGED ? local : host | (mode == MODE_BAR ? local : 0);
        VkMemoryPropertyFlags avoided = mode == MODE_HOST ? local : (mode == MODE_STAGED ? VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT : 0);
        uint32_t candidate;
        if (!GetMemoryTypeIndex(m_Memory, UINT32_MAX, required, &candidate, 0, avoided))
        {
            puts("SKIP: requested memory mode is unavailable");
            exit(77);
        }

        DeviceBuffer* buffer = new DeviceBuffer(usage | VK_BUFFER_USAGE_TRANSFER_DST_BIT);
        Check(CreateDeviceBuffer(m_Vulkan->m_PhysicalDevice.m_Device, m_Vulkan->m_LogicalDevice.m_Device,
                                 size, required, buffer, 0, avoided));
        VkMemoryPropertyFlags flags = m_Memory.memoryTypes[buffer->m_Allocation.m_MemoryTypeIndex].propertyFlags;
        if (mode == MODE_HOST && (flags & local))
        {
            puts("SKIP: separate host memory is unavailable on this device");
            exit(77);
        }

        if (mode != MODE_STAGED)
        {
            Check(buffer->m_Allocation.MapMemory(m_Vulkan->m_LogicalDevice.m_Device));
        }

        return buffer;
    }

    void Allocate(uint32_t vertex_bytes, uint32_t index_bytes, UploadMode mode)
    {
        for (uint32_t i = 0; i < m_Vulkan->m_NumFramesInFlight; ++i)
        {
            Slot& slot = m_Slots[i];
            slot.m_Vertices = CreateBuffer(vertex_bytes, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, mode);
            slot.m_Indices = CreateBuffer(index_bytes, VK_BUFFER_USAGE_INDEX_BUFFER_BIT, mode);
            if (mode == MODE_STAGED)
            {
                Check(CreateDeviceBufferAllocation(m_Vulkan->m_PhysicalDevice.m_Device, m_Vulkan->m_LogicalDevice.m_Device,
                                                   vertex_bytes + index_bytes, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                                   VK_BUFFER_USAGE_TRANSFER_SRC_BIT, &slot.m_Staging, 0, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT));
                Check(slot.m_Staging.MapMemory(m_Vulkan->m_LogicalDevice.m_Device));
            }
        }
    }

    void Upload(Slot& slot, VkCommandBuffer cmd, const float* vertices, uint32_t vertex_bytes,
                const uint32_t* indices, uint32_t index_bytes, UploadMode mode)
    {
        if (mode != MODE_STAGED)
        {
            memcpy(slot.m_Vertices->m_Allocation.m_MappedDataPtr, vertices, vertex_bytes);
            memcpy(slot.m_Indices->m_Allocation.m_MappedDataPtr, indices, index_bytes);
            return;
        }

        memcpy(slot.m_Staging.m_MappedDataPtr, vertices, vertex_bytes);
        memcpy((uint8_t*) slot.m_Staging.m_MappedDataPtr + vertex_bytes, indices, index_bytes);
        VkBufferMemoryBarrier barriers[2] = {};
        DeviceBuffer* buffers[] = { slot.m_Vertices, slot.m_Indices };
        const VkAccessFlags reads[] = { VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT, VK_ACCESS_INDEX_READ_BIT };
        for (uint32_t i = 0; i < 2; ++i)
        {
            barriers[i].sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
            barriers[i].srcAccessMask = slot.m_Initialized ? reads[i] : 0;
            barriers[i].dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            barriers[i].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barriers[i].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barriers[i].buffer = buffers[i]->m_Allocation.m_Handle.m_Buffer;
            barriers[i].size = VK_WHOLE_SIZE;
        }

        vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_VERTEX_INPUT_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, 0, 2, barriers, 0, 0);
        VkBufferCopy vertex_copy = { 0, 0, vertex_bytes };
        VkBufferCopy index_copy = { vertex_bytes, 0, index_bytes };
        vkCmdCopyBuffer(cmd, slot.m_Staging.m_Handle.m_Buffer, barriers[0].buffer, 1, &vertex_copy);
        vkCmdCopyBuffer(cmd, slot.m_Staging.m_Handle.m_Buffer, barriers[1].buffer, 1, &index_copy);
        for (uint32_t i = 0; i < 2; ++i)
        {
            barriers[i].srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            barriers[i].dstAccessMask = reads[i];
        }

        vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_VERTEX_INPUT_BIT, 0, 0, 0, 2, barriers, 0, 0);
    }

    void Collect(uint32_t slot_index, uint32_t warmup, double* gpu_times)
    {
        Slot& slot = m_Slots[slot_index];
        if (slot.m_PendingQuery && slot.m_LastFrame >= warmup)
        {
            uint64_t times[2];
            Check(vkGetQueryPoolResults(m_Vulkan->m_LogicalDevice.m_Device, m_Queries, slot_index * 2, 2,
                                        sizeof(times), times, sizeof(uint64_t), VK_QUERY_RESULT_64_BIT));
            gpu_times[slot.m_LastFrame - warmup] = double((times[1] - times[0]) & m_TimestampMask) *
                                                   m_Vulkan->m_PhysicalDevice.m_Properties.limits.timestampPeriod / 1000.0;
        }

        slot.m_PendingQuery = false;
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

    void Delete()
    {
        VkDevice device = m_Vulkan->m_LogicalDevice.m_Device;
        Check(vkDeviceWaitIdle(device));
        vkDestroyQueryPool(device, m_Queries, 0);
        for (uint32_t i = 0; i < m_Vulkan->m_NumFramesInFlight; ++i)
        {
            DestroyDeviceBufferAllocation(device, &m_Slots[i].m_Vertices->m_Allocation);
            DestroyDeviceBufferAllocation(device, &m_Slots[i].m_Indices->m_Allocation);
            DestroyDeviceBufferAllocation(device, &m_Slots[i].m_Staging);
            delete m_Slots[i].m_Vertices;
            delete m_Slots[i].m_Indices;
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
        fprintf(stderr, "Usage: %s host|bar|staged vertex_KiB upload_interval draws warmup_frames measured_frames verify\n", argv[0]);
        return 1;
    }

    UploadMode mode = strcmp(argv[1], "host") == 0 ? MODE_HOST : strcmp(argv[1], "bar") == 0 ? MODE_BAR : MODE_STAGED;
    Require(mode != MODE_STAGED || strcmp(argv[1], "staged") == 0, "valid upload mode");
    uint32_t vertex_count = (uint32_t) strtoul(argv[2], 0, 10) * 1024 / (8 * sizeof(float));
    vertex_count -= vertex_count % 3;
    uint32_t interval = (uint32_t) strtoul(argv[3], 0, 10);
    uint32_t draws = (uint32_t) strtoul(argv[4], 0, 10);
    uint32_t warmup = (uint32_t) strtoul(argv[5], 0, 10);
    uint32_t frames = (uint32_t) strtoul(argv[6], 0, 10);
    bool verify = atoi(argv[7]) != 0;
    Require(vertex_count >= 6 && frames > 0 && warmup >= DM_MAX_FRAMES_IN_FLIGHT && draws > 0, "valid workload");
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
    benchmark.Init(verify);
    uint64_t allocation_start = dmTime::GetMonotonicTime();
    benchmark.Allocate(vertex_bytes, index_bytes, mode);
    double allocation_us = double(dmTime::GetMonotonicTime() - allocation_start);
    double upload_us = 0;
    uint32_t uploads = 0;
    uint64_t start = 0;
    for (uint32_t frame = 0; frame < warmup + frames; ++frame)
    {
        if (frame == warmup)
        {
            Check(vkDeviceWaitIdle(benchmark.m_Vulkan->m_LogicalDevice.m_Device));
            start = dmTime::GetMonotonicTime();
        }

        BeginFrame(benchmark.m_Context);
        uint32_t slot_index = benchmark.m_Vulkan->m_CurrentFrameInFlight;
        Slot& slot = benchmark.m_Slots[slot_index];
        benchmark.Collect(slot_index, warmup, gpu_times);
        VkCommandBuffer cmd = benchmark.m_Vulkan->m_MainCommandBuffers[slot_index];
        vkCmdResetQueryPool(cmd, benchmark.m_Queries, slot_index * 2, 2);
        benchmark.m_WriteTimestamp(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, benchmark.m_Queries, slot_index * 2);
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
            benchmark.Upload(slot, cmd, vertices, vertex_bytes, indices, index_bytes, mode);
            if (frame >= warmup)
            {
                upload_us += double(dmTime::GetMonotonicTime() - upload_start);
                ++uploads;
            }

            slot.m_White = visible_vertices && visible_indices;
            slot.m_Initialized = true;
            slot.m_LastUpload = frame;
        }

        benchmark.Draw(slot, vertex_count, draws, verify);
        // ReadPixels may submit and replace the command buffer in verification runs.
        cmd = benchmark.m_Vulkan->m_MainCommandBuffers[slot_index];
        benchmark.m_WriteTimestamp(cmd, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, benchmark.m_Queries, slot_index * 2 + 1);
        slot.m_LastFrame = frame;
        slot.m_PendingQuery = true;
        DisableVertexDeclaration(benchmark.m_Context, benchmark.m_Declaration);
        DisableProgram(benchmark.m_Context);
        SetRenderTarget(benchmark.m_Context, 0, RenderTargetBindingParams());
        Flip(benchmark.m_Context);
    }

    Check(vkDeviceWaitIdle(benchmark.m_Vulkan->m_LogicalDevice.m_Device));
    double wall_us = double(dmTime::GetMonotonicTime() - start) / frames;
    for (uint32_t i = 0; i < benchmark.m_Vulkan->m_NumFramesInFlight; ++i)
    {
        benchmark.Collect(i, warmup, gpu_times);
    }

    double gpu_sum = 0;
    for (uint32_t i = 0; i < frames; ++i)
    {
        Require(gpu_times[i] > 0, "all measured frames have GPU timestamps");
        gpu_sum += gpu_times[i];
    }

    qsort(gpu_times, frames, sizeof(double), CompareDouble);
    uint32_t memory_index = benchmark.m_Slots[0].m_Vertices->m_Allocation.m_MemoryTypeIndex;
    VkMemoryType memory_type = benchmark.m_Memory.memoryTypes[memory_index];
    printf("RESULT {\"mode\":\"%s\",\"gpu\":\"%s\",\"driver\":%u,\"vertex_bytes\":%u,\"index_bytes\":%u,"
           "\"interval\":%u,\"draws\":%u,\"frames\":%u,\"warmup\":%u,\"verify\":%s,\"slots\":%u,"
           "\"memory_type\":%u,\"memory_flags\":%u,\"heap\":%u,\"heap_bytes\":%llu,"
           "\"allocation_us\":%.3f,\"uploads\":%u,\"upload_cpu_us\":%.3f,\"gpu_mean_us\":%.3f,"
           "\"gpu_median_us\":%.3f,\"gpu_p95_us\":%.3f,\"wall_us\":%.3f}\n",
           argv[1], benchmark.m_Vulkan->m_PhysicalDevice.m_Properties.deviceName,
           benchmark.m_Vulkan->m_PhysicalDevice.m_Properties.driverVersion, vertex_bytes, index_bytes,
           interval, draws, frames, warmup, verify ? "true" : "false", benchmark.m_Vulkan->m_NumFramesInFlight,
           memory_index, memory_type.propertyFlags, memory_type.heapIndex,
           (unsigned long long) benchmark.m_Memory.memoryHeaps[memory_type.heapIndex].size,
           allocation_us, uploads, uploads ? upload_us / uploads : 0.0, gpu_sum / frames,
           gpu_times[frames / 2], gpu_times[(frames - 1) * 95 / 100], wall_us);
    benchmark.Delete();
    free(vertices);
    free(indices);
    free(gpu_times);
    return 0;
}
