#include <gtest/gtest.h>
#include <dlib/dstrings.h>

#include <ddf/ddf.h>
#include <script/lua_source_ddf.h>

#include "../gui.h"
#include "../gui_private.h"
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

void GetTextMetricsCallback(const void* font, const char* text, float width, bool line_break, float leading, float tracking, dmGui::TextMetrics* out_metrics);
bool FetchRigSceneDataCallback(void* spine_scene, dmhash_t rig_scene_id, dmGui::RigSceneDataDesc* out_data);

class dmGuiScriptTest : public ::testing::Test
{
public:
    dmScript::HContext m_ScriptContext;
    dmGui::HContext m_Context;
    dmRig::HRigContext m_RigContext;

    virtual void SetUp()
    {
        dmScript::NewContextParams params;
        m_ScriptContext = dmScript::NewContext(&params);
        dmScript::Initialize(m_ScriptContext);

        dmGui::NewContextParams context_params;
        context_params.m_ScriptContext = m_ScriptContext;
        context_params.m_GetTextMetricsCallback = GetTextMetricsCallback;
        context_params.m_PhysicalWidth = 1;
        context_params.m_PhysicalHeight = 1;
        m_Context = dmGui::NewContext(&context_params);

        dmRig::NewContextParams rig_params = {0};
        rig_params.m_Context = &m_RigContext;
        rig_params.m_MaxRigInstanceCount = 2;
        dmRig::NewContext(rig_params);
    }

    virtual void TearDown()
    {
        dmRig::DeleteContext(m_RigContext);
        dmGui::DeleteContext(m_Context, m_ScriptContext);
        dmScript::Finalize(m_ScriptContext);
        dmScript::DeleteContext(m_ScriptContext);
    }
};

void GetTextMetricsCallback(const void* font, const char* text, float width, bool line_break, float leading, float tracking, dmGui::TextMetrics* out_metrics)
{
    out_metrics->m_Width = strlen(text) * TEXT_GLYPH_WIDTH;
    out_metrics->m_MaxAscent = TEXT_MAX_ASCENT;
    out_metrics->m_MaxDescent = TEXT_MAX_DESCENT;
}

bool FetchRigSceneDataCallback(void* spine_scene, dmhash_t rig_scene_id, dmGui::RigSceneDataDesc* out_data)
{
    if (!spine_scene) {
        return false;
    }

    dmGui::RigSceneDataDesc* spine_scene_ptr = (dmGui::RigSceneDataDesc*)spine_scene;
    out_data->m_BindPose = spine_scene_ptr->m_BindPose;
    out_data->m_Skeleton = spine_scene_ptr->m_Skeleton;
    out_data->m_MeshSet = spine_scene_ptr->m_MeshSet;
    out_data->m_AnimationSet = spine_scene_ptr->m_AnimationSet;

    return true;
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
    params.m_RigContext = m_RigContext;
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
    params.m_RigContext = m_RigContext;
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
    params.m_RigContext = m_RigContext;
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
    params.m_RigContext = m_RigContext;
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
    params.m_RigContext = m_RigContext;
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
    params.m_RigContext = m_RigContext;
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
    params.m_RigContext = m_RigContext;
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
    params.m_RigContext = m_RigContext;
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
    params.m_RigContext = m_RigContext;
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
    params.m_RigContext = m_RigContext;
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
    params.m_RigContext = m_RigContext;
    dmGui::HScene scene = dmGui::NewScene(m_Context, &params);
    dmGui::SetSceneScript(scene, script);

    dmGui::Result result;

    int t1, ts1;
    result = dmGui::AddTexture(scene, "t1", (void*) &t1, (void*) &ts1, 1, 1);
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
    params.m_RigContext = m_RigContext;
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

static void InitializeScriptScene(dmGui::HContext* context,
    dmGui::HScript* script, dmGui::HScene* scene, dmRig::HRigContext rig_context, void* user_data, const char* source)
{
    *script = dmGui::NewScript(*context);
    dmGui::NewSceneParams params;
    params.m_MaxNodes = 64;
    params.m_MaxAnimations = 32;
    params.m_UserData = user_data;
    params.m_RigContext = rig_context;
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
    InitializeScriptScene(&m_Context, &script, &scene, m_RigContext, this, source);
    ASSERT_EQ(0, scene->m_RenderOrder);

    dmGui::DeleteScene(scene);
    dmGui::DeleteScript(script);
}

TEST_F(dmGuiScriptTest, TestSetRenderOrderMaximum)
{
    dmGui::HScript script = NULL;
    dmGui::HScene scene = NULL;

    const char* source = "function init(self)\ngui.set_render_order(15)\nend\n";
    InitializeScriptScene(&m_Context, &script, &scene, m_RigContext, this, source);
    ASSERT_EQ(15, scene->m_RenderOrder);

    dmGui::DeleteScene(scene);
    dmGui::DeleteScript(script);
}

TEST_F(dmGuiScriptTest, TestSetRenderOrderUnderflow)
{
    dmGui::HScript script = NULL;
    dmGui::HScene scene = NULL;

    const char* source = "function init(self)\ngui.set_render_order(-1)\nend\n";
    InitializeScriptScene(&m_Context, &script, &scene, m_RigContext, this, source);
    ASSERT_EQ(0, scene->m_RenderOrder);

    dmGui::DeleteScene(scene);
    dmGui::DeleteScript(script);
}

TEST_F(dmGuiScriptTest, TestSetRenderOrderOverflow)
{
    dmGui::HScript script = NULL;
    dmGui::HScene scene = NULL;

    const char* source = "function init(self)\ngui.set_render_order(16)\nend\n";
    InitializeScriptScene(&m_Context, &script, &scene, m_RigContext, this, source);
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
    params.m_RigContext = m_RigContext;
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

// DEF-1972 Callback called to early (when finishing the first animation, not the last)
TEST_F(dmGuiScriptTest, TestLocalTransformAnimWithCallback)
{
    dmGui::HScript script = NewScript(m_Context);

    dmGui::NewSceneParams params;
    params.m_MaxNodes = 64;
    params.m_MaxAnimations = 32;
    params.m_UserData = this;
    params.m_RigContext = m_RigContext;
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

    Vectormath::Aos::Matrix4 transforms[2];
    dmGui::RenderScene(scene, RenderNodesStoreTransform, transforms);

    ASSERT_NEAR(0.0f, transforms[0].getElem(3, 0), EPSILON);
    ASSERT_NEAR(0.0f, transforms[0].getElem(3, 1), EPSILON);
    ASSERT_NEAR(0.0f, transforms[0].getElem(3, 2), EPSILON);
    ASSERT_NEAR(0.0f, transforms[1].getElem(3, 0), EPSILON);
    ASSERT_NEAR(0.0f, transforms[1].getElem(3, 1), EPSILON);
    ASSERT_NEAR(0.0f, transforms[1].getElem(3, 2), EPSILON);

    dmGui::UpdateScene(scene, 1.0f);

    dmGui::RenderScene(scene, RenderNodesStoreTransform, transforms);

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
    params.m_RigContext = m_RigContext;

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
        params.m_RigContext = m_RigContext;
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
        params.m_RigContext = m_RigContext;
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

static void BuildBindPose(dmArray<dmRig::RigBone>* bind_pose, dmRigDDF::Skeleton* skeleton)
{
    // Calculate bind pose
    // (code from res_rig_scene.h)
    uint32_t bone_count = skeleton->m_Bones.m_Count;
    bind_pose->SetCapacity(bone_count);
    bind_pose->SetSize(bone_count);
    for (uint32_t i = 0; i < bone_count; ++i)
    {
        dmRig::RigBone* bind_bone = &(*bind_pose)[i];
        dmRigDDF::Bone* bone = &skeleton->m_Bones[i];
        bind_bone->m_LocalToParent = dmTransform::Transform(Vector3(bone->m_Position), bone->m_Rotation, bone->m_Scale);
        if (i > 0)
        {
            bind_bone->m_LocalToModel = dmTransform::Mul((*bind_pose)[bone->m_Parent].m_LocalToModel, bind_bone->m_LocalToParent);
            if (!bone->m_InheritScale)
            {
                bind_bone->m_LocalToModel.SetScale(bind_bone->m_LocalToParent.GetScale());
            }
        }
        else
        {
            bind_bone->m_LocalToModel = bind_bone->m_LocalToParent;
        }
        bind_bone->m_ModelToLocal = dmTransform::ToMatrix4(dmTransform::Inv(bind_bone->m_LocalToModel));
        bind_bone->m_ParentIndex = bone->m_Parent;
        bind_bone->m_Length = bone->m_Length;
    }
}

static void CreateSpineDummyData(dmGui::RigSceneDataDesc* dummy_data)
{
    dmRigDDF::Skeleton* skeleton          = new dmRigDDF::Skeleton();
    dmRigDDF::MeshSet* mesh_set           = new dmRigDDF::MeshSet();
    dmRigDDF::AnimationSet* animation_set = new dmRigDDF::AnimationSet();

    uint32_t bone_count = 2;
    skeleton->m_Bones.m_Data = new dmRigDDF::Bone[bone_count];
    skeleton->m_Bones.m_Count = bone_count;

    // Bone 0
    dmRigDDF::Bone& bone0 = skeleton->m_Bones.m_Data[0];
    bone0.m_Parent       = 0xffff;
    bone0.m_Id           = dmHashString64("b0");
    bone0.m_Position     = Vectormath::Aos::Point3(0.0f, 0.0f, 0.0f);
    bone0.m_Rotation     = Vectormath::Aos::Quat::identity();
    bone0.m_Scale        = Vectormath::Aos::Vector3(1.0f, 1.0f, 1.0f);
    bone0.m_InheritScale = false;
    bone0.m_Length       = 0.0f;

    // Bone 1
    dmRigDDF::Bone& bone1 = skeleton->m_Bones.m_Data[1];
    bone1.m_Parent       = 0;
    bone1.m_Id           = dmHashString64("b1");
    bone1.m_Position     = Vectormath::Aos::Point3(1.0f, 0.0f, 0.0f);
    bone1.m_Rotation     = Vectormath::Aos::Quat::identity();
    bone1.m_Scale        = Vectormath::Aos::Vector3(1.0f, 1.0f, 1.0f);
    bone1.m_InheritScale = false;
    bone1.m_Length       = 1.0f;

    dummy_data->m_BindPose = new dmArray<dmRig::RigBone>();
    dummy_data->m_Skeleton = skeleton;
    dummy_data->m_MeshSet = mesh_set;
    dummy_data->m_AnimationSet = animation_set;

    BuildBindPose(dummy_data->m_BindPose, skeleton);
}

static void DeleteSpineDummyData(dmGui::RigSceneDataDesc* dummy_data)
{
    delete dummy_data->m_BindPose;
    delete [] dummy_data->m_Skeleton->m_Bones.m_Data;
    delete dummy_data->m_Skeleton;
    delete dummy_data->m_MeshSet;
    delete dummy_data->m_AnimationSet;
    delete dummy_data;
}

TEST_F(dmGuiScriptTest, SpineScenes)
{
    dmGui::HScript script = NewScript(m_Context);

        dmGui::NewSceneParams params;
        params.m_MaxNodes = 64;
        params.m_MaxAnimations = 32;
        params.m_UserData = this;
        params.m_RigContext = m_RigContext;
        params.m_FetchRigSceneDataCallback = FetchRigSceneDataCallback;
        dmGui::HScene scene = dmGui::NewScene(m_Context, &params);
        dmGui::SetSceneScript(scene, script);

        dmGui::RigSceneDataDesc* dummy_data = new dmGui::RigSceneDataDesc();
        CreateSpineDummyData(dummy_data);

        ASSERT_EQ(dmGui::RESULT_OK, dmGui::AddSpineScene(scene, "test_spine", (void*)dummy_data));

        // Set position
        const char* src =
                "function init(self)\n"
                "    local n = gui.new_spine_node(vmath.vector3(1, 1, 1), \"test_spine\")\n"
                "    gui.set_pivot(n, gui.PIVOT_SW)\n"
                "    gui.set_position(n, vmath.vector3(0, 0, 0))\n"
                "    gui.animate(n, gui.PROP_SCALE, vmath.vector3(2, 2, 2), gui.EASING_LINEAR, 1, 0, nil, gui.PLAYBACK_LOOP_FORWARD)\n"
                "end\n";

        dmGui::Result result = SetScript(script, LuaSourceFromStr(src));
        ASSERT_EQ(dmGui::RESULT_OK, result);

        result = dmGui::InitScene(scene);
        ASSERT_EQ(dmGui::RESULT_OK, result);

        dmGui::DeleteScene(scene);
        dmGui::DeleteScript(script);

        DeleteSpineDummyData(dummy_data);
}

TEST_F(dmGuiScriptTest, DeleteSpine)
{
    dmGui::HScript script = NewScript(m_Context);

        dmGui::NewSceneParams params;
        params.m_MaxNodes = 64;
        params.m_MaxAnimations = 32;
        params.m_UserData = this;
        params.m_RigContext = m_RigContext;
        params.m_FetchRigSceneDataCallback = FetchRigSceneDataCallback;
        dmGui::HScene scene = dmGui::NewScene(m_Context, &params);
        dmGui::SetSceneScript(scene, script);

        dmGui::RigSceneDataDesc* dummy_data = new dmGui::RigSceneDataDesc();
        CreateSpineDummyData(dummy_data);

        ASSERT_EQ(dmGui::RESULT_OK, dmGui::AddSpineScene(scene, "test_spine", (void*)dummy_data));

        // Set position
        const char* src =
                "function init(self)\n"
                "    n = gui.new_spine_node(vmath.vector3(1, 1, 1), \"test_spine\")\n"
                "    gui.set_pivot(n, gui.PIVOT_SW)\n"
                "end\n"
                "function update(self, dt)\n"
                "    -- produces a lua error since the spine nodes has been deleted\n"
                "    gui.get_position(n)\n"
                "    gui.delete_node(n)\n"
                "end\n";

        dmGui::Result result = SetScript(script, LuaSourceFromStr(src));
        ASSERT_EQ(dmGui::RESULT_OK, result);

        result = dmGui::InitScene(scene);
        ASSERT_EQ(dmGui::RESULT_OK, result);

        result = dmGui::UpdateScene(scene, 1.0f / 60);
        ASSERT_EQ(dmGui::RESULT_OK, result);

        result = dmGui::UpdateScene(scene, 1.0f / 60);
        ASSERT_NE(dmGui::RESULT_OK, result);

        dmGui::DeleteScene(scene);
        dmGui::DeleteScript(script);

        DeleteSpineDummyData(dummy_data);
}

TEST_F(dmGuiScriptTest, DeleteBone)
{
    dmGui::HScript script = NewScript(m_Context);

        dmGui::NewSceneParams params;
        params.m_MaxNodes = 64;
        params.m_MaxAnimations = 32;
        params.m_UserData = this;
        params.m_RigContext = m_RigContext;
        params.m_FetchRigSceneDataCallback = FetchRigSceneDataCallback;
        dmGui::HScene scene = dmGui::NewScene(m_Context, &params);
        dmGui::SetSceneScript(scene, script);

        dmGui::RigSceneDataDesc* dummy_data = new dmGui::RigSceneDataDesc();
        CreateSpineDummyData(dummy_data);

        ASSERT_EQ(dmGui::RESULT_OK, dmGui::AddSpineScene(scene, "test_spine", (void*)dummy_data));

        // Set position
        const char* src =
                "function init(self)\n"
                "    n = gui.new_spine_node(vmath.vector3(1, 1, 1), \"test_spine\")\n"
                "    gui.set_pivot(n, gui.PIVOT_SW)\n"
                "    bone_node = gui.get_spine_bone(n, \"b1\")\n"
                "end\n"
                "function update(self, dt)\n"
                "    -- produces a lua error since bone nodes cannot be deleted on their own\n"
                "    gui.get_position(bone_node)\n"
                "    gui.delete_node(bone_node)\n"
                "end\n";

        dmGui::Result result = SetScript(script, LuaSourceFromStr(src));
        ASSERT_EQ(dmGui::RESULT_OK, result);

        result = dmGui::InitScene(scene);
        ASSERT_EQ(dmGui::RESULT_OK, result);

        result = dmGui::UpdateScene(scene, 1.0f / 60);
        ASSERT_NE(dmGui::RESULT_OK, result);

        dmGui::DeleteScene(scene);
        dmGui::DeleteScript(script);

        DeleteSpineDummyData(dummy_data);
}

TEST_F(dmGuiScriptTest, SetBoneNodeProperties)
{
    dmGui::HScript script = NewScript(m_Context);

        dmGui::NewSceneParams params;
        params.m_MaxNodes = 64;
        params.m_MaxAnimations = 32;
        params.m_UserData = this;
        params.m_RigContext = m_RigContext;
        params.m_FetchRigSceneDataCallback = FetchRigSceneDataCallback;
        dmGui::HScene scene = dmGui::NewScene(m_Context, &params);
        dmGui::SetSceneScript(scene, script);

        dmGui::RigSceneDataDesc* dummy_data = new dmGui::RigSceneDataDesc();
        CreateSpineDummyData(dummy_data);

        ASSERT_EQ(dmGui::RESULT_OK, dmGui::AddSpineScene(scene, "test_spine", (void*)dummy_data));

        // Set position
        const char* src =
                "function init(self)\n"
                "    spine_node = gui.new_spine_node(vmath.vector3(1, 1, 1), \"test_spine\")\n"
                "    fake_parent = gui.new_box_node(vmath.vector3(1, 1, 1),vmath.vector3(1, 1, 1))\n"
                "\n"
                "    bone_node = gui.get_spine_bone(spine_node, \"b1\")\n"
                "    gui.set_position(bone_node, vmath.vector3(100, 100, 0))\n"
                "    gui.set_size(bone_node, vmath.vector3(100, 100, 100))\n"
                "    gui.set_scale(bone_node, vmath.vector3(100, 100, 100))\n"
                "    gui.set_scale(bone_node, vmath.vector3(100, 100, 100))\n"
                "    gui.set_parent(bone_node, fake_parent)\n"
                "    gui.set_spine_scene(bone_node, \"test_spine\")\n"
                "end\n"
                "\n"
                "function update(self, dt)\n"
                "    -- properties should not have changed since it's not possible to set them directly for bones\n"
                "    assert(gui.get_position(bone_node).x == 1.0)\n"
                "    assert(gui.get_size(bone_node).x == 0.0)\n"
                "    assert(gui.get_scale(bone_node).x == 1.0)\n"
                "    assert(gui.get_parent(bone_node) ~= fake_parent)\n"
                "    assert(gui.get_spine_scene(bone_node) == hash(\"\"))\n"
                "end\n";

        dmGui::Result result = SetScript(script, LuaSourceFromStr(src));
        ASSERT_EQ(dmGui::RESULT_OK, result);

        result = dmGui::InitScene(scene);
        ASSERT_EQ(dmGui::RESULT_OK, result);

        result = dmGui::UpdateScene(scene, 1.0f / 60);
        ASSERT_EQ(dmGui::RESULT_OK, result);

        result = dmGui::UpdateScene(scene, 1.0f / 60);
        ASSERT_EQ(dmGui::RESULT_OK, result);

        dmGui::DeleteScene(scene);
        dmGui::DeleteScript(script);

        DeleteSpineDummyData(dummy_data);
}

int main(int argc, char **argv)
{
    dmDDF::RegisterAllTypes();
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
