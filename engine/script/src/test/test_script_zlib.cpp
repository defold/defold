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

class ScriptZlibTest : public ::testing::Test
{
protected:
    virtual void SetUp()
    {
        dmConfigFile::Result r = dmConfigFile::Load("src/test/test.config", 0, 0, &m_ConfigFile);
        ASSERT_EQ(dmConfigFile::RESULT_OK, r);

        m_Context = dmScript::NewContext(m_ConfigFile);

        L = lua_open();
        luaL_openlibs(L);
        dmScript::ScriptParams params;
        params.m_Context = m_Context;
        dmScript::Initialize(L, params);
    }

    virtual void TearDown()
    {
        dmConfigFile::Delete(m_ConfigFile);
        dmScript::Finalize(L);
        dmScript::DeleteContext(m_Context);
        lua_close(L);
    }

    dmScript::HContext m_Context;
    dmConfigFile::HConfig m_ConfigFile;
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

TEST_F(ScriptZlibTest, TestZlib)
{
    int top = lua_gettop(L);

    ASSERT_TRUE(RunFile(L, "test_zlib.luac"));

    lua_getglobal(L, "functions");
    ASSERT_EQ(LUA_TTABLE, lua_type(L, -1));
    lua_getfield(L, -1, "test_zlib");
    ASSERT_EQ(LUA_TFUNCTION, lua_type(L, -1));
    int result = lua_pcall(L, 0, LUA_MULTRET, 0);
    if (result == LUA_ERRRUN)
    {
        dmLogError("Error running script: %s", lua_tostring(L,-1));
        ASSERT_TRUE(false);
        lua_pop(L, 1);
    }
    else
    {
        ASSERT_EQ(0, result);
    }
    lua_pop(L, 1);

    ASSERT_EQ(top, lua_gettop(L));
}

int main(int argc, char **argv)
{
    testing::InitGoogleTest(&argc, argv);

    int ret = RUN_ALL_TESTS();
    return ret;
}
