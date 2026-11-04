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

#include "render_command.h"
#include "render_private.h"

namespace dmRender
{
    static RenderFrameResult CopyOperand(HRenderFrame frame, uint64_t source, uint32_t size, uint32_t alignment, uint64_t* destination)
    {
        *destination = 0;
        // A null operand means optional data, such as a frustum, is absent and needs no copy.
        if (!source)
        {
            return RENDER_FRAME_OK;
        }

        uint32_t table_index;
        void* copy;
        RenderFrameResult result = CopyRenderFrameTable(frame, RENDER_COMMAND_PAYLOAD_TABLE_TYPE, 1, size, alignment, (const void*)source, &table_index, &copy);
        if (result == RENDER_FRAME_OK)
        {
            *destination = (uint64_t)copy;
        }
        return result;
    }

    static RenderFrameResult RecordDependency(HRenderFrame frame, RenderCommandDependencyType type, uint64_t handle)
    {
        if (!handle)
        {
            return RENDER_FRAME_OK;
        }
        RenderCommandDependency dependency;
        dependency.m_Handle = handle;
        dependency.m_Type = type;
        uint32_t table_index;
        // Record which GPU resources the command uses. These descriptors do not keep resources
        // alive or protect against handle reuse; that requires separate lifetime management.
        return CopyRenderFrameTable(frame, RENDER_COMMAND_DEPENDENCY_TABLE_TYPE, 1, sizeof(dependency), 8, &dependency, &table_index);
    }

    static RenderFrameResult CopyCameraSnapshot(HRenderContext context, HRenderFrame frame, HRenderCamera handle,
                                                bool use_frustum, uint64_t* destination)
    {
        *destination = RENDER_COMMAND_CAMERA_NONE;
        RenderCamera* camera = handle ? context->m_Recording.m_RenderCameras.Get(handle) : 0;
        if (!camera)
        {
            return RENDER_FRAME_OK;
        }
        if (camera->m_Dirty)
        {
            UpdateRenderCamera(context, handle, &camera->m_LastPosition, &camera->m_LastRotation);
        }
        RenderCommandCamera snapshot;
        snapshot.m_View = camera->m_View;
        snapshot.m_Projection = camera->m_Projection;
        snapshot.m_ViewProjection = camera->m_ViewProjection;
        snapshot.m_UseFrustum = use_frustum;
        uint32_t table_index;
        RenderFrameResult result = CopyRenderFrameTable(frame, RENDER_COMMAND_PAYLOAD_TABLE_TYPE, 1, sizeof(snapshot), 16, &snapshot, &table_index, 0);
        if (result == RENDER_FRAME_OK)
        {
            // Reserve zero for inherited selection, keeping table index zero usable.
            *destination = (uint64_t)table_index + 1;
        }
        return result;
    }

    RenderFrameResult RecordRenderCommand(HRenderContext context, HRenderFrame frame, const Command& source_command, Command* destination,
                                          HRenderCamera selected_camera, bool use_frustum, bool capture_camera)
    {
        const Command source = source_command;
        *destination = source;
        if (capture_camera && (source.m_Type == COMMAND_TYPE_DRAW || source.m_Type == COMMAND_TYPE_DRAW_DEBUG3D))
        {
            RenderFrameResult result = CopyCameraSnapshot(context, frame, selected_camera, use_frustum, &destination->m_Operands[4]);
            if (result != RENDER_FRAME_OK)
            {
                return result;
            }
        }
        switch (source.m_Type)
        {
            case COMMAND_TYPE_SET_VIEW:
            case COMMAND_TYPE_SET_PROJECTION:
            {
                return CopyOperand(frame, source.m_Operands[0], sizeof(dmVMath::Matrix4), 16, &destination->m_Operands[0]);
            }
            case COMMAND_TYPE_DRAW:
            {
                RenderFrameResult result = CopyOperand(frame, source.m_Operands[0], sizeof(Predicate), 8, &destination->m_Operands[0]);
                if (result != RENDER_FRAME_OK)
                {
                    return result;
                }
                result = CopyOperand(frame, source.m_Operands[2], sizeof(FrustumOptions), 16, &destination->m_Operands[2]);
                if (result != RENDER_FRAME_OK)
                {
                    return result;
                }
                HNamedConstantBuffer constants;
                result = CopyRenderCommandConstants(frame, (HNamedConstantBuffer)source.m_Operands[1], &constants);
                destination->m_Operands[1] = (uint64_t)constants;
                return result;
            }
            case COMMAND_TYPE_DRAW_DEBUG3D:
            {
                return CopyOperand(frame, source.m_Operands[0], sizeof(FrustumOptions), 16, &destination->m_Operands[0]);
            }
            case COMMAND_TYPE_DISPATCH_COMPUTE:
            {
                HNamedConstantBuffer constants;
                RenderFrameResult result = CopyRenderCommandConstants(frame, (HNamedConstantBuffer)source.m_Operands[3], &constants);
                destination->m_Operands[3] = (uint64_t)constants;
                return result;
            }
            case COMMAND_TYPE_SET_RENDER_CAMERA:
            {
                destination->m_Type = COMMAND_TYPE_SET_RENDER_CAMERA_SNAPSHOT;
                return CopyCameraSnapshot(context, frame, (HRenderCamera)source.m_Operands[0], source.m_Operands[1] != 0, &destination->m_Operands[0]);
            }
            case COMMAND_TYPE_ENABLE_TEXTURE:
            {
                return RecordDependency(frame, RENDER_COMMAND_DEPENDENCY_TEXTURE, source.m_Operands[2]);
            }
            case COMMAND_TYPE_SET_RENDER_TARGET:
            {
                return RecordDependency(frame, RENDER_COMMAND_DEPENDENCY_RENDER_TARGET, source.m_Operands[0]);
            }
            case COMMAND_TYPE_ENABLE_MATERIAL:
            {
                return RecordDependency(frame, RENDER_COMMAND_DEPENDENCY_MATERIAL, source.m_Operands[0]);
            }
            case COMMAND_TYPE_SET_COMPUTE:
            {
                return RecordDependency(frame, RENDER_COMMAND_DEPENDENCY_COMPUTE, source.m_Operands[0]);
            }
            default:
            {
                return RENDER_FRAME_OK;
            }
        }
    }

    RenderFrameResult AppendRenderCommands(HRenderFrame frame, const Command* commands, uint32_t count)
    {
        if (!count)
        {
            return RENDER_FRAME_OK;
        }
        uint32_t table_index;
        return CopyRenderFrameTable(frame, RENDER_COMMAND_TABLE_TYPE, count, sizeof(Command), 8, commands, &table_index);
    }
}
