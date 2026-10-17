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
#include <record/record_service.h>
#include <dlib/array.h>
#include <dlib/dstrings.h>
#include <dlib/mutex.h>
#include <dlib/socket.h>
#include <dlib/webserver.h>
#include <extension/extension.hpp>
#include <dmsdk/engine/extension.hpp>
#include <dmsdk/graphics/graphics_native.h>
#include <stdio.h>
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
    static bool            g_Teardown;
    static uint32_t        g_Finalizes;
    static uint32_t        g_LateFinalizes;

    bool                   IsSupported()
    {
        return g_Supported;
    }
    bool IsAudioSupported()
    {
        return false;
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

    void GetNativeStatus(Status* status)
    {
        dmMutex::ScopedLock lock(g_Mutex);
        *status = g_Status;
        status->m_Supported = IsSupported();
    }

    bool StartNative(const Window*, const char* path, uint32_t width, uint32_t height, uint32_t fps, bool audio, Status* status)
    {
        dmMutex::ScopedLock lock(g_Mutex);
        memset(&g_Status, 0, sizeof(g_Status));
        g_Status.m_Supported = true;
        dmStrlCpy(g_Status.m_Path, path, sizeof(g_Status.m_Path));
        FILE* file = fopen(path, "wb");
        if (!file || g_FailStart)
        {
            dmStrlCpy(g_Status.m_Failure, "test start failed", sizeof(g_Status.m_Failure));
            if (file)
                fclose(file);
        }
        else
        {
            const char contents[] = "standalone recording test\n";
            fwrite(contents, 1, sizeof(contents) - 1, file);
            fclose(file);
            g_Status.m_Active = true;
            g_Status.m_Width = width ? width : 320;
            g_Status.m_Height = height ? height : 240;
            g_Status.m_Fps = fps;
            g_Status.m_Audio = audio;
        }
        *status = g_Status;
        return status->m_Active;
    }

    bool StopNative(Status* status)
    {
        dmMutex::ScopedLock lock(g_Mutex);
        g_Status.m_Active = false;
        g_Status.m_Finalized = true;
        *status = g_Status;
        return true;
    }

    void FinalizeNative()
    {
        Status status;
        StopNative(&status);
        dmMutex::ScopedLock lock(g_Mutex);
        ++g_Finalizes;
        if (g_Teardown)
            ++g_LateFinalizes;
    }
} // namespace dmRecord

static bool                   g_Quit;
static dmWebServer::HServer   g_Server;
static const char*            g_Directory;
static dmExtension::AppParams g_AppParams;
static dmExtension::Params    g_Params;
static void*                  TestNativeWindow()
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
#endif
} // namespace dmGraphics

extern "C" void EngineRecord();

static void     EngineDelete()
{
    dmExtension::Event event;
    event.m_Event = EXTENSION_EVENT_ID_ENGINE_DELETE;
    dmExtension::DispatchEvent(&g_Params, &event);
    dmMutex::ScopedLock lock(dmRecord::g_Mutex);
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
    dmMutex::ScopedLock lock(dmRecord::g_Mutex);
    char                body[128];
    dmSnPrintf(body, sizeof(body), "{\"active\":%s,\"finalizes\":%u,\"late_finalizes\":%u}", dmRecord::g_Status.m_Active ? "true" : "false", dmRecord::g_Finalizes, dmRecord::g_LateFinalizes);
    dmWebServer::Send(request, body, (uint32_t)strlen(body));
}

static void TestHandler(void*, dmWebServer::Request* request)
{
    if (!strcmp(request->m_Resource, "/test/quit"))
        g_Quit = true;
    else if (!strcmp(request->m_Resource, "/test/fail-start"))
        dmRecord::g_FailStart = true;
    else if (!strcmp(request->m_Resource, "/test/unsupported"))
        dmRecord::g_Supported = false;
    else if (!strcmp(request->m_Resource, "/test/no-window"))
        dmRecord::g_NoWindow = true;
    else if (!strcmp(request->m_Resource, "/test/restart"))
    {
        EngineDelete();
        FinalizeExtensions();
        memset(&dmRecord::g_Status, 0, sizeof(dmRecord::g_Status));
        dmRecord::g_Supported = true;
        dmRecord::g_FailStart = false;
        dmRecord::g_NoWindow = false;
        dmRecord::g_Teardown = false;
        dmRecord::g_Finalizes = 0;
        dmRecord::g_LateFinalizes = 0;
        if (!InitializeService())
            dmWebServer::SetStatusCode(request, 500);
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
        dmRecord::FinalizeService();
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
        dmWebServer::SetStatusCode(request, 404);
}

int main(int argc, char** argv)
{
    if (argc != 2)
        return 2;
    g_Directory = argv[1];
    dmSocket::Initialize();
    dmRecord::g_Mutex = dmMutex::New();
    dmWebServer::NewParams params;
    params.m_Port = 0;
    if (dmWebServer::New(&params, &g_Server) != dmWebServer::RESULT_OK)
        return 3;
    dmWebServer::HandlerParams handler;
    handler.m_Userdata = 0;
    handler.m_Handler = TestHandler;
    dmWebServer::AddHandler(g_Server, "/", &handler);
    EngineRecord();
    ExtensionAppParamsInitialize(&g_AppParams);
    ExtensionParamsInitialize(&g_Params);
    if (!InitializeService())
        return 4;
    dmSocket::Address address;
    uint16_t          port;
    dmWebServer::GetName(g_Server, &address, &port);
    printf("RECORD_TEST_PORT=%u\n", port);
    fflush(stdout);
    while (!g_Quit)
    {
        dmWebServer::Update(g_Server);
        dmRecord::UpdateService();
    }
    EngineDelete();
    FinalizeExtensions();
    ExtensionParamsFinalize(&g_Params);
    ExtensionAppParamsFinalize(&g_AppParams);
    dmWebServer::Delete(g_Server);
    dmMutex::Delete(dmRecord::g_Mutex);
    dmSocket::Finalize();
    return 0;
}
