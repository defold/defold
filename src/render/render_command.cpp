#include <stdio.h>
#include <dlib/log.h>
#include "render_command.h"

namespace dmRender
{
    Command::Command(CommandType type)
    {
        m_Type = type;
    }

    Command::Command(CommandType type, uint32_t op0)
    {
        m_Type = type;
        m_Operands[0] = op0;
    }

    Command::Command(CommandType type, uint32_t op0, uint32_t op1)
    {
        m_Type = type;
        m_Operands[0] = op0;
        m_Operands[1] = op1;
    }

    Command::Command(CommandType type, uint32_t op0, uint32_t op1, uint32_t op2)
    {
        m_Type = type;
        m_Operands[0] = op0;
        m_Operands[1] = op1;
        m_Operands[2] = op2;
    }

    Command::Command(CommandType type, uint32_t op0, uint32_t op1, uint32_t op2, uint32_t op3)
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
                case COMMAND_TYPE_ENABLE_RENDERTARGET:
                {
                    dmGraphics::EnableRenderTarget(context, (dmGraphics::HRenderTarget)c->m_Operands[0] );
                    break;
                }
                case COMMAND_TYPE_DISABLE_RENDERTARGET:
                {
                    dmGraphics::DisableRenderTarget(context, (dmGraphics::HRenderTarget)c->m_Operands[0] );
                    break;
                }
                case COMMAND_TYPE_SET_TEXTURE_UNIT:
                {
                    dmGraphics::SetTextureUnit(context, c->m_Operands[0], (dmGraphics::HTexture)c->m_Operands[1]);
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
                    break;
                }
                case COMMAND_TYPE_SETVIEWPORT:
                {
                    dmGraphics::SetViewport(context, c->m_Operands[0], c->m_Operands[1]);
                    break;
                }
                case COMMAND_TYPE_SETVIEW:
                {
                    Vectormath::Aos::Matrix4* matrix = (Vectormath::Aos::Matrix4*)c->m_Operands[0];
                    dmRender::SetViewMatrix(render_context, *matrix);
                    delete matrix;
                    break;
                }
                case COMMAND_TYPE_SETPROJECTION:
                {
                    Vectormath::Aos::Matrix4* matrix = (Vectormath::Aos::Matrix4*)c->m_Operands[0];
                    dmRender::SetProjectionMatrix(render_context, *matrix);
                    delete matrix;
                    break;
                }
                case COMMAND_TYPE_SETBLENDFUNC:
                {
                    dmGraphics::SetBlendFunc(context, (dmGraphics::BlendFactor)c->m_Operands[0], (dmGraphics::BlendFactor)c->m_Operands[1]);
                    break;
                }
                case COMMAND_TYPE_SETCOLORMASK:
                {
                    dmGraphics::SetColorMask(context, c->m_Operands[0] != 0, c->m_Operands[1] != 0, c->m_Operands[2] != 0, c->m_Operands[3] != 0);
                    break;
                }
                case COMMAND_TYPE_SETDEPTHMASK:
                {
                    dmGraphics::SetDepthMask(context, c->m_Operands[0]);
                    break;
                }
                case COMMAND_TYPE_SETINDEXMASK:
                {
                    dmGraphics::SetIndexMask(context, c->m_Operands[0]);
                    break;
                }
                case COMMAND_TYPE_SETSTENCILMASK:
                {
                    dmGraphics::SetStencilMask(context, c->m_Operands[0]);
                    break;
                }
                case COMMAND_TYPE_SETCULLFACE:
                {
                    dmGraphics::SetCullFace(context, (dmGraphics::FaceType)c->m_Operands[0]);
                    break;
                }
                case COMMAND_TYPE_SETPOLYGONOFFSET:
                {
                    dmGraphics::SetPolygonOffset(context, (float)c->m_Operands[0], (float)c->m_Operands[1]);
                    break;
                }
                case COMMAND_TYPE_DRAW:
                {
                    dmRender::Draw(render_context, (dmRender::Predicate*)c->m_Operands[0]);
                    break;
                }
                case COMMAND_TYPE_DRAWDEBUG3D:
                {
                    dmRender::DrawDebug3d(render_context);
                    break;
                }
                case COMMAND_TYPE_DRAWDEBUG2D:
                {
                    dmRender::DrawDebug2d(render_context);
                    break;
                }
                default:
                {
                    dmLogError("No such render command (%d)");
                }
            }
        }
    }

}
