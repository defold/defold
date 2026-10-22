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

#include <record/record_private.h>
#include <record/record_file.h>
#include <record/record_service.h>
#include <dlib/array.h>
#include <dlib/atomic.h>
#include <dlib/dstrings.h>
#include <dlib/math.h>
#include <dlib/mutex.h>
#include <dlib/socket.h>
#include <dlib/time.h>
#include <dlib/thread.h>
#include <dlib/webserver.h>
#include <extension/extension.hpp>
#include <dmsdk/engine/extension.hpp>
#if defined(DM_PLATFORM_MACOS) || defined(DM_PLATFORM_WINDOWS)
#include <dmsdk/graphics/graphics_native.h>
#elif defined(DM_PLATFORM_LINUX)
#include <dmsdk/graphics/graphics.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// A controlled capture backend serves real HTTP without graphics, automation,
// Lua, native capture permissions or time-dependent recording assumptions.
namespace dmRecord
{
    static dmMutex::HMutex g_Mutex;
    static Status          g_Status;
    static bool            g_Supported = true;
    static bool            g_FailStart;
    static bool            g_NoWindow;
    static bool            g_AudioSupported;
    static bool            g_LargeRecording;
    static bool            g_Teardown;
    static uint32_t        g_Finalizes;
    static uint32_t        g_LateFinalizes;

    bool IsSupported()
    {
        return g_Supported;
    }
    bool IsAudioSupported()
    {
        return g_AudioSupported;
    }
    const char* GetBackendName()
    {
        return "test";
    }
    const char* GetMinimumPlatformVersion()
    {
        return "test platform";
    }
    const char* GetUnsupportedReason()
    {
        return "unsupported test platform";
    }
    bool GetWindow(void*, Window* window)
    {
        window->m_Handle = 1;
        return true;
    }

    bool PrepareWindow(void* native_window, Window* window)
    {
        return GetWindow(native_window, window);
    }

    void RestoreWindow() {}

    void GetNativeStatus(Status* status)
    {
        DM_MUTEX_SCOPED_LOCK(g_Mutex);
        *status = g_Status;
        status->m_Supported = IsSupported();
    }

    bool StartNative(const Window*, const char* path, uint32_t fps, bool audio, Status* status)
    {
        DM_MUTEX_SCOPED_LOCK(g_Mutex);
        memset(&g_Status, 0, sizeof(g_Status));
        g_Status.m_Supported = true;
        dmStrlCpy(g_Status.m_Path, path, sizeof(g_Status.m_Path));
        FILE* file = FileOpen(path, "wb");
        if (!file || g_FailStart)
        {
            dmStrlCpy(g_Status.m_Failure, "test start failed", sizeof(g_Status.m_Failure));
            if (file)
            {
                fclose(file);
            }
        }
        else
        {
            if (g_LargeRecording)
            {
                uint8_t contents[65536];
                for (uint32_t i = 0; i < sizeof(contents); ++i)
                {
                    contents[i] = (uint8_t)i;
                }
                for (uint32_t i = 0; i < 16; ++i)
                {
                    fwrite(contents, 1, sizeof(contents), file);
                }
            }
            else
            {
                const char contents[] = "standalone recording test\n";
                fwrite(contents, 1, sizeof(contents) - 1, file);
            }
            fclose(file);
            g_Status.m_Active = true;
            g_Status.m_Width = 320;
            g_Status.m_Height = 240;
            g_Status.m_Fps = fps;
            g_Status.m_Audio = audio;
        }
        *status = g_Status;
        return status->m_Active;
    }

    bool StopNative(Status* status)
    {
        DM_MUTEX_SCOPED_LOCK(g_Mutex);
        g_Status.m_Active = false;
        g_Status.m_Finalized = true;
        *status = g_Status;
        return true;
    }

    void FinalizeNative()
    {
        Status status;
        StopNative(&status);
        DM_MUTEX_SCOPED_LOCK(g_Mutex);
        ++g_Finalizes;
        if (g_Teardown)
        {
            ++g_LateFinalizes;
        }
    }
} // namespace dmRecord

static bool                   g_Quit;
static bool                   g_PauseUpdates;
static uint64_t               g_Updates;
static dmThread::TlsKey       g_UpdateThread;
static int32_atomic_t         g_StreamBytes;
static int32_atomic_t         g_StreamClosed;
static int32_atomic_t         g_StreamOnUpdate;
static int32_atomic_t         g_StreamOversizedReads;
static dmWebServer::HServer   g_Server;
static const char*            g_Directory;
static dmExtension::AppParams g_AppParams;
static dmExtension::Params    g_Params;
static void* TestNativeWindow()
{
    return dmRecord::g_NoWindow ? 0 : (void*)1;
}

// Supply the host entry points while exercising the real extension registration.
namespace dmEngine
{
    dmWebServer::HServer GetWebServer(dmExtension::AppParams*)
    {
        return g_Server;
    }
} // namespace dmEngine

namespace dmGraphics
{
#if defined(DM_PLATFORM_MACOS)
    id GetNativeOSXNSWindow()
    {
        return (id)TestNativeWindow();
    }
#elif defined(DM_PLATFORM_WINDOWS)
    HWND GetNativeWindowsHWND()
    {
        return (HWND)TestNativeWindow();
    }
#elif defined(DM_PLATFORM_LINUX)
    HContext GetInstalledContext()
    {
        return (HContext)TestNativeWindow();
    }
#endif
} // namespace dmGraphics

#if defined(DM_PLATFORM_LINUX)
namespace dmRecord
{
    void CaptureFrame() {}
}
#endif

extern "C" void EngineRecord();

static void EngineDelete()
{
    dmExtension::Event event;
    event.m_Event = EXTENSION_EVENT_ID_ENGINE_DELETE;
    dmExtension::DispatchEvent(&g_Params, &event);
    DM_MUTEX_SCOPED_LOCK(dmRecord::g_Mutex);
    dmRecord::g_Teardown = true;
}

static bool InitializeService()
{
    // Retain test-owned storage when AppInitialize registers the same server.
    return dmRecord::RegisterService(g_Server, TestNativeWindow, g_Directory) &&
        dmExtension::AppInitialize(&g_AppParams) == dmExtension::RESULT_OK &&
        dmExtension::Initialize(&g_Params) == dmExtension::RESULT_OK;
}

static void FinalizeExtensions()
{
    dmExtension::Finalize(&g_Params);
    dmExtension::AppFinalize(&g_AppParams);
}

static void SendLifecycle(dmWebServer::Request* request)
{
    DM_MUTEX_SCOPED_LOCK(dmRecord::g_Mutex);
    char body[128];
    dmSnPrintf(body, sizeof(body), "{\"active\":%s,\"finalizes\":%u,\"late_finalizes\":%u}", dmRecord::g_Status.m_Active ? "true" : "false", dmRecord::g_Finalizes, dmRecord::g_LateFinalizes);
    dmWebServer::Send(request, body, (uint32_t)strlen(body));
}

struct TestStream
{
    uint32_t m_Offset;
    uint32_t m_ReadLimit;
    uint32_t m_FailAfter;
};

static uint32_t ReadTestStream(void* context, void* buffer, uint32_t size)
{
    TestStream* stream = (TestStream*)context;
    if (dmThread::GetTlsValue(g_UpdateThread))
    {
        dmAtomicIncrement32(&g_StreamOnUpdate);
    }
    if (size > 65536)
    {
        dmAtomicIncrement32(&g_StreamOversizedReads);
    }
    size = dmMath::Min(size, dmMath::Min(stream->m_ReadLimit, stream->m_FailAfter - stream->m_Offset));
    for (uint32_t i = 0; i < size; ++i)
    {
        ((uint8_t*)buffer)[i] = (uint8_t)(stream->m_Offset + i);
    }
    stream->m_Offset += size;
    dmAtomicAdd32(&g_StreamBytes, size);
    return size;
}

static void CloseTestStream(void* context)
{
    delete (TestStream*)context;
    dmAtomicIncrement32(&g_StreamClosed);
}

static void TestHandler(void*, dmWebServer::Request* request)
{
    if (!strcmp(request->m_Resource, "/test/quit"))
    {
        g_Quit = true;
    }
    else if (!strcmp(request->m_Resource, "/test/pause-updates"))
    {
        g_PauseUpdates = true;
    }
    else if (!strcmp(request->m_Resource, "/test/fail-start"))
    {
        dmRecord::g_FailStart = true;
    }
    else if (!strcmp(request->m_Resource, "/test/unsupported"))
    {
        dmRecord::g_Supported = false;
    }
    else if (!strcmp(request->m_Resource, "/test/no-window"))
    {
        dmRecord::g_NoWindow = true;
    }
    else if (!strcmp(request->m_Resource, "/test/audio-support"))
    {
        dmRecord::g_AudioSupported = true;
    }
    else if (!strcmp(request->m_Resource, "/test/large-recording"))
    {
        dmRecord::g_LargeRecording = true;
    }
    else if (!strcmp(request->m_Resource, "/test/large-response"))
    {
        uint8_t data[65536];
        for (uint32_t i = 0; i < sizeof(data); ++i)
        {
            data[i] = (uint8_t)i;
        }
        for (uint32_t i = 0; i < 16; ++i)
        {
            if (dmWebServer::Send(request, data, sizeof(data)) != dmWebServer::RESULT_OK)
            {
                break;
            }
        }
    }
    else if (!strncmp(request->m_Resource, "/test/stream/", 13))
    {
        TestStream* stream = new TestStream();
        const char* limit = dmWebServer::GetHeader(request, "X-Test-Read-Limit");
        const char* fail = dmWebServer::GetHeader(request, "X-Test-Fail-After");
        stream->m_ReadLimit = limit ? (uint32_t)strtoul(limit, 0, 10) : UINT32_MAX;
        stream->m_FailAfter = fail ? (uint32_t)strtoul(fail, 0, 10) : UINT32_MAX;
        uint64_t length = strtoull(request->m_Resource + 13, 0, 10);
        if (dmWebServer::SendStream(request, length, ReadTestStream, CloseTestStream, stream) != dmWebServer::RESULT_OK)
        {
            CloseTestStream(stream);
            dmWebServer::SetStatusCode(request, 500);
        }
    }
    else if (!strcmp(request->m_Resource, "/test/stream-stats"))
    {
        char body[160];
        dmSnPrintf(body, sizeof(body), "{\"bytes\":%d,\"closed\":%d,\"on_update\":%d,\"oversized_reads\":%d}", dmAtomicGet32(&g_StreamBytes), dmAtomicGet32(&g_StreamClosed), dmAtomicGet32(&g_StreamOnUpdate), dmAtomicGet32(&g_StreamOversizedReads));
        dmWebServer::Send(request, body, (uint32_t)strlen(body));
    }
    else if (!strcmp(request->m_Resource, "/test/updates"))
    {
        char body[64];
        dmSnPrintf(body, sizeof(body), "{\"updates\":%llu}", (unsigned long long)g_Updates);
        dmWebServer::Send(request, body, (uint32_t)strlen(body));
    }
    else if (!strcmp(request->m_Resource, "/test/replace-recording"))
    {
        // A non-HTTP caller stops the service recording and immediately starts
        // its own, before another Update can consume the old receipt.
        dmRecord::Status status;
        bool stopped = dmRecord::Stop(&status);
        uint64_t deadline = dmTime::GetMonotonicTime() + 5000000;
        while (status.m_Pending && dmTime::GetMonotonicTime() < deadline)
        {
            dmRecord::GetStatus(&status);
        }
        char path[1024];
        dmSnPrintf(path, sizeof(path), "%s/caller.mp4", g_Directory);
        dmRecord::StartParams params;
        params.m_Window = TestNativeWindow();
        params.m_Path = path;
        if (!stopped || status.m_Pending || !dmRecord::Start(&params, &status))
        {
            dmWebServer::SetStatusCode(request, 500);
        }
    }
    else if (!strcmp(request->m_Resource, "/test/restart"))
    {
        EngineDelete();
        FinalizeExtensions();
        memset(&dmRecord::g_Status, 0, sizeof(dmRecord::g_Status));
        dmRecord::g_Supported = true;
        dmRecord::g_FailStart = false;
        dmRecord::g_NoWindow = false;
        dmRecord::g_AudioSupported = false;
        dmRecord::g_LargeRecording = false;
        dmRecord::g_Teardown = false;
        dmRecord::g_Finalizes = 0;
        dmRecord::g_LateFinalizes = 0;
        if (!InitializeService())
        {
            dmWebServer::SetStatusCode(request, 500);
        }
    }
    else if (!strcmp(request->m_Resource, "/test/engine-delete"))
    {
        EngineDelete();
        SendLifecycle(request);
    }
    else if (!strcmp(request->m_Resource, "/test/finalize"))
    {
        FinalizeExtensions();
        SendLifecycle(request);
    }
    else if (!strcmp(request->m_Resource, "/test/deactivate"))
    {
        dmExtension::Event event;
        event.m_Event = EXTENSION_EVENT_ID_DEACTIVATEAPP;
        dmExtension::DispatchEvent(&g_Params, &event);
        SendLifecycle(request);
    }
    else if (!strcmp(request->m_Resource, "/test/unregister"))
    {
        dmRecord::FinalizeService();
    }
    else if (!strcmp(request->m_Resource, "/openapi.json"))
    {
        dmArray<char> paths;
        dmWebServer::GetOpenAPIPaths(g_Server, &paths);
        const char header[] = "{\"openapi\":\"3.0.3\",\"info\":{\"title\":\"Record test\",\"version\":\"1\"},\"paths\":";
        dmWebServer::Send(request, header, sizeof(header) - 1);
        dmWebServer::Send(request, paths.Begin(), paths.Size() - 1);
        dmWebServer::Send(request, "}", 1);
    }
    else
    {
        dmWebServer::SetStatusCode(request, 404);
    }
}

#if defined(_WIN32)
int wmain(int argc, wchar_t** argv)
#else
int main(int argc, char** argv)
#endif
{
    if (argc != 2)
    {
        return 2;
    }
#if defined(_WIN32)
    char directory[1024];
    if (!WideCharToMultiByte(CP_UTF8, 0, argv[1], -1, directory, sizeof(directory), 0, 0))
    {
        return 2;
    }
    g_Directory = directory;
#else
    g_Directory = argv[1];
#endif
    dmSocket::Initialize();
    g_UpdateThread = dmThread::AllocTls();
    dmThread::SetTlsValue(g_UpdateThread, &g_Updates);
    dmRecord::g_Mutex = dmMutex::New();
    dmWebServer::NewParams params;
    params.m_Port = 0;
    const char* connection_timeout = getenv("DEFOLD_RECORD_TEST_CONNECTION_TIMEOUT");
    if (connection_timeout)
    {
        params.m_ConnectionTimeout = (uint16_t)strtoul(connection_timeout, 0, 10);
    }
    if (dmWebServer::New(&params, &g_Server) != dmWebServer::RESULT_OK)
    {
        return 3;
    }
    dmWebServer::HandlerParams handler;
    handler.m_Userdata = 0;
    handler.m_Handler = TestHandler;
    dmWebServer::AddHandler(g_Server, "/", &handler);
    EngineRecord();
    ExtensionAppParamsInitialize(&g_AppParams);
    ExtensionParamsInitialize(&g_Params);
    if (!InitializeService())
    {
        return 4;
    }
    dmSocket::Address address;
    uint16_t port;
    dmWebServer::GetName(g_Server, &address, &port);
    printf("RECORD_TEST_PORT=%u\n", port);
    fflush(stdout);
    while (!g_Quit)
    {
        dmWebServer::Update(g_Server);
        dmRecord::Update();
        ++g_Updates;
        if (g_PauseUpdates)
        {
            printf("RECORD_TEST_PAUSED\n");
            fflush(stdout);
            getchar();
            g_PauseUpdates = false;
        }
    }
    EngineDelete();
    FinalizeExtensions();
    ExtensionParamsFinalize(&g_Params);
    ExtensionAppParamsFinalize(&g_AppParams);
    dmWebServer::Delete(g_Server);
    dmThread::FreeTls(g_UpdateThread);
    dmMutex::Delete(dmRecord::g_Mutex);
    dmSocket::Finalize();
    return 0;
}
