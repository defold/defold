#include <gtest/gtest.h>
#include "../script.h"
#include "test/test_ddf.h"

extern "C"
{
#include <lua/lua.h>
#include <lua/lauxlib.h>
#include <lua/lualib.h>
}

TEST(LuaTableToDDF, Transform)
{
    lua_State *L = lua_open();

    dmScript::Initialize(L, dmScript::ScriptParams());

    int top = lua_gettop(L);

    lua_newtable(L);
    dmScript::PushVector3(L, Vectormath::Aos::Vector3(1.0f, 2.0f, 3.0f));
    lua_setfield(L, -2, "position");
    dmScript::PushQuat(L, Vectormath::Aos::Quat(4.0f, 5.0f, 6.0f, 7.0f));
    lua_setfield(L, -2, "rotation");

    char* buf = new char[sizeof(TestScript::Transform)];

    uint32_t size = dmScript::CheckDDF(L, TestScript::Transform::m_DDFDescriptor, buf, sizeof(TestScript::Transform), -1);
    ASSERT_EQ(sizeof(TestScript::Transform), size);

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

    dmScript::Finalize(L);
    lua_close(L);
}

TEST(DDFToLuaTable, Transform)
{
    lua_State *L = lua_open();

    dmScript::Initialize(L, dmScript::ScriptParams());

    int top = lua_gettop(L);

    TestScript::Transform* t = new TestScript::Transform;
    t->m_Position.setX(1.0f);
    t->m_Position.setY(2.0f);
    t->m_Position.setZ(3.0f);
    t->m_Rotation.setX(4.0f);
    t->m_Rotation.setY(5.0f);
    t->m_Rotation.setZ(6.0f);
    t->m_Rotation.setW(7.0f);

    dmScript::PushDDF(L, TestScript::Transform::m_DDFDescriptor, (const char*) t);

    ASSERT_EQ(LUA_TTABLE, lua_type(L, -1));
    lua_getfield(L, -1, "position");
    Vectormath::Aos::Vector3* position = dmScript::CheckVector3(L, -1);
    lua_getfield(L, -2, "rotation");
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

    dmScript::Finalize(L);
    lua_close(L);
}

TEST(LuaTableToDDF, MessageInMessage)
{
    lua_State *L = lua_open();

    dmScript::Initialize(L, dmScript::ScriptParams());

    int top = lua_gettop(L);

    lua_newtable(L);
    lua_pushinteger(L, 100);
    lua_setfield(L, -2, "uint_value");

    lua_pushinteger(L, 200);
    lua_setfield(L, -2, "int_value");

    lua_pushstring(L, "string_value");
    lua_setfield(L, -2, "string_value");

    dmScript::PushVector3(L, Vectormath::Aos::Vector3(1.0f, 2.0f, 3.0f));
    lua_setfield(L, -2, "vec3_value");

    dmScript::PushVector4(L, Vectormath::Aos::Vector4(1.0f, 2.0f, 3.0f, 4.0f));
    lua_setfield(L, -2, "vec4_value");

    dmScript::PushQuat(L, Vectormath::Aos::Quat(1.0f, 2.0f, 3.0f, 4.0f));
    lua_setfield(L, -2, "quat_value");

    Vectormath::Aos::Matrix4 m;
    for (uint32_t i = 0; i < 4; ++i)
        for (uint32_t j = 0; j < 4; ++j)
            m.setElem(i, j, i * 4 + j);
    dmScript::PushMatrix4(L, m);
    lua_setfield(L, -2, "matrix4_value");

    lua_newtable(L);
    lua_pushinteger(L, 1); lua_setfield(L, -2, "uint_value");

    lua_setfield(L, -2, "sub_msg_value");

    lua_pushinteger(L, 1);
    lua_setfield(L, -2, "enum_value");

    char* buf = new char[1024];

    uint32_t size = dmScript::CheckDDF(L, TestScript::Msg::m_DDFDescriptor, buf, 1024, -1);
    ASSERT_EQ(sizeof(TestScript::Msg) + strlen("string_value") + 1, size);

    TestScript::Msg* msg = (TestScript::Msg*) buf;

    msg->m_StringValue = (const char*) ((uintptr_t) msg->m_StringValue + (uintptr_t) msg);

    ASSERT_EQ(100U, msg->m_UintValue);
    ASSERT_EQ(200, msg->m_IntValue);
    ASSERT_STREQ("string_value", msg->m_StringValue);
    ASSERT_EQ(1.0f, msg->m_Vec3Value.getX());
    ASSERT_EQ(2.0f, msg->m_Vec3Value.getY());
    ASSERT_EQ(3.0f, msg->m_Vec3Value.getZ());
    ASSERT_EQ(1.0f, msg->m_Vec4Value.getX());
    ASSERT_EQ(2.0f, msg->m_Vec4Value.getY());
    ASSERT_EQ(3.0f, msg->m_Vec4Value.getZ());
    ASSERT_EQ(4.0f, msg->m_Vec4Value.getW());
    ASSERT_EQ(1.0f, msg->m_QuatValue.getX());
    ASSERT_EQ(2.0f, msg->m_QuatValue.getY());
    ASSERT_EQ(3.0f, msg->m_QuatValue.getZ());
    ASSERT_EQ(4.0f, msg->m_QuatValue.getW());
    for (uint32_t i = 0; i < 4; ++i)
        for (uint32_t j = 0; j < 4; ++j)
            ASSERT_EQ(i * 4 + j, msg->m_Matrix4Value.getElem(i, j));
    ASSERT_EQ(1U, msg->m_SubMsgValue.m_UintValue);
    ASSERT_EQ(TestScript::ENUM_VAL1, msg->m_EnumValue);

    delete[] buf;

    lua_pop(L, 1);

    ASSERT_EQ(top, lua_gettop(L));

    dmScript::Finalize(L);
    lua_close(L);
}

TEST(DDFToLuaTable, MessageInMessage)
{
    lua_State *L = lua_open();

    dmScript::Initialize(L, dmScript::ScriptParams());

    int top = lua_gettop(L);

    TestScript::Msg* g = new TestScript::Msg;
    g->m_UintValue = 1234;
    g->m_IntValue = 5678;
    g->m_StringValue = "foo";
    g->m_Vec3Value.setX(1.0f);
    g->m_Vec3Value.setY(2.0f);
    g->m_Vec3Value.setZ(3.0f);
    g->m_Vec4Value.setX(1.0f);
    g->m_Vec4Value.setY(2.0f);
    g->m_Vec4Value.setZ(3.0f);
    g->m_Vec4Value.setW(4.0f);
    g->m_SubMsgValue.m_UintValue = 1;
    g->m_EnumValue = TestScript::ENUM_VAL1;

    dmScript::PushDDF(L, TestScript::Msg::m_DDFDescriptor, (const char*) g);

    lua_getfield(L, -1, "uint_value"); ASSERT_EQ(1234, luaL_checkint(L, -1)); lua_pop(L, 1);
    lua_getfield(L, -1, "int_value"); ASSERT_EQ(5678, luaL_checkint(L, -1)); lua_pop(L, 1);
    lua_getfield(L, -1, "string_value"); ASSERT_STREQ("foo", luaL_checkstring(L, -1)); lua_pop(L, 1);

    lua_getfield(L, -1, "vec3_value");
    Vectormath::Aos::Vector3* v3 = dmScript::CheckVector3(L, -1);
    ASSERT_NE((void*)0x0, (void*)v3);
    ASSERT_EQ(1.0f, v3->getX());
    ASSERT_EQ(2.0f, v3->getY());
    ASSERT_EQ(3.0f, v3->getZ());
    lua_pop(L, 1);

    lua_getfield(L, -1, "vec4_value");
    Vectormath::Aos::Vector4* v4 = dmScript::CheckVector4(L, -1);
    ASSERT_NE((void*)0x0, (void*)v4);
    ASSERT_EQ(1.0f, v4->getX());
    ASSERT_EQ(2.0f, v4->getY());
    ASSERT_EQ(3.0f, v4->getZ());
    ASSERT_EQ(4.0f, v4->getW());
    lua_pop(L, 1);

    lua_getfield(L, -1, "sub_msg_value");
    lua_getfield(L, -1, "uint_value"); ASSERT_EQ(1, luaL_checkint(L, -1)); lua_pop(L, 1);
    lua_pop(L, 1);

    lua_getfield(L, -1, "enum_value"); ASSERT_EQ(TestScript::ENUM_VAL1, luaL_checkint(L, -1)); lua_pop(L, 1);

    lua_pop(L, 1);

    ASSERT_EQ(top, lua_gettop(L));

    delete g;

    dmScript::Finalize(L);
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
    lua_setfield(L, -2, "uint_value");

    lua_pushstring(L, "string_value");
    lua_setfield(L, -2, "string_value");

    uint32_t buffer_size = dmScript::CheckDDF(L, TestScript::LuaDDFBufferOverflow::m_DDFDescriptor, p->m_Buf, p->m_BufferSize, -1);
    assert(buffer_size == sizeof(TestScript::LuaDDFBufferOverflow) + strlen("string_value") + 1);
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

    dmScript::Finalize(L);
    lua_close(L);
}

int main(int argc, char **argv)
{
    testing::InitGoogleTest(&argc, argv);
    int ret = RUN_ALL_TESTS();
    return ret;
}
