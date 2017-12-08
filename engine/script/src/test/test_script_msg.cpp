#include <gtest/gtest.h>
#include <string.h>

#include "script.h"
#include "test/test_ddf.h"

#include <dlib/dstrings.h>
#include <dlib/hash.h>
#include <dlib/log.h>
#include <dlib/time.h>

extern "C"
{
#include <lua/lauxlib.h>
#include <lua/lualib.h>
}

#define PATH_FORMAT "build/default/src/test/%s"

#define DEFAULT_URL "__default_url"

int ResolvePathCallback(lua_State* L)
{
    uint32_t* user_data = (uint32_t*)lua_touserdata(L, 1);
    assert(*user_data == 1);
    const char* path = luaL_checkstring(L, 2);
    dmScript::PushHash(L, dmHashString64(path));
    return 1;
}

int GetURLCallback(lua_State* L)
{
    uint32_t* user_data = (uint32_t*)lua_touserdata(L, 1);
    assert(*user_data == 1);
    lua_getglobal(L, DEFAULT_URL);
    return 1;
}

static const luaL_reg META_TABLE[] =
{
    {dmScript::META_TABLE_RESOLVE_PATH, ResolvePathCallback},
    {dmScript::META_TABLE_GET_URL,      GetURLCallback},
    {0, 0}
};

class ScriptMsgTest : public ::testing::Test
{
protected:
    virtual void SetUp()
    {
        dmScript::NewContextParams params;
        m_ScriptContext = dmScript::NewContext(&params);
        dmScript::Initialize(m_ScriptContext);
        L = dmScript::GetLuaState(m_ScriptContext);

        assert(dmMessage::NewSocket("default_socket", &m_DefaultURL.m_Socket) == dmMessage::RESULT_OK);
        m_DefaultURL.m_Path = dmHashString64("default_path");
        m_DefaultURL.m_Fragment = dmHashString64("default_fragment");
        dmScript::PushURL(L, m_DefaultURL);
        lua_setglobal(L, DEFAULT_URL);

        int top = lua_gettop(L);
        (void)top;
        uint32_t* user_data = (uint32_t*)lua_newuserdata(L, 4);
        *user_data = 1;
        luaL_newmetatable(L, "ScriptMsgTest");
        luaL_register(L, 0, META_TABLE);
        lua_setmetatable(L, -2);
        dmScript::SetInstance(L);
        assert(top == lua_gettop(L));
    }

    virtual void TearDown()
    {
        lua_pushnil(L);
        dmScript::SetInstance(L);
        dmMessage::DeleteSocket(m_DefaultURL.m_Socket);
        dmScript::Finalize(m_ScriptContext);
        dmScript::DeleteContext(m_ScriptContext);
    }

    dmScript::HContext m_ScriptContext;
    lua_State* L;
    dmMessage::URL m_DefaultURL;
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
        "assert(url.socket == __default_url.socket, \"invalid socket\")\n"
        "assert(url.path == __default_url.path, \"invalid path\")\n"
        "assert(url.fragment == __default_url.fragment, \"invalid fragment\")\n"
       ));

    // empty string
    ASSERT_TRUE(RunString(L,
        "local url = msg.url(\"\")\n"
        "assert(url.socket == __default_url.socket, \"invalid socket\")\n"
        "assert(url.path == __default_url.path, \"invalid path\")\n"
        "assert(url.fragment == __default_url.fragment, \"invalid fragment\")\n"
        ));

    // default path
    ASSERT_TRUE(RunString(L,
        "local url = msg.url(\".\")\n"
        "assert(url.socket == __default_url.socket, \"invalid socket\")\n"
        "assert(url.path == __default_url.path, \"invalid path\")\n"
        "assert(url.fragment == nil, \"invalid fragment\")\n"
       ));

    // default fragment
    ASSERT_TRUE(RunString(L,
        "local url = msg.url(\"#\")\n"
        "assert(url.socket == __default_url.socket, \"invalid socket\")\n"
        "assert(url.path == __default_url.path, \"invalid path\")\n"
        "assert(url.fragment == __default_url.fragment, \"invalid fragment\")\n"
       ));

    // socket string
    dmMessage::HSocket socket;
    ASSERT_EQ(dmMessage::RESULT_OK, dmMessage::NewSocket("test", &socket));
    ASSERT_TRUE(RunString(L,
        "local url = msg.url(\"test:\")\n"
        "assert(url.socket ~= __default_url.socket, \"invalid socket\")\n"
        "assert(url.path == nil, \"invalid path\")\n"
        "assert(url.fragment == nil, \"invalid fragment\")\n"
        ));
    ASSERT_EQ(dmMessage::RESULT_OK, dmMessage::DeleteSocket(socket));

    // fragment string
    ASSERT_TRUE(RunString(L,
        "local url = msg.url(\"test\")\n"
        "assert(url.socket == __default_url.socket, \"invalid socket\")\n"
        "assert(url.path == hash(\"test\"), \"invalid path\")\n"
        "assert(url.fragment == nil, \"invalid fragment\")\n"
        ));

    // path string
    ASSERT_TRUE(RunString(L,
        "local url = msg.url(\"#test\")\n"
        "assert(url.socket == __default_url.socket, \"invalid socket\")\n"
        "assert(url.path == __default_url.path, \"invalid path\")\n"
        "assert(url.fragment == hash(\"test\"), \"invalid fragment\")\n"
        ));

    ASSERT_EQ(dmMessage::RESULT_OK, dmMessage::NewSocket("test", &socket));

    // socket arg string
    ASSERT_TRUE(RunString(L,
        "local url = msg.url(\"test\", \"\", \"\")\n"
        "assert(url.socket ~= __default_url.socket, \"invalid socket\")\n"
        "assert(url.path == nil, \"invalid path\")\n"
        "assert(url.fragment == nil, \"invalid fragment\")\n"
        ));

    // socket arg value
    ASSERT_TRUE(RunString(L,
        "local url1 = msg.url(\"test:\")\n"
        "local url2 = msg.url(url1.socket, \"\", \"\")\n"
        "assert(url2.socket ~= __default_url.socket, \"invalid socket\")\n"
        "assert(url2.path == nil, \"invalid path\")\n"
        "assert(url2.fragment == nil, \"invalid fragment\")\n"
        ));

    // path arg string
    ASSERT_TRUE(RunString(L,
        "local url = msg.url(\"test\", \"test\", \"\")\n"
        "assert(url.socket ~= __default_url.socket, \"invalid socket\")\n"
        "assert(url.path == hash(\"test\"), \"invalid path\")\n"
        "assert(url.fragment == nil, \"invalid fragment\")\n"
        ));

    // path arg value
    ASSERT_TRUE(RunString(L,
        "local url = msg.url(\"test\", hash(\"test\"), \"\")\n"
        "assert(url.socket ~= __default_url.socket, \"invalid socket\")\n"
        "assert(url.path == hash(\"test\"), \"invalid path\")\n"
        "assert(url.fragment == nil, \"invalid fragment\")\n"
        ));

    // path arg value
    ASSERT_TRUE(RunString(L,
        "local url = msg.url(\"test\", hash(\"test\"), nil)\n"
        "assert(url.socket ~= __default_url.socket, \"invalid socket\")\n"
        "assert(url.path == hash(\"test\"), \"invalid path\")\n"
        "assert(url.fragment == nil, \"invalid fragment\")\n"
        ));

    // fragment arg string
    ASSERT_TRUE(RunString(L,
        "local url = msg.url(\"test\", \"\", \"test\")\n"
        "assert(url.socket ~= __default_url.socket, \"invalid socket\")\n"
        "assert(url.path == nil, \"invalid path\")\n"
        "assert(url.fragment == hash(\"test\"), \"invalid fragment\")\n"
        ));

    // fragment arg value
    ASSERT_TRUE(RunString(L,
        "local url = msg.url(\"test\", \"\", hash(\"test\"))\n"
        "assert(url.socket ~= __default_url.socket, \"invalid socket\")\n"
        "assert(url.path == nil, \"invalid path\")\n"
        "assert(url.fragment == hash(\"test\"), \"invalid fragment\")\n"
        ));

    ASSERT_EQ(dmMessage::RESULT_OK, dmMessage::DeleteSocket(socket));

    ASSERT_EQ(top, lua_gettop(L));
}

static void ExtractURL(const dmMessage::StringURL& url, char* socket, char* path, char* fragment)
{
    memcpy(socket, url.m_Socket, url.m_SocketSize);
    socket[url.m_SocketSize] = 0;
    memcpy(path, url.m_Path, url.m_PathSize);
    path[url.m_PathSize] = 0;
    memcpy(fragment, url.m_Fragment, url.m_FragmentSize);
    fragment[url.m_FragmentSize] = 0;
}

TEST_F(ScriptMsgTest, ResolveURL)
{
    int top = lua_gettop(L);

    dmHashEnableReverseHash(true);

    char socket[256];
    char path[256];
    char fragment[256];
    dmMessage::StringURL string_url;

    // The ParseURL function
    ASSERT_EQ(dmMessage::RESULT_OK, dmMessage::ParseURL(0, &string_url));
    ExtractURL(string_url, socket, path, fragment);
    ASSERT_EQ( dmHashString64(""), dmHashString64(socket));
    ASSERT_EQ( dmHashString64(""), dmHashString64(path));
    ASSERT_EQ( dmHashString64(""), dmHashString64(fragment));

    ASSERT_EQ(dmMessage::RESULT_OK, dmMessage::ParseURL("", &string_url));
    ExtractURL(string_url, socket, path, fragment);
    ASSERT_EQ( dmHashString64(""), dmHashString64(socket));
    ASSERT_EQ( dmHashString64(""), dmHashString64(path));
    ASSERT_EQ( dmHashString64(""), dmHashString64(fragment));

    ASSERT_EQ(dmMessage::RESULT_OK, dmMessage::ParseURL("#", &string_url));
    ExtractURL(string_url, socket, path, fragment);
    ASSERT_EQ( dmHashString64(""), dmHashString64(socket));
    ASSERT_EQ( dmHashString64(""), dmHashString64(path));
    ASSERT_EQ( dmHashString64(""), dmHashString64(fragment));

    ASSERT_EQ(dmMessage::RESULT_OK, dmMessage::ParseURL("path#", &string_url));
    ExtractURL(string_url, socket, path, fragment);
    ASSERT_EQ( dmHashString64(""), dmHashString64(socket));
    ASSERT_EQ( dmHashString64("path"), dmHashString64(path));
    ASSERT_EQ( dmHashString64(""), dmHashString64(fragment));

    ASSERT_EQ(dmMessage::RESULT_OK, dmMessage::ParseURL("#fragment", &string_url));
    ExtractURL(string_url, socket, path, fragment);
    ASSERT_EQ( dmHashString64(""), dmHashString64(socket));
    ASSERT_EQ( dmHashString64(""), dmHashString64(path));
    ASSERT_EQ( dmHashString64("fragment"), dmHashString64(fragment));

    ASSERT_EQ(dmMessage::RESULT_OK, dmMessage::ParseURL("path#fragment", &string_url));
    ExtractURL(string_url, socket, path, fragment);
    ASSERT_EQ( dmHashString64(""), dmHashString64(socket));
    ASSERT_EQ( dmHashString64("path"), dmHashString64(path));
    ASSERT_EQ( dmHashString64("fragment"), dmHashString64(fragment));


    // The resolve function
    dmMessage::URL receiver;

    // nil
    lua_pushnil(L);
    memset(&receiver, 0, sizeof(receiver));
    dmScript::ResolveURL(L, 1, &receiver, 0x0);
    ASSERT_EQ(dmHashString64("default_path"), receiver.m_Path);
    ASSERT_EQ(dmHashString64("default_fragment"), receiver.m_Fragment);
    ASSERT_EQ(m_DefaultURL.m_Socket, receiver.m_Socket);
    lua_pop(L, 1);

    //
    lua_pushstring(L, "");
    memset(&receiver, 0, sizeof(receiver));
    dmScript::ResolveURL(L, 1, &receiver, 0x0);
    ASSERT_EQ(dmHashString64("default_path"), receiver.m_Path);
    ASSERT_EQ(dmHashString64("default_fragment"), receiver.m_Fragment);
    ASSERT_EQ(m_DefaultURL.m_Socket, receiver.m_Socket);
    lua_pop(L, 1);

    //
    lua_pushstring(L, "foo#bar");
    memset(&receiver, 0, sizeof(receiver));
    dmScript::ResolveURL(L, 1, &receiver, 0x0);
    ASSERT_EQ(dmHashString64("foo"), receiver.m_Path);
    ASSERT_EQ(dmHashString64("bar"), receiver.m_Fragment);
    ASSERT_EQ(m_DefaultURL.m_Socket, receiver.m_Socket);
    lua_pop(L, 1);

    //
    lua_pushstring(L, "#");
    memset(&receiver, 0, sizeof(receiver));
    dmScript::ResolveURL(L, 1, &receiver, 0x0);
    ASSERT_EQ(dmHashString64("default_path"), receiver.m_Path);
    ASSERT_EQ(dmHashString64("default_fragment"), receiver.m_Fragment);
    ASSERT_EQ(m_DefaultURL.m_Socket, receiver.m_Socket);
    lua_pop(L, 1);

    //
    lua_pushstring(L, "#fragment");
    memset(&receiver, 0, sizeof(receiver));
    dmScript::ResolveURL(L, 1, &receiver, 0x0);
    ASSERT_EQ(dmHashString64("default_path"), receiver.m_Path);
    ASSERT_EQ(dmHashString64("fragment"), receiver.m_Fragment);
    ASSERT_EQ(m_DefaultURL.m_Socket, receiver.m_Socket);
    lua_pop(L, 1);

    //
    lua_pushstring(L, ".");
    memset(&receiver, 0, sizeof(receiver));
    dmScript::ResolveURL(L, 1, &receiver, 0x0);
    ASSERT_EQ(dmHashString64("default_path"), receiver.m_Path);
    ASSERT_EQ(0, receiver.m_Fragment);
    ASSERT_EQ(m_DefaultURL.m_Socket, receiver.m_Socket);
    lua_pop(L, 1);

    //
    lua_pushstring(L, "foo");
    memset(&receiver, 0, sizeof(receiver));
    dmScript::ResolveURL(L, 1, &receiver, 0x0);
    ASSERT_EQ(dmHashString64("foo"), receiver.m_Path);
    ASSERT_EQ(0, receiver.m_Fragment);
    ASSERT_EQ(m_DefaultURL.m_Socket, receiver.m_Socket);
    lua_pop(L, 1);


    ASSERT_EQ(top, lua_gettop(L));

    dmHashEnableReverseHash(false);
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
    // invalid socket arg
    ASSERT_FALSE(RunString(L,
        "msg.url(\"\", nil, nil)\n"
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

    dmHashEnableReverseHash(true);

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

    ASSERT_TRUE(RunString(L,
        "local url = msg.url()\n"
        "-- the socket doesn't exist yet\n"
        "url = msg.url(\"socket_not_exist\", \"path\", \"test\")\n"
        "print(tostring(url))\n"
        "assert(url.socket == hash(\"socket_not_exist\"))\n"
        ));

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

    // socket not created yet
    ASSERT_TRUE(RunString(L,
        "msg.url(\"test:\")\n"
        ));

    // socket arg not found
    ASSERT_TRUE(RunString(L,
        "msg.url(\"test\", nil, nil)\n"
        ));

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

TEST_F(ScriptMsgTest, TestURLGlobal)
{
    lua_pushnil(L);
    lua_setglobal(L, DEFAULT_URL);

    ASSERT_TRUE(RunString(L,
            "local url1 = msg.url(\"default_socket:/path#fragment\")\n"
            "assert(url1.path == hash(\"/path\"))\n"
            "print(url1.fragment)\n"
            "assert(url1.fragment == hash(\"fragment\"))\n"));

    ASSERT_FALSE(RunString(L,
            "local url1 = msg.url(\"path#fragment\")\n"));
}

void DispatchCallbackDDF(dmMessage::Message *message, void* user_ptr)
{
    assert(message->m_Descriptor != 0);
    dmDDF::Descriptor* descriptor = (dmDDF::Descriptor*)message->m_Descriptor;
    if (descriptor == TestScript::SubMsg::m_DDFDescriptor)
    {
        TestScript::SubMsg* msg = (TestScript::SubMsg*)message->m_Data;
        *((uint32_t*)user_ptr) = msg->m_UintValue;
    }
    else if (descriptor == TestScript::EmptyMsg::m_DDFDescriptor)
    {
        *((uint32_t*)user_ptr) = 2;
    }
}

struct TableUserData
{
    TableUserData() { dmMessage::ResetURL(m_URL); }

    lua_State* L;
    uint32_t m_TestValue;
    dmMessage::URL m_URL;
};

void DispatchCallbackTable(dmMessage::Message *message, void* user_ptr)
{
    assert(message->m_Id == dmHashString64("table"));
    TableUserData* user_data = (TableUserData*)user_ptr;
    dmScript::PushTable(user_data->L, (const char*)message->m_Data, message->m_DataSize);
    lua_getfield(user_data->L, -1, "uint_value");
    user_data->m_TestValue = (uint32_t) lua_tonumber(user_data->L, -1);
    lua_pop(user_data->L, 2);
    assert(user_data->m_URL.m_Socket == message->m_Receiver.m_Socket);
    assert(user_data->m_URL.m_Path == message->m_Receiver.m_Path);
    assert(user_data->m_URL.m_Fragment == message->m_Receiver.m_Fragment);
}

TEST_F(ScriptMsgTest, TestPost)
{
    int top = lua_gettop(L);

    // DDF to default socket
    ASSERT_TRUE(RunString(L,
        "msg.post(\".\", \"sub_msg\", {uint_value = 1})\n"
        ));
    uint32_t test_value = 0;
    ASSERT_EQ(1u, dmMessage::Dispatch(m_DefaultURL.m_Socket, DispatchCallbackDDF, &test_value));
    ASSERT_EQ(1u, test_value);

    // DDF to default socket
    ASSERT_TRUE(RunString(L,
        "msg.post(\"\", \"sub_msg\", {uint_value = 1})\n"
        ));
    test_value = 0;
    ASSERT_EQ(1u, dmMessage::Dispatch(m_DefaultURL.m_Socket, DispatchCallbackDDF, &test_value));
    ASSERT_EQ(1u, test_value);

    dmMessage::HSocket socket;
    ASSERT_EQ(dmMessage::RESULT_OK, dmMessage::NewSocket("socket", &socket));

    // DDF
    ASSERT_TRUE(RunString(L,
        "msg.post(\"socket:\", \"sub_msg\", {uint_value = 1})\n"
        ));
    test_value = 0;
    dmMessage::Dispatch(socket, DispatchCallbackDDF, &test_value);
    ASSERT_EQ(1u, test_value);

    // Empty DDF
    ASSERT_TRUE(RunString(L,
        "msg.post(\"socket:\", \"empty_msg\")\n"
        ));
    test_value = 0;
    dmMessage::Dispatch(socket, DispatchCallbackDDF, &test_value);
    ASSERT_EQ(2u, test_value);

    // table
    ASSERT_TRUE(RunString(L,
        "msg.post(\"socket:\", \"table\", {uint_value = 1})\n"
        ));
    TableUserData user_data;
    user_data.L = L;
    user_data.m_TestValue = 0;
    user_data.m_URL.m_Socket = socket;
    ASSERT_EQ(1u, dmMessage::Dispatch(socket, DispatchCallbackTable, &user_data));
    ASSERT_EQ(1u, user_data.m_TestValue);

    // table, full url
    ASSERT_TRUE(RunString(L,
        "msg.post(\"socket:path2#fragment2\", \"table\", {uint_value = 1})\n"
        ));
    user_data.m_URL.m_Socket = socket;
    user_data.m_URL.m_Path = dmHashString64("path2");
    user_data.m_URL.m_Fragment = dmHashString64("fragment2");
    ASSERT_EQ(1u, dmMessage::Dispatch(socket, DispatchCallbackTable, &user_data));
    ASSERT_EQ(1u, user_data.m_TestValue);

    // table, resolve path
    ASSERT_TRUE(RunString(L,
        "msg.post(\"path2#fragment2\", \"table\", {uint_value = 1})\n"
        ));
    user_data.m_URL.m_Socket = m_DefaultURL.m_Socket;
    user_data.m_URL.m_Path = dmHashString64("path2");
    user_data.m_URL.m_Fragment = dmHashString64("fragment2");
    ASSERT_EQ(1u, dmMessage::Dispatch(m_DefaultURL.m_Socket, DispatchCallbackTable, &user_data));
    ASSERT_EQ(1u, user_data.m_TestValue);

    ASSERT_EQ(dmMessage::RESULT_OK, dmMessage::DeleteSocket(socket));

    ASSERT_EQ(top, lua_gettop(L));
}

TEST_F(ScriptMsgTest, TestFailPost)
{
    int top = lua_gettop(L);

    dmMessage::HSocket socket;
    ASSERT_EQ(dmMessage::RESULT_OK, dmMessage::NewSocket("socket", &socket));

    ASSERT_FALSE(RunString(L,
        "msg.post(\"socket2:\", \"sub_msg\", {uint_value = 1})\n"
        ));

    ASSERT_FALSE(RunString(L,
        "msg.post(\"#:\", \"sub_msg\", {uint_value = 1})\n"
        ));

    ASSERT_FALSE(RunString(L,
        "msg.post(\"::\", \"sub_msg\", {uint_value = 1})\n"
        ));

    ASSERT_EQ(dmMessage::RESULT_OK, dmMessage::DeleteSocket(socket));

    ASSERT_EQ(top, lua_gettop(L));
}


TEST_F(ScriptMsgTest, TestURLCreateBeforeSocket)
{
    int top = lua_gettop(L);

    dmHashEnableReverseHash(true);

    ASSERT_FALSE(RunString(L,
        "local url = msg.url(\"socket:\")"
        "msg.post(url, \"table\", {uint_value = 1})\n"
        ));

    dmMessage::HSocket socket;
    ASSERT_EQ(dmMessage::RESULT_OK, dmMessage::NewSocket("socket", &socket));

    ASSERT_TRUE(RunString(L,
        "local url = msg.url(\"socket:\")"
        "msg.post(url, \"table\", {uint_value = 2})\n"
        ));
    TableUserData user_data;
    user_data.L = L;
    user_data.m_TestValue = 0;
    user_data.m_URL.m_Socket = socket;
    ASSERT_EQ(1u, dmMessage::Dispatch(socket, DispatchCallbackTable, &user_data));
    ASSERT_EQ(2u, user_data.m_TestValue);

    ASSERT_EQ(dmMessage::RESULT_OK, dmMessage::DeleteSocket(socket));

    ASSERT_FALSE(RunString(L,
        "local url = msg.url(\"socket:\")"
        "msg.post(url, \"table\", {uint_value = 1})\n"
        ));

    ASSERT_EQ(top, lua_gettop(L));
}


TEST_F(ScriptMsgTest, TestPerf)
{
    uint64_t time = dmTime::GetTime();
    uint32_t count = 10000;
    char program[256];
    DM_SNPRINTF(program, 256,
        "local count = %u\n"
        "for i = 1,count do\n"
        "    msg.post(\"test_path\", \"table\", {uint_value = 1})\n"
        "end\n",
        count);
    ASSERT_TRUE(RunString(L, program));
    time = dmTime::GetTime() - time;
    printf("Time per post: %.4f\n", time / (double)count);
}

TEST_F(ScriptMsgTest, TestPostDeletedSocket)
{
    dmMessage::HSocket socket;
    ASSERT_EQ(dmMessage::RESULT_OK, dmMessage::NewSocket("test_socket", &socket));

    ASSERT_TRUE(RunString(L, "test_url = msg.url(\"test_socket:\")"));
    ASSERT_TRUE(RunString(L, "msg.post(test_url, \"test_message\")"));

    ASSERT_EQ(1u, dmMessage::Consume(socket));

    dmMessage::DeleteSocket(socket);

    ASSERT_FALSE(RunString(L, "msg.post(test_url, \"test_message\")"));
}

int main(int argc, char **argv)
{
    dmDDF::RegisterAllTypes();
    testing::InitGoogleTest(&argc, argv);
    int ret = RUN_ALL_TESTS();
    return ret;
}
