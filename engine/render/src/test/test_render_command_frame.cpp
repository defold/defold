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

#define JC_TEST_IMPLEMENTATION
#include <jc_test/jc_test.h>

#include <string.h>

#include <dlib/hash.h>
#include <platform/window.hpp>
#include <script/script.h>
#include <testmain/testmain.h>

#include "render/render_command.h"
#include "render/render_private.h"

using namespace dmRender;

static const Command* GetCommands(const RenderFrameView& frame, uint32_t* count)
{
    for (uint32_t i = 0; i < frame.m_Statistics.m_TableCount; ++i)
    {
        if (frame.m_Tables[i].m_Type == RENDER_COMMAND_TABLE_TYPE)
        {
            *count = frame.m_Tables[i].m_Count;
            return (const Command*)GetRenderFrameTableData(frame, i);
        }
    }
    *count = 0;
    return 0;
}

static void AssertMatrix(const dmVMath::Matrix4& expected, const dmVMath::Matrix4& actual)
{
    for (uint32_t column = 0; column < 4; ++column)
    {
        for (uint32_t row = 0; row < 4; ++row)
        {
            ASSERT_EQ(expected.getElem(column, row), actual.getElem(column, row));
        }
    }
}

static void AssertConstant(HNamedConstantBuffer buffer, dmhash_t name, float expected)
{
    dmVMath::Vector4* values;
    uint32_t count;
    ASSERT_TRUE(GetNamedConstant(buffer, name, &values, &count));
    ASSERT_EQ(1u, count);
    ASSERT_EQ(expected, values[0].getX());
}

// Verifies every pointer operand owns its bytes before producer mutation or deletion.
TEST(RenderCommandFrame, OwnsOperandData)
{
    RenderFrameLimits limits = { 4096, 16, 0 };
    HRenderFrame frame = NewRenderFrame(limits);
    BeginRenderFrame(frame, 31);

    dmVMath::Matrix4 matrix = dmVMath::Matrix4::translation(dmVMath::Vector3(1, 2, 3));
    dmVMath::Matrix4 expected_matrix = matrix;
    Predicate predicate = {};
    predicate.m_Tags[0] = dmHashString64("world");
    predicate.m_TagCount = 1;
    FrustumOptions frustum;
    frustum.m_Matrix = matrix;
    frustum.m_NumPlanes = FRUSTUM_PLANES_ALL;
    HNamedConstantBuffer constants = NewNamedConstantBuffer();
    dmhash_t name = dmHashString64("tint");
    dmVMath::Vector4 value(2.0f);
    SetNamedConstant(constants, name, &value, 1);

    Command commands[] = { Command(COMMAND_TYPE_SET_VIEW), Command(COMMAND_TYPE_DRAW), Command(COMMAND_TYPE_DRAW_DEBUG3D) };
    ASSERT_EQ(RENDER_FRAME_OK, RecordRenderCommand(0, frame, Command(COMMAND_TYPE_SET_VIEW, (uint64_t)&matrix), &commands[0]));
    ASSERT_EQ(RENDER_FRAME_OK, RecordRenderCommand(0, frame, Command(COMMAND_TYPE_DRAW, (uint64_t)&predicate, (uint64_t)constants, (uint64_t)&frustum, SORT_NONE), &commands[1]));
    ASSERT_EQ(RENDER_FRAME_OK, RecordRenderCommand(0, frame, Command(COMMAND_TYPE_DRAW_DEBUG3D, (uint64_t)&frustum), &commands[2]));
    ASSERT_EQ(RENDER_FRAME_OK, AppendRenderCommands(frame, commands, 3));

    matrix = dmVMath::Matrix4::identity();
    predicate.m_TagCount = 0;
    frustum.m_NumPlanes = FRUSTUM_PLANES_SIDES;
    DeleteNamedConstantBuffer(constants);
    ASSERT_EQ(RENDER_FRAME_OK, SealRenderFrame(frame));

    uint32_t count;
    const Command* recorded = GetCommands(*GetRenderFrameView(frame), &count);
    ASSERT_EQ(3u, count);
    AssertMatrix(expected_matrix, *(const dmVMath::Matrix4*)recorded[0].m_Operands[0]);
    ASSERT_EQ(1u, ((const Predicate*)recorded[1].m_Operands[0])->m_TagCount);
    ASSERT_EQ(dmHashString64("world"), ((const Predicate*)recorded[1].m_Operands[0])->m_Tags[0]);
    ASSERT_EQ(FRUSTUM_PLANES_ALL, ((const FrustumOptions*)recorded[1].m_Operands[2])->m_NumPlanes);
    ASSERT_EQ(FRUSTUM_PLANES_ALL, ((const FrustumOptions*)recorded[2].m_Operands[0])->m_NumPlanes);
    AssertConstant((HNamedConstantBuffer)recorded[1].m_Operands[1], name, 2.0f);
    ASSERT_EQ(0u, GetRenderFrameStatistics(frame).m_ReferenceCount);
    // Payload copies include matrices, predicates, frusta and constants, not only the commands.
    ASSERT_GT(GetRenderFrameStatistics(frame).m_CopiedBytes,
              (uint32_t)(sizeof(commands) + sizeof(matrix) + sizeof(predicate) + 2 * sizeof(frustum) + sizeof(value)));
    DeleteRenderFrame(frame);
}

// Verifies consecutive draw/compute commands keep distinct versions of reused constants.
TEST(RenderCommandFrame, ConstantsArePerCommand)
{
    RenderFrameLimits limits = { 4096, 16, 0 };
    HRenderFrame frame = NewRenderFrame(limits);
    BeginRenderFrame(frame, 32);
    HNamedConstantBuffer constants = NewNamedConstantBuffer();
    dmhash_t name = dmHashString64("tint");
    dmVMath::Vector4 value(3.0f);
    SetNamedConstant(constants, name, &value, 1);

    Command commands[] = { Command(COMMAND_TYPE_DRAW), Command(COMMAND_TYPE_DISPATCH_COMPUTE) };
    ASSERT_EQ(RENDER_FRAME_OK, RecordRenderCommand(0, frame, Command(COMMAND_TYPE_DRAW, 0, (uint64_t)constants, 0, SORT_NONE), &commands[0]));
    value = dmVMath::Vector4(7.0f);
    SetNamedConstant(constants, name, &value, 1);
    ASSERT_EQ(RENDER_FRAME_OK, RecordRenderCommand(0, frame, Command(COMMAND_TYPE_DISPATCH_COMPUTE, 1, 2, 3, (uint64_t)constants), &commands[1]));
    ASSERT_EQ(RENDER_FRAME_OK, AppendRenderCommands(frame, commands, 2));
    DeleteNamedConstantBuffer(constants);
    ASSERT_EQ(RENDER_FRAME_OK, SealRenderFrame(frame));

    uint32_t count;
    const Command* recorded = GetCommands(*GetRenderFrameView(frame), &count);
    ASSERT_EQ(2u, count);
    AssertConstant((HNamedConstantBuffer)recorded[0].m_Operands[1], name, 3.0f);
    AssertConstant((HNamedConstantBuffer)recorded[1].m_Operands[3], name, 7.0f);
    DeleteRenderFrame(frame);
}

// Verifies partial capture remains unpublished and cancellation reuses all arena storage.
TEST(RenderCommandFrame, PartialCaptureFailure)
{
    RenderFrameLimits limits = { sizeof(Predicate), 4, 0 };
    HRenderFrame frame = NewRenderFrame(limits);
    BeginRenderFrame(frame, 33);
    Predicate predicate = {};
    FrustumOptions frustum;
    frustum.m_Matrix = dmVMath::Matrix4::identity();
    frustum.m_NumPlanes = FRUSTUM_PLANES_ALL;
    Command copy(COMMAND_TYPE_DRAW);
    ASSERT_EQ(RENDER_FRAME_CAPACITY_EXCEEDED, RecordRenderCommand(0, frame, Command(COMMAND_TYPE_DRAW, (uint64_t)&predicate, 0, (uint64_t)&frustum, SORT_NONE), &copy));
    ASSERT_EQ(RENDER_FRAME_CAPACITY_EXCEEDED, SealRenderFrame(frame));
    ASSERT_EQ((const RenderFrameView*)0, GetRenderFrameView(frame));

    ResetRenderFrame(frame);
    BeginRenderFrame(frame, 34);
    Command clear(COMMAND_TYPE_CLEAR, 0, 0, 0, 0);
    ASSERT_EQ(RENDER_FRAME_OK, RecordRenderCommand(0, frame, clear, &copy));
    ASSERT_EQ(RENDER_FRAME_OK, AppendRenderCommands(frame, &copy, 1));
    ASSERT_EQ(RENDER_FRAME_OK, SealRenderFrame(frame));
    ASSERT_EQ((RenderFrameId)34, GetRenderFrameView(frame)->m_FrameId);
    DeleteRenderFrame(frame);
}

// Verifies pass dependencies are explicit without claiming ownership of the GPU resources.
TEST(RenderCommandFrame, ResourceDependencyDescriptors)
{
    RenderFrameLimits limits = { 1024, 8, 0 };
    HRenderFrame frame = NewRenderFrame(limits);
    BeginRenderFrame(frame, 35);
    Command sources[] = {
        Command(COMMAND_TYPE_ENABLE_TEXTURE, 0, 0, 11),
        Command(COMMAND_TYPE_SET_RENDER_TARGET, 12, 0, 0),
        Command(COMMAND_TYPE_ENABLE_MATERIAL, 13),
        Command(COMMAND_TYPE_SET_COMPUTE, 14),
    };
    for (uint32_t i = 0; i < 4; ++i)
    {
        Command copy(COMMAND_TYPE_MAX);
        ASSERT_EQ(RENDER_FRAME_OK, RecordRenderCommand(0, frame, sources[i], &copy));
    }
    ASSERT_EQ(RENDER_FRAME_OK, SealRenderFrame(frame));
    const RenderFrameView& view = *GetRenderFrameView(frame);
    ASSERT_EQ(4u, view.m_Statistics.m_TableCount);
    ASSERT_EQ(0u, view.m_Statistics.m_ReferenceCount);
    for (uint32_t i = 0; i < 4; ++i)
    {
        ASSERT_EQ(RENDER_COMMAND_DEPENDENCY_TABLE_TYPE, view.m_Tables[i].m_Type);
        const RenderCommandDependency* dependency = (const RenderCommandDependency*)GetRenderFrameTableData(view, i);
        ASSERT_EQ((uint64_t)(11 + i), dependency->m_Handle);
        ASSERT_EQ((RenderCommandDependencyType)i, dependency->m_Type);
    }
    DeleteRenderFrame(frame);
}

// Null-backend context for replay tests; recording uses the same arena as production.
class RenderCommandReplay : public jc_test_base_class
{
    protected:
    dmGraphics::HContext m_Graphics;
    dmScript::HContext   m_Script;
    HRenderContext       m_Context;
    HRenderFrame         m_Frame;
    HWindow              m_Window;

    virtual void SetUp()
    {
        dmGraphics::InstallAdapter(dmGraphics::ADAPTER_FAMILY_NONE);
        WindowCreateParams window_params;
        WindowCreateParamsInitialize(&window_params);
        window_params.m_Width = 20;
        window_params.m_Height = 10;
        m_Window = dmPlatform::NewWindow();
        dmPlatform::OpenWindow(m_Window, window_params);
        dmGraphics::ContextParams graphics_params;
        graphics_params.m_Width = 20;
        graphics_params.m_Height = 10;
        graphics_params.m_Window = m_Window;
        m_Graphics = dmGraphics::NewContext(graphics_params);

        dmScript::ContextParams script_params = {};
        script_params.m_GraphicsContext = m_Graphics;
        m_Script = dmScript::NewContext(script_params);
        dmScript::Initialize(m_Script);
        RenderContextParams render_params;
        render_params.m_ScriptContext = m_Script;
        render_params.m_MaxBatches = 1;
        m_Context = NewRenderContext(m_Graphics, render_params);
        RenderFrameLimits limits = { 16384, 64, 0 };
        m_Frame = NewRenderFrame(limits);
        BeginRenderFrame(m_Frame, 36);
    }

    virtual void TearDown()
    {
        DeleteRenderFrame(m_Frame);
        DeleteRenderContext(m_Context, 0);
        dmGraphics::CloseWindow(m_Graphics);
        dmGraphics::DeleteContext(m_Graphics);
        dmPlatform::CloseWindow(m_Window);
        dmPlatform::DeleteWindow(m_Window);
        dmScript::Finalize(m_Script);
        dmScript::DeleteContext(m_Script);
    }
};

// Verifies command-only frames replay repeatedly without freeing immutable frame operands.
TEST_F(RenderCommandReplay, ReplayDoesNotDestroyOperands)
{
    dmVMath::Matrix4* source_matrix = new dmVMath::Matrix4(dmVMath::Matrix4::translation(dmVMath::Vector3(4, 5, 6)));
    dmVMath::Matrix4 expected_matrix = *source_matrix;
    Command source(COMMAND_TYPE_SET_VIEW, (uint64_t)source_matrix);
    Command commands[] = { Command(COMMAND_TYPE_SET_VIEW), Command(COMMAND_TYPE_CLEAR, dmGraphics::BUFFER_TYPE_STENCIL_BIT, 0, 0, 0) };
    ASSERT_EQ(RENDER_FRAME_OK, RecordRenderCommand(m_Context, m_Frame, source, &commands[0]));
    ReleaseRenderCommandOperands(&source, 1);
    ASSERT_EQ((uint64_t)0, source.m_Operands[0]);
    ASSERT_EQ(RENDER_FRAME_OK, AppendRenderCommands(m_Frame, commands, 2));
    ASSERT_EQ(RENDER_FRAME_OK, SealRenderFrame(m_Frame));

    ConsumeRenderCommands(m_Context, *GetRenderFrameView(m_Frame));
    AssertMatrix(expected_matrix, GetViewMatrix(m_Context));
    ASSERT_TRUE(m_Context->m_StencilBufferCleared);
    SetViewMatrix(m_Context, dmVMath::Matrix4::identity());
    ConsumeRenderCommands(m_Context, *GetRenderFrameView(m_Frame));
    AssertMatrix(expected_matrix, GetViewMatrix(m_Context));
}

// Verifies camera selection is copied per pass and never resolves deleted game-owned cameras.
TEST_F(RenderCommandReplay, CameraSnapshotsSurviveDeletion)
{
    HRenderCamera camera = NewRenderCamera(m_Context);
    RenderCamera* camera_data = m_Context->m_Recording.m_RenderCameras.Get(camera);
    camera_data->m_View = dmVMath::Matrix4::translation(dmVMath::Vector3(1, 2, 3));
    camera_data->m_Projection = dmVMath::Matrix4::identity();
    camera_data->m_ViewProjection = camera_data->m_View;
    camera_data->m_Dirty = 0;
    const dmVMath::Matrix4 first_view = camera_data->m_View;

    Command commands[] = { Command(COMMAND_TYPE_MAX), Command(COMMAND_TYPE_DRAW, 0, 0, 0, SORT_NONE),
                           Command(COMMAND_TYPE_MAX), Command(COMMAND_TYPE_SET_VIEW), Command(COMMAND_TYPE_DRAW, 0, 0, 0, SORT_NONE) };
    ASSERT_EQ(RENDER_FRAME_OK, RecordRenderCommand(m_Context, m_Frame, Command(COMMAND_TYPE_SET_RENDER_CAMERA, camera, 1), &commands[0]));
    camera_data->m_View = dmVMath::Matrix4::translation(dmVMath::Vector3(7, 8, 9));
    camera_data->m_ViewProjection = camera_data->m_View;
    const dmVMath::Matrix4 second_view = camera_data->m_View;
    ASSERT_EQ(RENDER_FRAME_OK, RecordRenderCommand(m_Context, m_Frame, Command(COMMAND_TYPE_SET_RENDER_CAMERA, camera, 1), &commands[2]));
    dmVMath::Matrix4 changed_view = dmVMath::Matrix4::identity();
    ASSERT_EQ(RENDER_FRAME_OK, RecordRenderCommand(m_Context, m_Frame, Command(COMMAND_TYPE_SET_VIEW, (uint64_t)&changed_view), &commands[3]));
    ASSERT_EQ(RENDER_FRAME_OK, AppendRenderCommands(m_Frame, commands, 5));
    DeleteRenderCamera(m_Context, camera);
    ASSERT_EQ(RENDER_FRAME_OK, SealRenderFrame(m_Frame));

    uint32_t count;
    const Command* recorded = GetCommands(*GetRenderFrameView(m_Frame), &count);
    ASSERT_EQ(5u, count);
    AssertMatrix(first_view, ((const RenderCommandCamera*)recorded[0].m_Operands[0])->m_View);
    AssertMatrix(second_view, ((const RenderCommandCamera*)recorded[2].m_Operands[0])->m_View);
    RenderListBegin(m_Context);
    RenderListEnd(m_Context);
    ConsumeRenderCommands(m_Context, *GetRenderFrameView(m_Frame));
    AssertMatrix(second_view, GetViewMatrix(m_Context));
    ASSERT_EQ((HRenderCamera)0, m_Context->m_CurrentRenderCamera);
}

// Verifies debug drawing uses the same immutable camera override as ordinary draw passes.
TEST_F(RenderCommandReplay, DebugDrawUsesCameraSnapshot)
{
    HRenderCamera camera = NewRenderCamera(m_Context);
    RenderCamera* camera_data = m_Context->m_Recording.m_RenderCameras.Get(camera);
    const dmVMath::Matrix4 expected_view = dmVMath::Matrix4::translation(dmVMath::Vector3(3, 4, 5));
    camera_data->m_View = expected_view;
    camera_data->m_Projection = dmVMath::Matrix4::identity();
    camera_data->m_ViewProjection = expected_view;
    camera_data->m_Dirty = 0;
    Command commands[] = { Command(COMMAND_TYPE_SET_RENDER_CAMERA, camera, 1), Command(COMMAND_TYPE_DRAW_DEBUG3D, 0) };
    // In-place recording must copy the source before replacing the camera handle with payload.
    ASSERT_EQ(RENDER_FRAME_OK, RecordRenderCommand(m_Context, m_Frame, commands[0], &commands[0]));
    ASSERT_EQ(RENDER_FRAME_OK, AppendRenderCommands(m_Frame, commands, 2));
    DeleteRenderCamera(m_Context, camera);
    ASSERT_EQ(RENDER_FRAME_OK, SealRenderFrame(m_Frame));
    ConsumeRenderCommands(m_Context, *GetRenderFrameView(m_Frame));
    AssertMatrix(expected_view, GetViewMatrix(m_Context));
}

// Verifies each draw sees preceding camera changes, while later mutations cannot affect it.
TEST_F(RenderCommandReplay, CapturesCameraAtEachDraw)
{
    HRenderCamera camera = NewRenderCamera(m_Context);
    RenderCamera* camera_data = m_Context->m_Recording.m_RenderCameras.Get(camera);
    camera_data->m_View = dmVMath::Matrix4::translation(dmVMath::Vector3(1, 2, 3));
    camera_data->m_Projection = dmVMath::Matrix4::identity();
    camera_data->m_ViewProjection = camera_data->m_View;
    camera_data->m_Dirty = 0;
    const dmVMath::Matrix4 first_view = camera_data->m_View;
    Command commands[] = { Command(COMMAND_TYPE_DRAW, 0, 0, 0, SORT_NONE), Command(COMMAND_TYPE_DRAW_DEBUG3D, 0) };
    ASSERT_EQ(RENDER_FRAME_OK, RecordRenderCommand(m_Context, m_Frame, commands[0], &commands[0], camera, true, true));

    camera_data->m_View = dmVMath::Matrix4::translation(dmVMath::Vector3(7, 8, 9));
    camera_data->m_ViewProjection = camera_data->m_View;
    const dmVMath::Matrix4 second_view = camera_data->m_View;
    ASSERT_EQ(RENDER_FRAME_OK, RecordRenderCommand(m_Context, m_Frame, commands[1], &commands[1], camera, true, true));
    ASSERT_EQ(RENDER_FRAME_OK, AppendRenderCommands(m_Frame, commands, 2));
    DeleteRenderCamera(m_Context, camera);
    ASSERT_EQ(RENDER_FRAME_OK, SealRenderFrame(m_Frame));

    ASSERT_TRUE(commands[0].m_HasCameraSnapshot);
    ASSERT_TRUE(commands[1].m_HasCameraSnapshot);
    AssertMatrix(first_view, commands[0].m_CameraSnapshot->m_View);
    AssertMatrix(second_view, commands[1].m_CameraSnapshot->m_View);
    RenderListBegin(m_Context);
    RenderListEnd(m_Context);
    ConsumeRenderCommands(m_Context, *GetRenderFrameView(m_Frame));
    AssertMatrix(second_view, GetViewMatrix(m_Context));
}

// Verifies an explicitly disabled per-draw camera does not inherit an earlier selection.
TEST_F(RenderCommandReplay, PerDrawCameraCanBeDisabled)
{
    HRenderCamera camera = NewRenderCamera(m_Context);
    RenderCamera* camera_data = m_Context->m_Recording.m_RenderCameras.Get(camera);
    camera_data->m_View = dmVMath::Matrix4::identity();
    camera_data->m_Projection = dmVMath::Matrix4::identity();
    camera_data->m_ViewProjection = camera_data->m_View;
    camera_data->m_Dirty = 0;
    dmVMath::Matrix4 manual_view = dmVMath::Matrix4::translation(dmVMath::Vector3(2, 3, 4));
    Command commands[] = { Command(COMMAND_TYPE_SET_RENDER_CAMERA, camera, 1),
                           Command(COMMAND_TYPE_SET_VIEW, (uint64_t)&manual_view), Command(COMMAND_TYPE_DRAW, 0, 0, 0, SORT_NONE) };
    ASSERT_EQ(RENDER_FRAME_OK, RecordRenderCommand(m_Context, m_Frame, commands[0], &commands[0]));
    ASSERT_EQ(RENDER_FRAME_OK, RecordRenderCommand(m_Context, m_Frame, commands[1], &commands[1]));
    ASSERT_EQ(RENDER_FRAME_OK, RecordRenderCommand(m_Context, m_Frame, commands[2], &commands[2], 0, false, true));
    ASSERT_TRUE(commands[2].m_HasCameraSnapshot);
    ASSERT_EQ((RenderCommandCamera*)0, commands[2].m_CameraSnapshot);
    ASSERT_EQ(RENDER_FRAME_OK, AppendRenderCommands(m_Frame, commands, 3));
    DeleteRenderCamera(m_Context, camera);
    ASSERT_EQ(RENDER_FRAME_OK, SealRenderFrame(m_Frame));

    RenderListBegin(m_Context);
    RenderListEnd(m_Context);
    ConsumeRenderCommands(m_Context, *GetRenderFrameView(m_Frame));
    AssertMatrix(manual_view, GetViewMatrix(m_Context));
}

extern "C" void dmExportedSymbols();

int main(int argc, char** argv)
{
    dmExportedSymbols();
    TestMainPlatformInit();
    jc_test_init(&argc, argv);
    return jc_test_run_all();
}
