#include <gtest/gtest.h>

#include <stdio.h>
#include <stdint.h>
#include "script/script_vec_math.h"

#include <dlib/log.h>
#include <dlib/dstrings.h>

extern "C"
{
#include <lua/lauxlib.h>
#include <lua/lualib.h>
}

#define PATH_FORMAT "build/default/src/script/test/%s"

class ScriptVecMathTest : public ::testing::Test
{
protected:
    virtual void SetUp()
    {
        L = lua_open();

        luaopen_base(L);
        luaopen_table(L);
        luaopen_string(L);
        luaopen_math(L);
        luaopen_debug(L);

        dmScript::RegisterVecMathLibs(L);
    }

    virtual void TearDown()
    {
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
        dmLogError(lua_tolstring(L, -1, 0));
        return false;
    }
    return true;
}

bool RunString(lua_State* L, const char* script)
{
    if (luaL_dostring(L, script) != 0)
    {
        dmLogError(lua_tolstring(L, -1, 0));
        return false;
    }
    return true;
}

TEST_F(ScriptVecMathTest, TestVector3)
{
    ASSERT_TRUE(RunFile(L, "test_vector3.luac"));
}

TEST_F(ScriptVecMathTest, TestVector3Fail)
{
    // constructor
    ASSERT_FALSE(RunString(L, "local v = vec_math.vector3(0,0)"));
    // index
    ASSERT_FALSE(RunString(L, "local v = vec_math.vector3(0,0,0)\nlocal a = v.X"));
    // new index
    ASSERT_FALSE(RunString(L, "local v = vec_math.vector3(0,0,0)\nv.X = 1"));
    // add
    ASSERT_FALSE(RunString(L, "local v = vec_math.vector3(0,0,0)\nlocal v2 = v + 1"));
    // add
    ASSERT_FALSE(RunString(L, "local v = vec_math.vector3(0,0,0)\nlocal v2 = v - 1"));
    // mul
    ASSERT_FALSE(RunString(L, "local v = vec_math.vector3(0,0,0)\nlocal v2 = v * \"hej\""));
    // Dot
    ASSERT_FALSE(RunString(L, "local s = vec_math.dot(vec_math.vector3(0,0,0))"));
    ASSERT_FALSE(RunString(L, "local s = vec_math.dot(vec_math.vector3(0,0,0), 1)"));
    // LengthSqr
    ASSERT_FALSE(RunString(L, "local s = vec_math.lengthSqr(1)"));
    // Length
    ASSERT_FALSE(RunString(L, "local s = vec_math.length(1)"));
    // Normalize
    ASSERT_FALSE(RunString(L, "local s = vec_math.normalize(1)"));
    // Cross
    ASSERT_FALSE(RunString(L, "local s = vec_math.cross(1)"));
    ASSERT_FALSE(RunString(L, "local s = vec_math.cross(vec_math.vector3(0,0,0), 1)"));
    // Lerp
    ASSERT_FALSE(RunString(L, "local v = vec_math.lerp(0, vec_math.vector3(0,0,0), 1)"));
    // Slerp
    ASSERT_FALSE(RunString(L, "local v = vec_math.slerp(0, vec_math.vector3(0,0,0), 1)"));
}

TEST_F(ScriptVecMathTest, TestQuat)
{
    ASSERT_TRUE(RunFile(L, "test_quat.luac"));
}

TEST_F(ScriptVecMathTest, TestQuatFail)
{
    // constructor
    ASSERT_FALSE(RunString(L, "local q = vec_math.quat(0,0,0)"));
    // index
    ASSERT_FALSE(RunString(L, "local q = vec_math.quat(0,0,0,1)\nlocal a = q.X"));
    // new index
    ASSERT_FALSE(RunString(L, "local q = vec_math.quat(0,0,0,1)\nq.X = 1"));
    // mul
    ASSERT_FALSE(RunString(L, "local q = vec_math.quat(0,0,0,1) * 1"));
    // FromStartToEnd
    ASSERT_FALSE(RunString(L, "local q = vec_math.quat_from_start_to_end(vec_math.vector3(0, 0, 0), 1)"));
    // FromAxisAngle
    ASSERT_FALSE(RunString(L, "local q = vec_math.quat_from_axis_angle(0, 1)"));
    // FromBasis
    ASSERT_FALSE(RunString(L, "local q = vec_math.quat_from_basis(vec_math.vector3(1,0,0), vec_math.vector3(0,1,0), 1)"));
    // FromRotationX
    ASSERT_FALSE(RunString(L, "local q = vec_math.quat_from_rotation_x(vec_math.vector3(1,0,0))"));
    // FromRotationY
    ASSERT_FALSE(RunString(L, "local q = vec_math.quat_from_rotation_y(vec_math.vector3(1,0,0))"));
    // FromRotationZ
    ASSERT_FALSE(RunString(L, "local q = vec_math.quat_from_rotation_z(vec_math.vector3(1,0,0))"));
    // Conj
    ASSERT_FALSE(RunString(L, "local q = vec_math.conj(1)"));
    // Rotate
    ASSERT_FALSE(RunString(L, "local v = vec_math.rotate(vec_math.quat(0, 0, 0, 1), 1)"));
    // Lerp
    ASSERT_FALSE(RunString(L, "local q = vec_math.lerp(1, vec_math.quat(0, 0, 0, 1), vec_math.vector3(0, 0, 0))"));
    // Slerp
    ASSERT_FALSE(RunString(L, "local q = vec_math.slerp(1, vec_math.quat(0, 0, 0, 1), vec_math.vector3(0, 0, 0))"));
}

int main(int argc, char **argv)
{
    testing::InitGoogleTest(&argc, argv);

    int ret = RUN_ALL_TESTS();
    return ret;
}
