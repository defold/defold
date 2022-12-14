// Copyright 2020-2022 The Defold Foundation
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

#include <stdint.h>
#define JC_TEST_IMPLEMENTATION
#include <jc_test/jc_test.h>
#include <dmsdk/dlib/vmath.h>

#include "render/render.h"
#include "render/font_renderer.h"
#include "render/render_private.h"
#include "render/render_script.h"

#include "render/render_ddf.h"

using namespace dmVMath;

namespace
{
    // NOTE: we don't generate actual bytecode for this test-data, so
    // just pass in regular lua source instead.
    dmLuaDDF::LuaSource *LuaSourceFromString(const char *source)
    {
        static dmLuaDDF::LuaSource tmp;
        memset(&tmp, 0x00, sizeof(tmp));
        tmp.m_Script.m_Data = (uint8_t*)source;
        tmp.m_Script.m_Count = strlen(source);
        tmp.m_Bytecode.m_Data = (uint8_t*)source;
        tmp.m_Bytecode.m_Count = strlen(source);
        tmp.m_Bytecode64.m_Data = (uint8_t*)source;
        tmp.m_Bytecode64.m_Count = strlen(source);
        tmp.m_Filename = "render-dummy";
        return &tmp;
    }
}

class dmRenderScriptTest : public jc_test_base_class
{
protected:
    dmScript::HContext m_ScriptContext;
    dmRender::HRenderContext m_Context;
    dmGraphics::HContext m_GraphicsContext;
    dmRender::HFontMap m_SystemFontMap;
    dmRender::HMaterial m_FontMaterial;
    dmGraphics::HVertexProgram m_VertexProgram;
    dmGraphics::HFragmentProgram m_FragmentProgram;

    virtual void SetUp()
    {
        m_ScriptContext = dmScript::NewContext(0, 0, true);
        dmScript::Initialize(m_ScriptContext);
        dmGraphics::Initialize();
        m_GraphicsContext = dmGraphics::NewContext(dmGraphics::ContextParams());
        dmRender::FontMapParams font_map_params;
        font_map_params.m_CacheWidth = 128;
        font_map_params.m_CacheHeight = 128;
        font_map_params.m_CacheCellWidth = 8;
        font_map_params.m_CacheCellHeight = 8;
        font_map_params.m_Glyphs.SetCapacity(128);
        font_map_params.m_Glyphs.SetSize(128);
        memset((void*)&font_map_params.m_Glyphs[0], 0, sizeof(dmRender::Glyph)*128);
        for (uint32_t i = 0; i < 128; ++i)
        {
            font_map_params.m_Glyphs[i].m_Width = 1;
            font_map_params.m_Glyphs[i].m_Character = i;
        }
        m_SystemFontMap = dmRender::NewFontMap(m_GraphicsContext, font_map_params);
        dmRender::RenderContextParams params;
        params.m_ScriptContext = m_ScriptContext;
        params.m_SystemFontMap = m_SystemFontMap;
        params.m_MaxRenderTargets = 1;
        params.m_MaxInstances = 64;
        params.m_MaxCharacters = 32;
        m_Context = dmRender::NewRenderContext(m_GraphicsContext, params);

        dmGraphics::ShaderDesc::Shader shader_ddf;
        memset(&shader_ddf,0,sizeof(shader_ddf));
        shader_ddf.m_Source.m_Data  = (uint8_t*)"foo";
        shader_ddf.m_Source.m_Count = 3;

        m_VertexProgram = dmGraphics::NewVertexProgram(m_GraphicsContext, &shader_ddf);
        m_FragmentProgram = dmGraphics::NewFragmentProgram(m_GraphicsContext, &shader_ddf);

        m_FontMaterial = dmRender::NewMaterial(m_Context, m_VertexProgram, m_FragmentProgram);
        dmRender::SetFontMapMaterial(m_SystemFontMap, m_FontMaterial);

        dmGraphics::WindowParams win_params;
        win_params.m_Width = 20;
        win_params.m_Height = 10;
        dmGraphics::OpenWindow(m_GraphicsContext, &win_params);
    }

    virtual void TearDown()
    {
        dmGraphics::DeleteVertexProgram(m_VertexProgram);
        dmGraphics::DeleteFragmentProgram(m_FragmentProgram);
        dmRender::DeleteMaterial(m_Context, m_FontMaterial);

        dmGraphics::CloseWindow(m_GraphicsContext);
        dmRender::DeleteRenderContext(m_Context, 0);
        dmRender::DeleteFontMap(m_SystemFontMap);
        dmGraphics::DeleteContext(m_GraphicsContext);
        dmScript::Finalize(m_ScriptContext);
        dmScript::DeleteContext(m_ScriptContext);
    }
};

TEST_F(dmRenderScriptTest, TestNewDelete)
{
    dmRender::HRenderScript render_script = dmRender::NewRenderScript(m_Context, LuaSourceFromString(""));
    ASSERT_NE((void*)0, render_script);
    dmRender::DeleteRenderScript(m_Context, render_script);
}

TEST_F(dmRenderScriptTest, TestNewDeleteInstance)
{
    dmRender::HRenderScript render_script = dmRender::NewRenderScript(m_Context, LuaSourceFromString(""));
    ASSERT_NE((void*)0, render_script);
    dmRender::DeleteRenderScript(m_Context, render_script);
}

TEST_F(dmRenderScriptTest, TestReload)
{
    const char* script_a =
        "function init(self)\n"
        "    if not self.counter then\n"
        "        self.count = 0\n"
        "    end\n"
        "    self.count = self.count + 1\n"
        "    assert(self.count == 1)\n"
        "end\n";
    const char* script_b =
        "function init(self)\n"
        "    assert(self.count == 1)\n"
        "end\n";

    dmRender::HRenderScript render_script = dmRender::NewRenderScript(m_Context, LuaSourceFromString(script_a));
    ASSERT_NE((void*)0, render_script);
    dmRender::HRenderScriptInstance render_script_instance = dmRender::NewRenderScriptInstance(m_Context, render_script);
    ASSERT_NE((void*)0, render_script_instance);
    ASSERT_EQ(dmRender::RENDER_SCRIPT_RESULT_OK, dmRender::InitRenderScriptInstance(render_script_instance));
    ASSERT_TRUE(dmRender::ReloadRenderScript(m_Context, render_script, LuaSourceFromString(script_b)));
    ASSERT_EQ(dmRender::RENDER_SCRIPT_RESULT_OK, dmRender::InitRenderScriptInstance(render_script_instance));

    dmRender::DeleteRenderScriptInstance(render_script_instance);
    dmRender::DeleteRenderScript(m_Context, render_script);
}

TEST_F(dmRenderScriptTest, TestSetRenderScript)
{
    const char* script_a =
        "function init(self)\n"
        "    if not self.counter then\n"
        "        self.count = 0\n"
        "    end\n"
        "    self.count = self.count + 1\n"
        "    assert(self.count == 1)\n"
        "end\n";
    const char* script_b =
        "function init(self)\n"
        "    assert(self.count == 1)\n"
        "end\n";

    dmRender::HRenderScript render_script_a = dmRender::NewRenderScript(m_Context, LuaSourceFromString(script_a));
    dmRender::HRenderScript render_script_b = dmRender::NewRenderScript(m_Context, LuaSourceFromString(script_b));
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
    const char* script = "function init(self)\n"
    "    render.enable_material(\"test_material\")\n"
    "    render.disable_material(\"test_material\")\n"
    "end\n";
    dmRender::HRenderScript render_script = dmRender::NewRenderScript(m_Context, LuaSourceFromString(script));
    dmRender::HRenderScriptInstance render_script_instance = dmRender::NewRenderScriptInstance(m_Context, render_script);
    dmRender::HMaterial material = dmRender::NewMaterial(m_Context, m_VertexProgram, m_FragmentProgram);
    ASSERT_EQ(dmRender::RENDER_SCRIPT_RESULT_FAILED, dmRender::InitRenderScriptInstance(render_script_instance));
    dmRender::AddRenderScriptInstanceMaterial(render_script_instance, "test_material", material);
    ASSERT_EQ(dmRender::RENDER_SCRIPT_RESULT_OK, dmRender::InitRenderScriptInstance(render_script_instance));

    dmArray<dmRender::Command>& commands = render_script_instance->m_CommandBuffer;
    ASSERT_EQ(2u, commands.Size());

    dmRender::Command* command = &commands[0];
    ASSERT_EQ(dmRender::COMMAND_TYPE_ENABLE_MATERIAL, command->m_Type);
    ASSERT_NE((void*)0, (void*)command->m_Operands[0]);

    command = &commands[1];
    ASSERT_EQ(dmRender::COMMAND_TYPE_DISABLE_MATERIAL, command->m_Type);

    dmRender::ClearRenderScriptInstanceMaterials(render_script_instance);
    ASSERT_EQ(dmRender::RENDER_SCRIPT_RESULT_FAILED, dmRender::InitRenderScriptInstance(render_script_instance));

    dmRender::ParseCommands(m_Context, &commands[0], commands.Size());

    dmRender::DeleteMaterial(m_Context, material);
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
    "function on_message(self, message_id, message, sender)\n"
    "    if message_id == hash(\"window_resized\") then\n"
    "        self.width = message.width\n"
    "        self.height = message.height\n"
    "    end\n"
    "    assert(sender.path == hash(\"test_path\"), \"incorrect path\")\n"
    "end\n";
    dmRender::HRenderScript render_script = dmRender::NewRenderScript(m_Context, LuaSourceFromString(script));
    dmRender::HRenderScriptInstance render_script_instance = dmRender::NewRenderScriptInstance(m_Context, render_script);

    ASSERT_EQ(dmRender::RENDER_SCRIPT_RESULT_OK, dmRender::DispatchRenderScriptInstance(render_script_instance));
    ASSERT_EQ(dmRender::RENDER_SCRIPT_RESULT_FAILED, dmRender::UpdateRenderScriptInstance(render_script_instance, 0.0f));

    dmRenderDDF::WindowResized window_resize;
    window_resize.m_Width = 1;
    window_resize.m_Height = 1;
    dmhash_t message_id = dmRenderDDF::WindowResized::m_DDFDescriptor->m_NameHash;
    uintptr_t descriptor = (uintptr_t)dmRenderDDF::WindowResized::m_DDFDescriptor;
    uint32_t data_size = sizeof(dmRenderDDF::WindowResized);
    dmMessage::URL sender;
    dmMessage::ResetURL(&sender);
    sender.m_Path = dmHashString64("test_path");
    dmMessage::URL receiver;
    dmMessage::ResetURL(&receiver);
    ASSERT_EQ(dmMessage::RESULT_OK, dmMessage::GetSocket(dmRender::RENDER_SOCKET_NAME, &receiver.m_Socket));
    ASSERT_EQ(dmMessage::RESULT_OK, dmMessage::Post(&sender, &receiver, message_id, 0, descriptor, &window_resize, data_size, 0));
    ASSERT_EQ(dmRender::RENDER_SCRIPT_RESULT_OK, dmRender::DispatchRenderScriptInstance(render_script_instance));
    ASSERT_EQ(dmRender::RENDER_SCRIPT_RESULT_OK, dmRender::UpdateRenderScriptInstance(render_script_instance, 0.0f));

    dmRender::DeleteRenderScriptInstance(render_script_instance);
    dmRender::DeleteRenderScript(m_Context, render_script);
}

TEST_F(dmRenderScriptTest, TestRenderScriptUserMessage)
{
    const char* script =
    "function init(self)\n"
    "    msg.post(\"@render:\", \"test_message\", {test_value = 1})\n"
    "end\n"
    "function update(self)\n"
    "    assert(self.received)\n"
    "end\n"
    "\n"
    "function on_message(self, message_id, message, sender)\n"
    "    print(message_id)\n"
    "    if message_id == hash(\"test_message\") then\n"
    "        self.received = message.test_value == 1\n"
    "    end\n"
    "end\n";
    dmRender::HRenderScript render_script = dmRender::NewRenderScript(m_Context, LuaSourceFromString(script));
    dmRender::HRenderScriptInstance render_script_instance = dmRender::NewRenderScriptInstance(m_Context, render_script);

    ASSERT_EQ(dmRender::RENDER_SCRIPT_RESULT_OK, dmRender::DispatchRenderScriptInstance(render_script_instance));
    ASSERT_EQ(dmRender::RENDER_SCRIPT_RESULT_FAILED, dmRender::UpdateRenderScriptInstance(render_script_instance, 0.0f));

    ASSERT_EQ(dmRender::RENDER_SCRIPT_RESULT_OK, dmRender::InitRenderScriptInstance(render_script_instance));
    ASSERT_EQ(dmRender::RENDER_SCRIPT_RESULT_OK, dmRender::DispatchRenderScriptInstance(render_script_instance));
    ASSERT_EQ(dmRender::RENDER_SCRIPT_RESULT_OK, dmRender::UpdateRenderScriptInstance(render_script_instance, 0.0f));

    dmRender::DeleteRenderScriptInstance(render_script_instance);
    dmRender::DeleteRenderScript(m_Context, render_script);
}

TEST_F(dmRenderScriptTest, TestLuaState)
{
    const char* script =
    "function update(self)\n"
    "    render.enable_state(render.STATE_BLEND)\n"
    "    render.disable_state(render.STATE_BLEND)\n"
    "    render.set_blend_func(render.BLEND_ONE, render.BLEND_SRC_COLOR)\n"
    "    render.set_color_mask(true, true, true, true)\n"
    "    render.set_depth_mask(true)\n"
    "    render.set_depth_func(render.COMPARE_FUNC_GEQUAL)\n"
    "    render.set_stencil_mask(1)\n"
    "    render.set_stencil_func(render.COMPARE_FUNC_ALWAYS, 1, 2)\n"
    "    render.set_stencil_op(render.STENCIL_OP_REPLACE, render.STENCIL_OP_KEEP, render.STENCIL_OP_INVERT)\n"
    "    render.set_cull_face(render.FACE_BACK)\n"
    "    render.set_polygon_offset(1, 2)\n"
    "end\n";
    dmRender::HRenderScript render_script = dmRender::NewRenderScript(m_Context, LuaSourceFromString(script));
    dmRender::HRenderScriptInstance render_script_instance = dmRender::NewRenderScriptInstance(m_Context, render_script);

    ASSERT_EQ(dmRender::RENDER_SCRIPT_RESULT_OK, dmRender::DispatchRenderScriptInstance(render_script_instance));
    ASSERT_EQ(dmRender::RENDER_SCRIPT_RESULT_OK, dmRender::UpdateRenderScriptInstance(render_script_instance, 0.0f));

    dmArray<dmRender::Command>& commands = render_script_instance->m_CommandBuffer;
    ASSERT_EQ(11u, commands.Size());

    dmRender::Command* command = &commands[0];
    ASSERT_EQ(dmRender::COMMAND_TYPE_ENABLE_STATE, command->m_Type);
    ASSERT_EQ(dmGraphics::STATE_BLEND, (int32_t)command->m_Operands[0]);

    command = &commands[1];
    ASSERT_EQ(dmRender::COMMAND_TYPE_DISABLE_STATE, command->m_Type);
    ASSERT_EQ(dmGraphics::STATE_BLEND, (int32_t)command->m_Operands[0]);

    command = &commands[2];
    ASSERT_EQ(dmRender::COMMAND_TYPE_SET_BLEND_FUNC, command->m_Type);
    ASSERT_EQ(dmGraphics::BLEND_FACTOR_ONE, (int32_t)command->m_Operands[0]);
    ASSERT_EQ(dmGraphics::BLEND_FACTOR_SRC_COLOR, (int32_t)command->m_Operands[1]);

    command = &commands[3];
    ASSERT_EQ(dmRender::COMMAND_TYPE_SET_COLOR_MASK, command->m_Type);
    ASSERT_EQ(1u, command->m_Operands[0]);
    ASSERT_EQ(1u, command->m_Operands[1]);
    ASSERT_EQ(1u, command->m_Operands[2]);
    ASSERT_EQ(1u, command->m_Operands[3]);

    command = &commands[4];
    ASSERT_EQ(dmRender::COMMAND_TYPE_SET_DEPTH_MASK, command->m_Type);
    ASSERT_EQ(1u, command->m_Operands[0]);

    command = &commands[5];
    ASSERT_EQ(dmRender::COMMAND_TYPE_SET_DEPTH_FUNC, command->m_Type);
    ASSERT_EQ(dmGraphics::COMPARE_FUNC_GEQUAL, (int32_t)command->m_Operands[0]);

    command = &commands[6];
    ASSERT_EQ(dmRender::COMMAND_TYPE_SET_STENCIL_MASK, command->m_Type);
    ASSERT_EQ(1u, command->m_Operands[0]);

    command = &commands[7];
    ASSERT_EQ(dmRender::COMMAND_TYPE_SET_STENCIL_FUNC, command->m_Type);
    ASSERT_EQ(dmGraphics::COMPARE_FUNC_ALWAYS, (int32_t)command->m_Operands[0]);
    ASSERT_EQ(1, (int32_t)command->m_Operands[1]);
    ASSERT_EQ(2, (int32_t)command->m_Operands[2]);

    command = &commands[8];
    ASSERT_EQ(dmRender::COMMAND_TYPE_SET_STENCIL_OP, command->m_Type);
    ASSERT_EQ(dmGraphics::STENCIL_OP_REPLACE, (int32_t)command->m_Operands[0]);
    ASSERT_EQ(dmGraphics::STENCIL_OP_KEEP, (int32_t)command->m_Operands[1]);
    ASSERT_EQ(dmGraphics::STENCIL_OP_INVERT, (int32_t)command->m_Operands[2]);

    command = &commands[9];
    ASSERT_EQ(dmRender::COMMAND_TYPE_SET_CULL_FACE, command->m_Type);
    ASSERT_EQ(dmGraphics::FACE_TYPE_BACK, (int32_t)command->m_Operands[0]);

    command = &commands[10];
    ASSERT_EQ(dmRender::COMMAND_TYPE_SET_POLYGON_OFFSET, command->m_Type);
    ASSERT_EQ(1u, command->m_Operands[0]);
    ASSERT_EQ(2u, command->m_Operands[1]);

    dmRender::ParseCommands(m_Context, &commands[0], commands.Size());

    dmRender::DeleteRenderScriptInstance(render_script_instance);
    dmRender::DeleteRenderScript(m_Context, render_script);
}

TEST_F(dmRenderScriptTest, TestLuaRenderTargetTooLarge)
{
    const char* script =
    "function init(self)\n"
    "    local params_color = {\n"
    "        format = render.FORMAT_RGBA,\n"
    "        width = 1000000000,\n"
    "        height = 2,\n"
    "        min_filter = render.FILTER_NEAREST,\n"
    "        mag_filter = render.FILTER_LINEAR,\n"
    "        u_wrap = render.WRAP_REPEAT,\n"
    "        v_wrap = render.WRAP_MIRRORED_REPEAT\n"
    "    }\n"
    "    self.rt = render.render_target({[render.BUFFER_COLOR_BIT] = params_color})\n"
    "end\n";

    dmRender::HRenderScript render_script = dmRender::NewRenderScript(m_Context, LuaSourceFromString(script));
    dmRender::HRenderScriptInstance render_script_instance = dmRender::NewRenderScriptInstance(m_Context, render_script);
    ASSERT_EQ(dmRender::RENDER_SCRIPT_RESULT_FAILED, dmRender::InitRenderScriptInstance(render_script_instance));
    dmRender::DeleteRenderScriptInstance(render_script_instance);
    dmRender::DeleteRenderScript(m_Context, render_script);
}

TEST_F(dmRenderScriptTest, TestLuaRenderTarget)
{
    const char* script =
    "function update(self)\n"
    "    local params_color = {\n"
    "        format = render.FORMAT_RGBA,\n"
    "        width = 1,\n"
    "        height = 2,\n"
    "        min_filter = render.FILTER_NEAREST,\n"
    "        mag_filter = render.FILTER_LINEAR,\n"
    "        u_wrap = render.WRAP_REPEAT,\n"
    "        v_wrap = render.WRAP_MIRRORED_REPEAT\n"
    "    }\n"
    "    local params_depth = {\n"
    "        format = render.FORMAT_DEPTH,\n"
    "        width = 1,\n"
    "        height = 2,\n"
    "        min_filter = render.FILTER_NEAREST,\n"
    "        mag_filter = render.FILTER_LINEAR,\n"
    "        u_wrap = render.WRAP_REPEAT,\n"
    "        v_wrap = render.WRAP_MIRRORED_REPEAT\n"
    "    }\n"
    "    local params_stencil = {\n"
    "        format = render.FORMAT_STENCIL,\n"
    "        width = 1,\n"
    "        height = 2,\n"
    "        min_filter = render.FILTER_NEAREST,\n"
    "        mag_filter = render.FILTER_LINEAR,\n"
    "        u_wrap = render.WRAP_REPEAT,\n"
    "        v_wrap = render.WRAP_MIRRORED_REPEAT\n"
    "    }\n"
    "    self.rt = render.render_target({[render.BUFFER_COLOR_BIT] = params_color, [render.BUFFER_DEPTH_BIT] = params_depth, [render.BUFFER_STENCIL_BIT] = params_stencil})\n"
    "    render.set_render_target(self.rt, {transient = {render.BUFFER_DEPTH_BIT, render.BUFFER_STENCIL_BIT}})\n"
    "    render.set_render_target(self.rt)\n"
    "    render.set_render_target_size(self.rt, 3, 4)\n"
            " local rt_w = render.get_render_target_width(self.rt, render.BUFFER_COLOR_BIT)\n"
            " assert(rt_w == 3)\n"
            " local rt_h = render.get_render_target_height(self.rt, render.BUFFER_COLOR_BIT)\n"
            " assert(rt_h == 4)\n"
            " local rt_w = render.get_render_target_width(self.rt, render.BUFFER_DEPTH_BIT)\n"
            " assert(rt_w == 3)\n"
            " local rt_h = render.get_render_target_height(self.rt, render.BUFFER_DEPTH_BIT)\n"
            " assert(rt_h == 4)\n"
            " local rt_w = render.get_render_target_width(self.rt, render.BUFFER_DEPTH_BIT)\n"
            " assert(rt_w == 3)\n"
            " local rt_h = render.get_render_target_height(self.rt, render.BUFFER_DEPTH_BIT)\n"
            " assert(rt_h == 4)\n"
    "    render.set_render_target(nil, {transient = {render.BUFFER_COLOR_BIT}})\n"
    "    render.delete_render_target(self.rt)\n"
    "    render.set_render_target(nil, {transient = {}})\n"
    "    render.set_render_target(nil, {})\n"
    "    render.set_render_target()\n"
    "    render.set_render_target(render.RENDER_TARGET_DEFAULT)\n"
    "end\n";
    dmRender::HRenderScript render_script = dmRender::NewRenderScript(m_Context, LuaSourceFromString(script));
    dmRender::HRenderScriptInstance render_script_instance = dmRender::NewRenderScriptInstance(m_Context, render_script);

    ASSERT_EQ(dmRender::RENDER_SCRIPT_RESULT_OK, dmRender::DispatchRenderScriptInstance(render_script_instance));
    ASSERT_EQ(dmRender::RENDER_SCRIPT_RESULT_OK, dmRender::UpdateRenderScriptInstance(render_script_instance, 0.0f));

    dmArray<dmRender::Command>& commands = render_script_instance->m_CommandBuffer;
    ASSERT_EQ(7u, commands.Size());

    dmRender::Command* command = &commands[0];
    ASSERT_EQ(dmRender::COMMAND_TYPE_SET_RENDER_TARGET, command->m_Type);
    ASSERT_NE((void*)0, (void*)command->m_Operands[0]);
    ASSERT_EQ(dmGraphics::BUFFER_TYPE_DEPTH_BIT | dmGraphics::BUFFER_TYPE_STENCIL_BIT, (uint32_t)command->m_Operands[1]);

    command = &commands[1];
    ASSERT_EQ(dmRender::COMMAND_TYPE_SET_RENDER_TARGET, command->m_Type);
    ASSERT_NE((void*)0, (void*)command->m_Operands[0]);
    ASSERT_EQ(0, (uint32_t)command->m_Operands[1]);

    command = &commands[2];
    ASSERT_EQ(dmRender::COMMAND_TYPE_SET_RENDER_TARGET, command->m_Type);
    ASSERT_EQ((void*)0, (void*)command->m_Operands[0]);
    ASSERT_EQ(dmGraphics::BUFFER_TYPE_COLOR0_BIT, (uint32_t)command->m_Operands[1]);

    for(uint32_t i = 3; i < 7; ++i)
    {
        command = &commands[i];
        ASSERT_EQ(dmRender::COMMAND_TYPE_SET_RENDER_TARGET, command->m_Type);
        ASSERT_EQ((void*)0, (void*)command->m_Operands[0]);
        ASSERT_EQ(0, (uint32_t)command->m_Operands[1]);
    }

    dmRender::ParseCommands(m_Context, &commands[0], commands.Size());
    dmRender::DeleteRenderScriptInstance(render_script_instance);
    dmRender::DeleteRenderScript(m_Context, render_script);
}

TEST_F(dmRenderScriptTest, TestLuaRenderTargetDeprecated)
{
    // DEPRECATED functions tested, remove this test when render.enable/disable_render_target script functions are removed!
    const char* script =
    "function update(self)\n"
    "    local params_color = {\n"
    "        format = render.FORMAT_RGBA,\n"
    "        width = 1,\n"
    "        height = 2,\n"
    "        min_filter = render.FILTER_NEAREST,\n"
    "        mag_filter = render.FILTER_LINEAR,\n"
    "        u_wrap = render.WRAP_REPEAT,\n"
    "        v_wrap = render.WRAP_MIRRORED_REPEAT\n"
    "    }\n"
    "    local params_depth = {\n"
    "        format = render.FORMAT_DEPTH,\n"
    "        width = 1,\n"
    "        height = 2,\n"
    "        min_filter = render.FILTER_NEAREST,\n"
    "        mag_filter = render.FILTER_LINEAR,\n"
    "        u_wrap = render.WRAP_REPEAT,\n"
    "        v_wrap = render.WRAP_MIRRORED_REPEAT\n"
    "    }\n"
    "    local params_stencil = {\n"
    "        format = render.FORMAT_STENCIL,\n"
    "        width = 1,\n"
    "        height = 2,\n"
    "        min_filter = render.FILTER_NEAREST,\n"
    "        mag_filter = render.FILTER_LINEAR,\n"
    "        u_wrap = render.WRAP_REPEAT,\n"
    "        v_wrap = render.WRAP_MIRRORED_REPEAT\n"
    "    }\n"
    "    self.rt = render.render_target({[render.BUFFER_COLOR_BIT] = params_color, [render.BUFFER_DEPTH_BIT] = params_depth, [render.BUFFER_STENCIL_BIT] = params_stencil})\n"
    "    render.enable_render_target(self.rt)\n"
    "    render.disable_render_target(self.rt)\n"
    "    render.set_render_target_size(self.rt, 3, 4)\n"
            " local rt_w = render.get_render_target_width(self.rt, render.BUFFER_COLOR_BIT)\n"
            " assert(rt_w == 3)\n"
            " local rt_h = render.get_render_target_height(self.rt, render.BUFFER_COLOR_BIT)\n"
            " assert(rt_h == 4)\n"
            " local rt_w = render.get_render_target_width(self.rt, render.BUFFER_DEPTH_BIT)\n"
            " assert(rt_w == 3)\n"
            " local rt_h = render.get_render_target_height(self.rt, render.BUFFER_DEPTH_BIT)\n"
            " assert(rt_h == 4)\n"
            " local rt_w = render.get_render_target_width(self.rt, render.BUFFER_DEPTH_BIT)\n"
            " assert(rt_w == 3)\n"
            " local rt_h = render.get_render_target_height(self.rt, render.BUFFER_DEPTH_BIT)\n"
            " assert(rt_h == 4)\n"

    "    render.delete_render_target(self.rt)\n"
    "end\n";
    dmRender::HRenderScript render_script = dmRender::NewRenderScript(m_Context, LuaSourceFromString(script));
    dmRender::HRenderScriptInstance render_script_instance = dmRender::NewRenderScriptInstance(m_Context, render_script);

    ASSERT_EQ(dmRender::RENDER_SCRIPT_RESULT_OK, dmRender::DispatchRenderScriptInstance(render_script_instance));
    ASSERT_EQ(dmRender::RENDER_SCRIPT_RESULT_OK, dmRender::UpdateRenderScriptInstance(render_script_instance, 0.0f));

    dmArray<dmRender::Command>& commands = render_script_instance->m_CommandBuffer;
    ASSERT_EQ(2u, commands.Size());
    dmRender::Command* command = &commands[0];
    ASSERT_EQ(dmRender::COMMAND_TYPE_SET_RENDER_TARGET, command->m_Type);
    ASSERT_NE((void*)0, (void*)command->m_Operands[0]);
    ASSERT_EQ(0, (uint32_t)command->m_Operands[1]);
    command = &commands[1];
    ASSERT_EQ(dmRender::COMMAND_TYPE_SET_RENDER_TARGET, command->m_Type);
    ASSERT_EQ((void*)0, (void*)command->m_Operands[0]);
    ASSERT_EQ(0, (uint32_t)command->m_Operands[1]);

    dmRender::ParseCommands(m_Context, &commands[0], commands.Size());
    dmRender::DeleteRenderScriptInstance(render_script_instance);
    dmRender::DeleteRenderScript(m_Context, render_script);
}

TEST_F(dmRenderScriptTest, TestLuaRenderTargetRequiredKeys)
{
    const char* script =
    "function init(self)\n"
    "    local params_color = {\n"
    //"        format = render.FORMAT_RGBA,\n"
    "        width = 1,\n"
    "        height = 2,\n"
    "        min_filter = render.FILTER_NEAREST,\n"
    "        mag_filter = render.FILTER_LINEAR,\n"
    "        u_wrap = render.WRAP_REPEAT,\n"
    "        v_wrap = render.WRAP_MIRRORED_REPEAT\n"
    "    }\n"
    "    self.rt = render.render_target({[render.BUFFER_COLOR_BIT] = params_color})\n"
    "end\n";

    dmRender::HRenderScript render_script = dmRender::NewRenderScript(m_Context, LuaSourceFromString(script));
    dmRender::HRenderScriptInstance render_script_instance = dmRender::NewRenderScriptInstance(m_Context, render_script);
    ASSERT_EQ(dmRender::RENDER_SCRIPT_RESULT_FAILED, dmRender::InitRenderScriptInstance(render_script_instance));
    dmRender::DeleteRenderScriptInstance(render_script_instance);
    dmRender::DeleteRenderScript(m_Context, render_script);

    script =
    "function init(self)\n"
    "    local params_color = {\n"
    "        format = render.FORMAT_RGBA,\n"
    //"        width = 1,\n"
    "        height = 2,\n"
    "        min_filter = render.FILTER_NEAREST,\n"
    "        mag_filter = render.FILTER_LINEAR,\n"
    "        u_wrap = render.WRAP_REPEAT,\n"
    "        v_wrap = render.WRAP_MIRRORED_REPEAT\n"
    "    }\n"
    "    self.rt = render.render_target({[render.BUFFER_COLOR_BIT] = params_color})\n"
    "end\n";

    render_script = dmRender::NewRenderScript(m_Context, LuaSourceFromString(script));
    render_script_instance = dmRender::NewRenderScriptInstance(m_Context, render_script);
    ASSERT_EQ(dmRender::RENDER_SCRIPT_RESULT_FAILED, dmRender::InitRenderScriptInstance(render_script_instance));
    dmRender::DeleteRenderScriptInstance(render_script_instance);
    dmRender::DeleteRenderScript(m_Context, render_script);

    script =
    "function init(self)\n"
    "    local params_color = {\n"
    "        format = render.FORMAT_RGBA,\n"
    "        width = 1,\n"
    //"        height = 2,\n"
    "        min_filter = render.FILTER_NEAREST,\n"
    "        mag_filter = render.FILTER_LINEAR,\n"
    "        u_wrap = render.WRAP_REPEAT,\n"
    "        v_wrap = render.WRAP_MIRRORED_REPEAT\n"
    "    }\n"
    "    self.rt = render.render_target({[render.BUFFER_COLOR_BIT] = params_color})\n"
    "end\n";

    render_script = dmRender::NewRenderScript(m_Context, LuaSourceFromString(script));
    render_script_instance = dmRender::NewRenderScriptInstance(m_Context, render_script);
    ASSERT_EQ(dmRender::RENDER_SCRIPT_RESULT_FAILED, dmRender::InitRenderScriptInstance(render_script_instance));
    dmRender::DeleteRenderScriptInstance(render_script_instance);
    dmRender::DeleteRenderScript(m_Context, render_script);
}

TEST_F(dmRenderScriptTest, TestLuaClear)
{
    const char* script =
    "function update(self)\n"
    "    render.clear({[render.BUFFER_COLOR_BIT] = vmath.vector4(0, 0, 0, 0), [render.BUFFER_DEPTH_BIT] = 1})\n"
    "end\n";
    dmRender::HRenderScript render_script = dmRender::NewRenderScript(m_Context, LuaSourceFromString(script));
    dmRender::HRenderScriptInstance render_script_instance = dmRender::NewRenderScriptInstance(m_Context, render_script);

    ASSERT_EQ(dmRender::RENDER_SCRIPT_RESULT_OK, dmRender::DispatchRenderScriptInstance(render_script_instance));
    ASSERT_EQ(dmRender::RENDER_SCRIPT_RESULT_OK, dmRender::UpdateRenderScriptInstance(render_script_instance, 0.0f));

    dmArray<dmRender::Command>& commands = render_script_instance->m_CommandBuffer;
    ASSERT_EQ(1u, commands.Size());

    dmRender::Command* command = &commands[0];
    ASSERT_EQ(dmRender::COMMAND_TYPE_CLEAR, command->m_Type);
    uint32_t flags = command->m_Operands[0];
    ASSERT_NE(0u, flags & dmGraphics::BUFFER_TYPE_COLOR0_BIT);
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

TEST_F(dmRenderScriptTest, TestStencilBufferCleared)
{
    const char* script_no_stencil_clear =
    "function update(self)\n"
    "    render.clear({[render.BUFFER_COLOR_BIT] = vmath.vector4(0, 0, 0, 0), [render.BUFFER_DEPTH_BIT] = 1})\n"
    "end\n";
    dmRender::HRenderScript render_script = dmRender::NewRenderScript(m_Context, LuaSourceFromString(script_no_stencil_clear));
    dmRender::HRenderScriptInstance render_script_instance = dmRender::NewRenderScriptInstance(m_Context, render_script);
    ASSERT_EQ(0u, m_Context->m_StencilBufferCleared);
    ASSERT_EQ(dmRender::RENDER_SCRIPT_RESULT_OK, dmRender::DispatchRenderScriptInstance(render_script_instance));
    ASSERT_EQ(dmRender::RENDER_SCRIPT_RESULT_OK, dmRender::UpdateRenderScriptInstance(render_script_instance, 0.0f));
    ASSERT_EQ(0u, m_Context->m_StencilBufferCleared);
    dmRender::DeleteRenderScriptInstance(render_script_instance);
    dmRender::DeleteRenderScript(m_Context, render_script);


    const char* script_stencil_clear =
    "function update(self)\n"
    "    render.clear({[render.BUFFER_COLOR_BIT] = vmath.vector4(0, 0, 0, 0), [render.BUFFER_DEPTH_BIT] = 1, [render.BUFFER_STENCIL_BIT] = 0})\n"
    "end\n";
    render_script = dmRender::NewRenderScript(m_Context, LuaSourceFromString(script_stencil_clear));
    render_script_instance = dmRender::NewRenderScriptInstance(m_Context, render_script);
    ASSERT_EQ(0u, m_Context->m_StencilBufferCleared);
    ASSERT_EQ(dmRender::RENDER_SCRIPT_RESULT_OK, dmRender::DispatchRenderScriptInstance(render_script_instance));
    ASSERT_EQ(dmRender::RENDER_SCRIPT_RESULT_OK, dmRender::UpdateRenderScriptInstance(render_script_instance, 0.0f));
    ASSERT_EQ(1u, m_Context->m_StencilBufferCleared);
    dmRender::DeleteRenderScriptInstance(render_script_instance);
    dmRender::DeleteRenderScript(m_Context, render_script);
}

TEST_F(dmRenderScriptTest, TestLuaTransform)
{
    const char* script =
    "function init(self)\n"
    "    render.set_viewport(1, 2, 3, 4)\n"
    "    render.set_view(vmath.matrix4())\n"
    "    render.set_projection(vmath.matrix4())\n"
    "end\n";
    dmRender::HRenderScript render_script = dmRender::NewRenderScript(m_Context, LuaSourceFromString(script));
    dmRender::HRenderScriptInstance render_script_instance = dmRender::NewRenderScriptInstance(m_Context, render_script);

    ASSERT_EQ(dmRender::RENDER_SCRIPT_RESULT_OK, dmRender::InitRenderScriptInstance(render_script_instance));

    dmArray<dmRender::Command>& commands = render_script_instance->m_CommandBuffer;
    ASSERT_EQ(3u, commands.Size());

    dmRender::Command* command = &commands[0];
    ASSERT_EQ(dmRender::COMMAND_TYPE_SET_VIEWPORT, command->m_Type);
    ASSERT_EQ(1u, command->m_Operands[0]);
    ASSERT_EQ(2u, command->m_Operands[1]);
    ASSERT_EQ(3u, command->m_Operands[2]);
    ASSERT_EQ(4u, command->m_Operands[3]);

    Matrix4 identity = Matrix4::identity();

    command = &commands[1];
    ASSERT_EQ(dmRender::COMMAND_TYPE_SET_VIEW, command->m_Type);
    Matrix4* m = (Matrix4*)command->m_Operands[0];
    ASSERT_NE((void*)0, m);
    for (int i = 0; i < 4; ++i)
        for (int j = 0; j < 4; ++j)
            ASSERT_EQ(m->getElem(i, j), identity.getElem(i, j));

    command = &commands[2];
    ASSERT_EQ(dmRender::COMMAND_TYPE_SET_PROJECTION, command->m_Type);
    m = (Matrix4*)command->m_Operands[0];
    ASSERT_NE((void*)0, m);
    for (int i = 0; i < 4; ++i)
        for (int j = 0; j < 4; ++j)
            ASSERT_EQ(m->getElem(i, j), identity.getElem(i, j));

    dmRender::ParseCommands(m_Context, &commands[0], commands.Size());

    dmRender::DeleteRenderScriptInstance(render_script_instance);
    dmRender::DeleteRenderScript(m_Context, render_script);
}

TEST_F(dmRenderScriptTest, TestLuaDraw_StringPredicate)
{
    const char* script =
    "function init(self)\n"
    "    self.test_pred = render.predicate({\"one\", \"two\"})\n"
    "    render.draw(self.test_pred)\n"
    "    render.draw_debug3d()\n"
    "    render.draw_debug2d()\n"
    "end\n";
    dmRender::HRenderScript render_script = dmRender::NewRenderScript(m_Context, LuaSourceFromString(script));
    dmRender::HRenderScriptInstance render_script_instance = dmRender::NewRenderScriptInstance(m_Context, render_script);

    ASSERT_EQ(dmRender::RENDER_SCRIPT_RESULT_OK, dmRender::InitRenderScriptInstance(render_script_instance));

    dmArray<dmRender::Command>& commands = render_script_instance->m_CommandBuffer;
    ASSERT_EQ(2u, commands.Size());

    dmRender::Command* command = &commands[0];
    ASSERT_EQ(dmRender::COMMAND_TYPE_DRAW, command->m_Type);
    ASSERT_NE((void*)0, (void*)command->m_Operands[0]);

    command = &commands[1];
    ASSERT_EQ(dmRender::COMMAND_TYPE_DRAW_DEBUG3D, command->m_Type);

    dmRender::ParseCommands(m_Context, &commands[0], commands.Size());

    dmRender::DeleteRenderScriptInstance(render_script_instance);
    dmRender::DeleteRenderScript(m_Context, render_script);
}

TEST_F(dmRenderScriptTest, TestLuaDraw_HashPredicate)
{
    const char* script =
    "function init(self)\n"
    "    self.test_pred = render.predicate({hash(\"one\"), hash(\"two\")})\n"
    "    render.draw(self.test_pred)\n"
    "    render.draw_debug3d()\n"
    "    render.draw_debug2d()\n"
    "end\n";
    dmRender::HRenderScript render_script = dmRender::NewRenderScript(m_Context, LuaSourceFromString(script));
    dmRender::HRenderScriptInstance render_script_instance = dmRender::NewRenderScriptInstance(m_Context, render_script);

    ASSERT_EQ(dmRender::RENDER_SCRIPT_RESULT_OK, dmRender::InitRenderScriptInstance(render_script_instance));

    dmArray<dmRender::Command>& commands = render_script_instance->m_CommandBuffer;
    ASSERT_EQ(2u, commands.Size());

    dmRender::Command* command = &commands[0];
    ASSERT_EQ(dmRender::COMMAND_TYPE_DRAW, command->m_Type);
    ASSERT_NE((void*)0, (void*)command->m_Operands[0]);

    command = &commands[1];
    ASSERT_EQ(dmRender::COMMAND_TYPE_DRAW_DEBUG3D, command->m_Type);

    dmRender::ParseCommands(m_Context, &commands[0], commands.Size());

    dmRender::DeleteRenderScriptInstance(render_script_instance);
    dmRender::DeleteRenderScript(m_Context, render_script);
}

TEST_F(dmRenderScriptTest, TestLuaDraw_NoPredicate)
{
    const char* script =
    "function init(self)\n"
    "    render.draw()\n"
    "end\n";

    dmRender::HRenderScript render_script = dmRender::NewRenderScript(m_Context, LuaSourceFromString(script));
    dmRender::HRenderScriptInstance render_script_instance = dmRender::NewRenderScriptInstance(m_Context, render_script);

    ASSERT_EQ(dmRender::RENDER_SCRIPT_RESULT_FAILED, dmRender::InitRenderScriptInstance(render_script_instance));

    dmRender::DeleteRenderScriptInstance(render_script_instance);
    dmRender::DeleteRenderScript(m_Context, render_script);
}

TEST_F(dmRenderScriptTest, TestLuaWindowSize)
{
    const char* script =
    "function update(self)\n"
    "    assert(render.get_width() == 20)\n"
    "    assert(render.get_height() == 10)\n"
    "    assert(render.get_window_width() == 20)\n"
    "    assert(render.get_window_height() == 10)\n"
    "end\n";
    dmRender::HRenderScript render_script = dmRender::NewRenderScript(m_Context, LuaSourceFromString(script));
    dmRender::HRenderScriptInstance render_script_instance = dmRender::NewRenderScriptInstance(m_Context, render_script);

    ASSERT_EQ(dmRender::RENDER_SCRIPT_RESULT_OK, dmRender::DispatchRenderScriptInstance(render_script_instance));
    ASSERT_EQ(dmRender::RENDER_SCRIPT_RESULT_OK, dmRender::UpdateRenderScriptInstance(render_script_instance, 0.0f));

    dmRender::DeleteRenderScriptInstance(render_script_instance);
    dmRender::DeleteRenderScript(m_Context, render_script);
}

void TestDispatchCallback(dmMessage::Message *message, void* user_ptr)
{
    if (message->m_Id == dmHashString64("test_message"))
    {
        *((uint32_t*)user_ptr) = 1;
    }
}

TEST_F(dmRenderScriptTest, TestPost)
{
    const char* script =
    "function init(self)\n"
    "    msg.post(\"test_socket:\", \"test_message\")\n"
    "end\n";

    dmRender::HRenderScript render_script = dmRender::NewRenderScript(m_Context, LuaSourceFromString(script));
    dmRender::HRenderScriptInstance render_script_instance = dmRender::NewRenderScriptInstance(m_Context, render_script);

    dmMessage::HSocket test_socket;
    ASSERT_EQ(dmMessage::RESULT_OK, dmMessage::NewSocket("test_socket", &test_socket));

    ASSERT_EQ(dmRender::RENDER_SCRIPT_RESULT_OK, dmRender::InitRenderScriptInstance(render_script_instance));

    uint32_t test_value = 0;
    ASSERT_EQ(1u, dmMessage::Dispatch(test_socket, TestDispatchCallback, (void*)&test_value));
    ASSERT_EQ(1u, test_value);

    ASSERT_EQ(dmMessage::RESULT_OK, dmMessage::DeleteSocket(test_socket));

    dmRender::DeleteRenderScriptInstance(render_script_instance);
    dmRender::DeleteRenderScript(m_Context, render_script);
}

TEST_F(dmRenderScriptTest, TestPostToSelf)
{
    const char* script =
        "function init(self)\n"
        "    msg.post(\".\", \"test_message\", {test_value = 1})\n"
        "end\n"
        "function update(self, dt)\n"
        "    assert(self.test_value == 1, \"invalid test value\")\n"
        "end\n"
        "function on_message(self, message_id, message)\n"
        "    if (message_id == hash(\"test_message\")) then\n"
        "        self.test_value = message.test_value\n"
        "    end\n"
        "end\n";

    dmRender::HRenderScript render_script = dmRender::NewRenderScript(m_Context, LuaSourceFromString(script));
    dmRender::HRenderScriptInstance render_script_instance = dmRender::NewRenderScriptInstance(m_Context, render_script);

    ASSERT_EQ(dmRender::RENDER_SCRIPT_RESULT_OK, dmRender::InitRenderScriptInstance(render_script_instance));

    ASSERT_EQ(dmRender::RENDER_SCRIPT_RESULT_OK, dmRender::DispatchRenderScriptInstance(render_script_instance));
    ASSERT_EQ(dmRender::RENDER_SCRIPT_RESULT_OK, dmRender::UpdateRenderScriptInstance(render_script_instance, 0.0f));

    dmRender::DeleteRenderScriptInstance(render_script_instance);
    dmRender::DeleteRenderScript(m_Context, render_script);
}

TEST_F(dmRenderScriptTest, TestDrawText)
{
    const char* script =
        "function update(self, dt)\n"
        "    msg.post(\".\", \"draw_text\", {position = vmath.vector3(0, 0, 0), text = \"Hello world!\"})\n"
        "end\n";

    dmRender::HRenderScript render_script = dmRender::NewRenderScript(m_Context, LuaSourceFromString(script));
    dmRender::HRenderScriptInstance render_script_instance = dmRender::NewRenderScriptInstance(m_Context, render_script);

    ASSERT_EQ(0u, m_Context->m_TextContext.m_TextEntries.Size());

    ASSERT_EQ(dmRender::RENDER_SCRIPT_RESULT_OK, dmRender::InitRenderScriptInstance(render_script_instance));

    // We do three updates here,
    // First update: A "draw_text" message is sent
    ASSERT_EQ(dmRender::RENDER_SCRIPT_RESULT_OK, dmRender::DispatchRenderScriptInstance(render_script_instance));
    ASSERT_EQ(dmRender::RENDER_SCRIPT_RESULT_OK, dmRender::UpdateRenderScriptInstance(render_script_instance, 0.0f));
    dmRender::FlushTexts(m_Context, 0, 0, true);

    // Second update: "draw_text" is processed, but no glyphs are in font cache,
    //                they are marked as missing and uploaded. A new "draw_text" also is sent.
    ASSERT_EQ(dmRender::RENDER_SCRIPT_RESULT_OK, dmRender::DispatchRenderScriptInstance(render_script_instance));
    ASSERT_EQ(dmRender::RENDER_SCRIPT_RESULT_OK, dmRender::UpdateRenderScriptInstance(render_script_instance, 0.0f));
    dmRender::FlushTexts(m_Context, 0, 0, true);

    // Third update: The second "draw_text" is processed, this time the glyphs are uploaded
    //               and the text is drawn.
    ASSERT_EQ(dmRender::RENDER_SCRIPT_RESULT_OK, dmRender::DispatchRenderScriptInstance(render_script_instance));
    ASSERT_EQ(dmRender::RENDER_SCRIPT_RESULT_OK, dmRender::UpdateRenderScriptInstance(render_script_instance, 0.0f));
    dmRender::FlushTexts(m_Context, 0, 0, true);

    ASSERT_NE(0u, m_Context->m_TextContext.m_TextEntries.Size());

    dmRender::DeleteRenderScriptInstance(render_script_instance);
    dmRender::DeleteRenderScript(m_Context, render_script);
}

#define REF_VALUE "__ref_value"

int TestRef(lua_State* L)
{
    lua_getglobal(L, REF_VALUE);
    int* ref = (int*)lua_touserdata(L, -1);
    dmScript::GetInstance(L);
    *ref = dmScript::Ref(L, LUA_REGISTRYINDEX);
    lua_pop(L, 1);
    return 0;
}

TEST_F(dmRenderScriptTest, TestInstanceCallback)
{
    lua_State* L = m_Context->m_RenderScriptContext.m_LuaState;

    lua_register(L, "test_ref", TestRef);

    int ref = LUA_NOREF;

    lua_pushlightuserdata(L, &ref);
    lua_setglobal(L, REF_VALUE);

    const char* script =
        "function init(self)\n"
        "    test_ref()\n"
        "end\n";

    dmRender::HRenderScript render_script = dmRender::NewRenderScript(m_Context, LuaSourceFromString(script));
    dmRender::HRenderScriptInstance render_script_instance = dmRender::NewRenderScriptInstance(m_Context, render_script);

    ASSERT_EQ(dmRender::RENDER_SCRIPT_RESULT_OK, dmRender::InitRenderScriptInstance(render_script_instance));

    ASSERT_NE(ref, LUA_NOREF);
    lua_rawgeti(L, LUA_REGISTRYINDEX, ref);
    dmScript::SetInstance(L);
    ASSERT_TRUE(dmScript::IsInstanceValid(L));

    dmRender::DeleteRenderScriptInstance(render_script_instance);
    dmRender::DeleteRenderScript(m_Context, render_script);

    lua_rawgeti(L, LUA_REGISTRYINDEX, ref);
    dmScript::SetInstance(L);
    ASSERT_FALSE(dmScript::IsInstanceValid(L));
}

#undef REF_VALUE

TEST_F(dmRenderScriptTest, TestURL)
{
    const char* script =
    "local g_def_url = msg.url()\n"
    "local g_url = msg.url(\"@render:\")\n"
    "function init(self)\n"
    "    assert(g_def_url == g_url)\n"
    "    local url = msg.url(\"@render:\")\n"
    "    assert(msg.url() == url)\n"
    "end\n";
    dmRender::HRenderScript render_script = dmRender::NewRenderScript(m_Context, LuaSourceFromString(script));
    dmRender::HRenderScriptInstance render_script_instance = dmRender::NewRenderScriptInstance(m_Context, render_script);

    ASSERT_EQ(dmRender::RENDER_SCRIPT_RESULT_OK, dmRender::InitRenderScriptInstance(render_script_instance));

    dmRender::DeleteRenderScriptInstance(render_script_instance);
    dmRender::DeleteRenderScript(m_Context, render_script);
}

TEST_F(dmRenderScriptTest, TestInstanceContext)
{
    lua_State* L = m_Context->m_RenderScriptContext.m_LuaState;

    const char* script =
        "";

    dmRender::HRenderScript render_script = dmRender::NewRenderScript(m_Context, LuaSourceFromString(script));
    dmRender::HRenderScriptInstance render_script_instance = dmRender::NewRenderScriptInstance(m_Context, render_script);
    lua_rawgeti(L, LUA_REGISTRYINDEX, render_script_instance->m_InstanceReference);
    dmScript::SetInstance(L);
    ASSERT_TRUE(dmScript::IsInstanceValid(L));

    lua_pushstring(L, "__my_context_value");
    lua_pushnumber(L, 81233);
    ASSERT_TRUE(dmScript::SetInstanceContextValue(L));

    lua_pushstring(L, "__my_context_value");
    dmScript::GetInstanceContextValue(L);
    ASSERT_EQ(81233, lua_tonumber(L, -1));
    lua_pop(L, 1);

    dmRender::DeleteRenderScriptInstance(render_script_instance);
    dmRender::DeleteRenderScript(m_Context, render_script);

    lua_pushnil(L);
    dmScript::SetInstance(L);
    ASSERT_FALSE(dmScript::IsInstanceValid(L));
}

TEST_F(dmRenderScriptTest, DeltaTime)
{
    const char* script = "function update(self, dt)\n"
                    "assert (dt == 1122)\n"
                    "end\n";

    dmRender::HRenderScript render_script = dmRender::NewRenderScript(m_Context, LuaSourceFromString(script));
    dmRender::HRenderScriptInstance render_script_instance = dmRender::NewRenderScriptInstance(m_Context, render_script);

    ASSERT_EQ(dmRender::RENDER_SCRIPT_RESULT_OK, dmRender::UpdateRenderScriptInstance(render_script_instance, 1122));

    dmRender::DeleteRenderScriptInstance(render_script_instance);
    dmRender::DeleteRenderScript(m_Context, render_script);
}

TEST_F(dmRenderScriptTest, TestLuaConstantBuffers_Baseline)
{
    const char* script =
    "local function assert_vec4_equal(a,b)\n"
    "    assert(a.x == b.x and a.y == b.y and a.z == b.z and a.w == b.w)\n"
    "end\n"
    "local function assert_mat4_equal(a,b)\n"
    "    assert(a.m00 == b.m00 and a.m01 == b.m01 and a.m02 == b.m02 and a.m03 == b.m03 and\n"
    "        a.m10 == b.m10 and a.m11 == b.m11 and a.m12 == b.m12 and a.m13 == b.m13 and\n"
    "        a.m20 == b.m20 and a.m21 == b.m21 and a.m22 == b.m22 and a.m23 == b.m23 and\n"
    "        a.m30 == b.m30 and a.m31 == b.m31 and a.m32 == b.m32 and a.m33 == b.m33)\n"
    "end\n"
    "function init(self)\n"
    "    local cb = render.constant_buffer()\n"
    "    cb.vec4 = vmath.vector4(1,2,3,4)\n"
    "    cb.mat4 = vmath.matrix4_translation(vmath.vector3(1,2,3))\n"
    "    assert_vec4_equal(vmath.vector4(1,2,3,4), cb.vec4)\n"
    "    assert_mat4_equal(vmath.matrix4_translation(vmath.vector3(1,2,3)), cb.mat4)\n"
    "    cb.vec4_array    = { vmath.vector4(1,2,3,4), vmath.vector4(4,3,2,1) }\n"
    "    cb.vec4_array[8] = vmath.vector4(-1, -2, -3, -4)\n"
    "    cb.mat4_array    = { vmath.matrix4_translation(vmath.vector3(1,2,3)), vmath.matrix4_translation(vmath.vector3(3,2,1))}\n"
    "    cb.mat4_array[3] = vmath.matrix4_translation(vmath.vector3(-1,-2,-3))\n"
    "    assert_vec4_equal(vmath.vector4(1,2,3,4),        cb.vec4_array[1])\n"
    "    assert_vec4_equal(vmath.vector4(4,3,2,1),        cb.vec4_array[2])\n"
    "    assert_vec4_equal(vmath.vector4(0,0,0,0),        cb.vec4_array[3])\n"
    "    assert_vec4_equal(vmath.vector4(-1, -2, -3, -4), cb.vec4_array[8])\n"
    "    assert_mat4_equal(vmath.matrix4_translation(vmath.vector3(1,2,3)),    cb.mat4_array[1])\n"
    "    assert_mat4_equal(vmath.matrix4_translation(vmath.vector3(3,2,1)),    cb.mat4_array[2])\n"
    "    assert_mat4_equal(vmath.matrix4_translation(vmath.vector3(-1,-2,-3)), cb.mat4_array[3])\n"
    "end\n";

    dmRender::HRenderScript render_script = dmRender::NewRenderScript(m_Context, LuaSourceFromString(script));
    dmRender::HRenderScriptInstance render_script_instance = dmRender::NewRenderScriptInstance(m_Context, render_script);

    ASSERT_EQ(dmRender::RENDER_SCRIPT_RESULT_OK, dmRender::InitRenderScriptInstance(render_script_instance));

    dmRender::DeleteRenderScriptInstance(render_script_instance);
    dmRender::DeleteRenderScript(m_Context, render_script);
}

TEST_F(dmRenderScriptTest, TestLuaConstantBuffers_InvalidUsage)
{
    const char* script = 
        "function init(self)\n"
        "    self.cb = render.constant_buffer()\n"
        "    self.test = 0\n"
        "end\n"
        "function update(self)\n"
        "    self.test = self.test + 1\n"
        "    if self.test == 1 then self.cb.invalid_key   = { [-1] = vmath.vector4() } end\n"
        "    if self.test == 2 then self.cb.invalid_key   = { [\"bad_key\"] = vmath.vector4() } end\n"
        "    if self.test == 3 then self.cb.type_mismatch = { vmath.vector4(), vmath.matrix4() } end\n"
        "end\n";

    dmRender::HRenderScript render_script = dmRender::NewRenderScript(m_Context, LuaSourceFromString(script));
    dmRender::HRenderScriptInstance render_script_instance = dmRender::NewRenderScriptInstance(m_Context, render_script);

    ASSERT_EQ(dmRender::RENDER_SCRIPT_RESULT_OK, dmRender::InitRenderScriptInstance(render_script_instance));
    ASSERT_EQ(dmRender::RENDER_SCRIPT_RESULT_FAILED, dmRender::UpdateRenderScriptInstance(render_script_instance, 0.0f));
    ASSERT_EQ(dmRender::RENDER_SCRIPT_RESULT_FAILED, dmRender::UpdateRenderScriptInstance(render_script_instance, 0.0f));
    ASSERT_EQ(dmRender::RENDER_SCRIPT_RESULT_FAILED, dmRender::UpdateRenderScriptInstance(render_script_instance, 0.0f));

    dmRender::DeleteRenderScriptInstance(render_script_instance);
    dmRender::DeleteRenderScript(m_Context, render_script);
}

int main(int argc, char **argv)
{
    dmDDF::RegisterAllTypes();
    jc_test_init(&argc, argv);
    return jc_test_run_all();
}
