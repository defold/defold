#include <gtest/gtest.h>

#include "../facebook.h"
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

TEST_F(FBTest, DialogParams)
{
    lua_State* L = luaL_newstate();
    lua_createtable(L, 1, 0);
    lua_pushstring(L, "142175679447464,116897941987429");
    lua_setfield(L, 1, "to");
    lua_pushnumber(L, 1337);
    lua_setfield(L, 1, "another_key");

    int json_max_length = 1024;
    char json[json_max_length];
    int ret = dmFacebook::LuaDialogParamsToJson(L, 1, json, json_max_length);
    ASSERT_EQ(1, ret);

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
    ASSERT_EQ(dmJson::TYPE_STRING, doc.m_Nodes[4].m_Type); // "1337"

    dmJson::Free(&doc);
}

TEST_F(FBTest, DialogParamsWithArray)
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

    int json_max_length = 1024;
    char json[json_max_length];
    int ret = dmFacebook::LuaDialogParamsToJson(L, 1, json, json_max_length);
    ASSERT_EQ(1, ret);

    // parse json document
    dmJson::Document doc;
    dmJson::Result r = dmJson::Parse(json, &doc);
    ASSERT_EQ(dmJson::RESULT_OK, r);
    ASSERT_EQ(3, doc.m_NodeCount); // json-obj + "suggestions" + "userid1, userid2, userid3"

    ASSERT_EQ(dmJson::TYPE_OBJECT, doc.m_Nodes[0].m_Type);
    for (int i = 1; i < doc.m_NodeCount; ++i) {
        ASSERT_EQ(dmJson::TYPE_STRING, doc.m_Nodes[i].m_Type);
    }
    dmJson::Free(&doc);
}

int main(int argc, char **argv)
{
    testing::InitGoogleTest(&argc, argv);

    int ret = RUN_ALL_TESTS();
    return ret;
}
