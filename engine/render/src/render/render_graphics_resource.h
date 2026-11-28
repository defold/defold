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

#ifndef DM_RENDER_GRAPHICS_RESOURCE_H
#define DM_RENDER_GRAPHICS_RESOURCE_H

#include <graphics/graphics.h>

#include "render_graphics_service.h"

namespace dmRender
{
    /** Result of executing a resource request, separate from its admission result. */
    enum GraphicsResourceResult
    {
        GRAPHICS_RESOURCE_CANCELLED    = GRAPHICS_SERVICE_TASK_CANCELLED,
        GRAPHICS_RESOURCE_CONTEXT_LOST = GRAPHICS_SERVICE_CONTEXT_LOST,
        GRAPHICS_RESOURCE_OK           = 0,
        GRAPHICS_RESOURCE_INVALID      = 1,
        GRAPHICS_RESOURCE_UNSUPPORTED  = 2,
        GRAPHICS_RESOURCE_FAILED       = 3,
    };

    /** Created handle or queried metadata; unused fields are zero. No resource is retained. */
    struct GraphicsResourceResponse
    {
        dmGraphics::TextureInfo m_TextureInfo;
        uint64_t                m_Handle;
        uint32_t                m_Width;
        uint32_t                m_Height;
    };

    /**
     * Called on the game thread when the service delivers a request's result.
     * The enqueue functions never call this callback directly.
     * Copy any response values you need to keep; the response is only valid during this call.
     * The caller owns any newly created resource and must delete it when no longer needed.
     *
     * Success means the graphics API call finished, not that the GPU finished its work.
     * If the graphics API provides no error result, this callback cannot report GPU failures.
     * @param context Caller data, kept alive until this callback returns
     * @param result Whether the operation succeeded, failed or was cancelled
     * @param response Result data, valid only during this callback
     */
    typedef void (*FGraphicsResourceComplete)(void* context, GraphicsResourceResult result, const GraphicsResourceResponse& response);

    /**
     * These functions queue resource operations. When a request is accepted, its parameters
     * and any upload bytes are copied before the enqueue function returns. If enqueue fails,
     * no operation is queued and no completion callback will run.
     *
     * The caller must keep existing resources alive while requests use them. Before
     * queuing a change or deletion, make sure no queued or currently rendering frame still
     * needs that resource. The service does not protect resources or wait for those frames.
     * Context loss fails queued requests and rejects new ones with CONTEXT_LOST. After
     * restoration, recreate lost resources before resubmitting; old requests are not replayed.
     *
     * GPU readback, mapped buffers and shaders are not supported here yet. They need
     * additional rules for when their data can be used or freed. Browser readback must
     * also avoid blocking the browser's main thread while waiting for the GPU.
     */

    /** Create an ordinary 2D texture; the resulting handle is delivered in the response. */
    GraphicsServiceResult EnqueueCreateTexture(HGraphicsService service, const dmGraphics::TextureCreationParams& params, FGraphicsResourceComplete complete, void* context);

    /**
     * Copy and upload one 2D mip/subregion. Supports LUMINANCE, LUMINANCE_ALPHA, RGB and RGBA
     * byte data only, with DataSize exactly width * height * channels. Arrays, cube maps,
     * volumes and compressed formats are excluded until their full payload sizes are explicit.
     * Storage and mip bounds are checked when the request runs. A partial update needs
     * an allocated mip; otherwise completion reports GRAPHICS_RESOURCE_INVALID.
     */
    GraphicsServiceResult EnqueueUploadTexture(HGraphicsService service, dmGraphics::HTexture texture, const dmGraphics::TextureParams& params, FGraphicsResourceComplete complete, void* context);

    /** Query copied texture metadata; the callback does not retain the texture. */
    GraphicsServiceResult EnqueueGetTextureInfo(HGraphicsService service, dmGraphics::HTexture texture, FGraphicsResourceComplete complete, void* context);

    /** Delete an exclusively owned texture after all its captured users have finished. */
    GraphicsServiceResult EnqueueDeleteTexture(HGraphicsService service, dmGraphics::HTexture texture, FGraphicsResourceComplete complete, void* context);

    /** Create a vertex buffer, copying size bytes when data is supplied. */
    GraphicsServiceResult EnqueueCreateVertexBuffer(HGraphicsService service, uint32_t size, const void* data, dmGraphics::BufferUsage usage, FGraphicsResourceComplete complete, void* context);

    /** Replace vertex-buffer contents, copying size bytes when data is supplied. */
    GraphicsServiceResult EnqueueUploadVertexBuffer(HGraphicsService service, dmGraphics::HVertexBuffer buffer, uint32_t size, const void* data, dmGraphics::BufferUsage usage, FGraphicsResourceComplete complete, void* context);

    /** Delete an exclusively owned vertex buffer after all its captured users have finished. */
    GraphicsServiceResult EnqueueDeleteVertexBuffer(HGraphicsService service, dmGraphics::HVertexBuffer buffer, FGraphicsResourceComplete complete, void* context);

    /** Create an index buffer, copying size bytes when data is supplied. */
    GraphicsServiceResult EnqueueCreateIndexBuffer(HGraphicsService service, uint32_t size, const void* data, dmGraphics::BufferUsage usage, FGraphicsResourceComplete complete, void* context);

    /** Replace index-buffer contents, copying size bytes when data is supplied. */
    GraphicsServiceResult EnqueueUploadIndexBuffer(HGraphicsService service, dmGraphics::HIndexBuffer buffer, uint32_t size, const void* data, dmGraphics::BufferUsage usage, FGraphicsResourceComplete complete, void* context);

    /** Delete an exclusively owned index buffer after all its captured users have finished. */
    GraphicsServiceResult EnqueueDeleteIndexBuffer(HGraphicsService service, dmGraphics::HIndexBuffer buffer, FGraphicsResourceComplete complete, void* context);

    /** Create a render target. Attachment data pointers must be null; parameters are copied. */
    GraphicsServiceResult EnqueueCreateRenderTarget(HGraphicsService service, uint32_t buffer_flags, const dmGraphics::RenderTargetCreationParams& params, FGraphicsResourceComplete complete, void* context);

    /** Resize an exclusively owned target after all its captured users have finished. */
    GraphicsServiceResult EnqueueResizeRenderTarget(HGraphicsService service, dmGraphics::HRenderTarget target, uint32_t width, uint32_t height, FGraphicsResourceComplete complete, void* context);

    /** Query the dimensions of a target attachment; returns copied width and height. */
    GraphicsServiceResult EnqueueGetRenderTargetSize(HGraphicsService service, dmGraphics::HRenderTarget target, dmGraphics::BufferType buffer_type, FGraphicsResourceComplete complete, void* context);

    /** Delete an exclusively owned target after all its captured users have finished. */
    GraphicsServiceResult EnqueueDeleteRenderTarget(HGraphicsService service, dmGraphics::HRenderTarget target, FGraphicsResourceComplete complete, void* context);
}

#endif
