// Copyright 2020-2025 The Defold Foundation
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

#include <testmain/testmain.h>
#include <dmsdk/dlib/vmath.h>
#include <dmsdk/dlib/dstrings.h>

#include "render/render.h"
#include "render/font_renderer.h"
#include "render/render_private.h"
#include "render/render_script.h"

#include "render/render_ddf.h"

#include "../../../graphics/src/null/graphics_null_private.h"
#include "../../../graphics/src/test/test_graphics_util.h"

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

static dmRender::FontGlyph* GetGlyph(uint32_t utf8, void* user_ctx)
{
    dmRender::FontGlyph* glyphs = (dmRender::FontGlyph*)user_ctx;
    return &glyphs[utf8];
}

static void* GetGlyphData(uint32_t codepoint, void* user_ctx, uint32_t* out_size, uint32_t* out_compression, uint32_t* out_width, uint32_t* out_height, uint32_t* out_channels)
{
    return 0;
}

class dmRenderScriptTest : public jc_test_base_class
{
protected:
    dmPlatform::HWindow          m_Window;
    dmScript::HContext           m_ScriptContext;
    dmRender::HRenderContext     m_Context;
    dmGraphics::HContext         m_GraphicsContext;
    dmRender::HFontMap           m_SystemFontMap;
    dmRender::HMaterial          m_FontMaterial;
    dmRender::FontGlyph          m_Glyphs[128];

    dmGraphics::HProgram m_FontProgram;
    dmGraphics::HProgram m_ComputeProgram;

    dmRender::HComputeProgram m_Compute;

    void SetUp() override
    {
        dmGraphics::InstallAdapter();

        dmPlatform::WindowParams win_params = {};
        win_params.m_Width = 20;
        win_params.m_Height = 10;
        win_params.m_ContextAlphabits = 8;

        m_Window = dmPlatform::NewWindow();
        dmPlatform::OpenWindow(m_Window, win_params);

        dmGraphics::ContextParams graphics_context_params;
        graphics_context_params.m_Width = 20;
        graphics_context_params.m_Height = 10;
        graphics_context_params.m_Window = m_Window;

        m_GraphicsContext = dmGraphics::NewContext(graphics_context_params);

        dmScript::ContextParams script_context_params = {};
        script_context_params.m_GraphicsContext = m_GraphicsContext;
        m_ScriptContext = dmScript::NewContext(script_context_params);
        dmScript::Initialize(m_ScriptContext);

        dmRender::FontMapParams font_map_params;
        font_map_params.m_CacheWidth = 128;
        font_map_params.m_CacheHeight = 128;
        font_map_params.m_CacheCellWidth = 8;
        font_map_params.m_CacheCellHeight = 8;
        font_map_params.m_GetGlyph = GetGlyph;
        font_map_params.m_GetGlyphData = GetGlyphData;

        m_SystemFontMap = dmRender::NewFontMap(m_Context, m_GraphicsContext, font_map_params);

        memset(m_Glyphs, 0, sizeof(m_Glyphs));
        for (uint32_t i = 0; i < DM_ARRAY_SIZE(m_Glyphs); ++i)
        {
            m_Glyphs[i].m_Width = 1;
            m_Glyphs[i].m_Character = i;
        }
        dmRender::SetFontMapUserData(m_SystemFontMap, m_Glyphs);

        dmRender::RenderContextParams params;
        params.m_ScriptContext = m_ScriptContext;
        params.m_SystemFontMap = m_SystemFontMap;
        params.m_MaxRenderTargets = 1;
        params.m_MaxInstances = 64;
        params.m_MaxCharacters = 32;
        params.m_MaxBatches = 128;
        m_Context = dmRender::NewRenderContext(m_GraphicsContext, params);

        dmGraphics::ShaderDescBuilder shader_desc_builder;
        shader_desc_builder.AddShader(dmGraphics::ShaderDesc::SHADER_TYPE_VERTEX, dmGraphics::ShaderDesc::LANGUAGE_GLSL_SM330, "foo", 3);
        shader_desc_builder.AddShader(dmGraphics::ShaderDesc::SHADER_TYPE_FRAGMENT, dmGraphics::ShaderDesc::LANGUAGE_GLSL_SM330, "foo", 3);

        dmGraphics::ShaderDesc* shader_desc = shader_desc_builder.Get();
        m_FontProgram = dmGraphics::NewProgram(m_GraphicsContext, shader_desc, 0, 0);

        m_FontMaterial = dmRender::NewMaterial(m_Context, m_FontProgram);
        dmRender::SetFontMapMaterial(m_SystemFontMap, m_FontMaterial);

        const char* compute_program_src =
            "#version 430\n"
            "uniform vec4 tint;\n"
            "uniform sampler2D texture_sampler\n";

        dmGraphics::ShaderDescBuilder compute_desc_builder;
        compute_desc_builder.AddShader(dmGraphics::ShaderDesc::SHADER_TYPE_COMPUTE, dmGraphics::ShaderDesc::LANGUAGE_GLSL_SM430, compute_program_src, strlen(compute_program_src));
        m_ComputeProgram = dmGraphics::NewProgram(m_GraphicsContext, compute_desc_builder.Get(), 0, 0);
        m_Compute = dmRender::NewComputeProgram(m_Context, m_ComputeProgram);
    }

    void TearDown() override
    {
        dmGraphics::DeleteProgram(m_GraphicsContext, m_FontProgram);
        dmGraphics::DeleteProgram(m_GraphicsContext, m_ComputeProgram);
        dmRender::DeleteMaterial(m_Context, m_FontMaterial);
        dmRender::DeleteComputeProgram(m_Context, m_Compute);

        dmGraphics::CloseWindow(m_GraphicsContext);
        dmRender::DeleteRenderContext(m_Context, 0);
        dmRender::DeleteFontMap(m_SystemFontMap);
        dmGraphics::DeleteContext(m_GraphicsContext);
        dmPlatform::CloseWindow(m_Window);
        dmPlatform::DeleteWindow(m_Window);

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

    dmRender::HMaterial material = dmRender::NewMaterial(m_Context, m_FontProgram);

    ASSERT_EQ(dmRender::RENDER_SCRIPT_RESULT_FAILED, dmRender::InitRenderScriptInstance(render_script_instance));
    dmRender::AddRenderScriptInstanceRenderResource(render_script_instance, "test_material", (uint64_t) material, dmRender::RENDER_RESOURCE_TYPE_MATERIAL);
    ASSERT_EQ(dmRender::RENDER_SCRIPT_RESULT_OK, dmRender::InitRenderScriptInstance(render_script_instance));

    dmArray<dmRender::Command>& commands = render_script_instance->m_CommandBuffer;
    ASSERT_EQ(2u, commands.Size());

    dmRender::Command* command = &commands[0];
    ASSERT_EQ(dmRender::COMMAND_TYPE_ENABLE_MATERIAL, command->m_Type);
    ASSERT_NE((void*)0, (void*)command->m_Operands[0]);

    command = &commands[1];
    ASSERT_EQ(dmRender::COMMAND_TYPE_DISABLE_MATERIAL, command->m_Type);

    dmRender::ClearRenderScriptInstanceRenderResources(render_script_instance);
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
    "    render.enable_state(graphics.STATE_BLEND)\n"
    "    render.disable_state(graphics.STATE_BLEND)\n"
    "    render.set_blend_func(graphics.BLEND_FACTOR_ONE, graphics.BLEND_FACTOR_SRC_COLOR)\n"
    "    render.set_color_mask(true, true, true, true)\n"
    "    render.set_depth_mask(true)\n"
    "    render.set_depth_func(graphics.COMPARE_FUNC_GEQUAL)\n"
    "    render.set_stencil_mask(1)\n"
    "    render.set_stencil_func(graphics.COMPARE_FUNC_ALWAYS, 1, 2)\n"
    "    render.set_stencil_op(graphics.STENCIL_OP_REPLACE, graphics.STENCIL_OP_KEEP, graphics.STENCIL_OP_INVERT)\n"
    "    render.set_cull_face(graphics.FACE_TYPE_BACK)\n"
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
    "        format = graphics.TEXTURE_FORMAT_RGBA,\n"
    "        width = 1000000000,\n"
    "        height = 2,\n"
    "        min_filter = graphics.TEXTURE_FILTER_NEAREST,\n"
    "        mag_filter = graphics.TEXTURE_FILTER_LINEAR,\n"
    "        u_wrap = graphics.TEXTURE_WRAP_REPEAT,\n"
    "        v_wrap = graphics.TEXTURE_WRAP_MIRRORED_REPEAT\n"
    "    }\n"
    "    self.rt = render.render_target({[graphics.BUFFER_TYPE_COLOR0_BIT] = params_color})\n"
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
    "        format = graphics.TEXTURE_FORMAT_RGBA,\n"
    "        width = 1,\n"
    "        height = 2,\n"
    "        min_filter = graphics.TEXTURE_FILTER_NEAREST,\n"
    "        mag_filter = graphics.TEXTURE_FILTER_LINEAR,\n"
    "        u_wrap = graphics.TEXTURE_WRAP_REPEAT,\n"
    "        v_wrap = graphics.TEXTURE_WRAP_MIRRORED_REPEAT\n"
    "    }\n"
    "    local params_depth = {\n"
    "        format = graphics.TEXTURE_FORMAT_DEPTH,\n"
    "        width = 1,\n"
    "        height = 2,\n"
    "        min_filter = graphics.TEXTURE_FILTER_NEAREST,\n"
    "        mag_filter = graphics.TEXTURE_FILTER_LINEAR,\n"
    "        u_wrap = graphics.TEXTURE_WRAP_REPEAT,\n"
    "        v_wrap = graphics.TEXTURE_WRAP_MIRRORED_REPEAT\n"
    "    }\n"
    "    local params_stencil = {\n"
    "        format = graphics.TEXTURE_FORMAT_STENCIL,\n"
    "        width = 1,\n"
    "        height = 2,\n"
    "        min_filter = graphics.TEXTURE_FILTER_NEAREST,\n"
    "        mag_filter = graphics.TEXTURE_FILTER_LINEAR,\n"
    "        u_wrap = graphics.TEXTURE_WRAP_REPEAT,\n"
    "        v_wrap = graphics.TEXTURE_WRAP_MIRRORED_REPEAT\n"
    "    }\n"
    "    self.rt = render.render_target({[graphics.BUFFER_TYPE_COLOR0_BIT] = params_color, [graphics.BUFFER_TYPE_DEPTH_BIT] = params_depth, [graphics.BUFFER_TYPE_STENCIL_BIT] = params_stencil})\n"
    "    render.set_render_target(self.rt, {transient = {graphics.BUFFER_TYPE_DEPTH_BIT, graphics.BUFFER_TYPE_STENCIL_BIT}})\n"
    "    render.set_render_target(self.rt)\n"
    "    render.set_render_target_size(self.rt, 3, 4)\n"
            " local rt_w = render.get_render_target_width(self.rt, graphics.BUFFER_TYPE_COLOR0_BIT)\n"
            " assert(rt_w == 3)\n"
            " local rt_h = render.get_render_target_height(self.rt, graphics.BUFFER_TYPE_COLOR0_BIT)\n"
            " assert(rt_h == 4)\n"
            " local rt_w = render.get_render_target_width(self.rt, graphics.BUFFER_TYPE_DEPTH_BIT)\n"
            " assert(rt_w == 3)\n"
            " local rt_h = render.get_render_target_height(self.rt, graphics.BUFFER_TYPE_DEPTH_BIT)\n"
            " assert(rt_h == 4)\n"
            " local rt_w = render.get_render_target_width(self.rt, graphics.BUFFER_TYPE_DEPTH_BIT)\n"
            " assert(rt_w == 3)\n"
            " local rt_h = render.get_render_target_height(self.rt, graphics.BUFFER_TYPE_DEPTH_BIT)\n"
            " assert(rt_h == 4)\n"
    "    render.set_render_target(nil, {transient = {graphics.BUFFER_TYPE_COLOR0_BIT}})\n"
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
    "        format = graphics.TEXTURE_FORMAT_RGBA,\n"
    "        width = 1,\n"
    "        height = 2,\n"
    "        min_filter = graphics.TEXTURE_FILTER_NEAREST,\n"
    "        mag_filter = graphics.TEXTURE_FILTER_LINEAR,\n"
    "        u_wrap = graphics.TEXTURE_WRAP_REPEAT,\n"
    "        v_wrap = graphics.TEXTURE_WRAP_MIRRORED_REPEAT\n"
    "    }\n"
    "    local params_depth = {\n"
    "        format = graphics.TEXTURE_FORMAT_DEPTH,\n"
    "        width = 1,\n"
    "        height = 2,\n"
    "        min_filter = graphics.TEXTURE_FILTER_NEAREST,\n"
    "        mag_filter = graphics.TEXTURE_FILTER_LINEAR,\n"
    "        u_wrap = graphics.TEXTURE_WRAP_REPEAT,\n"
    "        v_wrap = graphics.TEXTURE_WRAP_MIRRORED_REPEAT\n"
    "    }\n"
    "    local params_stencil = {\n"
    "        format = graphics.TEXTURE_FORMAT_STENCIL,\n"
    "        width = 1,\n"
    "        height = 2,\n"
    "        min_filter = graphics.TEXTURE_FILTER_NEAREST,\n"
    "        mag_filter = graphics.TEXTURE_FILTER_LINEAR,\n"
    "        u_wrap = graphics.TEXTURE_WRAP_REPEAT,\n"
    "        v_wrap = graphics.TEXTURE_WRAP_MIRRORED_REPEAT\n"
    "    }\n"
    "    self.rt = render.render_target({[graphics.BUFFER_TYPE_COLOR0_BIT] = params_color, [graphics.BUFFER_TYPE_DEPTH_BIT] = params_depth, [graphics.BUFFER_TYPE_STENCIL_BIT] = params_stencil})\n"
    "    render.enable_render_target(self.rt)\n"
    "    render.disable_render_target(self.rt)\n"
    "    render.set_render_target_size(self.rt, 3, 4)\n"
            " local rt_w = render.get_render_target_width(self.rt, graphics.BUFFER_TYPE_COLOR0_BIT)\n"
            " assert(rt_w == 3)\n"
            " local rt_h = render.get_render_target_height(self.rt, graphics.BUFFER_TYPE_COLOR0_BIT)\n"
            " assert(rt_h == 4)\n"
            " local rt_w = render.get_render_target_width(self.rt, graphics.BUFFER_TYPE_DEPTH_BIT)\n"
            " assert(rt_w == 3)\n"
            " local rt_h = render.get_render_target_height(self.rt, graphics.BUFFER_TYPE_DEPTH_BIT)\n"
            " assert(rt_h == 4)\n"
            " local rt_w = render.get_render_target_width(self.rt, graphics.BUFFER_TYPE_DEPTH_BIT)\n"
            " assert(rt_w == 3)\n"
            " local rt_h = render.get_render_target_height(self.rt, graphics.BUFFER_TYPE_DEPTH_BIT)\n"
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
    //"        format = graphics.TEXTURE_FORMAT_RGBA,\n"
    "        width = 1,\n"
    "        height = 2,\n"
    "        min_filter = graphics.TEXTURE_FILTER_NEAREST,\n"
    "        mag_filter = graphics.TEXTURE_FILTER_LINEAR,\n"
    "        u_wrap = graphics.TEXTURE_WRAP_REPEAT,\n"
    "        v_wrap = graphics.TEXTURE_WRAP_MIRRORED_REPEAT\n"
    "    }\n"
    "    self.rt = render.render_target({[graphics.BUFFER_TYPE_COLOR0_BIT] = params_color})\n"
    "end\n";

    dmRender::HRenderScript render_script = dmRender::NewRenderScript(m_Context, LuaSourceFromString(script));
    dmRender::HRenderScriptInstance render_script_instance = dmRender::NewRenderScriptInstance(m_Context, render_script);
    ASSERT_EQ(dmRender::RENDER_SCRIPT_RESULT_FAILED, dmRender::InitRenderScriptInstance(render_script_instance));
    dmRender::DeleteRenderScriptInstance(render_script_instance);
    dmRender::DeleteRenderScript(m_Context, render_script);

    script =
    "function init(self)\n"
    "    local params_color = {\n"
    "        format = graphics.TEXTURE_FORMAT_RGBA,\n"
    //"        width = 1,\n"
    "        height = 2,\n"
    "        min_filter = graphics.TEXTURE_FILTER_NEAREST,\n"
    "        mag_filter = graphics.TEXTURE_FILTER_LINEAR,\n"
    "        u_wrap = graphics.TEXTURE_WRAP_REPEAT,\n"
    "        v_wrap = graphics.TEXTURE_WRAP_MIRRORED_REPEAT\n"
    "    }\n"
    "    self.rt = render.render_target({[graphics.BUFFER_TYPE_COLOR0_BIT] = params_color})\n"
    "end\n";

    render_script = dmRender::NewRenderScript(m_Context, LuaSourceFromString(script));
    render_script_instance = dmRender::NewRenderScriptInstance(m_Context, render_script);
    ASSERT_EQ(dmRender::RENDER_SCRIPT_RESULT_FAILED, dmRender::InitRenderScriptInstance(render_script_instance));
    dmRender::DeleteRenderScriptInstance(render_script_instance);
    dmRender::DeleteRenderScript(m_Context, render_script);

    script =
    "function init(self)\n"
    "    local params_color = {\n"
    "        format = graphics.TEXTURE_FORMAT_RGBA,\n"
    "        width = 1,\n"
    //"        height = 2,\n"
    "        min_filter = graphics.TEXTURE_FILTER_NEAREST,\n"
    "        mag_filter = graphics.TEXTURE_FILTER_LINEAR,\n"
    "        u_wrap = graphics.TEXTURE_WRAP_REPEAT,\n"
    "        v_wrap = graphics.TEXTURE_WRAP_MIRRORED_REPEAT\n"
    "    }\n"
    "    self.rt = render.render_target({[graphics.BUFFER_TYPE_COLOR0_BIT] = params_color})\n"
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
    "    render.clear({[graphics.BUFFER_TYPE_COLOR0_BIT] = vmath.vector4(0, 0, 0, 0), [graphics.BUFFER_TYPE_DEPTH_BIT] = 1})\n"
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
    "    render.clear({[graphics.BUFFER_TYPE_COLOR0_BIT] = vmath.vector4(0, 0, 0, 0), [graphics.BUFFER_TYPE_DEPTH_BIT] = 1})\n"
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
    "    render.clear({[graphics.BUFFER_TYPE_COLOR0_BIT] = vmath.vector4(0, 0, 0, 0), [graphics.BUFFER_TYPE_DEPTH_BIT] = 1, [graphics.BUFFER_TYPE_STENCIL_BIT] = 0})\n"
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

TEST_F(dmRenderScriptTest, TestLuaConstantBuffers_NestedStructs)
{
    /////////////////////////////
    // MATERIAL
    /////////////////////////////
    const char* shader_src = "foo";

    dmGraphics::ShaderDescBuilder shader_desc_builder;
    shader_desc_builder.AddShader(dmGraphics::ShaderDesc::SHADER_TYPE_VERTEX, dmGraphics::ShaderDesc::LANGUAGE_GLSL_SM330, shader_src, strlen(shader_src));
    shader_desc_builder.AddShader(dmGraphics::ShaderDesc::SHADER_TYPE_FRAGMENT, dmGraphics::ShaderDesc::LANGUAGE_GLSL_SM330, shader_src, strlen(shader_src));

    dmGraphics::ShaderDesc::ResourceMember ubo_members[2] = {};
    ubo_members[0].m_Name                     = "color";
    ubo_members[0].m_NameHash                 = dmHashString64(ubo_members[0].m_Name);
    ubo_members[0].m_Type.m_Type.m_ShaderType = dmGraphics::ShaderDesc::SHADER_TYPE_VEC4;

    ubo_members[1].m_Name                     = "nested";
    ubo_members[1].m_NameHash                 = dmHashString64(ubo_members[0].m_Name);
    ubo_members[1].m_Offset                   = 4 * sizeof(float);
    ubo_members[1].m_Type.m_UseTypeIndex      = 1;
    ubo_members[1].m_Type.m_Type.m_TypeIndex  = 1;

    shader_desc_builder.AddTypeMemberWithMembers("ubo", ubo_members, 2);
    shader_desc_builder.AddTypeMember("color", dmGraphics::ShaderDesc::SHADER_TYPE_VEC4);

    shader_desc_builder.AddUniform("ubo", 0, 0);

    dmGraphics::HProgram program = dmGraphics::NewProgram(m_GraphicsContext, shader_desc_builder.Get(), 0, 0);
    dmRender::HMaterial material = dmRender::NewMaterial(m_Context, program);

    dmRender::HConstant constant;
    ASSERT_TRUE(dmRender::GetMaterialProgramConstant(material, dmHashString64("color"), constant));
    ASSERT_TRUE(dmRender::GetMaterialProgramConstant(material, dmHashString64("nested.color"), constant));

    dmRender::DeleteMaterial(m_Context, material);
    dmGraphics::DeleteProgram(m_GraphicsContext, program);
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

TEST_F(dmRenderScriptTest, TestLuaConstantBuffers_ReuseSameTableKey)
{
    const char* script =
    "function init(self)\n"
    "    self.cb = render.constant_buffer()\n"
    "end\n"
    "function update(self)\n"
    "    -- create a new userdata entry every time this function is called\n"
    "    self.cb.tbl = {}\n"
    "end\n";

    dmRender::HRenderScript render_script = dmRender::NewRenderScript(m_Context, LuaSourceFromString(script));
    dmRender::HRenderScriptInstance render_script_instance = dmRender::NewRenderScriptInstance(m_Context, render_script);

    ASSERT_EQ(dmRender::RENDER_SCRIPT_RESULT_OK, dmRender::InitRenderScriptInstance(render_script_instance));

    int refCountStart = dmScript::GetLuaRefCount();
    for (int i = 0; i < 10; ++i)
    {
        ASSERT_EQ(dmRender::RENDER_SCRIPT_RESULT_OK, dmRender::UpdateRenderScriptInstance(render_script_instance, 0.0f));
    }
    int refCountEnd = dmScript::GetLuaRefCount();
    ASSERT_EQ(refCountStart + 1, refCountEnd);

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

TEST_F(dmRenderScriptTest, TestAssetHandlesValidRenderTarget)
{
    const char* script =
        "function init(self)\n"
        "   self.my_rt = render.render_target({[graphics.BUFFER_TYPE_COLOR0_BIT] = { format = graphics.TEXTURE_FORMAT_RGBA, width = 128, height = 128 }})\n"
        "end\n"
        "function update(self)\n"
        "    render.enable_texture(0, self.my_rt)\n"
        "    render.set_render_target(self.my_rt)\n"
        "end\n";

    dmRender::HRenderScript render_script                  = dmRender::NewRenderScript(m_Context, LuaSourceFromString(script));
    dmRender::HRenderScriptInstance render_script_instance = dmRender::NewRenderScriptInstance(m_Context, render_script);

    ASSERT_EQ(dmRender::RENDER_SCRIPT_RESULT_OK, dmRender::InitRenderScriptInstance(render_script_instance));
    ASSERT_EQ(dmRender::RENDER_SCRIPT_RESULT_OK, dmRender::UpdateRenderScriptInstance(render_script_instance, 0.0f));

    dmRender::DeleteRenderScriptInstance(render_script_instance);
    dmRender::DeleteRenderScript(m_Context, render_script);
}

TEST_F(dmRenderScriptTest, TestAssetHandlesValidTexture)
{
    const uint32_t unit = 1;

    const char* script =
        "function init(self)\n"
        "   self.my_texture = %llu\n"
        "end\n"
        "function update(self)\n"
        "    render.enable_texture(%d, self.my_texture)\n"
        "end\n";

    dmGraphics::TextureCreationParams creation_params;
    creation_params.m_Width          = 16;
    creation_params.m_Height         = 16;
    creation_params.m_OriginalWidth  = 16;
    creation_params.m_OriginalHeight = 16;

    dmGraphics::HTexture texture = dmGraphics::NewTexture(m_GraphicsContext, creation_params);

    char buf[256];
    dmSnPrintf(buf, sizeof(buf), script, texture, unit);

    dmRender::HRenderScript render_script                  = dmRender::NewRenderScript(m_Context, LuaSourceFromString(buf));
    dmRender::HRenderScriptInstance render_script_instance = dmRender::NewRenderScriptInstance(m_Context, render_script);

    ASSERT_EQ(dmRender::RENDER_SCRIPT_RESULT_OK, dmRender::InitRenderScriptInstance(render_script_instance));
    ASSERT_EQ(dmRender::RENDER_SCRIPT_RESULT_OK, dmRender::UpdateRenderScriptInstance(render_script_instance, 0.0f));

    dmArray<dmRender::Command>& commands = render_script_instance->m_CommandBuffer;
    ASSERT_EQ(1u, commands.Size());

    dmRender::Command* command = &commands[0];
    ASSERT_EQ(dmRender::COMMAND_TYPE_ENABLE_TEXTURE, command->m_Type);
    ASSERT_EQ(0,       command->m_Operands[0]);
    ASSERT_EQ(unit,    command->m_Operands[1]);
    ASSERT_EQ(texture, command->m_Operands[2]);

    dmRender::ParseCommands(m_Context, &commands[0], commands.Size());
    ASSERT_EQ(m_Context->m_TextureBindTable[unit].m_Texture, texture);

    dmGraphics::DeleteTexture(m_Context, texture);

    dmRender::DeleteRenderScriptInstance(render_script_instance);
    dmRender::DeleteRenderScript(m_Context, render_script);
}

/*
TEST_F(dmRenderScriptTest, TestAssetHandlesInvalid)
{
    // Out-of-range handle should cause assert
    ASSERT_DEATH(dmGraphics::MakeAssetHandle(0, (dmGraphics::AssetType) -1), "");

    const char* script =
        "function init(self)\n"
        "   self.test = 0\n"
        "end\n"
        "function update(self)\n"
        "    self.test = self.test + 1\n"
        "    if self.test == 1 then render.enable_texture(0, %llu) end\n"
        "    if self.test == 2 then render.enable_texture(0, %llu) end\n"
        "    if self.test == 3 then render.enable_texture(0, %llu) end\n"
        "end\n";

    dmGraphics::TextureCreationParams creation_params;
    creation_params.m_Width          = 16;
    creation_params.m_Height         = 16;
    creation_params.m_OriginalWidth  = 16;
    creation_params.m_OriginalHeight = 16;

    // Invalid type and the asset doesn't exist
    dmGraphics::HTexture texture_invalid_0 = 0;

    // Valid type, but the asset doesn't exist
    dmGraphics::HTexture texture_invalid_1 = dmGraphics::MakeAssetHandle(0, dmGraphics::ASSET_TYPE_TEXTURE);

    // Invalid type, but the asset does exist
    dmGraphics::HTexture texture           = dmGraphics::NewTexture(m_GraphicsContext, creation_params);
    HOpaqueHandle texture_opaque_bits      = dmGraphics::GetOpaqueHandle((dmGraphics::HAssetHandle) texture);
    dmGraphics::HTexture texture_invalid_2 = ((uint64_t) -1) << 32 | texture_opaque_bits;

    char buf[512];
    dmSnPrintf(buf, sizeof(buf), script,
        texture_invalid_0,
        texture_invalid_1,
        texture_invalid_2);

    dmRender::HRenderScript render_script                  = dmRender::NewRenderScript(m_Context, LuaSourceFromString(buf));
    dmRender::HRenderScriptInstance render_script_instance = dmRender::NewRenderScriptInstance(m_Context, render_script);

    ASSERT_EQ(dmRender::RENDER_SCRIPT_RESULT_OK, dmRender::InitRenderScriptInstance(render_script_instance));
    ASSERT_EQ(dmRender::RENDER_SCRIPT_RESULT_FAILED, dmRender::UpdateRenderScriptInstance(render_script_instance, 0.0f));
    ASSERT_EQ(dmRender::RENDER_SCRIPT_RESULT_FAILED, dmRender::UpdateRenderScriptInstance(render_script_instance, 0.0f));

    // Third update will cause an assert due to the handle being out-of-range
    ASSERT_DEATH(dmRender::UpdateRenderScriptInstance(render_script_instance, 0.0f), "");

    dmRender::DeleteRenderScriptInstance(render_script_instance);
    dmRender::DeleteRenderScript(m_Context, render_script);
}
*/

TEST_F(dmRenderScriptTest, TestRenderTargetResource)
{
    const char* script =
        "function assert_rt_buffer_size(rt, buffer, exp_w, exp_h)\n"
        "   assert(exp_w == render.get_render_target_width(rt, buffer))\n"
        "   assert(exp_h == render.get_render_target_height(rt, buffer))\n"
        "end\n"
        "function assert_rt_size(rt, ds, ss, c0s, c1s, c2s, c3s)\n"
        "   assert_rt_buffer_size(rt, graphics.BUFFER_TYPE_DEPTH_BIT, ds, ds)\n"
        "   assert_rt_buffer_size(rt, graphics.BUFFER_TYPE_STENCIL_BIT, ss, ss)\n"
        "   assert_rt_buffer_size(rt, graphics.BUFFER_TYPE_COLOR0_BIT, c0s, c0s)\n"
        "   assert_rt_buffer_size(rt, graphics.BUFFER_TYPE_COLOR1_BIT, c1s, c1s)\n"
        "   assert_rt_buffer_size(rt, graphics.BUFFER_TYPE_COLOR2_BIT, c2s, c2s)\n"
        "   assert_rt_buffer_size(rt, graphics.BUFFER_TYPE_COLOR3_BIT, c3s, c3s)\n"
        "end\n"
        "function init(self)\n"
        "   assert_rt_size('valid_rt', 8, 16, 32, 64, 96, 128)\n"
        "   render.set_render_target_size('valid_rt', 512, 512)\n"
        "   assert_rt_size('valid_rt', 512, 512, 512, 512, 512, 512)\n"
        "end\n";

    dmRender::HRenderScript render_script                  = dmRender::NewRenderScript(m_Context, LuaSourceFromString(script));
    dmRender::HRenderScriptInstance render_script_instance = dmRender::NewRenderScriptInstance(m_Context, render_script);

    dmGraphics::RenderTargetCreationParams params          = {};

    params.m_DepthBufferCreationParams.m_Width          = 8;
    params.m_DepthBufferCreationParams.m_Height         = 8;
    params.m_DepthBufferCreationParams.m_OriginalWidth  = 8;
    params.m_DepthBufferCreationParams.m_OriginalHeight = 8;

    params.m_DepthBufferParams.m_Width  = params.m_DepthBufferCreationParams.m_Width;
    params.m_DepthBufferParams.m_Height = params.m_DepthBufferCreationParams.m_Height;
    params.m_DepthBufferParams.m_Format = dmGraphics::TEXTURE_FORMAT_DEPTH;

    params.m_StencilBufferCreationParams.m_Width          = 16;
    params.m_StencilBufferCreationParams.m_Height         = 16;
    params.m_StencilBufferCreationParams.m_OriginalWidth  = 16;
    params.m_StencilBufferCreationParams.m_OriginalHeight = 16;

    params.m_StencilBufferParams.m_Width  = params.m_StencilBufferCreationParams.m_Width;
    params.m_StencilBufferParams.m_Height = params.m_StencilBufferCreationParams.m_Height;
    params.m_StencilBufferParams.m_Format = dmGraphics::TEXTURE_FORMAT_STENCIL;

    for (int i = 0; i < DM_ARRAY_SIZE(params.m_ColorBufferCreationParams); ++i)
    {
        params.m_ColorBufferCreationParams[i].m_Width          = 32 + 32 * i;
        params.m_ColorBufferCreationParams[i].m_Height         = 32 + 32 * i;
        params.m_ColorBufferCreationParams[i].m_OriginalWidth  = 32 + 32 * i;
        params.m_ColorBufferCreationParams[i].m_OriginalHeight = 32 + 32 * i;

        params.m_ColorBufferParams[i].m_Width  = params.m_ColorBufferCreationParams[i].m_Width;
        params.m_ColorBufferParams[i].m_Height = params.m_ColorBufferCreationParams[i].m_Height;
        params.m_ColorBufferParams[i].m_Format = dmGraphics::TEXTURE_FORMAT_LUMINANCE;
    }

    uint32_t rt_flags = 0xffff;

    dmGraphics::HRenderTarget rt = dmGraphics::NewRenderTarget(m_GraphicsContext, rt_flags, params);
    dmRender::AddRenderScriptInstanceRenderResource(render_script_instance, "valid_rt", rt, dmRender::RENDER_RESOURCE_TYPE_RENDER_TARGET);
    ASSERT_EQ(dmRender::RENDER_SCRIPT_RESULT_OK, dmRender::InitRenderScriptInstance(render_script_instance));

    ClearRenderScriptInstanceRenderResources(render_script_instance);

    dmGraphics::DeleteRenderTarget(m_GraphicsContext, rt);
    dmRender::DeleteRenderScriptInstance(render_script_instance);
    dmRender::DeleteRenderScript(m_Context, render_script);
}

TEST_F(dmRenderScriptTest, TestRenderCameraGetSetInfo)
{
    dmRender::HRenderCamera camera = dmRender::NewRenderCamera(m_Context);

    dmMessage::URL camera_url = {};
    camera_url.m_Socket   = dmHashString64("main");
    camera_url.m_Path     = dmHashString64("test_go");
    camera_url.m_Fragment = dmHashString64("camera");

    dmRender::SetRenderCameraURL(m_Context, camera, &camera_url);

    dmRender::RenderCameraData data = {};
    data.m_Viewport         = dmVMath::Vector4(0.0f, 0.0f, 1.0f, 1.0f);
    data.m_Fov              = 90.0f;
    data.m_NearZ            = 0.1f;
    data.m_FarZ             = 100.0f;
    data.m_AspectRatio      = 1.0f;
    data.m_OrthographicZoom = 1.0f;

    dmRender::SetRenderCameraData(m_Context, camera, &data);

    dmRender::RenderCamera* camera_ptr = dmRender::GetRenderCameraByUrl(m_Context, camera_url);
    ASSERT_NE((void*)0, camera_ptr);

    const char* script =
        "function assert_near(a,b)\n"
        "    assert(math.abs(a-b) < 0.00001)\n"
        "end\n"
        "function init(self)\n"
        "    local cams = camera.get_cameras()\n"
        "    assert(#cams == 1)\n"
        "    assert(cams[1].socket == hash('main'))\n"
        "    assert(cams[1].path == hash('test_go'))\n"
        "    assert(cams[1].fragment == hash('camera'))\n"
        // Test "get"
        "    assert_near(camera.get_aspect_ratio(cams[1]), 1)\n"
        "    assert_near(camera.get_near_z(cams[1]), 0.1)\n"
        "    assert_near(camera.get_far_z(cams[1]), 100)\n"
        "    assert_near(camera.get_fov(cams[1]), 90)\n"
        "    assert_near(camera.get_orthographic_zoom(cams[1]), 1)\n"
        // Test "set"
        "    camera.set_near_z(cams[1], -1)\n"
        "    assert_near(camera.get_near_z(cams[1]), -1)\n"
        "    camera.set_far_z(cams[1], 1)\n"
        "    assert_near(camera.get_far_z(cams[1]), 1)\n"
        "    camera.set_fov(cams[1], 45)\n"
        "    assert_near(camera.get_fov(cams[1]), 45)\n"
        "    camera.set_orthographic_zoom(cams[1], 2)\n"
        "    assert_near(camera.get_orthographic_zoom(cams[1]), 2)\n"
        // Test set_camera()
        "    render.set_camera(cams[1])\n"
        "    render.set_camera()\n"
        "end\n";

    dmRender::HRenderScript render_script                  = dmRender::NewRenderScript(m_Context, LuaSourceFromString(script));
    dmRender::HRenderScriptInstance render_script_instance = dmRender::NewRenderScriptInstance(m_Context, render_script);
    ASSERT_EQ(dmRender::RENDER_SCRIPT_RESULT_OK, dmRender::InitRenderScriptInstance(render_script_instance));

    dmArray<dmRender::Command>& commands = render_script_instance->m_CommandBuffer;
    ASSERT_EQ(2u, commands.Size());

    // set_camera(camera)
    ASSERT_EQ(dmRender::COMMAND_TYPE_SET_RENDER_CAMERA, commands[0].m_Type);
    ASSERT_EQ(camera, (dmRender::HRenderCamera) commands[0].m_Operands[0]);

    // draw
    ASSERT_EQ(dmRender::COMMAND_TYPE_SET_RENDER_CAMERA, commands[1].m_Type);
    ASSERT_EQ(0, commands[1].m_Operands[0]);

    dmRender::ParseCommands(m_Context, &commands[0], commands.Size());

    ClearRenderScriptInstanceRenderResources(render_script_instance);

    dmRender::DeleteRenderScriptInstance(render_script_instance);
    dmRender::DeleteRenderScript(m_Context, render_script);
}

TEST_F(dmRenderScriptTest, TestRenderResourceTable)
{
    const char* script =
        "function init(self)\n"
        "   self.update_num = -1\n"
        "end\n"
        "function update(self)\n"
        "   self.update_num = self.update_num + 1\n"
        "   if self.update_num == 0 then\n"
        "       render.enable_texture(0, 'valid_rt')\n"
        "       render.set_render_target('valid_rt')\n"
        "   elseif self.update_num == 1 then\n"
        "       render.disable_texture(0)\n"
        "       render.set_render_target(render.RENDER_TARGET_DEFAULT)\n"
        "   elseif self.update_num == 2 then\n"
        "       render.enable_texture(0, 'invalid_rt')\n"
        "   elseif self.update_num == 3 then\n"
        "       render.enable_material('valid_material')\n"
        "       render.enable_texture('texture_sampler_1', 'valid_rt')\n"
        "       render.enable_texture('texture_sampler_2', 'valid_rt')\n"
        "       render.enable_texture('texture_sampler_3', 'valid_rt')\n"
        "   elseif self.update_num == 4 then\n"
        "       render.enable_material('invalid_material')\n"
        "   end\n"
        "end\n";

    dmRender::HRenderScript render_script                  = dmRender::NewRenderScript(m_Context, LuaSourceFromString(script));
    dmRender::HRenderScriptInstance render_script_instance = dmRender::NewRenderScriptInstance(m_Context, render_script);


    /////////////////////////////
    // MATERIAL
    /////////////////////////////
    const char* shader_src = "uniform lowp sampler2D texture_sampler_1;\n"
                             "uniform lowp sampler2D texture_sampler_2;\n"
                             "uniform lowp sampler2D texture_sampler_3;\n";

    dmGraphics::ShaderDescBuilder shader_desc_builder;
    shader_desc_builder.AddShader(dmGraphics::ShaderDesc::SHADER_TYPE_VERTEX, dmGraphics::ShaderDesc::LANGUAGE_GLSL_SM330, shader_src, strlen(shader_src));
    shader_desc_builder.AddShader(dmGraphics::ShaderDesc::SHADER_TYPE_FRAGMENT, dmGraphics::ShaderDesc::LANGUAGE_GLSL_SM330, shader_src, strlen(shader_src));

    dmGraphics::HProgram program = dmGraphics::NewProgram(m_GraphicsContext, shader_desc_builder.Get(), 0, 0);
    dmRender::HMaterial material = dmRender::NewMaterial(m_Context, program);

    /////////////////////////////
    // RENDER TARGET
    /////////////////////////////
    dmGraphics::RenderTargetCreationParams params          = {};
    params.m_ColorBufferCreationParams[0].m_Width          = 32;
    params.m_ColorBufferCreationParams[0].m_Height         = 32;
    params.m_ColorBufferCreationParams[0].m_OriginalWidth  = 32;
    params.m_ColorBufferCreationParams[0].m_OriginalHeight = 32;

    params.m_ColorBufferParams[0].m_Width  = 32;
    params.m_ColorBufferParams[0].m_Height = 32;
    params.m_ColorBufferParams[0].m_Format = dmGraphics::TEXTURE_FORMAT_LUMINANCE;

    dmGraphics::HRenderTarget rt = dmGraphics::NewRenderTarget(m_GraphicsContext, dmGraphics::BUFFER_TYPE_COLOR0_BIT, params);
    dmGraphics::HTexture tex = dmGraphics::GetRenderTargetTexture(m_GraphicsContext, rt, dmGraphics::BUFFER_TYPE_COLOR0_BIT);

    dmRender::AddRenderScriptInstanceRenderResource(render_script_instance, "valid_rt", rt, dmRender::RENDER_RESOURCE_TYPE_RENDER_TARGET);
    dmRender::AddRenderScriptInstanceRenderResource(render_script_instance, "invalid_rt", 0x1337, dmRender::RENDER_RESOURCE_TYPE_RENDER_TARGET);

    dmRender::AddRenderScriptInstanceRenderResource(render_script_instance, "valid_material", (uint64_t) material, dmRender::RENDER_RESOURCE_TYPE_MATERIAL);
    dmRender::AddRenderScriptInstanceRenderResource(render_script_instance, "invalid_material", 0xBAD, dmRender::RENDER_RESOURCE_TYPE_INVALID);

    dmGraphics::NullContext* null_context = (dmGraphics::NullContext*) m_GraphicsContext;
    dmArray<dmRender::Command>& commands = render_script_instance->m_CommandBuffer;

    ASSERT_EQ(dmRender::RENDER_SCRIPT_RESULT_OK, dmRender::InitRenderScriptInstance(render_script_instance));

    // Update 1 - enable texture + set render target from render resource
    {
        ASSERT_EQ(dmRender::RENDER_SCRIPT_RESULT_OK, dmRender::UpdateRenderScriptInstance(render_script_instance, 0.0f));
        dmRender::ParseCommands(m_Context, &commands[0], commands.Size());

        dmGraphics::RenderTarget* rt_ptr = dmGraphics::GetAssetFromContainer<dmGraphics::RenderTarget>(null_context->m_AssetHandleContainer, rt);
        ASSERT_EQ(&rt_ptr->m_FrameBuffer, null_context->m_CurrentFrameBuffer);
        ASSERT_EQ(tex, m_Context->m_TextureBindTable[0].m_Texture);

        commands.SetSize(0);
    }

    // Update 2 - unbind both
    {
        ASSERT_EQ(dmRender::RENDER_SCRIPT_RESULT_OK, dmRender::UpdateRenderScriptInstance(render_script_instance, 0.0f));
        dmRender::ParseCommands(m_Context, &commands[0], commands.Size());

        ASSERT_EQ(&null_context->m_MainFrameBuffer, null_context->m_CurrentFrameBuffer);
        ASSERT_EQ(0, m_Context->m_TextureBindTable[0].m_Texture);
        commands.SetSize(0);
    }

    // Update 3 - try binding invalid render target
    {
        ASSERT_EQ(dmRender::RENDER_SCRIPT_RESULT_FAILED, dmRender::UpdateRenderScriptInstance(render_script_instance, 0.0f));
    }

    // Update 4 - bind valid material + rt
    {
        m_Context->m_TextureBindTable.SetSize(0);

        ASSERT_EQ(dmRender::RENDER_SCRIPT_RESULT_OK, dmRender::UpdateRenderScriptInstance(render_script_instance, 0.0f));
        dmRender::ParseCommands(m_Context, &commands[0], commands.Size());

        ASSERT_EQ(m_Context->m_Material, material);
        ASSERT_EQ(tex, m_Context->m_TextureBindTable[0].m_Texture);
        ASSERT_EQ(dmHashString64("texture_sampler_1"), m_Context->m_TextureBindTable[0].m_Samplerhash);

        ASSERT_EQ(tex, m_Context->m_TextureBindTable[1].m_Texture);
        ASSERT_EQ(dmHashString64("texture_sampler_2"), m_Context->m_TextureBindTable[1].m_Samplerhash);

        ASSERT_EQ(tex, m_Context->m_TextureBindTable[2].m_Texture);
        ASSERT_EQ(dmHashString64("texture_sampler_3"), m_Context->m_TextureBindTable[2].m_Samplerhash);

        commands.SetSize(0);
    }

    // Update 5 - try binding invalid material
    {
        ASSERT_EQ(dmRender::RENDER_SCRIPT_RESULT_FAILED, dmRender::UpdateRenderScriptInstance(render_script_instance, 0.0f));
    }

    ClearRenderScriptInstanceRenderResources(render_script_instance);

    dmGraphics::DeleteRenderTarget(m_GraphicsContext, rt);
    dmRender::DeleteMaterial(m_Context, material);
    dmGraphics::DeleteProgram(m_GraphicsContext, program);

    dmRender::DeleteRenderScriptInstance(render_script_instance);
    dmRender::DeleteRenderScript(m_Context, render_script);
}

TEST_F(dmRenderScriptTest, TestComputeEnableDisable)
{
    const char* script =
    "function init(self)\n"
    "   render.set_compute('test_compute')\n"
    "   render.set_compute(hash('test_compute'))\n"
    "   render.set_compute(nil)\n"
    "   render.set_compute()\n"
    "end\n";
    dmRender::HRenderScript render_script = dmRender::NewRenderScript(m_Context, LuaSourceFromString(script));
    dmRender::HRenderScriptInstance render_script_instance = dmRender::NewRenderScriptInstance(m_Context, render_script);

    ASSERT_EQ(dmRender::RENDER_SCRIPT_RESULT_FAILED, dmRender::InitRenderScriptInstance(render_script_instance));
    dmRender::AddRenderScriptInstanceRenderResource(render_script_instance, "test_compute", (uint64_t) m_Compute, dmRender::RENDER_RESOURCE_TYPE_COMPUTE);
    ASSERT_EQ(dmRender::RENDER_SCRIPT_RESULT_OK, dmRender::InitRenderScriptInstance(render_script_instance));

    dmRender::DeleteRenderScriptInstance(render_script_instance);
    dmRender::DeleteRenderScript(m_Context, render_script);
}

TEST_F(dmRenderScriptTest, TestDispatch)
{
    const char* script =
    "function init(self)\n"
    "   render.set_compute('test_compute')\n"
    "   render.dispatch_compute(1,2,3)\n"
    "   render.set_compute()\n"
    "end\n";
    dmRender::HRenderScript render_script = dmRender::NewRenderScript(m_Context, LuaSourceFromString(script));
    dmRender::HRenderScriptInstance render_script_instance = dmRender::NewRenderScriptInstance(m_Context, render_script);

    ASSERT_EQ(dmRender::RENDER_SCRIPT_RESULT_FAILED, dmRender::InitRenderScriptInstance(render_script_instance));
    dmRender::AddRenderScriptInstanceRenderResource(render_script_instance, "test_compute", (uint64_t) m_Compute, dmRender::RENDER_RESOURCE_TYPE_COMPUTE);
    ASSERT_EQ(dmRender::RENDER_SCRIPT_RESULT_OK, dmRender::InitRenderScriptInstance(render_script_instance));

    dmArray<dmRender::Command>& commands = render_script_instance->m_CommandBuffer;
    dmRender::ParseCommands(m_Context, &commands[0], commands.Size());

    ASSERT_EQ(dmRender::COMMAND_TYPE_SET_COMPUTE, commands[0].m_Type);
    ASSERT_EQ(m_Compute, (dmRender::HComputeProgram) commands[0].m_Operands[0]);

    ASSERT_EQ(dmRender::COMMAND_TYPE_DISPATCH_COMPUTE, commands[1].m_Type);
    ASSERT_EQ(1, commands[1].m_Operands[0]);
    ASSERT_EQ(2, commands[1].m_Operands[1]);
    ASSERT_EQ(3, commands[1].m_Operands[2]);

    ASSERT_EQ(dmRender::COMMAND_TYPE_SET_COMPUTE, commands[2].m_Type);
    ASSERT_EQ(0, commands[2].m_Operands[0]);

    commands.SetSize(0);

    dmRender::DeleteRenderScriptInstance(render_script_instance);
    dmRender::DeleteRenderScript(m_Context, render_script);
}

extern "C" void dmExportedSymbols();

int main(int argc, char **argv)
{
    dmExportedSymbols();
    TestMainPlatformInit();
    dmDDF::RegisterAllTypes();
    jc_test_init(&argc, argv);
    return jc_test_run_all();
}
