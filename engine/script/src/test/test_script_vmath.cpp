#include <gtest/gtest.h>

#include <stdio.h>
#include <stdint.h>

#include "script.h"
#include "script_vmath.h"

#include <dlib/log.h>
#include <dlib/dstrings.h>

extern "C"
{
#include <lua/lauxlib.h>
#include <lua/lualib.h>
}

#define PATH_FORMAT "build/default/src/test/%s"

class ScriptVmathTest : public ::testing::Test
{
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

TEST_F(ScriptVmathTest, TestNumber)
{
    ASSERT_TRUE(RunFile(L, "test_number.luac"));
}

TEST_F(ScriptVmathTest, TestVector)
{

    ASSERT_TRUE(RunFile(L, "test_vector.luac"));

}

TEST_F(ScriptVmathTest, TestVectorFail)
{
    // empty constructor and index
    ASSERT_FALSE(RunString(L, "local v = vmath.vector()\nlocal a = v[1]"));

    // string as constructor
    ASSERT_FALSE(RunString(L, "local v = vmath.vector(\"foo\")"));

    // index
    ASSERT_FALSE(RunString(L, "local v = vmath.vector( { 1, 2, 3 } )\nlocal a = v[4]"));

    // new index
    ASSERT_FALSE(RunString(L, "local v = vmath.vector( { 1, 2, 3 } )\nv[4] = 4"));
}

TEST_F(ScriptVmathTest, TestVector3)
{
    int top = lua_gettop(L);
    Vectormath::Aos::Vector3 v(1.0f, 2.0f, 3.0f);
    dmScript::PushVector3(L, v);
    Vectormath::Aos::Vector3* vp = dmScript::CheckVector3(L, -1);
    ASSERT_NE((void*)0x0, vp);
    ASSERT_EQ(v.getX(), vp->getX());
    ASSERT_EQ(v.getY(), vp->getY());
    ASSERT_EQ(v.getZ(), vp->getZ());
    lua_pop(L, 1);
    ASSERT_EQ(top, lua_gettop(L));

    ASSERT_TRUE(RunFile(L, "test_vector3.luac"));
}

TEST_F(ScriptVmathTest, TestVector3Fail)
{
    // constructor
    ASSERT_FALSE(RunString(L, "local v = vmath.vector3(0,0)"));
    // index
    ASSERT_FALSE(RunString(L, "local v = vmath.vector3(0,0,0)\nlocal a = v.X"));
    // new index
    ASSERT_FALSE(RunString(L, "local v = vmath.vector3(0,0,0)\nv.X = 1"));
    // add
    ASSERT_FALSE(RunString(L, "local v = vmath.vector3(0,0,0)\nlocal v2 = v + 1"));
    // add
    ASSERT_FALSE(RunString(L, "local v = vmath.vector3(0,0,0)\nlocal v2 = v - 1"));
    // mul
    ASSERT_FALSE(RunString(L, "local v = vmath.vector3(0,0,0)\nlocal v2 = v * \"hej\""));
    // Dot
    ASSERT_FALSE(RunString(L, "local s = vmath.dot(vmath.vector3(0,0,0))"));
    ASSERT_FALSE(RunString(L, "local s = vmath.dot(vmath.vector3(0,0,0), 1)"));
    // LengthSqr
    ASSERT_FALSE(RunString(L, "local s = vmath.lengthSqr(1)"));
    // Length
    ASSERT_FALSE(RunString(L, "local s = vmath.length(1)"));
    // Normalize
    ASSERT_FALSE(RunString(L, "local s = vmath.normalize(1)"));
    // Cross
    ASSERT_FALSE(RunString(L, "local s = vmath.cross(1)"));
    ASSERT_FALSE(RunString(L, "local s = vmath.cross(vmath.vector3(0,0,0), 1)"));
    // Lerp
    ASSERT_FALSE(RunString(L, "local v = vmath.lerp(0, vmath.vector3(0,0,0), 1)"));
    // Slerp
    ASSERT_FALSE(RunString(L, "local v = vmath.slerp(0, vmath.vector3(0,0,0), 1)"));
}

TEST_F(ScriptVmathTest, TestVector4)
{
    int top = lua_gettop(L);
    Vectormath::Aos::Vector4 v(1.0f, 2.0f, 3.0f, 4.0f);
    dmScript::PushVector4(L, v);
    Vectormath::Aos::Vector4* vp = dmScript::CheckVector4(L, -1);
    ASSERT_NE((void*)0x0, vp);
    ASSERT_EQ(v.getX(), vp->getX());
    ASSERT_EQ(v.getY(), vp->getY());
    ASSERT_EQ(v.getZ(), vp->getZ());
    ASSERT_EQ(v.getW(), vp->getW());
    lua_pop(L, 1);
    ASSERT_EQ(top, lua_gettop(L));

    ASSERT_TRUE(RunFile(L, "test_vector4.luac"));
}

TEST_F(ScriptVmathTest, TestVector4Fail)
{
    // constructor
    ASSERT_FALSE(RunString(L, "local v = vmath.vector4(0,0,0)"));
    // index
    ASSERT_FALSE(RunString(L, "local v = vmath.vector4(0,0,0,0)\nlocal a = v.X"));
    // new index
    ASSERT_FALSE(RunString(L, "local v = vmath.vector4(0,0,0,0)\nv.X = 1"));
}

TEST_F(ScriptVmathTest, TestQuat)
{
    int top = lua_gettop(L);
    Vectormath::Aos::Quat q(1.0f, 2.0f, 3.0f, 4.0f);
    dmScript::PushQuat(L, q);
    Vectormath::Aos::Quat* qp = dmScript::CheckQuat(L, -1);
    ASSERT_NE((void*)0x0, qp);
    ASSERT_EQ(q.getX(), qp->getX());
    ASSERT_EQ(q.getY(), qp->getY());
    ASSERT_EQ(q.getZ(), qp->getZ());
    ASSERT_EQ(q.getW(), qp->getW());
    lua_pop(L, 1);
    ASSERT_EQ(top, lua_gettop(L));

    ASSERT_TRUE(RunFile(L, "test_quat.luac"));
}

TEST_F(ScriptVmathTest, TestQuatFail)
{
    // constructor
    ASSERT_FALSE(RunString(L, "local q = vmath.quat(0,0,0)"));
    // index
    ASSERT_FALSE(RunString(L, "local q = vmath.quat(0,0,0,1)\nlocal a = q.X"));
    // new index
    ASSERT_FALSE(RunString(L, "local q = vmath.quat(0,0,0,1)\nq.X = 1"));
    // mul
    ASSERT_FALSE(RunString(L, "local q = vmath.quat(0,0,0,1) * 1"));
    // FromStartToEnd
    ASSERT_FALSE(RunString(L, "local q = vmath.quat_from_start_to_end(vmath.vector3(0, 0, 0), 1)"));
    // FromAxisAngle
    ASSERT_FALSE(RunString(L, "local q = vmath.quat_from_axis_angle(0, 1)"));
    // FromBasis
    ASSERT_FALSE(RunString(L, "local q = vmath.quat_from_basis(vmath.vector3(1,0,0), vmath.vector3(0,1,0), 1)"));
    // FromRotationX
    ASSERT_FALSE(RunString(L, "local q = vmath.quat_from_rotation_x(vmath.vector3(1,0,0))"));
    // FromRotationY
    ASSERT_FALSE(RunString(L, "local q = vmath.quat_from_rotation_y(vmath.vector3(1,0,0))"));
    // FromRotationZ
    ASSERT_FALSE(RunString(L, "local q = vmath.quat_from_rotation_z(vmath.vector3(1,0,0))"));
    // Conj
    ASSERT_FALSE(RunString(L, "local q = vmath.conj(1)"));
    // Rotate
    ASSERT_FALSE(RunString(L, "local v = vmath.rotate(vmath.quat(0, 0, 0, 1), 1)"));
    // Lerp
    ASSERT_FALSE(RunString(L, "local q = vmath.lerp(1, vmath.quat(0, 0, 0, 1), vmath.vector3(0, 0, 0))"));
    // Slerp
    ASSERT_FALSE(RunString(L, "local q = vmath.slerp(1, vmath.quat(0, 0, 0, 1), vmath.vector3(0, 0, 0))"));
}

TEST_F(ScriptVmathTest, TestTransform)
{
    int top = lua_gettop(L);
    Vectormath::Aos::Vector3 pos(1.0f, 2.0f, 3.0f);
    Vectormath::Aos::Quat rot(4.0f, 5.0f, 6.0f, 7.0f);
    lua_newtable(L);
    dmScript::PushVector3(L, pos);
    lua_setfield(L, -2, "Position");
    dmScript::PushQuat(L, rot);
    lua_setfield(L, -2, "Rotation");

    ASSERT_EQ(top + 1, lua_gettop(L));

    lua_pushstring(L, "Position");
    lua_rawget(L, -2);
    Vectormath::Aos::Vector3* vp = dmScript::CheckVector3(L, -1);
    ASSERT_NE((void*)0x0, (void*)vp);
    ASSERT_EQ(pos.getX(), vp->getX());
    ASSERT_EQ(pos.getY(), vp->getY());
    ASSERT_EQ(pos.getZ(), vp->getZ());
    lua_pop(L, 1);

    lua_pushstring(L, "Rotation");
    lua_rawget(L, -2);
    Vectormath::Aos::Quat* qp = dmScript::CheckQuat(L, -1);
    ASSERT_NE((void*)0x0, (void*)qp);
    ASSERT_EQ(rot.getX(), qp->getX());
    ASSERT_EQ(rot.getY(), qp->getY());
    ASSERT_EQ(rot.getZ(), qp->getZ());
    ASSERT_EQ(rot.getW(), qp->getW());
    lua_pop(L, 1);

    lua_pop(L, 1);

    ASSERT_EQ(top, lua_gettop(L));
}

TEST_F(ScriptVmathTest, TestMatrix4)
{
    int top = lua_gettop(L);
    Vectormath::Aos::Matrix4 m;
    for (uint32_t i = 0; i < 4; ++i)
        for (uint32_t j = 0; j < 4; ++j)
            m.setElem((float) i, (float) j, (float) (i * 4 + j));
    dmScript::PushMatrix4(L, m);
    ASSERT_TRUE(dmScript::IsMatrix4(L, -1));
    Vectormath::Aos::Matrix4* mp = dmScript::CheckMatrix4(L, -1);
    ASSERT_NE((void*)0x0, mp);
    for (int i = 0; i < 4; ++i)
    {
        for (int j = 0; j < 4; ++j)
        {
            ASSERT_EQ(m.getElem(i, j), mp->getElem(i, j));
        }
    }

    lua_pop(L, 1);
    ASSERT_EQ(top, lua_gettop(L));

    ASSERT_TRUE(RunFile(L, "test_matrix4.luac"));
}

TEST_F(ScriptVmathTest, TestMatrix4Fail)
{
    // constructor
    ASSERT_FALSE(RunString(L, "local m = vmath.matrix4(0,0,0)"));
    // index
    ASSERT_FALSE(RunString(L, "local m = vmath.matrix4()\nlocal a = m.a"));
    // new index
    ASSERT_FALSE(RunString(L, "local m = vmath.matrix4()\nm.a = 1"));
    // mul
    ASSERT_FALSE(RunString(L, "local m = vmath.matrix4() * true"));
}

int main(int argc, char **argv)
{
    testing::InitGoogleTest(&argc, argv);

    int ret = RUN_ALL_TESTS();
    return ret;
}
