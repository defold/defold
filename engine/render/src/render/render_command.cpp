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
#include <string.h>
#include <dlib/log.h>
#include <dmsdk/dlib/intersection.h>
#include "render_command.h"
#include "render_private.h"

namespace dmRender
{
    Command::Command(CommandType type)
    {
        memset(m_Operands, 0, sizeof(m_Operands));
        m_Type = type;
    }

    Command::Command(CommandType type, uint64_t op0)
    {
        memset(m_Operands, 0, sizeof(m_Operands));
        m_Type = type;
        m_Operands[0] = op0;
    }

    Command::Command(CommandType type, uint64_t op0, uint64_t op1)
    {
        memset(m_Operands, 0, sizeof(m_Operands));
        m_Type = type;
        m_Operands[0] = op0;
        m_Operands[1] = op1;
    }

    Command::Command(CommandType type, uint64_t op0, uint64_t op1, uint64_t op2)
    {
        memset(m_Operands, 0, sizeof(m_Operands));
        m_Type = type;
        m_Operands[0] = op0;
        m_Operands[1] = op1;
        m_Operands[2] = op2;
    }

    Command::Command(CommandType type, uint64_t op0, uint64_t op1, uint64_t op2, uint64_t op3)
    {
        memset(m_Operands, 0, sizeof(m_Operands));
        m_Type = type;
        m_Operands[0] = op0;
        m_Operands[1] = op1;
        m_Operands[2] = op2;
        m_Operands[3] = op3;
    }

    void ReleaseCommandOperands(Command* commands, uint32_t count)
    {
        for (uint32_t i = 0; i < count; ++i)
        {
            Command& c = commands[i];
            if (c.m_Type == COMMAND_TYPE_SET_VIEW || c.m_Type == COMMAND_TYPE_SET_PROJECTION)
                delete (dmVMath::Matrix4*)c.m_Operands[0];
            else if (c.m_Type == COMMAND_TYPE_DRAW)
                delete (FrustumOptions*)c.m_Operands[2];
            else if (c.m_Type == COMMAND_TYPE_DRAW_DEBUG3D)
                delete (FrustumOptions*)c.m_Operands[0];
        }
    }

    CapturedCommands::CapturedCommands() : m_Count(0)
    {
        memset(m_Constants, 0, sizeof(m_Constants));
    }

    CapturedCommands::~CapturedCommands()
    {
        for (uint32_t i = 0; i < MAX_COMMANDS; ++i)
            if (m_Constants[i]) DeleteNamedConstantBuffer(m_Constants[i]);
    }

    uint64_t GetCapturedCommandsCapacity(const CapturedCommands& commands)
    {
        uint64_t bytes = sizeof(commands);
        for (uint32_t i = 0; i < CapturedCommands::MAX_COMMANDS; ++i)
            if (commands.m_Constants[i]) bytes += GetNamedConstantBufferCapacity(commands.m_Constants[i]);
        return bytes;
    }

    bool CaptureCommands(Command* commands, uint32_t count, CapturedCommands* output, bool allow_constants, uint64_t limit)
    {
        output->m_Count = 0;
        if (count > CapturedCommands::MAX_COMMANDS)
        {
            dmLogError("Sprite thread command capacity exceeded (%u > %u)", count, CapturedCommands::MAX_COMMANDS);
            return false;
        }
        for (uint32_t i = 0; i < count; ++i)
        {
            Command& c = output->m_Commands[i];
            c = commands[i];
            switch (c.m_Type)
            {
                case COMMAND_TYPE_SET_VIEW:
                case COMMAND_TYPE_SET_PROJECTION:
                    output->m_Matrices[i] = *(dmVMath::Matrix4*)c.m_Operands[0];
                    c.m_Operands[0] = (uint64_t)&output->m_Matrices[i];
                    break;
                case COMMAND_TYPE_DRAW:
                    if (!c.m_Operands[0] || (c.m_Operands[1] && !allow_constants))
                        return false;
                    if (c.m_Operands[1])
                    {
                        HNamedConstantBuffer source = (HNamedConstantBuffer)c.m_Operands[1];
                        // Conservatively admit a complete replacement while old storage
                        // still exists; CopyNamedConstantBuffer only grows to source size.
                        uint64_t replacement = GetNamedConstantBufferCapacity(source) + GetNamedConstantBufferCapacity(0);
                        if (GetCapturedCommandsCapacity(*output) + replacement > limit)
                            return false;
                        if (!output->m_Constants[i]) output->m_Constants[i] = NewNamedConstantBuffer();
                        CopyNamedConstantBuffer(output->m_Constants[i], source);
                        c.m_Operands[1] = (uint64_t)output->m_Constants[i];
                    }
                    output->m_Predicates[i] = *(Predicate*)c.m_Operands[0];
                    c.m_Operands[0] = (uint64_t)&output->m_Predicates[i];
                    if (c.m_Operands[2])
                    {
                        output->m_Frustums[i] = *(FrustumOptions*)c.m_Operands[2];
                        c.m_Operands[2] = (uint64_t)&output->m_Frustums[i];
                    }
                    break;
                case COMMAND_TYPE_SET_RENDER_TARGET:
                    if (c.m_Operands[0])
                        return false;
                    break;
                case COMMAND_TYPE_ENABLE_STATE:
                case COMMAND_TYPE_DISABLE_STATE:
                case COMMAND_TYPE_CLEAR:
                case COMMAND_TYPE_SET_VIEWPORT:
                case COMMAND_TYPE_SET_BLEND_FUNC:
                case COMMAND_TYPE_SET_BLEND_FUNC_SEPARATE:
                case COMMAND_TYPE_SET_BLEND_EQUATION_SEPARATE:
                case COMMAND_TYPE_SET_COLOR_MASK:
                case COMMAND_TYPE_SET_DEPTH_MASK:
                case COMMAND_TYPE_SET_DEPTH_FUNC:
                case COMMAND_TYPE_SET_STENCIL_MASK:
                case COMMAND_TYPE_SET_STENCIL_FUNC:
                case COMMAND_TYPE_SET_STENCIL_OP:
                case COMMAND_TYPE_SET_CULL_FACE:
                case COMMAND_TYPE_SET_POLYGON_OFFSET:
                    break;
                default:
                    dmLogError("Unsupported sprite thread render command: %d", c.m_Type);
                    return false;
            }
        }
        output->m_Count = count;
        return true;
    }

    void ParseCommands(dmRender::HRenderContext render_context, Command* commands, uint32_t command_count, bool release_operands)
    {
        dmGraphics::HContext context = dmRender::GetGraphicsContext(render_context);

        for (uint32_t i=0; i<command_count; i++)
        {
            Command* c = &commands[i];
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
                    if (release_operands) delete matrix;
                    break;
                }
                case COMMAND_TYPE_SET_PROJECTION:
                {
                    dmVMath::Matrix4* matrix = (dmVMath::Matrix4*)c->m_Operands[0];
                    dmRender::SetProjectionMatrix(render_context, *matrix);
                    if (release_operands) delete matrix;
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
                    FrustumOptions* frustum_options = (FrustumOptions*)c->m_Operands[2];
                    dmRender::SortOrder sort_order = (dmRender::SortOrder)c->m_Operands[3];
                    dmRender::DrawRenderList(render_context, (dmRender::Predicate*)c->m_Operands[0],
                                                             (dmRender::HNamedConstantBuffer)c->m_Operands[1],
                                                             frustum_options,
                                                             sort_order);
                    if (release_operands) delete frustum_options;
                    break;
                }
                case COMMAND_TYPE_DRAW_DEBUG3D:
                {
                    FrustumOptions* frustum_options = (FrustumOptions*)c->m_Operands[0];
                    dmRender::DrawDebug3d(render_context, frustum_options);
                    if (release_operands) delete frustum_options;
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
                    render_context->m_CurrentRenderCamera           = (HRenderCamera) c->m_Operands[0];
                    render_context->m_CurrentRenderCameraUseFrustum = c->m_Operands[1];
                } break;
                default:
                {
                    dmLogError("No such render command (%d).", c->m_Type);
                }
            }
        }
    }

}
