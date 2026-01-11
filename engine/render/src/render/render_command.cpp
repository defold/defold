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
    {
        m_Type = type;
    }

    Command::Command(CommandType type, uint64_t op0)
    {
        m_Type = type;
        m_Operands[0] = op0;
    }

    Command::Command(CommandType type, uint64_t op0, uint64_t op1)
    {
        m_Type = type;
        m_Operands[0] = op0;
        m_Operands[1] = op1;
    }

    Command::Command(CommandType type, uint64_t op0, uint64_t op1, uint64_t op2)
    {
        m_Type = type;
        m_Operands[0] = op0;
        m_Operands[1] = op1;
        m_Operands[2] = op2;
    }

    Command::Command(CommandType type, uint64_t op0, uint64_t op1, uint64_t op2, uint64_t op3)
    {
        m_Type = type;
        m_Operands[0] = op0;
        m_Operands[1] = op1;
        m_Operands[2] = op2;
        m_Operands[3] = op3;
    }

    void ParseCommands(dmRender::HRenderContext render_context, Command* commands, uint32_t command_count)
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
                    dmGraphics::SetRenderTarget(context, c->m_Operands[0], c->m_Operands[1]);
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
                    delete matrix;
                    break;
                }
                case COMMAND_TYPE_SET_PROJECTION:
                {
                    dmVMath::Matrix4* matrix = (dmVMath::Matrix4*)c->m_Operands[0];
                    dmRender::SetProjectionMatrix(render_context, *matrix);
                    delete matrix;
                    break;
                }
                case COMMAND_TYPE_SET_BLEND_FUNC:
                {
                    dmGraphics::SetBlendFunc(context, (dmGraphics::BlendFactor)c->m_Operands[0], (dmGraphics::BlendFactor)c->m_Operands[1]);
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
                    delete frustum_options;
                    break;
                }
                case COMMAND_TYPE_DRAW_DEBUG3D:
                {
                    FrustumOptions* frustum_options = (FrustumOptions*)c->m_Operands[0];
                    dmRender::DrawDebug3d(render_context, frustum_options);
                    delete frustum_options;
                    break;
                }
                case COMMAND_TYPE_DRAW_DEBUG2D:
                {
                    dmRender::DrawDebug2d(render_context); // Deprecated
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
