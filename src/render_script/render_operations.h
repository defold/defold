#ifndef RENDER_OPERATIONS_H_
#define RENDER_OPERATIONS_H_

#include <stdint.h>
#include <render/render.h>

namespace dmEngine
{

    enum OpCode
    {
        CMD_ENABLE_STATE            = 0,
        CMD_DISABLE_STATE           = 1,
        CMD_ENABLE_RENDERTARGET     = 2,
        CMD_DISABLE_RENDERTARGET    = 3,
        CMD_CLEAR                   = 4,
        CMD_SETVIEWPORT             = 5,
        CMD_SETVIEW                 = 6,
        CMD_SETPROJECTION           = 7,
        CMD_SETBLENDFUNC            = 8,
        CMD_SETDEPTHMASK            = 9,
        CMD_SETCULLFACE             = 10,
        CMD_DRAW                    = 11,
        CMD_DRAWDEBUG3D             = 12,
        CMD_DRAWDEBUG2D             = 13,
        CMD_MAX                     = 14
    };

    struct RenderCommand
    {
        RenderCommand(OpCode opcode)
        {
            m_OpCode = opcode;
        }
        RenderCommand(OpCode opcode, uint32_t op0)
        {
            m_OpCode = opcode;
            m_Operands[0] = op0;
        }
        RenderCommand(OpCode opcode, uint32_t op0, uint32_t op1)
        {
            m_OpCode = opcode;
            m_Operands[0] = op0;
            m_Operands[1] = op1;
        }
        RenderCommand(OpCode opcode, uint32_t op0, uint32_t op1, uint32_t op2)
        {
            m_OpCode = opcode;
            m_Operands[0] = op0;
            m_Operands[1] = op1;
            m_Operands[2] = op2;
        }
        RenderCommand(OpCode opcode, uint32_t op0, uint32_t op1, uint32_t op2, uint32_t op3)
        {
            m_OpCode = opcode;
            m_Operands[0] = op0;
            m_Operands[1] = op1;
            m_Operands[2] = op2;
            m_Operands[3] = op3;
        }

        OpCode      m_OpCode;
        uint32_t    m_Operands[4];
    };

    void ParseRenderCommands(dmRender::HRenderContext rendercontext, RenderCommand* rendercommands, uint32_t command_count);

}


#endif /* RENDER_OPERATIONS_H_ */
