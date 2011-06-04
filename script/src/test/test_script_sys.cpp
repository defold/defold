#include <gtest/gtest.h>

#include "script.h"

#include <dlib/dstrings.h>
#include <dlib/hash.h>
#include <dlib/log.h>

extern "C"
{
#include <lua/lauxlib.h>
#include <lua/lualib.h>
}

#define PATH_FORMAT "build/default/src/test/%s"

class ScriptSysTest : public ::testing::Test
{
protected:
    virtual void SetUp()
    {
        L = lua_open();
        luaL_openlibs(L);
        dmScript::Initialize(L, dmScript::ScriptParams());
    }

    virtual void TearDown()
    {
        dmScript::Finalize(L);
        lua_close(L);
    }

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

TEST_F(ScriptSysTest, TestSys)
{
    int top = lua_gettop(L);

    ASSERT_TRUE(RunFile(L, "test_sys.luac"));

    lua_getglobal(L, "functions");
    ASSERT_EQ(LUA_TTABLE, lua_type(L, -1));
    lua_getfield(L, -1, "test_sys");
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

TEST_F(ScriptSysTest, Timer)
{
    int top = lua_gettop(L);

    const char* s = "g_self = {}\n"
                    "g_callback_called = false\n"
                    "function init()\n"
                    "    g_self.my_value = 123\n"
                    "    sys.timer(g_self, call_back, 1)\n"
                    "end\n"
                    "function update()\n"
                    "end\n"
                    "function call_back(self)\n"
                    "    assert (self.my_value == 123)\n"
                    "    g_callback_called = true\n"
                    "end\n"
                    "\n";

    int ret = luaL_loadbuffer(L, s, strlen(s), "test");
    ASSERT_EQ(0, ret);
    ret = lua_pcall(L, 0, LUA_MULTRET, 0);
    ASSERT_EQ(0, ret);

    lua_getglobal(L, "init");
    int init_ref = lua_ref(L, LUA_REGISTRYINDEX);
    ASSERT_NE(LUA_NOREF, init_ref);

    lua_getglobal(L, "update");
    int update_ref = lua_ref(L, LUA_REGISTRYINDEX);
    ASSERT_NE(LUA_NOREF, update_ref);

    lua_rawgeti(L, LUA_REGISTRYINDEX, init_ref);
    ret = lua_pcall(L, 0, 0, 0);
    ASSERT_EQ(0, ret);

    for (uint32_t i = 0; i < 60; ++i)
    {
        lua_getglobal(L, "g_callback_called");
        ASSERT_TRUE(lua_isboolean(L, 1));
        ASSERT_EQ(false, lua_toboolean(L, 1));
        lua_pop(L, 1);

        dmScript::Update(L, 1.0f / 60.0f);
    }

    lua_getglobal(L, "g_callback_called");
    ASSERT_TRUE(lua_isboolean(L, 1));
    ASSERT_EQ(true, lua_toboolean(L, 1));
    lua_pop(L, 1);

    ASSERT_EQ(top, lua_gettop(L));
}

TEST_F(ScriptSysTest, Stress)
{
    // Stress
    const char* s =
                    "function call_back(self)\n"
                    "    assert (self.my_value == 123)\n"
                    "    g_callback_called = true\n"
                    "end\n"
                    "for i=1,1000000 do\n"
                    "    local ret = sys.timer(123, call_back, 1000)\n"
                    "    if ret == false then\n"
                    "        return\n"
                    "    end\n"
                    "end\n"
                    "assert (false)\n"
                    "\n";

    ASSERT_TRUE(RunString(L, s));
}

int main(int argc, char **argv)
{
    testing::InitGoogleTest(&argc, argv);

    int ret = RUN_ALL_TESTS();
    return ret;
}
