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

#include "render_graphics_resource.h"

namespace dmRender
{
    enum ResourceOperation
    {
        RESOURCE_CREATE_TEXTURE,
        RESOURCE_UPLOAD_TEXTURE,
        RESOURCE_GET_TEXTURE_INFO,
        RESOURCE_DELETE_TEXTURE,
        RESOURCE_CREATE_VERTEX_BUFFER,
        RESOURCE_UPLOAD_VERTEX_BUFFER,
        RESOURCE_DELETE_VERTEX_BUFFER,
        RESOURCE_CREATE_INDEX_BUFFER,
        RESOURCE_UPLOAD_INDEX_BUFFER,
        RESOURCE_DELETE_INDEX_BUFFER,
        RESOURCE_CREATE_RENDER_TARGET,
        RESOURCE_RESIZE_RENDER_TARGET,
        RESOURCE_GET_RENDER_TARGET_SIZE,
        RESOURCE_DELETE_RENDER_TARGET,
    };

    // Callback/context are opaque completion-only values: execution never follows them.
    // Result storage travels with the request until game-owner delivery.
    struct ResourceTask
    {
        GraphicsResourceResponse  m_Response;
        uint64_t                  m_Handle;
        FGraphicsResourceComplete m_Complete;
        void*                     m_Context;
        ResourceOperation         m_Operation;
    };

    struct TextureTask
    {
        dmGraphics::TextureParams         m_Params;
        ResourceTask                      m_Task;
        dmGraphics::TextureCreationParams m_CreationParams;
    };

    struct BufferTask
    {
        ResourceTask            m_Task;
        uint32_t                m_Size;
        dmGraphics::BufferUsage m_Usage;
        uint8_t                 m_HasData : 1;
    };

    struct RenderTargetTask
    {
        dmGraphics::RenderTargetCreationParams m_Params;
        ResourceTask                           m_Task;
        uint32_t                               m_BufferFlags;
        uint32_t                               m_Width;
        uint32_t                               m_Height;
        dmGraphics::BufferType                 m_BufferType;
    };

    static ResourceTask MakeResourceTask(ResourceOperation operation, uint64_t handle, FGraphicsResourceComplete complete, void* context)
    {
        ResourceTask task = {};
        task.m_Complete = complete;
        task.m_Context = context;
        task.m_Handle = handle;
        task.m_Operation = operation;
        return task;
    }

    static void CompleteResourceTask(const ResourceTask& task, int32_t result)
    {
        task.m_Complete(task.m_Context, (GraphicsResourceResult)result, task.m_Response);
    }

    static void CompleteTexture(void*, int32_t result, const void* payload, uint32_t)
    {
        CompleteResourceTask(((const TextureTask*)payload)->m_Task, result);
    }

    static void CompleteBuffer(void*, int32_t result, const void* payload, uint32_t)
    {
        CompleteResourceTask(((const BufferTask*)payload)->m_Task, result);
    }

    static void CompleteRenderTarget(void*, int32_t result, const void* payload, uint32_t)
    {
        CompleteResourceTask(((const RenderTargetTask*)payload)->m_Task, result);
    }

    static bool IsTexture2D(const dmGraphics::TextureInfo& info)
    {
        return info.m_Type == dmGraphics::TEXTURE_TYPE_2D && info.m_Depth <= 1 && info.m_PageCount <= 1;
    }

    static int32_t ExecuteTexture(dmGraphics::HContext context, void* payload, uint32_t)
    {
        TextureTask* request = (TextureTask*)payload;
        ResourceTask& task = request->m_Task;
        if (task.m_Operation == RESOURCE_CREATE_TEXTURE)
        {
            task.m_Response.m_Handle = dmGraphics::NewTexture(context, request->m_CreationParams);
            return task.m_Response.m_Handle ? GRAPHICS_RESOURCE_OK : GRAPHICS_RESOURCE_FAILED;
        }

        if (dmGraphics::GetAssetType(task.m_Handle) != dmGraphics::ASSET_TYPE_TEXTURE || !dmGraphics::IsAssetHandleValid(context, task.m_Handle))
        {
            return GRAPHICS_RESOURCE_INVALID;
        }

        switch (task.m_Operation)
        {
            case RESOURCE_UPLOAD_TEXTURE:
            {
                const dmGraphics::TextureInfo info = dmGraphics::GetTextureInfo(context, task.m_Handle);
                if (!IsTexture2D(info))
                {
                    return GRAPHICS_RESOURCE_UNSUPPORTED;
                }

                dmGraphics::TextureParams& params = request->m_Params;
                const uint32_t longest = info.m_Width > info.m_Height ? info.m_Width : info.m_Height;
                if (params.m_MipMap >= dmGraphics::GetMipmapCount(longest))
                {
                    return GRAPHICS_RESOURCE_INVALID;
                }
                const uint32_t mip_width = dmGraphics::GetMipmapSize(info.m_Width, params.m_MipMap);
                const uint32_t mip_height = dmGraphics::GetMipmapSize(info.m_Height, params.m_MipMap);
                const uint32_t width = mip_width ? mip_width : 1;
                const uint32_t height = mip_height ? mip_height : 1;
                if ((params.m_SubUpdate && (params.m_X > width || params.m_Width > width - params.m_X || params.m_Y > height || params.m_Height > height - params.m_Y)) ||
                    (!params.m_SubUpdate && params.m_MipMap != 0 && (params.m_Width != width || params.m_Height != height)))
                {
                    return GRAPHICS_RESOURCE_INVALID;
                }

                params.m_Data = request + 1;
                // SetTexture consumes or stages these bytes before returning. The async API
                // borrows them for a worker and would outlive this service payload.
                dmGraphics::SetTexture(context, task.m_Handle, params);
                return GRAPHICS_RESOURCE_OK;
            }
            case RESOURCE_GET_TEXTURE_INFO:
            {
                task.m_Response.m_TextureInfo = dmGraphics::GetTextureInfo(context, task.m_Handle);
                return GRAPHICS_RESOURCE_OK;
            }
            case RESOURCE_DELETE_TEXTURE:
            {
                dmGraphics::DeleteTexture(context, task.m_Handle);
                return GRAPHICS_RESOURCE_OK;
            }
            default:
            {
                return GRAPHICS_RESOURCE_INVALID;
            }
        }
    }

    static int32_t ExecuteBuffer(dmGraphics::HContext context, void* payload, uint32_t)
    {
        BufferTask* request = (BufferTask*)payload;
        ResourceTask& task = request->m_Task;
        const void* data = request->m_HasData ? request + 1 : 0;
        switch (task.m_Operation)
        {
            case RESOURCE_CREATE_VERTEX_BUFFER:
            {
                task.m_Response.m_Handle = dmGraphics::NewVertexBuffer(context, request->m_Size, data, request->m_Usage);
                return task.m_Response.m_Handle ? GRAPHICS_RESOURCE_OK : GRAPHICS_RESOURCE_FAILED;
            }
            case RESOURCE_UPLOAD_VERTEX_BUFFER:
            {
                dmGraphics::SetVertexBufferData((dmGraphics::HVertexBuffer)task.m_Handle, request->m_Size, data, request->m_Usage);
                return GRAPHICS_RESOURCE_OK;
            }
            case RESOURCE_DELETE_VERTEX_BUFFER:
            {
                dmGraphics::DeleteVertexBuffer((dmGraphics::HVertexBuffer)task.m_Handle);
                return GRAPHICS_RESOURCE_OK;
            }
            case RESOURCE_CREATE_INDEX_BUFFER:
            {
                task.m_Response.m_Handle = dmGraphics::NewIndexBuffer(context, request->m_Size, data, request->m_Usage);
                return task.m_Response.m_Handle ? GRAPHICS_RESOURCE_OK : GRAPHICS_RESOURCE_FAILED;
            }
            case RESOURCE_UPLOAD_INDEX_BUFFER:
            {
                dmGraphics::SetIndexBufferData((dmGraphics::HIndexBuffer)task.m_Handle, request->m_Size, data, request->m_Usage);
                return GRAPHICS_RESOURCE_OK;
            }
            case RESOURCE_DELETE_INDEX_BUFFER:
            {
                dmGraphics::DeleteIndexBuffer((dmGraphics::HIndexBuffer)task.m_Handle);
                return GRAPHICS_RESOURCE_OK;
            }
            default:
            {
                return GRAPHICS_RESOURCE_INVALID;
            }
        }
    }

    static int32_t ExecuteRenderTarget(dmGraphics::HContext context, void* payload, uint32_t)
    {
        RenderTargetTask* request = (RenderTargetTask*)payload;
        ResourceTask& task = request->m_Task;
        if (task.m_Operation == RESOURCE_CREATE_RENDER_TARGET)
        {
            task.m_Response.m_Handle = dmGraphics::NewRenderTarget(context, request->m_BufferFlags, request->m_Params);
            return task.m_Response.m_Handle ? GRAPHICS_RESOURCE_OK : GRAPHICS_RESOURCE_FAILED;
        }

        if (dmGraphics::GetAssetType(task.m_Handle) != dmGraphics::ASSET_TYPE_RENDER_TARGET || !dmGraphics::IsAssetHandleValid(context, task.m_Handle))
        {
            return GRAPHICS_RESOURCE_INVALID;
        }

        switch (task.m_Operation)
        {
            case RESOURCE_RESIZE_RENDER_TARGET:
            {
                if (dmGraphics::GetRenderTargetTextureType(context, task.m_Handle) == dmGraphics::TEXTURE_TYPE_CUBE_MAP && request->m_Width != request->m_Height)
                {
                    return GRAPHICS_RESOURCE_INVALID;
                }
                dmGraphics::SetRenderTargetSize(context, task.m_Handle, request->m_Width, request->m_Height);
                return GRAPHICS_RESOURCE_OK;
            }
            case RESOURCE_GET_RENDER_TARGET_SIZE:
            {
                dmGraphics::GetRenderTargetSize(context, task.m_Handle, request->m_BufferType, task.m_Response.m_Width, task.m_Response.m_Height);
                return GRAPHICS_RESOURCE_OK;
            }
            case RESOURCE_DELETE_RENDER_TARGET:
            {
                dmGraphics::DeleteRenderTarget(context, task.m_Handle);
                return GRAPHICS_RESOURCE_OK;
            }
            default:
            {
                return GRAPHICS_RESOURCE_INVALID;
            }
        }
    }

    static GraphicsServiceResult EnqueueResource(HGraphicsService service, FGraphicsTaskExecute execute, FGraphicsTaskComplete complete, const void* payload, uint32_t payload_size, const void* data, uint32_t data_size)
    {
        GraphicsTask task = {};
        task.m_Execute = execute;
        task.m_Complete = complete;
        task.m_Payload = payload;
        task.m_PayloadSize = payload_size;
        return EnqueueGraphicsTask(service, task, data, data_size);
    }

    GraphicsServiceResult EnqueueCreateTexture(HGraphicsService service, const dmGraphics::TextureCreationParams& params, FGraphicsResourceComplete complete, void* context)
    {
        if (!complete || !params.m_Width || !params.m_Height || params.m_Type != dmGraphics::TEXTURE_TYPE_2D || params.m_Depth > 1 || params.m_LayerCount > 1)
        {
            return GRAPHICS_SERVICE_INVALID_ARGUMENT;
        }

        TextureTask request;
        request.m_Task = MakeResourceTask(RESOURCE_CREATE_TEXTURE, 0, complete, context);
        request.m_CreationParams = params;
        return EnqueueResource(service, ExecuteTexture, CompleteTexture, &request, sizeof(request), 0, 0);
    }

    GraphicsServiceResult EnqueueUploadTexture(HGraphicsService service, dmGraphics::HTexture texture, const dmGraphics::TextureParams& params, FGraphicsResourceComplete complete, void* context)
    {
        uint32_t channels;
        switch (params.m_Format)
        {
            case dmGraphics::TEXTURE_FORMAT_LUMINANCE:
            {
                channels = 1;
                break;
            }
            case dmGraphics::TEXTURE_FORMAT_LUMINANCE_ALPHA:
            {
                channels = 2;
                break;
            }
            case dmGraphics::TEXTURE_FORMAT_RGB:
            {
                channels = 3;
                break;
            }
            case dmGraphics::TEXTURE_FORMAT_RGBA:
            {
                channels = 4;
                break;
            }
            default:
            {
                return GRAPHICS_SERVICE_INVALID_ARGUMENT;
            }
        }
        const uint64_t bytes = (uint64_t)params.m_Width * params.m_Height * channels;
        if (!complete || !texture || !params.m_Data || !params.m_Width || !params.m_Height || params.m_DataSize != bytes || params.m_Depth > 1 || params.m_LayerCount > 1 || params.m_Z || params.m_Slice)
        {
            return GRAPHICS_SERVICE_INVALID_ARGUMENT;
        }

        TextureTask request;
        request.m_Task = MakeResourceTask(RESOURCE_UPLOAD_TEXTURE, texture, complete, context);
        request.m_Params = params;
        request.m_Params.m_Data = 0;
        return EnqueueResource(service, ExecuteTexture, CompleteTexture, &request, sizeof(request), params.m_Data, params.m_DataSize);
    }

    static GraphicsServiceResult EnqueueTextureHandle(HGraphicsService service, ResourceOperation operation, dmGraphics::HTexture texture, FGraphicsResourceComplete complete, void* context)
    {
        if (!complete || !texture)
        {
            return GRAPHICS_SERVICE_INVALID_ARGUMENT;
        }
        TextureTask request;
        request.m_Task = MakeResourceTask(operation, texture, complete, context);
        return EnqueueResource(service, ExecuteTexture, CompleteTexture, &request, sizeof(request), 0, 0);
    }

    GraphicsServiceResult EnqueueGetTextureInfo(HGraphicsService service, dmGraphics::HTexture texture, FGraphicsResourceComplete complete, void* context)
    {
        return EnqueueTextureHandle(service, RESOURCE_GET_TEXTURE_INFO, texture, complete, context);
    }

    GraphicsServiceResult EnqueueDeleteTexture(HGraphicsService service, dmGraphics::HTexture texture, FGraphicsResourceComplete complete, void* context)
    {
        return EnqueueTextureHandle(service, RESOURCE_DELETE_TEXTURE, texture, complete, context);
    }

    static GraphicsServiceResult EnqueueBuffer(HGraphicsService service, ResourceOperation operation, uint64_t buffer, uint32_t size, const void* data, dmGraphics::BufferUsage usage, FGraphicsResourceComplete complete, void* context)
    {
        if (!complete || (operation != RESOURCE_CREATE_VERTEX_BUFFER && operation != RESOURCE_CREATE_INDEX_BUFFER && !buffer))
        {
            return GRAPHICS_SERVICE_INVALID_ARGUMENT;
        }
        BufferTask request = {};
        request.m_Task = MakeResourceTask(operation, buffer, complete, context);
        request.m_Size = size;
        request.m_Usage = usage;
        request.m_HasData = data != 0;
        return EnqueueResource(service, ExecuteBuffer, CompleteBuffer, &request, sizeof(request), data, data ? size : 0);
    }

    GraphicsServiceResult EnqueueCreateVertexBuffer(HGraphicsService service, uint32_t size, const void* data, dmGraphics::BufferUsage usage, FGraphicsResourceComplete complete, void* context)
    {
        return EnqueueBuffer(service, RESOURCE_CREATE_VERTEX_BUFFER, 0, size, data, usage, complete, context);
    }

    GraphicsServiceResult EnqueueUploadVertexBuffer(HGraphicsService service, dmGraphics::HVertexBuffer buffer, uint32_t size, const void* data, dmGraphics::BufferUsage usage, FGraphicsResourceComplete complete, void* context)
    {
        return EnqueueBuffer(service, RESOURCE_UPLOAD_VERTEX_BUFFER, buffer, size, data, usage, complete, context);
    }

    GraphicsServiceResult EnqueueDeleteVertexBuffer(HGraphicsService service, dmGraphics::HVertexBuffer buffer, FGraphicsResourceComplete complete, void* context)
    {
        return EnqueueBuffer(service, RESOURCE_DELETE_VERTEX_BUFFER, buffer, 0, 0, dmGraphics::BUFFER_USAGE_STATIC_DRAW, complete, context);
    }

    GraphicsServiceResult EnqueueCreateIndexBuffer(HGraphicsService service, uint32_t size, const void* data, dmGraphics::BufferUsage usage, FGraphicsResourceComplete complete, void* context)
    {
        return EnqueueBuffer(service, RESOURCE_CREATE_INDEX_BUFFER, 0, size, data, usage, complete, context);
    }

    GraphicsServiceResult EnqueueUploadIndexBuffer(HGraphicsService service, dmGraphics::HIndexBuffer buffer, uint32_t size, const void* data, dmGraphics::BufferUsage usage, FGraphicsResourceComplete complete, void* context)
    {
        return EnqueueBuffer(service, RESOURCE_UPLOAD_INDEX_BUFFER, buffer, size, data, usage, complete, context);
    }

    GraphicsServiceResult EnqueueDeleteIndexBuffer(HGraphicsService service, dmGraphics::HIndexBuffer buffer, FGraphicsResourceComplete complete, void* context)
    {
        return EnqueueBuffer(service, RESOURCE_DELETE_INDEX_BUFFER, buffer, 0, 0, dmGraphics::BUFFER_USAGE_STATIC_DRAW, complete, context);
    }

    GraphicsServiceResult EnqueueCreateRenderTarget(HGraphicsService service, uint32_t buffer_flags, const dmGraphics::RenderTargetCreationParams& params, FGraphicsResourceComplete complete, void* context)
    {
        if (!complete || params.m_DepthBufferParams.m_Data || params.m_StencilBufferParams.m_Data)
        {
            return GRAPHICS_SERVICE_INVALID_ARGUMENT;
        }
        for (uint32_t i = 0; i < dmGraphics::MAX_BUFFER_COLOR_ATTACHMENTS; ++i)
        {
            if (params.m_ColorBufferParams[i].m_Data)
            {
                return GRAPHICS_SERVICE_INVALID_ARGUMENT;
            }
        }
        RenderTargetTask request;
        request.m_Task = MakeResourceTask(RESOURCE_CREATE_RENDER_TARGET, 0, complete, context);
        request.m_Params = params;
        request.m_BufferFlags = buffer_flags;
        request.m_Width = 0;
        request.m_Height = 0;
        request.m_BufferType = dmGraphics::BUFFER_TYPE_COLOR0_BIT;
        return EnqueueResource(service, ExecuteRenderTarget, CompleteRenderTarget, &request, sizeof(request), 0, 0);
    }

    static GraphicsServiceResult EnqueueTargetHandle(HGraphicsService service, ResourceOperation operation, dmGraphics::HRenderTarget target, uint32_t width, uint32_t height, dmGraphics::BufferType buffer_type, FGraphicsResourceComplete complete, void* context)
    {
        if (!complete || !target)
        {
            return GRAPHICS_SERVICE_INVALID_ARGUMENT;
        }
        RenderTargetTask request;
        request.m_Task = MakeResourceTask(operation, target, complete, context);
        request.m_BufferFlags = 0;
        request.m_Width = width;
        request.m_Height = height;
        request.m_BufferType = buffer_type;
        return EnqueueResource(service, ExecuteRenderTarget, CompleteRenderTarget, &request, sizeof(request), 0, 0);
    }

    GraphicsServiceResult EnqueueResizeRenderTarget(HGraphicsService service, dmGraphics::HRenderTarget target, uint32_t width, uint32_t height, FGraphicsResourceComplete complete, void* context)
    {
        // Texture dimensions are uint16_t in the graphics API.
        if (!width || !height || width > UINT16_MAX || height > UINT16_MAX)
        {
            return GRAPHICS_SERVICE_INVALID_ARGUMENT;
        }
        return EnqueueTargetHandle(service, RESOURCE_RESIZE_RENDER_TARGET, target, width, height, dmGraphics::BUFFER_TYPE_COLOR0_BIT, complete, context);
    }

    GraphicsServiceResult EnqueueGetRenderTargetSize(HGraphicsService service, dmGraphics::HRenderTarget target, dmGraphics::BufferType buffer_type, FGraphicsResourceComplete complete, void* context)
    {
        const uint32_t valid_types = dmGraphics::BUFFER_TYPE_COLOR0_BIT | dmGraphics::BUFFER_TYPE_COLOR1_BIT | dmGraphics::BUFFER_TYPE_COLOR2_BIT | dmGraphics::BUFFER_TYPE_COLOR3_BIT | dmGraphics::BUFFER_TYPE_DEPTH_BIT | dmGraphics::BUFFER_TYPE_STENCIL_BIT;
        const uint32_t type = (uint32_t)buffer_type;
        if (!(type & valid_types) || (type & (type - 1)) || (type & ~valid_types))
        {
            return GRAPHICS_SERVICE_INVALID_ARGUMENT;
        }
        return EnqueueTargetHandle(service, RESOURCE_GET_RENDER_TARGET_SIZE, target, 0, 0, buffer_type, complete, context);
    }

    GraphicsServiceResult EnqueueDeleteRenderTarget(HGraphicsService service, dmGraphics::HRenderTarget target, FGraphicsResourceComplete complete, void* context)
    {
        return EnqueueTargetHandle(service, RESOURCE_DELETE_RENDER_TARGET, target, 0, 0, dmGraphics::BUFFER_TYPE_COLOR0_BIT, complete, context);
    }
}
