#include <gtest/gtest.h>
#include "../script.h"
#include "test/test_ddf.h"

extern "C"
{
#include <lua/lua.h>
#include <lua/lauxlib.h>
#include <lua/lualib.h>
}

class ScriptDDFTest : public ::testing::Test
{
protected:
protected:
    virtual void SetUp()
    {
        m_Context = dmScript::NewContext(0, 0, true);
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


TEST_F(ScriptDDFTest, TransformToDDF)
{
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
}

TEST_F(ScriptDDFTest, TransformToLua)
{
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
}

TEST_F(ScriptDDFTest, MessageInMessageToDDF)
{
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
            m.setElem((float) i, (float) j, (float) (i * 4 + j));
    dmScript::PushMatrix4(L, m);
    lua_setfield(L, -2, "matrix4_value");

    lua_newtable(L);
    lua_pushinteger(L, 1); lua_setfield(L, -2, "uint_value");

    lua_setfield(L, -2, "sub_msg_value");

    lua_pushinteger(L, 1);
    lua_setfield(L, -2, "enum_value");

    lua_pushboolean(L, true);
    lua_setfield(L, -2, "bool_value");

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
    ASSERT_EQ(true, msg->m_BoolValue);

    delete[] buf;

    lua_pop(L, 1);

    ASSERT_EQ(top, lua_gettop(L));
}

TEST_F(ScriptDDFTest, MessageInMessageToLua)
{
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
    g->m_BoolValue = true;

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

    lua_getfield(L, -1, "bool_value"); ASSERT_EQ(true, lua_toboolean(L, -1)); lua_pop(L, 1);

    lua_pop(L, 1);

    ASSERT_EQ(top, lua_gettop(L));

    delete g;
}

TEST_F(ScriptDDFTest, RepeatedFieldDDFFromLua)
{
    int top = lua_gettop(L);

    lua_newtable(L);
    lua_newtable(L);
    for (int i=0;i<10;i++)
    {
        lua_newtable(L);
        if (i & 1)
        {
            lua_pushstring(L, "theoptvalue");
            lua_setfield(L, -2, "string_optional");
        }
        lua_pushstring(L, "thereqvalue");
        lua_setfield(L, -2, "string_required");
        lua_rawseti(L, -2, i+1);
    }

    lua_setfield(L, -2, "items");

    char *buf = new char[1024];

    dmScript::CheckDDF(L, TestScript::RepeatedContainer::m_DDFDescriptor, buf, 1024, -1);
    TestScript::RepeatedContainer* msg = (TestScript::RepeatedContainer*) buf;
    ASSERT_EQ(msg->m_Items.m_Count, 10);

    lua_pop(L, 1);
    ASSERT_EQ(top, lua_gettop(L));

    delete[] buf;
}

TEST_F(ScriptDDFTest, RepeatedFieldDDFToLua)
{
    TestScript::RepeatedContainer* msg = new TestScript::RepeatedContainer;
    TestScript::RepeatedItem* items = new TestScript::RepeatedItem[2];
    msg->m_Items.m_Count = 2;
    msg->m_Items.m_Data = items;

    items[0].m_StringOptional = "opt0";
    items[0].m_StringRequired = "req0";
    items[1].m_StringOptional = 0;
    items[1].m_StringRequired = "req1";

    // first iteration tests with PushDDF with real pointers, second time around with offsets
    dmScript::PushDDF(L, TestScript::RepeatedContainer::m_DDFDescriptor, (const char*) msg, false);
    for (int i=0;i<2;i++)
    {
        lua_getfield(L, -1, "items");
        ASSERT_EQ(false, lua_isnil(L, -1));

        lua_rawgeti(L, -1, 1);
        lua_getfield(L, -1, "string_required");
        ASSERT_EQ(0, strcmp(lua_tostring(L, -1), "req0"));
        lua_pop(L, 1);
        lua_getfield(L, -1, "string_optional");
        ASSERT_EQ(0, strcmp(lua_tostring(L, -1), "opt0"));
        lua_pop(L, 2);

        lua_rawgeti(L, -1, 2);
        lua_getfield(L, -1, "string_required");
        ASSERT_EQ(0, strcmp(lua_tostring(L, -1), "req1"));
        lua_pop(L, 1);
        lua_getfield(L, -1, "string_optional");
        ASSERT_EQ(true, lua_isnil(L, -1) || !strcmp(lua_tostring(L, -1), ""));
        lua_pop(L, 3);

        if (i == 0)
        {
            char* buf = new char[512];
            dmScript::CheckDDF(L, TestScript::RepeatedContainer::m_DDFDescriptor, buf, 512, -1);
            lua_pop(L, 1);
            dmScript::PushDDF(L, TestScript::RepeatedContainer::m_DDFDescriptor, (const char*) buf, true);
            delete[] buf;
        }
    }

    lua_pop(L, 1);


    delete[] items;
    delete msg;
}

TEST_F(ScriptDDFTest, DefaultValueToDDF)
{
     int top = lua_gettop(L);

    lua_newtable(L);

    char* buf = new char[1024];

    uint32_t size = dmScript::CheckDDF(L, TestScript::DefaultValue::m_DDFDescriptor, buf, 1024, -1);
    (void) size;

    TestScript::DefaultValue* msg = (TestScript::DefaultValue*) buf;

    msg->m_StringValue = (const char*) ((uintptr_t) msg->m_StringValue + (uintptr_t) msg);

    ASSERT_EQ(10U, msg->m_UintValue);
    ASSERT_STREQ("test", msg->m_StringValue);
    ASSERT_EQ(0.0f, msg->m_QuatValue.getX());
    ASSERT_EQ(0.0f, msg->m_QuatValue.getY());
    ASSERT_EQ(0.0f, msg->m_QuatValue.getZ());
    ASSERT_EQ(1.0f, msg->m_QuatValue.getW());
    ASSERT_EQ(1, msg->m_EnumValue);
    ASSERT_EQ(true, msg->m_BoolValue);

    delete[] buf;

    lua_pop(L, 1);

    ASSERT_EQ(top, lua_gettop(L));
}

TEST_F(ScriptDDFTest, OptionalNoDefaultValueToDDF)
{
    int top = lua_gettop(L);

    lua_newtable(L);

    char* buf = new char[1024];

    uint32_t size = dmScript::CheckDDF(L, TestScript::OptionalNoDefaultValue::m_DDFDescriptor, buf, 1024, -1);
    (void) size;

    TestScript::OptionalNoDefaultValue* msg = (TestScript::OptionalNoDefaultValue*) buf;

    msg->m_StringValue = (const char*) ((uintptr_t) msg->m_StringValue + (uintptr_t) msg);

    ASSERT_EQ(0U, msg->m_UintValue);
    ASSERT_STREQ("", msg->m_StringValue);
    ASSERT_EQ(0, msg->m_EnumValue);

    delete[] buf;

    lua_pop(L, 1);

    ASSERT_EQ(top, lua_gettop(L));
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

TEST_F(ScriptDDFTest, LuaDDFBufferOverflowToDDF)
{
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
}

TEST_F(ScriptDDFTest, Uint64ToDDF)
{
    int top = lua_gettop(L);

    struct Test
    {
        uint32_t m_Size;
        char* m_Buffer;
        static void CheckDDF(lua_State* L, Test* test)
        {
            test->m_Size = dmScript::CheckDDF(L, TestScript::Uint64Msg::m_DDFDescriptor, test->m_Buffer, sizeof(TestScript::Uint64Msg), -1);
        }

        static int TestString(lua_State* L)
        {
            Test* test = static_cast<Test*>(lua_touserdata(L, 1));
            lua_newtable(L);
            lua_pushstring(L, "test");
            lua_setfield(L, -2, "uint64_value");
            CheckDDF(L, test);
            return 0;
        }
        static int TestNumber(lua_State* L)
        {
            Test* test = static_cast<Test*>(lua_touserdata(L, 1));
            lua_newtable(L);
            lua_pushnumber(L, 1);
            lua_setfield(L, -2, "uint64_value");
            CheckDDF(L, test);
            return 0;
        }
        static int TestHash(lua_State* L)
        {
            Test* test = static_cast<Test*>(lua_touserdata(L, 1));
            lua_newtable(L);
            dmScript::PushHash(L, (dmhash_t)1);
            lua_setfield(L, -2, "uint64_value");
            CheckDDF(L, test);
            return 0;
        }
    };

    Test test;
    test.m_Size = 0;
    test.m_Buffer = new char[sizeof(TestScript::Uint64Msg)];

    int res = lua_cpcall(L, Test::TestString, &test);
    ASSERT_NE(0, res);
    ASSERT_TRUE((bool) lua_isstring(L, -1));
    lua_pop(L, 1);

    res = lua_cpcall(L, Test::TestNumber, &test);
    ASSERT_NE(0, res);
    ASSERT_TRUE((bool) lua_isstring(L, -1));
    lua_pop(L, 1);

    res = lua_cpcall(L, Test::TestHash, &test);
    ASSERT_EQ(0, res);
    ASSERT_EQ(sizeof(TestScript::Uint64Msg), test.m_Size);

    ASSERT_EQ(top, lua_gettop(L));

    delete[] test.m_Buffer;
}

int main(int argc, char **argv)
{
    testing::InitGoogleTest(&argc, argv);
    int ret = RUN_ALL_TESTS();
    return ret;
}
