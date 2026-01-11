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
        COMMAND_TYPE_ENABLE_TEXTURE,
        COMMAND_TYPE_DISABLE_TEXTURE,
        COMMAND_TYPE_SET_RENDER_TARGET,
        COMMAND_TYPE_CLEAR,
        COMMAND_TYPE_SET_VIEWPORT,
        COMMAND_TYPE_SET_VIEW,
        COMMAND_TYPE_SET_PROJECTION,
        COMMAND_TYPE_SET_BLEND_FUNC,
        COMMAND_TYPE_SET_COLOR_MASK,
        COMMAND_TYPE_SET_DEPTH_MASK,
        COMMAND_TYPE_SET_DEPTH_FUNC,
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
        COMMAND_TYPE_SET_RENDER_CAMERA,
        COMMAND_TYPE_SET_COMPUTE,
        COMMAND_TYPE_DISPATCH_COMPUTE,
        COMMAND_TYPE_MAX
    };

    struct Command
    {
        Command(CommandType type);
        Command(CommandType type, uint64_t op0);
        Command(CommandType type, uint64_t op0, uint64_t op1);
        Command(CommandType type, uint64_t op0, uint64_t op1, uint64_t op2);
        Command(CommandType type, uint64_t op0, uint64_t op1, uint64_t op2, uint64_t op3);

        CommandType m_Type;
        uint64_t    m_Operands[4];
    };

    void ParseCommands(dmRender::HRenderContext render_context, Command* commands, uint32_t command_count);
}

#endif /* RENDER_COMMANDS_H_ */
