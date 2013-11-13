#include <gtest/gtest.h>

#include "script.h"

#include <dlib/dstrings.h>
#include <dlib/hash.h>
#include <dlib/log.h>
#include <dlib/configfile.h>

extern "C"
{
#include <lua/lauxlib.h>
#include <lua/lualib.h>
}

#define PATH_FORMAT "build/default/src/test/%s"

class ScriptTest : public ::testing::Test
{
protected:
    virtual void SetUp()
    {
        m_Context = dmScript::NewContext(0x0, 0);

        L = lua_open();
        luaL_openlibs(L);
        dmScript::ScriptParams params;
        params.m_Context = m_Context;
        dmScript::Initialize(L, params);
    }

    virtual void TearDown()
    {
        dmScript::Finalize(L, m_Context);
        dmScript::DeleteContext(m_Context);
        lua_close(L);
    }

    dmScript::HContext m_Context;
    lua_State* L;
};

bool RunFile(lua_State* L, const char* filename)
{
    char path[64];
    DM_SNPRINTF(path, 64, PATH_FORMAT, filename);
    if (luaL_dofile(L, path) != 0)
    {
        dmLogError("%s", lua_tolstring(L, -1, 0));
        return false;
    }
    return true;
}

bool RunString(lua_State* L, const char* script)
{
    if (luaL_dostring(L, script) != 0)
    {
        dmLogError("%s", lua_tolstring(L, -1, 0));
        return false;
    }
    return true;
}

TEST_F(ScriptTest, TestPrint)
{
    int top = lua_gettop(L);
    ASSERT_TRUE(RunString(L, "print(\"test print\")"));
    ASSERT_TRUE(RunString(L, "print(\"test\", \"multiple\")"));

    ASSERT_EQ(top, lua_gettop(L));
}

TEST_F(ScriptTest, TestPPrint)
{
    int top = lua_gettop(L);
    ASSERT_TRUE(RunFile(L, "test_script.luac"));
    ASSERT_EQ(top, lua_gettop(L));
}

TEST_F(ScriptTest, TestRandom)
{
    int top = lua_gettop(L);
    ASSERT_TRUE(RunString(L, "math.randomseed(123)"));
    ASSERT_TRUE(RunString(L, "assert(math.random(0,100) == 1)"));
    ASSERT_TRUE(RunString(L, "assert(math.random(0,100) == 58)"));
    ASSERT_TRUE(RunString(L, "assert(math.abs(math.random() - 0.70421460615864) < 0.0000001)"));
    ASSERT_EQ(top, lua_gettop(L));
}

int main(int argc, char **argv)
{
    testing::InitGoogleTest(&argc, argv);

    int ret = RUN_ALL_TESTS();
    return ret;
}
