#include <gtest/gtest.h>

#include "script.h"
#include "script_job.h"

#include <dlib/dstrings.h>
#include <dlib/job.h>
#include <dlib/log.h>

extern "C"
{
#include <lua/lauxlib.h>
#include <lua/lualib.h>
}

#define PATH_FORMAT "build/default/src/test/%s"

class ScriptJobTest : public ::testing::Test
{
protected:
protected:
    virtual void SetUp()
    {
        dmBuffer::NewContext();
        m_Context = dmScript::NewContext(0, 0, true);
        dmScript::Initialize(m_Context);

        L = dmScript::GetLuaState(m_Context);
    }

    virtual void TearDown()
    {
        dmScript::Finalize(m_Context);
        dmScript::DeleteContext(m_Context);
        dmBuffer::DeleteContext();
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

TEST_F(ScriptJobTest, TestJob)
{
    int top = lua_gettop(L);

    ASSERT_TRUE(RunFile(L, "test_job.luac"));
    lua_getglobal(L, "functions");
    ASSERT_EQ(LUA_TTABLE, lua_type(L, -1));
    lua_getfield(L, -1, "test_job");
    ASSERT_EQ(LUA_TFUNCTION, lua_type(L, -1));
    //dmScript::PushHash(L, hash);
    //lua_pushstring(L, hash_hex);
    int result = dmScript::PCall(L, 0, LUA_MULTRET);
    if (result == LUA_ERRRUN)
    {
        ASSERT_TRUE(false);
    }
    else
    {
        ASSERT_EQ(0, result);
    }
    lua_pop(L, 1);

    dmJob::RunMainJobs();

    ASSERT_EQ(top, lua_gettop(L));
}

int main(int argc, char **argv)
{
    testing::InitGoogleTest(&argc, argv);

    int ret = RUN_ALL_TESTS();
    return ret;
}
