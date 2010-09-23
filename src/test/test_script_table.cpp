#include <setjmp.h>
#include <stdlib.h>
#include <gtest/gtest.h>
#include "../script.h"
#include "test/test_ddf.h"

extern "C"
{
#include <lua/lua.h>
#include <lua/lauxlib.h>
#include <lua/lualib.h>
}

class LuaTableTest* g_LuaTableTest = 0;

class LuaTableTest : public ::testing::Test
{
protected:
    virtual void SetUp()
    {
        g_LuaTableTest = this;
        L = lua_open();
        lua_atpanic(L, &AtPanic);
        dmScript::Initialize(L);
        top = lua_gettop(L);
    }

    static int AtPanic(lua_State *L)
    {
        longjmp(g_LuaTableTest->env, 0);
        return 0;
    }

    virtual void TearDown()
    {
        ASSERT_EQ(top, lua_gettop(L));
        lua_close(L);
        g_LuaTableTest = 0;
    }

    jmp_buf env;
    int top;
    lua_State* L;
};

TEST_F(LuaTableTest, EmptyTable)
{
    lua_newtable(L);
    char buf[1];
    uint32_t buffer_used = dmScript::CheckTable(L, buf, sizeof(buf), -1);
    ASSERT_EQ(1U, buffer_used);
    lua_pop(L, 1);
}

TEST_F(LuaTableTest, Table01)
{
    // Create table
    lua_newtable(L);
    lua_pushinteger(L, 123);
    lua_setfield(L, -2, "a");

    lua_pushinteger(L, 456);
    lua_setfield(L, -2, "b");

    char buf[256];

    uint32_t buffer_used = dmScript::CheckTable(L, buf, sizeof(buf), -1);
    (void) buffer_used;
    lua_pop(L, 1);

    dmScript::PushTable(L, buf);

    lua_getfield(L, -1, "a");
    ASSERT_EQ(LUA_TNUMBER, lua_type(L, -1));
    ASSERT_EQ(123, lua_tonumber(L, -1));
    lua_pop(L, 1);

    lua_getfield(L, -1, "b");
    ASSERT_EQ(LUA_TNUMBER, lua_type(L, -1));
    ASSERT_EQ(456, lua_tonumber(L, -1));
    lua_pop(L, 1);

    lua_pop(L, 1);

    // Create table again
    lua_newtable(L);
    lua_pushinteger(L, 123);
    lua_setfield(L, -2, "a");
    lua_pushinteger(L, 456);
    lua_setfield(L, -2, "b");

    int ret = setjmp(env);
    if (ret == 0)
    {
        // buffer_user - 1, expect error
        dmScript::CheckTable(L, buf, buffer_used-1, -1);
        ASSERT_TRUE(0); // Never reached due to error
    }
    else
    {
        lua_pop(L, 1);
    }
}

TEST_F(LuaTableTest, Table02)
{
    // Create table
    lua_newtable(L);
    lua_pushboolean(L, 1);
    lua_setfield(L, -2, "foo");

    lua_pushstring(L, "kalle");
    lua_setfield(L, -2, "foo2");

    char buf[256];

    uint32_t buffer_used = dmScript::CheckTable(L, buf, sizeof(buf), -1);
    (void) buffer_used;
    lua_pop(L, 1);

    dmScript::PushTable(L, buf);

    lua_getfield(L, -1, "foo");
    ASSERT_EQ(LUA_TBOOLEAN, lua_type(L, -1));
    ASSERT_EQ(1, lua_toboolean(L, -1));
    lua_pop(L, 1);

    lua_getfield(L, -1, "foo2");
    ASSERT_EQ(LUA_TSTRING, lua_type(L, -1));
    ASSERT_STREQ("kalle", lua_tostring(L, -1));
    lua_pop(L, 1);

    lua_pop(L, 1);

    // Create table again
    lua_newtable(L);
    lua_pushboolean(L, 1);
    lua_setfield(L, -2, "foo");

    lua_pushstring(L, "kalle");
    lua_setfield(L, -2, "foo2");

    int ret = setjmp(env);
    if (ret == 0)
    {
        // buffer_user - 1, expect error
        dmScript::CheckTable(L, buf, buffer_used-1, -1);
        ASSERT_TRUE(0); // Never reached due to error
    }
    else
    {
        lua_pop(L, 1);
    }
}

TEST_F(LuaTableTest, UnsupportedType)
{
    // Create table
    lua_newtable(L);
    lua_newtable(L);
    lua_setfield(L, -2, "foo");

    char buf[256];
    int ret = setjmp(env);
    if (ret == 0)
    {
        dmScript::CheckTable(L, buf, sizeof(buf), -1);
        ASSERT_TRUE(0); // Never reached due to error
    }
    else
    {
        lua_pop(L, 1);
    }
}

static std::string RandomString(int max_len)
{
    int n = rand() % max_len + 1;
    std::string s;
    for (int i = 0; i < n; ++i)
    {
        char buf[] = { rand() % 256, 0 };
        s += buf;
    }
    return s;
}

TEST_F(LuaTableTest, Stress)
{
    for (int iter = 0; iter < 100; ++iter)
    {
        for (int buf_size = 0; buf_size < 256; ++buf_size)
        {
            int n = rand() % 15 + 1;

            lua_newtable(L);
            for (int i = 0; i < n; ++i)
            {
                std::string key = RandomString(11);
                int value_type = rand() % 3;
                if (value_type == 0)
                {
                    lua_pushboolean(L, 1);
                }
                else if (value_type == 1)
                {
                    lua_pushnumber(L, 123);
                }
                else if (value_type == 2)
                {
                    lua_pushstring(L, RandomString(15).c_str());
                }

                lua_setfield(L, -2, key.c_str());
            }
            char* buf = new char[buf_size];

            bool check_ok = false;
            int ret = setjmp(env);
            if (ret == 0)
            {
                uint32_t buffer_used = dmScript::CheckTable(L, buf, buf_size, -1);
                check_ok = true;
                (void) buffer_used;


                dmScript::PushTable(L, buf);
                lua_pop(L, 1);
            }
            lua_pop(L, 1);
        }
    }
}

int main(int argc, char **argv)
{
    testing::InitGoogleTest(&argc, argv);
    int ret = RUN_ALL_TESTS();
    return ret;
}
