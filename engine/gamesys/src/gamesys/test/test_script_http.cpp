// Copyright 2020-2024 The Defold Foundation
// Copyright 2014-2020 King
// Copyright 2009-2014 Ragnar Svensson, Christian Murray
// Licensed under the Defold License version 1.0 (the "License"); you may not use
// this file except in compliance with the License.
//
// You may obtain a copy of the License, together with FAQs at
// https://www.defold.com/license
//
// Unless required by applicable law or agreed to in writing, software distributed
// under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
// CONDITIONS OF ANY KIND, either express or implied. See the License for the
// specific language governing permissions and limitations under the License.

#include "../scripts/script_http.h" // to set the timeout

#include <script/test_script.h>
#include <script/script.h>
#include <testmain/testmain.h>
#include <dlib/configfile.h>
#include <dlib/dstrings.h>
#include <dlib/hash.h>
#include <dlib/log.h>
#include <dlib/time.h>
#include <dlib/socket.h>
#include <dlib/thread.h>
#include <dlib/testutil.h>
#include <dlib/sys.h>
#include <dlib/testutil.h>

static int g_HttpPort = 9001;
char g_HttpAddress[128] = "localhost";

#define DEFAULT_URL "__default_url"

struct ScriptInstance
{
    int m_InstanceReference;
    int m_ContextTableReference;
};

static int ResolvePathCallback(lua_State* L)
{
    DM_LUA_STACK_CHECK(L, 1);

    const int self_index = 1;

    ScriptInstance* i = (ScriptInstance*)lua_touserdata(L, self_index);
    assert(i);
    assert(i->m_ContextTableReference != LUA_NOREF);
    const char* path = luaL_checkstring(L, 2);
    dmScript::PushHash(L, dmHashString64(path));
    return 1;
}

static int GetURLCallback(lua_State* L)
{
    DM_LUA_STACK_CHECK(L, 1);

    const int self_index = 1;

    ScriptInstance* i = (ScriptInstance*)lua_touserdata(L, self_index);
    assert(i);
    assert(i->m_ContextTableReference != LUA_NOREF);
    lua_getglobal(L, DEFAULT_URL);
    return 1;
}

static int GetInstaceContextTableRef(lua_State* L)
{
    DM_LUA_STACK_CHECK(L, 1);

    const int self_index = 1;

    ScriptInstance* i = (ScriptInstance*)lua_touserdata(L, self_index);
    lua_pushnumber(L, i ? i->m_ContextTableReference : LUA_NOREF);

    return 1;
}

static const luaL_reg META_TABLE[] =
{
    {dmScript::META_TABLE_RESOLVE_PATH,             ResolvePathCallback},
    {dmScript::META_TABLE_GET_URL,                  GetURLCallback},
    {dmScript::META_GET_INSTANCE_CONTEXT_TABLE_REF, GetInstaceContextTableRef},
    {0, 0}
};

class ScriptHttpTest : public jc_test_base_class
{
public:
    int m_HttpResponseCount;
    dmScript::HContext m_ScriptContext;
    lua_State* L;
    dmMessage::URL m_DefaultURL;
    dmConfigFile::HConfig m_ConfigFile;
    int m_NumberOfFails;

protected:

    virtual void SetUp()
    {
        char path[1024];
        dmTestUtil::MakeHostPath(path, sizeof(path), "src/gamesys/test/http/test_http.config.raw");

        dmConfigFile::Result r = dmConfigFile::Load(path, 0, 0, &m_ConfigFile);
        ASSERT_EQ(dmConfigFile::RESULT_OK, r);

        m_HttpResponseCount = 0;

        m_ScriptContext = dmScript::NewContext(m_ConfigFile, 0, true);
        dmScript::Initialize(m_ScriptContext);
        L = dmScript::GetLuaState(m_ScriptContext);

        dmGameSystem::ScriptLibContext scriptlibcontext;
        scriptlibcontext.m_LuaState        = L;
        scriptlibcontext.m_ScriptContext   = m_ScriptContext;

        dmGameSystem::InitializeScriptLibs(scriptlibcontext);

        ASSERT_EQ(dmMessage::RESULT_OK, dmMessage::NewSocket("default_socket", &m_DefaultURL.m_Socket));
        m_DefaultURL.m_Path = dmHashString64("default_path");
        m_DefaultURL.m_Fragment = dmHashString64("default_fragment");
        dmScript::PushURL(L, m_DefaultURL);

        lua_setglobal(L, DEFAULT_URL);

        int top = lua_gettop(L);
        (void)top;
        ScriptInstance* script_instance = (ScriptInstance*)lua_newuserdata(L, sizeof(ScriptInstance));
        lua_pushvalue(L, -1);
        script_instance->m_InstanceReference = dmScript::Ref(L, LUA_REGISTRYINDEX);
        lua_newtable(L);
        script_instance->m_ContextTableReference = dmScript::Ref(L, LUA_REGISTRYINDEX);
        luaL_newmetatable(L, "ScriptMsgTest");
        luaL_register(L, 0, META_TABLE);
        lua_setmetatable(L, -2);
        dmScript::SetInstance(L);
        assert(top == lua_gettop(L));
        m_NumberOfFails = 0;
    }

    virtual void TearDown()
    {
        dmScript::GetInstance(L);
        ScriptInstance* script_instance = (ScriptInstance*)lua_touserdata(L, -1);
        dmScript::Unref(L, LUA_REGISTRYINDEX, script_instance->m_ContextTableReference);
        dmScript::Unref(L, LUA_REGISTRYINDEX, script_instance->m_InstanceReference);
        lua_pop(L, 1);

        lua_pushnil(L);
        dmScript::SetInstance(L);

        if (m_DefaultURL.m_Socket) {
            dmMessage::DeleteSocket(m_DefaultURL.m_Socket);
        }
        dmScript::Finalize(m_ScriptContext);
        dmScript::DeleteContext(m_ScriptContext);
        dmConfigFile::Delete(m_ConfigFile);
    }

    void SetHttpAddress(lua_State* L)
    {
        char buf[128];
        dmSnPrintf(buf, sizeof(buf), "IP='%s'\n", g_HttpAddress);
        dmScriptTest::RunString(L, buf);
        dmSnPrintf(buf, sizeof(buf), "PORT=%d\n", g_HttpPort);
        dmScriptTest::RunString(L, buf);
        dmSnPrintf(buf, sizeof(buf), "ADDRESS='http://%s:%d'\n", g_HttpAddress, g_HttpPort);
        dmScriptTest::RunString(L, buf);
    }
};

static void DispatchCallbackDDF(dmMessage::Message *message, void* user_ptr)
{
    ScriptHttpTest* test = (ScriptHttpTest*) user_ptr;
    test->m_HttpResponseCount++;
    lua_State* L = test->L;
    assert(message->m_Descriptor != 0);
    dmDDF::Descriptor* descriptor = (dmDDF::Descriptor*)message->m_Descriptor;

    int ref = message->m_UserData2 + LUA_NOREF;
    dmScript::ResolveInInstance(L, ref);
    dmScript::UnrefInInstance(L, ref);
    lua_gc(L, LUA_GCCOLLECT, 0);

    dmScript::PushDDF(L, descriptor, (const char*)&message->m_Data[0]);
    int ret = dmScript::PCall(L, 1, 0);
    test->m_NumberOfFails += ret == 0 ? 0 : 1;

    ASSERT_EQ(0, ret);
}

TEST_F(ScriptHttpTest, TestGetPost)
{
    int top = lua_gettop(L);

    ASSERT_TRUE(dmScriptTest::RunFile(L, "test_http.lua.rawc", "build/src/gamesys/test/http"));
    SetHttpAddress(L);

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

        if (elapsed / 1000000 > 8) {
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
        dmGameSystem::SetHttpRequestTimeout(timeout);
    }
    ~SHttpRequestTimeoutGuard()
    {
        dmGameSystem::SetHttpRequestTimeout(0);
    }
};

TEST_F(ScriptHttpTest, TestTimeout)
{
    SHttpRequestTimeoutGuard timeoutguard(1000 * 1000);

    int top = lua_gettop(L);

    ASSERT_TRUE(dmScriptTest::RunFile(L, "test_http_timeout.lua.rawc", "build/src/gamesys/test/http"));
    SetHttpAddress(L);

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

    ASSERT_TRUE(dmScriptTest::RunFile(L, "test_http.lua.rawc", "build/src/gamesys/test/http"));
    SetHttpAddress(L);

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

static void Destroy()
{
    dmSocket::Finalize();
    dmLog::LogFinalize();
}

int main(int argc, char **argv)
{
    TestMainPlatformInit();
    dmLog::LogParams params;
    dmLog::LogInitialize(&params);
    dmSocket::Initialize();
    dmDDF::RegisterAllTypes();

    if(argc > 1)
    {
        char path[512];
        dmTestUtil::MakeHostPath(path, sizeof(path), argv[1]);

        dmConfigFile::HConfig config;
        if( dmConfigFile::Load(path, argc, (const char**)argv, &config) != dmConfigFile::RESULT_OK )
        {
            dmLogError("Could not read config file '%s'", argv[1]);
            return 1;
        }
        dmTestUtil::GetSocketsFromConfig(config, &g_HttpPort, 0, 0);
        if (!dmTestUtil::GetIpFromConfig(config, g_HttpAddress, sizeof(g_HttpAddress))) {
            dmLogError("Failed to get server ip!");
        } else {
            dmLogInfo("Server ip: %s:%d", g_HttpAddress, g_HttpPort);
        }

        dmConfigFile::Delete(config);
    }
    else
    {
        dmLogError("No config file specified!");
        Destroy();
        return 1;
    }

    jc_test_init(&argc, argv);
    int ret = jc_test_run_all();

    Destroy();
    return ret;
}
