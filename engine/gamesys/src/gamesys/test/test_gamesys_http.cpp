// Copyright 2020-2025 The Defold Foundation
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

#include <script/test_script.h>
#include <dlib/dstrings.h>
#include <dlib/time.h>

#include "test_gamesys.h"

using namespace dmVMath;

static int g_HttpPort = 9001;
static char g_HttpAddress[128] = "localhost";

#define DEFAULT_URL "__default_url"

static void SetHttpAddress(lua_State* L)
{
    char buf[128];
    dmSnPrintf(buf, sizeof(buf), "IP='%s'\n", g_HttpAddress);
    dmScriptTest::RunString(L, buf);
    dmSnPrintf(buf, sizeof(buf), "PORT=%d\n", g_HttpPort);
    dmScriptTest::RunString(L, buf);
    dmSnPrintf(buf, sizeof(buf), "ADDRESS='http://%s:%d'\n", g_HttpAddress, g_HttpPort);
    dmScriptTest::RunString(L, buf);
}

TEST_F(ComponentTest, HTTPRequest)
{
    lua_State* L = m_Scriptlibcontext.m_LuaState;

    dmMessage::URL m_DefaultURL;
    ASSERT_EQ(dmMessage::RESULT_OK, dmMessage::NewSocket("default_socket", &m_DefaultURL.m_Socket));
    m_DefaultURL.m_Path = dmHashString64("default_path");
    m_DefaultURL.m_Fragment = dmHashString64("default_fragment");
    dmScript::PushURL(m_Scriptlibcontext.m_LuaState, m_DefaultURL);
    lua_setglobal(L, DEFAULT_URL);

    SetHttpAddress(L);

    ASSERT_TRUE(dmGameObject::Init(m_Collection));

    dmGameObject::HInstance go = ::Spawn(m_Factory, m_Collection, "/http/http_request.goc", dmHashString64("/http_request"), 0, Point3(0, 0, 0), Quat(0, 0, 0, 1), Vector3(1, 1, 1));
    ASSERT_NE((void*)0, go);

    uint64_t stop_time = dmTime::GetMonotonicTime() + 1*1e6; // 1 second
    bool tests_done = false;
    while (dmTime::GetMonotonicTime() < stop_time && !tests_done)
    {
    	ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));

        // check if tests are done
        lua_getglobal(L, "tests_done");
        tests_done = lua_toboolean(L, -1);
        lua_pop(L, 1);

        dmTime::Sleep(30*1000);
    }

    ASSERT_TRUE(tests_done);
    ASSERT_TRUE(dmGameObject::Final(m_Collection));
    dmMessage::DeleteSocket(m_DefaultURL.m_Socket);
}

extern "C" void dmExportedSymbols();

int main(int argc, char **argv)
{
    dmExportedSymbols();
    TestMainPlatformInit();

    dmLog::LogParams params;
    dmLog::LogInitialize(&params);
    dmHashEnableReverseHash(true);
    // // Enable message descriptor translation when sending messages
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
        return 1;
    }

    jc_test_init(&argc, argv);
    int result = jc_test_run_all();
    dmLog::LogFinalize();
    return result;
}
