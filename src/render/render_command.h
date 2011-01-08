#ifndef RENDER_COMMANDS_H_
#define RENDER_COMMANDS_H_

#include <stdint.h>

#include <render/render.h>

namespace dmRender
{
    enum CommandType
    {
        COMMAND_TYPE_ENABLE_STATE           = 0,
        COMMAND_TYPE_DISABLE_STATE          = 1,
        COMMAND_TYPE_ENABLE_RENDERTARGET    = 2,
        COMMAND_TYPE_DISABLE_RENDERTARGET   = 3,
        COMMAND_TYPE_SET_TEXTURE_UNIT       = 4,
        COMMAND_TYPE_CLEAR                  = 5,
        COMMAND_TYPE_SETVIEWPORT            = 6,
        COMMAND_TYPE_SETVIEW                = 7,
        COMMAND_TYPE_SETPROJECTION          = 8,
        COMMAND_TYPE_SETBLENDFUNC           = 9,
        COMMAND_TYPE_SETCOLORMASK           = 10,
        COMMAND_TYPE_SETDEPTHMASK           = 11,
        COMMAND_TYPE_SETINDEXMASK           = 12,
        COMMAND_TYPE_SETSTENCILMASK         = 13,
        COMMAND_TYPE_SETCULLFACE            = 14,
        COMMAND_TYPE_SETPOLYGONOFFSET       = 15,
        COMMAND_TYPE_DRAW                   = 16,
        COMMAND_TYPE_DRAWDEBUG3D            = 17,
        COMMAND_TYPE_DRAWDEBUG2D            = 18,
        COMMAND_TYPE_SETMATERIAL            = 19,
        COMMAND_TYPE_MAX
    };

    struct Command
    {
        Command(CommandType type);
        Command(CommandType type, uint32_t op0);
        Command(CommandType type, uint32_t op0, uint32_t op1);
        Command(CommandType type, uint32_t op0, uint32_t op1, uint32_t op2);
        Command(CommandType type, uint32_t op0, uint32_t op1, uint32_t op2, uint32_t op3);

        CommandType m_Type;
        uint32_t    m_Operands[4];
    };

    void ParseCommands(dmRender::HRenderContext render_context, Command* commands, uint32_t command_count);
}

#endif /* RENDER_COMMANDS_H_ */
