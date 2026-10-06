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

// Debug-only runtime inspection and input API for automation-driven Defold testing.

#include "automation_private.h"

#if defined(DM_PLATFORM_WINDOWS)
#include <windows.h>
#elif defined(DM_PLATFORM_MACOS) || defined(DM_PLATFORM_LINUX) || defined(ANDROID) || defined(DM_PLATFORM_IOS)
#include <unistd.h>
#endif

#define MODULE_NAME EngineAutomation
#define LIB_NAME "Automation"

#include "automation_artifact.h"
#include "automation.h"
#include <hid.h>
#include <graphics.h>
#include <extension.hpp>
#include <script_extension.h>
#include <dlib/webserver.h>
#include "automation_engine_access.h"
#include "automation_recording.h"

namespace dmAutomation
{
    AutomationBridgeContext g_AutomationBridge;
    static void FreeAutomationBridgeContext(AutomationBridgeContext* bridge)
    {
        FinalizeObservations();
        FinalizeArtifacts();
        FreeSnapshot(&bridge->m_Snapshot);
        for (uint32_t i = 0; i < bridge->m_InputEvents.Size(); ++i)
        {
            FreeInputEvent(&bridge->m_InputEvents.Begin()[i]);
        }
        ArrayFree(&bridge->m_InputEvents);
        for (uint32_t i = 0; i < bridge->m_InputHistory.Size(); ++i)
        {
            FreeInputReceipt(&bridge->m_InputHistory.Begin()[i]);
        }
        ArrayFree(&bridge->m_InputHistory);
        FreeString(&bridge->m_ControllerClientId);
        FreeString(&bridge->m_ControllerSessionId);
        FreeApplicationBridge();
        ArrayFree(&bridge->m_ScreenshotHistory);
    }

    static void InitAutomationBridgeContext(AutomationBridgeContext* bridge)
    {
        memset(bridge, 0, sizeof(*bridge));
        InitSnapshot(&bridge->m_Snapshot);
        bridge->m_NextInputId = dmTime::GetMonotonicTime();
        bridge->m_DefaultInputDevice = INPUT_DEVICE_AUTO;
        bridge->m_DefaultInputVisualize = true;
        bridge->m_DisplayWidth = 960;
        bridge->m_DisplayHeight = 640;
        bridge->m_ProcessId = -1;
    }

    static int64_t GetPortableProcessId()
    {
#if defined(DM_PLATFORM_WINDOWS)
        return (int64_t)GetCurrentProcessId();
#elif defined(DM_PLATFORM_MACOS) || defined(DM_PLATFORM_LINUX) || defined(ANDROID) || defined(DM_PLATFORM_IOS)
        return (int64_t)getpid();
#else
        return -1;
#endif
    }

    static void FormatIdentity(char* output, uint32_t output_size, const char* prefix, uint64_t hash)
    {
        dmSnPrintf(output, output_size, "%s:%016llx", prefix, (unsigned long long)hash);
    }

    static void InitializeIdentity(dmExtension::AppParams* params)
    {
        AutomationBridgeContext* bridge = &g_AutomationBridge;
        bridge->m_StartWallTime = dmTime::GetTime();
        bridge->m_StartMonotonicTime = dmTime::GetMonotonicTime();
        bridge->m_ProcessId = GetPortableProcessId();

        char instance_seed[128];
        dmSnPrintf(instance_seed, sizeof(instance_seed), "%llu:%llu:%p",
                   (unsigned long long)bridge->m_StartWallTime,
                   (unsigned long long)bridge->m_StartMonotonicTime,
                   (void*)bridge);
        FormatIdentity(bridge->m_EngineInstanceId, sizeof(bridge->m_EngineInstanceId), "engine", dmHashString64(instance_seed));

        const char* title = "";
        const char* version = "";
        const char* bootstrap = "";
        if (params->m_ConfigFile)
        {
            title = dmConfigFile::GetString(params->m_ConfigFile, "project.title", "");
            version = dmConfigFile::GetString(params->m_ConfigFile, "project.version", "");
            bootstrap = dmConfigFile::GetString(params->m_ConfigFile, "bootstrap.main_collection", "");
        }
        FormatIdentity(bridge->m_ProjectIdentity, sizeof(bridge->m_ProjectIdentity), "project", dmHashString64(title));

        char build_seed[1024];
        dmSnPrintf(build_seed, sizeof(build_seed), "%s\n%s\n%s", title, version, bootstrap);
        FormatIdentity(bridge->m_BuildIdentity, sizeof(bridge->m_BuildIdentity), "config", dmHashString64(build_seed));
    }

    static ExtensionResult PreRender(ExtensionParams* params)
    {
        EngineAccessInitialize(params);
        // Flip() submits the Metal command buffer after CALLBACK_POST_RENDER.
        // Finish an active capture here so the previous frame is included.
        ProcessMetalCapturePostRender();
        ProcessPendingMetalCaptureStart();
        return EXTENSION_RESULT_OK;
    }

    static ExtensionResult PostRender(ExtensionParams* params)
    {
        EngineAccessInitialize(params);
        EngineAccessDrawInputVisualization(&g_AutomationBridge.m_InputVisualization);
        ProcessPendingScreenshot();
        CompleteObservations();
        return EXTENSION_RESULT_OK;
    }

    static dmExtension::Result AppInitialize(dmExtension::AppParams* params)
    {
        FreeAutomationBridgeContext(&g_AutomationBridge);
        InitAutomationBridgeContext(&g_AutomationBridge);
        InitializeIdentity(params);
        g_AutomationBridge.m_ScreenshotCounter = dmTime::GetMonotonicTime();
        g_AutomationBridge.m_Register = dmEngine::GetGameObjectRegister(params);
        g_AutomationBridge.m_HidContext = dmEngine::GetHIDContext(params);
        g_AutomationBridge.m_WebServer = dmEngine::GetWebServer(params);
        if (params->m_ConfigFile)
        {
            int32_t display_width = dmConfigFile::GetInt(params->m_ConfigFile, "display.width", 960);
            int32_t display_height = dmConfigFile::GetInt(params->m_ConfigFile, "display.height", 640);
            if (display_width > 0)
            {
                g_AutomationBridge.m_DisplayWidth = (uint32_t)display_width;
            }
            if (display_height > 0)
            {
                g_AutomationBridge.m_DisplayHeight = (uint32_t)display_height;
            }
        }
        g_AutomationBridge.m_LastTime = dmTime::GetTime();
        int32_t event_capacity = params->m_ConfigFile ? dmConfigFile::GetInt(params->m_ConfigFile, "automation_bridge.event_capacity", 256) : 256;
        event_capacity = event_capacity < 16 ? 16 : (event_capacity > 4096 ? 4096 : event_capacity);
        bool application_api_enabled = params->m_ConfigFile && dmConfigFile::GetInt(params->m_ConfigFile, "automation_bridge.application_api", 0) == 1;
        InitApplicationBridge((uint32_t)event_capacity, application_api_enabled);
        g_AutomationBridge.m_Initialized = true;
        RegisterWebEndpoint(params);
        dmExtension::RegisterCallback(dmExtension::CALLBACK_PRE_RENDER, PreRender);
        dmExtension::RegisterCallback(dmExtension::CALLBACK_POST_RENDER, PostRender);
        return dmExtension::RESULT_OK;
    }

    static void UpdateDebugger(dmScript::HContext, bool paused)
    {
        if (!g_AutomationBridge.m_Initialized) return;
        g_AutomationBridge.m_DebuggerPaused = paused;
        // Leases retain their wall-clock meaning. This only releases expired or
        // cancelled input; input dispatch and frame-dependent work wait for resume.
        MaintainInput();
        if (paused && g_AutomationBridge.m_WebServer)
            dmWebServer::Update(g_AutomationBridge.m_WebServer);
    }

    static dmExtension::Result Initialize(dmExtension::Params* params)
    {
        g_AutomationBridge.m_GraphicsContext = (dmGraphics::HContext)ExtensionParamsGetContextByName((ExtensionParams*)params, "graphics");
        EngineAccessInitialize(params);
        dmScript::HContext context = (dmScript::HContext)ExtensionParamsGetContextByName(params, SCRIPT_CONTEXT_NAME);
        static dmScript::ScriptExtension debugger_extension = {};
        debugger_extension.UpdateDebugger = UpdateDebugger;
        dmScript::RegisterScriptExtension(context, &debugger_extension);
        RegisterApplicationLua(params->m_L, context);
        dmLogInfo("Registered %s extension", LIB_NAME);
        return dmExtension::RESULT_OK;
    }

    static dmExtension::Result AppFinalize(dmExtension::AppParams* params)
    {
        (void)params;
        StopMetalCapture();
        FinalizeRecording();
        FreeAutomationBridgeContext(&g_AutomationBridge);
        InitAutomationBridgeContext(&g_AutomationBridge);
        return dmExtension::RESULT_OK;
    }

    static dmExtension::Result Finalize(dmExtension::Params* params)
    {
        (void)params;
        FinalizeApplicationLua();
        return dmExtension::RESULT_OK;
    }

    static dmExtension::Result OnUpdate(dmExtension::Params* params)
    {
        (void)params;
        if (!g_AutomationBridge.m_Initialized)
        {
            return dmExtension::RESULT_OK;
        }

        UpdateApplicationBridge();

        return dmExtension::RESULT_OK;
    }

    bool CheckExtensions()
    {
        if (!dmExtension::HasExtension("automation_bridge")) return true;
        dmLogError("Automation is built into this debug engine. Remove the legacy Automation Bridge native extension and rebuild.");
        return false;
    }

    bool HasPendingInput()
    {
        return g_AutomationBridge.m_Initialized &&
               (!g_AutomationBridge.m_InputEvents.Empty() ||
                (g_AutomationBridge.m_HidContext && dmHID::HasSyntheticTouch(g_AutomationBridge.m_HidContext)));
    }

    void BeforeInput(float dt, uint64_t frame)
    {
        if (!g_AutomationBridge.m_Initialized) return;
        g_AutomationBridge.m_Frame = frame;
        UpdateInput(dt);
    }

    void AfterInput()
    {
        if (g_AutomationBridge.m_HidContext) dmHID::ClearSyntheticTouch(g_AutomationBridge.m_HidContext);
        g_AutomationBridge.m_DispatchedFrame = g_AutomationBridge.m_Frame;
        for (uint32_t i = 0; i < g_AutomationBridge.m_InputHistory.Size(); ++i)
        {
            InputReceipt& receipt = g_AutomationBridge.m_InputHistory[i];
            if (receipt.m_ReleaseFrame && !receipt.m_DeliveredFrame && receipt.m_StartFrame)
                receipt.m_DeliveredFrame = g_AutomationBridge.m_Frame;
        }
        for (uint32_t i = 0; i < g_AutomationBridge.m_InputEvents.Size(); ++i)
        {
            InputReceipt& receipt = g_AutomationBridge.m_InputEvents[i].m_Receipt;
            if (receipt.m_ReleaseFrame && !receipt.m_DeliveredFrame && receipt.m_StartFrame)
                receipt.m_DeliveredFrame = g_AutomationBridge.m_Frame;
        }
    }

    void AfterUpdate(bool rendered)
    {
        if (!g_AutomationBridge.m_Initialized) return;
        // Input target validation may have cached the pre-update scene this frame.
        // Observations must see the completed update and its render projections.
        g_AutomationBridge.m_SnapshotFrame = UINT64_MAX;
        PrepareObservations(rendered && !dmGraphics::GetWindowStateParam(g_AutomationBridge.m_GraphicsContext, WINDOW_STATE_ICONIFIED) && IsScreenshotSupported());
    }

    void ServiceTick(bool rendering)
    {
        if (!g_AutomationBridge.m_Initialized) return;
        rendering = rendering && !dmGraphics::GetWindowStateParam(g_AutomationBridge.m_GraphicsContext, WINDOW_STATE_ICONIFIED);
        MaintainInput();
        PollRecording();
        if (!g_AutomationBridge.m_SceneReadyMonotonicTime && g_AutomationBridge.m_Frame)
            g_AutomationBridge.m_SceneReadyMonotonicTime = dmTime::GetMonotonicTime();
        CompleteObservations();
        ScreenshotCapture* capture = &g_AutomationBridge.m_Screenshot;
        if (!rendering && capture->m_State == SCREENSHOT_PENDING)
        {
            capture->m_State = SCREENSHOT_FAILED;
            dmStrlCpy(capture->m_Failure, "rendering_unavailable", sizeof(capture->m_Failure));
        }
        if (!rendering && (g_AutomationBridge.m_MetalCaptureState == METAL_CAPTURE_PENDING || g_AutomationBridge.m_MetalCaptureState == METAL_CAPTURE_CAPTURING))
        {
            StopMetalCapture();
            g_AutomationBridge.m_MetalCaptureState = METAL_CAPTURE_FAILED;
            dmStrlCpy(g_AutomationBridge.m_MetalCaptureError, "rendering_unavailable", sizeof(g_AutomationBridge.m_MetalCaptureError));
        }
        if (capture->m_State == SCREENSHOT_FAILED) ReleaseCapturePath(capture->m_Path);
        if (g_AutomationBridge.m_MetalCaptureState == METAL_CAPTURE_FAILED || g_AutomationBridge.m_MetalCaptureState == METAL_CAPTURE_CANCELED)
            ReleaseCapturePath(g_AutomationBridge.m_MetalCapturePath);
    }

    static void OnEvent(dmExtension::Params*, const dmExtension::Event* event)
    {
        if (event->m_Event != EXTENSION_EVENT_ID_ENGINE_DELETE || !g_AutomationBridge.m_Initialized) return;
        EngineAccessFinalize();
        FlushInput(0, 0, true, "engine_deleted");
        while (g_AutomationBridge.m_InputEvents.Size()) UpdateInput(0.0f);
        if (g_AutomationBridge.m_HidContext) dmHID::ClearSyntheticTouch(g_AutomationBridge.m_HidContext);
        StopMetalCapture();
        FinalizeRecording();
        g_AutomationBridge.m_Initialized = false;
    }

}

DM_DECLARE_EXTENSION(MODULE_NAME, LIB_NAME, dmAutomation::AppInitialize, dmAutomation::AppFinalize, dmAutomation::Initialize, dmAutomation::OnUpdate, dmAutomation::OnEvent, dmAutomation::Finalize)
