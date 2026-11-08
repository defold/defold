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
#include <dlib/log.h>
#include <dmsdk/dlib/intersection.h>
#include "render_command.h"
#include "render_private.h"

namespace dmRender
{
    Command::Command(CommandType type)
    : m_Operands()
    {
        m_Type = type;
    }

    Command::Command(CommandType type, uint64_t op0)
    : m_Operands()
    {
        m_Type = type;
        m_Operands[0] = op0;
    }

    Command::Command(CommandType type, uint64_t op0, uint64_t op1)
    : m_Operands()
    {
        m_Type = type;
        m_Operands[0] = op0;
        m_Operands[1] = op1;
    }

    Command::Command(CommandType type, uint64_t op0, uint64_t op1, uint64_t op2)
    : m_Operands()
    {
        m_Type = type;
        m_Operands[0] = op0;
        m_Operands[1] = op1;
        m_Operands[2] = op2;
    }

    Command::Command(CommandType type, uint64_t op0, uint64_t op1, uint64_t op2, uint64_t op3)
    : m_Operands()
    {
        m_Type = type;
        m_Operands[0] = op0;
        m_Operands[1] = op1;
        m_Operands[2] = op2;
        m_Operands[3] = op3;
    }

    Command::Command(CommandType type, uint64_t op0, uint64_t op1, uint64_t op2, uint64_t op3, uint64_t op4)
    {
        m_Type = type;
        m_Operands[0] = op0;
        m_Operands[1] = op1;
        m_Operands[2] = op2;
        m_Operands[3] = op3;
        m_Operands[4] = op4;
    }

    static RenderFrameResult CopyOperand(HRenderFrameStorage frame, uint64_t source, uint32_t size, uint32_t alignment, uint64_t* destination)
    {
        *destination = 0;
        // A null operand means optional data, such as a frustum, is absent and needs no copy.
        if (!source)
        {
            return RENDER_FRAME_OK;
        }

        // TODO: Allocate operands as raw frame payloads and reserve typed tables for
        // dispatch ranges. Migrate camera table indices/inheritance at the same time
        // and measure metadata/scanning costs before changing that contract.
        uint32_t table_index;
        void* copy;
        RenderFrameResult result = CopyRenderFrameTable(frame, RENDER_COMMAND_PAYLOAD_TABLE_TYPE, 1, size, alignment, (const void*)source, &table_index, &copy);
        if (result == RENDER_FRAME_OK)
        {
            *destination = (uint64_t)copy;
        }
        return result;
    }

    static RenderFrameResult RecordDependency(HRenderFrameStorage frame, RenderCommandDependencyType type, uint64_t handle)
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
        return CopyRenderFrameTable(frame, RENDER_COMMAND_DEPENDENCY_TABLE_TYPE, 1, sizeof(dependency), alignof(RenderCommandDependency), &dependency, &table_index);
    }

    static RenderFrameResult CopyCameraSnapshot(HRenderContext context, HRenderFrameStorage frame, HRenderCamera handle,
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
        // TODO: Reuse identical snapshots within a frame using semantic values or a
        // reliable version, preserving camera edits between draws and handle reuse.
        RenderCommandCamera snapshot;
        snapshot.m_View = camera->m_View;
        snapshot.m_Projection = camera->m_Projection;
        snapshot.m_ViewProjection = camera->m_ViewProjection;
        snapshot.m_UseFrustum = use_frustum;
        uint32_t table_index;
        RenderFrameResult result = CopyRenderFrameTable(frame, RENDER_COMMAND_PAYLOAD_TABLE_TYPE, 1, sizeof(snapshot), alignof(RenderCommandCamera), &snapshot, &table_index, 0);
        if (result == RENDER_FRAME_OK)
        {
            // Reserve zero for inherited selection, keeping table index zero usable.
            *destination = (uint64_t)table_index + 1;
        }
        return result;
    }

    RenderFrameResult RecordRenderCommand(HRenderContext context, HRenderFrameStorage frame, const Command& source_command, Command* destination,
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
                return CopyOperand(frame, source.m_Operands[0], sizeof(dmVMath::Matrix4), alignof(dmVMath::Matrix4), &destination->m_Operands[0]);
            }
            case COMMAND_TYPE_DRAW:
            {
                // TODO: Deduplicate predicate values per frame; pointer-only caching is
                // unsafe when Lua GC frees a predicate and its address is reused.
                RenderFrameResult result = CopyOperand(frame, source.m_Operands[0], sizeof(Predicate), alignof(Predicate), &destination->m_Operands[0]);
                if (result != RENDER_FRAME_OK)
                {
                    return result;
                }
                result = CopyOperand(frame, source.m_Operands[2], sizeof(FrustumOptions), alignof(FrustumOptions), &destination->m_Operands[2]);
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
                return CopyOperand(frame, source.m_Operands[0], sizeof(FrustumOptions), alignof(FrustumOptions), &destination->m_Operands[0]);
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

    RenderFrameResult AppendRenderCommands(HRenderFrameStorage frame, const Command* commands, uint32_t count)
    {
        if (!count)
        {
            return RENDER_FRAME_OK;
        }
        uint32_t table_index;
        return CopyRenderFrameTable(frame, RENDER_COMMAND_TABLE_TYPE, count, sizeof(Command), alignof(Command), commands, &table_index);
    }

    static const RenderCommandCamera* ResolveCameraSnapshot(const RenderFrameView* frame, uint64_t operand)
    {
        if (operand == RENDER_COMMAND_CAMERA_NONE)
        {
            return 0;
        }
        return (const RenderCommandCamera*)GetRenderFrameTableData(*frame, (uint32_t)(operand - 1));
    }

    static const FrustumOptions* ApplyCameraSnapshot(HRenderContext context, const RenderCommandCamera* camera,
                                                     const FrustumOptions* frustum, FrustumOptions* camera_frustum)
    {
        if (!camera)
        {
            return frustum;
        }
        SetViewMatrix(context, camera->m_View);
        SetProjectionMatrix(context, camera->m_Projection);
        if (camera->m_UseFrustum)
        {
            camera_frustum->m_Matrix = camera->m_ViewProjection;
            camera_frustum->m_NumPlanes = frustum ? frustum->m_NumPlanes : FRUSTUM_PLANES_SIDES;
            return camera_frustum;
        }
        return frustum;
    }

    static void ExecuteCommands(HRenderContext render_context, const Command* commands, uint32_t command_count,
                                const RenderFrameView* frame, const RenderCommandCamera** camera_snapshot)
    {
        dmGraphics::HContext context = dmRender::GetGraphicsContext(render_context);

        for (uint32_t i = 0; i < command_count; i++)
        {
            const Command* c = &commands[i];
            switch (c->m_Type)
            {
                case COMMAND_TYPE_ENABLE_STATE:
                {
                    dmGraphics::EnableState(context, (dmGraphics::State)c->m_Operands[0]);
                    break;
                }
                case COMMAND_TYPE_DISABLE_STATE:
                {
                    dmGraphics::DisableState(context, (dmGraphics::State)c->m_Operands[0]);
                    break;
                }
                case COMMAND_TYPE_SET_RENDER_TARGET:
                {
                    dmGraphics::RenderTargetBindingParams params = {};
                    params.m_TransientBufferTypes = c->m_Operands[1];
                    params.m_CubeMapFace = (dmGraphics::CubeMapFace) c->m_Operands[2];
                    dmGraphics::SetRenderTarget(context, c->m_Operands[0], params);
                    break;
                }
                case COMMAND_TYPE_ENABLE_TEXTURE:
                {
                    // operand order: Hash, Unit, Texture
                    if (c->m_Operands[0])
                        dmRender::SetTextureBindingByHash(render_context, c->m_Operands[0], c->m_Operands[2]);
                    else
                        dmRender::SetTextureBindingByUnit(render_context, c->m_Operands[1], c->m_Operands[2]);
                    break;
                }
                case COMMAND_TYPE_DISABLE_TEXTURE:
                {
                    // operand order: Hash, Unit, Texture
                    if (c->m_Operands[0])
                        dmRender::SetTextureBindingByHash(render_context, c->m_Operands[0], 0);
                    else
                        dmRender::SetTextureBindingByUnit(render_context, c->m_Operands[1], 0);
                    break;
                }
                case COMMAND_TYPE_CLEAR:
                {
                    uint8_t r = (c->m_Operands[1] >> 0) & 0xff;
                    uint8_t g = (c->m_Operands[1] >> 8) & 0xff;
                    uint8_t b = (c->m_Operands[1] >> 16) & 0xff;
                    uint8_t a = (c->m_Operands[1] >> 24) & 0xff;
                    union float_to_uint32_t {float f; uint32_t i;};
                    float_to_uint32_t ftoi;
                    ftoi.i = c->m_Operands[2];
                    dmGraphics::Clear(context, c->m_Operands[0], r, g, b, a, ftoi.f, c->m_Operands[3]);
                    render_context->m_StencilBufferCleared = (c->m_Operands[0] & dmGraphics::BUFFER_TYPE_STENCIL_BIT) != 0;
                    break;
                }
                case COMMAND_TYPE_SET_VIEWPORT:
                {
                    dmGraphics::SetViewport(context, c->m_Operands[0], c->m_Operands[1], c->m_Operands[2], c->m_Operands[3]);
                    break;
                }
                case COMMAND_TYPE_SET_VIEW:
                {
                    dmVMath::Matrix4* matrix = (dmVMath::Matrix4*)c->m_Operands[0];
                    dmRender::SetViewMatrix(render_context, *matrix);
                    break;
                }
                case COMMAND_TYPE_SET_PROJECTION:
                {
                    dmVMath::Matrix4* matrix = (dmVMath::Matrix4*)c->m_Operands[0];
                    dmRender::SetProjectionMatrix(render_context, *matrix);
                    break;
                }
                case COMMAND_TYPE_SET_BLEND_FUNC:
                {
                    dmGraphics::SetBlendFunc(context, (dmGraphics::BlendFactor)c->m_Operands[0], (dmGraphics::BlendFactor)c->m_Operands[1]);
                    break;
                }
                case COMMAND_TYPE_SET_BLEND_FUNC_SEPARATE:
                {
                    dmGraphics::SetBlendFuncSeparate(context,
                        (dmGraphics::BlendFactor) c->m_Operands[0],
                        (dmGraphics::BlendFactor) c->m_Operands[1],
                        (dmGraphics::BlendFactor) c->m_Operands[2],
                        (dmGraphics::BlendFactor) c->m_Operands[3]);
                    break;
                }
                case COMMAND_TYPE_SET_BLEND_EQUATION_SEPARATE:
                {
                    dmGraphics::SetBlendEquationSeparate(context,
                        (dmGraphics::BlendEquation) c->m_Operands[0],
                        (dmGraphics::BlendEquation) c->m_Operands[1]);
                    break;
                }
                case COMMAND_TYPE_SET_COLOR_MASK:
                {
                    dmGraphics::SetColorMask(context, c->m_Operands[0] != 0, c->m_Operands[1] != 0, c->m_Operands[2] != 0, c->m_Operands[3] != 0);
                    break;
                }
                case COMMAND_TYPE_SET_DEPTH_MASK:
                {
                    dmGraphics::SetDepthMask(context, (bool) c->m_Operands[0]);
                    break;
                }
                case COMMAND_TYPE_SET_DEPTH_FUNC:
                {
                    dmGraphics::SetDepthFunc(context, (dmGraphics::CompareFunc)c->m_Operands[0]);
                    break;
                }
                case COMMAND_TYPE_SET_STENCIL_MASK:
                {
                    dmGraphics::SetStencilMask(context, c->m_Operands[0]);
                    break;
                }
                case COMMAND_TYPE_SET_STENCIL_FUNC:
                {
                    dmGraphics::SetStencilFunc(context, (dmGraphics::CompareFunc)c->m_Operands[0], c->m_Operands[1], c->m_Operands[2]);
                    break;
                }
                case COMMAND_TYPE_SET_STENCIL_OP:
                {
                    dmGraphics::SetStencilOp(context, (dmGraphics::StencilOp)c->m_Operands[0], (dmGraphics::StencilOp)c->m_Operands[1], (dmGraphics::StencilOp)c->m_Operands[2]);
                    break;
                }
                case COMMAND_TYPE_SET_CULL_FACE:
                {
                    dmGraphics::SetCullFace(context, (dmGraphics::FaceType)c->m_Operands[0]);
                    break;
                }
                case COMMAND_TYPE_SET_POLYGON_OFFSET:
                {
                    dmGraphics::SetPolygonOffset(context, (float)c->m_Operands[0], (float)c->m_Operands[1]);
                    break;
                }
                case COMMAND_TYPE_DRAW:
                {
                    const FrustumOptions* frustum_options = (const FrustumOptions*)c->m_Operands[2];
                    FrustumOptions camera_frustum;
                    const RenderCommandCamera* camera = c->m_Operands[4] == RENDER_COMMAND_CAMERA_INHERIT ? *camera_snapshot : ResolveCameraSnapshot(frame, c->m_Operands[4]);
                    frustum_options = ApplyCameraSnapshot(render_context, camera, frustum_options, &camera_frustum);
                    dmRender::SortOrder sort_order = (dmRender::SortOrder)c->m_Operands[3];
                    dmRender::DrawRenderList(render_context, (dmRender::Predicate*)c->m_Operands[0],
                                             (dmRender::HNamedConstantBuffer)c->m_Operands[1],
                                             frustum_options,
                                             sort_order);
                    break;
                }
                case COMMAND_TYPE_DRAW_DEBUG3D:
                {
                    const FrustumOptions* frustum_options = (const FrustumOptions*)c->m_Operands[0];
                    FrustumOptions camera_frustum;
                    const RenderCommandCamera* camera = c->m_Operands[4] == RENDER_COMMAND_CAMERA_INHERIT ? *camera_snapshot : ResolveCameraSnapshot(frame, c->m_Operands[4]);
                    frustum_options = ApplyCameraSnapshot(render_context, camera, frustum_options, &camera_frustum);
                    dmRender::DrawDebug3d(render_context, frustum_options);
                    break;
                }
                case COMMAND_TYPE_ENABLE_MATERIAL:
                {
                    render_context->m_Material = (HMaterial)c->m_Operands[0];
                    break;
                }
                case COMMAND_TYPE_DISABLE_MATERIAL:
                {
                    render_context->m_Material = 0;
                    break;
                }
                case COMMAND_TYPE_SET_COMPUTE:
                {
                    render_context->m_ComputeProgram = (HComputeProgram) c->m_Operands[0];
                    break;
                }
                case COMMAND_TYPE_DISPATCH_COMPUTE:
                {
                    dmRender::DispatchCompute(render_context,
                        c->m_Operands[0], c->m_Operands[1], c->m_Operands[2], // group x,y,z
                        (dmRender::HNamedConstantBuffer) c->m_Operands[3]);
                    break;
                }
                case COMMAND_TYPE_SET_RENDER_CAMERA:
                {
                    *camera_snapshot = 0;
                    render_context->m_CurrentRenderCamera           = (HRenderCamera) c->m_Operands[0];
                    render_context->m_CurrentRenderCameraUseFrustum = c->m_Operands[1];
                    break;
                }
                case COMMAND_TYPE_SET_RENDER_CAMERA_SNAPSHOT:
                {
                    // No camera handle reaches DrawRenderList on the owned-frame path.
                    render_context->m_CurrentRenderCamera = 0;
                    *camera_snapshot = ResolveCameraSnapshot(frame, c->m_Operands[0]);
                    break;
                }
                default:
                {
                    dmLogError("No such render command (%d).", c->m_Type);
                }
            }
        }
    }

    void ReleaseRenderCommandOperands(Command* commands, uint32_t command_count)
    {
        for (uint32_t i = 0; i < command_count; ++i)
        {
            Command& command = commands[i];
            switch (command.m_Type)
            {
                case COMMAND_TYPE_SET_VIEW:
                case COMMAND_TYPE_SET_PROJECTION:
                {
                    delete (dmVMath::Matrix4*)command.m_Operands[0];
                    command.m_Operands[0] = 0;
                    break;
                }
                case COMMAND_TYPE_DRAW:
                {
                    delete (FrustumOptions*)command.m_Operands[2];
                    command.m_Operands[2] = 0;
                    break;
                }
                case COMMAND_TYPE_DRAW_DEBUG3D:
                {
                    delete (FrustumOptions*)command.m_Operands[0];
                    command.m_Operands[0] = 0;
                    break;
                }
                default:
                {
                    break;
                }
            }
        }
    }

    void ParseCommands(HRenderContext render_context, Command* commands, uint32_t command_count)
    {
        const RenderCommandCamera* camera = 0;
        ExecuteCommands(render_context, commands, command_count, 0, &camera);
        ReleaseRenderCommandOperands(commands, command_count);
    }

    void ConsumeRenderCommands(HRenderContext context, const RenderFrameView& frame)
    {
        const RenderCommandCamera* camera = 0;
        context->m_CurrentRenderCamera = 0;
        for (uint32_t i = 0; i < frame.m_Statistics.m_TableCount; ++i)
        {
            const RenderFrameTable& table = frame.m_Tables[i];
            if (table.m_Type == RENDER_COMMAND_TABLE_TYPE)
            {
                const Command* commands = (const Command*)GetRenderFrameTableData(frame, i);
                ExecuteCommands(context, commands, table.m_Count, &frame, &camera);
            }
        }
    }

}
