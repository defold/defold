#include <gtest/gtest.h>

#include "../facebook_private.h"
#include "../facebook_util.h"
#include <dlib/json.h>

class FBTest : public ::testing::Test
{
public:
    lua_State* L;

private:

    // Code here will be called immediately after the constructor (right before each test).
    virtual void SetUp() {
        L = luaL_newstate();
    }

    // Code here will be called immediately after each test (right before the destructor).
    virtual void TearDown() {
        lua_close(L);
    }
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

TEST_F(FBTest, TableIsArray1)
{
    // { 1: "asd", 2: "fgh", 3: "jkl" }
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

    ASSERT_TRUE(dmFacebook::IsLuaArray(L, 1));
}

TEST_F(FBTest, TableIsArray2)
{
    // { 1: "asd", "string_key": "fgh", 3: "jkl" }
    lua_createtable(L, 1, 0);
    lua_pushnumber(L, 1);
    lua_pushstring(L, "asd");
    lua_rawset(L, -3);
    lua_pushstring(L, "fgh");
    lua_setfield(L, 1, "string_key");
    lua_pushnumber(L, 3);
    lua_pushstring(L, "jkl");
    lua_rawset(L, -3);

    ASSERT_FALSE(dmFacebook::IsLuaArray(L, 1));
}

TEST_F(FBTest, TableIsArray3)
{
    // {}
    lua_createtable(L, 1, 0);

    ASSERT_TRUE(dmFacebook::IsLuaArray(L, 1));
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

TEST_F(FBTest, JoinCStringArray_NullArray)
{
    char buffer[1] = { 0 };

    dmFacebook::JoinCStringArray(NULL, 0, buffer, sizeof(buffer) / sizeof(buffer[0]), ",");
    // If it doesn't crash it works
}

TEST_F(FBTest, JoinCStringArray_NullBuffer)
{
    const char* array[1] = { 0 };

    dmFacebook::JoinCStringArray((const char**) array, sizeof(array) / sizeof(array[0]), NULL, 0, ",");
    // If it doesn't crash it works
}

TEST_F(FBTest, JoinCStringArray_LargerBuffer)
{
    const char* array[1] = { 0 };
    char buffer[9] = { 0 };

    const char* e1 = "one";
    array[0] = e1;

    dmFacebook::JoinCStringArray((const char**) array, sizeof(array) / sizeof(array[0]), buffer, sizeof(buffer) / sizeof(buffer[0]), ",");
    ASSERT_STRCASEEQ("one", buffer);
}

TEST_F(FBTest, JoinCStringArray_SmallerBuffer)
{
    const char* array[3] = { 0 };
    char buffer[6] = { 0 };

    const char* e1 = "one";
    const char* e2 = "two";
    const char* e3 = "three";

    array[0] = e1;
    array[1] = e2;
    array[2] = e3;

    dmFacebook::JoinCStringArray((const char**) array, sizeof(array) / sizeof(array[0]), buffer, sizeof(buffer) / sizeof(buffer[0]), ",");
    ASSERT_STRCASEEQ("one,t", buffer);
}

TEST_F(FBTest, JoinCStringArray_SingleElement)
{
    const char* array[1] = { 0 };
    char buffer[4] = { 0 };

    const char* e1 = "one";
    array[0] = e1;

    dmFacebook::JoinCStringArray((const char**) array, sizeof(array) / sizeof(array[0]), buffer, sizeof(buffer) / sizeof(buffer[0]), ",");
    ASSERT_STRCASEEQ("one", buffer);
}

TEST_F(FBTest, JoinCStringArray_MultipleElement)
{
    const char* array[3] = { 0 };
    char buffer[14] = { 0 };

    const char* e1 = "one";
    const char* e2 = "two";
    const char* e3 = "three";

    array[0] = e1;
    array[1] = e2;
    array[2] = e3;

    dmFacebook::JoinCStringArray((const char**) array, sizeof(array) / sizeof(array[0]), buffer, sizeof(buffer) / sizeof(buffer[0]), ",");
    ASSERT_STRCASEEQ("one,two,three", buffer);
}

TEST_F(FBTest, luaTableToCArray_NullBuffer)
{
    lua_createtable(L, 1, 0);
    lua_pushnumber(L, 1);
    lua_pushstring(L, "one");
    lua_rawset(L, -3);

    uint32_t result = dmFacebook::luaTableToCArray(L, -1, NULL, 0);
    ASSERT_EQ(0, result);
}

TEST_F(FBTest, luaTableToCArray_SmallerBuffer)
{
    lua_createtable(L, 1, 0);
    lua_pushnumber(L, 1);
    lua_pushstring(L, "one");
    lua_rawset(L, -3);
    lua_pushnumber(L, 2);
    lua_pushstring(L, "two");
    lua_rawset(L, -3);

    char* buffer[1] = { 0 };
    uint32_t result = dmFacebook::luaTableToCArray(L, 1, buffer, sizeof(buffer) / sizeof(buffer[0]));

    ASSERT_EQ(1, result);
    ASSERT_STRCASEEQ("one", buffer[0]);

    for (unsigned int i = 0; i < result; ++i)
    {
        free(buffer[i]);
    }
}

TEST_F(FBTest, luaTableToCArray_EmptyTable)
{
    lua_createtable(L, 1, 0);

    char* buffer[1] = { 0 };
    uint32_t result = dmFacebook::luaTableToCArray(L, 1, buffer, sizeof(buffer) / sizeof(buffer[0]));

    ASSERT_EQ(0, result);
}

TEST_F(FBTest, luaTableToCArray_SingleElement)
{
    lua_createtable(L, 1, 0);
    lua_pushnumber(L, 1);
    lua_pushstring(L, "one");
    lua_rawset(L, -3);

    char* buffer[1] = { 0 };
    uint32_t result = dmFacebook::luaTableToCArray(L, 1, buffer, sizeof(buffer) / sizeof(buffer[0]));

    ASSERT_EQ(1, result);
    ASSERT_STRCASEEQ("one", buffer[0]);

    for (unsigned int i = 0; i < result; ++i)
    {
        free(buffer[i]);
    }
}

TEST_F(FBTest, luaTableToCArray_MultipleElement)
{
    lua_createtable(L, 1, 0);
    lua_pushnumber(L, 1);
    lua_pushstring(L, "one");
    lua_rawset(L, -3);
    lua_pushnumber(L, 2);
    lua_pushstring(L, "two");
    lua_rawset(L, -3);
    lua_pushnumber(L, 3);
    lua_pushstring(L, "three");
    lua_rawset(L, -3);

    char* buffer[3] = { 0 };
    uint32_t result = dmFacebook::luaTableToCArray(L, 1, buffer, sizeof(buffer) / sizeof(buffer[0]));

    ASSERT_EQ(3, result);
    ASSERT_STRCASEEQ("one", buffer[0]);
    ASSERT_STRCASEEQ("two", buffer[1]);
    ASSERT_STRCASEEQ("three", buffer[2]);

    for (unsigned int i = 0; i < result; ++i)
    {
        free(buffer[i]);
    }
}

TEST_F(FBTest, luaTableTOCArray_InvalidType)
{
    lua_createtable(L, 1, 0);
    lua_pushnumber(L, 1);
        lua_newtable(L);
        lua_pushnumber(L, 1);
        lua_pushstring(L, "complex-type");
        lua_rawset(L, -3);
    lua_rawset(L, -3);

    char* buffer[1] = { 0 };
    uint32_t result = dmFacebook::luaTableToCArray(L, 1, buffer, sizeof(buffer) / sizeof(buffer[0]));

    ASSERT_EQ(-1, result);
}

static int WrapFailingCountCall(lua_State* L)
{
    size_t entry_count = 0;
    size_t len = dmFacebook::CountStringArrayLength(L, lua_gettop(L), entry_count);

    lua_pushinteger(L, entry_count);
    lua_pushinteger(L, len);
    return 2;
}

TEST_F(FBTest, CountStringArrayLength)
{
    // Test empty table
    lua_newtable(L);
    size_t entry_count = 0;
    size_t len = dmFacebook::CountStringArrayLength(L, lua_gettop(L), entry_count);
    ASSERT_EQ(0, len);
    ASSERT_EQ(0, entry_count);
    lua_pop(L, 1);

    // Test table with 3 string value
    lua_newtable(L);
    lua_pushnumber(L, 1);
    lua_pushstring(L, "one");
    lua_rawset(L, -3);
    lua_pushnumber(L, 2);
    lua_pushstring(L, "two");
    lua_rawset(L, -3);
    lua_pushnumber(L, 3);
    lua_pushstring(L, "three");
    lua_rawset(L, -3);

    entry_count = 0;
    len = dmFacebook::CountStringArrayLength(L, lua_gettop(L), entry_count);
    // Expected length: "one" + "two" + "three" = 3 + 3 + 5 = 11
    ASSERT_EQ(11, len);
    ASSERT_EQ(3, entry_count);
    lua_pop(L, 1);

    // Test table with 1 string value, 1 number value
    lua_newtable(L);
    lua_pushnumber(L, 1);
    lua_pushstring(L, "one");
    lua_rawset(L, -3);
    lua_pushnumber(L, 2);
    lua_pushnumber(L, 2);
    lua_rawset(L, -3);

    entry_count = 0;
    len = dmFacebook::CountStringArrayLength(L, lua_gettop(L), entry_count);
    // Expected length: "one" + "2" = 4
    ASSERT_EQ(4, len);
    ASSERT_EQ(2, entry_count);
    lua_pop(L, 1);


    // Test table with subtable entry
    // We wrap the call in a lua_pcall so that we can catch the Lua error.
    lua_pushcfunction(L, WrapFailingCountCall);
    lua_newtable(L);
    lua_pushnumber(L, 1);
    lua_newtable(L);
    lua_rawset(L, -3);

    int ret = lua_pcall(L, 1, 2, 0);
    ASSERT_NE(0, ret);
    ASSERT_STREQ("array arguments can only be strings (not table)", lua_tostring(L, -1));
    lua_pop(L, 1);
}

TEST_F(FBTest, SplitStringToTable)
{
    int top = lua_gettop(L);

    lua_newtable(L);
    int table_index = top + 1;

    const char t_empty[] = "";
    dmFacebook::SplitStringToTable(L, table_index, t_empty, ' ');

    // Should contain one entry with empty string
    ASSERT_EQ(table_index, lua_gettop(L));
    ASSERT_EQ(1, lua_objlen(L, table_index));

    lua_rawgeti(L, table_index, 1);
    ASSERT_EQ(LUA_TSTRING, lua_type(L, -1));
    ASSERT_STREQ(t_empty, lua_tostring(L, -1));
    lua_pop(L, 1);


    // pop and push empty table
    lua_pop(L, 1);
    lua_newtable(L);

    const char t_string_no_split[] = "asdasdasd";
    dmFacebook::SplitStringToTable(L, table_index, t_string_no_split, ' ');

    // Should contain one entry with a non splitted string
    ASSERT_EQ(table_index, lua_gettop(L));
    ASSERT_EQ(1, lua_objlen(L, table_index));

    lua_rawgeti(L, table_index, 1);
    ASSERT_EQ(LUA_TSTRING, lua_type(L, -1));
    ASSERT_STREQ(t_string_no_split, lua_tostring(L, -1));
    lua_pop(L, 1);


    // pop and push empty table
    lua_pop(L, 1);
    lua_newtable(L);

    const char t_string_commas[] = "asd,asd,asd";
    dmFacebook::SplitStringToTable(L, table_index, t_string_commas, ',');

    // Should contain three entries with "asd" in each
    ASSERT_EQ(table_index, lua_gettop(L));
    ASSERT_EQ(3, lua_objlen(L, table_index));

    lua_rawgeti(L, table_index, 1);
    ASSERT_EQ(LUA_TSTRING, lua_type(L, -1));
    ASSERT_STREQ("asd", lua_tostring(L, -1));
    lua_pop(L, 1);

    lua_rawgeti(L, table_index, 2);
    ASSERT_EQ(LUA_TSTRING, lua_type(L, -1));
    ASSERT_STREQ("asd", lua_tostring(L, -1));
    lua_pop(L, 1);

    lua_rawgeti(L, table_index, 3);
    ASSERT_EQ(LUA_TSTRING, lua_type(L, -1));
    ASSERT_STREQ("asd", lua_tostring(L, -1));
    lua_pop(L, 1);


    // pop and push empty table
    lua_pop(L, 1);
    lua_newtable(L);

    const char t_string_empty_commas[] = ",,";
    dmFacebook::SplitStringToTable(L, table_index, t_string_empty_commas, ',');

    // Should contain three entries with empty string in each
    ASSERT_EQ(table_index, lua_gettop(L));
    ASSERT_EQ(3, lua_objlen(L, table_index));

    lua_rawgeti(L, table_index, 1);
    ASSERT_EQ(LUA_TSTRING, lua_type(L, -1));
    ASSERT_STREQ(t_empty, lua_tostring(L, -1));
    lua_pop(L, 1);

    lua_rawgeti(L, table_index, 2);
    ASSERT_EQ(LUA_TSTRING, lua_type(L, -1));
    ASSERT_STREQ(t_empty, lua_tostring(L, -1));
    lua_pop(L, 1);

    lua_rawgeti(L, table_index, 3);
    ASSERT_EQ(LUA_TSTRING, lua_type(L, -1));
    ASSERT_STREQ(t_empty, lua_tostring(L, -1));
    lua_pop(L, 1);


    // pop and push empty table
    lua_pop(L, 1);
    lua_newtable(L);

    const char t_string_nullterm[] = "asd";
    dmFacebook::SplitStringToTable(L, table_index, t_string_nullterm, '\0');

    // Should contain one entry with the string "asd"
    ASSERT_EQ(table_index, lua_gettop(L));
    ASSERT_EQ(1, lua_objlen(L, table_index));

    lua_rawgeti(L, table_index, 1);
    ASSERT_EQ(LUA_TSTRING, lua_type(L, -1));
    ASSERT_STREQ(t_string_nullterm, lua_tostring(L, -1));
    lua_pop(L, 1);

    // pop table
    lua_pop(L, 1);
}

int main(int argc, char **argv)
{
    testing::InitGoogleTest(&argc, argv);

    int ret = RUN_ALL_TESTS();
    return ret;
}
