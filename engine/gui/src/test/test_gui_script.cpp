#include <gtest/gtest.h>

#include "../gui.h"
#include "../gui_script.h"

extern "C"
{
#include "lua/lua.h"
#include "lua/lauxlib.h"
}

class dmGuiScriptTest : public ::testing::Test
{
public:
    dmGui::HContext m_Context;

    virtual void SetUp()
    {
        dmGui::NewContextParams params;
        m_Context = dmGui::NewContext(&params);
    }

    virtual void TearDown()
    {
        dmGui::DeleteContext(m_Context, 0);
    }
};

TEST_F(dmGuiScriptTest, URLOutsideFunctions)
{
    dmGui::HScript script = NewScript(m_Context);

    const char* src = "local url = msg.url(\"test\")\n";
    dmGui::Result result = SetScript(script, src, strlen(src), "dummy_source");
    ASSERT_EQ(dmGui::RESULT_SCRIPT_ERROR, result);
    DeleteScript(script);
}

int main(int argc, char **argv)
{
    dmDDF::RegisterAllTypes();
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
