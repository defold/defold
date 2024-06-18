#include <stdio.h>
#include <memory.h>

#define JC_TEST_IMPLEMENTATION
#include <jc_test/jc_test.h>

extern "C"
{
#include <lua/lua.h>
#include <lua/lauxlib.h>
}

extern "C" int csRunTestsLua(lua_State* L);


static void PrintIndent(int indent)
{
    for (int i = 0; i < indent; ++i)
    {
        printf("    ");
    }
}

static void PrintTable(lua_State* L, int indent)
{
    if ((lua_type(L, -2) == LUA_TSTRING))
    {
        PrintIndent(indent);
        printf("%s\n", lua_tostring(L, -2));
    }

    lua_pushnil(L);
    while(lua_next(L, -2) != 0) {
        if(lua_isstring(L, -1))
        {
            PrintIndent(indent);
            printf("%s = %s\n", lua_tostring(L, -2), lua_tostring(L, -1));
        }
        else if(lua_isnumber(L, -1))
        {
            PrintIndent(indent);
            printf("%s = %f\n", lua_tostring(L, -2), lua_tonumber(L, -1));
        }
        else if(lua_istable(L, -1))
        {
            PrintTable(L, indent+1);
        }
        lua_pop(L, 1);
    }
}

TEST(CSharp, Lua)
{
    lua_State* L = luaL_newstate();

    int top = lua_gettop(L);
    ASSERT_EQ(0, top);

    // Test C
    printf("C ->\n");
    lua_newtable(L);
    lua_pushnumber(L, 1);
    lua_setfield(L, -2, "testvalue");
    PrintTable(L, 1);
    printf("<- C\n");

    // Test C#
    printf("C# ->\n");
    int result = csRunTestsLua(L);
    ASSERT_EQ(0, result);
    PrintTable(L, 1);
    printf("<- C#\n");

    lua_pop(L, 2); // two tables
    top = lua_gettop(L);
    ASSERT_EQ(0, top);
    lua_close(L);
}

int main(int argc, char **argv)
{
    jc_test_init(&argc, argv);
    return jc_test_run_all();
}
