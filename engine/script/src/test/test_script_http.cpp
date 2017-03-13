#include <gtest/gtest.h>

#include "script.h"
#include "script_http.h" // to set the timeout

#include <dlib/configfile.h>
#include <dlib/dstrings.h>
#include <dlib/hash.h>
#include <dlib/log.h>
#include <dlib/time.h>
#include <dlib/socket.h>
#include <dlib/thread.h>
#include <dlib/sys.h>

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

class ScriptHttpTest : public ::testing::Test
{
public:
    int m_HttpResponseCount;
    uint16_t m_WebServerPort;
    dmScript::HContext m_ScriptContext;
    lua_State* L;
    dmMessage::URL m_DefaultURL;
    dmConfigFile::HConfig m_ConfigFile;
    int m_NumberOfFails;

protected:

    virtual void SetUp()
    {
        dmConfigFile::Result r = dmConfigFile::Load("src/test/test.config", 0, 0, &m_ConfigFile);
        ASSERT_EQ(dmConfigFile::RESULT_OK, r);

        m_HttpResponseCount = 0;

        m_ScriptContext = dmScript::NewContext(m_ConfigFile, 0, true);
        dmScript::Initialize(m_ScriptContext);
        L = dmScript::GetLuaState(m_ScriptContext);

        ASSERT_EQ(dmMessage::RESULT_OK, dmMessage::NewSocket("default_socket", &m_DefaultURL.m_Socket));
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

        m_WebServerPort = 9001;
        m_NumberOfFails = 0;
    }

    virtual void TearDown()
    {
        lua_pushnil(L);
        dmScript::SetInstance(L);

        if (m_DefaultURL.m_Socket) {
            dmMessage::DeleteSocket(m_DefaultURL.m_Socket);
        }
        dmScript::Finalize(m_ScriptContext);
        dmScript::DeleteContext(m_ScriptContext);
        dmConfigFile::Delete(m_ConfigFile);
    }
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

void DispatchCallbackDDF(dmMessage::Message *message, void* user_ptr)
{
    ScriptHttpTest* test = (ScriptHttpTest*) user_ptr;
    test->m_HttpResponseCount++;
    lua_State* L = test->L;
    assert(message->m_Descriptor != 0);
    dmDDF::Descriptor* descriptor = (dmDDF::Descriptor*)message->m_Descriptor;

    int ref = message->m_Receiver.m_Function - 2;
    lua_rawgeti(L, LUA_REGISTRYINDEX, ref);
    dmScript::Unref(L, LUA_REGISTRYINDEX, ref);
    lua_gc(L, LUA_GCCOLLECT, 0);

    dmScript::PushDDF(L, descriptor, (const char*)&message->m_Data[0]);
    int ret = dmScript::PCall(L, 1, 0);
    test->m_NumberOfFails += ret == 0 ? 0 : 1;
    ASSERT_EQ(0, ret);
}

TEST_F(ScriptHttpTest, TestPost)
{
    int top = lua_gettop(L);

    ASSERT_TRUE(RunFile(L, "test_http.luac"));

    char buf[1024];
    DM_SNPRINTF(buf, sizeof(buf), "PORT = %d\n", m_WebServerPort);
    RunString(L, buf);

    lua_getglobal(L, "functions");
    ASSERT_EQ(LUA_TTABLE, lua_type(L, -1));
    lua_getfield(L, -1, "test_http");
    ASSERT_EQ(LUA_TFUNCTION, lua_type(L, -1));
    int result = dmScript::PCall(L, 0, LUA_MULTRET);
    if (result == LUA_ERRRUN)
    {
        ASSERT_TRUE(false);
    }
    else
    {
        ASSERT_EQ(0, result);
    }
    lua_pop(L, 1);

    uint64_t start = dmTime::GetTime();

    while (1) {
        dmSys::PumpMessageQueue();
        dmMessage::Dispatch(m_DefaultURL.m_Socket, DispatchCallbackDDF, this);

        lua_getglobal(L, "requests_left");
        int requests_left = lua_tointeger(L, -1);
        lua_pop(L, 1);

        if (requests_left == 0) {
            break;
        }

        // One issue causing intermittent failures (SOCKET_ERROR+WOULDBLOCK), was the fact that the connection creation times
        // could peak above 250ms. However, this was when we had the config script timeout set at 0.3 seconds.
        // Increasing that timeout value should make it robust again. /MAWE
        if( m_NumberOfFails )
        {
            break;
        }

        dmTime::Sleep(10 * 1000);

        uint64_t now = dmTime::GetTime();
        uint64_t elapsed = now - start;

        if (elapsed / 1000000 > 4) {
            dmLogError("The test timed out\n");
            ASSERT_TRUE(0);
        }
    }

    ASSERT_EQ(top, lua_gettop(L));
}

struct SHttpRequestTimeoutGuard // Makes sure it gets reset after gtest returns
{
    SHttpRequestTimeoutGuard(uint64_t timeout)
    {
        dmScript::SetHttpRequestTimeout(timeout);
    }
    ~SHttpRequestTimeoutGuard()
    {
        dmScript::SetHttpRequestTimeout(0);
    }
};

TEST_F(ScriptHttpTest, TestTimeout)
{
    SHttpRequestTimeoutGuard timeoutguard(1000 * 1000);

    int top = lua_gettop(L);

    ASSERT_TRUE(RunFile(L, "test_http_timeout.luac"));

    char buf[1024];
    DM_SNPRINTF(buf, sizeof(buf), "PORT = %d\n", m_WebServerPort);
    RunString(L, buf);

    lua_getglobal(L, "functions");
    ASSERT_EQ(LUA_TTABLE, lua_type(L, -1));
    lua_getfield(L, -1, "test_http_timeout");
    ASSERT_EQ(LUA_TFUNCTION, lua_type(L, -1));
    int result = dmScript::PCall(L, 0, LUA_MULTRET);
    if (result == LUA_ERRRUN)
    {
        ASSERT_TRUE(false);
    }
    else
    {
        ASSERT_EQ(0, result);
    }
    lua_pop(L, 1);

    uint64_t start = dmTime::GetTime();
    while (1) {
        dmSys::PumpMessageQueue();
        dmMessage::Dispatch(m_DefaultURL.m_Socket, DispatchCallbackDDF, this);

        lua_getglobal(L, "requests_left");
        int requests_left = lua_tointeger(L, -1);
        lua_pop(L, 1);

        if (requests_left == 0) {
            break;
        }

        if( m_NumberOfFails )
        {
            break;
        }

        dmTime::Sleep(10 * 1000);

        uint64_t now = dmTime::GetTime();
        uint64_t elapsed = now - start;
        if (elapsed / 1000000 > 4) {
            dmLogError("The test timed out\n");
            ASSERT_TRUE(0);
        }
    }

    ASSERT_EQ(top, lua_gettop(L));
}

TEST_F(ScriptHttpTest, TestDeletedSocket)
{
    SHttpRequestTimeoutGuard timeoutguard(300 * 1000);

    int top = lua_gettop(L);

    ASSERT_TRUE(RunFile(L, "test_http.luac"));

    char buf[1024];
    DM_SNPRINTF(buf, sizeof(buf), "PORT = %d\n", m_WebServerPort);
    RunString(L, buf);

    lua_getglobal(L, "functions");
    ASSERT_EQ(LUA_TTABLE, lua_type(L, -1));
    lua_getfield(L, -1, "test_http");
    ASSERT_EQ(LUA_TFUNCTION, lua_type(L, -1));
    int result = dmScript::PCall(L, 0, LUA_MULTRET);
    if (result == LUA_ERRRUN)
    {
        ASSERT_TRUE(false);
    }
    else
    {
        ASSERT_EQ(0, result);
    }
    lua_pop(L, 1);
    dmMessage::DeleteSocket(m_DefaultURL.m_Socket);
    m_DefaultURL.m_Socket = 0;

    for (int i = 0; i < 100; ++i) {
        dmSys::PumpMessageQueue();
        dmTime::Sleep(10 * 1000);
    }

    ASSERT_EQ(top, lua_gettop(L));
}

int main(int argc, char **argv)
{
    dmSocket::Initialize();
    dmDDF::RegisterAllTypes();
    testing::InitGoogleTest(&argc, argv);
    int ret = RUN_ALL_TESTS();
    dmSocket::Finalize();
    return ret;
}
