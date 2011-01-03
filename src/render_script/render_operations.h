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
        CMD_SET_TEXTURE_UNIT        = 4,
        CMD_CLEAR                   = 5,
        CMD_SETVIEWPORT             = 6,
        CMD_SETVIEW                 = 7,
        CMD_SETPROJECTION           = 8,
        CMD_SETBLENDFUNC            = 9,
        CMD_SETCOLORMASK            = 10,
        CMD_SETDEPTHMASK            = 11,
        CMD_SETINDEXMASK            = 12,
        CMD_SETSTENCILMASK          = 13,
        CMD_SETCULLFACE             = 14,
        CMD_DRAW                    = 15,
        CMD_DRAWDEBUG3D             = 16,
        CMD_DRAWDEBUG2D             = 17,
        CMD_MAX                     = 18
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
