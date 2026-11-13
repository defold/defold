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
#include <dlib/sys.h>
#include <dlib/thread.h>

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

// Verifies that all eight sys.reboot arguments reach the system message, guarding
// against losing debugger options or the project path after the sixth argument.
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

struct ConnectivityTestInstance
{
    int      m_ContextTableRef;
    uint32_t m_UniqueScriptId;
};

static int ConnectivityTestInstanceIsValid(lua_State* L)
{
    lua_pushboolean(L, 1);
    return 1;
}

static int ConnectivityTestInstanceGetContextTableRef(lua_State* L)
{
    lua_pushnumber(L, ((ConnectivityTestInstance*)lua_touserdata(L, 1))->m_ContextTableRef);
    return 1;
}

static int ConnectivityTestInstanceGetUniqueScriptId(lua_State* L)
{
    lua_pushinteger(L, ((ConnectivityTestInstance*)lua_touserdata(L, 1))->m_UniqueScriptId);
    return 1;
}

static const luaL_reg ConnectivityTestInstance_methods[] =
{
    { 0, 0 }
};

static const luaL_reg ConnectivityTestInstance_meta[] =
{
    { dmScript::META_TABLE_IS_VALID,                 ConnectivityTestInstanceIsValid },
    { dmScript::META_GET_INSTANCE_CONTEXT_TABLE_REF, ConnectivityTestInstanceGetContextTableRef },
    { dmScript::META_GET_UNIQUE_SCRIPT_ID,           ConnectivityTestInstanceGetUniqueScriptId },
    { 0, 0 }
};

#if defined(DM_HAS_THREADS)
static void PublishConnectivityThread(void* arg)
{
    dmSys::PublishNetworkConnectivity(*(dmSys::NetworkConnectivity*)arg);
}
#endif

static void PublishConnectivityFromThread(dmSys::NetworkConnectivity status)
{
#if defined(DM_HAS_THREADS)
    dmThread::Thread thread = dmThread::New(PublishConnectivityThread, 0x10000, &status, "connectivity");
    dmThread::Join(thread);
#else
    dmSys::PublishNetworkConnectivity(status);
#endif
}

class ScriptSysConnectivityTest : public ScriptSysTest
{
    public:
    void SetUp() override
    {
        ScriptSysTest::SetUp();

        dmSys::NetworkConnectivity status;
        dmSys::PublishNetworkConnectivity(dmSys::NETWORK_UNKNOWN);
        dmSys::ConsumeNetworkConnectivityChange(&status);

        const char* type_name = "ConnectivityTestInstance";
        dmScript::RegisterUserType(L, type_name, ConnectivityTestInstance_methods, ConnectivityTestInstance_meta);

        ConnectivityTestInstance* instance = (ConnectivityTestInstance*)lua_newuserdata(L, sizeof(ConnectivityTestInstance));
        luaL_getmetatable(L, type_name);
        lua_setmetatable(L, -2);
        lua_pushvalue(L, -1);
        m_InstanceRef = dmScript::Ref(L, LUA_REGISTRYINDEX);
        lua_newtable(L);
        m_ContextTableRef = dmScript::Ref(L, LUA_REGISTRYINDEX);
        instance->m_ContextTableRef = m_ContextTableRef;
        instance->m_UniqueScriptId = dmScript::GenerateUniqueScriptId();
        dmScript::SetInstance(L);

        ASSERT_TRUE(RunString(L,
                              "calls = 0\n"
                              "other_calls = 0\n"
                              "function on_connectivity(self, status)\n"
                              "    calls = calls + 1\n"
                              "    last_self = self\n"
                              "    last_status = status\n"
                              "end\n"
                              "function overwrite_freed_memory()\n"
                              "    collectgarbage()\n"
                              "    local strings = {}\n"
                              "    for length = 8, 96 do\n"
                              "        for char = 65, 90 do\n"
                              "            strings[#strings + 1] = string.rep(string.char(char), length)\n"
                              "        end\n"
                              "    end\n"
                              "end\n"
                              "function on_other_connectivity(self, status)\n"
                              "    other_calls = other_calls + 1\n"
                              "end\n"));
    }

    void TearDown() override
    {
        DeleteInstance();
        ScriptSysTest::TearDown();
    }

    void DeleteInstance()
    {
        if (m_InstanceRef == LUA_NOREF)
        {
            return;
        }
        lua_pushnil(L);
        dmScript::SetInstance(L);
        dmScript::Unref(L, LUA_REGISTRYINDEX, m_ContextTableRef);
        dmScript::Unref(L, LUA_REGISTRYINDEX, m_InstanceRef);
        m_InstanceRef = LUA_NOREF;
    }

    int GetGlobalInt(const char* name)
    {
        lua_getglobal(L, name);
        int value = (int)lua_tointeger(L, -1);
        lua_pop(L, 1);
        return value;
    }

    void Change(dmSys::NetworkConnectivity status)
    {
        PublishConnectivityFromThread(status);
        dmScript::Update(m_Context);
    }

    int m_InstanceRef;
    int m_ContextTableRef;
};

// Verifies that a listener is called once with the script instance and the status, on the first update after a
// change and not when it is set. Guards against a change being reported repeatedly, or from outside the engine thread.
TEST_F(ScriptSysConnectivityTest, ListenerCalledOnceAfterChange)
{
    int top = lua_gettop(L);
    ASSERT_TRUE(RunString(L, "sys.set_connectivity_listener(on_connectivity)"));
    dmScript::Update(m_Context);
    ASSERT_EQ(0, GetGlobalInt("calls"));

    PublishConnectivityFromThread(dmSys::NETWORK_CONNECTED);
    ASSERT_EQ(0, GetGlobalInt("calls"));

    dmScript::Update(m_Context);
    ASSERT_EQ(1, GetGlobalInt("calls"));
    ASSERT_EQ(dmSys::NETWORK_CONNECTED, GetGlobalInt("last_status"));
    ASSERT_TRUE(RunString(L, "assert(type(last_self) == 'userdata')"));

    dmScript::Update(m_Context);
    ASSERT_EQ(1, GetGlobalInt("calls"));
    ASSERT_EQ(top, lua_gettop(L));
}

// Verifies that the listener follows each transition and that the status matches the sys.NETWORK constants.
// Guards against the first known status being skipped, and against a mismatch between the Lua and C++ values.
TEST_F(ScriptSysConnectivityTest, ListenerFollowsTransitions)
{
    ASSERT_TRUE(RunString(L, "sys.set_connectivity_listener(on_connectivity)"));

    Change(dmSys::NETWORK_CONNECTED);
    ASSERT_TRUE(RunString(L, "assert(calls == 1 and last_status == sys.NETWORK_CONNECTED)"));
    Change(dmSys::NETWORK_CONNECTED_CELLULAR);
    ASSERT_TRUE(RunString(L, "assert(calls == 2 and last_status == sys.NETWORK_CONNECTED_CELLULAR)"));
    Change(dmSys::NETWORK_DISCONNECTED);
    ASSERT_TRUE(RunString(L, "assert(calls == 3 and last_status == sys.NETWORK_DISCONNECTED)"));
    Change(dmSys::NETWORK_UNKNOWN);
    ASSERT_TRUE(RunString(L, "assert(calls == 4 and last_status == sys.NETWORK_UNKNOWN)"));
}

// Verifies that a change published before the listener was set isn't reported to it.
// Guards against a listener receiving a stale status on the first update after being set.
TEST_F(ScriptSysConnectivityTest, ListenerIgnoresEarlierChange)
{
    PublishConnectivityFromThread(dmSys::NETWORK_CONNECTED);
    ASSERT_TRUE(RunString(L, "sys.set_connectivity_listener(on_connectivity)"));
    dmScript::Update(m_Context);
    ASSERT_EQ(0, GetGlobalInt("calls"));

    Change(dmSys::NETWORK_DISCONNECTED);
    ASSERT_EQ(1, GetGlobalInt("calls"));
    ASSERT_EQ(dmSys::NETWORK_DISCONNECTED, GetGlobalInt("last_status"));
}

// Verifies that passing nil removes the listener, also when there is none.
// Guards against a removed listener still being called.
TEST_F(ScriptSysConnectivityTest, ListenerRemovedWithNil)
{
    ASSERT_TRUE(RunString(L, "sys.set_connectivity_listener(nil)"));
    ASSERT_TRUE(RunString(L, "sys.set_connectivity_listener(on_connectivity)"));
    ASSERT_TRUE(RunString(L, "sys.set_connectivity_listener(nil)"));
    Change(dmSys::NETWORK_CONNECTED);
    ASSERT_EQ(0, GetGlobalInt("calls"));
}

// Verifies that a new listener replaces the previous one, and that an invalid argument keeps the current one.
// Guards against several listeners being called for one change.
TEST_F(ScriptSysConnectivityTest, ListenerReplaced)
{
    ASSERT_TRUE(RunString(L, "sys.set_connectivity_listener(on_other_connectivity)"));
    ASSERT_TRUE(RunString(L, "sys.set_connectivity_listener(on_connectivity)"));
    ASSERT_TRUE(RunString(L, "assert(not pcall(sys.set_connectivity_listener, 42))"));
    ASSERT_TRUE(RunString(L, "assert(not pcall(sys.set_connectivity_listener))"));
    Change(dmSys::NETWORK_CONNECTED);
    ASSERT_EQ(1, GetGlobalInt("calls"));
    ASSERT_EQ(0, GetGlobalInt("other_calls"));
}

// Verifies that a listener can remove itself while it is being called.
// Guards against the callback being destroyed before the call has been cleaned up.
TEST_F(ScriptSysConnectivityTest, ListenerRemovedFromListener)
{
    int top = lua_gettop(L);
    ASSERT_TRUE(RunString(L,
                          "sys.set_connectivity_listener(function(self, status)\n"
                          "    calls = calls + 1\n"
                          "    sys.set_connectivity_listener(nil)\n"
                          "    overwrite_freed_memory()\n"
                          "end)\n"));
    Change(dmSys::NETWORK_CONNECTED);
    ASSERT_EQ(1, GetGlobalInt("calls"));

    Change(dmSys::NETWORK_DISCONNECTED);
    ASSERT_EQ(1, GetGlobalInt("calls"));
    ASSERT_EQ(top, lua_gettop(L));
}

// Verifies that a listener can replace itself, more than once, while it is being called.
// Guards against destroying the running callback, or leaving the wrong listener in place.
TEST_F(ScriptSysConnectivityTest, ListenerReplacedFromListener)
{
    int top = lua_gettop(L);
    ASSERT_TRUE(RunString(L,
                          "replaced_calls = 0\n"
                          "sys.set_connectivity_listener(function(self, status)\n"
                          "    replaced_calls = replaced_calls + 1\n"
                          "    sys.set_connectivity_listener(on_other_connectivity)\n"
                          "    sys.set_connectivity_listener(on_connectivity)\n"
                          "    overwrite_freed_memory()\n"
                          "end)\n"));
    Change(dmSys::NETWORK_CONNECTED);
    ASSERT_EQ(1, GetGlobalInt("replaced_calls"));
    ASSERT_EQ(0, GetGlobalInt("calls"));

    Change(dmSys::NETWORK_DISCONNECTED);
    ASSERT_EQ(1, GetGlobalInt("replaced_calls"));
    ASSERT_EQ(0, GetGlobalInt("other_calls"));
    ASSERT_EQ(1, GetGlobalInt("calls"));
    ASSERT_EQ(top, lua_gettop(L));
}

// Verifies that a Lua error in the listener leaves the stack balanced and the listener in place.
// Guards against an error stopping later changes from being reported.
TEST_F(ScriptSysConnectivityTest, ListenerErrorKeepsListener)
{
    int top = lua_gettop(L);
    ASSERT_TRUE(RunString(L,
                          "sys.set_connectivity_listener(function(self, status)\n"
                          "    calls = calls + 1\n"
                          "    error('expected test error')\n"
                          "end)\n"));
    Change(dmSys::NETWORK_CONNECTED);
    Change(dmSys::NETWORK_DISCONNECTED);
    ASSERT_EQ(2, GetGlobalInt("calls"));
    ASSERT_EQ(top, lua_gettop(L));
}

// Verifies that the listener of a deleted script instance is removed without a call or a log message.
// Guards against calling into a deleted script, as the listener isn't removed when its script is deleted.
TEST_F(ScriptSysConnectivityTest, ListenerRemovedWithDeletedInstance)
{
    ASSERT_TRUE(RunString(L, "sys.set_connectivity_listener(on_connectivity)"));
    DeleteInstance();

    Change(dmSys::NETWORK_CONNECTED);
    Change(dmSys::NETWORK_DISCONNECTED);
    ASSERT_EQ(0, GetGlobalInt("calls"));
    ASSERT_EQ((char*)0, strstr(GetLog(), "callback"));
}

// Verifies that only the context that owns the listener reports changes, and that finalizing another context keeps it.
// Guards against a change being consumed by the wrong context, and against the listener being destroyed with it.
TEST_F(ScriptSysConnectivityTest, ListenerBelongsToItsContext)
{
    dmScript::ContextParams context_params = {};
    context_params.m_ConfigFile = m_ConfigFile;
    context_params.m_Factory = m_ResourceFactory;
    dmScript::HContext other_context = dmScript::NewContext(context_params);
    dmScript::Initialize(other_context);

    ASSERT_TRUE(RunString(L, "sys.set_connectivity_listener(on_connectivity)"));

    PublishConnectivityFromThread(dmSys::NETWORK_CONNECTED);
    dmScript::Update(other_context);
    ASSERT_EQ(0, GetGlobalInt("calls"));
    dmScript::Update(m_Context);
    ASSERT_EQ(1, GetGlobalInt("calls"));

    dmScript::Finalize(other_context);
    dmScript::DeleteContext(other_context);

    Change(dmSys::NETWORK_DISCONNECTED);
    ASSERT_EQ(2, GetGlobalInt("calls"));
}

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
