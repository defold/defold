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

#include <string.h>

#include <platform/window.hpp>
#include <testmain/testmain.h>

#include "../../../graphics/src/null/graphics_null_private.h"
#include "render/render_graphics_resource.h"

using namespace dmRender;

extern "C" void dmExportedSymbols();

// Captures game-owner completions without retaining any borrowed request storage.
struct ResourceCompletionProbe
{
    GraphicsResourceResponse m_Response;
    GraphicsResourceResult   m_Result;
    uint32_t                 m_Count;
};

static void CompleteResource(void* context, GraphicsResourceResult result, const GraphicsResourceResponse& response)
{
    ResourceCompletionProbe* probe = (ResourceCompletionProbe*)context;
    probe->m_Response = response;
    probe->m_Result = result;
    ++probe->m_Count;
}

class RenderGraphicsResource : public jc_test_base_class
{
    protected:
    dmGraphics::HContext m_Graphics;
    HGraphicsService     m_Service;
    HWindow              m_Window;

    virtual void SetUp()
    {
        dmGraphics::InstallAdapter(dmGraphics::ADAPTER_FAMILY_NONE);
        WindowCreateParams window_params;
        WindowCreateParamsInitialize(&window_params);
        window_params.m_Width = 20;
        window_params.m_Height = 10;
        m_Window = dmPlatform::NewWindow();
        dmPlatform::OpenWindow(m_Window, window_params);

        dmGraphics::ContextParams graphics_params;
        graphics_params.m_Width = 20;
        graphics_params.m_Height = 10;
        graphics_params.m_Window = m_Window;
        m_Graphics = dmGraphics::NewContext(graphics_params);
        ASSERT_NE((dmGraphics::HContext)0, m_Graphics);

        GraphicsServiceParams service_params = {};
        service_params.m_GraphicsContext = m_Graphics;
        service_params.m_Mode = EXECUTION_MODE_INLINE;
        service_params.m_MaxRequests = 4;
        service_params.m_MaxPayloadBytes = 16384;
        ExecutionSelection selection;
        m_Service = NewGraphicsService(service_params, &selection);
        ASSERT_NE((HGraphicsService)0, m_Service);
    }

    virtual void TearDown()
    {
        StopGraphicsService(m_Service);
        PumpGraphicsService(m_Service, 4);
        DispatchGraphicsCompletions(m_Service);
        DeleteGraphicsService(m_Service);
        dmGraphics::CloseWindow(m_Graphics);
        dmGraphics::DeleteContext(m_Graphics);
        dmPlatform::CloseWindow(m_Window);
        dmPlatform::DeleteWindow(m_Window);
    }

    void FinishRequests(uint32_t count)
    {
        ASSERT_EQ(GRAPHICS_SERVICE_PUMP_PROGRESSED, PumpGraphicsService(m_Service, count));
        ASSERT_EQ(count, DispatchGraphicsCompletions(m_Service));
        ASSERT_EQ(0u, GetGraphicsServiceStatus(m_Service).m_PendingRequests);
    }

    void CreateTexture(uint16_t width, uint16_t height, dmGraphics::HTexture* texture)
    {
        ResourceCompletionProbe probe = {};
        dmGraphics::TextureCreationParams params;
        params.m_Width = width;
        params.m_Height = height;
        ASSERT_EQ(GRAPHICS_SERVICE_OK, EnqueueCreateTexture(m_Service, params, CompleteResource, &probe));
        FinishRequests(1);
        ASSERT_EQ(GRAPHICS_RESOURCE_OK, probe.m_Result);
        *texture = probe.m_Response.m_Handle;
    }

    void DeleteTexture(dmGraphics::HTexture texture)
    {
        ResourceCompletionProbe probe = {};
        ASSERT_EQ(GRAPHICS_SERVICE_OK, EnqueueDeleteTexture(m_Service, texture, CompleteResource, &probe));
        FinishRequests(1);
        ASSERT_EQ(GRAPHICS_RESOURCE_OK, probe.m_Result);
    }
};

// Resource realization and copied uploads work before any frame; completions remain deferred.
TEST_F(RenderGraphicsResource, TextureLifecycleWithoutFrames)
{
    ResourceCompletionProbe created = {};
    dmGraphics::TextureCreationParams creation;
    creation.m_Width = 4;
    creation.m_Height = 2;
    ASSERT_EQ(GRAPHICS_SERVICE_OK, EnqueueCreateTexture(m_Service, creation, CompleteResource, &created));
    creation.m_Width = 9;
    ASSERT_EQ(0u, created.m_Count);
    ASSERT_EQ(GRAPHICS_SERVICE_PUMP_PROGRESSED, PumpGraphicsService(m_Service, 1));
    ASSERT_EQ(0u, created.m_Count);
    ASSERT_EQ(1u, DispatchGraphicsCompletions(m_Service));
    ASSERT_EQ(GRAPHICS_RESOURCE_OK, created.m_Result);
    const dmGraphics::HTexture texture = created.m_Response.m_Handle;
    ASSERT_NE((dmGraphics::HTexture)0, texture);

    uint8_t pixels[32];
    memset(pixels, 17, sizeof(pixels));
    dmGraphics::TextureParams upload;
    upload.m_Width = 4;
    upload.m_Height = 2;
    upload.m_Data = pixels;
    upload.m_DataSize = sizeof(pixels);
    ResourceCompletionProbe uploaded = {};
    ResourceCompletionProbe queried = {};
    ASSERT_EQ(GRAPHICS_SERVICE_OK, EnqueueUploadTexture(m_Service, texture, upload, CompleteResource, &uploaded));
    ASSERT_EQ(GRAPHICS_SERVICE_OK, EnqueueGetTextureInfo(m_Service, texture, CompleteResource, &queried));
    memset(pixels, 99, sizeof(pixels));
    upload.m_Width = 11;
    FinishRequests(2);

    ASSERT_EQ(GRAPHICS_RESOURCE_OK, uploaded.m_Result);
    ASSERT_EQ(GRAPHICS_RESOURCE_OK, queried.m_Result);
    ASSERT_EQ(4u, queried.m_Response.m_TextureInfo.m_Width);
    ASSERT_EQ(2u, queried.m_Response.m_TextureInfo.m_Height);
    uint8_t* uploaded_pixels = 0;
    ASSERT_EQ(dmGraphics::HANDLE_RESULT_OK, dmGraphics::GetTextureHandle(texture, (void**)&uploaded_pixels));
    for (uint32_t i = 0; i < sizeof(pixels); ++i)
    {
        ASSERT_EQ(17u, uploaded_pixels[i]);
    }

    DeleteTexture(texture);
    ASSERT_FALSE(dmGraphics::IsAssetHandleValid(m_Graphics, texture));
}

// Reject unknown byte layouts before admission, and report owner-side bounds/handle failures once.
TEST_F(RenderGraphicsResource, TextureValidationAndExecutionFailures)
{
    dmGraphics::HTexture texture;
    CreateTexture(4, 4, &texture);
    uint8_t pixels[16] = {};
    dmGraphics::TextureParams upload;
    upload.m_Width = 2;
    upload.m_Height = 2;
    upload.m_Data = pixels;
    upload.m_DataSize = sizeof(pixels) - 1;
    ResourceCompletionProbe invalid = {};
    ASSERT_EQ(GRAPHICS_SERVICE_INVALID_ARGUMENT, EnqueueUploadTexture(m_Service, texture, upload, CompleteResource, &invalid));
    upload.m_DataSize = sizeof(pixels);
    upload.m_LayerCount = 2;
    ASSERT_EQ(GRAPHICS_SERVICE_INVALID_ARGUMENT, EnqueueUploadTexture(m_Service, texture, upload, CompleteResource, &invalid));
    upload.m_LayerCount = 1;
    upload.m_Format = dmGraphics::TEXTURE_FORMAT_RGB_BC1;
    ASSERT_EQ(GRAPHICS_SERVICE_INVALID_ARGUMENT, EnqueueUploadTexture(m_Service, texture, upload, CompleteResource, &invalid));
    ASSERT_EQ(0u, invalid.m_Count);
    ASSERT_EQ(0u, GetGraphicsServiceStatus(m_Service).m_PendingRequests);

    upload.m_Format = dmGraphics::TEXTURE_FORMAT_RGBA;
    upload.m_SubUpdate = true;
    upload.m_X = 3;
    ASSERT_EQ(GRAPHICS_SERVICE_OK, EnqueueUploadTexture(m_Service, texture, upload, CompleteResource, &invalid));
    FinishRequests(1);
    ASSERT_EQ(GRAPHICS_RESOURCE_INVALID, invalid.m_Result);
    ASSERT_EQ(1u, invalid.m_Count);

    upload.m_SubUpdate = false;
    upload.m_X = 0;
    upload.m_MipMap = 127;
    ASSERT_EQ(GRAPHICS_SERVICE_OK, EnqueueUploadTexture(m_Service, texture, upload, CompleteResource, &invalid));
    FinishRequests(1);
    ASSERT_EQ(GRAPHICS_RESOURCE_INVALID, invalid.m_Result);
    ASSERT_EQ(2u, invalid.m_Count);

    DeleteTexture(texture);
    ASSERT_EQ(GRAPHICS_SERVICE_OK, EnqueueGetTextureInfo(m_Service, texture, CompleteResource, &invalid));
    FinishRequests(1);
    ASSERT_EQ(GRAPHICS_RESOURCE_INVALID, invalid.m_Result);
    ASSERT_EQ(3u, invalid.m_Count);
}

// Suspended shutdown cancels an unstarted create without realizing a handle or calling back inline.
TEST_F(RenderGraphicsResource, CancelledCreateCompletesWithoutGraphicsWork)
{
    dmGraphics::TextureCreationParams params;
    params.m_Width = 4;
    params.m_Height = 2;
    ResourceCompletionProbe probe = {};
    ASSERT_EQ(GRAPHICS_SERVICE_OK, EnqueueCreateTexture(m_Service, params, CompleteResource, &probe));

    CancelGraphicsService(m_Service, GRAPHICS_TASK_CANCELLED);
    ASSERT_EQ(0u, probe.m_Count);
    ASSERT_EQ((uint64_t)0, GetGraphicsServiceStatus(m_Service).m_Executed);
    ASSERT_EQ((uint64_t)1, GetGraphicsServiceStatus(m_Service).m_Cancelled);
    ASSERT_EQ(1u, DispatchGraphicsCompletions(m_Service));
    ASSERT_EQ(1u, probe.m_Count);
    ASSERT_EQ(GRAPHICS_RESOURCE_CANCELLED, probe.m_Result);
    ASSERT_EQ((uint64_t)0, probe.m_Response.m_Handle);

    ASSERT_EQ(GRAPHICS_SERVICE_PUMP_DRAINED, PumpGraphicsService(m_Service, 4));
    ASSERT_EQ(0u, DispatchGraphicsCompletions(m_Service));
    ASSERT_EQ(1u, probe.m_Count);
}

// Vertex/index operations own submitted bytes and preserve request ordering without frame submission.
TEST_F(RenderGraphicsResource, BufferLifecycleCopiesUploads)
{
    uint8_t data[16];
    memset(data, 3, sizeof(data));
    ResourceCompletionProbe vertex = {};
    ResourceCompletionProbe index = {};
    ASSERT_EQ(GRAPHICS_SERVICE_OK, EnqueueCreateVertexBuffer(m_Service, sizeof(data), data, dmGraphics::BUFFER_USAGE_STREAM_DRAW, CompleteResource, &vertex));
    ASSERT_EQ(GRAPHICS_SERVICE_OK, EnqueueCreateIndexBuffer(m_Service, sizeof(data), data, dmGraphics::BUFFER_USAGE_STREAM_DRAW, CompleteResource, &index));
    memset(data, 7, sizeof(data));
    FinishRequests(2);
    ASSERT_EQ(GRAPHICS_RESOURCE_OK, vertex.m_Result);
    ASSERT_EQ(GRAPHICS_RESOURCE_OK, index.m_Result);
    const dmGraphics::HVertexBuffer vertex_buffer = (dmGraphics::HVertexBuffer)vertex.m_Response.m_Handle;
    const dmGraphics::HIndexBuffer index_buffer = (dmGraphics::HIndexBuffer)index.m_Response.m_Handle;
    dmGraphics::VertexBuffer* vb = (dmGraphics::VertexBuffer*)vertex_buffer;
    dmGraphics::IndexBuffer* ib = (dmGraphics::IndexBuffer*)index_buffer;
    for (uint32_t i = 0; i < sizeof(data); ++i)
    {
        ASSERT_EQ(3u, ((uint8_t*)vb->m_Buffer)[i]);
        ASSERT_EQ(3u, ((uint8_t*)ib->m_Buffer)[i]);
    }

    ASSERT_EQ(GRAPHICS_SERVICE_OK, EnqueueUploadVertexBuffer(m_Service, vertex_buffer, sizeof(data), data, dmGraphics::BUFFER_USAGE_DYNAMIC_DRAW, CompleteResource, &vertex));
    ASSERT_EQ(GRAPHICS_SERVICE_OK, EnqueueUploadIndexBuffer(m_Service, index_buffer, sizeof(data), data, dmGraphics::BUFFER_USAGE_DYNAMIC_DRAW, CompleteResource, &index));
    memset(data, 11, sizeof(data));
    FinishRequests(2);
    for (uint32_t i = 0; i < sizeof(data); ++i)
    {
        ASSERT_EQ(7u, ((uint8_t*)vb->m_Buffer)[i]);
        ASSERT_EQ(7u, ((uint8_t*)ib->m_Buffer)[i]);
    }

    ASSERT_EQ(GRAPHICS_SERVICE_OK, EnqueueDeleteVertexBuffer(m_Service, vertex_buffer, CompleteResource, &vertex));
    ASSERT_EQ(GRAPHICS_SERVICE_OK, EnqueueDeleteIndexBuffer(m_Service, index_buffer, CompleteResource, &index));
    FinishRequests(2);
    ASSERT_EQ(3u, vertex.m_Count);
    ASSERT_EQ(3u, index.m_Count);
}

// A full service and an oversized upload reject without touching source bytes or making a callback.
TEST_F(RenderGraphicsResource, ResourceAdmissionLimits)
{
    ResourceCompletionProbe created[4] = {};
    for (uint32_t i = 0; i < 4; ++i)
    {
        ASSERT_EQ(GRAPHICS_SERVICE_OK, EnqueueCreateVertexBuffer(m_Service, 16, 0, dmGraphics::BUFFER_USAGE_STATIC_DRAW, CompleteResource, &created[i]));
    }

    ResourceCompletionProbe rejected = {};
    const void* unavailable_bytes = (const void*)(uintptr_t)1;
    ASSERT_EQ(GRAPHICS_SERVICE_WOULD_BLOCK, EnqueueCreateVertexBuffer(m_Service, 16, unavailable_bytes, dmGraphics::BUFFER_USAGE_STATIC_DRAW, CompleteResource, &rejected));
    ASSERT_EQ(0u, rejected.m_Count);
    FinishRequests(4);

    ASSERT_EQ(GRAPHICS_SERVICE_INVALID_ARGUMENT, EnqueueCreateVertexBuffer(m_Service, 16384, unavailable_bytes, dmGraphics::BUFFER_USAGE_STATIC_DRAW, CompleteResource, &rejected));
    ASSERT_EQ(0u, rejected.m_Count);
    for (uint32_t i = 0; i < 4; ++i)
    {
        ASSERT_EQ(GRAPHICS_SERVICE_OK, EnqueueDeleteVertexBuffer(m_Service, (dmGraphics::HVertexBuffer)created[i].m_Response.m_Handle, CompleteResource, &created[i]));
    }
    FinishRequests(4);
}

static dmGraphics::RenderTargetCreationParams MakeTargetParams(uint16_t width, uint16_t height)
{
    dmGraphics::RenderTargetCreationParams params;
    params.m_ColorBufferCreationParams[0].m_Width = width;
    params.m_ColorBufferCreationParams[0].m_Height = height;
    params.m_ColorBufferParams[0].m_Width = width;
    params.m_ColorBufferParams[0].m_Height = height;
    params.m_ColorBufferParams[0].m_Format = dmGraphics::TEXTURE_FORMAT_RGBA;
    return params;
}

// Target creation copies attachment descriptors, then resize/query/delete execute in FIFO order.
TEST_F(RenderGraphicsResource, RenderTargetLifecycleWithoutFrames)
{
    dmGraphics::RenderTargetCreationParams params = MakeTargetParams(4, 2);
    ResourceCompletionProbe created = {};
    ASSERT_EQ(GRAPHICS_SERVICE_OK, EnqueueCreateRenderTarget(m_Service, dmGraphics::BUFFER_TYPE_COLOR0_BIT, params, CompleteResource, &created));
    params.m_ColorBufferParams[0].m_Width = 9;
    FinishRequests(1);
    ASSERT_EQ(GRAPHICS_RESOURCE_OK, created.m_Result);
    const dmGraphics::HRenderTarget target = created.m_Response.m_Handle;

    ResourceCompletionProbe initial = {};
    ResourceCompletionProbe resized = {};
    ResourceCompletionProbe after_resize = {};
    ASSERT_EQ(GRAPHICS_SERVICE_OK, EnqueueGetRenderTargetSize(m_Service, target, dmGraphics::BUFFER_TYPE_COLOR0_BIT, CompleteResource, &initial));
    ASSERT_EQ(GRAPHICS_SERVICE_OK, EnqueueResizeRenderTarget(m_Service, target, 8, 6, CompleteResource, &resized));
    ASSERT_EQ(GRAPHICS_SERVICE_OK, EnqueueGetRenderTargetSize(m_Service, target, dmGraphics::BUFFER_TYPE_COLOR0_BIT, CompleteResource, &after_resize));
    FinishRequests(3);
    ASSERT_EQ(4u, initial.m_Response.m_Width);
    ASSERT_EQ(2u, initial.m_Response.m_Height);
    ASSERT_EQ(8u, after_resize.m_Response.m_Width);
    ASSERT_EQ(6u, after_resize.m_Response.m_Height);

    ResourceCompletionProbe removed = {};
    ASSERT_EQ(GRAPHICS_SERVICE_OK, EnqueueDeleteRenderTarget(m_Service, target, CompleteResource, &removed));
    FinishRequests(1);
    ASSERT_EQ(GRAPHICS_RESOURCE_OK, removed.m_Result);
    ASSERT_FALSE(dmGraphics::IsAssetHandleValid(m_Graphics, target));
}

// Pointer-bearing target uploads are rejected, while adapter creation failure is a deferred result.
TEST_F(RenderGraphicsResource, RenderTargetValidationAndFailure)
{
    dmGraphics::RenderTargetCreationParams params = MakeTargetParams(4, 2);
    uint8_t pixels[32] = {};
    params.m_ColorBufferParams[0].m_Data = pixels;
    ResourceCompletionProbe probe = {};
    ASSERT_EQ(GRAPHICS_SERVICE_INVALID_ARGUMENT, EnqueueCreateRenderTarget(m_Service, dmGraphics::BUFFER_TYPE_COLOR0_BIT, params, CompleteResource, &probe));
    ASSERT_EQ(0u, probe.m_Count);
    params.m_ColorBufferParams[0].m_Data = 0;
    ASSERT_EQ(GRAPHICS_SERVICE_OK, EnqueueCreateRenderTarget(m_Service, 0, params, CompleteResource, &probe));
    FinishRequests(1);
    ASSERT_EQ(GRAPHICS_RESOURCE_FAILED, probe.m_Result);
    ASSERT_EQ((uint64_t)0, probe.m_Response.m_Handle);

    ASSERT_EQ(GRAPHICS_SERVICE_INVALID_ARGUMENT, EnqueueResizeRenderTarget(m_Service, 1, 65536, 1, CompleteResource, &probe));
    ASSERT_EQ(GRAPHICS_SERVICE_INVALID_ARGUMENT, EnqueueGetRenderTargetSize(m_Service, 1, (dmGraphics::BufferType)0, CompleteResource, &probe));
    ASSERT_EQ(0u, GetGraphicsServiceStatus(m_Service).m_PendingRequests);
}

int main(int argc, char** argv)
{
    dmExportedSymbols();
    TestMainPlatformInit();
    jc_test_init(&argc, argv);
    return jc_test_run_all();
}
