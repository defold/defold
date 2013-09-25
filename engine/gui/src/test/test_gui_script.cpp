#include <gtest/gtest.h>

#include "../gui.h"
#include "../gui_script.h"

extern "C"
{
#include "lua/lua.h"
#include "lua/lauxlib.h"
}

static const float TEXT_GLYPH_WIDTH = 1.0f;
static const float TEXT_MAX_ASCENT = 0.75f;
static const float TEXT_MAX_DESCENT = 0.25f;

void GetTextMetricsCallback(const void* font, const char* text, float width, bool line_break, dmGui::TextMetrics* out_metrics);

class dmGuiScriptTest : public ::testing::Test
{
public:
    dmScript::HContext m_ScriptContext;
    dmGui::HContext m_Context;

    virtual void SetUp()
    {
        m_ScriptContext = dmScript::NewContext(0, 0);

        dmGui::NewContextParams context_params;
        context_params.m_ScriptContext = m_ScriptContext;
        context_params.m_GetTextMetricsCallback = GetTextMetricsCallback;
        m_Context = dmGui::NewContext(&context_params);
    }

    virtual void TearDown()
    {
        dmGui::DeleteContext(m_Context, m_ScriptContext);
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

    const char* src = "local url = msg.url(\"test\")\n";
    dmGui::Result result = SetScript(script, src, strlen(src), "dummy_source");
    ASSERT_EQ(dmGui::RESULT_SCRIPT_ERROR, result);
    DeleteScript(script);
}

TEST_F(dmGuiScriptTest, GetScreenPos)
{
    dmGui::HScript script = NewScript(m_Context);

    dmGui::NewSceneParams params;
    params.m_MaxNodes = 64;
    params.m_MaxAnimations = 32;
    params.m_UserData = this;
    dmGui::HScene scene = dmGui::NewScene(m_Context, &params);
    dmGui::SetSceneScript(scene, script);

    const char* src = "function init(self)\n"
                      "    local p = vmath.vector3(10, 10, 0)\n"
                      "    local s = vmath.vector3(20, 20, 0)\n"
                      "    local n1 = gui.new_box_node(p, s)\n"
                      "    local n2 = gui.new_text_node(p, \"text\")\n"
                      "    gui.set_size(n2, s)\n"
                      "    assert(gui.get_screen_position(n1) == gui.get_screen_position(n2))\n"
                      "    local n3 = gui.new_text_node(p, \"text\")\n"
                      "    gui.set_pivot(n3, gui.PIVOT_NW)\n"
                      "    assert(gui.get_screen_position(n2) == gui.get_screen_position(n3))\n"
                      "end\n";
    dmGui::Result result = SetScript(script, src, strlen(src), "dummy_source");
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

    dmGui::Result result = SetScript(script, src, strlen(src), "dummy_source");
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

int main(int argc, char **argv)
{
    dmDDF::RegisterAllTypes();
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
