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
        dmScript::Initialize(m_Context);
        L = dmScript::GetLuaState(m_Context);
    }

    virtual void TearDown()
    {
        dmScript::Finalize(m_Context);
        dmScript::DeleteContext(m_Context);
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
    ASSERT_TRUE(RunString(L, "assert(math.abs(math.random() - 0.70419311523438) < 0.0000001)"));
    ASSERT_EQ(top, lua_gettop(L));
}

static int LuaCallCallback(lua_State* L)
{
    luaL_checktype(L, -1, LUA_TFUNCTION);
    lua_pushvalue(L, 1);
    lua_setglobal(L, "_callback");
    lua_pushlightuserdata(L, L);
    lua_setglobal(L, "_state");
    return 0;
}

/*
 * This test mimicks how callbacks are handled in extensions, e.g. facebook, iap, push, with respect to coroutine safety
 */
TEST_F(ScriptTest, TestCoroutineCallback)
{
    int top = lua_gettop(L);
    // Setup test function
    lua_pushcfunction(L, LuaCallCallback);
    lua_setglobal(L, "call_callback");
    // Run script
    ASSERT_TRUE(RunFile(L, "test_script_coroutine.luac"));
    // Retrieve lua state from c func
    lua_getglobal(L, "_state");
    lua_State* state = (lua_State*)lua_touserdata(L, -1);
    lua_pop(L, 1);
    // Retrieve and run callback from c func
    lua_getglobal(L, "_callback");
    luaL_checktype(L, -1, LUA_TFUNCTION);
    int ret = lua_pcall(dmScript::GetMainThread(state), 0, LUA_MULTRET, 0);
    ASSERT_EQ(0, ret);

    // Retrieve coroutine
    lua_getglobal(L, "global_co");
    int status = lua_status(lua_tothread(L, -1));
    lua_pop(L, 1);
    ASSERT_NE(LUA_YIELD, status);

    ASSERT_EQ(top, lua_gettop(L));
}

int main(int argc, char **argv)
{
    testing::InitGoogleTest(&argc, argv);

    int ret = RUN_ALL_TESTS();
    return ret;
}
