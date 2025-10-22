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

#define JC_TEST_IMPLEMENTATION
#include <jc_test/jc_test.h>
#include <testmain/testmain.h>
#include <dlib/dstrings.h>
#include <dmsdk/dlib/vmath.h>

#include <ddf/ddf.h>
#include <script/lua_source_ddf.h>

#include "test_gui_shared.h"

#include "../gui.h"
#include "../gui_private.h"
#include "../gui_script.h"

extern "C"
{
#include "lua/lua.h"
#include "lua/lauxlib.h"
}

using namespace dmVMath;

static const float EPSILON = 0.000001f;

static const float TEXT_GLYPH_WIDTH = 1.0f;
static const float TEXT_MAX_ASCENT = 0.75f;
static const float TEXT_MAX_DESCENT = 0.25f;

static dmLuaDDF::LuaSource* LuaSourceFromStr(const char *str)
{
    static dmLuaDDF::LuaSource src;
    memset(&src, 0x00, sizeof(dmLuaDDF::LuaSource));
    src.m_Script.m_Data = (uint8_t*) str;
    src.m_Script.m_Count = strlen(str);
    src.m_Filename = "gui-dummy";
    return &src;
}

void GetTextMetricsCallback(const void* font, const char* text, float width, bool line_break, float leading, float tracking, dmGui::TextMetrics* out_metrics);
void RenderNodesStoreTransform(dmGui::HScene scene, const dmGui::RenderEntry* nodes, const dmVMath::Matrix4* node_transforms, const float* node_opacities,
        const dmGui::StencilScope** stencil_scopes, uint32_t node_count, void* context);

class dmGuiScriptTest : public jc_test_base_class
{
public:
    dmScript::HContext m_ScriptContext;
    dmPlatform::HWindow m_Window;
    dmHID::HContext m_HidContext;
    dmGui::HContext m_Context;
    dmGui::RenderSceneParams m_RenderParams;

    DynamicTextureContainer m_DynamicTextures;

    void SetUp() override
    {
        dmPlatform::WindowParams window_params = {};
        window_params.m_Width                  = 2;
        window_params.m_Height                 = 2;
        window_params.m_GraphicsApi            = dmPlatform::PLATFORM_GRAPHICS_API_NULL;

        m_Window = dmPlatform::NewWindow();
        dmPlatform::OpenWindow(m_Window, window_params);

        dmScript::ContextParams script_context_params = {};
        m_ScriptContext = dmScript::NewContext(script_context_params);
        dmScript::Initialize(m_ScriptContext);

        m_HidContext = dmHID::NewContext(dmHID::NewContextParams());
        dmHID::Init(m_HidContext);
        dmHID::SetWindow(m_HidContext, m_Window);

        dmGui::NewContextParams context_params;
        context_params.m_ScriptContext = m_ScriptContext;
        context_params.m_GetTextMetricsCallback = GetTextMetricsCallback;
        context_params.m_PhysicalWidth = 1;
        context_params.m_PhysicalHeight = 1;
        context_params.m_HidContext = m_HidContext;
        m_Context = dmGui::NewContext(&context_params);

        m_RenderParams.m_RenderNodes = RenderNodesStoreTransform;
    }

    void TearDown() override
    {
        dmGui::DeleteContext(m_Context, m_ScriptContext);
        dmScript::Finalize(m_ScriptContext);
        dmScript::DeleteContext(m_ScriptContext);
        dmHID::Final(m_HidContext);
        dmHID::DeleteContext(m_HidContext);
        dmPlatform::DeleteWindow(m_Window);
    }
};

void GetTextMetricsCallback(const void* font, const char* text, float width, bool line_break, float leading, float tracking, dmGui::TextMetrics* out_metrics)
{
    out_metrics->m_Width = strlen(text) * TEXT_GLYPH_WIDTH;
    out_metrics->m_MaxAscent = TEXT_MAX_ASCENT;
    out_metrics->m_MaxDescent = TEXT_MAX_DESCENT;
}

TEST_F(dmGuiScriptTest, URLOutsideFunctions)
{
    dmGui::HScript script = NewScript(m_Context);

    const char* src = "local url = msg.url(\"test\")\n"
                      "function init(self)\n"
                      "    assert(hash(\"test\") == url.path)\n"
                      "end\n";
    dmGui::NewSceneParams params;
    params.m_MaxNodes = 64;
    params.m_MaxAnimations = 32;
    params.m_UserData = this;
    dmGui::HScene scene = dmGui::NewScene(m_Context, &params);
    dmGui::SetSceneScript(scene, script);
    dmGui::Result result = SetScript(script, LuaSourceFromStr(src));
    ASSERT_EQ(dmGui::RESULT_OK, result);

    ASSERT_EQ(dmGui::RESULT_OK, dmGui::InitScene(scene));

    dmGui::DeleteScene(scene);
    dmGui::DeleteScript(script);
}

TEST_F(dmGuiScriptTest, GetScreenPos)
{
    dmGui::HScript script = NewScript(m_Context);

    dmGui::NewSceneParams params;
    params.m_MaxNodes = 64;
    params.m_MaxAnimations = 32;
    params.m_UserData = this;
    dmGui::HScene scene = dmGui::NewScene(m_Context, &params);
    dmGui::SetSceneResolution(scene, 1, 1);
    dmGui::SetSceneScript(scene, script);

    const char* src = "function init(self)\n"
                      "    local p = vmath.vector3(10, 10, 0)\n"
                      "    local s = vmath.vector3(20, 20, 0)\n"
                      "    local n1 = gui.new_box_node(p, s)\n"
                      "    local n2 = gui.new_text_node(p, \"text\")\n"
                      "    local n3 = gui.new_pie_node(p, s)\n"
                      "    gui.set_size(n2, s)\n"
                      "    gui.set_size(n3, s)\n"
                      "    assert(gui.get_screen_position(n1) == gui.get_screen_position(n2))\n"
                      "    assert(gui.get_screen_position(n1) == gui.get_screen_position(n3))\n"
                      "end\n";
    dmGui::Result result = SetScript(script, LuaSourceFromStr(src));
    ASSERT_EQ(dmGui::RESULT_OK, result);

    ASSERT_EQ(dmGui::RESULT_OK, dmGui::InitScene(scene));

    dmGui::DeleteScene(scene);

    DeleteScript(script);
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

TEST_F(dmGuiScriptTest, TestInstanceCallback)
{
    lua_State* L = dmGui::GetLuaState(m_Context);

    lua_register(L, "test_ref", TestRef);

    int ref = LUA_NOREF;

    lua_pushlightuserdata(L, &ref);
    lua_setglobal(L, REF_VALUE);

    dmGui::HScript script = NewScript(m_Context);

    dmGui::NewSceneParams params;
    params.m_MaxNodes = 64;
    params.m_MaxAnimations = 32;
    params.m_UserData = this;
    //params.m_RigContext = m_RigContext;
    dmGui::HScene scene = dmGui::NewScene(m_Context, &params);
    dmGui::SetSceneScript(scene, script);

    const char* src =
            "function init(self)\n"
            "    test_ref()\n"
            "end\n";

    dmGui::Result result = SetScript(script, LuaSourceFromStr(src));
    ASSERT_EQ(dmGui::RESULT_OK, result);

    dmGui::InitScene(scene);

    ASSERT_NE(ref, LUA_NOREF);
    lua_rawgeti(L, LUA_REGISTRYINDEX, ref);
    dmScript::SetInstance(L);
    ASSERT_TRUE(dmScript::IsInstanceValid(L));

    dmGui::DeleteScene(scene);

    dmGui::DeleteScript(script);

    lua_rawgeti(L, LUA_REGISTRYINDEX, ref);
    dmScript::SetInstance(L);
    ASSERT_FALSE(dmScript::IsInstanceValid(L));
}

#undef REF_VALUE

TEST_F(dmGuiScriptTest, TestGlobalNodeFail)
{
    dmGui::HScript script = NewScript(m_Context);

    dmGui::NewSceneParams params;
    params.m_MaxNodes = 64;
    params.m_MaxAnimations = 32;
    params.m_UserData = this;
    //params.m_RigContext = m_RigContext;
    dmGui::HScene scene = dmGui::NewScene(m_Context, &params);
    dmGui::HScene scene2 = dmGui::NewScene(m_Context, &params);
    dmGui::SetSceneScript(scene, script);
    dmGui::SetSceneScript(scene2, script);

    const char* src =
            "local n = nil\n"
            ""
            "function init(self)\n"
            "    n = gui.new_box_node(vmath.vector3(1, 1, 1), vmath.vector3(1, 1, 1))\n"
            "end\n"
            ""
            "function update(self, dt)\n"
            "    -- should produce lua error since update is called with another scene\n"
            "    assert(gui.get_position(n).x == 1)\n"
            "end\n";

    dmGui::Result result = SetScript(script, LuaSourceFromStr(src));
    ASSERT_EQ(dmGui::RESULT_OK, result);

    result = dmGui::InitScene(scene);
    ASSERT_EQ(dmGui::RESULT_OK, result);

    result = dmGui::UpdateScene(scene2, 1.0f / 60);
    ASSERT_NE(dmGui::RESULT_OK, result);

    dmGui::DeleteScene(scene);
    dmGui::DeleteScene(scene2);

    dmGui::DeleteScript(script);
}

TEST_F(dmGuiScriptTest, TestParenting)
{
    dmGui::HScript script = NewScript(m_Context);

    dmGui::NewSceneParams params;
    params.m_MaxNodes = 64;
    params.m_MaxAnimations = 32;
    params.m_UserData = this;
    //params.m_RigContext = m_RigContext;
    dmGui::HScene scene = dmGui::NewScene(m_Context, &params);
    dmGui::SetSceneScript(scene, script);

    const char* src =
            "function init(self)\n"
            "    local parent = gui.new_box_node(vmath.vector3(1, 1, 1), vmath.vector3(1, 1, 1))\n"
            "    local child = gui.new_box_node(vmath.vector3(1, 1, 1), vmath.vector3(1, 1, 1))\n"
            "    assert(gui.get_parent(child) == nil)\n"
            "    gui.set_parent(child, parent)\n"
            "    assert(gui.get_parent(child) == parent)\n"
            "    gui.set_parent(child, nil)\n"
            "    assert(gui.get_parent(child) == nil)\n"
            "end\n";

    dmGui::Result result = SetScript(script, LuaSourceFromStr(src));
    ASSERT_EQ(dmGui::RESULT_OK, result);

    result = dmGui::InitScene(scene);
    ASSERT_EQ(dmGui::RESULT_OK, result);

    dmGui::DeleteScene(scene);

    dmGui::DeleteScript(script);
}

TEST_F(dmGuiScriptTest, TestGetType)
{
    dmGui::HScript script = NewScript(m_Context);

    dmGui::NewSceneParams params;
    params.m_MaxNodes = 64;
    params.m_MaxAnimations = 32;
    params.m_UserData = this;
    dmGui::HScene scene = dmGui::NewScene(m_Context, &params);
    dmGui::SetSceneScript(scene, script);

    const char* src =
            "function init(self)\n"
            "    local node = gui.new_box_node(vmath.vector3(1, 1, 1), vmath.vector3(1, 1, 1))\n"
            "    local type, subtype = gui.get_type(node)\n"
            "    assert(type == gui.TYPE_BOX)\n"
            "    assert(subtype == nil)\n"
            "    local node = gui.new_text_node(vmath.vector3(1, 1, 1), \"TEST\")\n"
            "    local type, subtype = gui.get_type(node)\n"
            "    assert(type == gui.TYPE_TEXT)\n"
            "    assert(subtype == nil)\n"
            "    local node = gui.new_pie_node(vmath.vector3(1, 1, 1), vmath.vector3(1, 1, 1))\n"
            "    local type, subtype = gui.get_type(node)\n"
            "    assert(type == gui.TYPE_PIE)\n"
            "    assert(subtype == nil)\n"
            "end\n";

    dmGui::Result result = SetScript(script, LuaSourceFromStr(src));
    ASSERT_EQ(dmGui::RESULT_OK, result);

    result = dmGui::InitScene(scene);
    ASSERT_EQ(dmGui::RESULT_OK, result);

    dmGui::DeleteScene(scene);

    dmGui::DeleteScript(script);
}

TEST_F(dmGuiScriptTest, TestGetIndex)
{
    dmGui::HScript script = NewScript(m_Context);

    dmGui::NewSceneParams params;
    params.m_MaxNodes = 64;
    params.m_MaxAnimations = 32;
    params.m_UserData = this;
    //params.m_RigContext = m_RigContext;
    dmGui::HScene scene = dmGui::NewScene(m_Context, &params);
    dmGui::SetSceneScript(scene, script);

    const char* src =
            "function init(self)\n"
            "    local parent = gui.new_box_node(vmath.vector3(1, 1, 1), vmath.vector3(1, 1, 1))\n"
            "    local child = gui.new_box_node(vmath.vector3(1, 1, 1), vmath.vector3(1, 1, 1))\n"
            "    assert(gui.get_index(parent) == 0)\n"
            "    assert(gui.get_index(child) == 1)\n"
            "    gui.move_above(parent, nil)\n"
            "    assert(gui.get_index(parent) == 1)\n"
            "    assert(gui.get_index(child) == 0)\n"
            "    gui.set_parent(child, parent)\n"
            "    assert(gui.get_index(parent) == 0)\n"
            "    assert(gui.get_index(child) == 0)\n"
            "    gui.set_parent(child, nil)\n"
            "    assert(gui.get_index(parent) == 0)\n"
            "    assert(gui.get_index(child) == 1)\n"
            "end\n";

    dmGui::Result result = SetScript(script, LuaSourceFromStr(src));
    ASSERT_EQ(dmGui::RESULT_OK, result);

    result = dmGui::InitScene(scene);
    ASSERT_EQ(dmGui::RESULT_OK, result);

    dmGui::DeleteScene(scene);

    dmGui::DeleteScript(script);
}

TEST_F(dmGuiScriptTest, TestCloneNode)
{
    dmGui::HScript script = NewScript(m_Context);

    dmGui::NewSceneParams params;
    params.m_MaxNodes = 64;
    params.m_MaxAnimations = 32;
    params.m_UserData = this;
    dmGui::HScene scene = dmGui::NewScene(m_Context, &params);
    dmGui::SetSceneScript(scene, script);

    const char* src =
            "function init(self)\n"
            "    local node1 = gui.new_box_node(vmath.vector3(1), vmath.vector3(1))\n"
            "    local node2 = gui.new_box_node(vmath.vector3(2), vmath.vector3(2))\n"
            "    local node3 = gui.new_box_node(vmath.vector3(3), vmath.vector3(3))\n"
            "    gui.set_id(node1, \"box1\")\n"
            "    gui.set_id(node2, \"box2\")\n"
            "    gui.set_id(node3, \"box3\")\n"
            "    local clone1 = gui.clone(node1)\n"
            "    local clone2 = gui.clone(node2)\n"
            "    local clone3 = gui.clone(node3)\n"
            "    assert(gui.get_id(clone1) == hash(\"__node0\"))\n"
            "    assert(gui.get_id(clone2) == hash(\"__node1\"))\n"
            "    assert(gui.get_id(clone3) == hash(\"__node2\"))\n"
            "    assert(gui.get_position(clone1) == gui.get_position(node1))\n"
            "    assert(gui.get_position(clone2) == gui.get_position(node2))\n"
            "    assert(gui.get_position(clone3) == gui.get_position(node3))\n"
            "    assert(gui.get_size(clone1) == gui.get_size(node1))\n"
            "    assert(gui.get_size(clone2) == gui.get_size(node2))\n"
            "    assert(gui.get_size(clone3) == gui.get_size(node3))\n"
            "end\n";

    dmGui::Result result = SetScript(script, LuaSourceFromStr(src));
    ASSERT_EQ(dmGui::RESULT_OK, result);

    result = dmGui::InitScene(scene);
    ASSERT_EQ(dmGui::RESULT_OK, result);

    dmGui::DeleteScene(scene);

    dmGui::DeleteScript(script);
}

TEST_F(dmGuiScriptTest, TestCloneTree)
{
    dmGui::HScript script = NewScript(m_Context);

    dmGui::NewSceneParams params;
    params.m_MaxNodes = 64;
    params.m_MaxAnimations = 32;
    params.m_UserData = this;
    //params.m_RigContext = m_RigContext;
    dmGui::HScene scene = dmGui::NewScene(m_Context, &params);
    dmGui::SetSceneScript(scene, script);

    const char* src =
            "function init(self)\n"
            "    local n1 = gui.new_box_node(vmath.vector3(1, 1, 1), vmath.vector3(1, 1, 1))\n"
            "    gui.set_id(n1, \"n1\")\n"
            "    local n2 = gui.new_box_node(vmath.vector3(2, 2, 2), vmath.vector3(1, 1, 1))\n"
            "    gui.set_id(n2, \"n2\")\n"
            "    local n3 = gui.new_box_node(vmath.vector3(3, 3, 3), vmath.vector3(1, 1, 1))\n"
            "    gui.set_id(n3, \"n3\")\n"
            "    local n4 = gui.new_text_node(vmath.vector3(3, 3, 3), \"TEST\")\n"
            "    gui.set_id(n4, \"n4\")\n"
            "    gui.set_parent(n2, n1)\n"
            "    gui.set_parent(n3, n2)\n"
            "    gui.set_parent(n4, n3)\n"
            "    local t = gui.clone_tree(n1)\n"
            "    assert(gui.get_position(t.n1) == gui.get_position(n1))\n"
            "    assert(gui.get_position(t.n2) == gui.get_position(n2))\n"
            "    assert(gui.get_position(t.n3) == gui.get_position(n3))\n"
            "    assert(gui.get_text(t.n4) == gui.get_text(n4))\n"
            "    gui.set_position(t.n1, vmath.vector3(4, 4, 4))\n"
            "    assert(gui.get_position(t.n1) ~= gui.get_position(n1))\n"
            "    gui.set_text(t.n4, \"TEST2\")\n"
            "    assert(gui.get_text(t.n4) ~= gui.get_text(n4))\n"
            "end\n";

    dmGui::Result result = SetScript(script, LuaSourceFromStr(src));
    ASSERT_EQ(dmGui::RESULT_OK, result);

    result = dmGui::InitScene(scene);
    ASSERT_EQ(dmGui::RESULT_OK, result);

    dmGui::DeleteScene(scene);

    dmGui::DeleteScript(script);
}

TEST_F(dmGuiScriptTest, TestGetTree)
{
    dmGui::HScript script = NewScript(m_Context);

    dmGui::NewSceneParams params;
    params.m_MaxNodes = 64;
    params.m_MaxAnimations = 32;
    params.m_UserData = this;
    //params.m_RigContext = m_RigContext;
    dmGui::HScene scene = dmGui::NewScene(m_Context, &params);
    dmGui::SetSceneScript(scene, script);

    const char* src =
            "function init(self)\n"
            "    local n1 = gui.new_box_node(vmath.vector3(1, 1, 1), vmath.vector3(1, 1, 1))\n"
            "    gui.set_id(n1, \"n1\")\n"
            "    local n2 = gui.new_box_node(vmath.vector3(2, 2, 2), vmath.vector3(1, 1, 1))\n"
            "    gui.set_id(n2, \"n2\")\n"
            "    local n3 = gui.new_box_node(vmath.vector3(3, 3, 3), vmath.vector3(1, 1, 1))\n"
            "    gui.set_id(n3, \"n3\")\n"
            "    local n4 = gui.new_text_node(vmath.vector3(3, 3, 3), \"TEST\")\n"
            "    gui.set_id(n4, \"n4\")\n"
            "    gui.set_parent(n2, n1)\n"
            "    gui.set_parent(n3, n2)\n"
            "    gui.set_parent(n4, n3)\n"
            "    local t = gui.get_tree(n1)\n"
            "    assert(t.n1 == n1)\n"
            "    assert(t.n2 == n2)\n"
            "    assert(t.n3 == n3)\n"
            "    assert(t.n4 == n4)\n"
            "end\n";

    dmGui::Result result = SetScript(script, LuaSourceFromStr(src));
    ASSERT_EQ(dmGui::RESULT_OK, result);

    result = dmGui::InitScene(scene);
    ASSERT_EQ(dmGui::RESULT_OK, result);

    dmGui::DeleteScene(scene);

    dmGui::DeleteScript(script);
}

TEST_F(dmGuiScriptTest, TestPieNodeScript)
{
    dmGui::HScript script = NewScript(m_Context);

    dmGui::NewSceneParams params;
    params.m_MaxNodes = 64;
    params.m_MaxAnimations = 32;
    params.m_UserData = this;
    //params.m_RigContext = m_RigContext;
    dmGui::HScene scene = dmGui::NewScene(m_Context, &params);
    dmGui::SetSceneScript(scene, script);

    const char* src =
            "function init(self)\n"
            "    local n = gui.new_pie_node(vmath.vector3(1, 1, 1), vmath.vector3(1, 1, 1))\n"
            "    gui.set_perimeter_vertices(n, 123)\n"
            "    assert(gui.get_perimeter_vertices(n) == 123)\n"
            "    gui.set_inner_radius(n, 456)\n"
            "    assert(gui.get_inner_radius(n) == 456)\n"
            "    gui.set_outer_bounds(n, gui.PIEBOUNDS_RECTANGLE)\n"
            "    assert(gui.get_outer_bounds(n) == gui.PIEBOUNDS_RECTANGLE)\n"
            "    gui.set_outer_bounds(n, gui.PIEBOUNDS_ELLIPSE)\n"
            "    assert(gui.get_outer_bounds(n) == gui.PIEBOUNDS_ELLIPSE)\n"
            "    gui.set_fill_angle(n, 90)\n"
            "    assert(gui.get_fill_angle(n) == 90)\n"
            "end\n";

    dmGui::Result result = SetScript(script, LuaSourceFromStr(src));
    ASSERT_EQ(dmGui::RESULT_OK, result);

    result = dmGui::InitScene(scene);
    ASSERT_EQ(dmGui::RESULT_OK, result);

    dmGui::DeleteScene(scene);
    dmGui::DeleteScript(script);
}

TEST_F(dmGuiScriptTest, TestTextNodeScript)
{
    dmGui::HScript script = NewScript(m_Context);

    dmGui::NewSceneParams params;
    params.m_MaxNodes = 64;
    params.m_MaxAnimations = 32;
    params.m_UserData = this;
    //params.m_RigContext = m_RigContext;
    dmGui::HScene scene = dmGui::NewScene(m_Context, &params);
    dmGui::SetSceneScript(scene, script);

    const char* src =
            "function init(self)\n"
            "    local n = gui.new_text_node(vmath.vector3(0, 0, 0), \"name\")\n"
            "    gui.set_leading(n, 2.5)\n"
            "    assert(gui.get_leading(n) == 2.5)\n"
            "    gui.set_tracking(n, 0.5)\n"
            "    assert(gui.get_tracking(n) == 0.5)\n"
            "end\n";

    dmGui::Result result = SetScript(script, LuaSourceFromStr(src));
    ASSERT_EQ(dmGui::RESULT_OK, result);

    result = dmGui::InitScene(scene);
    ASSERT_EQ(dmGui::RESULT_OK, result);

    dmGui::DeleteScene(scene);
    dmGui::DeleteScript(script);
}

TEST_F(dmGuiScriptTest, TestSlice9)
{
    dmGui::HScript script = NewScript(m_Context);

    dmGui::NewSceneParams params;
    params.m_MaxNodes = 64;
    params.m_MaxAnimations = 32;
    params.m_UserData = this;
    //params.m_RigContext = m_RigContext;
    dmGui::HScene scene = dmGui::NewScene(m_Context, &params);
    dmGui::SetSceneScript(scene, script);

    const char* src =
            "function init(self)\n"
            "    local n = gui.new_box_node(vmath.vector3(1, 1, 1), vmath.vector3(1, 1, 1))\n"
            "    assert(gui.get_slice9(n) == vmath.vector4(0,0,0,0))\n"
            "    local v = vmath.vector4(12,34,56,78)\n"
            "    gui.set_slice9(n, v)\n"
            "    assert(gui.get_slice9(n) == v)\n"
            "end\n";

    dmGui::Result result = SetScript(script, LuaSourceFromStr(src));
    ASSERT_EQ(dmGui::RESULT_OK, result);

    result = dmGui::InitScene(scene);
    ASSERT_EQ(dmGui::RESULT_OK, result);

    dmGui::DeleteScene(scene);
    dmGui::DeleteScript(script);
}


TEST_F(dmGuiScriptTest, TestSizeMode)
{
    dmGui::HScript script = NewScript(m_Context);

    dmGui::NewSceneParams params;
    params.m_MaxNodes = 64;
    params.m_MaxAnimations = 32;
    params.m_UserData = this;
    //params.m_RigContext = m_RigContext;
    dmGui::HScene scene = dmGui::NewScene(m_Context, &params);
    dmGui::SetSceneScript(scene, script);

    dmGui::Result result;

    int t1;
    result = dmGui::AddTexture(scene, dmHashString64("t1"), (dmGui::HTextureSource) &t1, dmGui::NODE_TEXTURE_TYPE_TEXTURE_SET, 1, 1);
    ASSERT_EQ(result, dmGui::RESULT_OK);

    const char* src =
            "function init(self)\n"
            "    local n = gui.new_box_node(vmath.vector3(0, 0, 0), vmath.vector3(10, 10, 0))\n"
            "    gui.set_texture(n, 't1')\n"
            "    assert(gui.get_size_mode(n) == gui.SIZE_MODE_MANUAL)\n"
            "    assert(gui.get_size(n) == vmath.vector3(10, 10, 0))\n"
            "    gui.set_size_mode(n, gui.SIZE_MODE_AUTO)\n"
            "    assert(gui.get_size_mode(n) == gui.SIZE_MODE_AUTO)\n"
            "    assert(gui.get_size(n) == vmath.vector3(1, 1, 0))\n"
            "end\n";

    result = SetScript(script, LuaSourceFromStr(src));
    ASSERT_EQ(dmGui::RESULT_OK, result);

    result = dmGui::InitScene(scene);
    ASSERT_EQ(dmGui::RESULT_OK, result);

    dmGui::DeleteScene(scene);
    dmGui::DeleteScript(script);
}


void RenderNodesStoreTransform(dmGui::HScene scene, const dmGui::RenderEntry* nodes, const dmVMath::Matrix4* node_transforms, const float* node_opacities,
        const dmGui::StencilScope** stencil_scopes, uint32_t node_count, void* context)
{
    dmVMath::Matrix4* out_transforms = (dmVMath::Matrix4*)context;
    memcpy(out_transforms, node_transforms, sizeof(dmVMath::Matrix4) * node_count);
}

TEST_F(dmGuiScriptTest, TestLocalTransformSetPos)
{
    dmGui::HScript script = NewScript(m_Context);

    dmGui::NewSceneParams params;
    params.m_MaxNodes = 64;
    params.m_MaxAnimations = 32;
    params.m_UserData = this;
    //params.m_RigContext = m_RigContext;
    dmGui::HScene scene = dmGui::NewScene(m_Context, &params);
    dmGui::SetSceneResolution(scene, 1, 1);
    dmGui::SetSceneScript(scene, script);

    // Set position
    const char* src =
            "function init(self)\n"
            "    local n1 = gui.new_box_node(vmath.vector3(1, 1, 1), vmath.vector3(1, 1, 1))\n"
            "    gui.set_pivot(n1, gui.PIVOT_SW)\n"
            "    gui.set_position(n1, vmath.vector3(2, 2, 2))\n"
            "end\n";

    dmGui::Result result = SetScript(script, LuaSourceFromStr(src));
    ASSERT_EQ(dmGui::RESULT_OK, result);

    result = dmGui::InitScene(scene);
    ASSERT_EQ(dmGui::RESULT_OK, result);

    dmVMath::Matrix4 transform;
    dmGui::RenderScene(scene, m_RenderParams, &transform);

    ASSERT_NEAR(2.0f, transform.getElem(3, 0), EPSILON);
    ASSERT_NEAR(2.0f, transform.getElem(3, 1), EPSILON);
    ASSERT_NEAR(2.0f, transform.getElem(3, 2), EPSILON);

    dmGui::DeleteScene(scene);

    dmGui::DeleteScript(script);
}

static void InitializeScriptScene(dmGui::HContext* context,
    dmGui::HScript* script, dmGui::HScene* scene, void* user_data, const char* source)
{
    *script = dmGui::NewScript(*context);
    dmGui::NewSceneParams params;
    params.m_MaxNodes = 64;
    params.m_MaxAnimations = 32;
    params.m_UserData = user_data;
    //params.m_RigContext = rig_context;
    *scene = dmGui::NewScene(*context, &params);

    dmGui::Result result = dmGui::RESULT_OK;

    dmGui::SetSceneResolution(*scene, 1, 1);

    result = dmGui::SetSceneScript(*scene, *script);
    ASSERT_EQ(dmGui::RESULT_OK, result);

    result = SetScript(*script, LuaSourceFromStr(source));
    ASSERT_EQ(dmGui::RESULT_OK, result);

    result = dmGui::InitScene(*scene);
    ASSERT_EQ(dmGui::RESULT_OK, result);
}

TEST_F(dmGuiScriptTest, TestSetRenderOrderMinimum)
{
    dmGui::HScript script = NULL;
    dmGui::HScene scene = NULL;

    const char* source = "function init(self)\ngui.set_render_order(0)\nend\n";
    InitializeScriptScene(&m_Context, &script, &scene, this, source);
    ASSERT_EQ(0, scene->m_RenderOrder);

    dmGui::DeleteScene(scene);
    dmGui::DeleteScript(script);
}

TEST_F(dmGuiScriptTest, TestSetRenderOrderMaximum)
{
    dmGui::HScript script = NULL;
    dmGui::HScene scene = NULL;

    const char* source = "function init(self)\ngui.set_render_order(15)\nend\n";
    InitializeScriptScene(&m_Context, &script, &scene, this, source);
    ASSERT_EQ(15, scene->m_RenderOrder);

    dmGui::DeleteScene(scene);
    dmGui::DeleteScript(script);
}

TEST_F(dmGuiScriptTest, TestSetRenderOrderUnderflow)
{
    dmGui::HScript script = NULL;
    dmGui::HScene scene = NULL;

    const char* source = "function init(self)\ngui.set_render_order(-1)\nend\n";
    InitializeScriptScene(&m_Context, &script, &scene, this, source);
    ASSERT_EQ(0, scene->m_RenderOrder);

    dmGui::DeleteScene(scene);
    dmGui::DeleteScript(script);
}

TEST_F(dmGuiScriptTest, TestSetRenderOrderOverflow)
{
    dmGui::HScript script = NULL;
    dmGui::HScene scene = NULL;

    const char* source = "function init(self)\ngui.set_render_order(16)\nend\n";
    InitializeScriptScene(&m_Context, &script, &scene, this, source);
    ASSERT_EQ(15, scene->m_RenderOrder);

    dmGui::DeleteScene(scene);
    dmGui::DeleteScript(script);
}

TEST_F(dmGuiScriptTest, TestLocalTransformAnim)
{
    dmGui::HScript script = NewScript(m_Context);

    dmGui::NewSceneParams params;
    params.m_MaxNodes = 64;
    params.m_MaxAnimations = 32;
    params.m_UserData = this;
    //params.m_RigContext = m_RigContext;
    dmGui::HScene scene = dmGui::NewScene(m_Context, &params);
    dmGui::SetSceneResolution(scene, 1, 1);
    dmGui::SetSceneScript(scene, script);

    // Set position
    const char* src =
            "function init(self)\n"
            "    local n1 = gui.new_box_node(vmath.vector3(1, 1, 1), vmath.vector3(1, 1, 1))\n"
            "    gui.set_pivot(n1, gui.PIVOT_SW)\n"
            "    gui.set_position(n1, vmath.vector3(0, 0, 0))\n"
            "    gui.animate(n1, gui.PROP_POSITION, vmath.vector3(2, 2, 2), gui.EASING_LINEAR, 1)\n"
            "end\n";

    dmGui::Result result = SetScript(script, LuaSourceFromStr(src));
    ASSERT_EQ(dmGui::RESULT_OK, result);

    result = dmGui::InitScene(scene);
    ASSERT_EQ(dmGui::RESULT_OK, result);

    dmVMath::Matrix4 transform;
    dmGui::RenderScene(scene, m_RenderParams, &transform);

    ASSERT_NEAR(0.0f, transform.getElem(3, 0), EPSILON);
    ASSERT_NEAR(0.0f, transform.getElem(3, 1), EPSILON);
    ASSERT_NEAR(0.0f, transform.getElem(3, 2), EPSILON);

    dmGui::UpdateScene(scene, 1.0f);

    dmGui::RenderScene(scene, m_RenderParams, &transform);

    ASSERT_NEAR(2.0f, transform.getElem(3, 0), EPSILON);
    ASSERT_NEAR(2.0f, transform.getElem(3, 1), EPSILON);
    ASSERT_NEAR(2.0f, transform.getElem(3, 2), EPSILON);

    dmGui::DeleteScene(scene);

    dmGui::DeleteScript(script);
}

// DEF-1972 Callback called to early (when finishing the first animation, not the last)
TEST_F(dmGuiScriptTest, TestLocalTransformAnimWithCallback)
{
    dmGui::HScript script = NewScript(m_Context);

    dmGui::NewSceneParams params;
    params.m_MaxNodes = 64;
    params.m_MaxAnimations = 32;
    params.m_UserData = this;
    //params.m_RigContext = m_RigContext;
    dmGui::HScene scene = dmGui::NewScene(m_Context, &params);
    dmGui::SetSceneResolution(scene, 1, 1);
    dmGui::SetSceneScript(scene, script);

    // Set position
    const char* src =
            "local n2 = nil"
            "local function anim_done(self, node)\n"
            "   local pos = gui.get_position(node)\n"
            "   gui.set_position(n2, pos)\n"
            "end\n"
            "function init(self)\n"
            "    local n1 = gui.new_box_node(vmath.vector3(1, 1, 1), vmath.vector3(1, 1, 1))\n"
            "    n2 = gui.new_box_node(vmath.vector3(1, 1, 1), vmath.vector3(1, 1, 1))\n"
            "    gui.set_pivot(n1, gui.PIVOT_SW)\n"
            "    gui.set_pivot(n2, gui.PIVOT_SW)\n"
            "    gui.set_position(n1, vmath.vector3(0, 0, 0))\n"
            "    gui.set_position(n2, vmath.vector3(0, 0, 0))\n"
            "    gui.animate(n1, gui.PROP_POSITION, vmath.vector3(2, 2, 2), gui.EASING_LINEAR, 1, 0, anim_done)\n"
            "end\n";

    dmGui::Result result = SetScript(script, LuaSourceFromStr(src));
    ASSERT_EQ(dmGui::RESULT_OK, result);

    result = dmGui::InitScene(scene);
    ASSERT_EQ(dmGui::RESULT_OK, result);

    dmVMath::Matrix4 transforms[2];
    dmGui::RenderScene(scene, m_RenderParams, transforms);

    ASSERT_NEAR(0.0f, transforms[0].getElem(3, 0), EPSILON);
    ASSERT_NEAR(0.0f, transforms[0].getElem(3, 1), EPSILON);
    ASSERT_NEAR(0.0f, transforms[0].getElem(3, 2), EPSILON);
    ASSERT_NEAR(0.0f, transforms[1].getElem(3, 0), EPSILON);
    ASSERT_NEAR(0.0f, transforms[1].getElem(3, 1), EPSILON);
    ASSERT_NEAR(0.0f, transforms[1].getElem(3, 2), EPSILON);

    dmGui::UpdateScene(scene, 1.0f);

    dmGui::RenderScene(scene, m_RenderParams, transforms);

    ASSERT_NEAR(2.0f, transforms[0].getElem(3, 0), EPSILON);
    ASSERT_NEAR(2.0f, transforms[0].getElem(3, 1), EPSILON);
    ASSERT_NEAR(2.0f, transforms[0].getElem(3, 2), EPSILON);
    ASSERT_NEAR(2.0f, transforms[1].getElem(3, 0), EPSILON);
    ASSERT_NEAR(2.0f, transforms[1].getElem(3, 1), EPSILON);
    ASSERT_NEAR(2.0f, transforms[1].getElem(3, 2), EPSILON);

    dmGui::DeleteScene(scene);

    dmGui::DeleteScript(script);
}

TEST_F(dmGuiScriptTest, TestCustomEasingAnimation)
{
	dmGui::HScript script = NewScript(m_Context);

	dmGui::NewSceneParams params;
	params.m_MaxNodes = 64;
	params.m_MaxAnimations = 32;
	params.m_UserData = this;
    //params.m_RigContext = m_RigContext;

	dmGui::HScene scene = dmGui::NewScene(m_Context, &params);
	dmGui::SetSceneResolution(scene, 1, 1);
	dmGui::SetSceneScript(scene, script);

	// Create custom curve from vmath.vector and pass along to gui.animate
	const char* src =
			"function init(self)\n"
			"    local n1 = gui.new_box_node(vmath.vector3(1, 1, 1), vmath.vector3(1, 1, 1))\n"
			"    gui.set_pivot(n1, gui.PIVOT_SW)\n"
			"    gui.set_position(n1, vmath.vector3(0, 0, 0))\n"
			"    local curve = vmath.vector( {0.0, 0.1, 0.2, 0.3, 0.4, 0.5, 0.6, 0.7, 0.8, 0.9, 1.0} );\n"
			"    gui.animate(n1, gui.PROP_POSITION, vmath.vector3(1, 1, 1), curve, 1)\n"
			"end\n";

	dmGui::Result result = SetScript(script, LuaSourceFromStr(src));
	ASSERT_EQ(dmGui::RESULT_OK, result);

	result = dmGui::InitScene(scene);
	ASSERT_EQ(dmGui::RESULT_OK, result);

	dmVMath::Matrix4 transform;
	dmGui::RenderScene(scene, m_RenderParams, &transform);

	ASSERT_NEAR(0.0f, transform.getElem(3, 0), EPSILON);

	for (int i = 0; i < 60; ++i)
	{
		dmGui::RenderScene(scene, m_RenderParams, &transform);
		ASSERT_NEAR((float)i / 60.0f, transform.getElem(3, 0), EPSILON);
		dmGui::UpdateScene(scene, 1.0f / 60.0f);
	}

	dmGui::DeleteScene(scene);

	dmGui::DeleteScript(script);
}

TEST_F(dmGuiScriptTest, TestCancelAnimation)
{
    dmGui::HScript script = NewScript(m_Context);

    dmGui::NewSceneParams params;
    params.m_MaxNodes = 64;
    params.m_MaxAnimations = 32;
    params.m_UserData = this;
    //params.m_RigContext = m_RigContext;
    dmGui::HScene scene = dmGui::NewScene(m_Context, &params);
    dmGui::SetSceneResolution(scene, 1, 1);
    dmGui::SetSceneScript(scene, script);

    // Set position
    const char* src =
            "local n1\n"
            "local elapsed = 0\n"
            "local animating = true\n"
            "function init(self)\n"
            "    n1 = gui.new_box_node(vmath.vector3(1, 1, 1), vmath.vector3(1, 1, 1))\n"
            "    gui.set_pivot(n1, gui.PIVOT_SW)\n"
            "    gui.set_position(n1, vmath.vector3(0, 0, 0))\n"
            "    gui.animate(n1, gui.PROP_SCALE, vmath.vector3(2, 2, 2), gui.EASING_LINEAR, 1, 0, nil, gui.PLAYBACK_LOOP_FORWARD)\n"
            "end\n"
            "function update(self, dt)\n"
            "    local scale = gui.get_scale(n1)\n"
            "    elapsed = elapsed + dt\n"
            "    if 0.5 <= elapsed and animating then\n"
            "        gui.cancel_animations(n1, gui.PROP_SCALE)\n"
            "        animating = false\n"
            "    end\n"
            "end\n";

    dmGui::Result result = SetScript(script, LuaSourceFromStr(src));
    ASSERT_EQ(dmGui::RESULT_OK, result);

    result = dmGui::InitScene(scene);
    ASSERT_EQ(dmGui::RESULT_OK, result);

    int ticks = 0;
    dmVMath::Matrix4 t1;
    while (ticks < 4) {
        dmGui::RenderScene(scene, m_RenderParams, &t1);
        dmGui::UpdateScene(scene, 0.125f);
        ++ticks;
    }
    dmVMath::Vector3 postScaleDiagonal = Vector3(t1[0][0], t1[1][1], t1[2][2]);

    dmVMath::Matrix4 t2;
    const float tinyDifference = 10e-10f;
    while (ticks < 8) {
        dmGui::RenderScene(scene, m_RenderParams, &t1);
        dmGui::UpdateScene(scene, 0.125f);
        dmVMath::Vector3 currentDiagonal = Vector3(t1[0][0], t1[1][1], t1[2][2]);
        if (tinyDifference < dmVMath::LengthSqr(currentDiagonal - postScaleDiagonal)) {
            char animatedScale[64];
            char currentScale[64];
            dmSnPrintf(animatedScale, sizeof(animatedScale), "(%f,%f,%f)", postScaleDiagonal[0], postScaleDiagonal[1], postScaleDiagonal[2]);
            dmSnPrintf(currentScale, sizeof(currentScale), "(%f,%f,%f)", currentDiagonal[0], currentDiagonal[1], currentDiagonal[2]);
            EXPECT_STREQ(animatedScale, currentScale);
        }
        ++ticks;
    }

    dmGui::DeleteScene(scene);
    dmGui::DeleteScript(script);
}

TEST_F(dmGuiScriptTest, TestCancelAnimationComponent)
{
    dmGui::HScript script = NewScript(m_Context);

    dmGui::NewSceneParams params;
    params.m_MaxNodes = 64;
    params.m_MaxAnimations = 32;
    params.m_UserData = this;
    //params.m_RigContext = m_RigContext;
    dmGui::HScene scene = dmGui::NewScene(m_Context, &params);
    dmGui::SetSceneResolution(scene, 1, 1);
    dmGui::SetSceneScript(scene, script);

    // Set position
    const char* src =
            "local n1\n"
            "local elapsed = 0\n"
            "local animating = true\n"
            "function init(self)\n"
            "    n1 = gui.new_box_node(vmath.vector3(1, 1, 1), vmath.vector3(1, 1, 1))\n"
            "    gui.set_pivot(n1, gui.PIVOT_SW)\n"
            "    gui.set_position(n1, vmath.vector3(0, 0, 0))\n"
            "    gui.animate(n1, gui.PROP_SCALE, vmath.vector3(2, 2, 2), gui.EASING_LINEAR, 1, 0, nil, gui.PLAYBACK_LOOP_FORWARD)\n"
            "end\n"
            "function update(self, dt)\n"
            "    local scale = gui.get_scale(n1)\n"
            "    elapsed = elapsed + dt\n"
            "    if 0.5 <= elapsed and animating then\n"
            "        gui.cancel_animations(n1, \"scale.y\")\n"
            "        animating = false\n"
            "    end\n"
            "end\n";

    dmGui::Result result = SetScript(script, LuaSourceFromStr(src));
    ASSERT_EQ(dmGui::RESULT_OK, result);

    result = dmGui::InitScene(scene);
    ASSERT_EQ(dmGui::RESULT_OK, result);

    int ticks = 0;
    dmVMath::Matrix4 t1;
    while (ticks < 4) {
        dmGui::RenderScene(scene, m_RenderParams, &t1);
        dmGui::UpdateScene(scene, 0.125f);
        ++ticks;
    }
    dmVMath::Vector3 postScaleDiagonal = Vector3(t1[0][0], t1[1][1], t1[2][2]);

    dmVMath::Matrix4 t2;
    const float tinyDifference = 10e-10f;
    while (ticks < 8) {
        dmGui::RenderScene(scene, m_RenderParams, &t1);
        dmGui::UpdateScene(scene, 0.125f);
        dmVMath::Vector3 currentDiagonal = Vector3(t1[0][0], t1[1][1], t1[2][2]);
        dmVMath::Vector3 difference = dmVMath::AbsPerElem(currentDiagonal - postScaleDiagonal);
        if ( (tinyDifference >= difference[0]) || (tinyDifference < difference[1]) || (tinyDifference >= difference[2])) {
            char animatedScale[64];
            char currentScale[64];
            ::dmSnPrintf(animatedScale, sizeof(animatedScale), "(%f,%f,%f)", postScaleDiagonal[0], postScaleDiagonal[1], postScaleDiagonal[2]);
            ::dmSnPrintf(currentScale, sizeof(currentScale), "(%f,%f,%f)", currentDiagonal[0], currentDiagonal[1], currentDiagonal[2]);
            EXPECT_STREQ(animatedScale, currentScale);
        }
        ++ticks;
    }

    dmGui::DeleteScene(scene);
    dmGui::DeleteScript(script);
}

TEST_F(dmGuiScriptTest, TestCancelAnimationAll)
{
    dmGui::HScript script = NewScript(m_Context);

    dmGui::NewSceneParams params;
    params.m_MaxNodes = 64;
    params.m_MaxAnimations = 32;
    params.m_UserData = this;
    dmGui::HScene scene = dmGui::NewScene(m_Context, &params);
   	dmGui::SetSceneResolution(scene, 1, 1);
    dmGui::SetSceneScript(scene, script);

    // Animate position and scale

    // Update for .5 seconds
    const int num_steps = 8;
    const int num_steps_half = num_steps/2;
    const float step_dt = 1.0f / num_steps;

    char script_buffer[1024];
    dmSnPrintf(script_buffer, sizeof(script_buffer),
            "local n1\n"
            "local elapsed = 0\n"
            "local animating = true\n"
            "local max_frame_count = %d\n"
            "local trigger_frame = %d\n"
            "function init(self)\n"
            "    n1 = gui.new_box_node(vmath.vector3(0), vmath.vector3(1))\n"
            "    gui.set_pivot(n1, gui.PIVOT_SW)\n"
            "    gui.animate(n1, gui.PROP_POSITION, vmath.vector3(100, 100, 0), gui.EASING_LINEAR, 1)\n"
            "    gui.animate(n1, gui.PROP_SCALE, vmath.vector3(2), gui.EASING_LINEAR, 1)\n"
            "    self.frame = 0\n"
            "end\n"
            "function update(self, dt)\n"
            "    self.frame = self.frame + 1\n"
            "    if self.frame == trigger_frame and animating then\n"
            "        print('cancel animations on frame', self.frame)\n"
            "        gui.cancel_animations(n1)\n"
            "        animating = false\n"
            "    end\n"
            "end\n", num_steps, num_steps_half);

    dmGui::Result result = SetScript(script, LuaSourceFromStr(script_buffer));
    ASSERT_EQ(dmGui::RESULT_OK, result);

    result = dmGui::InitScene(scene);
    ASSERT_EQ(dmGui::RESULT_OK, result);

    int ticks = 0;
    dmVMath::Matrix4 t1;
    while (ticks < num_steps_half) {
        dmGui::RenderScene(scene, m_RenderParams, &t1);
        dmGui::UpdateScene(scene, step_dt);
        ++ticks;
    }

    // after half the steps, we should have cancelled the animations
    // so store the current positions, to compare with later...
    dmVMath::Vector3 translation = t1.getTranslation();
    dmVMath::Vector3 postScaleDiagonal = Vector3(t1[0][0], t1[1][1], t1[2][2]);

    const float epsilon = 10e-10f;
    while (ticks < num_steps) {
        dmGui::RenderScene(scene, m_RenderParams, &t1);
        dmGui::UpdateScene(scene, step_dt);

        dmVMath::Vector3 currentTranslation = t1.getTranslation();
        dmVMath::Vector3 currentDiagonal = Vector3(t1[0][0], t1[1][1], t1[2][2]);

        // Make sure that the values don't change after the animations were cancelled
        ASSERT_LE(Vectormath::Aos::lengthSqr(currentTranslation - translation), epsilon);
        ASSERT_LE(Vectormath::Aos::lengthSqr(currentDiagonal - postScaleDiagonal), epsilon);

        ++ticks;
    }

    dmGui::DeleteScene(scene);
    dmGui::DeleteScript(script);
}

TEST_F(dmGuiScriptTest, TestInstanceContext)
{
    lua_State* L = dmGui::GetLuaState(m_Context);

    dmGui::NewSceneParams params;
    params.m_MaxNodes = 64;
    params.m_MaxAnimations = 32;
    params.m_UserData = this;
    dmGui::HScene scene = dmGui::NewScene(m_Context, &params);

    dmGui::InitScene(scene);

    lua_rawgeti(L, LUA_REGISTRYINDEX, scene->m_InstanceReference);
    dmScript::SetInstance(L);

    ASSERT_TRUE(dmScript::IsInstanceValid(L));

    lua_pushstring(L, "__my_context_value");
    lua_pushnumber(L, 81233);
    ASSERT_TRUE(dmScript::SetInstanceContextValue(L));

    lua_pushstring(L, "__my_context_value");
    dmScript::GetInstanceContextValue(L);
    ASSERT_EQ(81233, lua_tonumber(L, -1));
    lua_pop(L, 1);

    dmGui::DeleteScene(scene);

    lua_pushnil(L);
    dmScript::SetInstance(L);
    ASSERT_FALSE(dmScript::IsInstanceValid(L));
}

TEST_F(dmGuiScriptTest, TestVisibilityApi)
{
    dmGui::HScript script = NewScript(m_Context);

    dmGui::NewSceneParams params;
    params.m_MaxNodes = 64;
    params.m_MaxAnimations = 32;
    params.m_UserData = this;
    dmGui::HScene scene = dmGui::NewScene(m_Context, &params);
    dmGui::SetSceneScript(scene, script);

    const char* src =
            "function init(self)\n"
            "    local parent = gui.new_box_node(vmath.vector3(1, 1, 1), vmath.vector3(1, 1, 1))\n"
            "    local child = gui.new_box_node(vmath.vector3(1, 1, 1), vmath.vector3(1, 1, 1))\n"
            "    gui.set_parent(child, parent)\n"
            "    gui.set_visible(parent, false)\n"
            "    assert(gui.get_visible(parent) == false)\n"
            "    assert(gui.get_visible(child) == true)\n"
            "end\n";

    dmGui::Result result = SetScript(script, LuaSourceFromStr(src));
    ASSERT_EQ(dmGui::RESULT_OK, result);

    result = dmGui::InitScene(scene);
    ASSERT_EQ(dmGui::RESULT_OK, result);

    dmGui::DeleteScene(scene);

    dmGui::DeleteScript(script);
}

TEST_F(dmGuiScriptTest, TestGuiGetSet)
{
    dmHashEnableReverseHash(true);

    dmGui::HScript script = NewScript(m_Context);

    dmGui::NewSceneParams params;
    params.m_MaxNodes = 64;
    params.m_MaxAnimations = 32;
    params.m_UserData = this;
    dmGui::HScene scene = dmGui::NewScene(m_Context, &params);
    dmGui::SetSceneScript(scene, script);

    const char* src =
            "local EPSILON = 0.001\n"
            "function assert_near(a,b)\n"
            "    assert(math.abs(a-b) < EPSILON)\n"
            "end\n"
            "function check_near_vector4(a,b)\n"
            "    assert(math.abs(a.x-b.x) < EPSILON)\n"
            "    assert(math.abs(a.y-b.y) < EPSILON)\n"
            "    assert(math.abs(a.z-b.z) < EPSILON)\n"
            "    assert(math.abs(a.w-b.w) < EPSILON)\n"
            "end\n"
            "function check_near_vector3(a,b)\n"
            "    assert(math.abs(a.x-b.x) < EPSILON)\n"
            "    assert(math.abs(a.y-b.y) < EPSILON)\n"
            "    assert(math.abs(a.z-b.z) < EPSILON)\n"
            "end\n"
            "function assert_near_vector4(a,b)\n"
            "    local ok = pcall(check_near_vector4, a, b)"
            "    if ok then return end"
            "    print(\"Wanted\", a, \"but got\", b)\n"
            "    assert(false)\n"
            "end\n"
            "function assert_near_vector3(a,b)\n"
            "    local ok = pcall(check_near_vector3, a, b)"
            "    if ok then return end"
            "    print(\"Wanted\", a, \"but got\", b)\n"
            "    assert(false)\n"
            "end\n"
            "local function assert_error(func)\n"
            "    local r, err = pcall(func)\n"
            "    if not r then\n"
            "        print(err)\n"
            "    end\n"
            "    assert(not r)\n"
            "end\n"
            "function init(self)\n"
            "   local node = gui.new_box_node(vmath.vector3(1, 2, 3), vmath.vector3(4, 5, 6))\n"
            // Test valid
            "   assert_near_vector4(vmath.vector4(1, 2, 3, 1), gui.get(node, 'position'))\n"
            "   assert_near_vector4(vmath.vector4(4, 5, 6, 0), gui.get(node, 'size'))\n"
            "   gui.set(node, 'position', vmath.vector3(99, 98, 97))\n"
            "   assert_near_vector4(vmath.vector4(99, 98, 97, 1), gui.get(node, 'position'))\n"
            "   gui.set(node, 'position', vmath.vector4(99, 98, 97, 1337))\n"
            "   assert_near_vector4(vmath.vector4(99, 98, 97, 1337), gui.get(node, 'position'))\n"
            // Test valid subcomponents
            "   gui.set(node, 'position.x', 2001)\n"
            "   assert_near(2001, gui.get(node, 'position.x'))\n"
            "   gui.set(node, 'position.y', 2002)\n"
            "   assert_near(2002, gui.get(node, 'position.y'))\n"
            "   gui.set(node, 'position.z', 2003)\n"
            "   assert_near(2003, gui.get(node, 'position.z'))\n"
            "   gui.set(node, 'position.w', 2004)\n"
            "   assert_near(2004, gui.get(node, 'position.w'))\n"
            "   assert_near_vector4(vmath.vector4(2001, 2002, 2003, 2004), gui.get(node, 'position'))\n"
            // Test rotation <-> euler conversion
            "   gui.set(node, 'rotation', vmath.quat_rotation_z(math.rad(45)))\n"
            "   assert_near_vector3(vmath.vector4(0, 0, 45, 0), gui.get(node, 'euler'))\n"
            "   assert_near_vector3(vmath.vector4(0, 0, 45, 0), gui.get_euler(node))\n"
            "   gui.set_rotation(node, vmath.quat_rotation_z(math.rad(90)))\n"
            "   assert_near_vector3(vmath.vector4(0, 0, 90, 0), gui.get(node, 'euler'))\n"
            "   assert_near_vector3(vmath.vector4(0, 0, 90, 0), gui.get_euler(node))\n"
            "   gui.set(node, 'euler', vmath.vector3(0, 0, 45))\n"
            "   assert_near_vector4(vmath.quat_rotation_z(math.rad(45)), gui.get(node, 'rotation'))\n"
            "   assert_near_vector4(vmath.quat_rotation_z(math.rad(45)), gui.get_rotation(node))\n"
            "   gui.set_euler(node, vmath.vector3(0, 0, 90))\n"
            "   assert_near_vector4(vmath.quat_rotation_z(math.rad(90)), gui.get(node, 'rotation'))\n"
            "   assert_near_vector4(vmath.quat_rotation_z(math.rad(90)), gui.get_rotation(node))\n"
            // Test rotation <-> euler conversion subcomponents
            "   gui.set(node, 'euler', vmath.vector4())\n"
            "   assert_near_vector4(vmath.vector4(0,0,0,1), gui.get(node, 'rotation'))\n"
            "   assert_near_vector4(vmath.vector4(0,0,0,1), gui.get_rotation(node))\n"
            "   gui.set(node, 'euler.x', 180)\n"
            "   assert_near(1, gui.get(node, 'rotation.x'))\n"
            // Incorrect input for get
            "   assert_error(function() gui.get('invalid', 'position') end)\n"
            "   assert_error(function() gui.get(hash('invalid'), 'position') end)\n"
            "   assert_error(function() gui.get(node, 'this_doesnt_exist') end)\n"
            // Incorrect input for set
            "   assert_error(function() gui.set('invalid', 'position', vmath.vector3()) end)\n"
            "   assert_error(function() gui.set(node, 'position', 'incorrect-string') end)\n"
            "   assert_error(function() gui.set(node, 'this_doesnt_exist', vmath.vector4()) end)\n"
            "   assert_error(function() gui.set(node, 'rotation', vmath.vector3()) end)\n"
            "   assert_error(function() gui.set(node, 'rotation', vmath.vector4()) end)\n"
            "   assert_error(function() gui.set(node, 'rotation', 1) end)\n"
            "   assert_error(function() gui.set(node, 'rotation.x', vmath.vector3()) end)\n"
            "end\n";

    dmGui::Result result = SetScript(script, LuaSourceFromStr(src));
    ASSERT_EQ(dmGui::RESULT_OK, result);

    result = dmGui::InitScene(scene);
    ASSERT_EQ(dmGui::RESULT_OK, result);

    dmGui::DeleteScene(scene);

    dmGui::DeleteScript(script);
}

TEST_F(dmGuiScriptTest, TestGuiAnimateEuler)
{
    dmGui::HScript script = NewScript(m_Context);

    dmGui::NewSceneParams params;
    params.m_MaxNodes = 64;
    params.m_MaxAnimations = 32;
    params.m_UserData = this;
    //params.m_RigContext = m_RigContext;
    dmGui::HScene scene = dmGui::NewScene(m_Context, &params);
    dmGui::SetSceneResolution(scene, 1, 1);
    dmGui::SetSceneScript(scene, script);

    // Set position
    const char* src =
            "function init(self)\n"
            "    local n1 = gui.new_box_node(vmath.vector3(1, 1, 1), vmath.vector3(1, 1, 1))\n"
            "    gui.animate(n1, gui.PROP_EULER, vmath.vector3(2, 3, 4), gui.EASING_LINEAR, 1)\n"
            "end\n";

    dmGui::Result result = SetScript(script, LuaSourceFromStr(src));
    ASSERT_EQ(dmGui::RESULT_OK, result);

    result = dmGui::InitScene(scene);
    ASSERT_EQ(dmGui::RESULT_OK, result);

    dmVMath::Matrix4 transform;
    dmGui::RenderScene(scene, m_RenderParams, &transform);

    {
        dmVMath::Quat q(transform.getUpper3x3());
        dmVMath::Vector3 euler = dmVMath::QuatToEuler(q.getX(), q.getY(), q.getZ(), q.getW());

        ASSERT_NEAR(0.0f, euler.getX(), EPSILON);
        ASSERT_NEAR(0.0f, euler.getY(), EPSILON);
        ASSERT_NEAR(0.0f, euler.getZ(), EPSILON);
    }

    dmGui::UpdateScene(scene, 1.0f);

    dmGui::RenderScene(scene, m_RenderParams, &transform);
    {
        dmVMath::Quat q(transform.getUpper3x3());
        dmVMath::Vector3 euler = dmVMath::QuatToEuler(q.getX(), q.getY(), q.getZ(), q.getW());

        // We use a fairly large epsilon here, as we are doing two conversions back to euler values
        ASSERT_NEAR(2.0f, euler.getX(), 0.01f);
        ASSERT_NEAR(3.0f, euler.getY(), 0.01f);
        ASSERT_NEAR(4.0f, euler.getZ(), 0.01f);
    }
    dmGui::DeleteScene(scene);

    dmGui::DeleteScript(script);
}

static dmGui::HTextureSource DynamicNewTexture(dmGui::HScene scene, const dmhash_t path_hash, uint32_t width, uint32_t height, dmImage::Type type, const void* buffer)
{
    dmGuiScriptTest* self = (dmGuiScriptTest*) scene->m_UserData;
    return (dmGui::HTextureSource) self->m_DynamicTextures.New(path_hash, width, height, type, buffer);
}

static void DynamicDeleteTexture(dmGui::HScene scene, dmhash_t path_hash, dmGui::HTextureSource texture_source)
{
    dmGuiScriptTest* self = (dmGuiScriptTest*) scene->m_UserData;
    self->m_DynamicTextures.Delete(path_hash);
}

static void DynamicSetTextureData(dmGui::HScene scene, dmhash_t path_hash, uint32_t width, uint32_t height, dmImage::Type type, const void* buffer)
{
    dmGuiScriptTest* self = (dmGuiScriptTest*) scene->m_UserData;
    self->m_DynamicTextures.Set(path_hash, width, height, type, buffer);
}

TEST_F(dmGuiScriptTest, TestRecreateDynamicTexture)
{
    dmGui::HScript script = NewScript(m_Context);

    dmGui::NewSceneParams params = {};
    params.m_MaxNodes                      = 64;
    params.m_MaxAnimations                 = 32;
    params.m_UserData                      = this;
    params.m_NewTextureResourceCallback    = DynamicNewTexture;
    params.m_DeleteTextureResourceCallback = DynamicDeleteTexture;
    params.m_SetTextureResourceCallback    = DynamicSetTextureData;

    dmGui::HScene scene = dmGui::NewScene(m_Context, &params);
    dmGui::SetSceneScript(scene, script);

    // Test
    // ----
    // * create a texture with id 'tex'
    // * delete the texture
    // * within the same frame, recreate the texture with the same id but with different params
    // * the texture should have the correct data (test in C world)
    const char* src =
            "function update(self)\n"
            "   local w = 4\n"
            "   local h = 4\n"
            "   local tex_id = 'tex'\n"
            "   local orange_pixel = string.char(0xff) .. string.char(0x80) .. string.char(0x10)\n"
            "   local res = gui.new_texture(tex_id, w, h, 'rgb', string.rep(orange_pixel, w * h))\n"
            "   assert(res)\n"
            "   gui.delete_texture(tex_id)\n"
            "   w = 8\n"
            "   h = 8\n"
            "   res = gui.new_texture(tex_id, w, h, 'rgb', string.rep(orange_pixel, w * h))\n"
            "   assert(res)\n"
            "end\n";

    dmGui::Result result = SetScript(script, LuaSourceFromStr(src));
    ASSERT_EQ(dmGui::RESULT_OK, result);

    result = dmGui::InitScene(scene);
    ASSERT_EQ(dmGui::RESULT_OK, result);

    result = dmGui::UpdateScene(scene, 1.0f / 60);
    ASSERT_EQ(dmGui::RESULT_OK, result);

    dmhash_t path_hash = dmHashString64("tex");

    TestDynamicTexture* tex = m_DynamicTextures.Get(path_hash);
    ASSERT_NE((TestDynamicTexture*) 0, tex);
    ASSERT_EQ(8, tex->m_Width);
    ASSERT_EQ(8, tex->m_Height);

    dmGui::DeleteScene(scene);

    dmGui::DeleteScript(script);
}

TEST_F(dmGuiScriptTest, TestKeyboardFunctions)
{
    dmGui::HScript script = NewScript(m_Context);

    dmGui::NewSceneParams params = {};
    params.m_MaxNodes = 64;
    params.m_MaxAnimations = 32;
    params.m_UserData = this;

    dmGui::HScene scene = dmGui::NewScene(m_Context, &params);
    dmGui::SetSceneScript(scene, script);

    const char* src =
            "function init(self)\n"
            "   gui.show_keyboard(gui.KEYBOARD_TYPE_EMAIL, false)\n"
            "   gui.show_keyboard(gui.KEYBOARD_TYPE_NUMBER_PAD, false)\n"
            "   gui.show_keyboard(gui.KEYBOARD_TYPE_PASSWORD, false)\n"
            "   gui.show_keyboard(gui.KEYBOARD_TYPE_DEFAULT, false)\n"
            "end\n"
            "function update(self)\n"
            "   gui.hide_keyboard()\n"
            "end\n";

    dmGui::Result result = SetScript(script, LuaSourceFromStr(src));
    ASSERT_EQ(dmGui::RESULT_OK, result);

    result = dmGui::InitScene(scene);
    ASSERT_EQ(dmGui::RESULT_OK, result);

    ASSERT_TRUE(dmPlatform::GetDeviceState(m_Window, dmPlatform::DEVICE_STATE_KEYBOARD_DEFAULT));
    ASSERT_FALSE(dmPlatform::GetDeviceState(m_Window, dmPlatform::DEVICE_STATE_KEYBOARD_NUMBER_PAD));
    ASSERT_FALSE(dmPlatform::GetDeviceState(m_Window, dmPlatform::DEVICE_STATE_KEYBOARD_EMAIL));
    ASSERT_FALSE(dmPlatform::GetDeviceState(m_Window, dmPlatform::DEVICE_STATE_KEYBOARD_PASSWORD));

    result = dmGui::UpdateScene(scene, 1.0f / 60);
    ASSERT_EQ(dmGui::RESULT_OK, result);

    ASSERT_FALSE(dmPlatform::GetDeviceState(m_Window, dmPlatform::DEVICE_STATE_KEYBOARD_DEFAULT));
    ASSERT_FALSE(dmPlatform::GetDeviceState(m_Window, dmPlatform::DEVICE_STATE_KEYBOARD_NUMBER_PAD));
    ASSERT_FALSE(dmPlatform::GetDeviceState(m_Window, dmPlatform::DEVICE_STATE_KEYBOARD_EMAIL));
    ASSERT_FALSE(dmPlatform::GetDeviceState(m_Window, dmPlatform::DEVICE_STATE_KEYBOARD_PASSWORD));

    dmGui::DeleteScene(scene);

    dmGui::DeleteScript(script);
}

int main(int argc, char **argv)
{
    TestMainPlatformInit();
    dmDDF::RegisterAllTypes();
    jc_test_init(&argc, argv);
    return jc_test_run_all();
}
