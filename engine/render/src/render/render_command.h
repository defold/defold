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

    struct RenderCommandCamera;

    // Compact pass command. Recorded draw commands may additionally own a camera snapshot.
    struct Command
    {
        Command(CommandType type);
        Command(CommandType type, uint64_t op0);
        Command(CommandType type, uint64_t op0, uint64_t op1);
        Command(CommandType type, uint64_t op0, uint64_t op1, uint64_t op2);
        Command(CommandType type, uint64_t op0, uint64_t op1, uint64_t op2, uint64_t op3);

        uint64_t             m_Operands[4];
        RenderCommandCamera* m_CameraSnapshot;
        CommandType          m_Type;
        uint8_t              m_HasCameraSnapshot : 1;
    };

    // Private frame table tags reserved for pass command data.
    const uint32_t RENDER_COMMAND_TABLE_TYPE = UINT32_MAX - 1;
    const uint32_t RENDER_COMMAND_PAYLOAD_TABLE_TYPE = UINT32_MAX - 2;
    const uint32_t RENDER_COMMAND_DEPENDENCY_TABLE_TYPE = UINT32_MAX - 3;

    // The selected camera's immutable values. Reapplied before each draw, like a live camera.
    struct RenderCommandCamera
    {
        dmVMath::Matrix4 m_View;
        dmVMath::Matrix4 m_Projection;
        dmVMath::Matrix4 m_ViewProjection;
        uint8_t         m_UseFrustum : 1;
    };

    // Resource kinds referenced by pass commands. Lifetime retention/versioning is D2.4.
    enum RenderCommandDependencyType
    {
        RENDER_COMMAND_DEPENDENCY_TEXTURE,
        RENDER_COMMAND_DEPENDENCY_RENDER_TARGET,
        RENDER_COMMAND_DEPENDENCY_MATERIAL,
        RENDER_COMMAND_DEPENDENCY_COMPUTE,
    };

    // Captures explicit handle operands, not inherited consumer bindings or resource lifetime.
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
     * @return Recording status; failures poison the whole frame
     */
    RenderFrameResult RecordRenderCommand(HRenderContext context, HRenderFrame frame, const Command& source, Command* destination,
                                          HRenderCamera selected_camera = 0, bool use_frustum = false, bool capture_camera = false);
    /** Copy an ordered command array into its operands' frame; the source array can be reused. */
    RenderFrameResult AppendRenderCommands(HRenderFrame frame, const Command* commands, uint32_t count);
    /** Replay published command tables in order on the render owner, without releasing frame bytes. */
    void ConsumeRenderCommands(HRenderContext context, const RenderFrameView& frame);
    /** Free temporary producer matrices/frusta, never published frame operands or borrowed resources. */
    void ReleaseRenderCommandOperands(Command* commands, uint32_t count);

    // Constant-buffer storage copied directly into the frame arena, without heap clones.
    RenderFrameResult CopyRenderCommandConstants(HRenderFrame frame, HNamedConstantBuffer source, HNamedConstantBuffer* destination);

    void ParseCommands(dmRender::HRenderContext render_context, Command* commands, uint32_t command_count);
}

#endif /* RENDER_COMMANDS_H_ */
