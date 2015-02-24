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
        COMMAND_TYPE_ENABLE_RENDER_TARGET,
        COMMAND_TYPE_DISABLE_RENDER_TARGET,
        COMMAND_TYPE_ENABLE_TEXTURE,
        COMMAND_TYPE_DISABLE_TEXTURE,
        COMMAND_TYPE_CLEAR,
        COMMAND_TYPE_SET_VIEWPORT,
        COMMAND_TYPE_SET_VIEW,
        COMMAND_TYPE_SET_PROJECTION,
        COMMAND_TYPE_SET_BLEND_FUNC,
        COMMAND_TYPE_SET_COLOR_MASK,
        COMMAND_TYPE_SET_DEPTH_MASK,
        COMMAND_TYPE_SET_STENCIL_MASK,
        COMMAND_TYPE_SET_STENCIL_FUNC,
        COMMAND_TYPE_SET_STENCIL_OP,
        COMMAND_TYPE_SET_CULL_FACE,
        COMMAND_TYPE_SET_POLYGON_OFFSET,
        COMMAND_TYPE_DRAW,
        COMMAND_TYPE_DRAW_DEBUG3D,
        COMMAND_TYPE_DRAW_DEBUG2D,
        COMMAND_TYPE_ENABLE_MATERIAL,
        COMMAND_TYPE_DISABLE_MATERIAL,
        COMMAND_TYPE_MAX
    };

    struct Command
    {
        Command(CommandType type);
        Command(CommandType type, uintptr_t op0);
        Command(CommandType type, uintptr_t op0, uintptr_t op1);
        Command(CommandType type, uintptr_t op0, uintptr_t op1, uintptr_t op2);
        Command(CommandType type, uintptr_t op0, uintptr_t op1, uintptr_t op2, uintptr_t op3);

        CommandType m_Type;
        uintptr_t   m_Operands[4];
    };

    void ParseCommands(dmRender::HRenderContext render_context, Command* commands, uint32_t command_count);
}

#endif /* RENDER_COMMANDS_H_ */
