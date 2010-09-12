#include <gtest/gtest.h>
#include "../../script/script_ddf.h"
#include "../../script/script_vec_math.h"
#include "script/test/test_ddf.h"

extern "C"
{
#include <lua/lua.h>
#include <lua/lauxlib.h>
#include <lua/lualib.h>
}

TEST(LuaTableToDDF, Transform)
{
    lua_State *L = lua_open();

    dmScript::RegisterVecMathLibs(L);

    int top = lua_gettop(L);

    lua_newtable(L);
    dmScript::PushVector3(L, Vectormath::Aos::Vector3(1.0f, 2.0f, 3.0f));
    lua_setfield(L, -2, "Position");
    dmScript::PushQuat(L, Vectormath::Aos::Quat(4.0f, 5.0f, 6.0f, 7.0f));
    lua_setfield(L, -2, "Rotation");

    char* buf = new char[sizeof(TestScript::Transform)];

    dmScript::LuaTableToDDF(L, TestScript::Transform::m_DDFDescriptor, buf, sizeof(TestScript::Transform));

    TestScript::Transform* transform = (TestScript::Transform*) buf;

    ASSERT_EQ(1.0f, transform->m_Position.getX());
    ASSERT_EQ(2.0f, transform->m_Position.getY());
    ASSERT_EQ(3.0f, transform->m_Position.getZ());
    ASSERT_EQ(4.0f, transform->m_Rotation.getX());
    ASSERT_EQ(5.0f, transform->m_Rotation.getY());
    ASSERT_EQ(6.0f, transform->m_Rotation.getZ());
    ASSERT_EQ(7.0f, transform->m_Rotation.getW());

    lua_pop(L, 1);

    ASSERT_EQ(top, lua_gettop(L));

    delete[] buf;

    lua_close(L);
}

TEST(DDFToLuaTable, Transform)
{
    lua_State *L = lua_open();

    dmScript::RegisterVecMathLibs(L);

    int top = lua_gettop(L);

    TestScript::Transform* t = new TestScript::Transform;
    t->m_Position.setX(1.0f);
    t->m_Position.setY(2.0f);
    t->m_Position.setZ(3.0f);
    t->m_Rotation.setX(4.0f);
    t->m_Rotation.setY(5.0f);
    t->m_Rotation.setZ(6.0f);
    t->m_Rotation.setW(7.0f);

    dmScript::DDFToLuaTable(L, TestScript::Transform::m_DDFDescriptor, (const char*) t);

    ASSERT_EQ(LUA_TTABLE, lua_type(L, -1));
    lua_getfield(L, -1, "Position");
    Vectormath::Aos::Vector3* position = dmScript::CheckVector3(L, -1);
    lua_getfield(L, -2, "Rotation");
    Vectormath::Aos::Quat* rotation = dmScript::CheckQuat(L, -1);
    ASSERT_NE((void*)0x0, (void*)position);
    ASSERT_NE((void*)0x0, (void*)rotation);
    ASSERT_EQ(1.0f, position->getX());
    ASSERT_EQ(2.0f, position->getY());
    ASSERT_EQ(3.0f, position->getZ());
    ASSERT_EQ(4.0f, rotation->getX());
    ASSERT_EQ(5.0f, rotation->getY());
    ASSERT_EQ(6.0f, rotation->getZ());
    ASSERT_EQ(7.0f, rotation->getW());

    delete t;

    lua_pop(L, 3);

    ASSERT_EQ(top, lua_gettop(L));

    lua_close(L);
}

TEST(LuaTableToDDF, MessageInMessage)
{
    lua_State *L = lua_open();

    dmScript::RegisterVecMathLibs(L);

    int top = lua_gettop(L);

    lua_newtable(L);
    lua_pushinteger(L, 100);
    lua_setfield(L, -2, "UIntValue");

    lua_pushinteger(L, 200);
    lua_setfield(L, -2, "IntValue");

    lua_pushstring(L, "string_value");
    lua_setfield(L, -2, "StringValue");

    dmScript::PushVector3(L, Vectormath::Aos::Vector3(1.0f, 2.0f, 3.0f));

    lua_setfield(L, -2, "VecValue");

    lua_newtable(L);
    lua_pushinteger(L, 1); lua_setfield(L, -2, "UIntValue");

    lua_setfield(L, -2, "SubMsgValue");

    char* buf = new char[1024];

    dmScript::LuaTableToDDF(L, TestScript::Msg::m_DDFDescriptor, buf, 1024);

    TestScript::Msg* msg = (TestScript::Msg*) buf;

    msg->m_StringValue = (const char*) ((uintptr_t) msg->m_StringValue + (uintptr_t) msg);

    ASSERT_EQ(100U, msg->m_UIntValue);
    ASSERT_EQ(200, msg->m_IntValue);
    ASSERT_STREQ("string_value", msg->m_StringValue);
    ASSERT_EQ(1.0f, msg->m_VecValue.getX());
    ASSERT_EQ(2.0f, msg->m_VecValue.getY());
    ASSERT_EQ(3.0f, msg->m_VecValue.getZ());
    ASSERT_EQ(1U, msg->m_SubMsgValue.m_UIntValue);

    delete[] buf;

    lua_pop(L, 1);

    ASSERT_EQ(top, lua_gettop(L));

    lua_close(L);
}

TEST(DDFToLuaTable, MessageInMessage)
{
    lua_State *L = lua_open();

    dmScript::RegisterVecMathLibs(L);

    int top = lua_gettop(L);

    TestScript::Msg* g = new TestScript::Msg;
    g->m_UIntValue = 1234;
    g->m_IntValue = 5678;
    g->m_StringValue = "foo";
    g->m_VecValue.setX(1.0f);
    g->m_VecValue.setY(2.0f);
    g->m_VecValue.setZ(3.0f);
    g->m_SubMsgValue.m_UIntValue = 1;

    dmScript::DDFToLuaTable(L, TestScript::Msg::m_DDFDescriptor, (const char*) g);

    lua_getfield(L, -1, "UIntValue"); ASSERT_EQ(1234, luaL_checkint(L, -1)); lua_pop(L, 1);
    lua_getfield(L, -1, "IntValue"); ASSERT_EQ(5678, luaL_checkint(L, -1)); lua_pop(L, 1);
    lua_getfield(L, -1, "StringValue"); ASSERT_STREQ("foo", luaL_checkstring(L, -1)); lua_pop(L, 1);

    lua_getfield(L, -1, "VecValue");
    Vectormath::Aos::Vector3* v = dmScript::CheckVector3(L, -1);
    ASSERT_NE((void*)0x0, (void*)v);
    ASSERT_EQ(1.0f, v->getX());
    ASSERT_EQ(2.0f, v->getY());
    ASSERT_EQ(3.0f, v->getZ());
    lua_pop(L, 1);

    lua_getfield(L, -1, "SubMsgValue");
    lua_getfield(L, -1, "UIntValue"); ASSERT_EQ(1, luaL_checkint(L, -1)); lua_pop(L, 1);
    lua_pop(L, 1);

    lua_pop(L, 1);

    ASSERT_EQ(top, lua_gettop(L));

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

    dmScript::LuaTableToDDF(L, TestScript::LuaDDFBufferOverflow::m_DDFDescriptor, p->m_Buf, p->m_BufferSize);

    return 0;
}

TEST(LuaTableToDDF, LuaDDFBufferOverflow)
{
    lua_State *L = lua_open();

    int top = lua_gettop(L);

    uint32_t expected_size = sizeof(TestScript::LuaDDFBufferOverflow) + strlen("string_value") + 1;

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
        {
            ASSERT_NE(0, ret);
            lua_pop(L, 1);
        }

        delete[] buf;
    }

    ASSERT_EQ(top, lua_gettop(L));

    lua_close(L);
}

int main(int argc, char **argv)
{
    testing::InitGoogleTest(&argc, argv);
    int ret = RUN_ALL_TESTS();
    return ret;
}
