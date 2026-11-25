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
    // Resource kind and action are separate so new kinds can reuse the same operations.
    enum ResourceType
    {
        RESOURCE_TYPE_TEXTURE       = 0,
        RESOURCE_TYPE_VERTEX_BUFFER = 1,
        RESOURCE_TYPE_INDEX_BUFFER  = 2,
        RESOURCE_TYPE_RENDER_TARGET = 3,
    };

    enum ResourceOperation
    {
        RESOURCE_OPERATION_CREATE   = 0,
        RESOURCE_OPERATION_UPLOAD   = 1,
        RESOURCE_OPERATION_DELETE   = 2,
        RESOURCE_OPERATION_RESIZE   = 3,
        RESOURCE_OPERATION_GET_INFO = 4,
        RESOURCE_OPERATION_GET_SIZE = 5,
    };

    // Keep this first in every resource request so they share one completion callback.
    // Callback/context are opaque completion-only values: execution never follows them.
    // Result storage travels with the request until game-owner delivery.
    struct ResourceTask
    {
        GraphicsResourceResponse  m_Response;
        uint64_t                  m_Handle;
        FGraphicsResourceComplete m_Complete;
        void*                     m_Context;
        ResourceOperation         m_Operation;
        ResourceType              m_ResourceType;
    };

    // Texture parameters and result, followed by copied pixel bytes for uploads.
    struct TextureTask
    {
        ResourceTask                      m_Task;
        dmGraphics::TextureParams         m_Params;
        dmGraphics::TextureCreationParams m_CreationParams;
    };

    // Buffer operation and result, followed by copied vertex/index bytes when supplied.
    struct BufferTask
    {
        ResourceTask            m_Task;
        uint32_t                m_Size;
        dmGraphics::BufferUsage m_Usage;
        uint8_t                 m_HasData : 1;
    };

    // Attachment descriptions or size/query parameters for a render target operation.
    struct RenderTargetTask
    {
        ResourceTask                           m_Task;
        dmGraphics::RenderTargetCreationParams m_Params;
        uint32_t                               m_BufferFlags;
        uint32_t                               m_Width;
        uint32_t                               m_Height;
        dmGraphics::BufferType                 m_BufferType;
    };

    static ResourceTask MakeResourceTask(ResourceType type, ResourceOperation operation, uint64_t handle, FGraphicsResourceComplete complete, void* context)
    {
        ResourceTask task = {};
        task.m_Complete = complete;
        task.m_Context = context;
        task.m_Handle = handle;
        task.m_Operation = operation;
        task.m_ResourceType = type;
        return task;
    }

    static void CompleteResourceTask(void*, int32_t result, const void* payload, uint32_t)
    {
        const ResourceTask* task = (const ResourceTask*)payload;
        task->m_Complete(task->m_Context, (GraphicsResourceResult)result, task->m_Response);
    }

    static bool IsTexture2D(const dmGraphics::TextureInfo& info)
    {
        return info.m_Type == dmGraphics::TEXTURE_TYPE_2D && info.m_Depth <= 1 && info.m_PageCount <= 1;
    }

    static int32_t ExecuteTexture(dmGraphics::HContext context, void* payload, uint32_t)
    {
        TextureTask* request = (TextureTask*)payload;
        ResourceTask& task = request->m_Task;
        if (task.m_Operation == RESOURCE_OPERATION_CREATE)
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
            case RESOURCE_OPERATION_UPLOAD:
            {
                const dmGraphics::TextureInfo info = dmGraphics::GetTextureInfo(context, task.m_Handle);
                if (!IsTexture2D(info))
                {
                    return GRAPHICS_RESOURCE_UNSUPPORTED;
                }

                dmGraphics::TextureParams& params = request->m_Params;
                // Earlier requests or direct graphics calls may have allocated or resized
                // the texture. Validate its storage now, not when the request was queued.
                if (!dmGraphics::IsTextureUploadValid(context, task.m_Handle, params))
                {
                    return GRAPHICS_RESOURCE_INVALID;
                }

                params.m_Data = request + 1;
                // SetTexture consumes or stages these bytes before returning. The async API
                // borrows them for a worker and would outlive this service payload.
                dmGraphics::SetTexture(context, task.m_Handle, params);
                return GRAPHICS_RESOURCE_OK;
            }
            case RESOURCE_OPERATION_GET_INFO:
            {
                task.m_Response.m_TextureInfo = dmGraphics::GetTextureInfo(context, task.m_Handle);
                return GRAPHICS_RESOURCE_OK;
            }
            case RESOURCE_OPERATION_DELETE:
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
        bool vertex = task.m_ResourceType == RESOURCE_TYPE_VERTEX_BUFFER;
        switch (task.m_Operation)
        {
            case RESOURCE_OPERATION_CREATE:
            {
                task.m_Response.m_Handle = vertex
                    ? dmGraphics::NewVertexBuffer(context, request->m_Size, data, request->m_Usage)
                    : dmGraphics::NewIndexBuffer(context, request->m_Size, data, request->m_Usage);
                return task.m_Response.m_Handle ? GRAPHICS_RESOURCE_OK : GRAPHICS_RESOURCE_FAILED;
            }
            case RESOURCE_OPERATION_UPLOAD:
            {
                if (vertex)
                {
                    dmGraphics::SetVertexBufferData((dmGraphics::HVertexBuffer)task.m_Handle, request->m_Size, data, request->m_Usage);
                }
                else
                {
                    dmGraphics::SetIndexBufferData((dmGraphics::HIndexBuffer)task.m_Handle, request->m_Size, data, request->m_Usage);
                }

                return GRAPHICS_RESOURCE_OK;
            }
            case RESOURCE_OPERATION_DELETE:
            {
                if (vertex)
                {
                    dmGraphics::DeleteVertexBuffer((dmGraphics::HVertexBuffer)task.m_Handle);
                }
                else
                {
                    dmGraphics::DeleteIndexBuffer((dmGraphics::HIndexBuffer)task.m_Handle);
                }

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
        if (task.m_Operation == RESOURCE_OPERATION_CREATE)
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
            case RESOURCE_OPERATION_RESIZE:
            {
                if (dmGraphics::GetRenderTargetTextureType(context, task.m_Handle) == dmGraphics::TEXTURE_TYPE_CUBE_MAP && request->m_Width != request->m_Height)
                {
                    return GRAPHICS_RESOURCE_INVALID;
                }

                dmGraphics::SetRenderTargetSize(context, task.m_Handle, request->m_Width, request->m_Height);
                return GRAPHICS_RESOURCE_OK;
            }
            case RESOURCE_OPERATION_GET_SIZE:
            {
                dmGraphics::GetRenderTargetSize(context, task.m_Handle, request->m_BufferType, task.m_Response.m_Width, task.m_Response.m_Height);
                return GRAPHICS_RESOURCE_OK;
            }
            case RESOURCE_OPERATION_DELETE:
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

    static GraphicsServiceResult EnqueueResource(HGraphicsService service, FGraphicsTaskExecute execute, const void* payload, uint32_t payload_size, const void* data, uint32_t data_size)
    {
        const ResourceTask* resource = (const ResourceTask*)payload;
        if (!resource->m_Complete)
        {
            return GRAPHICS_SERVICE_INVALID_ARGUMENT;
        }

        GraphicsTask task = {};
        task.m_Execute = execute;
        task.m_Complete = CompleteResourceTask;
        task.m_Payload = payload;
        task.m_PayloadSize = payload_size;
        return EnqueueGraphicsTask(service, task, data, data_size);
    }

    GraphicsServiceResult EnqueueCreateTexture(HGraphicsService service, const dmGraphics::TextureCreationParams& params, FGraphicsResourceComplete complete, void* context)
    {
        if (!params.m_Width || !params.m_Height || params.m_Type != dmGraphics::TEXTURE_TYPE_2D || params.m_Depth > 1 || params.m_LayerCount > 1)
        {
            return GRAPHICS_SERVICE_INVALID_ARGUMENT;
        }

        TextureTask request;
        request.m_Task = MakeResourceTask(RESOURCE_TYPE_TEXTURE, RESOURCE_OPERATION_CREATE, 0, complete, context);
        request.m_CreationParams = params;
        return EnqueueResource(service, ExecuteTexture, &request, sizeof(request), 0, 0);
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
        if (!texture || !params.m_Data || !params.m_Width || !params.m_Height || params.m_DataSize != bytes || params.m_Depth > 1 || params.m_LayerCount > 1 || params.m_Z || params.m_Slice)
        {
            return GRAPHICS_SERVICE_INVALID_ARGUMENT;
        }

        TextureTask request;
        request.m_Task = MakeResourceTask(RESOURCE_TYPE_TEXTURE, RESOURCE_OPERATION_UPLOAD, texture, complete, context);
        request.m_Params = params;
        request.m_Params.m_Data = 0;
        return EnqueueResource(service, ExecuteTexture, &request, sizeof(request), params.m_Data, params.m_DataSize);
    }

    static GraphicsServiceResult EnqueueTextureHandle(HGraphicsService service, ResourceOperation operation, dmGraphics::HTexture texture, FGraphicsResourceComplete complete, void* context)
    {
        if (!texture)
        {
            return GRAPHICS_SERVICE_INVALID_ARGUMENT;
        }

        TextureTask request;
        request.m_Task = MakeResourceTask(RESOURCE_TYPE_TEXTURE, operation, texture, complete, context);
        return EnqueueResource(service, ExecuteTexture, &request, sizeof(request), 0, 0);
    }

    GraphicsServiceResult EnqueueGetTextureInfo(HGraphicsService service, dmGraphics::HTexture texture, FGraphicsResourceComplete complete, void* context)
    {
        return EnqueueTextureHandle(service, RESOURCE_OPERATION_GET_INFO, texture, complete, context);
    }

    GraphicsServiceResult EnqueueDeleteTexture(HGraphicsService service, dmGraphics::HTexture texture, FGraphicsResourceComplete complete, void* context)
    {
        return EnqueueTextureHandle(service, RESOURCE_OPERATION_DELETE, texture, complete, context);
    }

    static GraphicsServiceResult EnqueueBuffer(HGraphicsService service, ResourceType type, ResourceOperation operation, uint64_t buffer, uint32_t size, const void* data, dmGraphics::BufferUsage usage, FGraphicsResourceComplete complete, void* context)
    {
        if (operation != RESOURCE_OPERATION_CREATE && !buffer)
        {
            return GRAPHICS_SERVICE_INVALID_ARGUMENT;
        }

        BufferTask request;
        request.m_Task = MakeResourceTask(type, operation, buffer, complete, context);
        request.m_Size = size;
        request.m_Usage = usage;
        request.m_HasData = data != 0;
        return EnqueueResource(service, ExecuteBuffer, &request, sizeof(request), data, data ? size : 0);
    }

    GraphicsServiceResult EnqueueCreateVertexBuffer(HGraphicsService service, uint32_t size, const void* data, dmGraphics::BufferUsage usage, FGraphicsResourceComplete complete, void* context)
    {
        return EnqueueBuffer(service, RESOURCE_TYPE_VERTEX_BUFFER, RESOURCE_OPERATION_CREATE, 0, size, data, usage, complete, context);
    }

    GraphicsServiceResult EnqueueUploadVertexBuffer(HGraphicsService service, dmGraphics::HVertexBuffer buffer, uint32_t size, const void* data, dmGraphics::BufferUsage usage, FGraphicsResourceComplete complete, void* context)
    {
        return EnqueueBuffer(service, RESOURCE_TYPE_VERTEX_BUFFER, RESOURCE_OPERATION_UPLOAD, buffer, size, data, usage, complete, context);
    }

    GraphicsServiceResult EnqueueDeleteVertexBuffer(HGraphicsService service, dmGraphics::HVertexBuffer buffer, FGraphicsResourceComplete complete, void* context)
    {
        return EnqueueBuffer(service, RESOURCE_TYPE_VERTEX_BUFFER, RESOURCE_OPERATION_DELETE, buffer, 0, 0, dmGraphics::BUFFER_USAGE_STATIC_DRAW, complete, context);
    }

    GraphicsServiceResult EnqueueCreateIndexBuffer(HGraphicsService service, uint32_t size, const void* data, dmGraphics::BufferUsage usage, FGraphicsResourceComplete complete, void* context)
    {
        return EnqueueBuffer(service, RESOURCE_TYPE_INDEX_BUFFER, RESOURCE_OPERATION_CREATE, 0, size, data, usage, complete, context);
    }

    GraphicsServiceResult EnqueueUploadIndexBuffer(HGraphicsService service, dmGraphics::HIndexBuffer buffer, uint32_t size, const void* data, dmGraphics::BufferUsage usage, FGraphicsResourceComplete complete, void* context)
    {
        return EnqueueBuffer(service, RESOURCE_TYPE_INDEX_BUFFER, RESOURCE_OPERATION_UPLOAD, buffer, size, data, usage, complete, context);
    }

    GraphicsServiceResult EnqueueDeleteIndexBuffer(HGraphicsService service, dmGraphics::HIndexBuffer buffer, FGraphicsResourceComplete complete, void* context)
    {
        return EnqueueBuffer(service, RESOURCE_TYPE_INDEX_BUFFER, RESOURCE_OPERATION_DELETE, buffer, 0, 0, dmGraphics::BUFFER_USAGE_STATIC_DRAW, complete, context);
    }

    GraphicsServiceResult EnqueueCreateRenderTarget(HGraphicsService service, uint32_t buffer_flags, const dmGraphics::RenderTargetCreationParams& params, FGraphicsResourceComplete complete, void* context)
    {
        if (params.m_DepthBufferParams.m_Data || params.m_StencilBufferParams.m_Data)
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
        request.m_Task = MakeResourceTask(RESOURCE_TYPE_RENDER_TARGET, RESOURCE_OPERATION_CREATE, 0, complete, context);
        request.m_Params = params;
        request.m_BufferFlags = buffer_flags;
        request.m_Width = 0;
        request.m_Height = 0;
        request.m_BufferType = dmGraphics::BUFFER_TYPE_COLOR0_BIT;
        return EnqueueResource(service, ExecuteRenderTarget, &request, sizeof(request), 0, 0);
    }

    static GraphicsServiceResult EnqueueTargetHandle(HGraphicsService service, ResourceOperation operation, dmGraphics::HRenderTarget target, uint32_t width, uint32_t height, dmGraphics::BufferType buffer_type, FGraphicsResourceComplete complete, void* context)
    {
        if (!target)
        {
            return GRAPHICS_SERVICE_INVALID_ARGUMENT;
        }

        RenderTargetTask request;
        request.m_Task = MakeResourceTask(RESOURCE_TYPE_RENDER_TARGET, operation, target, complete, context);
        request.m_BufferFlags = 0;
        request.m_Width = width;
        request.m_Height = height;
        request.m_BufferType = buffer_type;
        return EnqueueResource(service, ExecuteRenderTarget, &request, sizeof(request), 0, 0);
    }

    GraphicsServiceResult EnqueueResizeRenderTarget(HGraphicsService service, dmGraphics::HRenderTarget target, uint32_t width, uint32_t height, FGraphicsResourceComplete complete, void* context)
    {
        // Texture dimensions are uint16_t in the graphics API.
        if (!width || !height || width > UINT16_MAX || height > UINT16_MAX)
        {
            return GRAPHICS_SERVICE_INVALID_ARGUMENT;
        }

        return EnqueueTargetHandle(service, RESOURCE_OPERATION_RESIZE, target, width, height, dmGraphics::BUFFER_TYPE_COLOR0_BIT, complete, context);
    }

    GraphicsServiceResult EnqueueGetRenderTargetSize(HGraphicsService service, dmGraphics::HRenderTarget target, dmGraphics::BufferType buffer_type, FGraphicsResourceComplete complete, void* context)
    {
        if (!dmGraphics::IsColorBufferType(buffer_type) && buffer_type != dmGraphics::BUFFER_TYPE_DEPTH_BIT && buffer_type != dmGraphics::BUFFER_TYPE_STENCIL_BIT)
        {
            return GRAPHICS_SERVICE_INVALID_ARGUMENT;
        }

        return EnqueueTargetHandle(service, RESOURCE_OPERATION_GET_SIZE, target, 0, 0, buffer_type, complete, context);
    }

    GraphicsServiceResult EnqueueDeleteRenderTarget(HGraphicsService service, dmGraphics::HRenderTarget target, FGraphicsResourceComplete complete, void* context)
    {
        return EnqueueTargetHandle(service, RESOURCE_OPERATION_DELETE, target, 0, 0, dmGraphics::BUFFER_TYPE_COLOR0_BIT, complete, context);
    }

}
