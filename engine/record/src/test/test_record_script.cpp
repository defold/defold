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

#define JC_TEST_IMPLEMENTATION
#include <jc_test/jc_test.h>
#include <dlib/configfile.h>
#include <dlib/dstrings.h>
#include <dlib/mutex.h>
#include <dlib/time.h>
#include <script/script.h>
#include <record/record_private.h>
#include <record/record_script.h>
#include <string.h>

static dmMutex::HMutex g_Mutex;
static dmRecord::Status g_NativeStatus;
static bool g_Unsupported;
static bool g_FailStart;
static bool g_FailStop;
static bool g_Resized;

// Real Lua and recorder workers, with a deterministic backend that needs no display.
namespace dmRecord
{
    bool IsSupported() { return !g_Unsupported; }
    bool IsAudioSupported() { return false; }
    const char* GetUnsupportedReason() { return "unsupported test backend"; }
    bool GetWindow(void*, Window* window)
    {
        memset(window, 0, sizeof(*window));
        window->m_Width = g_Resized ? 640 : 320;
        window->m_Height = 240;
        window->m_Scale = 1;
        return true;
    }
    bool PrepareWindow(void* window, Window* target) { return GetWindow(window, target); }
    void RestoreWindow() {}
    void GetNativeStatus(Status* status)
    {
        dmMutex::ScopedLock lock(g_Mutex);
        *status = g_NativeStatus;
    }
    bool StartNative(const Window*, const char* path, uint32_t fps, bool audio, Status* status)
    {
        dmMutex::ScopedLock lock(g_Mutex);
        memset(&g_NativeStatus, 0, sizeof(g_NativeStatus));
        g_NativeStatus.m_Active = !g_FailStart;
        g_NativeStatus.m_Width = 320;
        g_NativeStatus.m_Height = 240;
        g_NativeStatus.m_Fps = fps;
        g_NativeStatus.m_Audio = audio;
        dmStrlCpy(g_NativeStatus.m_Path, path, sizeof(g_NativeStatus.m_Path));
        if (g_FailStart)
            dmStrlCpy(g_NativeStatus.m_Failure, "permission denied", sizeof(g_NativeStatus.m_Failure));
        *status = g_NativeStatus;
        return status->m_Active;
    }
    bool StopNative(Status* status)
    {
        dmMutex::ScopedLock lock(g_Mutex);
        g_NativeStatus.m_Active = false;
        g_NativeStatus.m_Finalized = !g_FailStop && !g_NativeStatus.m_Failure[0];
        if (g_FailStop)
            dmStrlCpy(g_NativeStatus.m_Failure, "disk full", sizeof(g_NativeStatus.m_Failure));
        *status = g_NativeStatus;
        return status->m_Finalized;
    }
    void FinalizeNative()
    {
        Status status;
        StopNative(&status);
    }
}

struct ScriptInstance
{
    int m_Context;
    uint32_t m_Id;
    bool m_Valid;
};

static int InstanceContext(lua_State* L)
{
    lua_pushinteger(L, ((ScriptInstance*)lua_touserdata(L, 1))->m_Context);
    return 1;
}

static int InstanceId(lua_State* L)
{
    lua_pushinteger(L, ((ScriptInstance*)lua_touserdata(L, 1))->m_Id);
    return 1;
}

static int InstanceValid(lua_State* L)
{
    lua_pushboolean(L, ((ScriptInstance*)lua_touserdata(L, 1))->m_Valid);
    return 1;
}

class RecordScriptTest : public jc_test_base_class
{
protected:
    dmScript::HContext m_Context;
    dmConfigFile::HConfig m_Config;
    lua_State* m_L;
    ScriptInstance* m_Instance;
    int m_InstanceRef;
    int m_Refs;

    void SetUp()
    {
        g_Mutex = dmMutex::New();
        memset(&g_NativeStatus, 0, sizeof(g_NativeStatus));
        g_Unsupported = g_FailStart = g_FailStop = g_Resized = false;
        ASSERT_EQ(dmConfigFile::RESULT_OK, dmConfigFile::LoadFromBuffer("", 0, 0, 0, &m_Config));
        dmScript::ContextParams params = {};
        params.m_ConfigFile = m_Config;
        m_Context = dmScript::NewContext(params);
        dmScript::Initialize(m_Context);
        m_L = dmScript::GetLuaState(m_Context);
        const luaL_Reg methods[] = {{0, 0}};
        const luaL_Reg meta[] = {
            {dmScript::META_GET_INSTANCE_CONTEXT_TABLE_REF, InstanceContext},
            {dmScript::META_GET_UNIQUE_SCRIPT_ID, InstanceId},
            {dmScript::META_TABLE_IS_VALID, InstanceValid},
            {0, 0}
        };
        dmScript::RegisterUserType(m_L, "RecordingScriptInstance", methods, meta);
        m_Instance = (ScriptInstance*)lua_newuserdata(m_L, sizeof(ScriptInstance));
        m_Instance->m_Id = dmScript::GenerateUniqueScriptId();
        m_Instance->m_Valid = true;
        luaL_getmetatable(m_L, "RecordingScriptInstance");
        lua_setmetatable(m_L, -2);
        m_InstanceRef = dmScript::Ref(m_L, LUA_REGISTRYINDEX);
        lua_newtable(m_L);
        m_Instance->m_Context = dmScript::Ref(m_L, LUA_REGISTRYINDEX);
        lua_rawgeti(m_L, LUA_REGISTRYINDEX, m_InstanceRef);
        lua_pushvalue(m_L, -1);
        lua_setglobal(m_L, "expected_self");
        dmScript::SetInstance(m_L);
        dmRecord::InitializeScript(m_L, (void*)1);
        m_Refs = dmScript::GetLuaRefCount();
        ASSERT_TRUE(Run(
            "events = {}; results = {}; "
            "function callback(self, event, result) "
            "assert(self == expected_self); "
            "events[#events + 1] = event; results[#results + 1] = result; end"));
    }

    void TearDown()
    {
        dmRecord::FinalizeScript();
        ASSERT_EQ(m_Refs, dmScript::GetLuaRefCount());
        ASSERT_EQ(0, lua_gettop(m_L));
        lua_pushnil(m_L);
        dmScript::SetInstance(m_L);
        dmScript::Unref(m_L, LUA_REGISTRYINDEX, m_Instance->m_Context);
        dmScript::Unref(m_L, LUA_REGISTRYINDEX, m_InstanceRef);
        dmScript::Finalize(m_Context);
        dmScript::DeleteContext(m_Context);
        dmConfigFile::Delete(m_Config);
        dmMutex::Delete(g_Mutex);
    }

    bool Run(const char* source)
    {
        if (luaL_dostring(m_L, source))
        {
            printf("%s\n", lua_tostring(m_L, -1));
            lua_pop(m_L, 1);
            return false;
        }
        return true;
    }

    void Complete()
    {
        dmRecord::Status status;
        uint64_t deadline = dmTime::GetMonotonicTime() + 5000000;
        do
        {
            dmRecord::GetStatus(&status);
        } while (status.m_Pending && dmTime::GetMonotonicTime() < deadline);
        ASSERT_FALSE(status.m_Pending);
    }
};

// The two-function API delivers ordered events on update, with the original script self and options.
TEST_F(RecordScriptTest, StartStop)
{
    ASSERT_TRUE(Run("assert(recorder.get_status == nil and recorder.get_capabilities == nil); "
                   "assert(recorder.start({path='capture.mp4', fps=24}, callback)); assert(#events == 0)"));
    Complete();
    ASSERT_TRUE(Run("assert(#events == 0)"));
    dmRecord::UpdateScript();
    ASSERT_TRUE(Run("assert(#events == 1 and events[1] == recorder.EVENT_STARTED); "
                   "assert(results[1].path == 'capture.mp4' and results[1].fps == 24); "
                   "assert(results[1].width == 320 and results[1].height == 240 and not results[1].audio); "
                   "assert(recorder.stop()); assert(#events == 1)"));
    Complete();
    dmRecord::UpdateScript();
    dmRecord::UpdateScript();
    ASSERT_TRUE(Run("assert(#events == 2 and events[2] == recorder.EVENT_STOPPED)"));
}

// Busy starts and idle stops fail without replacing or leaking the original callback.
TEST_F(RecordScriptTest, ConflictingRequests)
{
    ASSERT_TRUE(Run("local ok, err = recorder.stop(); assert(ok == nil and type(err) == 'string'); "
                   "assert(recorder.start({path='first.mp4'}, callback)); "
                   "ok, err = recorder.start({path='second.mp4'}, callback); assert(ok == nil and err)"));
    Complete();
    ASSERT_TRUE(Run("assert(recorder.stop())"));
    Complete();
    ASSERT_TRUE(Run("local ok, err = recorder.start({path='second.mp4'}, callback); assert(ok == nil and err)"));
    dmRecord::UpdateScript();
    ASSERT_TRUE(Run("assert(#events == 2 and results[2].path == 'first.mp4')"));
}

// Unsupported platforms/audio reject synchronously and never schedule a callback.
TEST_F(RecordScriptTest, Unsupported)
{
    g_Unsupported = true;
    ASSERT_TRUE(Run("local ok, err = recorder.start({path='capture.mp4'}, callback); "
                   "assert(ok == nil and err == 'unsupported test backend')"));
    g_Unsupported = false;
    ASSERT_TRUE(Run("local ok, err = recorder.start({path='capture.mp4', audio=true}, callback); assert(ok == nil and err)"));
    dmRecord::UpdateScript();
    ASSERT_TRUE(Run("assert(#events == 0)"));
}

// Invalid options raise Lua argument errors before native recording or callback allocation.
TEST_F(RecordScriptTest, InvalidOptions)
{
    ASSERT_TRUE(Run(
        "local invalid = { {}, {path=''}, {path='a' .. string.char(0) .. 'b'}, {path=42}, "
        "{path='x', fps=0}, {path='x', fps=61}, {path='x', fps=1.5}, {path='x', fps=0/0}, "
        "{path='x', fps='30'}, {path='x', audio=1}, {path='x', width=320}, {path='x', [1]=1} }; "
        "for _, options in ipairs(invalid) do assert(not pcall(recorder.start, options, callback)) end; "
        "assert(not pcall(recorder.start, {path='x'}, nil)); assert(#events == 0)"));
}

// Values produced by an options metatable remain alive across validation and callback allocation.
TEST_F(RecordScriptTest, GeneratedOptions)
{
    ASSERT_TRUE(Run("local options = setmetatable({}, {__index=function(_, key) "
                   "if key == 'path' then return string.rep('a', 100) .. '.mp4' end "
                   "collectgarbage('collect'); return nil end}); "
                   "assert(recorder.start(options, callback))"));
    Complete();
    dmRecord::UpdateScript();
    ASSERT_TRUE(Run("assert(results[1].path == string.rep('a', 100) .. '.mp4')"));
}

// A permission/startup failure produces one terminal error and allows a subsequent recording.
TEST_F(RecordScriptTest, StartFailure)
{
    g_FailStart = true;
    ASSERT_TRUE(Run("assert(recorder.start({path='capture.mp4'}, callback))"));
    Complete();
    dmRecord::UpdateScript();
    ASSERT_TRUE(Run("assert(#events == 1 and events[1] == recorder.EVENT_ERROR); "
                   "assert(results[1].error == 'permission denied' and results[1].path == 'capture.mp4')"));
    g_FailStart = false;
    ASSERT_TRUE(Run("assert(recorder.start({path='retry.mp4'}, callback))"));
    Complete();
    dmRecord::UpdateScript();
    ASSERT_TRUE(Run("assert(events[2] == recorder.EVENT_STARTED)"));
}

// Failure while finalizing an accepted stop must report ERROR instead of a usable MP4.
TEST_F(RecordScriptTest, StopFailure)
{
    ASSERT_TRUE(Run("assert(recorder.start({path='capture.mp4'}, callback))"));
    Complete();
    dmRecord::UpdateScript();
    g_FailStop = true;
    ASSERT_TRUE(Run("assert(recorder.stop())"));
    Complete();
    dmRecord::UpdateScript();
    ASSERT_TRUE(Run("assert(#events == 2 and events[2] == recorder.EVENT_ERROR and results[2].error == 'disk full')"));
}

// Completion is retained even when an HTTP/C++ caller starts another recording before Lua updates.
TEST_F(RecordScriptTest, ExternalReplacement)
{
    ASSERT_TRUE(Run("assert(recorder.start({path='lua.mp4'}, callback))"));
    Complete();
    dmRecord::Status status;
    ASSERT_TRUE(dmRecord::Stop(&status));
    Complete();
    dmRecord::StartParams params;
    params.m_Window = (void*)1;
    params.m_Path = "http.mp4";
    ASSERT_TRUE(dmRecord::Start(&params, &status));
    Complete();
    dmRecord::UpdateScript();
    ASSERT_TRUE(Run("assert(#events == 2 and events[2] == recorder.EVENT_STOPPED); "
                   "assert(results[2].path == 'lua.mp4'); assert(recorder.stop())"));
}

// A terminal callback may start another recording without the old callback cleanup destroying it.
TEST_F(RecordScriptTest, RestartFromCallback)
{
    ASSERT_TRUE(Run("function restart(self, event, result) "
                   "callback(self, event, result); "
                   "if event == recorder.EVENT_STOPPED then assert(recorder.start({path='next.mp4'}, callback)) end end; "
                   "assert(recorder.start({path='first.mp4'}, restart))"));
    Complete();
    ASSERT_TRUE(Run("assert(recorder.stop())"));
    Complete();
    dmRecord::UpdateScript();
    Complete();
    dmRecord::UpdateScript();
    ASSERT_TRUE(Run("assert(#events == 3 and events[3] == recorder.EVENT_STARTED and results[3].path == 'next.mp4')"));
}

// STARTED callbacks may stop immediately without re-entering Lua from the native observer.
TEST_F(RecordScriptTest, StopFromStartedCallback)
{
    ASSERT_TRUE(Run("function stop_started(self, event, result) "
                   "callback(self, event, result); "
                   "if event == recorder.EVENT_STARTED then assert(recorder.stop()) end end; "
                   "assert(recorder.start({path='capture.mp4'}, stop_started))"));
    Complete();
    dmRecord::UpdateScript();
    Complete();
    dmRecord::UpdateScript();
    ASSERT_TRUE(Run("assert(#events == 2 and events[1] == recorder.EVENT_STARTED and events[2] == recorder.EVENT_STOPPED)"));
}

// A capture error alone must deliver one terminal callback and permit another recording.
TEST_F(RecordScriptTest, CaptureFailure)
{
    ASSERT_TRUE(Run("assert(recorder.start({path='capture.mp4'}, callback))"));
    Complete();
    dmRecord::UpdateScript();
    {
        dmMutex::ScopedLock lock(g_Mutex);
        dmStrlCpy(g_NativeStatus.m_Failure, "capture failed", sizeof(g_NativeStatus.m_Failure));
    }
    Complete();
    dmRecord::UpdateScript();
    dmRecord::UpdateScript();
    ASSERT_TRUE(Run("assert(#events == 2 and events[2] == recorder.EVENT_ERROR); "
                   "assert(results[2].error == 'capture failed'); "
                   "assert(recorder.start({path='retry.mp4'}, callback))"));
    Complete();
    dmRecord::UpdateScript();
    ASSERT_TRUE(Run("assert(#events == 3 and events[3] == recorder.EVENT_STARTED)"));
}

// Resizing finalizes recording through ordinary updates without requiring a Lua stop call.
TEST_F(RecordScriptTest, Resize)
{
    ASSERT_TRUE(Run("assert(recorder.start({path='capture.mp4'}, callback))"));
    Complete();
    dmRecord::UpdateScript();
    g_Resized = true;
    Complete();
    dmRecord::UpdateScript();
    ASSERT_TRUE(Run("assert(#events == 2 and events[2] == recorder.EVENT_STOPPED)"));
}

// Deleting the script suppresses its callback while allowing the global recording to finish.
TEST_F(RecordScriptTest, DeletedScript)
{
    ASSERT_TRUE(Run("assert(recorder.start({path='capture.mp4'}, callback))"));
    Complete();
    m_Instance->m_Valid = false;
    m_Instance->m_Id = dmScript::GenerateUniqueScriptId();
    dmRecord::UpdateScript();
    dmRecord::Status status;
    dmRecord::GetStatus(&status);
    ASSERT_TRUE(status.m_Active);
    ASSERT_EQ(m_Refs, dmScript::GetLuaRefCount());
    ASSERT_TRUE(Run("assert(#events == 0); assert(recorder.stop())"));
    Complete();
    dmRecord::UpdateScript();
    ASSERT_TRUE(Run("assert(#events == 0)"));
}

// Shutdown joins pending work, releases callbacks, and rejects starts from later script finalizers.
TEST_F(RecordScriptTest, Shutdown)
{
    ASSERT_TRUE(Run("assert(recorder.start({path='capture.mp4'}, callback))"));
    dmRecord::FinalizeScript();
    ASSERT_FALSE(g_NativeStatus.m_Active);
    ASSERT_TRUE(Run("assert(#events == 0); "
                   "local ok, err = recorder.start({path='late.mp4'}, callback); assert(ok == nil and err); "
                   "ok, err = recorder.stop(); assert(ok == nil and err)"));
}

int main(int argc, char** argv)
{
    jc_test_init(&argc, argv);
    return jc_test_run_all();
}
