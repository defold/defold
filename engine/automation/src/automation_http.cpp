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

#include "automation_private.h"
#include "automation_artifact.h"
#include "automation.h"
#include <hid.h>
#include <dmsdk/dlib/array.h>
#include <dlib/sys.h>
#include <remotery/profiler_remotery.h>
#include "automation_engine_access.h"
#include "automation_recording.h"

#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <string.h>

namespace dmAutomation
{
    static const uint32_t MAX_SCREEN_DIMENSION = (uint32_t)INT_MAX;

    struct RequestContext
    {
        dmWebServer::Request*    m_Request;
        char*                    m_Path;
        char*                    m_Route;
        dmArray<QueryParam>        m_Query;
        dmArray<QueryParam>        m_JsonFields;
    };

    typedef void (*RouteHandler)(RequestContext* ctx);

    struct RouteDefinition
    {
        const char*  m_Route;
        const char*  m_Method;
        RouteHandler m_Handler;
    };

    static void SendResponse(dmWebServer::Request* request, int status_code, const char* content_type, const StringBuffer* response, const char* allow = 0)
    {
        const char* data = response->m_Data ? response->m_Data : "";
        dmWebServer::SetStatusCode(request, status_code);
        dmWebServer::SendAttribute(request, "Content-Type", content_type);
        dmWebServer::SendAttribute(request, "Cache-Control", "no-store");
        if (!IsEmpty(allow))
        {
            dmWebServer::SendAttribute(request, "Allow", allow);
        }
        dmWebServer::Send(request, data, response->m_Size);
    }

    static void RequestSendJson(RequestContext* ctx, int status_code, StringBuffer* response)
    {
        SendResponse(ctx->m_Request, status_code, "application/json", response);
        StringBufferFree(response);
    }

    static void RequestSendError(RequestContext* ctx, int status_code, const char* code, const char* message)
    {
        StringBuffer response;
        StringBufferInit(&response);
        StringBufferAppend(&response, "{\"ok\":false,\"error\":{\"status\":");
        AppendNumber(&response, (double)status_code);
        StringBufferAppend(&response, ",\"code\":");
        AppendJsonString(&response, code);
        StringBufferAppend(&response, ",\"message\":");
        AppendJsonString(&response, message);
        StringBufferAppend(&response, "}}\n");
        RequestSendJson(ctx, status_code, &response);
    }

    static char* ReadRequestBody(dmWebServer::Request* request)
    {
        char* body = (char*)malloc((size_t)request->m_ContentLength + 1);
        if (!body)
        {
            return 0;
        }
        uint32_t remaining = request->m_ContentLength;
        uint32_t offset = 0;
        while (remaining > 0)
        {
            uint32_t received = 0;
            dmWebServer::Result result = dmWebServer::Receive(request, body + offset, remaining, &received);
            if (result != dmWebServer::RESULT_OK || received == 0)
            {
                free(body);
                return 0;
            }
            remaining -= received;
            offset += received;
        }
        body[offset] = 0;
        return body;
    }

    static void DiscardRequestBody(dmWebServer::Request* request)
    {
        char buffer[512];
        uint32_t remaining = request->m_ContentLength;
        while (remaining > 0)
        {
            uint32_t received = 0;
            uint32_t requested = remaining < sizeof(buffer) ? remaining : (uint32_t)sizeof(buffer);
            if (dmWebServer::Receive(request, buffer, requested, &received) != dmWebServer::RESULT_OK || received == 0)
            {
                return;
            }
            remaining -= received;
        }
    }

    static void InitRequestContext(RequestContext* ctx, dmWebServer::Request* request)
    {
        memset(ctx, 0, sizeof(*ctx));
        ctx->m_Request = request;
        ParseResource(request ? request->m_Resource : 0, &ctx->m_Path, &ctx->m_Query);
        if (StartsWith(ctx->m_Path, API_PREFIX))
        {
            const char* route = ctx->m_Path + strlen(API_PREFIX);
            SetString(&ctx->m_Route, route[0] ? route : "/");
        }
    }

    static void FreeRequestContext(RequestContext* ctx)
    {
        FreeString(&ctx->m_Path);
        FreeString(&ctx->m_Route);
        FreeQueryParams(&ctx->m_Query);
        FreeQueryParams(&ctx->m_JsonFields);
    }

    static bool RequestHasApiPrefix(const RequestContext* ctx)
    {
        return StartsWith(ctx->m_Path, API_PREFIX);
    }

    static bool RequestIsMethod(const RequestContext* ctx, const char* method)
    {
        return ctx->m_Request->m_Method && strcmp(ctx->m_Request->m_Method, method) == 0;
    }

    static bool RequestGetFloatParam(const RequestContext* ctx, const char* key, float* value)
    {
        return GetFloatParam(&ctx->m_Query, key, value);
    }

    static bool RequestHasParam(const RequestContext* ctx, const char* key)
    {
        return GetParam(&ctx->m_Query, key) != 0 || GetParam(&ctx->m_JsonFields, key) != 0;
    }

    static bool RequestGetBoolParam(const RequestContext* ctx, const char* key, bool* value)
    {
        return GetBoolParam(&ctx->m_Query, key, value);
    }

    static bool RequestGetUIntParam(const RequestContext* ctx, const char* key, uint32_t* value, uint32_t max_value = 0xffffffffU)
    {
        const char* text = GetParam(&ctx->m_Query, key);
        if (IsEmpty(text))
        {
            return false;
        }
        if (!isdigit((unsigned char)text[0]))
        {
            return false;
        }

        char* end = 0;
        errno = 0;
        unsigned long parsed = strtoul(text, &end, 10);
        if (errno == ERANGE || !end || *end != 0 || parsed == 0 || parsed > (unsigned long)max_value)
        {
            return false;
        }

        *value = (uint32_t)parsed;
        return true;
    }

    static bool RequestGetUIntParamAllowZero(const RequestContext* ctx, const char* key, uint32_t* value, uint32_t max_value = 0xffffffffU)
    {
        const char* text = GetParam(&ctx->m_Query, key);
        if (IsEmpty(text) || !isdigit((unsigned char)text[0]))
        {
            return false;
        }
        char* end = 0;
        errno = 0;
        unsigned long parsed = strtoul(text, &end, 10);
        if (errno == ERANGE || !end || *end != 0 || parsed > (unsigned long)max_value)
        {
            return false;
        }
        *value = (uint32_t)parsed;
        return true;
    }

    static bool RequestGetUInt64Param(const RequestContext* ctx, const char* key, uint64_t* value,
                                      uint64_t max_value = 0xffffffffffffffffULL, bool allow_zero = true)
    {
        const char* text = GetParam(&ctx->m_Query, key);
        if (IsEmpty(text) || !isdigit((unsigned char)text[0]))
        {
            return false;
        }
        char* end = 0;
        errno = 0;
        unsigned long long parsed = strtoull(text, &end, 10);
        if (errno == ERANGE || !end || *end != 0 || parsed > max_value || (!allow_zero && parsed == 0))
        {
            return false;
        }
        *value = (uint64_t)parsed;
        return true;
    }

    static const char* RequestGetParam(const RequestContext* ctx, const char* key)
    {
        return GetParam(&ctx->m_Query, key);
    }

    static bool IncludeHasToken(const char* include, const char* token)
    {
        if (!include)
        {
            return false;
        }

        uint32_t token_len = (uint32_t)strlen(token);
        const char* start = include;
        while (*start)
        {
            const char* end = strchr(start, ',');
            if (!end)
            {
                end = start + strlen(start);
            }

            const char* first = start;
            while (first < end && isspace((unsigned char)*first))
            {
                ++first;
            }

            const char* last = end;
            while (last > first && isspace((unsigned char)*(last - 1)))
            {
                --last;
            }

            if ((uint32_t)(last - first) == token_len && strncmp(first, token, token_len) == 0)
            {
                return true;
            }

            if (*end == 0)
            {
                break;
            }
            start = end + 1;
        }
        return false;
    }

    static IncludeOptions ParseInclude(const RequestContext* ctx, bool default_properties, bool default_children)
    {
        IncludeOptions options;
        options.m_Bounds = true;
        options.m_Properties = default_properties;
        options.m_Children = default_children;

        const char* include = RequestGetParam(ctx, "include");
        if (IsEmpty(include))
        {
            return options;
        }

        bool all = IncludeHasToken(include, "all");
        options.m_Bounds = all || IncludeHasToken(include, "bounds");
        options.m_Properties = all || IncludeHasToken(include, "properties");
        options.m_Children = all || IncludeHasToken(include, "children");
        return options;
    }

    static void RefreshSnapshotForRequest()
    {
        if (g_AutomationBridge.m_Snapshot.m_Sequence == 0 || g_AutomationBridge.m_SnapshotFrame != g_AutomationBridge.m_Frame)
        {
            UpdateSnapshot();
            g_AutomationBridge.m_SnapshotFrame = g_AutomationBridge.m_Frame;
        }
    }

    static bool ValidateExpectedScene(RequestContext* ctx)
    {
        const char* expected_text = RequestGetParam(ctx, "expected_scene_sequence");
        if (IsEmpty(expected_text))
        {
            return true;
        }
        uint64_t expected = 0;
        if (!RequestGetUInt64Param(ctx, "expected_scene_sequence", &expected))
        {
            RequestSendError(ctx, 400, "bad_request", "expected_scene_sequence must be an unsigned integer");
            return false;
        }
        if (expected != g_AutomationBridge.m_Snapshot.m_Sequence)
        {
            RequestSendError(ctx, 409, "stale_scene", "scene sequence changed before input target resolution; refresh the scene and target");
            return false;
        }
        return true;
    }

    static bool ValidateExpectedLogicalIdentity(RequestContext* ctx, const char* id, const char* parameter)
    {
        const char* expected = RequestGetParam(ctx, parameter);
        if (IsEmpty(expected))
        {
            return true;
        }
        const Node* node = FindNodeById(id);
        if (!node || IsEmpty(node->m_LogicalId) || !StringsEqual(expected, node->m_LogicalId))
        {
            RequestSendError(ctx, 409, "stale_element", "element id now resolves to a different runtime instance; re-query the element before sending input");
            return false;
        }
        return true;
    }

    static bool HasSceneCapability()
    {
        return g_AutomationBridge.m_Register != 0;
    }

    static bool HasScreenCapability()
    {
        return g_AutomationBridge.m_GraphicsContext && dmGraphics::GetInstalledAdapterFamily() != dmGraphics::ADAPTER_FAMILY_NULL;
    }

    static bool HasInputCapability()
    {
        return g_AutomationBridge.m_HidContext != 0;
    }

    static void AppendCapability(StringBuffer* names, StringBuffer* versions, bool* first, const char* name, const char* version = "1")
    {
        if (!*first)
        {
            StringBufferAppend(names, ",");
            StringBufferAppend(versions, ",");
        }
        AppendJsonString(names, name);
        AppendJsonString(versions, name);
        StringBufferAppend(versions, ":");
        AppendJsonString(versions, version);
        *first = false;
    }

    static void AppendCapabilitiesJson(StringBuffer* out)
    {
        StringBuffer names;
        StringBuffer versions;
        StringBufferInit(&names);
        StringBufferInit(&versions);
        bool first = true;
        AppendCapability(&names, &versions, &first, "runtime.health");
        AppendCapability(&names, &versions, &first, "runtime.lifecycle");
        AppendCapability(&names, &versions, &first, "diagnostics.metadata");
        if (HasSceneCapability())
        {
            AppendCapability(&names, &versions, &first, "scene");
            AppendCapability(&names, &versions, &first, "elements");
            AppendCapability(&names, &versions, &first, "element");
            AppendCapability(&names, &versions, &first, "scene.pagination");
            AppendCapability(&names, &versions, &first, "scene.identity");
            AppendCapability(&names, &versions, &first, "observations");
        }
        if (HasScreenCapability())
        {
            AppendCapability(&names, &versions, &first, "screen");
            AppendCapability(&names, &versions, &first, "coordinates.convert");
            if (EngineAccessCanSetWindowSize())
            {
                AppendCapability(&names, &versions, &first, "screen.resize");
            }
        }
        if (HasInputCapability())
        {
            AppendCapability(&names, &versions, &first, "input.click");
            AppendCapability(&names, &versions, &first, "input.drag");
            AppendCapability(&names, &versions, &first, "input.drag_path");
            AppendCapability(&names, &versions, &first, "input.pointer");
            AppendCapability(&names, &versions, &first, "input.key", "2");
            AppendCapability(&names, &versions, &first, "input.modifiers");
            AppendCapability(&names, &versions, &first, "input.receipts");
            AppendCapability(&names, &versions, &first, "input.queue");
            AppendCapability(&names, &versions, &first, "input.controller");
            if (IsInputDeviceSupported(INPUT_DEVICE_MOUSE))
                AppendCapability(&names, &versions, &first, "input.device.mouse");
            if (IsInputDeviceSupported(INPUT_DEVICE_TOUCH))
            {
                AppendCapability(&names, &versions, &first, "input.device.touch");
            }
        }
        AppendCapability(&names, &versions, &first, "events.cursor");
        AppendCapability(&names, &versions, &first, "timeline.markers");
        AppendCapability(&names, &versions, &first, "frame.wait");
        if (g_AutomationBridge.m_ApplicationApiEnabled)
        {
            AppendCapability(&names, &versions, &first, "application.events");
            AppendCapability(&names, &versions, &first, "application.state");
            AppendCapability(&names, &versions, &first, "application.commands");
            AppendCapability(&names, &versions, &first, "application.catalog");
            AppendCapability(&names, &versions, &first, "application.acknowledgements");
            AppendCapability(&names, &versions, &first, "application.annotations");
        }
        if (IsScreenshotSupported())
        {
            AppendCapability(&names, &versions, &first, "screenshot");
            AppendCapability(&names, &versions, &first, "artifacts");
        }
        if (IsMetalCaptureSupported())
        {
            AppendCapability(&names, &versions, &first, "metal.capture");
        }
        if (IsNativeRecordingSupported())
        {
            AppendCapability(&names, &versions, &first, "recording.video");
            if (IsNativeRecordingAudioSupported())
            {
                AppendCapability(&names, &versions, &first, "recording.application_audio");
            }
        }
        StringBufferAppend(out, "{");
        StringBufferAppend(out, versions.m_Data ? versions.m_Data : "");
        StringBufferAppend(out, "}");
        StringBufferFree(&names);
        StringBufferFree(&versions);
    }

    static void AppendTimestamp(StringBuffer* out, uint64_t value)
    {
        char number[32];
        dmSnPrintf(number, sizeof(number), "%llu", (unsigned long long)value);
        StringBufferAppend(out, number);
    }

    static void AppendLifecycleStage(StringBuffer* out, const char* stage, uint64_t timestamp, bool* first)
    {
        if (!timestamp)
        {
            return;
        }
        if (!*first)
        {
            StringBufferAppend(out, ",");
        }
        StringBufferAppend(out, "{\"stage\":");
        AppendJsonString(out, stage);
        StringBufferAppend(out, ",\"state\":\"completed\",\"monotonic_us\":");
        AppendTimestamp(out, timestamp);
        StringBufferAppend(out, "}");
        *first = false;
    }

    static void UpdateLifecycleForRequest()
    {
        uint64_t now = dmTime::GetMonotonicTime();
        if (!g_AutomationBridge.m_FirstHealthMonotonicTime)
        {
            g_AutomationBridge.m_FirstHealthMonotonicTime = now;
        }
        if (!g_AutomationBridge.m_SceneReadyMonotonicTime &&
            g_AutomationBridge.m_Snapshot.m_Root >= 0 &&
            (uint32_t)g_AutomationBridge.m_Snapshot.m_Root < g_AutomationBridge.m_Snapshot.m_Nodes.Size())
        {
            g_AutomationBridge.m_SceneReadyMonotonicTime = now;
        }
    }

    static void AppendLifecycleJson(StringBuffer* out)
    {
        StringBufferAppend(out, "{\"current_stage\":");
        AppendJsonString(out, g_AutomationBridge.m_SceneReadyMonotonicTime ? "initial_scene_ready" : "bridge_healthy");
        StringBufferAppend(out, ",\"events\":[");
        bool first = true;
        AppendLifecycleStage(out, "engine_started", g_AutomationBridge.m_StartMonotonicTime, &first);
        AppendLifecycleStage(out, "new_engine_registered", g_AutomationBridge.m_RegisteredMonotonicTime, &first);
        AppendLifecycleStage(out, "bridge_healthy", g_AutomationBridge.m_FirstHealthMonotonicTime, &first);
        AppendLifecycleStage(out, "initial_scene_ready", g_AutomationBridge.m_SceneReadyMonotonicTime, &first);
        StringBufferAppend(out, "]}");
    }

    static void HandleHealth(RequestContext* ctx)
    {
        UpdateLifecycleForRequest();
        RefreshScreenInfo(&g_AutomationBridge.m_Snapshot);

        StringBuffer response;
        StringBufferInit(&response);
        StringBufferAppend(&response, "{\"ok\":true,\"data\":{\"version\":");
        AppendJsonString(&response, API_VERSION);
        dmSys::EngineInfo info;
        dmSys::GetEngineInfo(&info);
        StringBufferAppend(&response, ",\"engine\":{\"version\":");
        AppendJsonString(&response, info.m_Version);
        StringBufferAppend(&response, ",\"commit\":");
        AppendJsonString(&response, info.m_VersionSHA1);
        StringBufferAppend(&response, "}");
        StringBufferAppend(&response, ",\"profiler\":");
        uint16_t profiler_port = dmProfilerRemotery::GetConnectionPort();
        if (profiler_port)
        {
            StringBufferAppend(&response, "{\"backend\":\"remotery\",\"port\":");
            AppendNumber(&response, profiler_port);
            StringBufferAppend(&response, ",\"path\":\"/rmt\"}");
        }
        else StringBufferAppend(&response, "null");
        StringBufferAppend(&response, ",\"debug\":true,\"platform\":");
#if defined(DM_PLATFORM_MACOS)
        AppendJsonString(&response, "macos");
#elif defined(DM_PLATFORM_WINDOWS)
        AppendJsonString(&response, "windows");
#elif defined(DM_PLATFORM_LINUX)
        AppendJsonString(&response, "linux");
#elif defined(ANDROID)
        AppendJsonString(&response, "android");
#elif defined(DM_PLATFORM_IOS)
        AppendJsonString(&response, "ios");
#else
        AppendJsonString(&response, "unknown");
#endif
        StringBufferAppend(&response, ",\"backend\":{\"headless\":");
        StringBufferAppend(&response, dmGraphics::GetInstalledAdapterFamily() == dmGraphics::ADAPTER_FAMILY_NULL ? "true" : "false");
        StringBufferAppend(&response, ",\"graphics\":");
        StringBufferAppend(&response, HasScreenCapability() ? "true" : "false");
        StringBufferAppend(&response, ",\"hid\":");
        StringBufferAppend(&response, HasInputCapability() ? "true" : "false");
        StringBufferAppend(&response, "},\"capabilities\":");
        AppendCapabilitiesJson(&response);
        StringBufferAppend(&response, ",\"identity\":{\"engine_instance_id\":");
        AppendJsonString(&response, g_AutomationBridge.m_EngineInstanceId);
        StringBufferAppend(&response, ",\"engine_instance_scope\":\"engine_initialization\"");
        StringBufferAppend(&response, ",\"process_id\":");
        if (g_AutomationBridge.m_ProcessId >= 0)
        {
            AppendNumber(&response, (double)g_AutomationBridge.m_ProcessId);
        }
        else
        {
            StringBufferAppend(&response, "null");
        }
        StringBufferAppend(&response, ",\"start_wall_time_us\":");
        AppendTimestamp(&response, g_AutomationBridge.m_StartWallTime);
        StringBufferAppend(&response, ",\"start_monotonic_time_us\":");
        AppendTimestamp(&response, g_AutomationBridge.m_StartMonotonicTime);
        StringBufferAppend(&response, ",\"project_identity\":");
        AppendJsonString(&response, g_AutomationBridge.m_ProjectIdentity);
        StringBufferAppend(&response, ",\"build_identity\":");
        AppendJsonString(&response, g_AutomationBridge.m_BuildIdentity);
        StringBufferAppend(&response, ",\"build_identity_kind\":\"project_configuration_fingerprint\"");
        StringBufferAppend(&response, "},\"lifecycle\":");
        AppendLifecycleJson(&response);
        StringBufferAppend(&response, ",\"screen\":");
        AppendScreenJson(&response, &g_AutomationBridge.m_Snapshot);
        StringBufferAppend(&response, ",\"scene_sequence\":");
        AppendNumber(&response, (double)g_AutomationBridge.m_Snapshot.m_Sequence);
        StringBufferAppend(&response, ",\"engine_frame\":");
        AppendNumber(&response, (double)g_AutomationBridge.m_Frame);
        StringBufferAppend(&response, ",\"engine_instance_id\":");
        AppendJsonString(&response, g_AutomationBridge.m_EngineInstanceId);
        StringBufferAppend(&response, ",\"input\":{\"default_device\":");
        AppendJsonString(&response, InputDeviceName(g_AutomationBridge.m_DefaultInputDevice));
        StringBufferAppend(&response, ",\"default_visualize\":");
        StringBufferAppend(&response, g_AutomationBridge.m_DefaultInputVisualize ? "true" : "false");
        StringBufferAppend(&response, ",\"devices\":{\"auto\":true,\"mouse\":");
        StringBufferAppend(&response, IsInputDeviceSupported(INPUT_DEVICE_MOUSE) ? "true" : "false");
        StringBufferAppend(&response, ",\"touch\":");
        StringBufferAppend(&response, IsInputDeviceSupported(INPUT_DEVICE_TOUCH) ? "true" : "false");
        StringBufferAppend(&response, "}}");
        StringBufferAppend(&response, ",\"application_api_enabled\":");
        StringBufferAppend(&response, g_AutomationBridge.m_ApplicationApiEnabled ? "true" : "false");
        StringBufferAppend(&response, "}}\n");
        RequestSendJson(ctx, 200, &response);
    }

    static void HandleLifecycle(RequestContext* ctx)
    {
        UpdateLifecycleForRequest();
        StringBuffer response;
        StringBufferInit(&response);
        StringBufferAppend(&response, "{\"ok\":true,\"data\":");
        AppendLifecycleJson(&response);
        StringBufferAppend(&response, "}\n");
        RequestSendJson(ctx, 200, &response);
    }

    static void HandleScreen(RequestContext* ctx)
    {
        if (!HasScreenCapability())
        {
            RequestSendError(ctx, 501, "unsupported_capability", "screen is unavailable because the runtime has no graphics context");
            return;
        }
        RefreshSnapshotForRequest();

        StringBuffer response;
        StringBufferInit(&response);
        StringBufferAppend(&response, "{\"ok\":true,\"data\":");
        AppendScreenJson(&response, &g_AutomationBridge.m_Snapshot);
        response.m_Size--;
        response.m_Data[response.m_Size] = 0;
        StringBufferAppend(&response, ",\"scene_sequence\":");
        AppendNumber(&response, (double)g_AutomationBridge.m_Snapshot.m_Sequence);
        StringBufferAppend(&response, ",\"engine_frame\":");
        AppendNumber(&response, (double)g_AutomationBridge.m_Frame);
        StringBufferAppend(&response, "}}\n");
        RequestSendJson(ctx, 200, &response);
    }

    static void HandleScreenPut(RequestContext* ctx)
    {
        if (!HasScreenCapability() || !EngineAccessCanSetWindowSize())
        {
            RequestSendError(ctx, 501, "unsupported_capability", "screen.resize is unavailable on this platform/backend");
            return;
        }
        uint32_t width = 0;
        uint32_t height = 0;
        if (!RequestGetUIntParam(ctx, "width", &width, MAX_SCREEN_DIMENSION) ||
            !RequestGetUIntParam(ctx, "height", &height, MAX_SCREEN_DIMENSION))
        {
            RequestSendError(ctx, 400, "bad_request", "provide positive integer width and height no greater than 2147483647");
            return;
        }

        if (!EngineAccessCanSetWindowSize())
        {
            RequestSendError(ctx, 409, "screen_resize_unsupported", "the platform window cannot be resized");
            return;
        }

        RefreshSnapshotForRequest();
        bool already_correct = g_AutomationBridge.m_Snapshot.m_WindowWidth == width &&
                               g_AutomationBridge.m_Snapshot.m_WindowHeight == height;
        bool observed = already_correct || EngineAccessSetWindowSize(width, height);

        UpdateSnapshot();
        g_AutomationBridge.m_SnapshotFrame = g_AutomationBridge.m_Frame;

        StringBuffer response;
        StringBufferInit(&response);
        StringBufferAppend(&response, "{\"ok\":true,\"data\":{\"requested\":{\"width\":");
        AppendNumber(&response, width);
        StringBufferAppend(&response, ",\"height\":");
        AppendNumber(&response, height);
        StringBufferAppend(&response, "},\"outcome\":");
        AppendJsonString(&response, already_correct ? "already_correct" : (observed ? "resized" : "requested_not_observed"));
        StringBufferAppend(&response, ",\"window_matches\":");
        StringBufferAppend(&response, g_AutomationBridge.m_Snapshot.m_WindowWidth == width && g_AutomationBridge.m_Snapshot.m_WindowHeight == height ? "true" : "false");
        StringBufferAppend(&response, ",\"backbuffer_matches\":");
        StringBufferAppend(&response, g_AutomationBridge.m_Snapshot.m_BackbufferWidth == width && g_AutomationBridge.m_Snapshot.m_BackbufferHeight == height ? "true" : "false");
        StringBufferAppend(&response, ",\"screen\":");
        AppendScreenJson(&response, &g_AutomationBridge.m_Snapshot);
        StringBufferAppend(&response, ",\"scene_sequence\":");
        AppendNumber(&response, (double)g_AutomationBridge.m_Snapshot.m_Sequence);
        StringBufferAppend(&response, ",\"engine_frame\":");
        AppendNumber(&response, (double)g_AutomationBridge.m_Frame);
        StringBufferAppend(&response, "}}\n");
        RequestSendJson(ctx, 200, &response);
    }

    static void HandleScene(RequestContext* ctx)
    {
        if (!HasSceneCapability())
        {
            RequestSendError(ctx, 501, "unsupported_capability", "scene inspection is unavailable because the runtime has no game-object register");
            return;
        }
        RefreshSnapshotForRequest();

        IncludeOptions include = ParseInclude(ctx, false, true);
        bool visible_only = false;
        RequestGetBoolParam(ctx, "visible", &visible_only);

        const Snapshot* snapshot = &g_AutomationBridge.m_Snapshot;
        if (snapshot->m_Root < 0 || (uint32_t)snapshot->m_Root >= snapshot->m_Nodes.Size())
        {
            RequestSendError(ctx, 404, "scene_unavailable", "scene graph is not available");
            return;
        }

        StringBuffer response;
        StringBufferInit(&response);
        StringBufferAppend(&response, "{\"ok\":true,\"data\":{\"count\":");
        AppendNumber(&response, (double)snapshot->m_Nodes.Size());
        StringBufferAppend(&response, ",\"visible_filter\":");
        StringBufferAppend(&response, visible_only ? "true" : "false");
        StringBufferAppend(&response, ",\"screen\":");
        AppendScreenJson(&response, snapshot);
        StringBufferAppend(&response, ",\"scene_sequence\":");
        AppendNumber(&response, (double)snapshot->m_Sequence);
        StringBufferAppend(&response, ",\"engine_frame\":");
        AppendNumber(&response, (double)g_AutomationBridge.m_Frame);
        StringBufferAppend(&response, ",\"root\":");
        AppendNodeJson(&response, snapshot, &snapshot->m_Nodes.Begin()[snapshot->m_Root], &include, true, visible_only);
        StringBufferAppend(&response, "}}\n");
        RequestSendJson(ctx, 200, &response);
    }

    static bool NodeMatchesFilters(const Node* node, const RequestContext* ctx, bool ignore_state_filters = false)
    {
        bool case_sensitive = false;
        RequestGetBoolParam(ctx, "case_sensitive", &case_sensitive);
        const char* value = RequestGetParam(ctx, "id");
        if (value && !StringsEqual(node->m_Id, value))
        {
            return false;
        }
        value = RequestGetParam(ctx, "type");
        if (value && !(case_sensitive ? ContainsCaseSensitive(node->m_Type, value) : ContainsCaseInsensitive(node->m_Type, value)))
        {
            return false;
        }
        value = RequestGetParam(ctx, "type_exact");
        if (value && !StringsEqual(node->m_Type, value))
        {
            return false;
        }
        value = RequestGetParam(ctx, "name");
        if (value && !(case_sensitive ? ContainsCaseSensitive(node->m_Name, value) : ContainsCaseInsensitive(node->m_Name, value)))
        {
            return false;
        }
        value = RequestGetParam(ctx, "name_exact");
        if (value && !StringsEqual(node->m_Name, value))
        {
            return false;
        }
        value = RequestGetParam(ctx, "text");
        if (value && !(case_sensitive ? ContainsCaseSensitive(node->m_Text, value) : ContainsCaseInsensitive(node->m_Text, value)))
        {
            return false;
        }
        value = RequestGetParam(ctx, "text_exact");
        if (value && !StringsEqual(node->m_Text, value))
        {
            return false;
        }
        value = RequestGetParam(ctx, "url");
        if (value && !(case_sensitive ? ContainsCaseSensitive(node->m_Url, value) : ContainsCaseInsensitive(node->m_Url, value)))
        {
            return false;
        }
        value = RequestGetParam(ctx, "url_exact");
        if (value && !StringsEqual(node->m_Url, value))
        {
            return false;
        }
        value = RequestGetParam(ctx, "automation_id");
        if (value && !StringsEqual(node->m_AutomationId, value))
        {
            return false;
        }
        value = RequestGetParam(ctx, "localization_key");
        if (value && !StringsEqual(node->m_LocalizationKey, value))
        {
            return false;
        }
        value = RequestGetParam(ctx, "role");
        if (value && !StringsEqual(node->m_Role, value))
        {
            return false;
        }
        value = RequestGetParam(ctx, "path");
        if (value && !StringsEqual(node->m_Path, value))
        {
            return false;
        }
        value = RequestGetParam(ctx, "kind");
        if (value && !StringsEqual(node->m_Kind, value))
        {
            return false;
        }
        value = RequestGetParam(ctx, "instance_id");
        if (value && !StringsEqual(node->m_InstanceId, value))
        {
            return false;
        }
        value = RequestGetParam(ctx, "logical_id");
        if (value && !StringsEqual(node->m_LogicalId, value))
        {
            return false;
        }
        if (!ignore_state_filters)
        {
            bool visible = false;
            if (RequestGetBoolParam(ctx, "visible", &visible) && node->m_Visible != visible)
            {
                return false;
            }
            bool enabled = false;
            if (RequestGetBoolParam(ctx, "enabled", &enabled) && node->m_Enabled != enabled)
            {
                return false;
            }
            bool has_bounds = false;
            if (RequestGetBoolParam(ctx, "has_bounds", &has_bounds) && node->m_Bounds.m_Valid != has_bounds)
            {
                return false;
            }
            bool visible_and_enabled = false;
            if (RequestGetBoolParam(ctx, "visible_and_enabled", &visible_and_enabled) &&
                (node->m_Visible && node->m_Enabled) != visible_and_enabled)
            {
                return false;
            }
        }
        return true;
    }

    static void AppendActiveCollectionsJson(StringBuffer* response, const Snapshot* snapshot)
    {
        StringBufferAppend(response, "[");
        uint32_t emitted = 0;
        for (uint32_t i = 0; i < snapshot->m_Nodes.Size(); ++i)
        {
            const Node* node = &snapshot->m_Nodes.Begin()[i];
            if (!StringsEqual(node->m_Kind, "collection") || !node->m_Visible)
            {
                continue;
            }
            if (emitted++)
            {
                StringBufferAppendChar(response, ',');
            }
            AppendJsonString(response, node->m_Name);
        }
        StringBufferAppend(response, "]");
    }

    struct Observation
    {
        char m_Id[96];
        uint64_t m_Frame;
        uint64_t m_RenderFrame;
        uint64_t m_Sequence;
        uint64_t m_EventCursor;
        uint64_t m_Deadline;
        uint64_t m_CompletedOrder;
        uint64_t m_Screenshot;
        ScreenshotCapture m_CapturedScreenshot;
        bool m_WantsScreenshot;
        bool m_Prepared;
        bool m_Complete;
        char m_Failure[64];
        dmArray<char*> m_Elements;
        dmArray<QueryParam> m_Filters;
        char* m_State;
        char* m_Screen;
        char* m_Collections;
    };
    static dmArray<Observation*> g_Observations;
    static uint64_t g_ObservationId;
    static uint64_t g_ObservationCompletedOrder;

    static void DeleteObservation(Observation* observation)
    {
        for (uint32_t i = 0; i < observation->m_Elements.Size(); ++i) free(observation->m_Elements[i]);
        FreeQueryParams(&observation->m_Filters);
        free(observation->m_State);
        free(observation->m_Screen);
        free(observation->m_Collections);
        delete observation;
    }

    void FinalizeObservations()
    {
        for (uint32_t i = 0; i < g_Observations.Size(); ++i) DeleteObservation(g_Observations[i]);
        g_Observations.SetSize(0);
    }

    static void EvictObservations()
    {
        uint32_t completed = 0;
        for (uint32_t i = 0; i < g_Observations.Size(); ++i)
        {
            Observation* observation = g_Observations[i];
            if (!observation->m_Complete) continue;
            ++completed;
            if (!observation->m_CompletedOrder)
                observation->m_CompletedOrder = ++g_ObservationCompletedOrder;
        }
        while (completed > 16)
        {
            uint32_t oldest = UINT32_MAX;
            for (uint32_t i = 0; i < g_Observations.Size(); ++i)
            {
                if (!g_Observations[i]->m_Complete) continue;
                if (oldest == UINT32_MAX || g_Observations[i]->m_CompletedOrder < g_Observations[oldest]->m_CompletedOrder)
                    oldest = i;
            }
            DeleteObservation(g_Observations[oldest]);
            memmove(g_Observations.Begin() + oldest, g_Observations.Begin() + oldest + 1,
                    (g_Observations.Size() - oldest - 1) * sizeof(Observation*));
            g_Observations.Pop();
            --completed;
        }
    }

    static Observation* NewObservation(RequestContext* ctx, bool screenshot, uint64_t frame)
    {
        uint32_t pending = 0;
        for (uint32_t i = 0; i < g_Observations.Size(); ++i) pending += !g_Observations[i]->m_Complete;
        if (pending >= 16) return 0;
        Observation* observation = new Observation;
        memset(observation, 0, sizeof(*observation));
        observation->m_WantsScreenshot = screenshot;
        observation->m_Frame = frame;
        observation->m_Deadline = dmTime::GetMonotonicTime() + 30000000;
        dmSnPrintf(observation->m_Id, sizeof(observation->m_Id), "%s-o%llu", g_AutomationBridge.m_EngineInstanceId, (unsigned long long)++g_ObservationId);
        for (uint32_t i = 0; i < ctx->m_Query.Size(); ++i)
            JsonPushParam(&observation->m_Filters, ctx->m_Query.Begin()[i].m_Key, DuplicateString(ctx->m_Query.Begin()[i].m_Value));
        if (g_Observations.Full()) g_Observations.OffsetCapacity(16);
        g_Observations.Push(observation);
        return observation;
    }

    static Observation* FindObservation(const char* id)
    {
        for (uint32_t i = 0; i < g_Observations.Size(); ++i)
            if (StringsEqual(g_Observations[i]->m_Id, id)) return g_Observations[i];
        return 0;
    }

    static bool SelectedElement(const char* ids, const char* id)
    {
        if (!ids) return true;
        if (*ids++ != '[') return false;
        JsonSkipWhitespace(&ids);
        while (*ids && *ids != ']')
        {
            char* candidate = 0;
            if (!JsonParseString(&ids, &candidate)) return false;
            bool found = StringsEqual(candidate, id);
            free(candidate);
            if (found) return true;
            JsonSkipWhitespace(&ids);
            if (*ids++ != ',') return false;
            JsonSkipWhitespace(&ids);
        }
        return false;
    }

    static void PrepareObservation(Observation* observation, bool rendered)
    {
        RefreshSnapshotForRequest();
        const Snapshot* snapshot = &g_AutomationBridge.m_Snapshot;
        observation->m_Frame = snapshot->m_Frame;
        observation->m_RenderFrame = rendered ? snapshot->m_Frame : 0;
        observation->m_Sequence = snapshot->m_Sequence;
        observation->m_EventCursor = GetEventNextCursor();
        RequestContext filters;
        memset(&filters, 0, sizeof(filters));
        filters.m_Query.Swap(observation->m_Filters);
        IncludeOptions include = ParseInclude(&filters, false, false);
        const char* ids = GetParam(&filters.m_Query, "ids");
        for (uint32_t i = 0; i < snapshot->m_Nodes.Size(); ++i)
        {
            const Node* node = &snapshot->m_Nodes.Begin()[i];
            if (!NodeMatchesFilters(node, &filters) || !SelectedElement(ids, node->m_Id)) continue;
            StringBuffer json;
            StringBufferInit(&json);
            AppendNodeJson(&json, snapshot, node, &include, false, false);
            if (observation->m_Elements.Full()) observation->m_Elements.OffsetCapacity(32);
            observation->m_Elements.Push(StringBufferDetach(&json));
        }
        StringBuffer json;
        StringBufferInit(&json);
        AppendPublishedStatesJson(&json, 0, 0);
        observation->m_State = StringBufferDetach(&json);
        StringBufferInit(&json);
        AppendScreenJson(&json, snapshot);
        observation->m_Screen = StringBufferDetach(&json);
        StringBufferInit(&json);
        AppendActiveCollectionsJson(&json, snapshot);
        observation->m_Collections = StringBufferDetach(&json);
        filters.m_Query.Swap(observation->m_Filters);
        observation->m_Prepared = true;
        observation->m_Complete = !observation->m_WantsScreenshot;
    }

    void PrepareObservations(bool rendered)
    {
        uint64_t capture_id = 0;
        for (uint32_t i = 0; i < g_Observations.Size(); ++i)
        {
            Observation* observation = g_Observations[i];
            if (observation->m_Prepared || observation->m_Complete || observation->m_Frame > g_AutomationBridge.m_Frame) continue;
            if (observation->m_WantsScreenshot)
            {
                if (!rendered || observation->m_Frame != g_AutomationBridge.m_Frame)
                {
                    dmStrlCpy(observation->m_Failure, "requested_frame_unavailable", sizeof(observation->m_Failure));
                    observation->m_Complete = true;
                    continue;
                }
                if (!capture_id)
                {
                    ScreenshotCapture capture;
                    if (!ScheduleScreenshot(0, &capture))
                    {
                        dmStrlCpy(observation->m_Failure, "capture_busy", sizeof(observation->m_Failure));
                        observation->m_Complete = true;
                        continue;
                    }
                    capture_id = capture.m_Id;
                }
                observation->m_Screenshot = capture_id;
            }
            PrepareObservation(observation, rendered);
        }
        EvictObservations();
    }

    void CompleteObservations()
    {
        uint64_t now = dmTime::GetMonotonicTime();
        for (uint32_t i = 0; i < g_Observations.Size(); ++i)
        {
            Observation* observation = g_Observations[i];
            if (observation->m_Complete) continue;
            const ScreenshotCapture* capture = observation->m_Screenshot ? FindScreenshotCapture(observation->m_Screenshot) : 0;
            if (capture && capture->m_State == SCREENSHOT_COMPLETE && capture->m_Frame == observation->m_Frame)
            {
                observation->m_CapturedScreenshot = *capture;
                observation->m_Complete = true;
            }
            else if ((capture && capture->m_State == SCREENSHOT_FAILED) || now >= observation->m_Deadline)
            {
                observation->m_Complete = true;
                dmStrlCpy(observation->m_Failure, capture ? capture->m_Failure : "observation_expired", sizeof(observation->m_Failure));
            }
        }
        EvictObservations();
    }

    static void SendObservation(RequestContext* ctx, Observation* observation, bool elements_only, uint32_t offset)
    {
        uint32_t limit = 50;
        if (RequestHasParam(ctx, "limit") && !RequestGetUIntParamAllowZero(ctx, "limit", &limit, 500))
        { RequestSendError(ctx, 400, "bad_request", "limit must be between 0 and 500"); return; }
        StringBuffer response;
        StringBufferInit(&response);
        StringBufferAppend(&response, "{\"ok\":true,\"data\":{\"id\":"); AppendJsonString(&response, observation->m_Id);
        StringBufferAppend(&response, ",\"snapshot_id\":"); AppendJsonString(&response, observation->m_Id);
        StringBufferAppend(&response, ",\"runtime_id\":"); AppendJsonString(&response, g_AutomationBridge.m_EngineInstanceId);
        StringBufferAppend(&response, ",\"state\":"); AppendJsonString(&response, !observation->m_Complete ? "pending" : (observation->m_Failure[0] ? "failed" : "complete"));
        StringBufferAppend(&response, ",\"failure_reason\":");
        if (observation->m_Failure[0]) AppendJsonString(&response, observation->m_Failure); else StringBufferAppend(&response, "null");
        StringBufferAppend(&response, ",\"engine_frame\":"); AppendNumber(&response, (double)observation->m_Frame);
        StringBufferAppend(&response, ",\"simulation_frame\":"); AppendNumber(&response, (double)observation->m_Frame);
        StringBufferAppend(&response, ",\"render_frame\":");
        if (observation->m_RenderFrame) AppendNumber(&response, (double)observation->m_RenderFrame); else StringBufferAppend(&response, "null");
        StringBufferAppend(&response, ",\"scene_sequence\":"); AppendNumber(&response, (double)observation->m_Sequence);
        StringBufferAppend(&response, ",\"elements\":[");
        uint32_t end = offset <= observation->m_Elements.Size() ? offset + limit : observation->m_Elements.Size();
        if (end > observation->m_Elements.Size()) end = observation->m_Elements.Size();
        for (uint32_t i = offset; i < end; ++i)
        {
            if (i > offset) StringBufferAppendChar(&response, ',');
            StringBufferAppend(&response, observation->m_Elements[i]);
        }
        StringBufferAppend(&response, "],\"count\":"); AppendNumber(&response, end > offset ? end - offset : 0);
        StringBufferAppend(&response, ",\"matched\":"); AppendNumber(&response, observation->m_Elements.Size());
        StringBufferAppend(&response, ",\"total\":"); AppendNumber(&response, observation->m_Elements.Size());
        StringBufferAppend(&response, ",\"offset\":"); AppendNumber(&response, offset);
        StringBufferAppend(&response, ",\"next_cursor\":");
        if (end < observation->m_Elements.Size())
        {
            char cursor[128];
            dmSnPrintf(cursor, sizeof(cursor), "%s:%u", observation->m_Id, end);
            AppendJsonString(&response, cursor);
        }
        else StringBufferAppend(&response, "null");
        StringBufferAppend(&response, ",\"truncated\":"); StringBufferAppend(&response, end < observation->m_Elements.Size() ? "true" : "false");
        StringBufferAppend(&response, ",\"active_collections\":"); StringBufferAppend(&response, observation->m_Collections ? observation->m_Collections : "[]");
        if (!elements_only && observation->m_Prepared)
        {
            StringBufferAppend(&response, ",\"published_state\":"); StringBufferAppend(&response, observation->m_State);
            StringBufferAppend(&response, ",\"event_cursor\":"); AppendNumber(&response, (double)observation->m_EventCursor);
            StringBufferAppend(&response, ",\"screen\":"); StringBufferAppend(&response, observation->m_Screen);
            StringBufferAppend(&response, ",\"screenshot\":");
            const ScreenshotCapture* capture = observation->m_CapturedScreenshot.m_Id ? &observation->m_CapturedScreenshot : (observation->m_Screenshot ? FindScreenshotCapture(observation->m_Screenshot) : 0);
            if (capture) AppendScreenshotJson(&response, capture); else StringBufferAppend(&response, "null");
        }
        StringBufferAppend(&response, "}}");
        RequestSendJson(ctx, observation->m_Complete ? 200 : 202, &response);
    }

    static void HandleObservationPost(RequestContext* ctx)
    {
        bool screenshot = false;
        RequestGetBoolParam(ctx, "screenshot", &screenshot);
        if (screenshot && !IsScreenshotSupported()) { RequestSendError(ctx, 501, "screenshot_unsupported", "the current backend cannot capture images"); return; }
        uint64_t frame = g_AutomationBridge.m_Frame + 1;
        if (RequestHasParam(ctx, "frame") && (!RequestGetUInt64Param(ctx, "frame", &frame) || frame <= g_AutomationBridge.m_Frame || frame > g_AutomationBridge.m_Frame + 600))
        { RequestSendError(ctx, 400, "bad_request", "frame must be one of the next 600 simulation frames"); return; }
        Observation* observation = NewObservation(ctx, screenshot, frame);
        if (!observation) { RequestSendError(ctx, 429, "observation_queue_full", "16 observations are already pending"); return; }
        SendObservation(ctx, observation, false, 0);
    }

    static bool ParseSnapshotCursor(const char* cursor, Observation** observation, uint32_t* offset)
    {
        const char* separator = cursor ? strrchr(cursor, ':') : 0;
        if (!separator || !isdigit((unsigned char)separator[1])) return false;
        char* end = 0;
        unsigned long value = strtoul(separator + 1, &end, 10);
        if (*end || value > UINT32_MAX) return false;
        char* id = DuplicateStringN(cursor, separator - cursor);
        *observation = FindObservation(id);
        free(id);
        *offset = (uint32_t)value;
        return *observation != 0;
    }

    static void HandleObservationGet(RequestContext* ctx, const char* id)
    {
        Observation* observation = FindObservation(id);
        uint32_t offset = 0;
        if (RequestGetParam(ctx, "offset") && !RequestGetUIntParamAllowZero(ctx, "offset", &offset))
        { RequestSendError(ctx, 400, "bad_request", "offset must be an unsigned integer"); return; }
        const char* cursor = RequestGetParam(ctx, "cursor");
        if (cursor && (!ParseSnapshotCursor(cursor, &observation, &offset) || !StringsEqual(observation->m_Id, id))) observation = 0;
        if (!observation) { RequestSendError(ctx, 410, "stale_snapshot", "observation belongs to an earlier runtime or has been evicted"); return; }
        SendObservation(ctx, observation, false, offset);
    }

    static void HandleElements(RequestContext* ctx)
    {
        Observation* observation = 0;
        uint32_t offset = 0;
        if (RequestGetParam(ctx, "offset") && !RequestGetUIntParamAllowZero(ctx, "offset", &offset))
        { RequestSendError(ctx, 400, "bad_request", "offset must be an unsigned integer"); return; }
        const char* cursor = RequestGetParam(ctx, "cursor");
        if (cursor)
        {
            if (!ParseSnapshotCursor(cursor, &observation, &offset))
            { RequestSendError(ctx, 410, "stale_snapshot", "snapshot cursor was evicted or belongs to an earlier runtime"); return; }
        }
        else
        {
            if (!HasSceneCapability()) { RequestSendError(ctx, 501, "unsupported_capability", "scene inspection is unavailable"); return; }
            observation = NewObservation(ctx, false, g_AutomationBridge.m_Frame);
            if (!observation) { RequestSendError(ctx, 429, "observation_queue_full", "observation queue is full"); return; }
            PrepareObservation(observation, false);
            EvictObservations();
        }
        SendObservation(ctx, observation, true, offset);
    }

    static void HandleElement(RequestContext* ctx)
    {
        if (!HasSceneCapability())
        {
            RequestSendError(ctx, 501, "unsupported_capability", "element is unavailable because the runtime has no game-object register");
            return;
        }
        RefreshSnapshotForRequest();

        const char* id = RequestGetParam(ctx, "id");
        if (IsEmpty(id))
        {
            RequestSendError(ctx, 400, "bad_request", "missing element id");
            return;
        }

        const Node* node = FindNodeById(id);
        if (!node)
        {
            RequestSendError(ctx, 404, "not_found", "element id was not found");
            return;
        }

        IncludeOptions include = ParseInclude(ctx, true, false);
        StringBuffer response;
        StringBufferInit(&response);
        StringBufferAppend(&response, "{\"ok\":true,\"data\":{\"element\":");
        AppendNodeJson(&response, &g_AutomationBridge.m_Snapshot, node, &include, include.m_Children, false);
        StringBufferAppend(&response, ",\"scene_sequence\":");
        AppendNumber(&response, (double)g_AutomationBridge.m_Snapshot.m_Sequence);
        StringBufferAppend(&response, ",\"engine_frame\":");
        AppendNumber(&response, (double)g_AutomationBridge.m_Frame);
        StringBufferAppend(&response, "}}\n");
        RequestSendJson(ctx, 200, &response);
    }

    static bool GetInputIdentity(RequestContext* ctx, const char** client_id, const char** session_id,
                                 const char** request_id, float* lease)
    {
        *client_id = RequestGetParam(ctx, "client_id");
        *session_id = RequestGetParam(ctx, "session_id");
        *request_id = RequestGetParam(ctx, "request_id");
        *lease = 5.0f;
        const char* lease_text = RequestGetParam(ctx, "lease");
        if (!IsEmpty(lease_text) && (!RequestGetFloatParam(ctx, "lease", lease) || !IsFiniteFloat(*lease) || *lease <= 0.0f || *lease > MAX_INPUT_DURATION))
        {
            RequestSendError(ctx, 400, "bad_request", "lease must be finite and between 0 and 60 seconds");
            return false;
        }
        return true;
    }

    static bool AcquireControllerForRequest(RequestContext* ctx, const char* client_id, const char* session_id, float lease)
    {
        const char* error = 0;
        if (!AcquireInputController(client_id, session_id, lease, &error))
        {
            RequestSendError(ctx, 409, "input_controller_busy", error);
            return false;
        }
        return true;
    }

    static bool GetInputOptions(RequestContext* ctx, InputDevice* device, uint32_t* pointer_id, bool* visualize)
    {
        if (!HasInputCapability())
        {
            RequestSendError(ctx, 501, "unsupported_capability", "input injection is unavailable because the runtime has no HID context");
            return false;
        }
        *device = g_AutomationBridge.m_DefaultInputDevice;
        const char* device_text = RequestGetParam(ctx, "device");
        if (!IsEmpty(device_text) && !ParseInputDevice(device_text, device))
        {
            RequestSendError(ctx, 400, "bad_request", "device must be auto, mouse, or touch");
            return false;
        }
        if (!IsInputDeviceSupported(ResolveInputDevice(*device)))
        {
            RequestSendError(ctx, 422, "input_device_unsupported", "requested input device is not supported on this platform");
            return false;
        }
        *pointer_id = 0;
        uint64_t pointer_value = 0;
        const char* pointer_text = RequestGetParam(ctx, "pointer_id");
        if (!IsEmpty(pointer_text))
        {
            if (!RequestGetUInt64Param(ctx, "pointer_id", &pointer_value, 0xffffffffU))
            {
                RequestSendError(ctx, 400, "bad_request", "pointer_id must be an unsigned 32-bit integer");
                return false;
            }
            *pointer_id = (uint32_t)pointer_value;
        }
        *visualize = g_AutomationBridge.m_DefaultInputVisualize;
        RequestGetBoolParam(ctx, "visualize", visualize);
        return true;
    }

    static void SendReceiptResponse(RequestContext* ctx, const InputReceipt* receipt, uint32_t queue_position)
    {
        StringBuffer response;
        StringBufferInit(&response);
        StringBufferAppend(&response, "{\"ok\":true,\"data\":");
        AppendInputReceiptJson(&response, receipt, queue_position);
        StringBufferAppend(&response, "}\n");
        RequestSendJson(ctx, receipt && receipt->m_State == INPUT_STATE_ACCEPTED && !RequestIsMethod(ctx, "GET") ? 202 : 200, &response);
    }

    static bool ValidateDuration(RequestContext* ctx, float value, const char* name)
    {
        if (IsFiniteFloat(value) && value >= 0.0f && value <= MAX_INPUT_DURATION)
        {
            return true;
        }
        StringBuffer message;
        StringBufferInit(&message);
        StringBufferAppend(&message, name);
        StringBufferAppend(&message, " must be finite and between 0 and 60 seconds");
        char* text = StringBufferDetach(&message);
        RequestSendError(ctx, 400, "bad_request", text);
        free(text);
        return false;
    }

    static bool JsonReadNumber(const char** cursor, float* result)
    {
        JsonSkipWhitespace(cursor);
        char* end = 0;
        double value = strtod(*cursor, &end);
        if (end == *cursor || !IsFiniteDouble(value) || !IsFiniteFloat((float)value)) return false;
        *cursor = end;
        *result = (float)value;
        JsonSkipWhitespace(cursor);
        return true;
    }

    static bool ParsePointList(const char* value, dmArray<InputPoint>* points)
    {
        ArrayInit(points);
        if (!value || *value++ != '[') return false;
        JsonSkipWhitespace(&value);
        while (*value != ']')
        {
            InputPoint point;
            memset(&point, 0, sizeof(point));
            if (*value++ != '[' || !JsonReadNumber(&value, &point.m_X) || *value++ != ',' ||
                !JsonReadNumber(&value, &point.m_Y) || *value++ != ']') return false;
            if (!ArrayPush(points, &point) || points->Size() > MAX_INPUT_PATH_POINTS) return false;
            JsonSkipWhitespace(&value);
            if (*value == ']') break;
            if (*value++ != ',') return false;
            JsonSkipWhitespace(&value);
        }
        return *value++ == ']' && *value == 0 && points->Size() > 0;
    }

    static bool ParseDurationList(const char* value, float* durations, uint32_t expected)
    {
        if (!value || *value++ != '[') return false;
        for (uint32_t i = 0; i < expected; ++i)
        {
            if (i && *value++ != ',') return false;
            if (!JsonReadNumber(&value, &durations[i]) || durations[i] < 0 || durations[i] > MAX_INPUT_DURATION) return false;
        }
        return *value++ == ']' && *value == 0;
    }

    static bool ParseEasingList(const char* value, InputEasing* easing, uint32_t expected)
    {
        if (!value)
        {
            for (uint32_t i = 0; i < expected; ++i) easing[i] = INPUT_EASING_LINEAR;
            return true;
        }
        if (*value++ != '[') return false;
        for (uint32_t i = 0; i < expected; ++i)
        {
            if (i && *value++ != ',') return false;
            JsonSkipWhitespace(&value);
            char* name = 0;
            if (!JsonParseString(&value, &name)) return false;
            bool valid = ParseInputEasing(name, &easing[i]);
            free(name);
            if (!valid) return false;
            JsonSkipWhitespace(&value);
        }
        return *value++ == ']' && *value == 0;
    }

    static bool GetModifiersParam(RequestContext* ctx, dmHID::Key* modifiers, uint32_t* modifier_count)
    {
        *modifier_count = 0;
        const char* value = RequestGetParam(ctx, "modifiers");
        if (!value) return true;
        if (*value++ == '[')
        {
            JsonSkipWhitespace(&value);
            while (*modifier_count < MAX_INPUT_MODIFIERS)
            {
                char* name = 0;
                if (!JsonParseString(&value, &name)) break;
                dmHID::Key key = KeyFromName(name);
                free(name);
                if (key == dmHID::MAX_KEY_COUNT) break;
                modifiers[(*modifier_count)++] = key;
                JsonSkipWhitespace(&value);
                if (*value == ']')
                {
                    ++value;
                    if (*value == 0) return true;
                    break;
                }
                if (*value++ != ',') break;
                JsonSkipWhitespace(&value);
            }
        }
        RequestSendError(ctx, 400, "unsupported_key", "modifiers must be an array of one to four supported key names");
        return false;
    }

    static bool QueuePointerPath(RequestContext* ctx, dmArray<InputPoint>* points, InputPathMode mode,
                                 float hold_before, float hold_after, const char* kind, bool pointer_open, float pointer_lease)
    {
        InputDevice device;
        uint32_t pointer_id = 0;
        bool visualize = true;
        if (!GetInputOptions(ctx, &device, &pointer_id, &visualize)) return false;
        dmHID::Key modifiers[MAX_INPUT_MODIFIERS];
        uint32_t modifier_count = 0;
        if (!GetModifiersParam(ctx, modifiers, &modifier_count)) return false;
        const char* client_id = 0;
        const char* session_id = 0;
        const char* request_id = 0;
        float controller_lease = 5.0f;
        if (!GetInputIdentity(ctx, &client_id, &session_id, &request_id, &controller_lease) ||
            !AcquireControllerForRequest(ctx, client_id, session_id, controller_lease)) return false;
        InputReceipt* receipt = 0;
        if (!AddMouseInput(points, mode, hold_before, hold_after, device, pointer_id, visualize, kind,
                           modifiers, modifier_count,
                           client_id, session_id, request_id, g_AutomationBridge.m_Snapshot.m_Sequence,
                           pointer_lease, pointer_open, &receipt))
        {
            RequestSendError(ctx, 429, "input_queue_full", "input could not be queued; check device support and queue capacity");
            return false;
        }
        InputEvent* queued = &g_AutomationBridge.m_InputEvents[g_AutomationBridge.m_InputEvents.Size() - 1];
        const char* target = RequestGetParam(ctx, "id");
        if (!target) target = RequestGetParam(ctx, "from_id");
        queued->m_Target = DuplicateString(target);
        queued->m_EndTarget = DuplicateString(RequestGetParam(ctx, "to_id"));
        SendReceiptResponse(ctx, receipt, g_AutomationBridge.m_InputEvents.Size());
        return true;
    }
    static void HandleClick(RequestContext* ctx)
    {
        if (!HasInputCapability())
        {
            RequestSendError(ctx, 501, "unsupported_capability", "input.click is unavailable because the runtime has no HID context");
            return;
        }
        RefreshSnapshotForRequest();
        if (!ValidateExpectedScene(ctx)) return;
        float x = 0.0f;
        float y = 0.0f;
        const char* id = RequestGetParam(ctx, "id");
        if (!IsEmpty(id))
        {
            if (!ValidateExpectedLogicalIdentity(ctx, id, "expected_logical_id")) return;
            const char* error = 0;
            if (!GetNodeCenter(id, &x, &y, &error))
            {
                RequestSendError(ctx, 404, "not_found", error);
                return;
            }
        }
        else if (!RequestGetFloatParam(ctx, "x", &x) || !RequestGetFloatParam(ctx, "y", &y) || !IsFiniteFloat(x) || !IsFiniteFloat(y))
        {
            RequestSendError(ctx, 400, "bad_request", "provide either id or finite x/y");
            return;
        }
        dmArray<InputPoint> points;
        ArrayInit(&points);
        InputPoint point = {x, y, 0.0f, INPUT_EASING_LINEAR};
        ArrayPush(&points, &point);
        QueuePointerPath(ctx, &points, INPUT_PATH_SAMPLED, 0.0f, 0.0f, "click", false, 0.0f);
        ArrayFree(&points);
    }

    static void HandleDrag(RequestContext* ctx)
    {
        if (!HasInputCapability())
        {
            RequestSendError(ctx, 501, "unsupported_capability", "input.drag is unavailable because the runtime has no HID context");
            return;
        }
        RefreshSnapshotForRequest();
        if (!ValidateExpectedScene(ctx)) return;
        float x1 = 0.0f, y1 = 0.0f, x2 = 0.0f, y2 = 0.0f;
        float duration = 0.35f;
        RequestGetFloatParam(ctx, "duration", &duration);
        if (!ValidateDuration(ctx, duration, "duration")) return;
        const char* from_id = RequestGetParam(ctx, "from_id");
        const char* to_id = RequestGetParam(ctx, "to_id");
        if (!IsEmpty(from_id) && !IsEmpty(to_id))
        {
            if (!ValidateExpectedLogicalIdentity(ctx, from_id, "expected_from_logical_id") ||
                !ValidateExpectedLogicalIdentity(ctx, to_id, "expected_to_logical_id")) return;
            const char* error = 0;
            if (!GetNodeCenter(from_id, &x1, &y1, &error) || !GetNodeCenter(to_id, &x2, &y2, &error))
            {
                RequestSendError(ctx, 404, "not_found", error);
                return;
            }
        }
        else if (!RequestGetFloatParam(ctx, "x1", &x1) || !RequestGetFloatParam(ctx, "y1", &y1) ||
                 !RequestGetFloatParam(ctx, "x2", &x2) || !RequestGetFloatParam(ctx, "y2", &y2) ||
                 !IsFiniteFloat(x1) || !IsFiniteFloat(y1) || !IsFiniteFloat(x2) || !IsFiniteFloat(y2))
        {
            RequestSendError(ctx, 400, "bad_request", "provide either from_id/to_id or finite x1/y1/x2/y2");
            return;
        }
        InputEasing easing = INPUT_EASING_LINEAR;
        if (!ParseInputEasing(RequestGetParam(ctx, "easing"), &easing))
        {
            RequestSendError(ctx, 400, "bad_request", "unsupported easing");
            return;
        }
        float hold_before = 0.0f, hold_after = 0.0f;
        RequestGetFloatParam(ctx, "hold_before", &hold_before);
        RequestGetFloatParam(ctx, "hold_after", &hold_after);
        if (!ValidateDuration(ctx, hold_before, "hold_before")) return;
        if (!ValidateDuration(ctx, hold_after, "hold_after")) return;
        if (duration + hold_before + hold_after > MAX_INPUT_DURATION)
        {
            RequestSendError(ctx, 400, "bad_request", "total gesture duration exceeds 60 seconds");
            return;
        }
        dmArray<InputPoint> points;
        ArrayInit(&points);
        InputPoint start = {x1, y1, 0.0f, INPUT_EASING_LINEAR};
        InputPoint end = {x2, y2, duration, easing};
        ArrayPush(&points, &start);
        ArrayPush(&points, &end);
        QueuePointerPath(ctx, &points, INPUT_PATH_SAMPLED, hold_before, hold_after, "drag", false, 0.0f);
        ArrayFree(&points);
    }

    static void HandleDragPath(RequestContext* ctx)
    {
        RefreshSnapshotForRequest();
        if (!ValidateExpectedScene(ctx)) return;
        dmArray<InputPoint> points;
        if (!ParsePointList(RequestGetParam(ctx, "points"), &points))
        {
            ArrayFree(&points);
            RequestSendError(ctx, 400, "bad_request", "points must contain 2-128 finite x,y pairs separated by semicolons");
            return;
        }
        InputPathMode mode = INPUT_PATH_SAMPLED;
        const char* mode_text = RequestGetParam(ctx, "path");
        if (!IsEmpty(mode_text) && StringsEqual(mode_text, "quadratic")) mode = INPUT_PATH_QUADRATIC;
        else if (!IsEmpty(mode_text) && StringsEqual(mode_text, "cubic")) mode = INPUT_PATH_CUBIC;
        else if (!IsEmpty(mode_text) && !StringsEqual(mode_text, "sampled") && !StringsEqual(mode_text, "linear"))
        {
            ArrayFree(&points);
            RequestSendError(ctx, 400, "bad_request", "path must be sampled, linear, quadratic, or cubic");
            return;
        }
        uint32_t segment_count = mode == INPUT_PATH_SAMPLED ? points.Size() - 1 : 1;
        if (points.Size() < 2 || (mode == INPUT_PATH_QUADRATIC && points.Size() != 3) || (mode == INPUT_PATH_CUBIC && points.Size() != 4))
        {
            ArrayFree(&points);
            RequestSendError(ctx, 400, "bad_request", "sampled paths need at least 2 points; quadratic needs 3; cubic needs 4");
            return;
        }
        float durations[MAX_INPUT_PATH_POINTS];
        InputEasing easing[MAX_INPUT_PATH_POINTS];
        if (!ParseDurationList(RequestGetParam(ctx, "durations"), durations, segment_count) ||
            !ParseEasingList(RequestGetParam(ctx, "easing"), easing, segment_count))
        {
            ArrayFree(&points);
            RequestSendError(ctx, 400, "bad_request", "durations and easing must contain exactly one value per path segment");
            return;
        }
        float total = 0.0f;
        for (uint32_t i = 0; i < segment_count; ++i)
        {
            total += durations[i];
            uint32_t point_index = mode == INPUT_PATH_SAMPLED ? i + 1 : points.Size() - 1;
            points.Begin()[point_index].m_Duration = durations[i];
            points.Begin()[point_index].m_Easing = easing[i];
        }
        float hold_before = 0.0f, hold_after = 0.0f;
        RequestGetFloatParam(ctx, "hold_before", &hold_before);
        RequestGetFloatParam(ctx, "hold_after", &hold_after);
        if (!ValidateDuration(ctx, hold_before, "hold_before") || !ValidateDuration(ctx, hold_after, "hold_after"))
        {
            ArrayFree(&points);
            return;
        }
        if (total + hold_before + hold_after > MAX_INPUT_DURATION)
        {
            RequestSendError(ctx, 400, "bad_request", "total gesture duration exceeds 60 seconds");
            ArrayFree(&points);
            return;
        }
        QueuePointerPath(ctx, &points, mode, hold_before, hold_after, "drag_path", false, 0.0f);
        ArrayFree(&points);
    }

    static void HandleKey(RequestContext* ctx)
    {
        if (!HasInputCapability())
        {
            RequestSendError(ctx, 501, "unsupported_capability", "input.key is unavailable because the runtime has no HID context");
            return;
        }
        RefreshSnapshotForRequest();
        if (!ValidateExpectedScene(ctx)) return;
        const char* value = RequestGetParam(ctx, "keys");
        bool parse_special_keys = value != 0;
        if (!value) value = RequestGetParam(ctx, "text");
        if (IsEmpty(value) || strlen(value) > MAX_KEY_INPUT_BYTES)
        {
            RequestSendError(ctx, IsEmpty(value) ? 400 : 413, IsEmpty(value) ? "bad_request" : "input_too_large", "provide text/keys no larger than 4096 bytes");
            return;
        }
        const char* key_error = 0;
        uint32_t special_key_count = 0;
        if (parse_special_keys && !ValidateSpecialKeyInput(value, &key_error, &special_key_count))
        {
            RequestSendError(ctx, 400, "unsupported_key", key_error);
            return;
        }
        float key_hold = 0.0f;
        if (RequestHasParam(ctx, "hold") && !RequestGetFloatParam(ctx, "hold", &key_hold))
        {
            // Supplied but unparseable values, including null, objects, and arrays in a
            // JSON body, must fail loudly instead of degrading to a tap.
            RequestSendError(ctx, 400, "bad_request", "hold must be finite and between 0 and 60 seconds");
            return;
        }
        if (!ValidateDuration(ctx, key_hold, "hold")) return;
        if (key_hold > 0.0f && special_key_count == 0)
        {
            RequestSendError(ctx, 400, "bad_request", "hold requires at least one {KEY_...} special key via the keys parameter; literal text cannot be held");
            return;
        }
        float requested_duration = key_hold * (float)special_key_count;
        if (requested_duration > MAX_INPUT_DURATION)
        {
            RequestSendError(ctx, 400, "bad_request", "total key hold duration exceeds 60 seconds");
            return;
        }
        dmHID::Key modifiers[MAX_INPUT_MODIFIERS];
        uint32_t modifier_count = 0;
        if (!GetModifiersParam(ctx, modifiers, &modifier_count)) return;
        if (modifier_count > 0 && special_key_count == 0)
        {
            RequestSendError(ctx, 400, "bad_request", "modifiers require at least one {KEY_...} special key via the keys parameter; literal text cannot be chorded");
            return;
        }
        const char* client_id = 0;
        const char* session_id = 0;
        const char* request_id = 0;
        float lease = 5.0f;
        if (!GetInputIdentity(ctx, &client_id, &session_id, &request_id, &lease) || !AcquireControllerForRequest(ctx, client_id, session_id, lease)) return;
        InputReceipt* receipt = 0;
        if (!AddKeyInput(value, parse_special_keys, key_hold, requested_duration,
                         modifiers, modifier_count, client_id, session_id, request_id,
                         g_AutomationBridge.m_Snapshot.m_Sequence, &receipt))
        {
            RequestSendError(ctx, 429, "input_queue_full", "too many input events are already queued");
            return;
        }
        SendReceiptResponse(ctx, receipt, g_AutomationBridge.m_InputEvents.Size());
    }

    static bool GetInputId(RequestContext* ctx, uint64_t* input_id)
    {
        if (!RequestGetUInt64Param(ctx, "input_id", input_id) || *input_id == 0)
        {
            RequestSendError(ctx, 400, "bad_request", "input_id must be a positive integer");
            return false;
        }
        return true;
    }

    static uint32_t QueuePosition(uint64_t input_id)
    {
        for (uint32_t i = 0; i < g_AutomationBridge.m_InputEvents.Size(); ++i)
            if (g_AutomationBridge.m_InputEvents.Begin()[i].m_Receipt.m_Id == input_id) return i + 1;
        return 0;
    }

    static void HandleInputStatus(RequestContext* ctx)
    {
        uint64_t input_id = 0;
        if (!GetInputId(ctx, &input_id)) return;
        const InputReceipt* receipt = FindInputReceipt(input_id);
        if (!receipt)
        {
            RequestSendError(ctx, 404, "input_not_found", "input receipt was not found or expired from bounded history");
            return;
        }
        SendReceiptResponse(ctx, receipt, QueuePosition(input_id));
    }

    static void HandleInputPending(RequestContext* ctx)
    {
        StringBuffer response;
        StringBufferInit(&response);
        StringBufferAppend(&response, "{\"ok\":true,\"data\":{\"inputs\":[");
        for (uint32_t i = 0; i < g_AutomationBridge.m_InputEvents.Size(); ++i)
        {
            if (i) StringBufferAppendChar(&response, ',');
            AppendInputReceiptJson(&response, &g_AutomationBridge.m_InputEvents.Begin()[i].m_Receipt, i + 1);
        }
        StringBufferAppend(&response, "],\"count\":");
        AppendNumber(&response, (double)g_AutomationBridge.m_InputEvents.Size());
        StringBufferAppend(&response, "}}\n");
        RequestSendJson(ctx, 200, &response);
    }

    static bool PointToBackbuffer(const Snapshot* snapshot, const char* space, float x, float y, float* out_x, float* out_y)
    {
        float window_w = (float)snapshot->m_WindowWidth;
        float window_h = (float)snapshot->m_WindowHeight;
        float backbuffer_w = (float)snapshot->m_BackbufferWidth;
        float backbuffer_h = (float)snapshot->m_BackbufferHeight;
        float viewport_top = backbuffer_h - (float)snapshot->m_ViewportY - (float)snapshot->m_ViewportHeight;
        if (window_w <= 0.0f || window_h <= 0.0f || backbuffer_w <= 0.0f || backbuffer_h <= 0.0f)
        {
            return false;
        }
        if (StringsEqual(space, "window") || StringsEqual(space, "client") || StringsEqual(space, "display_pixels"))
        {
            *out_x = x * backbuffer_w / window_w;
            *out_y = y * backbuffer_h / window_h;
            return true;
        }
        if (StringsEqual(space, "backbuffer"))
        {
            *out_x = x;
            *out_y = y;
            return true;
        }
        if (StringsEqual(space, "viewport"))
        {
            *out_x = (float)snapshot->m_ViewportX + x;
            *out_y = viewport_top + y;
            return true;
        }
        if (StringsEqual(space, "normalized_viewport"))
        {
            *out_x = (float)snapshot->m_ViewportX + x * (float)snapshot->m_ViewportWidth;
            *out_y = viewport_top + y * (float)snapshot->m_ViewportHeight;
            return true;
        }
        return false;
    }

    static bool PointFromBackbuffer(const Snapshot* snapshot, const char* space, float x, float y, float* out_x, float* out_y)
    {
        float window_w = (float)snapshot->m_WindowWidth;
        float window_h = (float)snapshot->m_WindowHeight;
        float backbuffer_w = (float)snapshot->m_BackbufferWidth;
        float backbuffer_h = (float)snapshot->m_BackbufferHeight;
        float viewport_top = backbuffer_h - (float)snapshot->m_ViewportY - (float)snapshot->m_ViewportHeight;
        if (window_w <= 0.0f || window_h <= 0.0f || backbuffer_w <= 0.0f || backbuffer_h <= 0.0f)
        {
            return false;
        }
        if (StringsEqual(space, "window") || StringsEqual(space, "client") || StringsEqual(space, "display_pixels"))
        {
            *out_x = x * window_w / backbuffer_w;
            *out_y = y * window_h / backbuffer_h;
            return true;
        }
        if (StringsEqual(space, "backbuffer"))
        {
            *out_x = x;
            *out_y = y;
            return true;
        }
        if (StringsEqual(space, "viewport"))
        {
            *out_x = x - (float)snapshot->m_ViewportX;
            *out_y = y - viewport_top;
            return true;
        }
        if (StringsEqual(space, "normalized_viewport") && snapshot->m_ViewportWidth > 0 && snapshot->m_ViewportHeight > 0)
        {
            *out_x = (x - (float)snapshot->m_ViewportX) / (float)snapshot->m_ViewportWidth;
            *out_y = (y - viewport_top) / (float)snapshot->m_ViewportHeight;
            return true;
        }
        return false;
    }

    static void HandleCoordinateConvert(RequestContext* ctx)
    {
        RefreshSnapshotForRequest();
        float x = 0.0f;
        float y = 0.0f;
        const char* from_space = RequestGetParam(ctx, "from_space");
        const char* to_space = RequestGetParam(ctx, "to_space");
        bool has_x = RequestGetFloatParam(ctx, "point.x", &x) || RequestGetFloatParam(ctx, "x", &x);
        bool has_y = RequestGetFloatParam(ctx, "point.y", &y) || RequestGetFloatParam(ctx, "y", &y);
        if (!has_x || !has_y || IsEmpty(from_space) || IsEmpty(to_space))
        {
            RequestSendError(ctx, 400, "bad_request", "provide numeric x/y and named from_space/to_space");
            return;
        }
        float backbuffer_x = 0.0f;
        float backbuffer_y = 0.0f;
        float converted_x = 0.0f;
        float converted_y = 0.0f;
        const Snapshot* snapshot = &g_AutomationBridge.m_Snapshot;
        if (!PointToBackbuffer(snapshot, from_space, x, y, &backbuffer_x, &backbuffer_y) ||
            !PointFromBackbuffer(snapshot, to_space, backbuffer_x, backbuffer_y, &converted_x, &converted_y))
        {
            RequestSendError(ctx, 400, "unsupported_coordinate_space", "supported spaces are window, client, display_pixels, backbuffer, viewport, and normalized_viewport");
            return;
        }

        StringBuffer response;
        StringBufferInit(&response);
        StringBufferAppend(&response, "{\"ok\":true,\"data\":{\"point\":{\"x\":");
        AppendNumber(&response, converted_x);
        StringBufferAppend(&response, ",\"y\":");
        AppendNumber(&response, converted_y);
        StringBufferAppend(&response, "},\"from_space\":");
        AppendJsonString(&response, from_space);
        StringBufferAppend(&response, ",\"to_space\":");
        AppendJsonString(&response, to_space);
        StringBufferAppend(&response, ",\"scene_sequence\":");
        AppendNumber(&response, (double)snapshot->m_Sequence);
        StringBufferAppend(&response, ",\"engine_frame\":");
        AppendNumber(&response, (double)g_AutomationBridge.m_Frame);
        StringBufferAppend(&response, "}}\n");
        RequestSendJson(ctx, 200, &response);
    }

    static void HandleFrame(RequestContext* ctx)
    {
        StringBuffer response;
        StringBufferInit(&response);
        StringBufferAppend(&response, "{\"ok\":true,\"data\":{\"engine_frame\":");
        AppendNumber(&response, (double)g_AutomationBridge.m_Frame);
        StringBufferAppend(&response, ",\"scene_sequence\":");
        AppendNumber(&response, (double)g_AutomationBridge.m_Snapshot.m_Sequence);
        StringBufferAppend(&response, "}}\n");
        RequestSendJson(ctx, 200, &response);
    }

    static bool VerifyReceiptOwner(RequestContext* ctx, const InputReceipt* receipt, const char* client_id, const char* session_id)
    {
        if (receipt && StringsEqual(receipt->m_ClientId, IsEmpty(client_id) ? "anonymous" : client_id) &&
            StringsEqual(receipt->m_SessionId, IsEmpty(session_id) ? "default" : session_id)) return true;
        RequestSendError(ctx, 403, "input_not_owned", "input belongs to a different client/session");
        return false;
    }

    static void HandleInputCancel(RequestContext* ctx)
    {
        if (!HasInputCapability())
        {
            RequestSendError(ctx, 501, "unsupported_capability", "input cancellation is unavailable because the runtime has no HID context");
            return;
        }
        uint64_t input_id = 0;
        if (!GetInputId(ctx, &input_id)) return;
        const char* client_id = 0, *session_id = 0, *request_id = 0;
        float lease = 5.0f;
        if (!GetInputIdentity(ctx, &client_id, &session_id, &request_id, &lease) || !AcquireControllerForRequest(ctx, client_id, session_id, lease)) return;
        const InputReceipt* receipt = FindInputReceipt(input_id);
        if (!receipt)
        {
            RequestSendError(ctx, 404, "input_not_found", "input was not found");
            return;
        }
        if (!VerifyReceiptOwner(ctx, receipt, client_id, session_id)) return;
        bool release = true;
        RequestGetBoolParam(ctx, "release", &release);
        if (!CancelInput(input_id, release, "cancelled_by_client"))
        {
            RequestSendError(ctx, 409, "input_already_finished", "input is already in a terminal state");
            return;
        }
        SendReceiptResponse(ctx, FindInputReceipt(input_id), QueuePosition(input_id));
    }

    static void HandleInputFlush(RequestContext* ctx)
    {
        if (!HasInputCapability())
        {
            RequestSendError(ctx, 501, "unsupported_capability", "input flush is unavailable because the runtime has no HID context");
            return;
        }
        const char* client_id = 0, *session_id = 0, *request_id = 0;
        float lease = 5.0f;
        if (!GetInputIdentity(ctx, &client_id, &session_id, &request_id, &lease) || !AcquireControllerForRequest(ctx, client_id, session_id, lease)) return;
        bool release = true;
        RequestGetBoolParam(ctx, "release", &release);
        uint32_t count = FlushInput(IsEmpty(client_id) ? "anonymous" : client_id, IsEmpty(session_id) ? "default" : session_id,
                                    release, "flushed_by_client");
        StringBuffer response;
        StringBufferInit(&response);
        StringBufferAppend(&response, "{\"ok\":true,\"data\":{\"cancel_requested\":");
        AppendNumber(&response, (double)count);
        StringBufferAppend(&response, ",\"release\":");
        StringBufferAppend(&response, release ? "true" : "false");
        StringBufferAppend(&response, "}}\n");
        RequestSendJson(ctx, 200, &response);
    }

    static void HandleInputConfigure(RequestContext* ctx)
    {
        if (!HasInputCapability())
        {
            RequestSendError(ctx, 501, "unsupported_capability", "input configuration is unavailable because the runtime has no HID context");
            return;
        }
        const char* client_id = 0, *session_id = 0, *request_id = 0;
        float lease = 5.0f;
        if (!GetInputIdentity(ctx, &client_id, &session_id, &request_id, &lease) || !AcquireControllerForRequest(ctx, client_id, session_id, lease)) return;
        InputDevice device = g_AutomationBridge.m_DefaultInputDevice;
        const char* device_text = RequestGetParam(ctx, "device");
        if (!IsEmpty(device_text) && (!ParseInputDevice(device_text, &device) || !IsInputDeviceSupported(ResolveInputDevice(device))))
        {
            RequestSendError(ctx, 422, "input_device_unsupported", "device must be auto/mouse or a supported touch device");
            return;
        }
        bool visualize = g_AutomationBridge.m_DefaultInputVisualize;
        RequestGetBoolParam(ctx, "visualize", &visualize);
        g_AutomationBridge.m_DefaultInputDevice = device;
        g_AutomationBridge.m_DefaultInputVisualize = visualize;
        StringBuffer response;
        StringBufferInit(&response);
        StringBufferAppend(&response, "{\"ok\":true,\"data\":{\"device\":");
        AppendJsonString(&response, InputDeviceName(device));
        StringBufferAppend(&response, ",\"resolved_device\":");
        AppendJsonString(&response, InputDeviceName(ResolveInputDevice(device)));
        StringBufferAppend(&response, ",\"visualize\":");
        StringBufferAppend(&response, visualize ? "true" : "false");
        StringBufferAppend(&response, ",\"controller\":{\"client_id\":");
        AppendJsonString(&response, IsEmpty(client_id) ? "anonymous" : client_id);
        StringBufferAppend(&response, ",\"session_id\":");
        AppendJsonString(&response, IsEmpty(session_id) ? "default" : session_id);
        StringBufferAppend(&response, "},\"devices\":{\"auto\":true,\"mouse\":true,\"touch\":");
        StringBufferAppend(&response, IsInputDeviceSupported(INPUT_DEVICE_TOUCH) ? "true" : "false");
        StringBufferAppend(&response, "}}}\n");
        RequestSendJson(ctx, 200, &response);
    }

    static void HandlePointerOpen(RequestContext* ctx)
    {
        RefreshSnapshotForRequest();
        if (!ValidateExpectedScene(ctx)) return;
        float x = 0.0f, y = 0.0f;
        if (!RequestGetFloatParam(ctx, "x", &x) || !RequestGetFloatParam(ctx, "y", &y) || !IsFiniteFloat(x) || !IsFiniteFloat(y))
        {
            RequestSendError(ctx, 400, "bad_request", "pointer open requires finite x/y");
            return;
        }
        float pointer_lease = 2.0f;
        RequestGetFloatParam(ctx, "pointer_lease", &pointer_lease);
        if (!ValidateDuration(ctx, pointer_lease, "pointer_lease")) return;
        if (pointer_lease <= 0.0f)
        {
            RequestSendError(ctx, 400, "bad_request", "pointer_lease must be greater than zero");
            return;
        }
        dmArray<InputPoint> points;
        ArrayInit(&points);
        InputPoint point = {x, y, 0.0f, INPUT_EASING_LINEAR};
        ArrayPush(&points, &point);
        QueuePointerPath(ctx, &points, INPUT_PATH_SAMPLED, 0.0f, 0.0f, "pointer", true, pointer_lease);
        ArrayFree(&points);
    }

    static bool PreparePointerCommand(RequestContext* ctx, uint64_t* input_id, float* pointer_lease,
                                      const char** client_id, const char** session_id)
    {
        if (!HasInputCapability())
        {
            RequestSendError(ctx, 501, "unsupported_capability", "pointer input is unavailable because the runtime has no HID context");
            return false;
        }
        if (!GetInputId(ctx, input_id)) return false;
        const char* request_id = 0;
        float controller_lease = 5.0f;
        if (!GetInputIdentity(ctx, client_id, session_id, &request_id, &controller_lease) ||
            !AcquireControllerForRequest(ctx, *client_id, *session_id, controller_lease)) return false;
        const InputReceipt* receipt = FindInputReceipt(*input_id);
        if (!receipt)
        {
            RequestSendError(ctx, 404, "input_not_found", "pointer session was not found");
            return false;
        }
        if (!VerifyReceiptOwner(ctx, receipt, *client_id, *session_id)) return false;
        *pointer_lease = 2.0f;
        RequestGetFloatParam(ctx, "pointer_lease", pointer_lease);
        if (!ValidateDuration(ctx, *pointer_lease, "pointer_lease")) return false;
        if (*pointer_lease <= 0.0f)
        {
            RequestSendError(ctx, 400, "bad_request", "pointer_lease must be greater than zero");
            return false;
        }
        return true;
    }

    static void HandlePointerMove(RequestContext* ctx)
    {
        uint64_t input_id = 0;
        float pointer_lease = 0.0f;
        const char* client_id = 0, *session_id = 0;
        if (!PreparePointerCommand(ctx, &input_id, &pointer_lease, &client_id, &session_id)) return;
        InputPoint point;
        memset(&point, 0, sizeof(point));
        if (!RequestGetFloatParam(ctx, "x", &point.m_X) || !RequestGetFloatParam(ctx, "y", &point.m_Y) ||
            !RequestGetFloatParam(ctx, "duration", &point.m_Duration) || !IsFiniteFloat(point.m_X) || !IsFiniteFloat(point.m_Y))
        {
            RequestSendError(ctx, 400, "bad_request", "pointer move requires finite x/y and duration");
            return;
        }
        if (!ValidateDuration(ctx, point.m_Duration, "duration")) return;
        if (!ParseInputEasing(RequestGetParam(ctx, "easing"), &point.m_Easing))
        {
            RequestSendError(ctx, 400, "bad_request", "pointer move easing is unsupported");
            return;
        }
        const char* error = 0;
        if (!AppendPointerMove(input_id, &point, pointer_lease, &error))
        {
            RequestSendError(ctx, 409, "pointer_closed", error);
            return;
        }
        SendReceiptResponse(ctx, FindInputReceipt(input_id), QueuePosition(input_id));
    }

    static void HandlePointerHold(RequestContext* ctx)
    {
        uint64_t input_id = 0;
        float pointer_lease = 0.0f;
        const char* client_id = 0, *session_id = 0;
        if (!PreparePointerCommand(ctx, &input_id, &pointer_lease, &client_id, &session_id)) return;
        float duration = 0.0f;
        if (!RequestGetFloatParam(ctx, "duration", &duration) || !ValidateDuration(ctx, duration, "duration")) return;
        const char* error = 0;
        if (!AppendPointerHold(input_id, duration, pointer_lease, &error))
        {
            RequestSendError(ctx, 409, "pointer_closed", error);
            return;
        }
        SendReceiptResponse(ctx, FindInputReceipt(input_id), QueuePosition(input_id));
    }

    static void HandlePointerUp(RequestContext* ctx)
    {
        uint64_t input_id = 0;
        float pointer_lease = 0.0f;
        const char* client_id = 0, *session_id = 0;
        if (!PreparePointerCommand(ctx, &input_id, &pointer_lease, &client_id, &session_id)) return;
        const char* error = 0;
        if (!ReleasePointer(input_id, &error))
        {
            RequestSendError(ctx, 409, "pointer_closed", error);
            return;
        }
        SendReceiptResponse(ctx, FindInputReceipt(input_id), QueuePosition(input_id));
    }

    static void HandleScreenshot(RequestContext* ctx)
    {
        RefreshSnapshotForRequest();
        if (!IsScreenshotSupported())
        {
            RequestSendError(ctx, 501, "screenshot_unsupported", "screenshot capture is not supported by the current graphics adapter");
            return;
        }

        uint32_t after_frames = 0;
        RequestGetUIntParamAllowZero(ctx, "after_frames", &after_frames, 600);
        ScreenshotCapture capture;
        if (!ScheduleScreenshot(after_frames, &capture))
        {
            RequestSendError(ctx, 409, "screenshot_pending", "a screenshot is already pending");
            return;
        }

        StringBuffer response;
        StringBufferInit(&response);
        StringBufferAppend(&response, "{\"ok\":true,\"data\":");
        AppendScreenshotJson(&response, &capture);
        response.m_Size--;
        response.m_Data[response.m_Size] = 0;
        StringBufferAppend(&response, ",\"accepted_frame\":");
        AppendNumber(&response, (double)g_AutomationBridge.m_Frame);
        StringBufferAppend(&response, ",\"accepted_scene_sequence\":");
        AppendNumber(&response, (double)g_AutomationBridge.m_Snapshot.m_Sequence);
        StringBufferAppend(&response, "}}\n");
        RequestSendJson(ctx, 200, &response);
    }

    static void HandleScreenshotStatus(RequestContext* ctx)
    {
        uint64_t capture_id = 0;
        if (!RequestGetUInt64Param(ctx, "capture_id", &capture_id) && !RequestGetUInt64Param(ctx, "id", &capture_id))
        {
            RequestSendError(ctx, 400, "bad_request", "provide capture_id");
            return;
        }
        const ScreenshotCapture* capture = FindScreenshotCapture(capture_id);
        if (!capture)
        {
            RequestSendError(ctx, 404, "screenshot_not_found", "screenshot capture id was not found");
            return;
        }
        StringBuffer response;
        StringBufferInit(&response);
        StringBufferAppend(&response, "{\"ok\":true,\"data\":");
        AppendScreenshotJson(&response, capture);
        StringBufferAppend(&response, "}\n");
        RequestSendJson(ctx, 200, &response);
    }

    static void AppendRecordingStatusJson(StringBuffer* out, const RecordingStatus* status)
    {
        StringBufferAppend(out, "{\"operation_id\":");
        AppendNumber(out, (double)status->m_Id);
        StringBufferAppend(out, ",\"pending\":");
        StringBufferAppend(out, status->m_Pending ? "true" : "false");
        StringBufferAppend(out, ",\"state\":");
        AppendJsonString(out, status->m_Pending ? (status->m_Stopping ? "stopping" : "starting") : (status->m_Failure[0] ? "failed" : (status->m_Active ? "recording" : (status->m_Finalized ? "complete" : "idle"))));
        StringBufferAppend(out, ",\"supported\":");
        StringBufferAppend(out, status->m_Supported ? "true" : "false");
        StringBufferAppend(out, ",\"active\":");
        StringBufferAppend(out, status->m_Active ? "true" : "false");
        StringBufferAppend(out, ",\"finalized\":");
        StringBufferAppend(out, status->m_Finalized ? "true" : "false");
        StringBufferAppend(out, ",\"backend\":");
        AppendJsonString(out, NativeRecordingBackendName());
        StringBufferAppend(out, ",\"container\":\"mp4\",\"video_codec\":\"h264\"");
        StringBufferAppend(out, ",\"audio\":");
        StringBufferAppend(out, status->m_Audio ? "true" : "false");
        StringBufferAppend(out, ",\"width\":");
        AppendNumber(out, (double)status->m_Width);
        StringBufferAppend(out, ",\"height\":");
        AppendNumber(out, (double)status->m_Height);
        StringBufferAppend(out, ",\"fps\":");
        AppendNumber(out, (double)status->m_Fps);
        StringBufferAppend(out, ",\"artifact\":");
        AppendArtifactJson(out, status->m_Finalized ? status->m_Path : 0);
        StringBufferAppend(out, ",\"started_wall_time_us\":");
        if (status->m_StartedWallTime) AppendTimestamp(out, status->m_StartedWallTime); else StringBufferAppend(out, "null");
        StringBufferAppend(out, ",\"started_monotonic_time_us\":");
        if (status->m_StartedMonotonicTime) AppendTimestamp(out, status->m_StartedMonotonicTime); else StringBufferAppend(out, "null");
        StringBufferAppend(out, ",\"stopped_wall_time_us\":");
        if (status->m_StoppedWallTime) AppendTimestamp(out, status->m_StoppedWallTime); else StringBufferAppend(out, "null");
        StringBufferAppend(out, ",\"stopped_monotonic_time_us\":");
        if (status->m_StoppedMonotonicTime) AppendTimestamp(out, status->m_StoppedMonotonicTime); else StringBufferAppend(out, "null");
        StringBufferAppend(out, ",\"duration_seconds\":");
        if (status->m_StartedMonotonicTime && status->m_StoppedMonotonicTime >= status->m_StartedMonotonicTime)
        {
            AppendNumber(out, (double)(status->m_StoppedMonotonicTime - status->m_StartedMonotonicTime) / 1000000.0);
        }
        else
        {
            StringBufferAppend(out, "null");
        }
        StringBufferAppend(out, ",\"failure\":");
        if (status->m_Failure[0]) AppendJsonString(out, status->m_Failure); else StringBufferAppend(out, "null");
        StringBufferAppend(out, "}");
    }

    static void SendRecordingStatus(RequestContext* ctx, const RecordingStatus* status)
    {
        StringBuffer response;
        StringBufferInit(&response);
        StringBufferAppend(&response, "{\"ok\":true,\"data\":");
        AppendRecordingStatusJson(&response, status);
        StringBufferAppend(&response, "}\n");
        RequestSendJson(ctx, status->m_Pending ? 202 : 200, &response);
    }

    static void HandleRecordingCapabilities(RequestContext* ctx)
    {
        RecordingStatus status;
        GetRecordingStatus(&status);
        StringBuffer response;
        StringBufferInit(&response);
        StringBufferAppend(&response, "{\"ok\":true,\"data\":{\"backend\":");
        AppendJsonString(&response, NativeRecordingBackendName());
        StringBufferAppend(&response, ",\"available\":");
        StringBufferAppend(&response, status.m_Supported ? "true" : "false");
        StringBufferAppend(&response, ",\"application_window\":");
        StringBufferAppend(&response, status.m_Supported ? "true" : "false");
        StringBufferAppend(&response, ",\"application_audio\":");
        StringBufferAppend(&response, status.m_Supported && IsNativeRecordingAudioSupported() ? "true" : "false");
        StringBufferAppend(&response, ",\"resize_output\":");
        StringBufferAppend(&response, status.m_Supported ? "true" : "false");
        StringBufferAppend(&response, ",\"frame_rate\":");
        StringBufferAppend(&response, status.m_Supported ? "true" : "false");
        StringBufferAppend(&response, ",\"containers\":[\"mp4\"],\"video_codecs\":[\"h264\"],\"minimum_platform_version\":");
        AppendJsonString(&response, NativeRecordingMinimumPlatformVersion());
        StringBufferAppend(&response, ",\"reason\":");
        if (!status.m_Supported && status.m_Failure[0]) AppendJsonString(&response, status.m_Failure); else StringBufferAppend(&response, "null");
        StringBufferAppend(&response, "}}\n");
        RequestSendJson(ctx, 200, &response);
    }

    static void HandleRecordingStatus(RequestContext* ctx)
    {
        RecordingStatus status;
        GetRecordingStatus(&status);
        SendRecordingStatus(ctx, &status);
    }

    static void HandleRecordingStart(RequestContext* ctx)
    {
        if (!IsNativeRecordingSupported())
        {
            RequestSendError(ctx, 501, "recording_unsupported", NativeRecordingUnsupportedReason());
            return;
        }
        uint32_t width = 0;
        uint32_t height = 0;
        bool has_width = !IsEmpty(RequestGetParam(ctx, "width"));
        bool has_height = !IsEmpty(RequestGetParam(ctx, "height"));
        if (has_width != has_height ||
            (has_width && (!RequestGetUIntParam(ctx, "width", &width, 16384) ||
                           !RequestGetUIntParam(ctx, "height", &height, 16384))))
        {
            RequestSendError(ctx, 400, "bad_request", "width and height must be positive integers no greater than 16384, or both omitted");
            return;
        }
        uint32_t fps = 30;
        if (!IsEmpty(RequestGetParam(ctx, "fps")) && !RequestGetUIntParam(ctx, "fps", &fps, 60))
        {
            RequestSendError(ctx, 400, "bad_request", "fps must be an integer between 1 and 60");
            return;
        }
        bool audio = IsNativeRecordingAudioSupported();
        if (!IsEmpty(RequestGetParam(ctx, "audio")) && !RequestGetBoolParam(ctx, "audio", &audio))
        {
            RequestSendError(ctx, 400, "bad_request", "audio must be a boolean");
            return;
        }
        if (audio && !IsNativeRecordingAudioSupported())
        {
            RequestSendError(ctx, 501, "recording_audio_unsupported", "application audio is not supported by this recording backend; use audio=false");
            return;
        }
        char path[1024];
        if (!BuildCapturePath(".mp4", path, sizeof(path)))
        {
            RequestSendError(ctx, 500, "storage_unavailable", "cannot create engine capture storage");
            return;
        }
        RecordingStatus status;
        if (!StartRecording(path, width, height, fps, audio, &status))
        {
            ReleaseCapturePath(path);
            RequestSendError(ctx, status.m_Active ? 409 : 500,
                             status.m_Active ? "recording_active" : "recording_start_failed",
                             status.m_Failure[0] ? status.m_Failure : "video recording could not be started");
            return;
        }
        SendRecordingStatus(ctx, &status);
    }

    static void HandleRecordingStop(RequestContext* ctx)
    {
        uint64_t operation_id = 0;
        RecordingStatus current;
        GetRecordingStatus(&current);
        if (!RequestGetUInt64Param(ctx, "operation_id", &operation_id))
        { RequestSendError(ctx, 400, "bad_request", "operation_id is required"); return; }
        if (current.m_Id != operation_id)
        { RequestSendError(ctx, 410, "stale_operation", "recording receipt was replaced by another operation"); return; }
        RecordingStatus status;
        GetRecordingStatus(&status);
        if (!status.m_Active)
        {
            RequestSendError(ctx, 409, "recording_inactive", "no video recording is active");
            return;
        }
        if (!StopRecording(&status))
        {
            RequestSendError(ctx, 500, "recording_stop_failed",
                             status.m_Failure[0] ? status.m_Failure : "video recording could not be stopped");
            return;
        }
        SendRecordingStatus(ctx, &status);
    }

    static void HandleEventCursor(RequestContext* ctx)
    {
        StringBuffer response;
        StringBufferInit(&response);
        StringBufferAppend(&response, "{\"ok\":true,\"data\":{\"cursor\":");
        AppendNumber(&response, (double)GetEventNextCursor());
        StringBufferAppend(&response, ",\"oldest_cursor\":");
        AppendNumber(&response, (double)GetEventOldestCursor());
        StringBufferAppend(&response, "}}\n");
        RequestSendJson(ctx, 200, &response);
    }

    static void HandleEvents(RequestContext* ctx)
    {
        uint64_t cursor = GetEventOldestCursor();
        const char* cursor_text = RequestGetParam(ctx, "cursor");
        if (!IsEmpty(cursor_text) && !RequestGetUInt64Param(ctx, "cursor", &cursor))
        {
            RequestSendError(ctx, 400, "bad_cursor", "cursor must be an unsigned integer");
            return;
        }
        uint64_t timeout_ms_64 = 0;
        const char* timeout_text = RequestGetParam(ctx, "timeout_ms");
        if (!IsEmpty(timeout_text) && !RequestGetUInt64Param(ctx, "timeout_ms", &timeout_ms_64, 30000))
        {
            RequestSendError(ctx, 400, "bad_timeout", "timeout_ms must be between 0 and 30000");
            return;
        }
        uint64_t limit_64 = 100;
        const char* limit_text = RequestGetParam(ctx, "limit");
        if (!IsEmpty(limit_text) && (!RequestGetUInt64Param(ctx, "limit", &limit_64, 256) || limit_64 == 0))
        {
            RequestSendError(ctx, 400, "bad_limit", "limit must be between 1 and 256");
            return;
        }

        StringBuffer page;
        StringBufferInit(&page);
        bool overflow = false;
        uint64_t next_cursor = cursor;
        AppendEventPageJson(&page, cursor, (uint32_t)limit_64, &overflow, &next_cursor);

        StringBuffer response;
        StringBufferInit(&response);
        StringBufferAppend(&response, "{\"ok\":true,\"data\":");
        StringBufferAppend(&response, page.m_Data);
        StringBufferAppend(&response, "}\n");
        StringBufferFree(&page);
        RequestSendJson(ctx, 200, &response);
    }

    static void HandleApplicationCatalog(RequestContext* ctx)
    {
        if (!g_AutomationBridge.m_ApplicationApiEnabled)
        {
            RequestSendError(ctx, 501, "unsupported_capability", "application.catalog requires application_api = 1");
            return;
        }
        const char* kind = RequestGetParam(ctx, "kind");
        const char* name = RequestGetParam(ctx, "name");
        if (kind && !(StringsEqual(kind, "command") || StringsEqual(kind, "state") || StringsEqual(kind, "event")))
        {
            RequestSendError(ctx, 400, "bad_request", "kind must be command, state, or event");
            return;
        }
        if (name && (IsEmpty(name) || strlen(name) > MAX_APPLICATION_NAME_BYTES))
        {
            RequestSendError(ctx, 400, "bad_request", "name must be a non-empty application identifier");
            return;
        }
        uint32_t limit = 50;
        uint32_t offset = 0;
        uint32_t cursor = 0;
        if ((RequestGetParam(ctx, "limit") && !RequestGetUIntParamAllowZero(ctx, "limit", &limit, 100)) ||
            (RequestGetParam(ctx, "offset") && !RequestGetUIntParamAllowZero(ctx, "offset", &offset)) ||
            (RequestGetParam(ctx, "cursor") && !RequestGetUIntParamAllowZero(ctx, "cursor", &cursor)))
        {
            RequestSendError(ctx, 400, "bad_request", "limit must be 0-100; offset and cursor must be unsigned 32-bit integers");
            return;
        }
        if (RequestGetParam(ctx, "cursor")) offset = cursor;
        StringBuffer response;
        StringBufferInit(&response);
        StringBufferAppend(&response, "{\"ok\":true,\"data\":");
        AppendApplicationCatalogJson(&response, kind, name, offset, limit);
        StringBufferAppend(&response, "}\n");
        RequestSendJson(ctx, 200, &response);
    }

    static void HandleState(RequestContext* ctx)
    {
        StringBuffer page;
        StringBufferInit(&page);
        AppendPublishedStatesJson(&page, RequestGetParam(ctx, "name"), 0);
        StringBuffer response;
        StringBufferInit(&response);
        StringBufferAppend(&response, "{\"ok\":true,\"data\":");
        StringBufferAppend(&response, page.m_Data);
        StringBufferAppend(&response, "}\n");
        StringBufferFree(&page);
        RequestSendJson(ctx, 200, &response);
    }

    static void HandleStateWait(RequestContext* ctx)
    {
        uint64_t after_revision = 0;
        if (!RequestGetUInt64Param(ctx, "after_revision", &after_revision))
        {
            RequestSendError(ctx, 400, "bad_revision", "after_revision is required and must be an unsigned integer");
            return;
        }
        uint64_t timeout_ms = 0;
        const char* timeout_text = RequestGetParam(ctx, "timeout_ms");
        if (!IsEmpty(timeout_text) && !RequestGetUInt64Param(ctx, "timeout_ms", &timeout_ms, 30000))
        {
            RequestSendError(ctx, 400, "bad_timeout", "timeout_ms must be between 0 and 30000");
            return;
        }
        StringBuffer page;
        StringBufferInit(&page);
        AppendPublishedStatesJson(&page, RequestGetParam(ctx, "name"), after_revision);
        StringBuffer response;
        StringBufferInit(&response);
        StringBufferAppend(&response, "{\"ok\":true,\"data\":");
        StringBufferAppend(&response, page.m_Data);
        StringBufferAppend(&response, "}\n");
        StringBufferFree(&page);
        RequestSendJson(ctx, 200, &response);
    }

    static void HandleCommandSubmit(RequestContext* ctx)
    {
        const char* name = RequestGetParam(ctx, "name");
        const char* data = RequestGetParam(ctx, "data");
        if (!data) data = "{}";
        uint64_t timeout_ms = 30000;
        const char* timeout_text = RequestGetParam(ctx, "timeout_ms");
        if (!IsEmpty(timeout_text) && (!RequestGetUInt64Param(ctx, "timeout_ms", &timeout_ms, 300000) || timeout_ms == 0))
        {
            RequestSendError(ctx, 400, "bad_timeout", "timeout_ms must be between 1 and 300000");
            return;
        }
        uint64_t command_id = 0;
        const char* error = 0;
        if (!SubmitCommand(name, data, (uint32_t)timeout_ms, &command_id, &error))
        {
            RequestSendError(ctx, 400, "command_rejected", error);
            return;
        }
        StringBuffer event_data;
        StringBufferInit(&event_data);
        StringBufferAppend(&event_data, "{\"command_id\":"); AppendNumber(&event_data, (double)command_id);
        StringBufferAppend(&event_data, ",\"name\":"); AppendJsonString(&event_data, name);
        StringBufferAppend(&event_data, "}");
        EmitBridgeEvent("command", "command.accepted", event_data.m_Data, 0, false);
        StringBufferFree(&event_data);

        StringBuffer response;
        StringBufferInit(&response);
        StringBufferAppend(&response, "{\"ok\":true,\"data\":{\"command_id\":");
        AppendNumber(&response, (double)command_id);
        StringBufferAppend(&response, ",\"state\":\"pending\"}}\n");
        RequestSendJson(ctx, 202, &response);
    }

    static void HandleCommandStatus(RequestContext* ctx)
    {
        uint64_t command_id = 0;
        if (!RequestGetUInt64Param(ctx, "id", &command_id) || command_id == 0)
        {
            RequestSendError(ctx, 400, "bad_command_id", "id must be a positive integer");
            return;
        }
        StringBuffer command;
        StringBufferInit(&command);
        if (!AppendCommandJson(&command, command_id))
        {
            StringBufferFree(&command);
            RequestSendError(ctx, 404, "command_not_found", "command id was not found");
            return;
        }
        StringBuffer response;
        StringBufferInit(&response);
        StringBufferAppend(&response, "{\"ok\":true,\"data\":");
        StringBufferAppend(&response, command.m_Data);
        StringBufferAppend(&response, "}\n");
        StringBufferFree(&command);
        RequestSendJson(ctx, 200, &response);
    }

    static void HandleCommandCancel(RequestContext* ctx)
    {
        uint64_t command_id = 0;
        if (!RequestGetUInt64Param(ctx, "id", &command_id) || command_id == 0)
        {
            RequestSendError(ctx, 400, "bad_command_id", "id must be a positive integer");
            return;
        }
        const char* error = 0;
        if (!CancelCommand(command_id, &error))
        {
            RequestSendError(ctx, 409, "command_not_cancellable", error);
            return;
        }
        StringBuffer response;
        StringBufferInit(&response);
        StringBufferAppend(&response, "{\"ok\":true,\"data\":{\"command_id\":");
        AppendNumber(&response, (double)command_id);
        StringBufferAppend(&response, ",\"state\":\"cancelled\"}}\n");
        RequestSendJson(ctx, 200, &response);
    }

    static void HandleMarker(RequestContext* ctx)
    {
        const char* name = RequestGetParam(ctx, "name");
        const char* data = RequestGetParam(ctx, "data");
        if (!data) data = "{}";
        uint64_t recording_timestamp_us = 0;
        bool has_recording_timestamp = RequestGetUInt64Param(ctx, "recording_timestamp_us", &recording_timestamp_us);
        uint64_t event_sequence = 0;
        uint64_t native_timestamp_us = 0;
        if (!AddTimelineMarker(name, data, recording_timestamp_us, has_recording_timestamp, &event_sequence, &native_timestamp_us))
        {
            RequestSendError(ctx, 400, "marker_rejected", "marker name or JSON data is invalid");
            return;
        }
        StringBuffer response;
        StringBufferInit(&response);
        StringBufferAppend(&response, "{\"ok\":true,\"data\":{\"event_sequence\":");
        AppendNumber(&response, (double)event_sequence);
        StringBufferAppend(&response, ",\"name\":"); AppendJsonString(&response, name);
        StringBufferAppend(&response, ",\"native_timestamp_us\":"); AppendNumber(&response, (double)native_timestamp_us);
        StringBufferAppend(&response, ",\"recording_timestamp_us\":");
        if (has_recording_timestamp) AppendNumber(&response, (double)recording_timestamp_us); else StringBufferAppend(&response, "null");
        StringBufferAppend(&response, "}}\n");
        RequestSendJson(ctx, 200, &response);
    }

    static const char* MetalCaptureStateName(MetalCaptureState state)
    {
        switch (state)
        {
            case METAL_CAPTURE_PENDING:   return "pending";
            case METAL_CAPTURE_CAPTURING: return "capturing";
            case METAL_CAPTURE_COMPLETE:  return "complete";
            case METAL_CAPTURE_CANCELED:  return "canceled";
            case METAL_CAPTURE_FAILED:    return "failed";
            default:                      return "idle";
        }
    }

    static void AppendMetalCaptureJson(StringBuffer* response)
    {
        StringBufferAppend(response, "{\"state\":");
        AppendJsonString(response, MetalCaptureStateName(g_AutomationBridge.m_MetalCaptureState));
        StringBufferAppend(response, ",\"artifact\":");
        AppendArtifactJson(response, g_AutomationBridge.m_MetalCaptureState == METAL_CAPTURE_COMPLETE ? g_AutomationBridge.m_MetalCapturePath : 0);
        StringBufferAppend(response, ",\"frames\":");
        AppendNumber(response, (double)g_AutomationBridge.m_MetalCaptureFrames);
        StringBufferAppend(response, ",\"frames_captured\":");
        AppendNumber(response, (double)g_AutomationBridge.m_MetalCaptureFramesCaptured);
        StringBufferAppend(response, ",\"stop_requested\":");
        StringBufferAppend(response, g_AutomationBridge.m_MetalCaptureStopRequested ? "true" : "false");
        if (!IsEmpty(g_AutomationBridge.m_MetalCaptureError))
        {
            StringBufferAppend(response, ",\"error\":");
            AppendJsonString(response, g_AutomationBridge.m_MetalCaptureError);
        }
        StringBufferAppendChar(response, '}');
    }

    static void HandleMetalGet(RequestContext* ctx)
    {
        if (!IsMetalCaptureSupported())
        {
            RequestSendError(ctx, 501, "metal_capture_unsupported", "Metal GPU trace capture is not supported by the current platform and graphics adapter");
            return;
        }

        StringBuffer response;
        StringBufferInit(&response);
        StringBufferAppend(&response, "{\"ok\":true,\"data\":");
        AppendMetalCaptureJson(&response);
        StringBufferAppend(&response, "}\n");
        RequestSendJson(ctx, 200, &response);
    }

    static void HandleMetalPost(RequestContext* ctx)
    {
        if (!IsMetalCaptureSupported())
        {
            RequestSendError(ctx, 501, "metal_capture_unsupported", "Metal GPU trace capture is not supported by the current platform and graphics adapter");
            return;
        }


        uint32_t frames = 1;
        const char* frames_text = RequestGetParam(ctx, "frames");
        if (!IsEmpty(frames_text) && !RequestGetUIntParam(ctx, "frames", &frames, MAX_METAL_CAPTURE_FRAMES))
        {
            RequestSendError(ctx, 400, "bad_request", "frames must be a positive integer no greater than 10000");
            return;
        }

        char path[1024];
        if (!BuildCapturePath(".gputrace", path, sizeof(path)))
        {
            RequestSendError(ctx, 500, "storage_unavailable", "cannot create engine capture storage");
            return;
        }
        if (!ScheduleMetalCapture(path, frames))
        {
            ReleaseCapturePath(path);
            RequestSendError(ctx, 409, "metal_capture_active", "a Metal capture is already pending or active");
            return;
        }

        StringBuffer response;
        StringBufferInit(&response);
        StringBufferAppend(&response, "{\"ok\":true,\"data\":");
        AppendMetalCaptureJson(&response);
        StringBufferAppend(&response, "}\n");
        RequestSendJson(ctx, 200, &response);
    }

    static void HandleMetalDelete(RequestContext* ctx)
    {
        if (!IsMetalCaptureSupported())
        {
            RequestSendError(ctx, 501, "metal_capture_unsupported", "Metal GPU trace capture is not supported by the current platform and graphics adapter");
            return;
        }
        if (!RequestMetalCaptureStop())
        {
            RequestSendError(ctx, 409, "metal_capture_inactive", "no Metal capture is pending or active");
            return;
        }

        StringBuffer response;
        StringBufferInit(&response);
        StringBufferAppend(&response, "{\"ok\":true,\"data\":");
        AppendMetalCaptureJson(&response);
        StringBufferAppend(&response, "}\n");
        RequestSendJson(ctx, 200, &response);
    }

    static void SendMethodNotAllowed(RequestContext* ctx, const char* methods)
    {
        StringBuffer response;
        StringBufferInit(&response);
        StringBufferAppend(&response, "{\"ok\":false,\"error\":{\"status\":405,\"code\":");
        AppendJsonString(&response, "method_not_allowed");
        StringBufferAppend(&response, ",\"message\":");
        StringBuffer message;
        StringBufferInit(&message);
        StringBufferAppend(&message, "use ");
        StringBufferAppend(&message, methods);
        char* message_string = StringBufferDetach(&message);
        AppendJsonString(&response, message_string);
        free(message_string);
        StringBufferAppend(&response, "}}\n");
        SendResponse(ctx->m_Request, 405, "application/json", &response, methods);
        StringBufferFree(&response);
    }

    static void AppendAllowedMethod(StringBuffer* methods, const char* method)
    {
        if (methods->m_Size > 0)
        {
            StringBufferAppend(methods, ", ");
        }
        StringBufferAppend(methods, method);
    }

    static const RouteDefinition ROUTES[] = {
        {"/health", "GET", HandleHealth},
        {"/lifecycle", "GET", HandleLifecycle},
        {"/screen", "GET", HandleScreen},
        {"/screen", "PUT", HandleScreenPut},
        {"/coordinates/convert", "POST", HandleCoordinateConvert},
        {"/frame", "GET", HandleFrame},
        {"/scene", "GET", HandleScene},
        {"/elements", "GET", HandleElements},
        {"/element", "GET", HandleElement},
        {"/input/click", "POST", HandleClick},
        {"/input/drag", "POST", HandleDrag},
        {"/input/drag_path", "POST", HandleDragPath},
        {"/input/key", "POST", HandleKey},
        {"/input/status", "GET", HandleInputStatus},
        {"/input/pending", "GET", HandleInputPending},
        {"/input/cancel", "POST", HandleInputCancel},
        {"/input/flush", "POST", HandleInputFlush},
        {"/input/configure", "PUT", HandleInputConfigure},
        {"/input/pointer/open", "POST", HandlePointerOpen},
        {"/input/pointer/move", "POST", HandlePointerMove},
        {"/input/pointer/hold", "POST", HandlePointerHold},
        {"/input/pointer/up", "POST", HandlePointerUp},
        {"/screenshot", "POST", HandleScreenshot},
        {"/recording/capabilities", "GET", HandleRecordingCapabilities},
        {"/recording/status", "GET", HandleRecordingStatus},
        {"/recording/start", "POST", HandleRecordingStart},
        {"/recording/stop", "POST", HandleRecordingStop},
        {"/events/cursor", "GET", HandleEventCursor},
        {"/events", "GET", HandleEvents},
        {"/state", "GET", HandleState},
        {"/application/catalog", "GET", HandleApplicationCatalog},
        {"/state/wait", "GET", HandleStateWait},
        {"/commands", "POST", HandleCommandSubmit},
        {"/commands", "GET", HandleCommandStatus},
        {"/commands", "DELETE", HandleCommandCancel},
        {"/markers", "POST", HandleMarker},
        {"/screenshot/status", "GET", HandleScreenshotStatus},
        {"/metal", "GET", HandleMetalGet},
        {"/metal", "POST", HandleMetalPost},
        {"/metal", "DELETE", HandleMetalDelete}
    };

    void AutomationBridgeHandler(void* user_data, dmWebServer::Request* request)
    {
        (void)user_data;

        RequestContext ctx;
        InitRequestContext(&ctx, request);
        if (!g_AutomationBridge.m_Initialized)
        {
            DiscardRequestBody(request);
            RequestSendError(&ctx, 503, "runtime_unavailable", "engine runtime is restarting or shutting down");
            FreeRequestContext(&ctx);
            return;
        }
        const char* runtime = dmWebServer::GetHeader(request, "X-Automation-Runtime");
        if (runtime && !StringsEqual(runtime, g_AutomationBridge.m_EngineInstanceId) && !StringsEqual(ctx.m_Route, "/health"))
        {
            DiscardRequestBody(request);
            RequestSendError(&ctx, 410, "stale_runtime", "engine has rebooted; obtain health and reacquire references before continuing");
            FreeRequestContext(&ctx);
            return;
        }
        if (!RequestIsMethod(&ctx, "GET") && ctx.m_Query.Size())
        {
            DiscardRequestBody(request);
            RequestSendError(&ctx, 400, "bad_request", "v3 mutations require a JSON body, not query parameters");
            FreeRequestContext(&ctx);
            return;
        }

        if (request->m_ContentLength > 0)
        {
            if (request->m_ContentLength > MAX_JSON_REQUEST_BYTES)
            {
                DiscardRequestBody(request);
                RequestSendError(&ctx, 413, "json_body_too_large", "JSON request body exceeds the 65536-byte limit");
                FreeRequestContext(&ctx);
                return;
            }
            const char* content_type = dmWebServer::GetHeader(request, "Content-Type");
            if (!content_type || !StartsWith(content_type, "application/json"))
            {
                free(ReadRequestBody(request));
                RequestSendError(&ctx, 415, "unsupported_media_type", "request bodies must use application/json");
                FreeRequestContext(&ctx);
                return;
            }
            if (RequestIsMethod(&ctx, "GET"))
            {
                free(ReadRequestBody(request));
                RequestSendError(&ctx, 400, "body_not_supported", "GET endpoints do not accept request bodies");
                FreeRequestContext(&ctx);
                return;
            }
            char* body = ReadRequestBody(request);
            bool parsed = body && strlen(body) == request->m_ContentLength && JsonParseRequest(body, &ctx.m_Query, &ctx.m_JsonFields);
            free(body);
            if (!parsed)
            {
                RequestSendError(&ctx, 400, "invalid_json", "request body must be a valid JSON object with at most 16 nesting levels");
                FreeRequestContext(&ctx);
                return;
            }
        }

        if (!RequestHasApiPrefix(&ctx))
        {
            RequestSendError(&ctx, 404, "not_found", "endpoint not found");
            FreeRequestContext(&ctx);
            return;
        }

        if (StartsWith(ctx.m_Route, "/artifacts/"))
        {
            HandleArtifact(request, ctx.m_Route + strlen("/artifacts/"));
            FreeRequestContext(&ctx);
            return;
        }

        if (StringsEqual(ctx.m_Route, "/observations") && RequestIsMethod(&ctx, "POST"))
        {
            HandleObservationPost(&ctx);
            FreeRequestContext(&ctx);
            return;
        }
        if (StartsWith(ctx.m_Route, "/observations/") && RequestIsMethod(&ctx, "GET"))
        {
            HandleObservationGet(&ctx, ctx.m_Route + strlen("/observations/"));
            FreeRequestContext(&ctx);
            return;
        }

        StringBuffer allowed_methods;
        StringBufferInit(&allowed_methods);
        for (uint32_t i = 0; i < DM_ARRAY_SIZE(ROUTES); ++i)
        {
            const RouteDefinition* route = &ROUTES[i];
            if (!StringsEqual(ctx.m_Route, route->m_Route))
            {
                continue;
            }
            if (!RequestIsMethod(&ctx, route->m_Method))
            {
                AppendAllowedMethod(&allowed_methods, route->m_Method);
                continue;
            }
            StringBufferFree(&allowed_methods);
            route->m_Handler(&ctx);
            FreeRequestContext(&ctx);
            return;
        }

        if (allowed_methods.m_Size > 0)
        {
            char* methods = StringBufferDetach(&allowed_methods);
            SendMethodNotAllowed(&ctx, methods);
            free(methods);
            FreeRequestContext(&ctx);
            return;
        }
        StringBufferFree(&allowed_methods);

        RequestSendError(&ctx, 404, "not_found", "endpoint not found");
        FreeRequestContext(&ctx);
    }

    void RegisterWebEndpoint(dmExtension::AppParams* params)
    {
        dmWebServer::HServer server = dmEngine::GetWebServer(params);
        if (!server)
        {
            dmLogWarning("Automation Bridge extension: debug web server is unavailable");
            return;
        }

        dmWebServer::HandlerParams handler_params;
        handler_params.m_Userdata = 0;
        handler_params.m_Handler = AutomationBridgeHandler;

        dmWebServer::Result result = dmWebServer::AddHandler(server, API_PREFIX, &handler_params);
        // The service owns the route across reboots. Keep its registration order
        // ahead of the profiler's catch-all route; the handler reads the current runtime.
        if (result == dmWebServer::RESULT_OK || result == dmWebServer::RESULT_HANDLER_ALREADY_REGISTRED)
        {
            g_AutomationBridge.m_RegisteredMonotonicTime = dmTime::GetMonotonicTime();
            dmLogInfo("Automation Bridge endpoint registered at `%s`", API_PREFIX);
        }
        else
        {
            dmLogWarning("Unable to register Automation Bridge endpoint '%s' (%d)", API_PREFIX, result);
        }
    }

}
