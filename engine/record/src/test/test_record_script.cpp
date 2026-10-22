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
#include <extension/extension.hpp>
#include <script/script.h>
#include <record/record_private.h>
#include <string.h>
#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif
#include <record/record_window.h>
#if defined(DM_PLATFORM_LINUX)
#include <graphics/graphics.h>
extern "C" void GraphicsAdapterNull();
#endif

extern "C" void ScriptRecord();

static dmMutex::HMutex  g_Mutex;
static dmRecord::Status g_NativeStatus;
static void*            g_NativeWindow = (void*)1;
static bool             g_Unsupported;
static bool             g_FailStart;
static bool             g_FailStop;
static bool             g_Resized;
#if defined(DM_PLATFORM_LINUX)
static uint32_t g_RenderedFrames;
static uint32_t g_CapturedFrames;
#endif

namespace dmGraphics
{
#if defined(DM_PLATFORM_MACOS)
    id GetNativeOSXNSWindow()
    {
        return (id)g_NativeWindow;
    }
#elif defined(DM_PLATFORM_WINDOWS)
    HWND GetNativeWindowsHWND()
    {
        return (HWND)g_NativeWindow;
    }
#endif
}

// Real Lua and recorder workers, with a deterministic backend that needs no display.
namespace dmRecord
{
    bool IsSupported()
    {
        return !g_Unsupported;
    }
    bool IsAudioSupported()
    {
        return false;
    }
    const char* GetUnsupportedReason()
    {
        return "unsupported test backend";
    }
    bool GetWindow(void* native_window, Window* window)
    {
        EXPECT_EQ(g_NativeWindow, native_window);
        memset(window, 0, sizeof(*window));
        window->m_Width = g_Resized ? 640 : 320;
        window->m_Height = 240;
        window->m_Scale = 1;
        return true;
    }
    bool PrepareWindow(void* window, Window* target)
    {
        return GetWindow(window, target);
    }
    void RestoreWindow() {}
    void GetNativeStatus(Status* status)
    {
        DM_MUTEX_SCOPED_LOCK(g_Mutex);
        *status = g_NativeStatus;
    }
    bool StartNative(const Window*, const char* path, uint32_t fps, bool audio, Status* status)
    {
        DM_MUTEX_SCOPED_LOCK(g_Mutex);
        memset(&g_NativeStatus, 0, sizeof(g_NativeStatus));
        g_NativeStatus.m_Active = !g_FailStart;
        g_NativeStatus.m_Width = 320;
        g_NativeStatus.m_Height = 240;
        g_NativeStatus.m_Fps = fps;
        g_NativeStatus.m_Audio = audio;
        dmStrlCpy(g_NativeStatus.m_Path, path, sizeof(g_NativeStatus.m_Path));
        if (g_FailStart)
        {
            dmStrlCpy(g_NativeStatus.m_Failure, "permission denied", sizeof(g_NativeStatus.m_Failure));
        }
        *status = g_NativeStatus;
        return status->m_Active;
    }
    bool StopNative(Status* status)
    {
        DM_MUTEX_SCOPED_LOCK(g_Mutex);
        g_NativeStatus.m_Active = false;
        g_NativeStatus.m_Finalized = !g_FailStop && !g_NativeStatus.m_Failure[0];
        if (g_FailStop)
        {
            dmStrlCpy(g_NativeStatus.m_Failure, "disk full", sizeof(g_NativeStatus.m_Failure));
        }
        *status = g_NativeStatus;
        return status->m_Finalized;
    }
    void FinalizeNative()
    {
        Status status;
        StopNative(&status);
    }
#if defined(DM_PLATFORM_LINUX)
    void CaptureFrame()
    {
        ASSERT_EQ(g_CapturedFrames + 1, g_RenderedFrames);
        ++g_CapturedFrames;
    }
#endif
}

#if defined(DM_PLATFORM_LINUX)
static ExtensionResult Render(dmExtension::Params*)
{
    ++g_RenderedFrames;
    return EXTENSION_RESULT_OK;
}

static dmExtension::Result AppInitializeRenderer(dmExtension::AppParams*)
{
    dmExtension::RegisterCallback(dmExtension::CALLBACK_POST_RENDER, Render);
    return dmExtension::RESULT_OK;
}

DM_DECLARE_EXTENSION(RecordTestRenderer, "RecordRenderer", AppInitializeRenderer, 0, 0, 0, 0, 0)
#endif

struct ScriptInstance
{
    int      m_Context;
    uint32_t m_Id;
    bool     m_Valid;
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
    dmScript::HContext     m_Context;
    dmConfigFile::HConfig  m_Config;
    lua_State*             m_L;
    ScriptInstance*        m_Instance;
    int                    m_InstanceRef;
    int                    m_Refs;
    dmExtension::AppParams m_AppParams;
    dmExtension::Params    m_Params;
#if defined(DM_PLATFORM_LINUX)
    HWindow              m_Window;
    dmGraphics::HContext m_GraphicsContext;
#endif

    void SetUp()
    {
        g_Mutex = dmMutex::New();
        memset(&g_NativeStatus, 0, sizeof(g_NativeStatus));
        g_Unsupported = g_FailStart = g_FailStop = g_Resized = false;
#if defined(DM_PLATFORM_LINUX)
        g_RenderedFrames = g_CapturedFrames = 0;
        WindowCreateParams window_params;
        WindowCreateParamsInitialize(&window_params);
        m_Window = WindowNew();
        ASSERT_EQ(WINDOW_RESULT_OK, WindowOpen(m_Window, &window_params));
        dmGraphics::ContextParams graphics_params;
        graphics_params.m_Window = m_Window;
        m_GraphicsContext = dmGraphics::NewContext(graphics_params);
        ASSERT_NE((dmGraphics::HContext)0, m_GraphicsContext);
        g_NativeWindow = (void*)m_GraphicsContext;
#endif
        ExtensionAppParamsInitialize(&m_AppParams);
        ExtensionParamsInitialize(&m_Params);
        ASSERT_EQ(dmExtension::RESULT_OK, dmExtension::AppInitialize(&m_AppParams));
        ASSERT_EQ(dmConfigFile::RESULT_OK, dmConfigFile::LoadFromBuffer("", 0, 0, 0, &m_Config));
        dmScript::ContextParams params = {};
        params.m_ConfigFile = m_Config;
        m_Context = dmScript::NewContext(params);
        dmScript::Initialize(m_Context);
        m_L = dmScript::GetLuaState(m_Context);
        const luaL_Reg methods[] = { { 0, 0 } };
        const luaL_Reg meta[] = {
            { dmScript::META_GET_INSTANCE_CONTEXT_TABLE_REF, InstanceContext },
            { dmScript::META_GET_UNIQUE_SCRIPT_ID, InstanceId },
            { dmScript::META_TABLE_IS_VALID, InstanceValid },
            { 0, 0 }
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
        m_Params.m_L = m_L;
        ASSERT_EQ(dmExtension::RESULT_OK, dmExtension::Initialize(&m_Params));
        m_Refs = dmScript::GetLuaRefCount();
        ASSERT_TRUE(Run(
            "events = {}; results = {}; "
            "function callback(self, event, result) "
            "assert(self == expected_self); "
            "events[#events + 1] = event; results[#results + 1] = result; end"));
    }

    void TearDown()
    {
        EngineDelete();
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
        g_Mutex = 0;
        m_Params.m_L = 0;
#if defined(DM_PLATFORM_LINUX)
        dmGraphics::CloseWindow(m_GraphicsContext);
        dmGraphics::DeleteContext(m_GraphicsContext);
        WindowDelete(m_Window);
        g_NativeWindow = 0;
#endif
        // Late extension cleanup must be safe after Lua and capture resources are gone.
        ASSERT_EQ(dmExtension::RESULT_OK, dmExtension::Finalize(&m_Params));
        ASSERT_EQ(dmExtension::RESULT_OK, dmExtension::AppFinalize(&m_AppParams));
        ExtensionParamsFinalize(&m_Params);
        ExtensionAppParamsFinalize(&m_AppParams);
    }

    void UpdateExtension()
    {
        ASSERT_EQ(dmExtension::RESULT_OK, dmExtension::Update(&m_Params));
    }

    void EngineDelete()
    {
        dmExtension::Event event;
        event.m_Event = EXTENSION_EVENT_ID_ENGINE_DELETE;
        dmExtension::DispatchEvent(&m_Params, &event);
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

    void Complete(bool update = true)
    {
        dmRecord::Status status;
        uint64_t deadline = dmTime::GetMonotonicTime() + 5000000;
        do
        {
            dmRecord::GetStatus(&status);
            if (update && !status.m_Pending)
            {
                bool was_active = status.m_Active;
                dmRecord::Update();
                dmRecord::GetStatus(&status);
                if (was_active && !status.m_Pending && !status.m_Active)
                {
                    dmRecord::Update();
                }
            }
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
    UpdateExtension();
    ASSERT_TRUE(Run("assert(#events == 1 and events[1] == recorder.EVENT_STARTED); "
                    "assert(results[1].path == 'capture.mp4' and results[1].fps == 24); "
                    "assert(results[1].width == 320 and results[1].height == 240 and not results[1].audio); "
                    "assert(recorder.stop()); assert(#events == 1)"));
    Complete();
    UpdateExtension();
    UpdateExtension();
    ASSERT_TRUE(Run("assert(#events == 2 and events[2] == recorder.EVENT_STOPPED)"));
}

// Extension updates advance recording before dispatching its Lua notifications, without an HTTP service.
TEST_F(RecordScriptTest, UpdateBeforeCallbacks)
{
    ASSERT_TRUE(Run("assert(recorder.start({path='capture.mp4'}, callback))"));
    Complete(false);
    ASSERT_TRUE(Run("assert(#events == 0)"));
    UpdateExtension();
    ASSERT_TRUE(Run("assert(#events == 1 and events[1] == recorder.EVENT_STARTED)"));
    g_Resized = true;
    UpdateExtension();
    Complete(false);
    UpdateExtension();
    dmRecord::Status status;
    dmRecord::GetStatus(&status);
    ASSERT_FALSE(status.m_Active);
    ASSERT_FALSE(status.m_Pending);
    ASSERT_TRUE(Run("assert(#events == 2 and events[1] == recorder.EVENT_STARTED and events[2] == recorder.EVENT_STOPPED)"));
}

// Paths from the Windows host code page reach native capture as UTF-8 and return to Lua unchanged.
TEST_F(RecordScriptTest, HostPathEncoding)
{
    char host[128];
    char expected[128];
#if defined(_WIN32)
    const wchar_t path[] = L"capture-\x00e9-\x0416-\x65e5.mp4";
    wchar_t wide[128];
    ASSERT_GT(WideCharToMultiByte(CP_ACP, 0, path, -1, host, sizeof(host), 0, 0), 0);
    ASSERT_GT(MultiByteToWideChar(CP_ACP, 0, host, -1, wide, sizeof(wide) / sizeof(wide[0])), 0);
    ASSERT_GT(WideCharToMultiByte(CP_UTF8, 0, wide, -1, expected, sizeof(expected), 0, 0), 0);
#else
    strcpy(host, "capture-\xc3\xa9-\xe6\x97\xa5.mp4");
    strcpy(expected, host);
#endif
    lua_pushstring(m_L, host);
    lua_setglobal(m_L, "output_path");
    ASSERT_TRUE(Run("assert(recorder.start({path=output_path}, callback))"));
    Complete();
    {
        DM_MUTEX_SCOPED_LOCK(g_Mutex);
        ASSERT_STREQ(expected, g_NativeStatus.m_Path);
    }
    UpdateExtension();
    ASSERT_TRUE(Run("assert(#events == 1 and results[1].path == output_path); assert(recorder.stop())"));
    Complete();
    UpdateExtension();
    ASSERT_TRUE(Run("assert(#events == 2 and events[2] == recorder.EVENT_STOPPED); "
                    "assert(results[2].path == output_path)"));
}

#if defined(_WIN32)
// A host path that expands beyond the native UTF-8 limit must fail before allocating a callback.
TEST_F(RecordScriptTest, ExpandedHostPathLimit)
{
    const wchar_t candidates[] = { 0x00e9, 0x0416, 0x65e5, 0xac00, 0x03a9, 0x0e01, 0x05d0, 0x0633 };
    char host_character[8];
    size_t host_size = 0;
    for (size_t i = 0; i < sizeof(candidates) / sizeof(candidates[0]); ++i)
    {
        wchar_t wide[] = { candidates[i], 0 };
        wchar_t roundtrip[2];
        char utf8[8];
        int size = WideCharToMultiByte(CP_ACP, 0, wide, -1, host_character, sizeof(host_character), 0, 0);
        if (size <= 1 || !MultiByteToWideChar(CP_ACP, 0, host_character, -1, roundtrip, 2) || roundtrip[0] != wide[0])
        {
            continue;
        }
        int utf8_size = WideCharToMultiByte(CP_UTF8, 0, wide, -1, utf8, sizeof(utf8), 0, 0);
        if (utf8_size > size)
        {
            host_size = size - 1;
            break;
        }
    }
    if (!host_size)
    {
        SKIP(); // UTF-8 system locales cannot expand during this conversion.
    }
    char path[1024];
    size_t length = 0;
    while (length + host_size + sizeof(".mp4") <= sizeof(path))
    {
        memcpy(path + length, host_character, host_size);
        length += host_size;
    }
    strcpy(path + length, ".mp4");
    lua_pushstring(m_L, path);
    lua_setglobal(m_L, "output_path");
    ASSERT_TRUE(Run("local ok, err = pcall(recorder.start, {path=output_path}, callback); "
                    "assert(not ok and string.find(err, 'UTF-8 conversion', 1, true)); assert(#events == 0)"));
    ASSERT_EQ(m_Refs, dmScript::GetLuaRefCount());
    ASSERT_FALSE(g_NativeStatus.m_Active);
}
#endif

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
    UpdateExtension();
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
    UpdateExtension();
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
    UpdateExtension();
    ASSERT_TRUE(Run("assert(results[1].path == string.rep('a', 100) .. '.mp4')"));
}

// A permission/startup failure produces one terminal error and allows a subsequent recording.
TEST_F(RecordScriptTest, StartFailure)
{
    g_FailStart = true;
    ASSERT_TRUE(Run("assert(recorder.start({path='capture.mp4'}, callback))"));
    Complete();
    UpdateExtension();
    ASSERT_TRUE(Run("assert(#events == 1 and events[1] == recorder.EVENT_ERROR); "
                    "assert(results[1].error == 'permission denied' and results[1].path == 'capture.mp4')"));
    g_FailStart = false;
    ASSERT_TRUE(Run("assert(recorder.start({path='retry.mp4'}, callback))"));
    Complete();
    UpdateExtension();
    ASSERT_TRUE(Run("assert(events[2] == recorder.EVENT_STARTED)"));
}

// Failure while finalizing an accepted stop must report ERROR instead of a usable MP4.
TEST_F(RecordScriptTest, StopFailure)
{
    ASSERT_TRUE(Run("assert(recorder.start({path='capture.mp4'}, callback))"));
    Complete();
    UpdateExtension();
    g_FailStop = true;
    ASSERT_TRUE(Run("assert(recorder.stop())"));
    Complete();
    UpdateExtension();
    ASSERT_TRUE(Run("assert(#events == 2 and events[2] == recorder.EVENT_ERROR and results[2].error == 'disk full')"));
}

// Completion is retained even when an HTTP/C++ caller starts another recording before Lua updates.
TEST_F(RecordScriptTest, ExternalReplacement)
{
    ASSERT_TRUE(Run("assert(recorder.start({path='lua.mp4'}, callback))"));
    Complete(false);
    dmRecord::Status status;
    ASSERT_TRUE(dmRecord::Stop(&status));
    Complete(false);
    dmRecord::StartParams params;
    params.m_Window = g_NativeWindow;
    params.m_Path = "http.mp4";
    ASSERT_TRUE(dmRecord::Start(&params, &status));
    Complete(false);
    UpdateExtension();
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
    UpdateExtension();
    Complete();
    UpdateExtension();
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
    UpdateExtension();
    Complete();
    UpdateExtension();
    ASSERT_TRUE(Run("assert(#events == 2 and events[1] == recorder.EVENT_STARTED and events[2] == recorder.EVENT_STOPPED)"));
}

// A capture error alone must deliver one terminal callback and permit another recording.
TEST_F(RecordScriptTest, CaptureFailure)
{
    ASSERT_TRUE(Run("assert(recorder.start({path='capture.mp4'}, callback))"));
    Complete();
    UpdateExtension();
    {
        DM_MUTEX_SCOPED_LOCK(g_Mutex);
        dmStrlCpy(g_NativeStatus.m_Failure, "capture failed", sizeof(g_NativeStatus.m_Failure));
    }
    Complete();
    UpdateExtension();
    UpdateExtension();
    ASSERT_TRUE(Run("assert(#events == 2 and events[2] == recorder.EVENT_ERROR); "
                    "assert(results[2].error == 'capture failed'); "
                    "assert(recorder.start({path='retry.mp4'}, callback))"));
    Complete();
    UpdateExtension();
    ASSERT_TRUE(Run("assert(#events == 3 and events[3] == recorder.EVENT_STARTED)"));
}

// Resizing finalizes recording through ordinary updates without requiring a Lua stop call.
TEST_F(RecordScriptTest, Resize)
{
    ASSERT_TRUE(Run("assert(recorder.start({path='capture.mp4'}, callback))"));
    Complete();
    UpdateExtension();
    g_Resized = true;
    Complete();
    UpdateExtension();
    ASSERT_TRUE(Run("assert(#events == 2 and events[2] == recorder.EVENT_STOPPED)"));
}

// A resize before the first update must preserve STARTED before the automatic STOPPED event.
TEST_F(RecordScriptTest, ResizeBeforeFirstUpdate)
{
    ASSERT_TRUE(Run("assert(recorder.start({path='capture.mp4'}, callback))"));
    g_Resized = true;
    Complete();
    ASSERT_TRUE(Run("assert(#events == 0)"));
    UpdateExtension();
    UpdateExtension();
    ASSERT_TRUE(Run("assert(#events == 2 and events[1] == recorder.EVENT_STARTED and events[2] == recorder.EVENT_STOPPED)"));
}

// Deleting the script suppresses its callback while allowing the global recording to finish.
TEST_F(RecordScriptTest, DeletedScript)
{
    ASSERT_TRUE(Run("assert(recorder.start({path='capture.mp4'}, callback))"));
    Complete();
    m_Instance->m_Valid = false;
    m_Instance->m_Id = dmScript::GenerateUniqueScriptId();
    UpdateExtension();
    dmRecord::Status status;
    dmRecord::GetStatus(&status);
    ASSERT_TRUE(status.m_Active);
    ASSERT_EQ(m_Refs, dmScript::GetLuaRefCount());
    ASSERT_TRUE(Run("assert(#events == 0); assert(recorder.stop())"));
    Complete();
    UpdateExtension();
    ASSERT_TRUE(Run("assert(#events == 0)"));
}

// Shutdown joins pending work, releases callbacks, and rejects starts from later script finalizers.
TEST_F(RecordScriptTest, Shutdown)
{
    ASSERT_TRUE(Run("assert(recorder.start({path='capture.mp4'}, callback))"));
    EngineDelete();
    ASSERT_FALSE(g_NativeStatus.m_Active);
    ASSERT_TRUE(Run("assert(#events == 0); "
                    "local ok, err = recorder.start({path='late.mp4'}, callback); assert(ok == nil and err); "
                    "ok, err = recorder.stop(); assert(ok == nil and err)"));
}

// Reboot must discard the previous callback and restore Lua recording without duplicate status listeners.
TEST_F(RecordScriptTest, ShutdownAndReboot)
{
    for (uint32_t run = 1; run <= 2; ++run)
    {
        ASSERT_TRUE(Run("assert(recorder.start({path='capture.mp4'}, callback))"));
        Complete(false);
        UpdateExtension();
        ASSERT_TRUE(Run("assert(#events == 1 and events[1] == recorder.EVENT_STARTED); events = {}"));
        EngineDelete();
        ASSERT_FALSE(g_NativeStatus.m_Active);
        ASSERT_EQ(m_Refs, dmScript::GetLuaRefCount());
        ASSERT_EQ(dmExtension::RESULT_OK, dmExtension::Finalize(&m_Params));
        ASSERT_EQ(dmExtension::RESULT_OK, dmExtension::AppFinalize(&m_AppParams));
        if (run == 1)
        {
            ASSERT_EQ(dmExtension::RESULT_OK, dmExtension::AppInitialize(&m_AppParams));
            ASSERT_EQ(dmExtension::RESULT_OK, dmExtension::Initialize(&m_Params));
        }
    }
}

// Hosts without ENGINE_DELETE still join capture and release Lua callbacks during extension finalization.
TEST_F(RecordScriptTest, FinalizeWithoutEngineDelete)
{
    ASSERT_TRUE(Run("assert(recorder.start({path='capture.mp4'}, callback))"));
    ASSERT_EQ(dmExtension::RESULT_OK, dmExtension::Finalize(&m_Params));
    ASSERT_FALSE(g_NativeStatus.m_Active);
    ASSERT_EQ(m_Refs, dmScript::GetLuaRefCount());
    ASSERT_TRUE(Run("assert(#events == 0); local ok, err = recorder.stop(); assert(ok == nil and err)"));
}

// Failure before Lua initialization must allow early and repeated cleanup with no script or capture resources.
TEST(RecordScriptExtension, ShutdownBeforeLuaInitialization)
{
    dmExtension::AppParams app_params;
    dmExtension::Params params;
    ExtensionAppParamsInitialize(&app_params);
    ExtensionParamsInitialize(&params);
    ASSERT_EQ(dmExtension::RESULT_OK, dmExtension::AppInitialize(&app_params));
    dmExtension::Event event;
    event.m_Event = EXTENSION_EVENT_ID_ENGINE_DELETE;
    dmExtension::DispatchEvent(&params, &event);
    ASSERT_EQ(dmExtension::RESULT_OK, dmExtension::Finalize(&params));
    ASSERT_EQ(dmExtension::RESULT_OK, dmExtension::AppFinalize(&app_params));
    ExtensionParamsFinalize(&params);
    ExtensionAppParamsFinalize(&app_params);
}

#if defined(DM_PLATFORM_LINUX)
// Registering recording before rendering extensions captures their drawing before the engine flips.
TEST_F(RecordScriptTest, CaptureAfterExtensionRendering)
{
    for (uint32_t frame = 1; frame <= 3; ++frame)
    {
        dmExtension::PostRender(&m_Params);
        ASSERT_EQ(frame, g_RenderedFrames);
        ASSERT_EQ(frame, g_CapturedFrames);
    }
}
#endif

int main(int argc, char** argv)
{
    // Match engine/Extender registration: callbacks traverse the prepended list.
    ScriptRecord();
#if defined(DM_PLATFORM_LINUX)
    RecordTestRenderer();
    GraphicsAdapterNull();
    if (!dmGraphics::InstallAdapter(dmGraphics::ADAPTER_FAMILY_NULL))
    {
        return 1;
    }
#endif
    jc_test_init(&argc, argv);
    return jc_test_run_all();
}
