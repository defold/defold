#ifndef RENDER_COMMANDS_H_
#define RENDER_COMMANDS_H_

#include <stdint.h>

#include <render/render.h>

namespace dmRender
{
    enum CommandType
    {
        COMMAND_TYPE_ENABLE_STATE,
        COMMAND_TYPE_DISABLE_STATE,
        COMMAND_TYPE_ENABLE_RENDERTARGET,
        COMMAND_TYPE_DISABLE_RENDERTARGET,
        COMMAND_TYPE_SET_TEXTURE_UNIT,
        COMMAND_TYPE_CLEAR,
        COMMAND_TYPE_SETVIEWPORT,
        COMMAND_TYPE_SETVIEW,
        COMMAND_TYPE_SETPROJECTION,
        COMMAND_TYPE_SETBLENDFUNC,
        COMMAND_TYPE_SETCOLORMASK,
        COMMAND_TYPE_SETDEPTHMASK,
        COMMAND_TYPE_SETINDEXMASK,
        COMMAND_TYPE_SETSTENCILMASK,
        COMMAND_TYPE_SETCULLFACE,
        COMMAND_TYPE_SETPOLYGONOFFSET,
        COMMAND_TYPE_DRAW,
        COMMAND_TYPE_DRAWDEBUG3D,
        COMMAND_TYPE_DRAWDEBUG2D,
        COMMAND_TYPE_SETMATERIAL,
        COMMAND_TYPE_SETVERTEXCONSTANT,
        COMMAND_TYPE_SETVERTEXCONSTANTBLOCK,
        COMMAND_TYPE_SETFRAGMENTCONSTANT,
        COMMAND_TYPE_SETFRAGMENTCONSTANTBLOCK,
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
