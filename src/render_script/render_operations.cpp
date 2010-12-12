#include <stdio.h>
#include <dlib/log.h>
#include "render_operations.h"


namespace dmEngine
{

    void ParseRenderCommands(dmRender::HRenderContext rendercontext, RenderCommand* rendercommands, uint32_t command_count)
    {
        dmGraphics::HContext context = dmRender::GetGraphicsContext(rendercontext);
        for (uint32_t i=0; i<command_count; i++)
        {
            RenderCommand* c = &rendercommands[i];
            switch (c->m_OpCode)
            {
                case CMD_ENABLE_STATE:
                {
                    dmGraphics::EnableState(context, (dmGraphics::RenderState)c->m_Operands[0]);
                    break;
                }
                case CMD_DISABLE_STATE:
                {
                    dmGraphics::DisableState(context, (dmGraphics::RenderState)c->m_Operands[0]);
                    break;
                }
                case CMD_ENABLE_RENDERTARGET:
                {
                    dmGraphics::EnableRenderTarget(context, (dmGraphics::HRenderTarget)c->m_Operands[0] );
                    break;
                }
                case CMD_DISABLE_RENDERTARGET:
                {
                    dmGraphics::DisableRenderTarget(context, (dmGraphics::HRenderTarget)c->m_Operands[0] );
                    break;
                }
                case CMD_CLEAR:
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
                case CMD_SETVIEWPORT:
                {
                    dmGraphics::SetViewport(context, c->m_Operands[0], c->m_Operands[1]);
                    break;
                }
                case CMD_SETVIEW:
                {
                    Vectormath::Aos::Matrix4* matrix = (Vectormath::Aos::Matrix4*)c->m_Operands[0];
                    dmRender::SetViewMatrix(rendercontext, *matrix);
                    delete matrix;
                    break;
                }
                case CMD_SETPROJECTION:
                {
                    Vectormath::Aos::Matrix4* matrix = (Vectormath::Aos::Matrix4*)c->m_Operands[0];
                    dmRender::SetProjectionMatrix(rendercontext, *matrix);
                    delete matrix;
                    break;
                }
                case CMD_SETBLENDFUNC:
                {
                    dmGraphics::SetBlendFunc(context, (dmGraphics::BlendFactor)c->m_Operands[0], (dmGraphics::BlendFactor)c->m_Operands[1]);
                    break;
                }
                case CMD_SETDEPTHMASK:
                {
                    dmGraphics::SetDepthMask(context, c->m_Operands[0]);
                    break;
                }
                case CMD_SETCULLFACE:
                {
                    dmGraphics::SetCullFace(context, (dmGraphics::FaceType)c->m_Operands[0]);
                    break;
                }
                case CMD_DRAW:
                {
                    dmRender::Draw(rendercontext, (dmRender::Predicate*)c->m_Operands[0]);
                    break;
                }
                case CMD_DRAWDEBUG3D:
                {
                    dmRender::DrawDebug3d(rendercontext);
                    break;
                }
                case CMD_DRAWDEBUG2D:
                {
                    dmRender::DrawDebug2d(rendercontext);
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
