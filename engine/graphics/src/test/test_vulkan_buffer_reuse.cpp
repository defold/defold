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

static const float TRIANGLES[] = { -1, -1, 3, -1, -1, 3, 7, -1, 11, -1, 7, 3 };
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
        window.m_Width = 512;
        window.m_Height = 64;
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
        params.m_Width = 512;
        params.m_Height = 64;
        params.m_JobContext = m_Jobs;
        params.m_VerifyGraphicsCalls = 1;
        params.m_UseValidationLayers = 1;
        m_Context = NewContext(params);
        Require(m_Context != 0, "create Vulkan context");
        SetSwapInterval(m_Context, 0);
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
        RenderTargetCreationParams target = {};
        target.m_SampleCount = 1;
        target.m_ColorBufferCreationParams[0].m_Width = 512;
        target.m_ColorBufferCreationParams[0].m_Height = 64;
        target.m_ColorBufferParams[0].m_Width = 512;
        target.m_ColorBufferParams[0].m_Height = 64;
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

    void DrawCell(uint32_t cell, HVertexBuffer vertices, HIndexBuffer indices = 0, uint32_t index_offset = 0)
    {
        SetViewport(m_Context, cell * 32, 0, 32, 64);
        EnableVertexBuffer(m_Context, vertices, 0);
        EnableVertexDeclaration(m_Context, m_Declaration, 0, 0, m_Program);
        if (indices)
        {
            DrawElements(m_Context, PRIMITIVE_TRIANGLES, index_offset, 3, TYPE_UNSIGNED_SHORT, indices, 1);
        }
        else
        {
            Draw(m_Context, PRIMITIVE_TRIANGLES, 0, 3, 1);
        }
    }

    void Check(uint32_t count, const bool* white)
    {
        uint8_t pixels[512 * 64 * 4];
        ReadPixels(m_Context, 0, 0, 512, 64, pixels, sizeof(pixels));
        for (uint32_t cell = 0; cell < count; ++cell)
        {
            const uint8_t* pixel = pixels + (32 * 512 + cell * 32 + 16) * 4;
            uint8_t expected = white[cell] ? 255 : 0;
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
    uint32_t count = buffer->m_Handle.m_Buffer != VK_NULL_HANDLE;
    for (uint32_t i = 0; buffer->m_Pool && i < DM_MAX_FRAMES_IN_FLIGHT; ++i)
    {
        count += buffer->m_Pool->m_Backing[i].m_Handle.m_Buffer != VK_NULL_HANDLE;
    }
    return count;
}

// Verify pre-frame and in-frame uploads reuse retained allocations after fences,
// and deleting a logical buffer retires every cached allocation exactly once.
static void TestReuseAndDeletion(Fixture& fixture)
{
    HVertexBuffer vb = NewVertexBuffer(fixture.m_Context, sizeof(TRIANGLES), TRIANGLES, BUFFER_USAGE_DYNAMIC_DRAW);
    HIndexBuffer ib = NewIndexBuffer(fixture.m_Context, 6, INDICES, BUFFER_USAGE_STREAM_DRAW);
    VkBuffer retained[4] = {};
    for (uint32_t frame = 0; frame < 16; ++frame)
    {
        if (frame % 2)
        {
            fixture.Begin();
        }
        SetVertexBufferData(vb, sizeof(TRIANGLES), TRIANGLES, BUFFER_USAGE_DYNAMIC_DRAW);
        SetIndexBufferData(ib, 6, INDICES + (frame % 2) * 3, BUFFER_USAGE_STREAM_DRAW);
        if (!(frame % 2))
        {
            fixture.Begin();
        }
        DeviceBuffer* buffer = (DeviceBuffer*) vb;
        Require(buffer->m_Pool && buffer->m_MappedDataPtr, "dynamic backing remains mapped");
        if (frame == 8)
        {
            retained[0] = buffer->m_Handle.m_Buffer;
            for (uint32_t i = 0; i < 3; ++i)
            {
                retained[i + 1] = buffer->m_Pool->m_Backing[i].m_Handle.m_Buffer;
            }
        }
        else if (frame > 8)
        {
            bool reused = false;
            for (uint32_t i = 0; i < 4; ++i)
            {
                reused |= retained[i] == buffer->m_Handle.m_Buffer;
            }
            Require(reused, "steady uploads must not allocate new backings");
        }
        fixture.DrawCell(0, vb, ib);
        fixture.DrawCell(1, vb);
        if (frame == 15)
        {
            const bool expected[] = { false, true };
            fixture.Check(2, expected);
        }
        fixture.End();
    }
    uint32_t expected_retirements = BackingCount((DeviceBuffer*) vb) + BackingCount((DeviceBuffer*) ib);
    uint32_t pending = PendingResources(fixture);
    DeleteVertexBuffer(vb);
    DeleteIndexBuffer(ib);
    Require(PendingResources(fixture) == pending + expected_retirements, "delete must retire active and cached backings");
    puts("PASS: reuse, pre-frame updates and deletion");
}

// Verify cache overflow, growth and shrink preserve all earlier vertex/index
// draws, including more replacements in one frame than the cache can retain.
static void TestRepeatedReplacement(Fixture& fixture)
{
    HVertexBuffer vb = NewVertexBuffer(fixture.m_Context, 0, 0, BUFFER_USAGE_STREAM_DRAW);
    HVertexBuffer indexed = NewVertexBuffer(fixture.m_Context, sizeof(TRIANGLES), TRIANGLES, BUFFER_USAGE_STATIC_DRAW);
    HIndexBuffer ib = NewIndexBuffer(fixture.m_Context, 0, 0, BUFFER_USAGE_DYNAMIC_DRAW);
    bool expected[16];
    for (uint32_t frame = 0; frame < 6; ++frame)
    {
        fixture.Begin();
        for (uint32_t cell = 0; cell < 8; ++cell)
        {
            uint32_t outside = (cell + frame) % 2;
            float vertices[24] = {};
            memcpy(vertices, TRIANGLES + outside * 6, 24);
            uint32_t size = 24 * (1 + cell % 4);
            SetVertexBufferData(vb, size, vertices, BUFFER_USAGE_STREAM_DRAW);
            Require(((DeviceBuffer*) vb)->m_Base.m_Size == size, "logical vertex size follows shrink/growth");
            fixture.DrawCell(cell, vb);
            uint16_t indices[12] = {};
            memcpy(indices, INDICES + outside * 3, 6);
            size = 6 * (1 + cell % 4);
            SetIndexBufferData(ib, size, indices, BUFFER_USAGE_DYNAMIC_DRAW);
            Require(((DeviceBuffer*) ib)->m_Base.m_Size == size, "logical index size follows shrink/growth");
            fixture.DrawCell(cell + 8, indexed, ib);
            expected[cell] = expected[cell + 8] = !outside;
        }
        if (frame == 5)
        {
            fixture.Check(16, expected);
        }
        fixture.End();
    }
    DeleteVertexBuffer(vb);
    DeleteVertexBuffer(indexed);
    DeleteIndexBuffer(ib);
    puts("PASS: repeated replacements, growth, shrink and cache eviction");
}

// Verify partial writes preserve earlier draws and untouched bytes at nonzero
// vertex/index offsets when a cached backing must be replaced.
static void TestPartialUpdates(Fixture& fixture)
{
    HVertexBuffer vb = NewVertexBuffer(fixture.m_Context, sizeof(TRIANGLES), TRIANGLES, BUFFER_USAGE_DYNAMIC_DRAW);
    HIndexBuffer ib = NewIndexBuffer(fixture.m_Context, sizeof(INDICES), INDICES, BUFFER_USAGE_DYNAMIC_DRAW);
    fixture.Begin();
    fixture.DrawCell(0, vb, ib);
    SetVertexBufferSubData(vb, 24, 24, TRIANGLES);
    fixture.DrawCell(1, vb, ib, 6);
    fixture.DrawCell(2, vb, ib);
    SetVertexBufferSubData(vb, 0, 24, TRIANGLES + 6);
    fixture.DrawCell(3, vb, ib);
    fixture.DrawCell(4, vb, ib, 6);
    SetIndexBufferSubData(ib, 6, 6, INDICES);
    fixture.DrawCell(5, vb, ib, 6);
    fixture.DrawCell(6, vb, ib);
    SetIndexBufferSubData(ib, 0, 6, INDICES + 3);
    fixture.DrawCell(7, vb, ib);
    fixture.DrawCell(8, vb, ib, 6);
    const bool expected[] = { true, true, true, false, true, false, false, true, false };
    fixture.Check(9, expected);
    fixture.End();
    SetVertexBufferData(vb, sizeof(TRIANGLES), TRIANGLES, BUFFER_USAGE_STATIC_DRAW);
    Require(!((DeviceBuffer*) vb)->m_Pool, "static replacement releases cache");
    SetVertexBufferData(vb, sizeof(TRIANGLES), TRIANGLES, BUFFER_USAGE_DYNAMIC_DRAW);
    Require(((DeviceBuffer*) vb)->m_Pool != 0, "dynamic replacement recreates cache");
    SetVertexBufferData(vb, 0, 0, BUFFER_USAGE_DYNAMIC_DRAW);
    Require(((DeviceBuffer*) vb)->m_Base.m_Size == sizeof(TRIANGLES), "zero-size update retains existing behavior");
    DeleteVertexBuffer(vb);
    DeleteIndexBuffer(ib);
    puts("PASS: partial writes, usage transitions and zero-size update");
}

// Verify potentially GPU-written transfer/storage buffers retain their existing
// allocation path rather than entering the CPU-authored dynamic cache.
static void TestExcludedUsages(Fixture& fixture)
{
    HVertexBuffer vb = NewVertexBuffer(fixture.m_Context, sizeof(TRIANGLES), TRIANGLES, (BufferUsage)(BUFFER_USAGE_DYNAMIC_DRAW | BUFFER_USAGE_TRANSFER));
    SetVertexBufferData(vb, sizeof(TRIANGLES), TRIANGLES, BUFFER_USAGE_DYNAMIC_DRAW);
    Require(!((DeviceBuffer*) vb)->m_Pool, "transfer buffer must not enter dynamic cache");
    HStorageBuffer storage = NewStorageBuffer(fixture.m_Context, sizeof(TRIANGLES), TRIANGLES, BUFFER_USAGE_DYNAMIC_DRAW);
    SetStorageBufferData(fixture.m_Context, storage, sizeof(TRIANGLES), TRIANGLES, BUFFER_USAGE_DYNAMIC_DRAW);
    Require(!((DeviceBuffer*) storage)->m_Pool, "storage buffer must not enter dynamic cache");
    DeleteVertexBuffer(vb);
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
