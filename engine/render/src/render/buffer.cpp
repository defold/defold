// Copyright 2020-2023 The Defold Foundation
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
            default:assert(0);
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
            default:assert(0);
        }
    }

    static inline void CreateAndPush(HRenderContext render_context, HBufferedRenderBuffer buffer)
    {
        buffer->m_Buffers.Push(NewRenderBuffer(render_context->m_GraphicsContext, buffer->m_Type));
    }

    static inline void PopAndDelete(HRenderContext render_context, HBufferedRenderBuffer buffer)
    {
        DeleteRenderBuffer(buffer->m_Buffers.Back(), buffer->m_Type);
        buffer->m_Buffers.Pop();
    }

    HBufferedRenderBuffer NewBufferedRenderBuffer(HRenderContext render_context, RenderBufferType type)
    {
        BufferedRenderBuffer* buffer = new BufferedRenderBuffer();
        memset(buffer, 0, sizeof(BufferedRenderBuffer));
        RewindBuffer(render_context, buffer);
        buffer->m_Type = type;
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

    HRenderBuffer AdvanceRenderBuffer(HRenderContext render_context, HBufferedRenderBuffer buffer)
    {
        if (!buffer)
            return 0;

        if (buffer->m_Buffers.Empty())
        {
            buffer->m_Buffers.OffsetCapacity(1);
            CreateAndPush(render_context, buffer);
        }

        if (!render_context->m_MultiBufferingRequired)
        {
            buffer->m_BufferIndex = 0;
            assert(buffer->m_Buffers.Size() == 1);
            return buffer->m_Buffers.Front();
        }

        buffer->m_BufferIndex++;
        if (buffer->m_BufferIndex >= buffer->m_Buffers.Capacity())
        {
            buffer->m_Buffers.OffsetCapacity(1);
            CreateAndPush(render_context, buffer);
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
            default:assert(0);
        }
    }

    void TrimBuffer(HRenderContext render_context, HBufferedRenderBuffer buffer)
    {
        if (!buffer)
            return;

        uint32_t n = buffer->m_Buffers.Size();
        uint32_t new_buffer_count = buffer->m_BufferIndex+1;

        if (new_buffer_count == n)
            return;

        for (int i = 0; i < (n - new_buffer_count); ++i)
        {
            PopAndDelete(render_context, buffer);
        }

        buffer->m_Buffers.SetCapacity(new_buffer_count);
    }

    void RewindBuffer(HRenderContext render_context, HBufferedRenderBuffer buffer)
    {
        if (!buffer)
            return;
        buffer->m_BufferIndex = -1;
    }
}
