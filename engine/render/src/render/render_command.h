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
#include <render/render_frame.h>

namespace dmRender
{
    /**
     * Operations recorded by render scripts and replayed by the renderer.
     */
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
        COMMAND_TYPE_SET_BLEND_FUNC_SEPARATE,
        COMMAND_TYPE_SET_BLEND_EQUATION_SEPARATE,
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
        COMMAND_TYPE_ENABLE_MATERIAL,
        COMMAND_TYPE_DISABLE_MATERIAL,
        COMMAND_TYPE_SET_RENDER_CAMERA,
        COMMAND_TYPE_SET_COMPUTE,
        COMMAND_TYPE_DISPATCH_COMPUTE,
        // Internal replay command. Its camera values belong to the frame, not the camera pool.
        COMMAND_TYPE_SET_RENDER_CAMERA_SNAPSHOT,
        COMMAND_TYPE_MAX
    };

    /**
     * Compact pass command. Each command type defines how its operands are interpreted.
     */
    struct Command
    {
        Command(CommandType type);
        Command(CommandType type, uint64_t op0);
        Command(CommandType type, uint64_t op0, uint64_t op1);
        Command(CommandType type, uint64_t op0, uint64_t op1, uint64_t op2);
        Command(CommandType type, uint64_t op0, uint64_t op1, uint64_t op2, uint64_t op3);
        Command(CommandType type, uint64_t op0, uint64_t op1, uint64_t op2, uint64_t op3, uint64_t op4);

        uint64_t    m_Operands[5];
        CommandType m_Type;
    };

    /**
     * Private frame table tags reserved for pass command data.
     */
    const uint32_t RENDER_COMMAND_TABLE_TYPE = UINT32_MAX - 1;
    const uint32_t RENDER_COMMAND_PAYLOAD_TABLE_TYPE = UINT32_MAX - 2;
    const uint32_t RENDER_COMMAND_DEPENDENCY_TABLE_TYPE = UINT32_MAX - 3;

    /**
     * Draw/debug-draw operand 4 holds a camera snapshot's frame-table index + 1.
     * Zero inherits the preceding camera selection; NONE explicitly disables it.
     * SET_RENDER_CAMERA_SNAPSHOT uses operand 0 with an index + 1 or NONE.
     */
    const uint64_t RENDER_COMMAND_CAMERA_INHERIT = 0;
    const uint64_t RENDER_COMMAND_CAMERA_NONE = UINT64_MAX;

    /**
     * The selected camera's immutable values. Reapplied before each draw, like a live camera.
     */
    struct RenderCommandCamera
    {
        dmVMath::Matrix4 m_View;
        dmVMath::Matrix4 m_Projection;
        dmVMath::Matrix4 m_ViewProjection;
        uint8_t         m_UseFrustum : 1;
    };

    /**
     * Resource kinds referenced by pass commands. Lifetime retention/versioning is D2.4.
     */
    enum RenderCommandDependencyType
    {
        RENDER_COMMAND_DEPENDENCY_TEXTURE,
        RENDER_COMMAND_DEPENDENCY_RENDER_TARGET,
        RENDER_COMMAND_DEPENDENCY_MATERIAL,
        RENDER_COMMAND_DEPENDENCY_COMPUTE,
    };

    /**
     * Captures explicit handle operands, not inherited consumer bindings or resource lifetime.
     */
    struct RenderCommandDependency
    {
        uint64_t                    m_Handle;
        RenderCommandDependencyType m_Type;
    };

    /**
     * Copy a fresh producer command while its operands are alive. Do not pass commands
     * already recorded into another frame. Source ownership never transfers; release
     * temporary matrices/frusta after accepting the copy. Source and destination may alias.
     * @param context Game-owner recording context
     * @param frame Building frame receiving the immutable operands
     * @param source Producer command with borrowed operands
     * @param destination Command referencing the frame copies; append only on success
     * @param selected_camera Camera selected by the producer script
     * @param use_frustum Whether the selected camera supplies draw culling bounds
     * @param capture_camera Snapshot camera selection for this draw, including disabled selection
     * @return Recording status; failures invalidate the whole frame
     */
    RenderFrameResult RecordRenderCommand(HRenderContext context, HRenderFrameStorage frame, const Command& source, Command* destination,
                                          HRenderCamera selected_camera, bool use_frustum, bool capture_camera);

    /**
     * Copy commands in order into the frame that owns their operands.
     * The source command array can be reused after copying.
     * @param frame Building frame containing the recorded operands
     * @param commands Commands referencing operands in this frame
     * @param count Number of commands to copy
     * @return Recording status; failures invalidate the whole frame
     */
    RenderFrameResult AppendRenderCommands(HRenderFrameStorage frame, const Command* commands, uint32_t count);

    /**
     * Replay published command tables in order on the render owner.
     * Frame data remains owned by the frame and is not released here.
     * @param context Render-owner execution context
     * @param frame Published frame containing commands and their operands
     */
    void ConsumeRenderCommands(HRenderContext context, const RenderFrameView& frame);

    /**
     * Free temporary producer matrices and frusta. Does not release borrowed resources.
     * Do not pass recorded commands whose operands belong to a frame.
     * @param commands Producer commands whose temporary operands are no longer needed
     * @param count Number of commands
     */
    void ReleaseRenderCommandOperands(Command* commands, uint32_t count);

    /**
     * Copy a constant buffer into frame-owned reusable dynamic storage.
     * The source remains owned by the caller; the frame manages the copy's lifetime.
     * @param frame Building frame receiving the snapshot
     * @param source Constant buffer to copy, or null
     * @param destination Receives the snapshot; set to null when the source is null
     * @return Recording status; failures invalidate the whole frame
     */
    RenderFrameResult CopyRenderCommandConstants(HRenderFrameStorage frame, HNamedConstantBuffer source, HNamedConstantBuffer* destination);

    /**
     * Execute producer commands immediately and release their temporary matrices and frusta.
     * Use ConsumeRenderCommands for commands recorded into a frame.
     * @param render_context Render context used to execute the commands
     * @param commands Producer commands with live operands
     * @param command_count Number of commands to execute
     */
    void ParseCommands(dmRender::HRenderContext render_context, Command* commands, uint32_t command_count);
}

#endif /* RENDER_COMMANDS_H_ */
