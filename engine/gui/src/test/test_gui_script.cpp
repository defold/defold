#include <gtest/gtest.h>
#include <dlib/dstrings.h>

#include <ddf/ddf.h>
#include <script/lua_source_ddf.h>

#include "../gui.h"
#include "../gui_script.h"

extern "C"
{
#include "lua/lua.h"
#include "lua/lauxlib.h"
}

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

void GetTextMetricsCallback(const void* font, const char* text, float width, bool line_break, dmGui::TextMetrics* out_metrics);

class dmGuiScriptTest : public ::testing::Test
{
public:
    dmScript::HContext m_ScriptContext;
    dmGui::HContext m_Context;

    virtual void SetUp()
    {
        m_ScriptContext = dmScript::NewContext(0, 0);
        dmScript::Initialize(m_ScriptContext);

        dmGui::NewContextParams context_params;
        context_params.m_ScriptContext = m_ScriptContext;
        context_params.m_GetTextMetricsCallback = GetTextMetricsCallback;
        context_params.m_PhysicalWidth = 1;
        context_params.m_PhysicalHeight = 1;
        m_Context = dmGui::NewContext(&context_params);
    }

    virtual void TearDown()
    {
        dmGui::DeleteContext(m_Context, m_ScriptContext);
        dmScript::Finalize(m_ScriptContext);
        dmScript::DeleteContext(m_ScriptContext);
    }
};

void GetTextMetricsCallback(const void* font, const char* text, float width, bool line_break, dmGui::TextMetrics* out_metrics)
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
    *ref = luaL_ref(L, LUA_REGISTRYINDEX);
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

TEST_F(dmGuiScriptTest, TestGetIndex)
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

TEST_F(dmGuiScriptTest, TestCloneTree)
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

TEST_F(dmGuiScriptTest, TestPieNodeScript)
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

TEST_F(dmGuiScriptTest, TestSlice9)
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

void RenderNodesStoreTransform(dmGui::HScene scene, const dmGui::RenderEntry* nodes, const Vectormath::Aos::Matrix4* node_transforms, const float* node_opacities,
        const dmGui::StencilScope** stencil_scopes, uint32_t node_count, void* context)
{
    Vectormath::Aos::Matrix4* out_transforms = (Vectormath::Aos::Matrix4*)context;
    memcpy(out_transforms, node_transforms, sizeof(Vectormath::Aos::Matrix4) * node_count);
}

TEST_F(dmGuiScriptTest, TestLocalTransformSetPos)
{
    dmGui::HScript script = NewScript(m_Context);

    dmGui::NewSceneParams params;
    params.m_MaxNodes = 64;
    params.m_MaxAnimations = 32;
    params.m_UserData = this;
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

    Vectormath::Aos::Matrix4 transform;
    dmGui::RenderScene(scene, RenderNodesStoreTransform, &transform);

    ASSERT_NEAR(2.0f, transform.getElem(3, 0), EPSILON);
    ASSERT_NEAR(2.0f, transform.getElem(3, 1), EPSILON);
    ASSERT_NEAR(2.0f, transform.getElem(3, 2), EPSILON);

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

    Vectormath::Aos::Matrix4 transform;
    dmGui::RenderScene(scene, RenderNodesStoreTransform, &transform);

    ASSERT_NEAR(0.0f, transform.getElem(3, 0), EPSILON);
    ASSERT_NEAR(0.0f, transform.getElem(3, 1), EPSILON);
    ASSERT_NEAR(0.0f, transform.getElem(3, 2), EPSILON);

    dmGui::UpdateScene(scene, 1.0f);

    dmGui::RenderScene(scene, RenderNodesStoreTransform, &transform);

    ASSERT_NEAR(2.0f, transform.getElem(3, 0), EPSILON);
    ASSERT_NEAR(2.0f, transform.getElem(3, 1), EPSILON);
    ASSERT_NEAR(2.0f, transform.getElem(3, 2), EPSILON);

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

	Vectormath::Aos::Matrix4 transform;
	dmGui::RenderScene(scene, RenderNodesStoreTransform, &transform);

	ASSERT_NEAR(0.0f, transform.getElem(3, 0), EPSILON);

	for (int i = 0; i < 60; ++i)
	{
		dmGui::RenderScene(scene, RenderNodesStoreTransform, &transform);
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
        dmGui::HScene scene = dmGui::NewScene(m_Context, &params);
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
                "        gui.cancel_animation(n1, gui.PROP_SCALE)\n"
                "        animating = false\n"
                "    end\n"
                "end\n";

        dmGui::Result result = SetScript(script, LuaSourceFromStr(src));
        ASSERT_EQ(dmGui::RESULT_OK, result);

        result = dmGui::InitScene(scene);
        ASSERT_EQ(dmGui::RESULT_OK, result);

        int ticks = 0;
        Vectormath::Aos::Matrix4 t1;
        while (ticks < 4) {
            dmGui::RenderScene(scene, RenderNodesStoreTransform, &t1);
            dmGui::UpdateScene(scene, 0.125f);
            ++ticks;
        }
        Vectormath::Aos::Vector3 postScaleDiagonal = Vector3(t1[0][0], t1[1][1], t1[2][2]);

        Vectormath::Aos::Matrix4 t2;
        const float tinyDifference = 10e-10f;
        while (ticks < 8) {
            dmGui::RenderScene(scene, RenderNodesStoreTransform, &t1);
            dmGui::UpdateScene(scene, 0.125f);
            Vectormath::Aos::Vector3 currentDiagonal = Vector3(t1[0][0], t1[1][1], t1[2][2]);
            if (tinyDifference < Vectormath::Aos::lengthSqr(currentDiagonal - postScaleDiagonal)) {
                char animatedScale[64];
                char currentScale[64];
                DM_SNPRINTF(animatedScale, sizeof(animatedScale), "(%f,%f,%f)", postScaleDiagonal[0], postScaleDiagonal[1], postScaleDiagonal[2]);
                DM_SNPRINTF(currentScale, sizeof(currentScale), "(%f,%f,%f)", currentDiagonal[0], currentDiagonal[1], currentDiagonal[2]);
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
        dmGui::HScene scene = dmGui::NewScene(m_Context, &params);
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
                "        gui.cancel_animation(n1, \"scale.y\")\n"
                "        animating = false\n"
                "    end\n"
                "end\n";

        dmGui::Result result = SetScript(script, LuaSourceFromStr(src));
        ASSERT_EQ(dmGui::RESULT_OK, result);

        result = dmGui::InitScene(scene);
        ASSERT_EQ(dmGui::RESULT_OK, result);

        int ticks = 0;
        Vectormath::Aos::Matrix4 t1;
        while (ticks < 4) {
            dmGui::RenderScene(scene, RenderNodesStoreTransform, &t1);
            dmGui::UpdateScene(scene, 0.125f);
            ++ticks;
        }
        Vectormath::Aos::Vector3 postScaleDiagonal = Vector3(t1[0][0], t1[1][1], t1[2][2]);

        Vectormath::Aos::Matrix4 t2;
        const float tinyDifference = 10e-10f;
        while (ticks < 8) {
            dmGui::RenderScene(scene, RenderNodesStoreTransform, &t1);
            dmGui::UpdateScene(scene, 0.125f);
            Vectormath::Aos::Vector3 currentDiagonal = Vector3(t1[0][0], t1[1][1], t1[2][2]);
            Vectormath::Aos::Vector3 difference = Vectormath::Aos::absPerElem(currentDiagonal - postScaleDiagonal);
            if ( (tinyDifference >= difference[0]) || (tinyDifference < difference[1]) || (tinyDifference >= difference[2])) {
                char animatedScale[64];
                char currentScale[64];
                ::DM_SNPRINTF(animatedScale, sizeof(animatedScale), "(%f,%f,%f)", postScaleDiagonal[0], postScaleDiagonal[1], postScaleDiagonal[2]);
                ::DM_SNPRINTF(currentScale, sizeof(currentScale), "(%f,%f,%f)", currentDiagonal[0], currentDiagonal[1], currentDiagonal[2]);
                EXPECT_STREQ(animatedScale, currentScale);
            }
            ++ticks;
        }

        dmGui::DeleteScene(scene);
        dmGui::DeleteScript(script);
}

int main(int argc, char **argv)
{
    dmDDF::RegisterAllTypes();
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
