#include <gtest/gtest.h>

#include "../facebook_private.h"
#include "../facebook_util.h"
#include <dlib/json.h>

class FBTest : public ::testing::Test
{
};

TEST_F(FBTest, EscapeJsonString)
{
    char unescaped_1[] = "\"";
    char unescaped_2[] = "\\";
    char unescaped_3[] = "\b";
    char unescaped_4[] = "\n";
    char unescaped_5[] = "\t";
    char unescaped_all[] = "\"\\\b\n\t";
    char unescaped_none[] = "abcdefghij";
    char escaped[32];

    ASSERT_EQ(2, dmFacebook::EscapeJsonString(unescaped_1, escaped, escaped+sizeof(escaped)));
    ASSERT_EQ(2, dmFacebook::EscapeJsonString(unescaped_2, escaped, escaped+sizeof(escaped)));
    ASSERT_EQ(2, dmFacebook::EscapeJsonString(unescaped_3, escaped, escaped+sizeof(escaped)));
    ASSERT_EQ(2, dmFacebook::EscapeJsonString(unescaped_4, escaped, escaped+sizeof(escaped)));
    ASSERT_EQ(2, dmFacebook::EscapeJsonString(unescaped_5, escaped, escaped+sizeof(escaped)));

    ASSERT_EQ(10, dmFacebook::EscapeJsonString(unescaped_all, escaped, escaped+sizeof(escaped)));
    ASSERT_STREQ("\\\"\\\\\\\b\\\n\\\t", escaped);

    ASSERT_EQ(10, dmFacebook::EscapeJsonString(unescaped_none, escaped, escaped+sizeof(escaped)));
}

TEST_F(FBTest, TableIsArray)
{
    // { 1: "asd", 2: "fgh", 3: "jkl" }
    lua_State* L_array = luaL_newstate();
    lua_createtable(L_array, 1, 0);
    lua_pushnumber(L_array, 1);
    lua_pushstring(L_array, "asd");
    lua_rawset(L_array, -3);
    lua_pushnumber(L_array, 2);
    lua_pushstring(L_array, "fgh");
    lua_rawset(L_array, -3);
    lua_pushnumber(L_array, 3);
    lua_pushstring(L_array, "jkl");
    lua_rawset(L_array, -3);

    ASSERT_TRUE(dmFacebook::IsLuaArray(L_array, 1));

    // { 1: "asd", "string_key": "fgh", 3: "jkl" }
    lua_State* L_dict = luaL_newstate();
    lua_createtable(L_dict, 1, 0);
    lua_pushnumber(L_dict, 1);
    lua_pushstring(L_dict, "asd");
    lua_rawset(L_dict, -3);
    lua_pushstring(L_dict, "fgh");
    lua_setfield(L_dict, 1, "string_key");
    lua_pushnumber(L_dict, 3);
    lua_pushstring(L_dict, "jkl");
    lua_rawset(L_dict, -3);

    ASSERT_FALSE(dmFacebook::IsLuaArray(L_dict, 1));

    // {}
    lua_State* L_empty = luaL_newstate();
    lua_createtable(L_empty, 1, 0);

    ASSERT_TRUE(dmFacebook::IsLuaArray(L_empty, 1));
}

TEST_F(FBTest, EscapeJsonValues)
{
    char out_json[256];
    ASSERT_EQ(15, dmFacebook::WriteEscapedJsonString(out_json, 256, "apa bepa cepa", 13));

    // should not write past buffer
    char small_buffer[16];
    ASSERT_EQ(0, dmFacebook::WriteEscapedJsonString(small_buffer, 13, "apa bepa cepa", 13)); // not enough for ""\0
    ASSERT_EQ(0, dmFacebook::WriteEscapedJsonString(small_buffer, 14, "apa bepa cepa", 13)); // not enough for ""
    ASSERT_EQ(15, dmFacebook::WriteEscapedJsonString(small_buffer, 16, "apa bepa cepa", 13)); // exact fit

    // empty value input
    ASSERT_EQ(2, dmFacebook::WriteEscapedJsonString(out_json, 256, "", 0));
    ASSERT_EQ(0, dmFacebook::WriteEscapedJsonString(out_json, 256, NULL, 0));
    ASSERT_EQ(0, dmFacebook::WriteEscapedJsonString(out_json, 0, "abc", 3));

    // empty output buffer
    ASSERT_EQ(5, dmFacebook::WriteEscapedJsonString(NULL, 0, "abc", 3));
    ASSERT_EQ(0, dmFacebook::WriteEscapedJsonString(out_json, 0, NULL, 0));
    ASSERT_EQ(0, dmFacebook::WriteEscapedJsonString(NULL, 0, NULL, 0));
}

TEST_F(FBTest, ValueToJson)
{
    char json[256];

    lua_State* L = luaL_newstate();
    lua_pushnumber(L, 1);
    lua_pushnumber(L, 1.1);
    lua_pushnumber(L, 0.0);
    lua_pushnumber(L, -1.0);
    lua_pushnumber(L, -1.1);
    lua_pushstring(L, "asd");
    lua_pushboolean(L, false);
    lua_pushboolean(L, true);
    lua_pushnil(L);
    lua_pushcfunction(L, NULL);

    ASSERT_EQ(1, dmFacebook::LuaValueToJsonValue(L, 1, json, 256));
    ASSERT_STREQ("1", json);

    ASSERT_EQ(3, dmFacebook::LuaValueToJsonValue(L, 2, json, 256));
    ASSERT_STREQ("1.1", json);

    ASSERT_EQ(1, dmFacebook::LuaValueToJsonValue(L, 3, json, 256));
    ASSERT_STREQ("0", json);

    ASSERT_EQ(2, dmFacebook::LuaValueToJsonValue(L, 4, json, 256));
    ASSERT_STREQ("-1", json);

    ASSERT_EQ(4, dmFacebook::LuaValueToJsonValue(L, 5, json, 256));
    ASSERT_STREQ("-1.1", json);

    ASSERT_EQ(5, dmFacebook::LuaValueToJsonValue(L, 6, json, 256));
    ASSERT_STREQ("\"asd\"", json);

    ASSERT_EQ(5, dmFacebook::LuaValueToJsonValue(L, 7, json, 256));
    ASSERT_STREQ("false", json);

    ASSERT_EQ(4, dmFacebook::LuaValueToJsonValue(L, 8, json, 256));
    ASSERT_STREQ("true", json);

    ASSERT_EQ(4, dmFacebook::LuaValueToJsonValue(L, 9, json, 256));
    ASSERT_STREQ("null", json);
}

TEST_F(FBTest, InvalidValueToJson)
{
    char json[256];

    lua_State* L = luaL_newstate();
    lua_pushcfunction(L, NULL);
    lua_pushlightuserdata(L, NULL);
    lua_pushthread(L);
    lua_pushnumber(L, 1);

    ASSERT_EQ(0, dmFacebook::LuaValueToJsonValue(L, 1, json, 256));
    ASSERT_STREQ("", json);

    ASSERT_EQ(0, dmFacebook::LuaValueToJsonValue(L, 2, json, 256));
    ASSERT_STREQ("", json);

    ASSERT_EQ(0, dmFacebook::LuaValueToJsonValue(L, 3, json, 256));
    ASSERT_STREQ("", json);

    char small_buffer[2];
    ASSERT_EQ(0, dmFacebook::LuaValueToJsonValue(L, 4, small_buffer, 0));
    ASSERT_EQ(0, dmFacebook::LuaValueToJsonValue(L, 4, small_buffer, 1));
    ASSERT_EQ(1, dmFacebook::LuaValueToJsonValue(L, 4, small_buffer, 2));
    ASSERT_STREQ("1", small_buffer);
}

TEST_F(FBTest, ArrayToJsonStrings)
{
    char json[256];
    int r = 0;

    // { 1: "asd", 2: "fgh", 3: "jkl" }
    lua_State* L = luaL_newstate();
    lua_createtable(L, 1, 0);
    lua_pushnumber(L, 1);
    lua_pushstring(L, "asd");
    lua_rawset(L, -3);
    lua_pushnumber(L, 2);
    lua_pushstring(L, "fgh");
    lua_rawset(L, -3);
    lua_pushnumber(L, 3);
    lua_pushstring(L, "jkl");
    lua_rawset(L, -3);

    r = dmFacebook::LuaValueToJsonValue(L, 1, json, 256);
    ASSERT_NE(0, r);
    ASSERT_STREQ("[\"asd\",\"fgh\",\"jkl\"]", json);
}

TEST_F(FBTest, ArrayToJsonMixed1)
{
    char json[256];
    int r = 0;

    // { 1: "asd", 2: 1.1, 3: 1 }
    lua_State* L = luaL_newstate();
    lua_createtable(L, 1, 0);
    lua_pushnumber(L, 1);
    lua_pushstring(L, "asd");
    lua_rawset(L, -3);
    lua_pushnumber(L, 2);
    lua_pushnumber(L, 1.1);
    lua_rawset(L, -3);
    lua_pushnumber(L, 3);
    lua_pushnumber(L, 1.0);
    lua_rawset(L, -3);

    r = dmFacebook::LuaValueToJsonValue(L, 1, json, 256);
    ASSERT_NE(0, r);
    ASSERT_STREQ("[\"asd\",1.1,1]", json);
}

TEST_F(FBTest, ArrayToJsonMixed2)
{
    char json[256];
    int r = 0;

    // { 1: false, 2: 1.337, 3: true }
    lua_State* L = luaL_newstate();
    lua_createtable(L, 1, 0);
    lua_pushnumber(L, 1);
    lua_pushboolean(L, false);
    lua_rawset(L, -3);
    lua_pushnumber(L, 2);
    lua_pushnumber(L, 1.337);
    lua_rawset(L, -3);
    lua_pushnumber(L, 3);
    lua_pushboolean(L, true);
    lua_rawset(L, -3);

    r = dmFacebook::LuaValueToJsonValue(L, 1, json, 256);
    ASSERT_NE(0, r);
    ASSERT_STREQ("[false,1.337,true]", json);
}

TEST_F(FBTest, TableToJsonStrings)
{
    char json[256];
    int r = 0;

    // { 1: "asd", "string_key": "fgh", 3: "jkl" }
    lua_State* L = luaL_newstate();
    lua_createtable(L, 1, 0);
    lua_pushnumber(L, 1);
    lua_pushstring(L, "asd");
    lua_rawset(L, -3);
    lua_pushstring(L, "fgh");
    lua_setfield(L, 1, "string_key");
    lua_pushnumber(L, 3);
    lua_pushstring(L, "jkl");
    lua_rawset(L, -3);

    r = dmFacebook::LuaValueToJsonValue(L, 1, json, 256);
    ASSERT_NE(0, r);
    ASSERT_STREQ("{1:\"asd\",3:\"jkl\",\"string_key\":\"fgh\"}", json);
}

TEST_F(FBTest, TableToJsonMultiLevelStrings)
{
    char json[256];
    int r = 0;

    // { "suggestions": ["userid1", "userid2", "userid3"] }
    lua_State* L = luaL_newstate();
    lua_createtable(L, 1, 0);
        lua_createtable(L, 1, 0);
        lua_pushnumber(L, 1);
        lua_pushstring(L, "userid1");
        lua_rawset(L, -3);
        lua_pushnumber(L, 2);
        lua_pushstring(L, "userid2");
        lua_rawset(L, -3);
        lua_pushnumber(L, 3);
        lua_pushstring(L, "userid3");
        lua_rawset(L, -3);
    lua_setfield(L, 1, "suggestions");

    r = dmFacebook::LuaValueToJsonValue(L, 1, json, 256);
    ASSERT_NE(0, r);
    ASSERT_STREQ("{\"suggestions\":[\"userid1\",\"userid2\",\"userid3\"]}", json);
}

TEST_F(FBTest, TableToJsonMultiLevelMixed)
{
    char json[256];
    int r = 0;

    // { "a": {1: "d", 2: "e", "b":["c"]} }
    lua_State* L = luaL_newstate();
    lua_createtable(L, 1, 0);
        lua_createtable(L, 1, 0);
        lua_pushstring(L, "b");
        lua_createtable(L, 1, 0);
        lua_pushnumber(L, 1);
        lua_pushstring(L, "c");
        lua_rawset(L, -3);
        lua_rawset(L, -3);

        lua_pushnumber(L, 1);
        lua_pushstring(L, "d");
        lua_rawset(L, -3);
        lua_pushnumber(L, 2);
        lua_pushstring(L, "e");
        lua_rawset(L, -3);
    lua_setfield(L, 1, "a");

    r = dmFacebook::LuaValueToJsonValue(L, 1, json, 256);
    ASSERT_NE(0, r);
    ASSERT_STREQ("{\"a\":{1:\"d\",2:\"e\",\"b\":[\"c\"]}}", json);
}

TEST_F(FBTest, TableToJsonEmpty1)
{
    char json[256];
    int r = 0;

    // {}
    lua_State* L = luaL_newstate();
    lua_createtable(L, 1, 0);

    r = dmFacebook::LuaValueToJsonValue(L, 1, json, 256);
    ASSERT_NE(0, r);
    ASSERT_STREQ("[]", json);
}

TEST_F(FBTest, TableToJsonEmpty2)
{
    char json[256];
    int r = 0;

    // {}
    lua_State* L = luaL_newstate();
    lua_createtable(L, 1, 0);

    r = dmFacebook::LuaValueToJsonValue(L, -1, json, 256);
    ASSERT_NE(0, r);
    ASSERT_STREQ("[]", json);
}

TEST_F(FBTest, TableToJsonBufferOverflow1)
{
    char json[] = "abcd";
    int r = 0;

    // {}
    lua_State* L = luaL_newstate();
    lua_createtable(L, 1, 0);

    r = dmFacebook::LuaValueToJsonValue(L, 1, json, 2);
    ASSERT_EQ(0, r);
    ASSERT_STREQ("cd", json+2);
}


TEST_F(FBTest, TableToJsonBufferOverflow2)
{
    char json[] = "abcdefghij";
    int r = 0;

    // {"12345"} -> ["12345"]
    lua_State* L = luaL_newstate();
    lua_newtable(L);
    lua_pushnumber(L, 1);
    lua_pushstring(L, "12345");
    lua_rawset(L, -3);

    r = dmFacebook::LuaValueToJsonValue(L, 1, json, 7);
    ASSERT_EQ(0, r);
    ASSERT_STREQ("hij", json+7);
}

TEST_F(FBTest, ParseJsonSimple)
{
    lua_State* L = luaL_newstate();
    lua_createtable(L, 1, 0);
    lua_pushstring(L, "142175679447464,116897941987429");
    lua_setfield(L, 1, "to");
    lua_pushnumber(L, 1337);
    lua_setfield(L, 1, "another_key");

    char json[1024];
    ASSERT_NE(0, dmFacebook::LuaValueToJsonValue(L, 1, json, 1024));

    // parse json document
    dmJson::Document doc;
    dmJson::Result r = dmJson::Parse(json, &doc);
    ASSERT_EQ(dmJson::RESULT_OK, r);
    ASSERT_EQ(dmJson::TYPE_OBJECT, doc.m_Nodes[0].m_Type);

    // check "to" field
    dmJson::Node* node = &doc.m_Nodes[1];
    ASSERT_EQ(dmJson::TYPE_STRING, doc.m_Nodes[1].m_Type);
    ASSERT_EQ('t', (uint8_t) doc.m_Json[node->m_Start]);
    ASSERT_EQ('o', (uint8_t) doc.m_Json[node->m_Start+1]);

    ASSERT_EQ(dmJson::TYPE_STRING, doc.m_Nodes[2].m_Type); // "142175679447464,116897941987429"
    ASSERT_EQ(dmJson::TYPE_STRING, doc.m_Nodes[3].m_Type); // "another_key"
    ASSERT_EQ(dmJson::TYPE_PRIMITIVE, doc.m_Nodes[4].m_Type); // 1337

    dmJson::Free(&doc);
}

TEST_F(FBTest, ParseJsonMultiTable)
{
    lua_State* L = luaL_newstate();
    lua_createtable(L, 1, 0);
        lua_createtable(L, 1, 0);
        lua_pushnumber(L, 1);
        lua_pushstring(L, "userid1");
        lua_rawset(L, -3);
        lua_pushnumber(L, 2);
        lua_pushstring(L, "userid2");
        lua_rawset(L, -3);
        lua_pushnumber(L, 3);
        lua_pushstring(L, "userid3");
        lua_rawset(L, -3);
    lua_setfield(L, 1, "suggestions");

    char json[1024];
    ASSERT_NE(0, dmFacebook::LuaValueToJsonValue(L, 1, json, 1024));

    // parse json document
    dmJson::Document doc;
    dmJson::Result r = dmJson::Parse(json, &doc);
    ASSERT_EQ(dmJson::RESULT_OK, r);
    ASSERT_EQ(6, doc.m_NodeCount); // json-obj + "suggestions" + ["userid1", "userid2", "userid3"]

    ASSERT_EQ(dmJson::TYPE_OBJECT, doc.m_Nodes[0].m_Type); // obj container
    ASSERT_EQ(dmJson::TYPE_STRING, doc.m_Nodes[1].m_Type); // "suggestions"
    ASSERT_EQ(dmJson::TYPE_ARRAY,  doc.m_Nodes[2].m_Type); // suggestions array
    ASSERT_EQ(dmJson::TYPE_STRING, doc.m_Nodes[3].m_Type); // "userid1"
    ASSERT_EQ(dmJson::TYPE_STRING, doc.m_Nodes[4].m_Type); // "userid2"
    ASSERT_EQ(dmJson::TYPE_STRING, doc.m_Nodes[5].m_Type); // "userid3"

    dmJson::Free(&doc);
}

TEST_F(FBTest, DuplicateTable)
{
    lua_State* L = luaL_newstate();
    lua_newtable(L);
    lua_pushstring(L, "suggestions");
        lua_newtable(L);
        lua_pushnumber(L, 1);
        lua_pushstring(L, "userid1");
        lua_rawset(L, -3);
        lua_pushstring(L, "abcd");
        lua_pushstring(L, "userid2");
        lua_rawset(L, -3);
        lua_pushnumber(L, 3);
        lua_pushstring(L, "userid3");
        lua_rawset(L, -3);
    lua_rawset(L, -3);
    int from_index = lua_gettop(L);

    lua_newtable(L);
    int to_index = lua_gettop(L);

    ASSERT_EQ(1, dmFacebook::DuplicateLuaTable(L, from_index, to_index, 4));
    ASSERT_EQ(to_index, lua_gettop(L));

    lua_getfield(L, to_index, "suggestions");
    lua_getfield(L, -1, "abcd");
    ASSERT_STREQ("userid2", lua_tostring(L, -1));
    lua_pop(L, 2);
    ASSERT_EQ(to_index, lua_gettop(L));
}

TEST_F(FBTest, LuaTableToJson)
{
    lua_State* L = luaL_newstate();
    lua_createtable(L, 1, 0);
        lua_createtable(L, 1, 0);
        lua_pushnumber(L, 1);
        lua_pushstring(L, "userid1");
        lua_rawset(L, -3);
        lua_pushnumber(L, 2);
        lua_pushstring(L, "userid2");
        lua_rawset(L, -3);
        lua_pushnumber(L, 3);
        lua_pushstring(L, "userid3");
        lua_rawset(L, -3);
    lua_setfield(L, 1, "suggestions");
    int table_index = lua_gettop(L);

    int size_needed = dmFacebook::LuaTableToJson(L, table_index, 0, 0);
    ASSERT_NE(0, size_needed);

    char* json = (char*)malloc(size_needed + 1 + strlen("magic") + 1);
    strcpy(json + size_needed + 1, "magic");

    int size_written = dmFacebook::LuaTableToJson(L, table_index, json, size_needed+1);
    ASSERT_EQ(size_needed, size_written);
    ASSERT_STREQ("magic", json+size_written+1);

    // parse json document
    dmJson::Document doc;
    dmJson::Result r = dmJson::Parse(json, &doc);
    ASSERT_EQ(dmJson::RESULT_OK, r);
    ASSERT_EQ(6, doc.m_NodeCount); // json-obj + "suggestions" + ["userid1", "userid2", "userid3"]

    ASSERT_EQ(dmJson::TYPE_OBJECT, doc.m_Nodes[0].m_Type); // obj container
    ASSERT_EQ(dmJson::TYPE_STRING, doc.m_Nodes[1].m_Type); // "suggestions"
    ASSERT_EQ(dmJson::TYPE_ARRAY,  doc.m_Nodes[2].m_Type); // suggestions array
    ASSERT_EQ(dmJson::TYPE_STRING, doc.m_Nodes[3].m_Type); // "userid1"
    ASSERT_EQ(dmJson::TYPE_STRING, doc.m_Nodes[4].m_Type); // "userid2"
    ASSERT_EQ(dmJson::TYPE_STRING, doc.m_Nodes[5].m_Type); // "userid3"

    dmJson::Free(&doc);
    free(json);
}

TEST_F(FBTest, DuplicateTableMaxRecursion)
{
    lua_State* L = luaL_newstate();
    lua_newtable(L);
    lua_pushstring(L, "level0");
        lua_newtable(L);
        lua_pushstring(L, "level1");
            lua_newtable(L);
            lua_pushstring(L, "level2");
            lua_pushstring(L, "level2");
            lua_rawset(L, -3);
        lua_rawset(L, -3);
    lua_rawset(L, -3);
    int from_index = lua_gettop(L);

    lua_newtable(L);
    int to_index = lua_gettop(L);

    ASSERT_EQ(0, dmFacebook::DuplicateLuaTable(L, from_index, to_index, 1));
    ASSERT_EQ(to_index, lua_gettop(L));

    lua_getfield(L, to_index, "level0");
    lua_getfield(L, -1, "level1");
    ASSERT_EQ(LUA_TNIL, lua_type(L, -1));
    lua_pop(L, 2);
    ASSERT_EQ(to_index, lua_gettop(L));
}

TEST_F(FBTest, DialogTableConversionAndroid)
{
    lua_State* L = luaL_newstate();
    lua_newtable(L);
    lua_pushstring(L, "to");
        lua_newtable(L);
        lua_pushnumber(L, 1);
        lua_pushstring(L, "userid1");
        lua_rawset(L, -3);
        lua_pushnumber(L, 2);
        lua_pushstring(L, "userid2");
        lua_rawset(L, -3);
        lua_pushnumber(L, 3);
        lua_pushstring(L, "userid3");
        lua_rawset(L, -3);
    lua_rawset(L, -3);
    lua_pushstring(L, "filters");
    lua_pushnumber(L, dmFacebook::GAMEREQUEST_FILTER_APPUSERS);
    lua_rawset(L, -3);
    int from_index = lua_gettop(L);

    lua_newtable(L);
    int to_index = lua_gettop(L);

    ASSERT_EQ(1, dmFacebook::DialogTableToAndroid(L, (char*)"apprequests", from_index, to_index));
    ASSERT_EQ(to_index, lua_gettop(L));

    lua_getfield(L, to_index, "to");
    ASSERT_STREQ("userid1,userid2,userid3", lua_tostring(L, -1));
    lua_pop(L, 1);

    lua_getfield(L, to_index, "filters");
    ASSERT_EQ(LUA_TNUMBER, lua_type(L, -1));
    ASSERT_EQ(dmFacebook::GAMEREQUEST_FILTER_APPUSERS, lua_tointeger(L, -1));
    lua_pop(L, 1);

    ASSERT_EQ(to_index, lua_gettop(L));
}

TEST_F(FBTest, DialogTableConversionEmscripten)
{
    lua_State* L = luaL_newstate();
    lua_newtable(L);
    lua_pushstring(L, "to");
        lua_newtable(L);
        lua_pushnumber(L, 1);
        lua_pushstring(L, "userid1");
        lua_rawset(L, -3);
        lua_pushnumber(L, 2);
        lua_pushstring(L, "userid2");
        lua_rawset(L, -3);
        lua_pushnumber(L, 3);
        lua_pushstring(L, "userid3");
        lua_rawset(L, -3);
    lua_rawset(L, -3);
    lua_pushstring(L, "filters");
    lua_pushnumber(L, dmFacebook::GAMEREQUEST_FILTER_APPUSERS);
    lua_rawset(L, -3);
    lua_pushstring(L, "action_type");
    lua_pushnumber(L, dmFacebook::GAMEREQUEST_ACTIONTYPE_SEND);
    lua_rawset(L, -3);
    int from_index = lua_gettop(L);

    lua_newtable(L);
    int to_index = lua_gettop(L);

    ASSERT_EQ(1, dmFacebook::DialogTableToEmscripten(L, (char*)"apprequests", from_index, to_index));
    ASSERT_EQ(to_index, lua_gettop(L));

    lua_getfield(L, to_index, "to");
    ASSERT_STREQ("userid1,userid2,userid3", lua_tostring(L, -1));
    lua_pop(L, 1);

    lua_getfield(L, to_index, "action_type");
    ASSERT_STREQ("send", lua_tostring(L, -1));
    lua_pop(L, 1);

    lua_getfield(L, to_index, "filters");
    ASSERT_EQ(LUA_TTABLE, lua_type(L, -1));
    lua_rawgeti(L, -1, 1);
    ASSERT_EQ(LUA_TSTRING, lua_type(L, -1));
    ASSERT_STREQ("app_users", lua_tostring(L, -1));
    lua_pop(L, 2);

    ASSERT_EQ(to_index, lua_gettop(L));
}

int main(int argc, char **argv)
{
    testing::InitGoogleTest(&argc, argv);

    int ret = RUN_ALL_TESTS();
    return ret;
}
