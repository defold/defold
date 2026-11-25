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
#include <platform/window.hpp>
#include <testmain/testmain.h>
#include "test_app_graphics.h"
#include "../vulkan/graphics_vulkan_private.h"
#include "buffer_reuse_vert.h"
#include "buffer_reuse_frag.h"

using namespace dmGraphics;

static void Require(bool condition, const char* message)
{
    if (!condition)
    {
        fprintf(stderr, "Vulkan buffer reuse: FAIL: %s\n", message);
        exit(1);
    }
}

enum
{
    TARGET_WIDTH          = 512,
    TARGET_HEIGHT         = 64,
    CELL_WIDTH            = 32,
    VERTICES_PER_TRIANGLE = 3,
    FLOATS_PER_TRIANGLE   = VERTICES_PER_TRIANGLE * 2,
    TRIANGLE_VERTEX_BYTES = FLOATS_PER_TRIANGLE * sizeof(float),
    TRIANGLE_INDEX_BYTES  = VERTICES_PER_TRIANGLE * sizeof(uint16_t)
};

// The first triangle covers a cell; the second lies entirely outside its viewport.
// Choosing either triangle makes the cell white or leaves its cleared black color.
static const float TRIANGLES[] = {
    -1, -1, 3, -1, -1, 3,
    7, -1, 11, -1, 7, 3
};
static const uint16_t INDICES[] = { 0, 1, 2, 3, 4, 5 };

struct Fixture
{
    HWindow            m_Window;
    HContext           m_Context;
    HJobContext        m_Jobs;
    HProgram           m_Program;
    HVertexDeclaration m_Declaration;
    HRenderTarget      m_Target;

    void Init()
    {
        Require(InstallAdapter(ADAPTER_FAMILY_VULKAN) == RESULT_OK, "install Vulkan");
        m_Window = dmPlatform::NewWindow();
        WindowCreateParams window;
        WindowCreateParamsInitialize(&window);
        window.m_Width = TARGET_WIDTH;
        window.m_Height = TARGET_HEIGHT;
        window.m_Samples = 1;
        window.m_Hidden = 1;
        window.m_FocusOnShow = 0;
        window.m_Title = "Vulkan buffer reuse tests";
        window.m_GraphicsApi = WINDOW_GRAPHICS_API_VULKAN;
        Require(dmPlatform::OpenWindow(m_Window, window) == WINDOW_RESULT_OK, "open hidden window");

        JobSystemCreateParams jobs = {};
        jobs.m_ThreadCount = 1;
        m_Jobs = JobSystemCreate(&jobs);

        ContextParams params;
        params.m_Window = m_Window;
        params.m_Width = TARGET_WIDTH;
        params.m_Height = TARGET_HEIGHT;
        params.m_JobContext = m_Jobs;
        params.m_VerifyGraphicsCalls = 1;
        params.m_UseValidationLayers = 1;
        m_Context = NewContext(params);
        Require(m_Context != 0, "create Vulkan context");
        SetSwapInterval(m_Context, 0);

        CreateProgramAndVertexDeclaration();
        CreateRenderTarget();
    }

    void CreateProgramAndVertexDeclaration()
    {
        ShaderDesc desc = {};
        AddShader(&desc, ShaderDesc::LANGUAGE_SPIRV, ShaderDesc::SHADER_TYPE_VERTEX, buffer_reuse_vert, buffer_reuse_vert_SIZE);
        AddShader(&desc, ShaderDesc::LANGUAGE_SPIRV, ShaderDesc::SHADER_TYPE_FRAGMENT, buffer_reuse_frag, buffer_reuse_frag_SIZE);
        AddShaderResource(&desc, "position", ShaderDesc::SHADER_TYPE_VEC2, 0, 0, BINDING_TYPE_INPUT, SHADER_STAGE_FLAG_VERTEX);
        char error[2048] = {};
        m_Program = NewProgram(m_Context, &desc, error, sizeof(error));
        Require(m_Program != 0, error);
        DeleteShaderDesc(&desc);

        HVertexStreamDeclaration streams = NewVertexStreamDeclaration(m_Context);
        AddVertexStream(streams, "position", 2, TYPE_FLOAT, false);
        m_Declaration = NewVertexDeclaration(m_Context, streams);
        DeleteVertexStreamDeclaration(streams);
    }

    void CreateRenderTarget()
    {
        RenderTargetCreationParams target = {};
        target.m_SampleCount = 1;
        target.m_ColorBufferCreationParams[0].m_Width = TARGET_WIDTH;
        target.m_ColorBufferCreationParams[0].m_Height = TARGET_HEIGHT;
        target.m_ColorBufferParams[0].m_Width = TARGET_WIDTH;
        target.m_ColorBufferParams[0].m_Height = TARGET_HEIGHT;
        target.m_ColorBufferParams[0].m_Format = TEXTURE_FORMAT_RGBA;
        target.m_ColorBufferLoadOps[0] = ATTACHMENT_OP_LOAD;
        target.m_ColorBufferStoreOps[0] = ATTACHMENT_OP_STORE;
        m_Target = NewRenderTarget(m_Context, BUFFER_TYPE_COLOR0_BIT, target);
        Require(m_Target != 0, "create target");
    }

    void Begin()
    {
        BeginFrame(m_Context);
        SetRenderTarget(m_Context, m_Target, RenderTargetBindingParams());
        Clear(m_Context, BUFFER_TYPE_COLOR0_BIT, 0, 0, 0, 255, 1, 0);
        EnableProgram(m_Context, m_Program);
        DisableState(m_Context, STATE_BLEND);
        DisableState(m_Context, STATE_DEPTH_TEST);
        DisableState(m_Context, STATE_CULL_FACE);
    }

    void DrawCell(uint32_t cell, HVertexBuffer vertices, HIndexBuffer indices = 0, uint32_t index_byte_offset = 0)
    {
        SetViewport(m_Context, cell * CELL_WIDTH, 0, CELL_WIDTH, TARGET_HEIGHT);
        EnableVertexBuffer(m_Context, vertices, 0);
        EnableVertexDeclaration(m_Context, m_Declaration, 0, 0, m_Program);
        if (indices)
        {
            DrawElements(m_Context, PRIMITIVE_TRIANGLES, index_byte_offset, VERTICES_PER_TRIANGLE, TYPE_UNSIGNED_SHORT, indices, 1);
        }
        else
        {
            Draw(m_Context, PRIMITIVE_TRIANGLES, 0, VERTICES_PER_TRIANGLE, 1);
        }
    }

    void CheckCellColors(uint32_t count, const bool* expect_white)
    {
        uint8_t pixels[TARGET_WIDTH * TARGET_HEIGHT * 4];
        ReadPixels(m_Context, 0, 0, TARGET_WIDTH, TARGET_HEIGHT, pixels, sizeof(pixels));
        for (uint32_t cell = 0; cell < count; ++cell)
        {
            uint32_t center_x = cell * CELL_WIDTH + CELL_WIDTH / 2;
            uint32_t center_y = TARGET_HEIGHT / 2;
            const uint8_t* pixel = pixels + (center_y * TARGET_WIDTH + center_x) * 4;
            uint8_t expected = expect_white[cell] ? 255 : 0;
            if (pixel[0] != expected || pixel[1] != expected || pixel[2] != expected || pixel[3] != 255)
            {
                fprintf(stderr, "cell %u: expected %u, got %u,%u,%u,%u\n", cell, expected, pixel[0], pixel[1], pixel[2], pixel[3]);
                Require(false, "earlier draw or untouched bytes changed");
            }
        }
    }

    void End()
    {
        DisableVertexDeclaration(m_Context, m_Declaration);
        DisableProgram(m_Context);
        SetRenderTarget(m_Context, 0, RenderTargetBindingParams());
        Flip(m_Context);
    }

    void Delete()
    {
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

static uint32_t PendingResources(Fixture& fixture)
{
    VulkanContext* context = (VulkanContext*) fixture.m_Context;
    uint32_t count = 0;
    for (uint32_t i = 0; i < DM_MAX_FRAMES_IN_FLIGHT; ++i)
    {
        count += context->m_MainResourcesToDestroy[i]->Size();
    }
    return count;
}

// Device-local storage must retain allocation metadata and never require a CPU mapping.
static void CheckAllocationSizes(Fixture& fixture, DeviceBuffer* buffer)
{
    VulkanContext* context = (VulkanContext*) fixture.m_Context;
    const DeviceBufferAllocation& allocation = buffer->m_Allocation;
    Require(allocation.m_Capacity >= buffer->m_Base.m_Size, "allocation covers logical data");
    VkMemoryRequirements requirements;
    vkGetBufferMemoryRequirements(context->m_LogicalDevice.m_Device, allocation.m_Handle.m_Buffer, &requirements);
    Require(allocation.m_AllocationSize == requirements.size, "buffer retains actual allocation size");
    Require(allocation.m_Capacity <= allocation.m_AllocationSize, "usable capacity fits allocation");
    VkMemoryPropertyFlags flags = context->m_PhysicalDevice.m_MemoryProperties.memoryTypes[allocation.m_MemoryTypeIndex].propertyFlags;
    Require((flags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) != 0, "vertex/index destination is device local");
    Require(allocation.m_MappedDataPtr == 0, "vertex/index destination is not CPU mapped");
}

static uint32_t CollectStagingBuffers(VulkanContext* context, VkBuffer* handles, uint32_t capacity)
{
    uint32_t count = 0;
    for (uint32_t i = 0; i <= DM_MAX_FRAMES_IN_FLIGHT; ++i)
    {
        const BufferUploadPool& pool = i == DM_MAX_FRAMES_IN_FLIGHT ? context->m_PendingBufferUploadPool : context->m_BufferUploadPools[i];
        for (uint32_t j = 0; j < pool.m_Blocks.Size(); ++j)
        {
            Require(count < capacity, "staging snapshot capacity");
            Require(pool.m_Blocks[j].m_Allocation.m_MappedDataPtr != 0, "staging remains mapped");
            handles[count++] = pool.m_Blocks[j].m_Allocation.m_Handle.m_Buffer;
        }
    }

    return count;
}

// Deleting a zero-sized buffer is valid even if its first upload never created a VkBuffer.
static void TestDeleteEmptyBuffers(Fixture& fixture)
{
    HVertexBuffer vertices = NewVertexBuffer(fixture.m_Context, 0, 0, BUFFER_USAGE_STREAM_DRAW);
    HIndexBuffer indices = NewIndexBuffer(fixture.m_Context, 0, 0, BUFFER_USAGE_DYNAMIC_DRAW);
    Require(vertices && indices, "zero-sized vertex and index buffers must have valid handles");
    DeleteVertexBuffer(vertices);
    DeleteIndexBuffer(indices);
    puts("PASS: deleting empty buffers before their first upload");
}

// Repeated uploads must reuse device-local destinations and fenced staging blocks,
// including writes before BeginFrame; deletion must retire each destination once.
static void TestReuseAndDeletion(Fixture& fixture)
{
    HVertexBuffer vertices = NewVertexBuffer(fixture.m_Context, sizeof(TRIANGLES), TRIANGLES, BUFFER_USAGE_DYNAMIC_DRAW);
    HIndexBuffer indices = NewIndexBuffer(fixture.m_Context, TRIANGLE_INDEX_BYTES, INDICES, BUFFER_USAGE_STREAM_DRAW);
    VkBuffer vertex_handle = ((DeviceBuffer*) vertices)->m_Allocation.m_Handle.m_Buffer;
    VkBuffer index_handle = ((DeviceBuffer*) indices)->m_Allocation.m_Handle.m_Buffer;
    VkBuffer retained[32] = {};
    uint32_t retained_count = 0;
    for (uint32_t frame = 0; frame < 20; ++frame)
    {
        bool in_frame = (frame % 2) != 0;
        if (in_frame)
        {
            fixture.Begin();
        }

        SetVertexBufferData(vertices, sizeof(TRIANGLES), TRIANGLES, BUFFER_USAGE_DYNAMIC_DRAW);
        SetIndexBufferData(indices, TRIANGLE_INDEX_BYTES, INDICES + (frame % 2) * VERTICES_PER_TRIANGLE, BUFFER_USAGE_STREAM_DRAW);
        if (!in_frame)
        {
            fixture.Begin();
        }

        Require(((DeviceBuffer*) vertices)->m_Allocation.m_Handle.m_Buffer == vertex_handle, "vertex destination reused");
        Require(((DeviceBuffer*) indices)->m_Allocation.m_Handle.m_Buffer == index_handle, "index destination reused");
        CheckAllocationSizes(fixture, (DeviceBuffer*) vertices);
        CheckAllocationSizes(fixture, (DeviceBuffer*) indices);
        VkBuffer current[32];
        uint32_t count = CollectStagingBuffers((VulkanContext*) fixture.m_Context, current, DM_ARRAY_SIZE(current));
        if (frame == 10)
        {
            memcpy(retained, current, count * sizeof(VkBuffer));
            retained_count = count;
        }
        else if (frame > 10)
        {
            Require(count == retained_count, "staging block count stabilizes");
            for (uint32_t i = 0; i < count; ++i)
            {
                bool reused = false;
                for (uint32_t j = 0; j < retained_count; ++j)
                {
                    reused |= current[i] == retained[j];
                }

                Require(reused, "steady uploads reuse staging allocations");
            }
        }

        fixture.DrawCell(0, vertices, indices);
        fixture.DrawCell(1, vertices);
        if (frame == 19)
        {
            const bool expected[] = { false, true };
            fixture.CheckCellColors(2, expected);
        }
        fixture.End();
    }

    uint32_t pending = PendingResources(fixture);
    DeleteVertexBuffer(vertices);
    DeleteIndexBuffer(indices);
    Require(PendingResources(fixture) == pending + 2, "delete retires each destination once");
    puts("PASS: device-local destinations, staging reuse, pre-frame uploads and deletion");
}

// Growth, shrink and repeated updates in one render pass must preserve every
// earlier vertex/index draw, including reuse of the same destination allocation.
static void TestRepeatedReplacement(Fixture& fixture)
{
    HVertexBuffer vertex_buffer = NewVertexBuffer(fixture.m_Context, 0, 0, BUFFER_USAGE_STREAM_DRAW);
    HVertexBuffer indexed_vertices = NewVertexBuffer(fixture.m_Context, sizeof(TRIANGLES), TRIANGLES, BUFFER_USAGE_STATIC_DRAW);
    HIndexBuffer index_buffer = NewIndexBuffer(fixture.m_Context, 0, 0, BUFFER_USAGE_DYNAMIC_DRAW);
    bool expected[16];
    for (uint32_t frame = 0; frame < 6; ++frame)
    {
        fixture.Begin();
        for (uint32_t cell = 0; cell < 8; ++cell)
        {
            uint32_t triangle_index = (cell + frame) % 2;
            bool expect_white = triangle_index == 0;
            // Vary the logical size through four capacities while drawing one triangle.
            float vertices[4 * FLOATS_PER_TRIANGLE] = {};
            memcpy(vertices, TRIANGLES + triangle_index * FLOATS_PER_TRIANGLE, TRIANGLE_VERTEX_BYTES);
            uint32_t vertex_bytes = TRIANGLE_VERTEX_BYTES * (1 + cell % 4);
            SetVertexBufferData(vertex_buffer, vertex_bytes, vertices, BUFFER_USAGE_STREAM_DRAW);
            Require(((DeviceBuffer*) vertex_buffer)->m_Base.m_Size == vertex_bytes, "logical vertex size follows shrink/growth");
            CheckAllocationSizes(fixture, (DeviceBuffer*) vertex_buffer);
            fixture.DrawCell(cell, vertex_buffer);

            uint16_t indices[4 * VERTICES_PER_TRIANGLE] = {};
            memcpy(indices, INDICES + triangle_index * VERTICES_PER_TRIANGLE, TRIANGLE_INDEX_BYTES);
            uint32_t index_bytes = TRIANGLE_INDEX_BYTES * (1 + cell % 4);
            SetIndexBufferData(index_buffer, index_bytes, indices, BUFFER_USAGE_DYNAMIC_DRAW);
            Require(((DeviceBuffer*) index_buffer)->m_Base.m_Size == index_bytes, "logical index size follows shrink/growth");
            CheckAllocationSizes(fixture, (DeviceBuffer*) index_buffer);
            fixture.DrawCell(cell + 8, indexed_vertices, index_buffer);
            expected[cell] = expect_white;
            expected[cell + 8] = expect_white;
        }
        if (frame == 5)
        {
            fixture.CheckCellColors(16, expected);
        }
        fixture.End();
    }
    DeleteVertexBuffer(vertex_buffer);
    DeleteVertexBuffer(indexed_vertices);
    DeleteIndexBuffer(index_buffer);
    puts("PASS: repeated uploads, growth, shrink and destination reuse");
}

// Partial writes must preserve earlier draws and untouched bytes, including
// six-byte index ranges that cannot use vkCmdUpdateBuffer.
static void TestPartialUpdates(Fixture& fixture)
{
    HVertexBuffer vertex_buffer = NewVertexBuffer(fixture.m_Context, sizeof(TRIANGLES), TRIANGLES, BUFFER_USAGE_DYNAMIC_DRAW);
    HIndexBuffer index_buffer = NewIndexBuffer(fixture.m_Context, sizeof(INDICES), INDICES, BUFFER_USAGE_DYNAMIC_DRAW);
    fixture.Begin();
    fixture.DrawCell(0, vertex_buffer, index_buffer);
    // Replace the second triangle while preserving the first recorded draw.
    SetVertexBufferSubData(vertex_buffer, TRIANGLE_VERTEX_BYTES, TRIANGLE_VERTEX_BYTES, TRIANGLES);
    fixture.DrawCell(1, vertex_buffer, index_buffer, TRIANGLE_INDEX_BYTES);
    fixture.DrawCell(2, vertex_buffer, index_buffer);

    // Move the first triangle offscreen; the second must remain visible.
    SetVertexBufferSubData(vertex_buffer, 0, TRIANGLE_VERTEX_BYTES, TRIANGLES + FLOATS_PER_TRIANGLE);
    fixture.DrawCell(3, vertex_buffer, index_buffer);
    fixture.DrawCell(4, vertex_buffer, index_buffer, TRIANGLE_INDEX_BYTES);

    // Redirect the second index range to the now-offscreen first triangle.
    SetIndexBufferSubData(index_buffer, TRIANGLE_INDEX_BYTES, TRIANGLE_INDEX_BYTES, INDICES);
    fixture.DrawCell(5, vertex_buffer, index_buffer, TRIANGLE_INDEX_BYTES);
    fixture.DrawCell(6, vertex_buffer, index_buffer);

    // Redirect the first index range to the still-visible second triangle.
    SetIndexBufferSubData(index_buffer, 0, TRIANGLE_INDEX_BYTES, INDICES + VERTICES_PER_TRIANGLE);
    fixture.DrawCell(7, vertex_buffer, index_buffer);
    fixture.DrawCell(8, vertex_buffer, index_buffer, TRIANGLE_INDEX_BYTES);

    const bool expected[] = {
        true,              // Original contents.
        true, true,        // Second vertex range replaced.
        false, true,       // First vertex range replaced.
        false, false,      // Second index range redirected.
        true, false        // First index range redirected.
    };
    fixture.CheckCellColors(9, expected);
    fixture.End();

    // Usage hints share one allocation policy; zero-sized writes still do nothing.
    VkBuffer retained = ((DeviceBuffer*) vertex_buffer)->m_Allocation.m_Handle.m_Buffer;
    SetVertexBufferData(vertex_buffer, sizeof(TRIANGLES), TRIANGLES, BUFFER_USAGE_STATIC_DRAW);
    Require(((DeviceBuffer*) vertex_buffer)->m_Allocation.m_Handle.m_Buffer == retained, "static update reuses destination");
    SetVertexBufferData(vertex_buffer, sizeof(TRIANGLES), TRIANGLES, BUFFER_USAGE_DYNAMIC_DRAW);
    Require(((DeviceBuffer*) vertex_buffer)->m_Allocation.m_Handle.m_Buffer == retained, "dynamic update reuses destination");
    SetVertexBufferData(vertex_buffer, 0, 0, BUFFER_USAGE_DYNAMIC_DRAW);
    Require(((DeviceBuffer*) vertex_buffer)->m_Base.m_Size == sizeof(TRIANGLES), "zero-size update retains existing behavior");
    DeleteVertexBuffer(vertex_buffer);
    DeleteIndexBuffer(index_buffer);
    puts("PASS: partial writes, usage transitions and zero-size update");
}

// Patching a buffer in pieces before its first draw must reuse one allocation.
// The indexed draw checks that all the independently written pieces reach the GPU.
static void TestConsecutiveWritesBeforeDraw(Fixture& fixture)
{
    fixture.Begin();
    HVertexBuffer vertices = NewVertexBuffer(fixture.m_Context, TRIANGLE_VERTEX_BYTES, 0, BUFFER_USAGE_STREAM_DRAW);
    HIndexBuffer indices = NewIndexBuffer(fixture.m_Context, TRIANGLE_INDEX_BYTES, 0, BUFFER_USAGE_DYNAMIC_DRAW);
    VkBuffer vertex_allocation = ((DeviceBuffer*) vertices)->m_Allocation.m_Handle.m_Buffer;
    VkBuffer index_allocation = ((DeviceBuffer*) indices)->m_Allocation.m_Handle.m_Buffer;

    for (uint32_t i = 0; i < VERTICES_PER_TRIANGLE; ++i)
    {
        const uint32_t vertex_stride = 2 * sizeof(float);
        SetVertexBufferSubData(vertices, i * vertex_stride, vertex_stride, &TRIANGLES[i * 2]);
        SetIndexBufferSubData(indices, i * sizeof(uint16_t), sizeof(uint16_t), &INDICES[i]);
        Require(((DeviceBuffer*) vertices)->m_Allocation.m_Handle.m_Buffer == vertex_allocation, "vertex patch replaced an undrawn allocation");
        Require(((DeviceBuffer*) indices)->m_Allocation.m_Handle.m_Buffer == index_allocation, "index patch replaced an undrawn allocation");
    }

    fixture.DrawCell(0, vertices, indices);
    const bool expected[] = { true };
    fixture.CheckCellColors(1, expected);
    fixture.End();
    DeleteVertexBuffer(vertices);
    DeleteIndexBuffer(indices);
    puts("PASS: consecutive vertex and index patches before the first draw");
}

// Transfer-enabled vertices use device-local uploads too; storage buffers retain
// their own replacement and shader-access tracking while sharing staging storage.
static void TestTransferAndStorageBuffers(Fixture& fixture)
{
    fixture.Begin();
    HVertexBuffer vertices = NewVertexBuffer(fixture.m_Context, sizeof(TRIANGLES), TRIANGLES, (BufferUsage)(BUFFER_USAGE_DYNAMIC_DRAW | BUFFER_USAGE_TRANSFER));
    VkBuffer retained = ((DeviceBuffer*) vertices)->m_Allocation.m_Handle.m_Buffer;
    fixture.DrawCell(0, vertices);
    SetVertexBufferData(vertices, sizeof(TRIANGLES), TRIANGLES, BUFFER_USAGE_DYNAMIC_DRAW);
    Require(((DeviceBuffer*) vertices)->m_Allocation.m_Handle.m_Buffer == retained, "transfer-enabled destination reused");
    CheckAllocationSizes(fixture, (DeviceBuffer*) vertices);
    fixture.DrawCell(1, vertices);
    HStorageBuffer storage = NewStorageBuffer(fixture.m_Context, sizeof(TRIANGLES), TRIANGLES, BUFFER_USAGE_DYNAMIC_DRAW);
    SetStorageBufferData(fixture.m_Context, storage, sizeof(TRIANGLES), TRIANGLES, BUFFER_USAGE_DYNAMIC_DRAW);
    const bool expected[] = { true, true };
    fixture.CheckCellColors(2, expected);
    DeleteVertexBuffer(vertices);
    DeleteStorageBuffer(fixture.m_Context, storage);
    fixture.End();
    puts("PASS: transfer-enabled vertices and storage buffers");
}

// Pre-frame writes snapshot caller memory, while deletion and growth cancel only
// the obsolete destination's pending copies and preserve other uploads' order.
static void TestPendingUploads(Fixture& fixture)
{
    VulkanContext* context = (VulkanContext*) fixture.m_Context;
    Require(context->m_PendingBufferUploads.Empty(), "no pending uploads at test start");
    float data[DM_ARRAY_SIZE(TRIANGLES)];
    memcpy(data, TRIANGLES, sizeof(data));
    HVertexBuffer vertices = NewVertexBuffer(fixture.m_Context, sizeof(data), data, BUFFER_USAGE_STATIC_DRAW);
    memset(data, 0, sizeof(data));
    HIndexBuffer indices = NewIndexBuffer(fixture.m_Context, TRIANGLE_INDEX_BYTES, INDICES, BUFFER_USAGE_STATIC_DRAW);
    HVertexBuffer discarded = NewVertexBuffer(fixture.m_Context, TRIANGLE_VERTEX_BYTES, TRIANGLES, BUFFER_USAGE_STREAM_DRAW);
    SetVertexBufferData(discarded, sizeof(TRIANGLES), TRIANGLES, BUFFER_USAGE_STREAM_DRAW);
    Require(context->m_PendingBufferUploads.Size() == 3, "growth cancels the old destination upload");
    DeleteVertexBuffer(discarded);
    Require(context->m_PendingBufferUploads.Size() == 2, "deletion cancels only its pending upload");
    fixture.Begin();
    Require(context->m_PendingBufferUploads.Empty(), "BeginFrame records queued copies");
    fixture.DrawCell(0, vertices, indices);
    const bool expected[] = { true };
    fixture.CheckCellColors(1, expected);
    fixture.End();
    DeleteVertexBuffer(vertices);
    DeleteIndexBuffer(indices);
    puts("PASS: pre-frame snapshots, growth and cancellation");
}

// Growing the pre-frame staging block array must not invalidate queued sources.
// Null data and byte-sized patches must preserve the untouched index bytes.
static void TestLargePendingUploadsAndBytePatches(Fixture& fixture)
{
    const uint32_t buffer_size = 300 * 1024;
    float* data = (float*) calloc(1, buffer_size);
    Require(data != 0, "allocate test vertices");
    HVertexBuffer vertices[6];
    bool expected[DM_ARRAY_SIZE(vertices)];
    for (uint32_t i = 0; i < DM_ARRAY_SIZE(vertices); ++i)
    {
        memcpy(data, TRIANGLES + (i % 2) * FLOATS_PER_TRIANGLE, TRIANGLE_VERTEX_BYTES);
        vertices[i] = NewVertexBuffer(fixture.m_Context, buffer_size, data, BUFFER_USAGE_STATIC_DRAW);
        expected[i] = i % 2 == 0;
    }

    memset(data, 0, buffer_size);
    free(data);
    HIndexBuffer indices = NewIndexBuffer(fixture.m_Context, TRIANGLE_INDEX_BYTES, 0, BUFFER_USAGE_STREAM_DRAW);
    // Six single-byte patches exercise arbitrary copy offsets and sizes after zero-fill.
    const uint8_t* index_bytes = (const uint8_t*) INDICES;
    for (uint32_t i = 0; i < TRIANGLE_INDEX_BYTES; ++i)
    {
        SetIndexBufferSubData(indices, i, 1, index_bytes + i);
    }

    fixture.Begin();
    for (uint32_t i = 0; i < DM_ARRAY_SIZE(vertices); ++i)
    {
        fixture.DrawCell(i, vertices[i], indices);
    }

    fixture.CheckCellColors(DM_ARRAY_SIZE(expected), expected);
    // A null partial update restores zero indices and must not affect earlier draws.
    SetIndexBufferSubData(indices, 1, TRIANGLE_INDEX_BYTES - 1, 0);
    fixture.DrawCell(6, vertices[0], indices);
    const bool after_zero[] = { true, false, true, false, true, false, false };
    fixture.CheckCellColors(DM_ARRAY_SIZE(after_zero), after_zero);
    fixture.End();
    for (uint32_t i = 0; i < DM_ARRAY_SIZE(vertices); ++i)
    {
        DeleteVertexBuffer(vertices[i]);
    }

    DeleteIndexBuffer(indices);
    puts("PASS: staging block growth, caller lifetime, byte patches and zero-fill");
}

extern "C" void dmExportedSymbols();

// Rank compatible memory types by caller intent without weakening required flags,
// assuming a discrete heap layout, or changing driver order when scores tie.
static void TestMemoryTypeSelection()
{
    const VkMemoryPropertyFlags host = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    const VkMemoryPropertyFlags local = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    const VkMemoryPropertyFlags cached = VK_MEMORY_PROPERTY_HOST_CACHED_BIT;
    VkPhysicalDeviceMemoryProperties properties = {};
    properties.memoryTypeCount = 4;
    properties.memoryTypes[0].propertyFlags = host;
    properties.memoryTypes[1].propertyFlags = local;
    properties.memoryTypes[2].propertyFlags = host | cached;
    properties.memoryTypes[3].propertyFlags = host | local;
    uint32_t selected = 99;
    Require(GetMemoryTypeIndex(properties, 15, host, &selected) && selected == 0, "default preserves driver order");
    Require(GetMemoryTypeIndex(properties, 15, host, &selected, local) && selected == 3, "prefer mappable device-local memory");
    Require(GetMemoryTypeIndex(properties, 7, host, &selected, local) && selected == 0, "incompatible preferred type is ignored");
    Require(GetMemoryTypeIndex(properties, 15, host, &selected, cached) && selected == 2, "prefer cached memory for readback");
    Require(GetMemoryTypeIndex(properties, 15, local, &selected, 0, host) && selected == 1, "reserve mappable VRAM when it is unnecessary");
    properties.memoryTypes[0].propertyFlags = host | local;
    properties.memoryTypes[3].propertyFlags = host;
    Require(GetMemoryTypeIndex(properties, 15, host, &selected, 0, local) && selected == 2, "staging preference works regardless of memory type order");
    Require(GetMemoryTypeIndex(properties, 1, host, &selected, 0, local) && selected == 0, "UMA can use shared memory despite a soft avoidance");
    selected = 99;
    Require(!GetMemoryTypeIndex(properties, 2, host, &selected, local) && selected == 99, "preferences cannot weaken requirements");
    Require(!GetMemoryTypeIndex(properties, 0, host, &selected) && selected == 99, "no compatible type leaves output untouched");
    properties.memoryTypeCount = VK_MAX_MEMORY_TYPES;
    properties.memoryTypes[31].propertyFlags = host;
    Require(GetMemoryTypeIndex(properties, 1u << 31, host, &selected) && selected == 31, "highest memory type bit is supported");
    properties.memoryTypes[2].propertyFlags = host | local | cached;
    Require(GetMemoryTypeIndex(properties, 15, host, &selected, local | cached) && selected == 2, "all preferred properties outrank a partial match");
    puts("PASS: memory type requirements, preferences, fallback and driver order");
}

int main(int argc, char** argv)
{
    TestMemoryTypeSelection();
    if (argc == 2 && strcmp(argv[1], "--memory-types-only") == 0)
    {
        return 0;
    }

    dmExportedSymbols();
    TestMainPlatformInit();
    Fixture fixture = {};
    fixture.Init();
    TestDeleteEmptyBuffers(fixture);
    TestReuseAndDeletion(fixture);
    TestRepeatedReplacement(fixture);
    TestPartialUpdates(fixture);
    TestConsecutiveWritesBeforeDraw(fixture);
    TestTransferAndStorageBuffers(fixture);
    TestPendingUploads(fixture);
    TestLargePendingUploadsAndBytePatches(fixture);
    fixture.Delete();
    return 0;
}
