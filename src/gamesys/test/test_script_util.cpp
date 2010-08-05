#include <gtest/gtest.h>
#include "../gameobject.h"
#include "../../script/script_util.h"
#include "gamesys/test/test_resource_ddf.h"

extern "C"
{
#include <lua/lua.h>
#include <lua/lauxlib.h>
#include <lua/lualib.h>
}

TEST(LuaTableToDDF, Vec3)
{
    lua_State *L = lua_open();

    lua_newtable(L);
    lua_pushinteger(L, 11); lua_setfield(L, -2, "X");
    lua_pushinteger(L, 22); lua_setfield(L, -2, "Y");
    lua_pushinteger(L, 33); lua_setfield(L, -2, "Z");

    char* buf = new char[sizeof(TestResource::Vec)];

    dmScriptUtil::LuaTableToDDF(L, TestResource::Vec::m_DDFDescriptor, buf, sizeof(TestResource::Vec));

    TestResource::Vec* vec = (TestResource::Vec*) buf;

    ASSERT_EQ(11, vec->m_X);
    ASSERT_EQ(22, vec->m_Y);
    ASSERT_EQ(33, vec->m_Z);

    delete[] buf;

    lua_close(L);
}

TEST(DDFToLuaTable, Vec3)
{
    lua_State *L = lua_open();

    TestResource::Vec* vec = new TestResource::Vec;
    vec->m_X = 111;
    vec->m_Y = 222;
    vec->m_Z = 333;

    dmScriptUtil::DDFToLuaTable(L, TestResource::Vec::m_DDFDescriptor, (const char*) vec);

    lua_getfield(L, -1, "X"); ASSERT_EQ(111, luaL_checkint(L, -1)); lua_pop(L, 1);
    lua_getfield(L, -1, "Y"); ASSERT_EQ(222, luaL_checkint(L, -1)); lua_pop(L, 1);
    lua_getfield(L, -1, "Z"); ASSERT_EQ(333, luaL_checkint(L, -1)); lua_pop(L, 1);

    delete vec;

    lua_close(L);
}

TEST(LuaTableToDDF, MessageInMessage)
{
    lua_State *L = lua_open();

    lua_newtable(L);
    lua_pushinteger(L, 100);
    lua_setfield(L, -2, "UIntValue");

    lua_pushinteger(L, 200);
    lua_setfield(L, -2, "IntValue");

    lua_pushstring(L, "string_value");
    lua_setfield(L, -2, "StringValue");

    lua_newtable(L);
    lua_pushinteger(L, 11); lua_setfield(L, -2, "X");
    lua_pushinteger(L, 22); lua_setfield(L, -2, "Y");
    lua_pushinteger(L, 33); lua_setfield(L, -2, "Z");

    lua_setfield(L, -2, "VecValue");

    char* buf = new char[1024];

    dmScriptUtil::LuaTableToDDF(L, TestResource::GlobalData::m_DDFDescriptor, buf, 1024);

    TestResource::GlobalData* global_data = (TestResource::GlobalData*) buf;

    global_data->m_StringValue = (const char*) ((uintptr_t) global_data->m_StringValue + (uintptr_t) global_data);

    ASSERT_EQ(100U, global_data->m_UIntValue);
    ASSERT_EQ(200, global_data->m_IntValue);
    ASSERT_STREQ("string_value", global_data->m_StringValue);
    ASSERT_EQ(11, global_data->m_VecValue.m_X);
    ASSERT_EQ(22, global_data->m_VecValue.m_Y);
    ASSERT_EQ(33, global_data->m_VecValue.m_Z);

    delete[] buf;

    lua_close(L);
}

TEST(DDFToLuaTable, MessageInMessage)
{
    lua_State *L = lua_open();

    TestResource::GlobalData* g = new TestResource::GlobalData;
    g->m_UIntValue = 1234;
    g->m_IntValue = 5678;
    g->m_StringValue = "foo";
    g->m_VecValue.m_X = 11;
    g->m_VecValue.m_Y = 22;
    g->m_VecValue.m_Z = 33;

    dmScriptUtil::DDFToLuaTable(L, TestResource::GlobalData::m_DDFDescriptor, (const char*) g);

    lua_getfield(L, -1, "UIntValue"); ASSERT_EQ(1234, luaL_checkint(L, -1)); lua_pop(L, 1);
    lua_getfield(L, -1, "IntValue"); ASSERT_EQ(5678, luaL_checkint(L, -1)); lua_pop(L, 1);
    lua_getfield(L, -1, "StringValue"); ASSERT_STREQ("foo", luaL_checkstring(L, -1)); lua_pop(L, 1);

    lua_getfield(L, -1, "VecValue");
    lua_getfield(L, -1, "X"); ASSERT_EQ(11, luaL_checkint(L, -1)); lua_pop(L, 1);
    lua_getfield(L, -1, "Y"); ASSERT_EQ(22, luaL_checkint(L, -1)); lua_pop(L, 1);
    lua_getfield(L, -1, "Z"); ASSERT_EQ(33, luaL_checkint(L, -1)); lua_pop(L, 1);
    lua_pop(L, 1);

    delete g;

    lua_close(L);
}

struct LuaDDFBufferOverflowParam
{
    char*    m_Buf;
    uint32_t m_BufferSize;
};

int ProtectedLuaDDFBufferOverflow (lua_State *L)
{
    LuaDDFBufferOverflowParam* p = (LuaDDFBufferOverflowParam*) lua_touserdata(L, 1);

    lua_newtable(L);
    lua_pushinteger(L, 100);
    lua_setfield(L, -2, "UIntValue");

    lua_pushstring(L, "string_value");
    lua_setfield(L, -2, "StringValue");

    dmScriptUtil::LuaTableToDDF(L, TestResource::LuaDDFBufferOverflow::m_DDFDescriptor, p->m_Buf, p->m_BufferSize);

    return 0;
}

TEST(LuaTableToDDF, LuaDDFBufferOverflow)
{
    lua_State *L = lua_open();

    uint32_t expected_size = sizeof(TestResource::LuaDDFBufferOverflow) + strlen("string_value") + 1;

    for (uint32_t i = 0; i < 512; ++i)
    {
        char* buf = new char[i];

        LuaDDFBufferOverflowParam p;
        p.m_Buf = buf;
        p.m_BufferSize = i;

        int ret = lua_cpcall(L, &ProtectedLuaDDFBufferOverflow, &p);
        if (i >= expected_size)
            ASSERT_EQ(0, ret);
        else
            ASSERT_NE(0, ret);

        delete[] buf;
    }
    lua_close(L);
}

int main(int argc, char **argv)
{
    testing::InitGoogleTest(&argc, argv);
    int ret = RUN_ALL_TESTS();
    return ret;
}
