#include <gtest/gtest.h>

#include "script.h"

#include <dlib/dstrings.h>
#include <dlib/hash.h>
#include <dlib/log.h>

extern "C"
{
#include <lua/lauxlib.h>
#include <lua/lualib.h>
}

#define PATH_FORMAT "build/default/src/test/%s"

class ScriptMsgTest : public ::testing::Test
{
protected:
    virtual void SetUp()
    {
        L = lua_open();
        luaL_openlibs(L);
        dmScript::Initialize(L);
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
        dmLogError("%s", lua_tolstring(L, -1, 0));
        lua_pop(L, 1);
        return false;
    }
    return true;
}

bool RunString(lua_State* L, const char* script)
{
    if (luaL_dostring(L, script) != 0)
    {
        dmLogError("%s", lua_tolstring(L, -1, 0));
        lua_pop(L, 1);
        return false;
    }
    return true;
}

TEST_F(ScriptMsgTest, TestURLNewAndIndex)
{
    int top = lua_gettop(L);

    // empty
    ASSERT_TRUE(RunString(L,
        "local url = msg.url()\n"
        "assert(url.socket == nil, \"invalid socket\")\n"
        "assert(url.path == nil, \"invalid path\")\n"
        "assert(url.fragment == nil, \"invalid fragment\")\n"
        ));

    // nil
    ASSERT_TRUE(RunString(L,
        "local url = msg.url(nil)\n"
        "assert(url.socket == nil, \"invalid socket\")\n"
        "assert(url.path == nil, \"invalid path\")\n"
        "assert(url.fragment == nil, \"invalid fragment\")\n"
        ));

    // empty string
    ASSERT_TRUE(RunString(L,
        "local url = msg.url(\"\")\n"
        "assert(url.socket == nil, \"invalid socket\")\n"
        "assert(url.path == nil, \"invalid path\")\n"
        "assert(url.fragment == nil, \"invalid fragment\")\n"
        ));

    // socket string
    dmMessage::HSocket socket;
    ASSERT_EQ(dmMessage::RESULT_OK, dmMessage::NewSocket("test", &socket));
    ASSERT_TRUE(RunString(L,
        "local url = msg.url(\"test:\")\n"
        "assert(url.socket ~= nil, \"invalid socket\")\n"
        "assert(url.path == nil, \"invalid path\")\n"
        "assert(url.fragment == nil, \"invalid fragment\")\n"
        ));
    ASSERT_EQ(dmMessage::RESULT_OK, dmMessage::DeleteSocket(socket));

    // fragment string
    ASSERT_TRUE(RunString(L,
        "local url = msg.url(\"test\")\n"
        "assert(url.socket == nil, \"invalid socket\")\n"
        "assert(url.path == hash(\"test\"), \"invalid path\")\n"
        "assert(url.fragment == nil, \"invalid fragment\")\n"
        ));

    // path string
    ASSERT_TRUE(RunString(L,
        "local url = msg.url(\"#test\")\n"
        "assert(url.socket == nil, \"invalid socket\")\n"
        "assert(url.path == nil, \"invalid path\")\n"
        "assert(url.fragment == hash(\"test\"), \"invalid fragment\")\n"
        ));

    ASSERT_EQ(dmMessage::RESULT_OK, dmMessage::NewSocket("test", &socket));

    // socket arg string
    ASSERT_TRUE(RunString(L,
        "local url = msg.url(\"test\", \"\", \"\")\n"
        "assert(url.socket ~= nil, \"invalid socket\")\n"
        "assert(url.path == nil, \"invalid path\")\n"
        "assert(url.fragment == nil, \"invalid fragment\")\n"
        ));

    // socket arg value
    ASSERT_TRUE(RunString(L,
        "local url1 = msg.url(\"test:\")\n"
        "local url2 = msg.url(url1.socket, \"\", \"\")\n"
        "assert(url2.socket ~= nil, \"invalid socket\")\n"
        "assert(url2.path == nil, \"invalid path\")\n"
        "assert(url2.fragment == nil, \"invalid fragment\")\n"
        ));

    // path arg string
    ASSERT_TRUE(RunString(L,
        "local url = msg.url(\"test\", \"test\", \"\")\n"
        "assert(url.socket ~= nil, \"invalid socket\")\n"
        "assert(url.path == hash(\"test\"), \"invalid path\")\n"
        "assert(url.fragment == nil, \"invalid fragment\")\n"
        ));

    // path arg value
    ASSERT_TRUE(RunString(L,
        "local url = msg.url(\"test\", hash(\"test\"), \"\")\n"
        "assert(url.socket ~= nil, \"invalid socket\")\n"
        "assert(url.path == hash(\"test\"), \"invalid path\")\n"
        "assert(url.fragment == nil, \"invalid fragment\")\n"
        ));

    // fragment arg string
    ASSERT_TRUE(RunString(L,
        "local url = msg.url(\"test\", \"\", \"test\")\n"
        "assert(url.socket ~= nil, \"invalid socket\")\n"
        "assert(url.path == nil, \"invalid path\")\n"
        "assert(url.fragment == hash(\"test\"), \"invalid fragment\")\n"
        ));

    // fragment arg value
    ASSERT_TRUE(RunString(L,
        "local url = msg.url(\"test\", \"\", hash(\"test\"))\n"
        "assert(url.socket ~= nil, \"invalid socket\")\n"
        "assert(url.path == nil, \"invalid path\")\n"
        "assert(url.fragment == hash(\"test\"), \"invalid fragment\")\n"
        ));

    ASSERT_EQ(dmMessage::RESULT_OK, dmMessage::DeleteSocket(socket));

    ASSERT_EQ(top, lua_gettop(L));
}

TEST_F(ScriptMsgTest, TestFailURLNewAndIndex)
{
    int top = lua_gettop(L);

    // invalid arg
    ASSERT_FALSE(RunString(L,
        "msg.url({})\n"
        ));
    // malformed
    ASSERT_FALSE(RunString(L,
        "msg.url(\"test:test:\")\n"
        ));
    // invalid socket
    ASSERT_FALSE(RunString(L,
        "msg.url(\":\")\n"
        ));
    // socket not found
    ASSERT_FALSE(RunString(L,
        "msg.url(\"test:\")\n"
        ));
    // invalid socket arg
    ASSERT_FALSE(RunString(L,
        "msg.url(\"\", nil, nil)\n"
        ));
    // socket arg not found
    ASSERT_FALSE(RunString(L,
        "msg.url(\"test\", nil, nil)\n"
        ));
    // path arg
    ASSERT_FALSE(RunString(L,
        "msg.url(nil, {}, nil)\n"
        ));
    // fragment arg
    ASSERT_FALSE(RunString(L,
        "msg.url(nil, nil, {})\n"
        ));

    ASSERT_EQ(top, lua_gettop(L));
}

TEST_F(ScriptMsgTest, TestURLToString)
{
    int top = lua_gettop(L);

    dmMessage::HSocket socket;
    dmMessage::HSocket overflow_socket;
    ASSERT_EQ(dmMessage::RESULT_OK, dmMessage::NewSocket("socket", &socket));
    ASSERT_EQ(dmMessage::RESULT_OK, dmMessage::NewSocket("very_very_very_very_very_very_very_very_very_very_very_very_socket", &overflow_socket));
    ASSERT_TRUE(RunString(L,
        "local url = msg.url()\n"
        "print(tostring(url))\n"
        "url = msg.url(\"socket\", \"path\", \"test\")\n"
        "print(tostring(url))\n"
        "-- overflow\n"
        "url = msg.url(\"very_very_very_very_very_very_very_very_very_very_very_very_socket\", \"path\", \"test\")\n"
        "print(tostring(url))\n"
        ));
    ASSERT_EQ(dmMessage::RESULT_OK, dmMessage::DeleteSocket(socket));
    ASSERT_EQ(dmMessage::RESULT_OK, dmMessage::DeleteSocket(overflow_socket));

    ASSERT_EQ(top, lua_gettop(L));
}

TEST_F(ScriptMsgTest, TestURLConcat)
{
    int top = lua_gettop(L);

    dmMessage::HSocket socket;
    dmMessage::HSocket overflow_socket;
    ASSERT_EQ(dmMessage::RESULT_OK, dmMessage::NewSocket("socket", &socket));
    ASSERT_EQ(dmMessage::RESULT_OK, dmMessage::NewSocket("very_very_very_very_very_very_very_very_very_very_very_very_socket", &overflow_socket));
    ASSERT_TRUE(RunString(L,
        "local url = msg.url()\n"
        "print(\"url: \" .. url)\n"
        "url = msg.url(\"socket\", \"path\", \"fragment\")\n"
        "print(\"url: \" .. url)\n"
        "-- overflow\n"
        "url = msg.url(\"very_very_very_very_very_very_very_very_very_very_very_very_socket\", \"path\", \"fragment\")\n"
        "print(\"url: \" .. url)\n"
        ));
    ASSERT_EQ(dmMessage::RESULT_OK, dmMessage::DeleteSocket(socket));
    ASSERT_EQ(dmMessage::RESULT_OK, dmMessage::DeleteSocket(overflow_socket));

    ASSERT_EQ(top, lua_gettop(L));
}

TEST_F(ScriptMsgTest, TestURLNewIndex)
{
    int top = lua_gettop(L);

    dmMessage::HSocket socket;
    ASSERT_EQ(dmMessage::RESULT_OK, dmMessage::NewSocket("socket", &socket));
    ASSERT_TRUE(RunString(L,
        "local url1 = msg.url()\n"
        "local url2 = msg.url(\"socket\", nil, nil)\n"
        "url1.socket = url2.socket\n"
        "assert(url1.socket == url2.socket)\n"
        "url1.socket = nil\n"
        "assert(url1.socket ~= url2.socket)\n"
        "assert(url1.socket == nil)\n"
        "url1.socket = \"socket\"\n"
        "assert(url1.socket == url2.socket)\n"
        "url1.path = \"path\"\n"
        "url2.path = hash(\"path\")\n"
        "assert(url1.path == url2.path)\n"
        "url1.path = nil\n"
        "assert(url1.path == nil)\n"
        "url1.fragment = \"fragment\"\n"
        "url2.fragment = hash(\"fragment\")\n"
        "assert(url1.fragment == url2.fragment)\n"
        "url1.fragment = nil\n"
        "assert(url1.fragment == nil)\n"
        ));
    ASSERT_EQ(dmMessage::RESULT_OK, dmMessage::DeleteSocket(socket));

    ASSERT_EQ(top, lua_gettop(L));
}

TEST_F(ScriptMsgTest, TestFailURLNewIndex)
{
    int top = lua_gettop(L);

    ASSERT_FALSE(RunString(L,
        "local url = msg.url()\n"
        "url.socket = {}\n"
        ));
    ASSERT_FALSE(RunString(L,
        "local url = msg.url()\n"
        "url.path = {}\n"
        ));
    ASSERT_FALSE(RunString(L,
        "local url = msg.url()\n"
        "url.fragment = {}\n"
        ));

    ASSERT_EQ(top, lua_gettop(L));
}

TEST_F(ScriptMsgTest, TestURLEq)
{
    int top = lua_gettop(L);

    dmMessage::HSocket socket;
    ASSERT_EQ(dmMessage::RESULT_OK, dmMessage::NewSocket("socket", &socket));
    ASSERT_TRUE(RunString(L,
        "local url1 = msg.url(\"socket\", \"path\", \"fragment\")\n"
        "local url2 = msg.url()\n"
        "assert(url1 ~= url2)\n"
        "url2.socket = \"socket\"\n"
        "url2.path = \"path\"\n"
        "url2.fragment = \"fragment\"\n"
        "assert(url1 == url2)\n"
        "url2.socket = nil\n"
        "assert(url1 ~= url2)\n"
        "url2.socket = \"socket\"\n"
        "assert(url1 == url2)\n"
        "url2.path = nil\n"
        "assert(url1 ~= url2)\n"
        "url2.path = \"path\"\n"
        "assert(url1 == url2)\n"
        "url2.fragment = nil\n"
        "assert(url1 ~= url2)\n"
        "url2.fragment = \"fragment\"\n"
        "assert(url1 == url2)\n"
        "assert(url1 ~= 1)\n"
        ));
    ASSERT_EQ(dmMessage::RESULT_OK, dmMessage::DeleteSocket(socket));

    ASSERT_EQ(top, lua_gettop(L));
}

int main(int argc, char **argv)
{
    testing::InitGoogleTest(&argc, argv);

    int ret = RUN_ALL_TESTS();
    return ret;
}
