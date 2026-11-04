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

#include <string.h>

#include <dlib/log.h>

#include "render_command.h"
#include "render_pass.h"
#include "render_private.h"

namespace dmRender
{
    RenderFrameResult CalculateRenderFrameLimits(HRenderContext context, const RenderFrameLimits& component_limits, RenderFrameLimits* limits)
    {
        // The default clear/draw path emits four commands, even when a caller configures
        // a smaller script buffer. Size for whichever path needs more storage.
        uint64_t command_count = context->m_Recording.m_RenderScriptContext.m_CommandBufferSize;
        if (command_count < 4)
        {
            command_count = 4;
        }

        // A draw is the largest fixed operand set. Allow each allocation's worst-case
        // alignment padding, including when component inputs end at an unaligned offset.
        // Constant snapshots use separate growable storage, including their headers.
        const uint64_t operand_bytes = sizeof(RenderCommandCamera) + 15
                                       + sizeof(Predicate) + 7
                                       + sizeof(FrustumOptions) + 15;
        const uint64_t fixed_bytes = sizeof(RenderFrameInfo) + 7
                                     + command_count * sizeof(Command) + 7
                                     + command_count * operand_bytes;
        const uint64_t payload_bytes = component_limits.m_PayloadBytes + fixed_bytes;
        // Camera, predicate, frustum and constants per draw, plus metadata/command array.
        const uint64_t table_count = component_limits.m_MaxTables + 4 * command_count + 2;
        if (payload_bytes > UINT32_MAX || table_count * sizeof(RenderFrameTable) > UINT32_MAX)
        {
            dmLogError("Render frame storage exceeds the supported capacity. Reduce command capacity or component storage.");
            return RENDER_FRAME_CAPACITY_EXCEEDED;
        }

        *limits = component_limits;
        limits->m_PayloadBytes = (uint32_t)payload_bytes;
        limits->m_MaxTables = (uint32_t)table_count;
        return RENDER_FRAME_OK;
    }

    void BeginRenderFrameRecording(HRenderContext context, float time, float dt)
    {
        context->m_Recording.m_Time = time;
        context->m_Recording.m_Dt = dt;
    }

    RenderFrameInfo GetRenderFrameInfo(HRenderContext context)
    {
        RenderFrameInfo info;
        info.m_SurfaceId = 0;
        info.m_Width = dmGraphics::GetWindowWidth(context->m_GraphicsContext);
        info.m_Height = dmGraphics::GetWindowHeight(context->m_GraphicsContext);
        info.m_Time = context->m_Recording.m_Time;
        info.m_Dt = context->m_Recording.m_Dt;
        return info;
    }

    RenderFrameResult RecordRenderFrameInfo(HRenderFrame frame, const RenderFrameInfo& info)
    {
        uint32_t table;
        return CopyRenderFrameTable(frame, RENDER_FRAME_INFO_TABLE_TYPE, 1, sizeof(info), 8, &info, &table);
    }

    void BeginRenderFrameConsumption(HRenderContext context, const RenderFrameView& frame)
    {
        for (uint32_t i = 0; i < frame.m_Statistics.m_TableCount; ++i)
        {
            if (frame.m_Tables[i].m_Type == RENDER_FRAME_INFO_TABLE_TYPE)
            {
                const RenderFrameInfo* info = (const RenderFrameInfo*)GetRenderFrameTableData(frame, i);
                // Apply only immutable timing here. BeginFrame's legacy preparation
                // still precedes producer component traversal: resetting submissions
                // here would erase lights already submitted by inline components.
                // D2.2 moves that preparation together with consumer-owned lists.
                context->m_Time = info->m_Time;
                context->m_Dt = info->m_Dt;
                dmGraphics::BeginFrame(context->m_GraphicsContext);
                return;
            }
        }
    }

    RenderFrameResult RecordDefaultRenderCommands(HRenderContext context, HRenderFrame frame, const RenderFrameInfo& info, uint32_t clear_color)
    {
        float depth = 1.0f;
        uint32_t depth_bits;
        memcpy(&depth_bits, &depth, sizeof(depth_bits));
        Command commands[] = {
            Command(COMMAND_TYPE_SET_RENDER_CAMERA, 0, 0),
            Command(COMMAND_TYPE_SET_VIEWPORT, 0, 0, info.m_Width, info.m_Height),
            Command(COMMAND_TYPE_CLEAR, dmGraphics::BUFFER_TYPE_COLOR0_BIT | dmGraphics::BUFFER_TYPE_DEPTH_BIT | dmGraphics::BUFFER_TYPE_STENCIL_BIT,
                    clear_color, depth_bits, 0),
            Command(COMMAND_TYPE_DRAW, 0, 0, 0, SORT_BACK_TO_FRONT)
        };
        for (uint32_t i = 0; i < sizeof(commands) / sizeof(commands[0]); ++i)
        {
            Command source = commands[i];
            RenderFrameResult result = RecordRenderCommand(context, frame, source, &commands[i], 0, false, false);
            if (result != RENDER_FRAME_OK)
            {
                return result;
            }
        }
        return AppendRenderCommands(frame, commands, sizeof(commands) / sizeof(commands[0]));
    }
}
