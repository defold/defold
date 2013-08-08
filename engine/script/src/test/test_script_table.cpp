#include <setjmp.h>
#include <stdlib.h>
#include <dlib/dstrings.h>
#include <dlib/log.h>
#include <dlib/align.h>
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
        accept_panic = false;
        g_LuaTableTest = this;
        L = lua_open();
        lua_atpanic(L, &AtPanic);
        m_Context = dmScript::NewContext(0, 0);
        dmScript::ScriptParams params;
        params.m_Context = m_Context;
        dmScript::Initialize(L, params);
        top = lua_gettop(L);
    }

    static int AtPanic(lua_State *L)
    {
        if (g_LuaTableTest->accept_panic)
            longjmp(g_LuaTableTest->env, 0);
        dmLogError("Unexpected error: %s", lua_tostring(L, -1));
        exit(5);
        return 0;
    }

    virtual void TearDown()
    {
        ASSERT_EQ(top, lua_gettop(L));
        dmScript::Finalize(L, m_Context);
        lua_close(L);
        dmScript::DeleteContext(m_Context);
        g_LuaTableTest = 0;
    }

    char DM_ALIGNED(16) m_Buf[256];
    bool accept_panic;
    jmp_buf env;
    int top;
    dmScript::HContext m_Context;
    lua_State* L;
};

TEST_F(LuaTableTest, EmptyTable)
{
    lua_newtable(L);
    char buf[2];
    uint32_t buffer_used = dmScript::CheckTable(L, buf, sizeof(buf), -1);
    // 2 bytes for count
    ASSERT_EQ(2U, buffer_used);
    lua_pop(L, 1);
}

// count (+ align) + n * element-size
const uint32_t OVERFLOW_BUFFER_SIZE = 2 + 2 + 0xffff * (sizeof(char) + sizeof(char) + sizeof(uint16_t) + sizeof(lua_Number));

int ProduceOverflow(lua_State *L)
{
    char buf[OVERFLOW_BUFFER_SIZE];
    lua_newtable(L);
    // too many iterations
    for (uint32_t i = 0; i <= 0xffff; ++i)
    {
        // key
        lua_pushnumber(L, i);
        // value
        lua_pushnumber(L, i);
        // store pair
        lua_settable(L, -3);
    }
    uint32_t buffer_used = dmScript::CheckTable(L, buf, OVERFLOW_BUFFER_SIZE, -1);
    // expect it to fail, avoid warning
    (void)buffer_used;
    return 1;
}

TEST_F(LuaTableTest, Overflow)
{
    int result = lua_cpcall(L, ProduceOverflow, 0x0);
    // 2 bytes for count
    ASSERT_NE(0, result);
    char expected_error[64];
    DM_SNPRINTF(expected_error, 64, "too many values in table, %d is max", 0xffff);
    ASSERT_STREQ(expected_error, lua_tostring(L, -1));
    // pop error message
    lua_pop(L, 1);
}

const uint32_t IOOB_BUFFER_SIZE = 2 + 2 + (sizeof(char) + sizeof(char) + sizeof(uint16_t) + sizeof(lua_Number));

int ProduceIndexOutOfBounds(lua_State *L)
{
    char buf[IOOB_BUFFER_SIZE];
    lua_newtable(L);
    // invalid key
    lua_pushnumber(L, 0xffff+1);
    // value
    lua_pushnumber(L, 0);
    // store pair
    lua_settable(L, -3);
    uint32_t buffer_used = dmScript::CheckTable(L, buf, IOOB_BUFFER_SIZE, -1);
    // expect it to fail, avoid warning
    (void)buffer_used;
    return 1;
}

TEST_F(LuaTableTest, IndexOutOfBounds)
{
    int result = lua_cpcall(L, ProduceIndexOutOfBounds, 0x0);
    // 2 bytes for count
    ASSERT_NE(0, result);
    char expected_error[64];
    DM_SNPRINTF(expected_error, 64, "index out of bounds, max is %d", 0xffff);
    ASSERT_STREQ(expected_error, lua_tostring(L, -1));
    // pop error message
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

    uint32_t buffer_used = dmScript::CheckTable(L, m_Buf, sizeof(m_Buf), -1);
    (void) buffer_used;
    lua_pop(L, 1);

    dmScript::PushTable(L, m_Buf);

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
        accept_panic = true;
        dmScript::CheckTable(L, m_Buf, buffer_used-1, -1);
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

    uint32_t buffer_used = dmScript::CheckTable(L, m_Buf, sizeof(m_Buf), -1);
    (void) buffer_used;
    lua_pop(L, 1);

    dmScript::PushTable(L, m_Buf);

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
        accept_panic = true;
        dmScript::CheckTable(L, m_Buf, buffer_used-1, -1);
        ASSERT_TRUE(0); // Never reached due to error
    }
    else
    {
        lua_pop(L, 1);
    }
}

TEST_F(LuaTableTest, case1308)
{
    // Create table
    lua_newtable(L);
    lua_pushstring(L, "ab");
    lua_setfield(L, -2, "a");

    lua_newtable(L);
    lua_pushinteger(L, 123);
    lua_setfield(L, -2, "x");

    lua_setfield(L, -2, "t");

    uint32_t buffer_used = dmScript::CheckTable(L, m_Buf, sizeof(m_Buf), -1);
    (void) buffer_used;
    lua_pop(L, 1);

    dmScript::PushTable(L, m_Buf);

    lua_getfield(L, -1, "a");
    ASSERT_EQ(LUA_TSTRING, lua_type(L, -1));
    ASSERT_STREQ("ab", lua_tostring(L, -1));
    lua_pop(L, 1);

    lua_getfield(L, -1, "t");
    ASSERT_EQ(LUA_TTABLE, lua_type(L, -1));
    lua_getfield(L, -1, "x");
    ASSERT_EQ(LUA_TNUMBER, lua_type(L, -1));
    ASSERT_EQ(123, lua_tonumber(L, -1));
    lua_pop(L, 1);
    lua_pop(L, 1);

    lua_pop(L, 1);
}


TEST_F(LuaTableTest, Vector3)
{
    // Create table
    lua_newtable(L);
    dmScript::PushVector3(L, Vectormath::Aos::Vector3(1,2,3));
    lua_setfield(L, -2, "v");

    uint32_t buffer_used = dmScript::CheckTable(L, m_Buf, sizeof(m_Buf), -1);
    (void) buffer_used;
    lua_pop(L, 1);

    dmScript::PushTable(L, m_Buf);

    lua_getfield(L, -1, "v");
    ASSERT_TRUE(dmScript::IsVector3(L, -1));
    Vectormath::Aos::Vector3* v = dmScript::CheckVector3(L, -1);
    ASSERT_EQ(1, v->getX());
    ASSERT_EQ(2, v->getY());
    ASSERT_EQ(3, v->getZ());
    lua_pop(L, 1);

    lua_pop(L, 1);
}

TEST_F(LuaTableTest, Vector4)
{
    // Create table
    lua_newtable(L);
    dmScript::PushVector4(L, Vectormath::Aos::Vector4(1,2,3,4));
    lua_setfield(L, -2, "v");

    uint32_t buffer_used = dmScript::CheckTable(L, m_Buf, sizeof(m_Buf), -1);
    (void) buffer_used;
    lua_pop(L, 1);

    dmScript::PushTable(L, m_Buf);

    lua_getfield(L, -1, "v");
    ASSERT_TRUE(dmScript::IsVector4(L, -1));
    Vectormath::Aos::Vector4* v = dmScript::CheckVector4(L, -1);
    ASSERT_EQ(1, v->getX());
    ASSERT_EQ(2, v->getY());
    ASSERT_EQ(3, v->getZ());
    ASSERT_EQ(4, v->getW());
    lua_pop(L, 1);

    lua_pop(L, 1);
}

TEST_F(LuaTableTest, Quat)
{
    // Create table
    lua_newtable(L);
    dmScript::PushQuat(L, Vectormath::Aos::Quat(1,2,3,4));
    lua_setfield(L, -2, "v");

    uint32_t buffer_used = dmScript::CheckTable(L, m_Buf, sizeof(m_Buf), -1);
    (void) buffer_used;
    lua_pop(L, 1);

    dmScript::PushTable(L, m_Buf);

    lua_getfield(L, -1, "v");
    ASSERT_TRUE(dmScript::IsQuat(L, -1));
    Vectormath::Aos::Quat* v = dmScript::CheckQuat(L, -1);
    ASSERT_EQ(1, v->getX());
    ASSERT_EQ(2, v->getY());
    ASSERT_EQ(3, v->getZ());
    ASSERT_EQ(4, v->getW());
    lua_pop(L, 1);

    lua_pop(L, 1);
}

TEST_F(LuaTableTest, Matrix4)
{
    // Create table
    lua_newtable(L);
    Vectormath::Aos::Matrix4 m;
    for (uint32_t i = 0; i < 4; ++i)
        for (uint32_t j = 0; j < 4; ++j)
            m.setElem(i, j, i * 4 + j);
    dmScript::PushMatrix4(L, m);
    lua_setfield(L, -2, "v");

    uint32_t buffer_used = dmScript::CheckTable(L, m_Buf, sizeof(m_Buf), -1);
    (void) buffer_used;
    lua_pop(L, 1);

    dmScript::PushTable(L, m_Buf);

    lua_getfield(L, -1, "v");
    ASSERT_TRUE(dmScript::IsMatrix4(L, -1));
    Vectormath::Aos::Matrix4* v = dmScript::CheckMatrix4(L, -1);
    for (uint32_t i = 0; i < 4; ++i)
        for (uint32_t j = 0; j < 4; ++j)
            ASSERT_EQ(i * 4 + j, v->getElem(i, j));
    lua_pop(L, 1);

    lua_pop(L, 1);
}

TEST_F(LuaTableTest, Hash)
{
    int top = lua_gettop(L);

    // Create table
    lua_newtable(L);
    dmhash_t hash = dmHashString64("test");
    dmScript::PushHash(L, hash);
    lua_setfield(L, -2, "h");

    uint32_t buffer_used = dmScript::CheckTable(L, m_Buf, sizeof(m_Buf), -1);
    (void) buffer_used;
    lua_pop(L, 1);

    dmScript::PushTable(L, m_Buf);

    lua_getfield(L, -1, "h");
    ASSERT_TRUE(dmScript::IsHash(L, -1));
    dmhash_t hash2 = dmScript::CheckHash(L, -1);
    ASSERT_EQ(hash, hash2);
    lua_pop(L, 1);

    lua_pop(L, 1);

    ASSERT_EQ(top, lua_gettop(L));
}

TEST_F(LuaTableTest, URL)
{
    int top = lua_gettop(L);

    // Create table
    lua_newtable(L);
    dmMessage::URL url = dmMessage::URL();
    url.m_Socket = 1;
    url.m_Path = 2;
    url.m_Fragment = 3;
    dmScript::PushURL(L, url);
    lua_setfield(L, -2, "url");

    uint32_t buffer_used = dmScript::CheckTable(L, m_Buf, sizeof(m_Buf), -1);
    (void) buffer_used;
    lua_pop(L, 1);

    dmScript::PushTable(L, m_Buf);

    lua_getfield(L, -1, "url");
    ASSERT_TRUE(dmScript::IsURL(L, -1));
    dmMessage::URL url2 = *dmScript::CheckURL(L, -1);
    ASSERT_EQ(url.m_Socket, url2.m_Socket);
    ASSERT_EQ(url.m_Path, url2.m_Path);
    ASSERT_EQ(url.m_Fragment, url2.m_Fragment);
    lua_pop(L, 1);

    lua_pop(L, 1);

    ASSERT_EQ(top, lua_gettop(L));
}

TEST_F(LuaTableTest, MixedKeys)
{
    // Create table
    lua_newtable(L);

    lua_pushnumber(L, 1);
    lua_pushnumber(L, 2);
    lua_settable(L, -3);

    lua_pushstring(L, "key1");
    lua_pushnumber(L, 3);
    lua_settable(L, -3);

    lua_pushnumber(L, 2);
    lua_pushnumber(L, 4);
    lua_settable(L, -3);

    lua_pushstring(L, "key2");
    lua_pushnumber(L, 5);
    lua_settable(L, -3);

    uint32_t buffer_used = dmScript::CheckTable(L, m_Buf, sizeof(m_Buf), -1);
    (void) buffer_used;
    lua_pop(L, 1);

    dmScript::PushTable(L, m_Buf);

    lua_pushnumber(L, 1);
    lua_gettable(L, -2);
    ASSERT_EQ(LUA_TNUMBER, lua_type(L, -1));
    ASSERT_EQ(2, lua_tonumber(L, -1));
    lua_pop(L, 1);

    lua_pushstring(L, "key1");
    lua_gettable(L, -2);
    ASSERT_EQ(LUA_TNUMBER, lua_type(L, -1));
    ASSERT_EQ(3, lua_tonumber(L, -1));
    lua_pop(L, 1);

    lua_pushnumber(L, 2);
    lua_gettable(L, -2);
    ASSERT_EQ(LUA_TNUMBER, lua_type(L, -1));
    ASSERT_EQ(4, lua_tonumber(L, -1));
    lua_pop(L, 1);

    lua_pushstring(L, "key2");
    lua_gettable(L, -2);
    ASSERT_EQ(LUA_TNUMBER, lua_type(L, -1));
    ASSERT_EQ(5, lua_tonumber(L, -1));
    lua_pop(L, 1);

    lua_pop(L, 1);
}

static void RandomString(char* s, int max_len)
{
    int n = rand() % max_len + 1;
    for (int i = 0; i < n; ++i)
    {
        (*s++) = (char)(rand() % 256);
    }
    *s = '\0';
}

TEST_F(LuaTableTest, Stress)
{
    accept_panic = true;

    for (int iter = 0; iter < 100; ++iter)
    {
        for (int buf_size = 0; buf_size < 256; ++buf_size)
        {
            int n = rand() % 15 + 1;

            lua_newtable(L);
            for (int i = 0; i < n; ++i)
            {
                int key_type = rand() % 2;
                if (key_type == 0)
                {
                    char key[12];
                    RandomString(key, 11);
                    lua_pushstring(L, key);
                }
                else if (key_type == 1)
                {
                    lua_pushnumber(L, rand() % (n + 1));
                }
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
                    char value[16];
                    RandomString(value, 15);
                    lua_pushstring(L, value);
                }

                lua_settable(L, -3);
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

            delete[] buf;
        }
    }
}

int main(int argc, char **argv)
{
    testing::InitGoogleTest(&argc, argv);
    int ret = RUN_ALL_TESTS();
    return ret;
}
