#include <stdio.h>
#include <dlib/log.h>
#include "render_command.h"
#include "render_private.h"

namespace dmRender
{
    Command::Command(CommandType type)
    {
        m_Type = type;
    }

    Command::Command(CommandType type, uintptr_t op0)
    {
        m_Type = type;
        m_Operands[0] = op0;
    }

    Command::Command(CommandType type, uintptr_t op0, uintptr_t op1)
    {
        m_Type = type;
        m_Operands[0] = op0;
        m_Operands[1] = op1;
    }

    Command::Command(CommandType type, uintptr_t op0, uintptr_t op1, uintptr_t op2)
    {
        m_Type = type;
        m_Operands[0] = op0;
        m_Operands[1] = op1;
        m_Operands[2] = op2;
    }

    Command::Command(CommandType type, uintptr_t op0, uintptr_t op1, uintptr_t op2, uintptr_t op3)
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
                case COMMAND_TYPE_ENABLE_RENDER_TARGET:
                {
                    dmGraphics::HRenderTarget rt = (dmGraphics::HRenderTarget)c->m_Operands[0];
                    render_context->m_CurrentRenderTarget = rt;
                    dmGraphics::EnableRenderTarget(context, rt);
                    break;
                }
                case COMMAND_TYPE_DISABLE_RENDER_TARGET:
                {
                    render_context->m_CurrentRenderTarget = 0x0;
                    dmGraphics::DisableRenderTarget(context, (dmGraphics::HRenderTarget)c->m_Operands[0] );
                    break;
                }
                case COMMAND_TYPE_ENABLE_TEXTURE:
                {
                    render_context->m_Textures[c->m_Operands[0]] = (dmGraphics::HTexture)c->m_Operands[1];
                    break;
                }
                case COMMAND_TYPE_DISABLE_TEXTURE:
                {
                    render_context->m_Textures[c->m_Operands[0]] = 0;
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
                    Vectormath::Aos::Matrix4* matrix = (Vectormath::Aos::Matrix4*)c->m_Operands[0];
                    dmRender::SetViewMatrix(render_context, *matrix);
                    delete matrix;
                    break;
                }
                case COMMAND_TYPE_SET_PROJECTION:
                {
                    Vectormath::Aos::Matrix4* matrix = (Vectormath::Aos::Matrix4*)c->m_Operands[0];
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
                    dmRender::DrawRenderList(render_context, (dmRender::Predicate*)c->m_Operands[0], (dmRender::HNamedConstantBuffer)c->m_Operands[1]);
                    break;
                }
                case COMMAND_TYPE_DRAW_DEBUG3D:
                {
                    dmRender::DrawDebug3d(render_context);
                    break;
                }
                case COMMAND_TYPE_DRAW_DEBUG2D:
                {
                    dmRender::DrawDebug2d(render_context);
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
                default:
                {
                    dmLogError("No such render command (%d).", c->m_Type);
                }
            }
        }
    }

}
