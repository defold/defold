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
        GRAPHICS_RESOURCE_CANCELLED = GRAPHICS_TASK_CANCELLED,
        GRAPHICS_RESOURCE_OK        = 0,
        GRAPHICS_RESOURCE_INVALID,
        GRAPHICS_RESOURCE_UNSUPPORTED,
        GRAPHICS_RESOURCE_FAILED,
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
     * Receive a resource result at the game-owner completion boundary, never inside enqueue.
     * Copy needed response values before returning. Success means the adapter call finished,
     * not that the GPU is idle; void graphics operations cannot report device failures here.
     * @param context Caller context, which must outlive the accepted request
     * @param result Resource execution result
     * @param response Borrowed result; created handles become the caller's responsibility
     */
    typedef void (*FGraphicsResourceComplete)(void* context, GraphicsResourceResult result, const GraphicsResourceResponse& response);

    /**
     * These helpers copy their inputs only after service admission. Rejected requests have
     * no callback or side effects. Existing handles must remain alive until completion;
     * mutations/deletion additionally require no outstanding captured frames using them.
     * These requests do not yet provide frame-resource retention or mutation ordering.
     * Readback, mapped buffers and shaders need separate lifetime/backend contracts and
     * are not included. In particular, browser readback must not use a blocking adapter API.
     */

    /** Create an ordinary 2D texture; the resulting handle is delivered in the response. */
    GraphicsServiceResult EnqueueCreateTexture(HGraphicsService service, const dmGraphics::TextureCreationParams& params, FGraphicsResourceComplete complete, void* context);

    /**
     * Copy and upload one 2D mip/subregion. Supports LUMINANCE, LUMINANCE_ALPHA, RGB and RGBA
     * byte data only, with DataSize exactly width * height * channels. Arrays, cube maps,
     * volumes and compressed formats are excluded until their full payload sizes are explicit.
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
