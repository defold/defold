#include <gtest/gtest.h>

#include "script.h"

#include <dlib/dstrings.h>
#include <dlib/hash.h>
#include <dlib/log.h>
#include <dlib/configfile.h>

extern "C"
{
#include <lua/lauxlib.h>
#include <lua/lualib.h>
}

#define PATH_FORMAT "build/default/src/test/%s"

#define USERTYPE "UserType"

struct UserType
{
    int m_Reference;
};

static const luaL_reg UserType_methods[] =
{
    {0,0}
};

static int UserType_gc(lua_State *L)
{
    UserType* object = (UserType*)dmScript::CheckUserType(L, 1, USERTYPE, "Expected UserType");
    memset(object, 0, sizeof(*object));
    (void) object;
    assert(object);
    return 0;
}

static int UserType_GetUserData(lua_State* L)
{
    UserType* ut = (UserType*)lua_touserdata(L, -1);
    lua_pushlightuserdata(L, ut);
    return 1;
}

static const luaL_reg UserType_meta[] =
{
    {"__gc",                                UserType_gc},
    {dmScript::META_TABLE_GET_USER_DATA,    UserType_GetUserData},
    {0, 0}
};

UserType* NewUserType(lua_State* L)
{

    int top = lua_gettop(L);
    (void) top;

    UserType* object = (UserType*)lua_newuserdata(L, sizeof(UserType));

    lua_pushvalue(L, -1);
    object->m_Reference = dmScript::Ref(L, LUA_REGISTRYINDEX);

    luaL_getmetatable(L, USERTYPE);
    lua_setmetatable(L, -2);

    lua_pop(L, 1);

    assert(top == lua_gettop(L));

    return object;
}

void DeleteUserType(lua_State* L, UserType* object)
{
    int top = lua_gettop(L);
    (void) top;

    dmScript::Unref(L, LUA_REGISTRYINDEX, object->m_Reference);

    assert(top == lua_gettop(L));
}

void PushUserType(lua_State* L, UserType* object)
{
    lua_rawgeti(L, LUA_REGISTRYINDEX, object->m_Reference);
    lua_pushvalue(L, -1);
    dmScript::SetInstance(L);
}

void PopUserType(lua_State* L)
{
    lua_pop(L, 1);
    lua_pushnil(L);
    dmScript::SetInstance(L);
}

class ScriptUserTypeTest : public ::testing::Test
{
protected:
    virtual void SetUp()
    {
        m_Context = dmScript::NewContext(0x0, 0, true);
        dmScript::Initialize(m_Context);
        L = dmScript::GetLuaState(m_Context);

        dmScript::RegisterUserType(L, USERTYPE, UserType_methods, UserType_meta);
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

TEST_F(ScriptUserTypeTest, TestUserType)
{
    int top = lua_gettop(L);

    UserType* object = NewUserType(L);
    PushUserType(L, object);
    PopUserType(L);
    DeleteUserType(L, object);

    ASSERT_EQ(top, lua_gettop(L));
}

TEST_F(ScriptUserTypeTest, TestIsUserType)
{
    int top = lua_gettop(L);

    UserType* object = NewUserType(L);
    PushUserType(L, object);

    ASSERT_TRUE(dmScript::IsUserType(L, -1, USERTYPE));

    PopUserType(L);
    DeleteUserType(L, object);

    ASSERT_EQ(top, lua_gettop(L));
}

TEST_F(ScriptUserTypeTest, TestCheckUserType)
{
    int top = lua_gettop(L);

    UserType* object = NewUserType(L);
    PushUserType(L, object);

    ASSERT_EQ(object, dmScript::CheckUserType(L, -1, USERTYPE));

    PopUserType(L);
    DeleteUserType(L, object);

    ASSERT_EQ(top, lua_gettop(L));
}

TEST_F(ScriptUserTypeTest, TestGetUserData)
{
    int top = lua_gettop(L);

    UserType* object = NewUserType(L);
    PushUserType(L, object);

    uintptr_t user_data;
    ASSERT_TRUE(dmScript::GetUserData(L, &user_data, USERTYPE));

    ASSERT_EQ((uintptr_t)object, user_data);

    ASSERT_FALSE(dmScript::GetUserData(L, &user_data, "incorrect_type"));

    PopUserType(L);
    DeleteUserType(L, object);

    ASSERT_EQ(top, lua_gettop(L));
}

int main(int argc, char **argv)
{
    testing::InitGoogleTest(&argc, argv);

    int ret = RUN_ALL_TESTS();
    return ret;
}
