// Copyright 2020-2026 The Defold Foundation
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

#include "script.h"
#include "script/sys_ddf.h"
#include "test_script.h"

#include <testmain/testmain.h>
#include <ddf/ddf.h>
#include <dlib/log.h>
#include <dlib/message.h>
#include <dlib/socket.h>

class ScriptSysTest : public dmScriptTest::ScriptTest
{
};

TEST_F(ScriptSysTest, TestSys)
{
    int top = lua_gettop(L);

    ASSERT_TRUE(RunFile(L, "test_sys.luac"));

    lua_getglobal(L, "functions");
    ASSERT_EQ(LUA_TTABLE, lua_type(L, -1));
    lua_getfield(L, -1, "test_sys");
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

    ASSERT_EQ(top, lua_gettop(L));
}

static void CheckRebootArguments(dmMessage::Message* message, void* user_data)
{
    ASSERT_EQ((uintptr_t)dmSystemDDF::Reboot::m_DDFDescriptor, message->m_Descriptor);
    dmDDF::ResolvePointers(dmSystemDDF::Reboot::m_DDFDescriptor, message->m_Data);
    dmSystemDDF::Reboot* reboot = (dmSystemDDF::Reboot*)message->m_Data;
    ASSERT_STREQ("one", reboot->m_Arg1);
    ASSERT_STREQ("two", reboot->m_Arg2);
    ASSERT_STREQ("three", reboot->m_Arg3);
    ASSERT_STREQ("four", reboot->m_Arg4);
    ASSERT_STREQ("five", reboot->m_Arg5);
    ASSERT_STREQ("six", reboot->m_Arg6);
    ASSERT_STREQ("seven", reboot->m_Arg7);
    ASSERT_STREQ("game.projectc", reboot->m_Arg8);
    *(bool*)user_data = true;
}

TEST_F(ScriptSysTest, RebootArguments)
{
    dmMessage::HSocket socket;
    ASSERT_EQ(dmMessage::RESULT_OK, dmMessage::NewSocket("@system", &socket));
    ASSERT_TRUE(RunString(L, "sys.reboot('one', 'two', 'three', 'four', 'five', 'six', 'seven', 'game.projectc')"));
    bool received = false;
    ASSERT_EQ(1u, dmMessage::Dispatch(socket, CheckRebootArguments, &received));
    ASSERT_TRUE(received);
    ASSERT_EQ(dmMessage::RESULT_OK, dmMessage::DeleteSocket(socket));
}

extern "C" void dmExportedSymbols();

int main(int argc, char **argv)
{
    dmExportedSymbols();
    TestMainPlatformInit();
    dmSocket::Initialize();
    jc_test_init(&argc, argv);
    int ret = jc_test_run_all();
    dmSocket::Finalize();
    return ret;
}
