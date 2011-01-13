#include <stdint.h>
#include <gtest/gtest.h>
#include <vectormath/cpp/vectormath_aos.h>

#include "render/render.h"
#include "render/render_private.h"
#include "render/render_script.h"

#include "render/render_ddf.h"

using namespace Vectormath::Aos;

class dmRenderScriptTest : public ::testing::Test
{
protected:
    dmRender::HRenderContext m_Context;
    dmGraphics::HContext m_GraphicsContext;

    virtual void SetUp()
    {
        m_GraphicsContext = dmGraphics::NewContext();
        dmRender::RenderContextParams params;
        params.m_MaxRenderTargets = 1;
        params.m_MaxInstances = 1;
        m_Context = dmRender::NewRenderContext(m_GraphicsContext, params);
        dmGraphics::WindowParams win_params;
        dmGraphics::OpenWindow(m_GraphicsContext, &win_params);
    }

    virtual void TearDown()
    {
        dmGraphics::CloseWindow(m_GraphicsContext);
        dmRender::DeleteRenderContext(m_Context);
        dmGraphics::DeleteContext(m_GraphicsContext);
    }
};

TEST_F(dmRenderScriptTest, TestNewDelete)
{
    dmRender::HRenderScript render_script = dmRender::NewRenderScript(m_Context, "", 0, "none");
    ASSERT_NE((void*)0, render_script);
    dmRender::DeleteRenderScript(m_Context, render_script);
}

TEST_F(dmRenderScriptTest, TestNewDeleteInstance)
{
    dmRender::HRenderScript render_script = dmRender::NewRenderScript(m_Context, "", 0, "none");
    ASSERT_NE((void*)0, render_script);
    dmRender::DeleteRenderScript(m_Context, render_script);
}

TEST_F(dmRenderScriptTest, TestReload)
{
    const char* script_a =
        "function init(self)"
        "    if not self.counter then"
        "        self.count = 0"
        "    end"
        "    self.count = self.count + 1"
        "    assert(self.count == 1)"
        "end";
    const char* script_b =
        "function init(self)"
        "    assert(self.count == 1)"
        "end";

    dmRender::HRenderScript render_script = dmRender::NewRenderScript(m_Context, script_a, strlen(script_a), "none");
    ASSERT_NE((void*)0, render_script);
    dmRender::HRenderScriptInstance render_script_instance = dmRender::NewRenderScriptInstance(m_Context, render_script);
    ASSERT_NE((void*)0, render_script_instance);
    ASSERT_EQ(dmRender::RENDER_SCRIPT_RESULT_OK, dmRender::InitRenderScriptInstance(render_script_instance));
    ASSERT_TRUE(dmRender::ReloadRenderScript(m_Context, render_script, script_b, strlen(script_b), "none"));
    ASSERT_EQ(dmRender::RENDER_SCRIPT_RESULT_OK, dmRender::InitRenderScriptInstance(render_script_instance));

    dmRender::DeleteRenderScriptInstance(render_script_instance);
    dmRender::DeleteRenderScript(m_Context, render_script);
}

TEST_F(dmRenderScriptTest, TestSetRenderScript)
{
    const char* script_a =
        "function init(self)"
        "    if not self.counter then"
        "        self.count = 0"
        "    end"
        "    self.count = self.count + 1"
        "    assert(self.count == 1)"
        "end";
    const char* script_b =
        "function init(self)"
        "    assert(self.count == 1)"
        "end";

    dmRender::HRenderScript render_script_a = dmRender::NewRenderScript(m_Context, script_a, strlen(script_a), "none");
    dmRender::HRenderScript render_script_b = dmRender::NewRenderScript(m_Context, script_b, strlen(script_b), "none");
    ASSERT_NE((void*)0, render_script_a);
    ASSERT_NE((void*)0, render_script_b);
    dmRender::HRenderScriptInstance render_script_instance = dmRender::NewRenderScriptInstance(m_Context, render_script_a);
    ASSERT_NE((void*)0, render_script_instance);
    ASSERT_EQ(dmRender::RENDER_SCRIPT_RESULT_OK, dmRender::InitRenderScriptInstance(render_script_instance));
    dmRender::SetRenderScriptInstanceRenderScript(render_script_instance, render_script_b);
    ASSERT_EQ(dmRender::RENDER_SCRIPT_RESULT_OK, dmRender::InitRenderScriptInstance(render_script_instance));

    dmRender::DeleteRenderScriptInstance(render_script_instance);
    dmRender::DeleteRenderScript(m_Context, render_script_a);
    dmRender::DeleteRenderScript(m_Context, render_script_b);
}

TEST_F(dmRenderScriptTest, TestRenderScriptMaterial)
{
    const char* script = "function update(self)"
    "    render.enable_material(self, \"test_material\")"
    "end";
    dmRender::HRenderScript render_script = dmRender::NewRenderScript(m_Context, script, strlen(script), "none");
    dmRender::HRenderScriptInstance render_script_instance = dmRender::NewRenderScriptInstance(m_Context, render_script);
    dmRender::HMaterial material = dmRender::NewMaterial();
    ASSERT_EQ(dmRender::RENDER_SCRIPT_RESULT_FAILED, dmRender::UpdateRenderScriptInstance(render_script_instance));
    dmRender::AddRenderScriptInstanceMaterial(render_script_instance, "test_material", material);
    ASSERT_EQ(dmRender::RENDER_SCRIPT_RESULT_OK, dmRender::UpdateRenderScriptInstance(render_script_instance));
    dmRender::ClearRenderScriptInstanceMaterials(render_script_instance);
    ASSERT_EQ(dmRender::RENDER_SCRIPT_RESULT_FAILED, dmRender::UpdateRenderScriptInstance(render_script_instance));

    dmRender::DeleteMaterial(material);
    dmRender::DeleteRenderScriptInstance(render_script_instance);
    dmRender::DeleteRenderScript(m_Context, render_script);
}

TEST_F(dmRenderScriptTest, TestRenderScriptMessage)
{
    const char* script =
    "function update(self)\n"
    "    assert(self.width == 1)\n"
    "    assert(self.height == 1)\n"
    "end\n"
    "\n"
    "function on_message(self, message_id, message)\n"
    "    if message_id == hash(\"window_resized\") then\n"
    "        self.width = message.width\n"
    "        self.height = message.height\n"
    "    end\n"
    "end\n";
    dmRender::HRenderScript render_script = dmRender::NewRenderScript(m_Context, script, strlen(script), "none");
    dmRender::HRenderScriptInstance render_script_instance = dmRender::NewRenderScriptInstance(m_Context, render_script);

    ASSERT_EQ(dmRender::RENDER_SCRIPT_RESULT_FAILED, dmRender::UpdateRenderScriptInstance(render_script_instance));

    char buffer[sizeof(dmRenderDDF::WindowResized)];
    dmRenderDDF::WindowResized* window_resize = (dmRenderDDF::WindowResized*)buffer;
    window_resize->m_Width = 1;
    window_resize->m_Height = 1;
    dmRender::Message message;
    message.m_Id = dmHashString64(dmRenderDDF::WindowResized::m_DDFDescriptor->m_ScriptName);
    message.m_DDFDescriptor = dmRenderDDF::WindowResized::m_DDFDescriptor;
    message.m_Buffer = buffer;
    message.m_BufferSize = sizeof(dmRenderDDF::WindowResized);
    dmRender::OnMessageRenderScriptInstance(render_script_instance, &message);
    ASSERT_EQ(dmRender::RENDER_SCRIPT_RESULT_OK, dmRender::UpdateRenderScriptInstance(render_script_instance));

    dmRender::DeleteRenderScriptInstance(render_script_instance);
    dmRender::DeleteRenderScript(m_Context, render_script);
}

TEST_F(dmRenderScriptTest, TestLuaState)
{
    const char* script =
    "function update(self)\n"
    "    render.enable_state(self, render.STATE_ALPHA_TEST)\n"
    "    render.disable_state(self, render.STATE_ALPHA_TEST)\n"
    "end\n";
    dmRender::HRenderScript render_script = dmRender::NewRenderScript(m_Context, script, strlen(script), "none");
    dmRender::HRenderScriptInstance render_script_instance = dmRender::NewRenderScriptInstance(m_Context, render_script);

    ASSERT_EQ(dmRender::RENDER_SCRIPT_RESULT_OK, dmRender::UpdateRenderScriptInstance(render_script_instance));

    dmArray<dmRender::Command>& commands = render_script_instance->m_CommandBuffer;
    ASSERT_EQ(2u, commands.Size());
    dmRender::Command* command = &commands[0];
    ASSERT_EQ(dmRender::COMMAND_TYPE_ENABLE_STATE, command->m_Type);
    ASSERT_EQ(dmGraphics::STATE_ALPHA_TEST, (int32_t)command->m_Operands[0]);
    command = &commands[1];
    ASSERT_EQ(dmRender::COMMAND_TYPE_DISABLE_STATE, command->m_Type);
    ASSERT_EQ(dmGraphics::STATE_ALPHA_TEST, (int32_t)command->m_Operands[0]);

    dmRender::ParseCommands(m_Context, &commands[0], commands.Size());

    dmRender::DeleteRenderScriptInstance(render_script_instance);
    dmRender::DeleteRenderScript(m_Context, render_script);
}

TEST_F(dmRenderScriptTest, TestLuaRenderTarget)
{
    const char* script =
    "function update(self)\n"
    "    local params = {\n"
    "        format = render.TEXTURE_FORMAT_RGB,\n"
    "        width = 1,\n"
    "        height = 2,\n"
    "        min_filter = render.TEXTURE_FILTER_NEAREST,\n"
    "        mag_filter = render.TEXTURE_FILTER_LINEAR,\n"
    "        u_wrap = render.TEXTURE_WRAP_REPEAT,\n"
    "        v_wrap = render.TEXTURE_WRAP_MIRRORED_REPEAT\n"
    "    }\n"
    "    self.rt = render.render_target(self, \"rt\", {[render.BUFFER_TYPE_DEPTH_BIT] = params})\n"
    "    render.enable_render_target(self, self.rt)\n"
    "    render.disable_render_target(self, self.rt)\n"
    "    render.set_render_target_size(self, self.rt, 3, 4)\n"
    "    render.delete_render_target(self, self.rt)\n"
    "end\n";
    dmRender::HRenderScript render_script = dmRender::NewRenderScript(m_Context, script, strlen(script), "none");
    dmRender::HRenderScriptInstance render_script_instance = dmRender::NewRenderScriptInstance(m_Context, render_script);

    ASSERT_EQ(dmRender::RENDER_SCRIPT_RESULT_OK, dmRender::UpdateRenderScriptInstance(render_script_instance));

    dmArray<dmRender::Command>& commands = render_script_instance->m_CommandBuffer;
    ASSERT_EQ(2u, commands.Size());
    dmRender::Command* command = &commands[0];
    ASSERT_EQ(dmRender::COMMAND_TYPE_ENABLE_RENDER_TARGET, command->m_Type);
    ASSERT_NE((void*)0, (void*)command->m_Operands[0]);
    command = &commands[1];
    ASSERT_EQ(dmRender::COMMAND_TYPE_DISABLE_RENDER_TARGET, command->m_Type);
    ASSERT_NE((void*)0, (void*)command->m_Operands[0]);

    dmRender::ParseCommands(m_Context, &commands[0], commands.Size());

    dmRender::DeleteRenderScriptInstance(render_script_instance);
    dmRender::DeleteRenderScript(m_Context, render_script);
}

TEST_F(dmRenderScriptTest, TestLuaClear)
{
    const char* script =
    "function update(self)\n"
    "    render.clear(self, {[render.BUFFER_TYPE_COLOR_BIT] = vmath.vector4(0, 0, 0, 0), [render.BUFFER_TYPE_DEPTH_BIT] = 1})\n"
    "end\n";
    dmRender::HRenderScript render_script = dmRender::NewRenderScript(m_Context, script, strlen(script), "none");
    dmRender::HRenderScriptInstance render_script_instance = dmRender::NewRenderScriptInstance(m_Context, render_script);

    ASSERT_EQ(dmRender::RENDER_SCRIPT_RESULT_OK, dmRender::UpdateRenderScriptInstance(render_script_instance));

    dmArray<dmRender::Command>& commands = render_script_instance->m_CommandBuffer;
    ASSERT_EQ(1u, commands.Size());

    dmRender::Command* command = &commands[0];
    ASSERT_EQ(dmRender::COMMAND_TYPE_CLEAR, command->m_Type);
    uint32_t flags = command->m_Operands[0];
    ASSERT_NE(0u, flags & dmGraphics::BUFFER_TYPE_COLOR_BIT);
    ASSERT_NE(0u, flags & dmGraphics::BUFFER_TYPE_DEPTH_BIT);
    ASSERT_EQ(0u, flags & dmGraphics::BUFFER_TYPE_STENCIL_BIT);
    ASSERT_EQ(0u, command->m_Operands[1]);
    union { float f; uint32_t i; } f_to_i;
    f_to_i.f = 1.0f;
    ASSERT_EQ(f_to_i.i, command->m_Operands[2]);
    ASSERT_EQ(0u, command->m_Operands[3]);

    dmRender::ParseCommands(m_Context, &commands[0], commands.Size());

    dmRender::DeleteRenderScriptInstance(render_script_instance);
    dmRender::DeleteRenderScript(m_Context, render_script);
}

int main(int argc, char **argv)
{
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
