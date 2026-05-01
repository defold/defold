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

#include "render_private.h"

namespace dmRender
{
    static HRenderBuffer NewRenderBuffer(dmGraphics::HContext graphics_context, RenderBufferType type)
    {
        switch(type)
        {
            case RENDER_BUFFER_TYPE_VERTEX_BUFFER: return (HRenderBuffer) dmGraphics::NewVertexBuffer(graphics_context, 0, 0x0, dmGraphics::BUFFER_USAGE_DYNAMIC_DRAW);
            case RENDER_BUFFER_TYPE_INDEX_BUFFER:  return (HRenderBuffer) dmGraphics::NewIndexBuffer(graphics_context, 0, 0x0, dmGraphics::BUFFER_USAGE_DYNAMIC_DRAW);
            default:break;
        }
        return (HRenderBuffer) 0;
    }

    static void DeleteRenderBuffer(HRenderBuffer buffer_to_remove, RenderBufferType type)
    {
        switch(type)
        {
            case RENDER_BUFFER_TYPE_VERTEX_BUFFER:
                dmGraphics::DeleteVertexBuffer((dmGraphics::HVertexBuffer) buffer_to_remove);
                break;
            case RENDER_BUFFER_TYPE_INDEX_BUFFER:
                dmGraphics::DeleteIndexBuffer((dmGraphics::HIndexBuffer) buffer_to_remove);
                break;
            default:break;
        }
    }

    static inline void CreateAndPush(HRenderContext render_context, HBufferedRenderBuffer buffer)
    {
        buffer->m_Buffers.Push(NewRenderBuffer(render_context->m_GraphicsContext, buffer->m_Type));
    }

    HBufferedRenderBuffer NewBufferedRenderBuffer(HRenderContext render_context, RenderBufferType type)
    {
        BufferedRenderBuffer* buffer = new BufferedRenderBuffer();
        memset(buffer, 0, sizeof(BufferedRenderBuffer));

        buffer->m_Type = type;
        buffer->m_Buffers.SetCapacity(1);

        RewindBuffer(render_context, buffer);
        CreateAndPush(render_context, buffer);

        return buffer;
    }

    void DeleteBufferedRenderBuffer(HRenderContext render_context, HBufferedRenderBuffer buffer)
    {
        if (!buffer)
            return;

        for (int i = 0; i < buffer->m_Buffers.Size(); ++i)
        {
            DeleteRenderBuffer(buffer->m_Buffers[i], buffer->m_Type);
        }
        delete buffer;
    }

    HRenderBuffer AddRenderBuffer(HRenderContext render_context, HBufferedRenderBuffer buffer)
    {
        if (!buffer)
            return 0;

        if (render_context->m_MultiBufferingRequired)
        {
            buffer->m_BufferIndex++;

            if (buffer->m_BufferIndex > 0)
            {
                if (buffer->m_Buffers.Full())
                {
                    buffer->m_Buffers.OffsetCapacity(4);
                }

                CreateAndPush(render_context, buffer);
            }
        }

        return buffer->m_Buffers[buffer->m_BufferIndex];
    }

    void SetBufferData(HRenderContext render_context, HBufferedRenderBuffer buffer, uint32_t size, void* data, dmGraphics::BufferUsage buffer_usage)
    {
        switch(buffer->m_Type)
        {
            case RENDER_BUFFER_TYPE_VERTEX_BUFFER:
            {
                dmGraphics::HVertexBuffer vbuf = (dmGraphics::HVertexBuffer) buffer->m_Buffers[buffer->m_BufferIndex];
                dmGraphics::SetVertexBufferData(vbuf, size, data, buffer_usage);
            } break;
            case RENDER_BUFFER_TYPE_INDEX_BUFFER:
            {
                dmGraphics::HIndexBuffer ibuf = (dmGraphics::HIndexBuffer) buffer->m_Buffers[buffer->m_BufferIndex];
                dmGraphics::SetIndexBufferData(ibuf, size, data, buffer_usage);
            } break;
            default:break;
        }
    }

    void SetBufferSubData(HRenderContext render_context, HBufferedRenderBuffer buffer, uint32_t offset, uint32_t size, void* data)
    {
        switch(buffer->m_Type)
        {
        case RENDER_BUFFER_TYPE_VERTEX_BUFFER:
        {
            dmGraphics::HVertexBuffer vbuf = (dmGraphics::HVertexBuffer) buffer->m_Buffers[buffer->m_BufferIndex];
            dmGraphics::SetVertexBufferSubData(vbuf, offset, size, data);
        } break;
        case RENDER_BUFFER_TYPE_INDEX_BUFFER:
        {
            dmGraphics::HIndexBuffer ibuf = (dmGraphics::HIndexBuffer) buffer->m_Buffers[buffer->m_BufferIndex];
            dmGraphics::SetIndexBufferSubData(ibuf, offset, size, data);
        } break;
        default:break;
        }
    }

    void TrimBuffer(HRenderContext render_context, HBufferedRenderBuffer buffer)
    {
        if (!buffer)
            return;

        uint32_t n = buffer->m_Buffers.Size();
        uint32_t new_buffer_count = dmMath::Max(1, buffer->m_BufferIndex+1);

        for (int i = new_buffer_count; i < n; ++i)
        {
            DeleteRenderBuffer(buffer->m_Buffers[i], buffer->m_Type);
        }
        buffer->m_Buffers.SetSize(new_buffer_count);
    }

    HRenderBuffer GetBuffer(HRenderContext render_context, HBufferedRenderBuffer buffer)
    {
        return buffer->m_Buffers[buffer->m_BufferIndex];
    }

    int32_t GetBufferIndex(HRenderContext render_context, HBufferedRenderBuffer buffer)
    {
        return buffer->m_BufferIndex;
    }

    void RewindBuffer(HRenderContext render_context, HBufferedRenderBuffer buffer)
    {
        if (!buffer)
            return;
        buffer->m_BufferIndex = 0;
    }
}
