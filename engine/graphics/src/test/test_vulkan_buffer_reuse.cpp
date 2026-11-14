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
        Require(InstallAdapter(ADAPTER_FAMILY_VULKAN), "install Vulkan");
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

static uint32_t BackingCount(DeviceBuffer* buffer)
{
    uint32_t count = buffer->m_Allocation.m_Handle.m_Buffer != VK_NULL_HANDLE;
    if (!buffer->m_Pool)
    {
        return count;
    }
    for (uint32_t i = 0; i < DM_MAX_FRAMES_IN_FLIGHT; ++i)
    {
        count += buffer->m_Pool->m_Backing[i].m_Handle.m_Buffer != VK_NULL_HANDLE;
    }
    return count;
}

// Allocation metadata must travel with each backing when it changes owners.
static void CheckAllocationSizes(Fixture& fixture, DeviceBuffer* buffer)
{
    VulkanContext* context = (VulkanContext*) fixture.m_Context;
    Require(buffer->m_Allocation.m_Capacity >= buffer->m_Base.m_Size, "active allocation covers the logical data");
    for (uint32_t i = 0; i <= DM_MAX_FRAMES_IN_FLIGHT; ++i)
    {
        if (i && !buffer->m_Pool)
        {
            break;
        }
        const DeviceBufferAllocation& allocation = i ? buffer->m_Pool->m_Backing[i - 1] : buffer->m_Allocation;
        if (allocation.m_Handle.m_Buffer == VK_NULL_HANDLE)
        {
            continue;
        }
        VkMemoryRequirements requirements;
        vkGetBufferMemoryRequirements(context->m_LogicalDevice.m_Device, allocation.m_Handle.m_Buffer, &requirements);
        Require(allocation.m_AllocationSize == requirements.size, "backing retains its actual allocation size");
        Require(allocation.m_Capacity <= allocation.m_AllocationSize, "usable capacity fits the allocation");
    }
}

// Verify pre-frame and in-frame uploads reuse retained allocations after fences,
// and deleting a logical buffer retires every cached allocation exactly once.
static void TestReuseAndDeletion(Fixture& fixture)
{
    HVertexBuffer vertex_buffer = NewVertexBuffer(fixture.m_Context, sizeof(TRIANGLES), TRIANGLES, BUFFER_USAGE_DYNAMIC_DRAW);
    HIndexBuffer index_buffer = NewIndexBuffer(fixture.m_Context, TRIANGLE_INDEX_BYTES, INDICES, BUFFER_USAGE_STREAM_DRAW);
    // Snapshot the active backing and every spare after the frame slots warm up.
    VkBuffer retained[1 + DM_MAX_FRAMES_IN_FLIGHT] = {};
    for (uint32_t frame = 0; frame < 16; ++frame)
    {
        bool upload_during_frame = (frame % 2) != 0;
        if (upload_during_frame)
        {
            fixture.Begin();
        }
        SetVertexBufferData(vertex_buffer, sizeof(TRIANGLES), TRIANGLES, BUFFER_USAGE_DYNAMIC_DRAW);
        const uint16_t* triangle_indices = INDICES + (frame % 2) * VERTICES_PER_TRIANGLE;
        SetIndexBufferData(index_buffer, TRIANGLE_INDEX_BYTES, triangle_indices, BUFFER_USAGE_STREAM_DRAW);
        if (!upload_during_frame)
        {
            fixture.Begin();
        }
        DeviceBuffer* buffer = (DeviceBuffer*) vertex_buffer;
        Require(buffer->m_Pool && buffer->m_Allocation.m_MappedDataPtr, "dynamic backing remains mapped");
        if (frame == 8)
        {
            retained[0] = buffer->m_Allocation.m_Handle.m_Buffer;
            for (uint32_t i = 0; i < DM_MAX_FRAMES_IN_FLIGHT; ++i)
            {
                retained[i + 1] = buffer->m_Pool->m_Backing[i].m_Handle.m_Buffer;
            }
        }
        else if (frame > 8)
        {
            bool reused = false;
            for (uint32_t i = 0; i < DM_ARRAY_SIZE(retained); ++i)
            {
                reused |= retained[i] == buffer->m_Allocation.m_Handle.m_Buffer;
            }
            Require(reused, "steady uploads must not allocate new backings");
        }
        fixture.DrawCell(0, vertex_buffer, index_buffer);
        fixture.DrawCell(1, vertex_buffer);
        if (frame == 15)
        {
            const bool expected[] = { false, true };
            fixture.CheckCellColors(2, expected);
        }
        fixture.End();
    }

    // Deletion must enqueue both the active allocations and the cached spares.
    uint32_t expected_retirements = BackingCount((DeviceBuffer*) vertex_buffer) + BackingCount((DeviceBuffer*) index_buffer);
    uint32_t pending = PendingResources(fixture);
    DeleteVertexBuffer(vertex_buffer);
    DeleteIndexBuffer(index_buffer);
    Require(PendingResources(fixture) == pending + expected_retirements, "delete must retire active and cached backings");
    puts("PASS: reuse, pre-frame updates and deletion");
}

// Verify cache overflow, growth and shrink preserve all earlier vertex/index
// draws, including more replacements in one frame than the cache can retain.
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
    puts("PASS: repeated replacements, growth, shrink and cache eviction");
}

// Verify partial writes preserve earlier draws and untouched bytes at nonzero
// vertex/index offsets when a cached backing must be replaced.
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

    // Usage changes release/recreate the dynamic cache; zero-sized writes do neither.
    SetVertexBufferData(vertex_buffer, sizeof(TRIANGLES), TRIANGLES, BUFFER_USAGE_STATIC_DRAW);
    Require(!((DeviceBuffer*) vertex_buffer)->m_Pool, "static replacement releases cache");
    SetVertexBufferData(vertex_buffer, sizeof(TRIANGLES), TRIANGLES, BUFFER_USAGE_DYNAMIC_DRAW);
    Require(((DeviceBuffer*) vertex_buffer)->m_Pool != 0, "dynamic replacement recreates cache");
    SetVertexBufferData(vertex_buffer, 0, 0, BUFFER_USAGE_DYNAMIC_DRAW);
    Require(((DeviceBuffer*) vertex_buffer)->m_Base.m_Size == sizeof(TRIANGLES), "zero-size update retains existing behavior");
    DeleteVertexBuffer(vertex_buffer);
    DeleteIndexBuffer(index_buffer);
    puts("PASS: partial writes, usage transitions and zero-size update");
}

// Verify potentially GPU-written transfer/storage buffers retain their existing
// allocation path rather than entering the CPU-authored dynamic cache.
static void TestExcludedUsages(Fixture& fixture)
{
    HVertexBuffer vertex_buffer = NewVertexBuffer(fixture.m_Context, sizeof(TRIANGLES), TRIANGLES, (BufferUsage)(BUFFER_USAGE_DYNAMIC_DRAW | BUFFER_USAGE_TRANSFER));
    SetVertexBufferData(vertex_buffer, sizeof(TRIANGLES), TRIANGLES, BUFFER_USAGE_DYNAMIC_DRAW);
    Require(!((DeviceBuffer*) vertex_buffer)->m_Pool, "transfer buffer must not enter dynamic cache");

    HStorageBuffer storage = NewStorageBuffer(fixture.m_Context, sizeof(TRIANGLES), TRIANGLES, BUFFER_USAGE_DYNAMIC_DRAW);
    SetStorageBufferData(fixture.m_Context, storage, sizeof(TRIANGLES), TRIANGLES, BUFFER_USAGE_DYNAMIC_DRAW);
    Require(!((DeviceBuffer*) storage)->m_Pool, "storage buffer must not enter dynamic cache");
    DeleteVertexBuffer(vertex_buffer);
    DeleteStorageBuffer(fixture.m_Context, storage);
    puts("PASS: transfer and storage exclusions");
}

extern "C" void dmExportedSymbols();
int main()
{
    dmExportedSymbols();
    TestMainPlatformInit();
    Fixture fixture = {};
    fixture.Init();
    TestReuseAndDeletion(fixture);
    TestRepeatedReplacement(fixture);
    TestPartialUpdates(fixture);
    TestExcludedUsages(fixture);
    fixture.Delete();
    return 0;
}
